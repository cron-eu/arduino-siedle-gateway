#include "app_status.h"

#include <stdatomic.h>
#include <time.h>

#include "cloud.h"
#include "esp_app_desc.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "sdkconfig.h"
#include "wifi_mgr.h"

static const char *s_hostname;
static const char *s_client_id;
static atomic_bool s_time_synced;

void app_status_init(const char *hostname, const char *client_id)
{
    s_hostname = hostname;
    s_client_id = client_id;
}

void app_status_set_time_synced(void)
{
    atomic_store(&s_time_synced, true);
}

bool app_status_time_synced(void)
{
    return atomic_load(&s_time_synced);
}

static const char *reset_reason_name(esp_reset_reason_t reason)
{
    switch (reason) {
    case ESP_RST_POWERON: return "power on";
    case ESP_RST_EXT: return "external pin";
    case ESP_RST_SW: return "software restart";
    case ESP_RST_PANIC: return "crash";
    case ESP_RST_INT_WDT: return "interrupt watchdog";
    case ESP_RST_TASK_WDT: return "task watchdog";
    case ESP_RST_WDT: return "watchdog";
    case ESP_RST_DEEPSLEEP: return "deep sleep wakeup";
    case ESP_RST_BROWNOUT: return "brownout";
    default: return "unknown";
    }
}

static const char *sta_state_name(wifi_mgr_sta_state_t state)
{
    switch (state) {
    case WIFI_MGR_STA_CONNECTING: return "connecting";
    case WIFI_MGR_STA_CONNECTED: return "connected";
    case WIFI_MGR_STA_DISCONNECTED: return "disconnected";
    default: return "unconfigured";
    }
}

static const char *trial_name(wifi_mgr_trial_t trial)
{
    switch (trial) {
    case WIFI_MGR_TRIAL_PENDING: return "pending";
    case WIFI_MGR_TRIAL_OK: return "ok";
    case WIFI_MGR_TRIAL_FAILED: return "failed";
    default: return "none";
    }
}

cJSON *app_status_build(void)
{
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        return NULL;
    }

    const esp_app_desc_t *app = esp_app_get_description();
    cJSON *device = cJSON_AddObjectToObject(root, "device");
    cJSON_AddStringToObject(device, "hostname", s_hostname);
    cJSON_AddStringToObject(device, "version", app->version);
    cJSON_AddStringToObject(device, "idf", app->idf_ver);
    cJSON_AddStringToObject(device, "chip", CONFIG_IDF_TARGET);
    cJSON_AddNumberToObject(device, "uptime_s", (double)(esp_timer_get_time() / 1000000));
    cJSON_AddStringToObject(device, "reset_reason", reset_reason_name(esp_reset_reason()));
    cJSON_AddNumberToObject(device, "heap_free", esp_get_free_heap_size());
    cJSON_AddNumberToObject(device, "heap_min", esp_get_minimum_free_heap_size());
    bool synced = app_status_time_synced();
    cJSON_AddBoolToObject(device, "time_synced", synced);
    if (synced) {
        cJSON_AddNumberToObject(device, "time", (double)time(NULL));
    }

    wifi_mgr_status_t w;
    wifi_mgr_get_status(&w);
    cJSON *wifi = cJSON_AddObjectToObject(root, "wifi");
    cJSON_AddStringToObject(wifi, "state", sta_state_name(w.sta_state));
    cJSON_AddStringToObject(wifi, "ssid", w.ssid);
    cJSON_AddStringToObject(wifi, "ip", w.ip);
    cJSON_AddNumberToObject(wifi, "rssi", w.rssi);
    cJSON_AddNumberToObject(wifi, "last_reason", w.last_reason);
    cJSON_AddBoolToObject(wifi, "portal", w.portal != WIFI_MGR_PORTAL_OFF);
    cJSON_AddStringToObject(wifi, "ap_ssid", w.ap_ssid);
    cJSON_AddBoolToObject(wifi, "ap_secured", w.ap_secured);
    cJSON_AddStringToObject(wifi, "trial", trial_name(w.trial));

    cloud_status_t c;
    cloud_get_status(&c);
    cJSON *cloud = cJSON_AddObjectToObject(root, "cloud");
    cJSON_AddBoolToObject(cloud, "configured", c.configured);
    if (c.configured) {
        cJSON_AddStringToObject(cloud, "client_id", s_client_id);
    }
    cJSON_AddBoolToObject(cloud, "connected", c.connected);
    cJSON_AddNumberToObject(cloud, "connects", c.connects);
    cJSON_AddNumberToObject(cloud, "published", c.published);
    cJSON_AddNumberToObject(cloud, "received", c.received);
    cJSON_AddNumberToObject(cloud, "dropped", c.dropped);

    // the bus driver follows in phase 2
    cJSON *bus = cJSON_AddObjectToObject(root, "bus");
    cJSON_AddBoolToObject(bus, "available", false);

    return root;
}
