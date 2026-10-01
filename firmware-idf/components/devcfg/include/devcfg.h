/*
 * Read-only per-device configuration, stored in the "devcfg" NVS partition (see devcfg/README.md).
 *
 * All values are optional: without a cloud identity the gateway still works locally (web UI, setup hotspot).
 */
#pragma once

#include <stdbool.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char *mqtt_uri;     /**< e.g. mqtts://xxxxxxxx-ats.iot.eu-central-1.amazonaws.com:8883 */
    char *client_id;    /**< MQTT client id, must match the AWS IoT thing / policy */
    char *client_cert;  /**< PEM */
    char *client_key;   /**< PEM */
    char *ap_password;  /**< WPA2 password of the setup hotspot, open hotspot if unset */
    char *hostname;     /**< overrides CONFIG_GATEWAY_HOSTNAME */
} devcfg_t;

/**
 * Load the configuration. Missing keys (or a blank partition) leave the fields NULL.
 * Strings are heap allocated and live for the lifetime of the application.
 */
esp_err_t devcfg_load(devcfg_t *out);

bool devcfg_has_cloud(const devcfg_t *cfg);

#ifdef __cplusplus
}
#endif
