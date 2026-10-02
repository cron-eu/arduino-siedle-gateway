/*
 * Local web UI: device status, bus event log and Wi-Fi setup (captive portal).
 *
 *   GET  /                 single page app (www/index.html)
 *   GET  /api/status       device status JSON
 *   GET  /api/log          recent bus events
 *   GET  /api/wifi/scan    available networks          (setup hotspot only)
 *   POST /api/wifi         {"ssid":..,"password":..}   (setup hotspot only)
 *
 * The Wi-Fi endpoints are only available while the setup hotspot is open, so changing the network requires
 * physical access to the device (setup button) once it is installed.
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
} web_ui_config_t;

esp_err_t web_ui_start(const web_ui_config_t *cfg);

#ifdef __cplusplus
}
#endif
