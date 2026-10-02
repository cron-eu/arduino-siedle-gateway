/*
 * Device identity and per-device settings, kept in the "identity" namespace of the nvs partition, so all devices
 * run the same firmware image and OTA updates never touch them:
 *
 *   key        EC P-256 private key (PEM), generated on the device, it never leaves it
 *   cert       AWS IoT certificate for that key (PEM), created from identity_csr()
 *   endpoint   AWS IoT endpoint, e.g. xxxxxxxxxxxxxx-ats.iot.eu-central-1.amazonaws.com
 *   thing      AWS IoT thing name, also the MQTT client id
 *   ap_pass    WPA2 password of the setup hotspot, open hotspot if unset
 *   hostname   overrides CONFIG_GATEWAY_HOSTNAME
 *
 * Apart from the key, everything is set up on the setup page. The functions are thread safe.
 */
#pragma once

#include <stdbool.h>

#include "esp_err.h"
#include "identity_validate.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char *endpoint;
    char *thing;
    char *cert;        /**< PEM */
    char *key;         /**< PEM */
    char *ap_password;
    char *hostname;
} identity_t;

typedef enum {
    IDENTITY_CERT_OK,
    IDENTITY_CERT_INVALID,   /**< not a PEM certificate, or too large */
    IDENTITY_CERT_WRONG_KEY, /**< issued for a different key than the device key */
    IDENTITY_CERT_NO_KEY,    /**< there is no device key */
} identity_cert_check_t;

typedef struct {
    char subject[128];
    char issuer[160];
    char not_after[24];   /**< ISO 8601 UTC, e.g. 2049-12-31T23:59:59Z */
    char fingerprint[65]; /**< SHA-256 of the certificate (DER) in lowercase hex, the certificate ID in AWS IoT */
} identity_cert_info_t;

/** Call once, after nvs_flash_init(). */
esp_err_t identity_init(void);

/** Read everything, missing values are NULL. Release with identity_free(), which also wipes the secrets. */
esp_err_t identity_load(identity_t *out);

void identity_free(identity_t *id);

/** Endpoint, thing name, certificate and key are all set */
bool identity_has_cloud(const identity_t *id);

/**
 * Generate the device key if there is none yet. Call once Wi-Fi is started: only then the hardware random number
 * generator is a true one.
 */
esp_err_t identity_ensure_key(void);

/** Replace the device key. The certificate is deleted, since it no longer matches. */
esp_err_t identity_new_key(void);

/**
 * Certificate signing request for the device key, with CN=<thing>, PEM encoded. Free the result with free().
 * ESP_ERR_INVALID_STATE if there is no device key.
 */
esp_err_t identity_csr(const char *thing, char **out);

/** Check that a PEM certificate belongs to the device key. */
identity_cert_check_t identity_check_cert(const char *pem);

/**
 * Store the AWS IoT settings. cert is a PEM certificate for the device key (only the first one of a chain is
 * kept), NULL keeps the current certificate. ESP_ERR_INVALID_ARG if a value does not pass the checks.
 */
esp_err_t identity_set_cloud(const char *endpoint, const char *thing, const char *cert);

/** Summary of the stored certificate, ESP_ERR_NOT_FOUND if there is none. */
esp_err_t identity_cert_info(identity_cert_info_t *out);

/** NULL or "" restores the default hostname. */
esp_err_t identity_set_hostname(const char *hostname);

/** NULL or "" for an open setup hotspot. */
esp_err_t identity_set_ap_password(const char *password);

#ifdef __cplusplus
}
#endif
