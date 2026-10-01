/*
 * Wi-Fi station management with a captive portal setup hotspot.
 *
 * - Credentials are kept in the "wifi" NVS namespace, only after they were proven to work.
 * - Without credentials, or when the station could not connect for CONFIG_WIFI_MGR_PORTAL_FALLBACK_SEC, the
 *   setup hotspot ("<prefix>-XXXX") is started next to the station interface. A DNS server answers every query
 *   with the hotspot address so phones and laptops pop up the setup page automatically.
 * - Once the station is connected, the hotspot is shut down after CONFIG_WIFI_MGR_PORTAL_LINGER_SEC, giving the
 *   setup page time to show the result.
 *
 * Other components can rely on the regular IP_EVENT_STA_GOT_IP / WIFI_EVENT_STA_DISCONNECTED events.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char *hostname;    /**< DHCP hostname of the station interface */
    const char *ap_password; /**< setup hotspot WPA2 password (>= 8 chars), NULL or empty for an open hotspot */
} wifi_mgr_config_t;

typedef enum {
    WIFI_MGR_STA_UNCONFIGURED,
    WIFI_MGR_STA_CONNECTING,
    WIFI_MGR_STA_CONNECTED,
    WIFI_MGR_STA_DISCONNECTED, /**< connection lost or failed, retrying with backoff */
} wifi_mgr_sta_state_t;

/** Outcome of the last wifi_mgr_connect() call */
typedef enum {
    WIFI_MGR_TRIAL_NONE,
    WIFI_MGR_TRIAL_PENDING,
    WIFI_MGR_TRIAL_OK,
    WIFI_MGR_TRIAL_FAILED,
} wifi_mgr_trial_t;

typedef struct {
    wifi_mgr_sta_state_t sta_state;
    char ssid[33];        /**< configured network, empty if unconfigured */
    char ip[16];          /**< station IPv4 address, empty if not connected */
    int8_t rssi;          /**< dBm, 0 if not connected */
    uint8_t last_reason;  /**< last wifi_err_reason_t of a disconnect */
    bool portal_active;
    char ap_ssid[33];
    bool ap_secured;
    wifi_mgr_trial_t trial;
} wifi_mgr_status_t;

typedef struct {
    char ssid[33];
    int8_t rssi;
    bool secured;
} wifi_mgr_ap_t;

/** Initialize Wi-Fi (esp_netif and the default event loop must exist) and start connecting. */
esp_err_t wifi_mgr_start(const wifi_mgr_config_t *cfg);

void wifi_mgr_get_status(wifi_mgr_status_t *out);

bool wifi_mgr_portal_active(void);

/**
 * Scan for networks (blocking, ~2-3 s). Results are de-duplicated by SSID and sorted by signal strength.
 */
esp_err_t wifi_mgr_scan(wifi_mgr_ap_t *out, size_t max, size_t *found);

/**
 * Try to connect to a network. Returns immediately, the outcome is reported via wifi_mgr_status_t.trial.
 * The credentials are stored only after the connection succeeded, on failure the previous ones are restored.
 */
esp_err_t wifi_mgr_connect(const char *ssid, const char *password);

/** Erase the stored credentials and open the setup hotspot. */
esp_err_t wifi_mgr_forget(void);

#ifdef __cplusplus
}
#endif
