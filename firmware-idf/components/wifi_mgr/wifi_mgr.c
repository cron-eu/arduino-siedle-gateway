#include "wifi_mgr.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dns_server.h"
#include "esp_check.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "lwip/inet.h"
#include "nvs.h"

#define NVS_NAMESPACE      "wifi"
#define RETRY_DELAY_MIN_MS 1000
#define RETRY_DELAY_MAX_MS 30000
#define TRIAL_ATTEMPTS     3
#define SCAN_MAX_RECORDS   24

static const char *TAG = "wifi_mgr";

typedef struct {
    char ssid[33];
    char password[65];
} credentials_t;

static SemaphoreHandle_t s_lock;
static esp_netif_t *s_sta_netif;
static esp_netif_t *s_ap_netif;
static esp_timer_handle_t s_retry_timer;
static esp_timer_handle_t s_fallback_timer;
static esp_timer_handle_t s_portal_stop_timer;
static esp_timer_handle_t s_portal_idle_timer;
static dns_server_handle_t s_dns;

static credentials_t s_saved;      // stored in NVS, ssid[0] == 0 if none
static credentials_t s_trial;      // being tried via wifi_mgr_connect()
static wifi_mgr_trial_t s_trial_state;
static int s_trial_failures;
static wifi_mgr_sta_state_t s_state;
static uint8_t s_last_reason;
static uint32_t s_retry_count;
static bool s_scanning;
static bool s_expect_disconnect; // the next WIFI_REASON_ASSOC_LEAVE was caused by us
static wifi_mgr_portal_t s_portal;
static bool s_portal_secured;       // the open hotspot uses a password
static char s_ap_ssid[33];
static char s_ap_password[65];      // for the next time the hotspot opens
static bool s_ap_password_changed;  // since the hotspot opened
static char s_portal_uri[32];

#define LOCK()   xSemaphoreTake(s_lock, portMAX_DELAY)
#define UNLOCK() xSemaphoreGive(s_lock)

/* ---- credentials ------------------------------------------------------------------------------------------- */

static void load_credentials(credentials_t *out)
{
    memset(out, 0, sizeof(*out));
    nvs_handle_t nvs;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs) != ESP_OK) {
        return;
    }
    size_t len = sizeof(out->ssid);
    if (nvs_get_str(nvs, "ssid", out->ssid, &len) == ESP_OK) {
        len = sizeof(out->password);
        if (nvs_get_str(nvs, "password", out->password, &len) != ESP_OK) {
            out->password[0] = '\0';
        }
    } else {
        out->ssid[0] = '\0';
    }
    nvs_close(nvs);
}

static esp_err_t store_credentials(const credentials_t *creds)
{
    nvs_handle_t nvs;
    ESP_RETURN_ON_ERROR(nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs), TAG, "nvs open");
    esp_err_t err = nvs_set_str(nvs, "ssid", creds->ssid);
    if (err == ESP_OK) {
        err = nvs_set_str(nvs, "password", creds->password);
    }
    if (err == ESP_OK) {
        err = nvs_commit(nvs);
    }
    nvs_close(nvs);
    return err;
}

static esp_err_t erase_credentials(void)
{
    nvs_handle_t nvs;
    ESP_RETURN_ON_ERROR(nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs), TAG, "nvs open");
    esp_err_t err = nvs_erase_all(nvs);
    if (err == ESP_OK) {
        err = nvs_commit(nvs);
    }
    nvs_close(nvs);
    return err;
}

/* ---- station ----------------------------------------------------------------------------------------------- */

// The credentials the station should currently use, NULL if none (lock held)
static const credentials_t *active_credentials(void)
{
    if (s_trial_state == WIFI_MGR_TRIAL_PENDING) {
        return &s_trial;
    }
    return s_saved.ssid[0] ? &s_saved : NULL;
}

// Apply the active credentials and start connecting (lock held)
static void sta_connect(void)
{
    const credentials_t *creds = active_credentials();
    if (creds == NULL) {
        s_state = WIFI_MGR_STA_UNCONFIGURED;
        return;
    }

    wifi_config_t cfg = { 0 };
    strlcpy((char *)cfg.sta.ssid, creds->ssid, sizeof(cfg.sta.ssid));
    strlcpy((char *)cfg.sta.password, creds->password, sizeof(cfg.sta.password));
    cfg.sta.threshold.authmode = creds->password[0] ? WIFI_AUTH_WPA2_PSK : WIFI_AUTH_OPEN;
    cfg.sta.pmf_cfg.capable = true;
    cfg.sta.scan_method = WIFI_ALL_CHANNEL_SCAN;
    cfg.sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL;

    esp_err_t err = esp_wifi_set_config(WIFI_IF_STA, &cfg);
    if (err == ESP_OK) {
        err = esp_wifi_connect();
    }
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "connect: %s", esp_err_to_name(err));
    }
    s_state = WIFI_MGR_STA_CONNECTING;
}

// Disconnect without triggering the retry logic, the caller decides what happens next (lock held)
static void sta_disconnect_on_purpose(void)
{
    if (s_state == WIFI_MGR_STA_CONNECTED || s_state == WIFI_MGR_STA_CONNECTING) {
        s_expect_disconnect = true;
    }
    esp_wifi_disconnect();
}

// Retry with exponential backoff (lock held)
static void schedule_retry(void)
{
    uint32_t delay_ms = RETRY_DELAY_MIN_MS << (s_retry_count < 5 ? s_retry_count : 5);
    if (delay_ms > RETRY_DELAY_MAX_MS) {
        delay_ms = RETRY_DELAY_MAX_MS;
    }
    s_retry_count++;
    esp_timer_stop(s_retry_timer);
    esp_timer_start_once(s_retry_timer, (uint64_t)delay_ms * 1000);
}

static void retry_timer_cb(void *arg)
{
    LOCK();
    if (!s_scanning && s_state == WIFI_MGR_STA_DISCONNECTED) {
        sta_connect();
    }
    UNLOCK();
}

/* ---- setup hotspot ----------------------------------------------------------------------------------------- */

// (lock held)
static void portal_arm_idle_timer(void)
{
    esp_timer_stop(s_portal_idle_timer);
    esp_timer_start_once(s_portal_idle_timer, (uint64_t)CONFIG_WIFI_MGR_PORTAL_IDLE_SEC * 1000000);
}

// Open the hotspot, or change the reason it is open for (lock held)
static void portal_open(wifi_mgr_portal_t reason)
{
    if (reason == WIFI_MGR_PORTAL_MANUAL) {
        portal_arm_idle_timer();
    } else {
        esp_timer_stop(s_portal_idle_timer);
    }
    if (s_portal != WIFI_MGR_PORTAL_OFF) {
        s_portal = reason;
        return;
    }

    wifi_config_t cfg = {
        .ap = {
            .channel = CONFIG_WIFI_MGR_AP_CHANNEL,
            .max_connection = 4,
            .authmode = s_ap_password[0] ? WIFI_AUTH_WPA2_PSK : WIFI_AUTH_OPEN,
        },
    };
    strlcpy((char *)cfg.ap.ssid, s_ap_ssid, sizeof(cfg.ap.ssid));
    cfg.ap.ssid_len = strlen(s_ap_ssid);
    strlcpy((char *)cfg.ap.password, s_ap_password, sizeof(cfg.ap.password));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &cfg));

    // DHCP option 114 tells modern clients the portal URL directly
    esp_netif_ip_info_t ip;
    esp_netif_get_ip_info(s_ap_netif, &ip);
    snprintf(s_portal_uri, sizeof(s_portal_uri), "http://" IPSTR "/", IP2STR(&ip.ip));
    esp_netif_dhcps_stop(s_ap_netif);
    esp_netif_dhcps_option(s_ap_netif, ESP_NETIF_OP_SET, ESP_NETIF_CAPTIVEPORTAL_URI, s_portal_uri,
                           strlen(s_portal_uri));
    esp_netif_dhcps_start(s_ap_netif);

    // answer every DNS query with our address, this triggers the captive portal popup
    dns_server_config_t dns_cfg = DNS_SERVER_CONFIG_SINGLE("*", "WIFI_AP_DEF");
    s_dns = start_dns_server(&dns_cfg);

    s_portal = reason;
    s_portal_secured = s_ap_password[0] != '\0';
    s_ap_password_changed = false;
    ESP_LOGI(TAG, "setup hotspot '%s' (%s) started, portal at %s", s_ap_ssid,
             s_ap_password[0] ? "WPA2" : "open", s_portal_uri);
}

// (lock held)
static void portal_stop(void)
{
    if (s_portal == WIFI_MGR_PORTAL_OFF) {
        return;
    }
    esp_timer_stop(s_portal_stop_timer);
    esp_timer_stop(s_portal_idle_timer);
    if (s_dns) {
        stop_dns_server(s_dns);
        s_dns = NULL;
    }
    esp_wifi_set_mode(WIFI_MODE_STA);
    s_portal = WIFI_MGR_PORTAL_OFF;
    ESP_LOGI(TAG, "setup hotspot stopped");
}

static void fallback_timer_cb(void *arg)
{
    LOCK();
    // a hotspot opened on request keeps its reason, its idle timeout takes care of it
    if (s_state != WIFI_MGR_STA_CONNECTED && s_portal == WIFI_MGR_PORTAL_OFF) {
        ESP_LOGW(TAG, "no connection for %d s, opening the setup hotspot", CONFIG_WIFI_MGR_PORTAL_FALLBACK_SEC);
        portal_open(WIFI_MGR_PORTAL_FALLBACK);
    }
    UNLOCK();
}

static void portal_stop_timer_cb(void *arg)
{
    LOCK();
    if (s_state == WIFI_MGR_STA_CONNECTED) {
        portal_stop();
    }
    UNLOCK();
}

// Close a hotspot opened on request once nobody uses it anymore
static void portal_idle_timer_cb(void *arg)
{
    LOCK();
    if (s_portal == WIFI_MGR_PORTAL_MANUAL) {
        wifi_sta_list_t clients;
        if (esp_wifi_ap_get_sta_list(&clients) == ESP_OK && clients.num > 0) {
            portal_arm_idle_timer(); // still in use, check again later
        } else if (s_state == WIFI_MGR_STA_CONNECTED) {
            ESP_LOGI(TAG, "setup hotspot unused for %d s", CONFIG_WIFI_MGR_PORTAL_IDLE_SEC);
            portal_stop();
        } else {
            // offline: stay open like the automatic hotspot, until the station is back
            s_portal = WIFI_MGR_PORTAL_FALLBACK;
        }
    }
    UNLOCK();
}

static void start_fallback_timer(void)
{
    if (s_portal == WIFI_MGR_PORTAL_OFF && !esp_timer_is_active(s_fallback_timer)) {
        esp_timer_start_once(s_fallback_timer, (uint64_t)CONFIG_WIFI_MGR_PORTAL_FALLBACK_SEC * 1000000);
    }
}

// (lock held)
static esp_err_t set_ap_password(const char *password)
{
    size_t len = password ? strlen(password) : 0;
    ESP_RETURN_ON_FALSE(len == 0 || (len >= 8 && len < sizeof(s_ap_password)), ESP_ERR_INVALID_ARG, TAG,
                        "setup hotspot password needs 8-64 characters");
    strlcpy(s_ap_password, len ? password : "", sizeof(s_ap_password));
    s_ap_password_changed |= s_portal != WIFI_MGR_PORTAL_OFF;
    return ESP_OK;
}

/* ---- events ------------------------------------------------------------------------------------------------ */

static void on_sta_disconnected(const wifi_event_sta_disconnected_t *event)
{
    if (s_expect_disconnect && event->reason == WIFI_REASON_ASSOC_LEAVE) {
        s_expect_disconnect = false;
        return;
    }
    if (s_scanning) {
        return;
    }

    bool was_connected = s_state == WIFI_MGR_STA_CONNECTED;
    s_last_reason = event->reason;
    ESP_LOGW(TAG, "disconnected from '%.32s', reason %d", event->ssid, event->reason);

    if (s_trial_state == WIFI_MGR_TRIAL_PENDING && ++s_trial_failures >= TRIAL_ATTEMPTS) {
        ESP_LOGW(TAG, "new credentials for '%s' do not work", s_trial.ssid);
        s_trial_state = WIFI_MGR_TRIAL_FAILED;
        s_retry_count = 0;
        sta_connect(); // back to the stored credentials, if any
        return;
    }

    if (active_credentials() == NULL) {
        s_state = WIFI_MGR_STA_UNCONFIGURED;
        return;
    }

    s_state = WIFI_MGR_STA_DISCONNECTED;
    schedule_retry();
    if (was_connected) {
        start_fallback_timer();
    }
}

static void on_got_ip(const ip_event_got_ip_t *event)
{
    ESP_LOGI(TAG, "connected, ip " IPSTR, IP2STR(&event->ip_info.ip));
    s_state = WIFI_MGR_STA_CONNECTED;
    s_expect_disconnect = false;
    s_retry_count = 0;
    esp_timer_stop(s_retry_timer);
    esp_timer_stop(s_fallback_timer);

    bool new_network = s_trial_state == WIFI_MGR_TRIAL_PENDING;
    if (new_network) {
        esp_err_t err = store_credentials(&s_trial);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "storing credentials failed: %s", esp_err_to_name(err));
        }
        s_saved = s_trial;
        s_trial_state = WIFI_MGR_TRIAL_OK;
    }

    // a hotspot opened on request stays until it is unused, unless it was used to change the network
    if (s_portal != WIFI_MGR_PORTAL_OFF && (s_portal != WIFI_MGR_PORTAL_MANUAL || new_network)) {
        esp_timer_stop(s_portal_stop_timer);
        esp_timer_start_once(s_portal_stop_timer, (uint64_t)CONFIG_WIFI_MGR_PORTAL_LINGER_SEC * 1000000);
    }
}

static void event_handler(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    LOCK();
    if (base == WIFI_EVENT) {
        switch (id) {
        case WIFI_EVENT_STA_START:
            sta_connect();
            break;
        case WIFI_EVENT_STA_DISCONNECTED:
            on_sta_disconnected(data);
            break;
        case WIFI_EVENT_AP_STACONNECTED:
            ESP_LOGI(TAG, "client " MACSTR " joined the setup hotspot",
                     MAC2STR(((wifi_event_ap_staconnected_t *)data)->mac));
            break;
        case WIFI_EVENT_AP_STADISCONNECTED:
            if (s_portal == WIFI_MGR_PORTAL_MANUAL) {
                portal_arm_idle_timer(); // count the idle time from the moment the last client left
            }
            break;
        default:
            break;
        }
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        on_got_ip(data);
    }
    UNLOCK();
}

/* ---- public API -------------------------------------------------------------------------------------------- */

esp_err_t wifi_mgr_start(const wifi_mgr_config_t *cfg)
{
    s_lock = xSemaphoreCreateMutex();
    ESP_RETURN_ON_FALSE(s_lock, ESP_ERR_NO_MEM, TAG, "mutex");

    // the vendored captive portal DNS server logs every query at info level
    esp_log_level_set("example_dns_redirect_server", ESP_LOG_WARN);

    LOCK();
    if (set_ap_password(cfg->ap_password) != ESP_OK || s_ap_password[0] == '\0') {
        ESP_LOGW(TAG, "the setup hotspot is open, set a password on the setup page to secure it");
    }
    UNLOCK();

    uint8_t mac[6];
    ESP_RETURN_ON_ERROR(esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP), TAG, "read mac");
    snprintf(s_ap_ssid, sizeof(s_ap_ssid), "%s-%02X%02X", CONFIG_WIFI_MGR_AP_SSID_PREFIX, mac[4], mac[5]);

    s_sta_netif = esp_netif_create_default_wifi_sta();
    s_ap_netif = esp_netif_create_default_wifi_ap();
    if (cfg->hostname) {
        esp_netif_set_hostname(s_sta_netif, cfg->hostname);
    }

    const esp_timer_create_args_t retry_args = { .callback = retry_timer_cb, .name = "wifi_retry" };
    const esp_timer_create_args_t fallback_args = { .callback = fallback_timer_cb, .name = "wifi_fallback" };
    const esp_timer_create_args_t stop_args = { .callback = portal_stop_timer_cb, .name = "portal_stop" };
    const esp_timer_create_args_t idle_args = { .callback = portal_idle_timer_cb, .name = "portal_idle" };
    ESP_RETURN_ON_ERROR(esp_timer_create(&retry_args, &s_retry_timer), TAG, "timer");
    ESP_RETURN_ON_ERROR(esp_timer_create(&fallback_args, &s_fallback_timer), TAG, "timer");
    ESP_RETURN_ON_ERROR(esp_timer_create(&stop_args, &s_portal_stop_timer), TAG, "timer");
    ESP_RETURN_ON_ERROR(esp_timer_create(&idle_args, &s_portal_idle_timer), TAG, "timer");

    wifi_init_config_t init_cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(esp_wifi_init(&init_cfg), TAG, "wifi init");
    // credentials are managed by us, keep the driver from writing its own copy to flash
    ESP_RETURN_ON_ERROR(esp_wifi_set_storage(WIFI_STORAGE_RAM), TAG, "wifi storage");

    ESP_RETURN_ON_ERROR(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, event_handler, NULL), TAG, "evt");
    ESP_RETURN_ON_ERROR(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, event_handler, NULL), TAG, "evt");

    LOCK();
    load_credentials(&s_saved);
    s_state = s_saved.ssid[0] ? WIFI_MGR_STA_CONNECTING : WIFI_MGR_STA_UNCONFIGURED;
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    if (s_saved.ssid[0]) {
        ESP_LOGI(TAG, "connecting to '%s'", s_saved.ssid);
        start_fallback_timer();
    } else {
        ESP_LOGI(TAG, "no Wi-Fi configured");
        portal_open(WIFI_MGR_PORTAL_UNCONFIGURED);
    }
    UNLOCK();

    return esp_wifi_start(); // connecting continues in WIFI_EVENT_STA_START
}

void wifi_mgr_get_status(wifi_mgr_status_t *out)
{
    memset(out, 0, sizeof(*out));
    LOCK();
    out->sta_state = s_state;
    const credentials_t *creds = active_credentials();
    if (creds) {
        strlcpy(out->ssid, creds->ssid, sizeof(out->ssid));
    }
    out->last_reason = s_last_reason;
    out->portal = s_portal;
    strlcpy(out->ap_ssid, s_ap_ssid, sizeof(out->ap_ssid));
    out->ap_secured = s_portal != WIFI_MGR_PORTAL_OFF ? s_portal_secured : s_ap_password[0] != '\0';
    out->trial = s_trial_state;
    UNLOCK();

    if (out->sta_state == WIFI_MGR_STA_CONNECTED) {
        esp_netif_ip_info_t ip;
        if (esp_netif_get_ip_info(s_sta_netif, &ip) == ESP_OK) {
            snprintf(out->ip, sizeof(out->ip), IPSTR, IP2STR(&ip.ip));
        }
        wifi_ap_record_t ap;
        if (esp_wifi_sta_get_ap_info(&ap) == ESP_OK) {
            out->rssi = ap.rssi;
        }
    }
}

bool wifi_mgr_portal_active(void)
{
    LOCK();
    bool active = s_portal != WIFI_MGR_PORTAL_OFF;
    UNLOCK();
    return active;
}

wifi_mgr_portal_t wifi_mgr_portal_mode(void)
{
    LOCK();
    wifi_mgr_portal_t mode = s_portal;
    UNLOCK();
    return mode;
}

static int compare_rssi(const void *a, const void *b)
{
    return ((const wifi_mgr_ap_t *)b)->rssi - ((const wifi_mgr_ap_t *)a)->rssi;
}

esp_err_t wifi_mgr_scan(wifi_mgr_ap_t *out, size_t max, size_t *found)
{
    *found = 0;

    LOCK();
    if (s_scanning) {
        UNLOCK();
        return ESP_ERR_INVALID_STATE;
    }
    s_scanning = true;
    esp_timer_stop(s_retry_timer);
    bool was_connected = s_state == WIFI_MGR_STA_CONNECTED;
    if (!was_connected) {
        // a pending connection attempt blocks scanning, abort it (scanning while connected is fine)
        sta_disconnect_on_purpose();
    }
    UNLOCK();

    wifi_scan_config_t scan_cfg = { .show_hidden = false };
    esp_err_t err = esp_wifi_scan_start(&scan_cfg, true);

    wifi_ap_record_t *records = NULL;
    uint16_t num = SCAN_MAX_RECORDS;
    if (err == ESP_OK) {
        records = calloc(SCAN_MAX_RECORDS, sizeof(*records));
        err = records ? esp_wifi_scan_get_ap_records(&num, records) : ESP_ERR_NO_MEM;
    }
    if (err != ESP_OK) {
        esp_wifi_clear_ap_list();
    }

    for (uint16_t i = 0; err == ESP_OK && i < num; i++) {
        const char *ssid = (const char *)records[i].ssid;
        if (ssid[0] == '\0') {
            continue;
        }
        size_t j = 0;
        while (j < *found && strcmp(out[j].ssid, ssid) != 0) {
            j++;
        }
        if (j < *found) {
            if (records[i].rssi > out[j].rssi) {
                out[j].rssi = records[i].rssi;
            }
            continue;
        }
        if (*found == max) {
            continue;
        }
        strlcpy(out[j].ssid, ssid, sizeof(out[j].ssid));
        out[j].rssi = records[i].rssi;
        out[j].secured = records[i].authmode != WIFI_AUTH_OPEN;
        (*found)++;
    }
    free(records);
    qsort(out, *found, sizeof(*out), compare_rssi);

    LOCK();
    s_scanning = false;
    if (!was_connected && active_credentials()) {
        s_state = WIFI_MGR_STA_DISCONNECTED;
        s_retry_count = 0;
        schedule_retry();
    }
    UNLOCK();

    return err;
}

esp_err_t wifi_mgr_connect(const char *ssid, const char *password)
{
    size_t ssid_len = ssid ? strlen(ssid) : 0;
    size_t pass_len = password ? strlen(password) : 0;
    ESP_RETURN_ON_FALSE(ssid_len >= 1 && ssid_len <= 32, ESP_ERR_INVALID_ARG, TAG, "invalid ssid");
    ESP_RETURN_ON_FALSE(pass_len == 0 || (pass_len >= 8 && pass_len <= 64), ESP_ERR_INVALID_ARG, TAG,
                        "invalid password");

    LOCK();
    strlcpy(s_trial.ssid, ssid, sizeof(s_trial.ssid));
    strlcpy(s_trial.password, password ? password : "", sizeof(s_trial.password));
    s_trial_state = WIFI_MGR_TRIAL_PENDING;
    s_trial_failures = 0;
    s_retry_count = 0;
    esp_timer_stop(s_retry_timer);
    esp_timer_stop(s_portal_stop_timer);
    ESP_LOGI(TAG, "trying '%s'", s_trial.ssid);
    sta_disconnect_on_purpose();
    sta_connect();
    UNLOCK();
    return ESP_OK;
}

esp_err_t wifi_mgr_forget(void)
{
    ESP_LOGW(TAG, "forgetting Wi-Fi credentials");
    esp_err_t err = erase_credentials();

    LOCK();
    sta_disconnect_on_purpose();
    memset(&s_saved, 0, sizeof(s_saved));
    s_trial_state = WIFI_MGR_TRIAL_NONE;
    s_state = WIFI_MGR_STA_UNCONFIGURED;
    esp_timer_stop(s_retry_timer);
    esp_timer_stop(s_portal_stop_timer);
    if (s_ap_password_changed) {
        portal_stop(); // reopen it with the new password, e.g. after the setup button reset it
    }
    portal_open(WIFI_MGR_PORTAL_UNCONFIGURED);
    UNLOCK();
    return err;
}

esp_err_t wifi_mgr_open_portal(void)
{
    LOCK();
    if (s_saved.ssid[0] == '\0') {
        portal_open(WIFI_MGR_PORTAL_UNCONFIGURED); // nothing to fall back to, stays open anyway
    } else {
        ESP_LOGI(TAG, "opening the setup hotspot on request, it closes after %d s without clients",
                 CONFIG_WIFI_MGR_PORTAL_IDLE_SEC);
        esp_timer_stop(s_portal_stop_timer);
        portal_open(WIFI_MGR_PORTAL_MANUAL);
    }
    UNLOCK();
    return ESP_OK;
}

esp_err_t wifi_mgr_set_hostname(const char *hostname)
{
    return esp_netif_set_hostname(s_sta_netif, hostname);
}

esp_err_t wifi_mgr_set_ap_password(const char *password)
{
    LOCK();
    esp_err_t err = set_ap_password(password);
    UNLOCK();
    return err;
}
