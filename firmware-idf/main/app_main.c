/*
 * Siedle In-Home bus <-> AWS IoT gateway.
 *
 * Boot sequence: NVS -> device identity -> Wi-Fi (setup hotspot if needed) -> device key (first boot) -> mDNS ->
 * cloud client -> SNTP (connects the cloud once the time is valid) -> web UI -> setup button.
 */
#include <inttypes.h>
#include <stdio.h>
#include <sys/time.h>

#include "app_status.h"
#include "cloud.h"
#include "esp_app_desc.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_netif_sntp.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "identity.h"
#include "mdns.h"
#include "nvs_flash.h"
#include "ota.h"
#include "sdkconfig.h"
#include "setup_button.h"
#include "siedle_log.h"
#include "web_ui.h"
#include "wifi_mgr.h"

static const char *TAG = "main";

static SemaphoreHandle_t s_cloud_lock;

static esp_err_t init_nvs(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGE(TAG, "NVS partition is full or has a newer format, erasing it. The Wi-Fi network and the device "
                      "identity (key, AWS IoT certificate) are lost and have to be set up again.");
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    return err;
}

// Placeholder until the bus driver exists (phase 2): record the request so it shows up in the web UI
static void on_send_request(siedle_cmd_t cmd)
{
    ESP_LOGW(TAG, "send request %" PRIu32 " dropped, no bus driver yet", cmd);
    siedle_log_add(cmd, SIEDLE_LOG_TX, false);
}

// Runs in the lwIP context after every successful SNTP sync
static void on_time_sync(struct timeval *tv)
{
    if (app_status_time_synced()) {
        return;
    }
    ESP_LOGI(TAG, "time synchronized");
    app_status_set_time_synced();
    ESP_ERROR_CHECK_WITHOUT_ABORT(cloud_start());
}

// While offline, SNTP backs off its retries (up to 150 s). Restart it as soon as we are online so the cloud
// connection does not wait for the next retry.
static void on_got_ip(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    if (!app_status_time_synced()) {
        esp_netif_sntp_start();
    }
}

static void start_mdns(const char *hostname)
{
    esp_err_t err = mdns_init();
    if (err == ESP_OK) {
        err = mdns_hostname_set(hostname);
    }
    if (err == ESP_OK) {
        err = mdns_instance_name_set("Siedle Gateway");
    }
    if (err == ESP_OK) {
        err = mdns_service_add(NULL, "_http", "_tcp", 80, NULL, 0);
    }
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "mDNS: %s", esp_err_to_name(err));
    }
}

// Connect with the AWS IoT settings from the identity store, replacing the current connection. Blocks while that
// one is closed.
static void apply_cloud_settings(void)
{
    // one at a time, so the settings read last are the ones that end up connected
    xSemaphoreTake(s_cloud_lock, portMAX_DELAY);
    identity_t id;
    if (identity_load(&id) == ESP_OK) {
        if (identity_has_cloud(&id)) {
            char uri[sizeof("mqtts://:8883") + IDENTITY_ENDPOINT_MAX];
            snprintf(uri, sizeof(uri), "mqtts://%s:8883", id.endpoint);
            const cloud_config_t cfg = {
                .uri = uri,
                .client_id = id.thing,
                .client_cert = id.cert,
                .client_key = id.key,
            };
            ESP_ERROR_CHECK_WITHOUT_ABORT(cloud_configure(&cfg));
        } else {
            ESP_LOGW(TAG, "no AWS IoT identity, set it up on the setup page");
            ESP_ERROR_CHECK_WITHOUT_ABORT(cloud_configure(NULL));
        }
        identity_free(&id);
    }
    xSemaphoreGive(s_cloud_lock);
}

static void cloud_settings_task(void *arg)
{
    apply_cloud_settings();
    vTaskDelete(NULL);
}

// The setup page changed the AWS IoT settings or the device key. Reconnecting can take a few seconds, so it runs on
// a task of its own instead of the HTTP server's.
static void on_cloud_changed(void)
{
    if (xTaskCreate(cloud_settings_task, "cloud_settings", 4096, NULL, 3, NULL) != pdPASS) {
        ESP_LOGE(TAG, "applying the cloud settings failed: out of memory");
    }
}

// The setup page changed the hostname or the hotspot password. The hostname applies to mDNS right away and to DHCP
// from the next request on, the hotspot password the next time the hotspot opens.
static void on_device_changed(void)
{
    identity_t id;
    if (identity_load(&id) != ESP_OK) {
        return;
    }
    const char *hostname = id.hostname ? id.hostname : CONFIG_GATEWAY_HOSTNAME;
    app_status_set_hostname(hostname);
    ESP_ERROR_CHECK_WITHOUT_ABORT(wifi_mgr_set_hostname(hostname));
    ESP_ERROR_CHECK_WITHOUT_ABORT(mdns_hostname_set(hostname));
    ESP_ERROR_CHECK_WITHOUT_ABORT(wifi_mgr_set_ap_password(id.ap_password));
    identity_free(&id);
}

void app_main(void)
{
    const esp_app_desc_t *app = esp_app_get_description();
    ESP_LOGI(TAG, "%s %s", app->project_name, app->version);

    static StaticSemaphore_t cloud_lock_buf;
    s_cloud_lock = xSemaphoreCreateMutexStatic(&cloud_lock_buf);
    ESP_ERROR_CHECK(init_nvs());
    ESP_ERROR_CHECK(identity_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(siedle_log_init());
    ESP_ERROR_CHECK(ota_init());

    identity_t id;
    ESP_ERROR_CHECK_WITHOUT_ABORT(identity_load(&id));
    const char *hostname = id.hostname ? id.hostname : CONFIG_GATEWAY_HOSTNAME;
    app_status_set_hostname(hostname);

    const wifi_mgr_config_t wifi_cfg = {
        .hostname = hostname,
        .ap_password = id.ap_password,
    };
    ESP_ERROR_CHECK(wifi_mgr_start(&wifi_cfg));
    // only now: the hardware random number generator is a true one while Wi-Fi is running
    ESP_ERROR_CHECK_WITHOUT_ABORT(identity_ensure_key());
    start_mdns(hostname);
    identity_free(&id);

    const cloud_callbacks_t cloud_callbacks = {
        .on_send_request = on_send_request,
        .build_status = app_status_build,
    };
    ESP_ERROR_CHECK(cloud_init(&cloud_callbacks));
    apply_cloud_settings();

    // must come after cloud_init(), the sync callback starts the cloud connection
    esp_sntp_config_t sntp_cfg = ESP_NETIF_SNTP_DEFAULT_CONFIG(CONFIG_GATEWAY_NTP_SERVER);
    sntp_cfg.sync_cb = on_time_sync;
    ESP_ERROR_CHECK(esp_netif_sntp_init(&sntp_cfg));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, on_got_ip, NULL));

    const web_ui_config_t web_cfg = {
        .build_status = app_status_build,
        .on_cloud_changed = on_cloud_changed,
        .on_device_changed = on_device_changed,
    };
    ESP_ERROR_CHECK(web_ui_start(&web_cfg));
    ESP_ERROR_CHECK(setup_button_start());
}
