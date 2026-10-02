/*
 * Local web UI: device status, bus event log, and the setup page on the setup hotspot (captive portal).
 *
 *   GET  /                 single page app (www/index.html)
 *   GET  /api/status       device status JSON
 *   GET  /api/log          recent bus events
 *
 *   setup hotspot only:
 *   GET  /api/wifi/scan    available networks
 *   POST /api/wifi         {"ssid":..,"password":..}
 *
 *   setup hotspot opened on site only:
 *   GET  /api/cloud        AWS IoT endpoint and thing name, device key and certificate, connection state
 *   POST /api/cloud        {"endpoint":..,"thing_name":..,"certificate":<PEM, optional>}
 *   GET  /api/cloud/csr    ?thing_name=.. certificate signing request for the device key (download)
 *   POST /api/cloud/key    {} replaces the device key, which deletes the certificate
 *   GET  /api/device       hostname, whether the setup hotspot has a password
 *   POST /api/device       {"hostname":..,"ap_password":..}, both optional, "" restores the default
 *
 * The setup endpoints are only available to clients of the setup hotspot, so on the regular network the UI is
 * read-only and changes need physical access to the device. The station may be connected while the hotspot is
 * open, so it is the interface a request comes in through that counts. /api/status tells the page with
 * "via_hotspot".
 *
 * The cloud and device settings control the door (siedle/send can open it), so they also need the hotspot to be
 * opened on site: with the setup button, or because no network is configured. The hotspot that opens by itself
 * after the gateway was offline for a while is not enough. /api/status tells the page with "settings_unlocked".
 * Development builds can unlock them from the network as well (CONFIG_WEB_UI_SETTINGS_FROM_NETWORK).
 *
 * POST bodies must be sent as application/json, which keeps other web sites from posting here.
 */
#pragma once

#include "cJSON.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    /** Returns a new JSON object describing the device (ownership is transferred) */
    cJSON *(*build_status)(void);
    /** The AWS IoT settings or the device key were changed, called on the HTTP server task (must not block) */
    void (*on_cloud_changed)(void);
    /** The hostname or the hotspot password were changed, called on the HTTP server task (must not block) */
    void (*on_device_changed)(void);
} web_ui_config_t;

esp_err_t web_ui_start(const web_ui_config_t *cfg);

#ifdef __cplusplus
}
#endif
