/*
 * Siedle In-Home bus <-> AWS IoT gateway.
 *
 * Boot sequence: NVS -> device config -> Wi-Fi (setup hotspot if needed) -> mDNS -> cloud client -> SNTP
 * (connects the cloud once the time is valid) -> web UI -> reset button.
 */
#include <inttypes.h>
#include <sys/time.h>

#include "app_status.h"
#include "cloud.h"
#include "devcfg.h"
#include "esp_app_desc.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_netif_sntp.h"
#include "mdns.h"
#include "nvs_flash.h"
#include "reset_button.h"
#include "sdkconfig.h"
#include "siedle_log.h"
#include "web_ui.h"
#include "wifi_mgr.h"

static const char *TAG = "main";

static devcfg_t s_devcfg;
static bool s_cloud_ready;

static esp_err_t init_nvs(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS partition is full or has a newer format, erasing it");
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
    if (s_cloud_ready) {
        ESP_ERROR_CHECK_WITHOUT_ABORT(cloud_start());
    }
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

void app_main(void)
{
    const esp_app_desc_t *app = esp_app_get_description();
    ESP_LOGI(TAG, "%s %s", app->project_name, app->version);

    ESP_ERROR_CHECK(init_nvs());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(siedle_log_init());

    if (devcfg_load(&s_devcfg) != ESP_OK) {
        ESP_LOGE(TAG, "reading the device configuration failed, continuing without");
    }
    const char *hostname = s_devcfg.hostname ? s_devcfg.hostname : CONFIG_GATEWAY_HOSTNAME;
    app_status_init(hostname, s_devcfg.client_id);

    const wifi_mgr_config_t wifi_cfg = {
        .hostname = hostname,
        .ap_password = s_devcfg.ap_password,
    };
    ESP_ERROR_CHECK(wifi_mgr_start(&wifi_cfg));
    start_mdns(hostname);

    if (devcfg_has_cloud(&s_devcfg)) {
        const cloud_config_t cloud_cfg = {
            .uri = s_devcfg.mqtt_uri,
            .client_id = s_devcfg.client_id,
            .client_cert = s_devcfg.client_cert,
            .client_key = s_devcfg.client_key,
            .on_send_request = on_send_request,
            .build_status = app_status_build,
        };
        s_cloud_ready = cloud_init(&cloud_cfg) == ESP_OK;
    } else {
        ESP_LOGW(TAG, "no AWS IoT identity in devcfg, running without cloud connection");
    }

    // must come after cloud_init(), the sync callback starts the cloud connection
    esp_sntp_config_t sntp_cfg = ESP_NETIF_SNTP_DEFAULT_CONFIG(CONFIG_GATEWAY_NTP_SERVER);
    sntp_cfg.sync_cb = on_time_sync;
    ESP_ERROR_CHECK(esp_netif_sntp_init(&sntp_cfg));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, on_got_ip, NULL));

    const web_ui_config_t web_cfg = { .build_status = app_status_build };
    ESP_ERROR_CHECK(web_ui_start(&web_cfg));
    ESP_ERROR_CHECK(reset_button_start());
}
