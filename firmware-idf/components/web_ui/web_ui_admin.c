/*
 * Cloud and device settings, see web_ui.h.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cloud.h"
#include "esp_check.h"
#include "identity.h"
#include "mbedtls/platform_util.h"
#include "web_ui_internal.h"

#define CLOUD_BODY_LEN 8192 // a certificate is 1.2-2 KB, more if a whole chain is pasted

static const char *TAG = "web_ui";

static web_ui_config_t s_cfg;

/* ---- cloud ------------------------------------------------------------------------------------------------- */

static cJSON *cloud_doc(void)
{
    cJSON *doc = cJSON_CreateObject();
    if (doc == NULL) {
        return NULL;
    }
    identity_t id;
    identity_load(&id);
    cJSON_AddStringToObject(doc, "endpoint", id.endpoint ? id.endpoint : "");
    cJSON_AddStringToObject(doc, "thing_name", id.thing ? id.thing : "");
    cJSON_AddBoolToObject(doc, "key", id.key != NULL);
    identity_free(&id);

    identity_cert_info_t info;
    if (identity_cert_info(&info) == ESP_OK) {
        cJSON *cert = cJSON_AddObjectToObject(doc, "certificate");
        cJSON_AddStringToObject(cert, "subject", info.subject);
        cJSON_AddStringToObject(cert, "issuer", info.issuer);
        cJSON_AddStringToObject(cert, "not_after", info.not_after);
        cJSON_AddStringToObject(cert, "fingerprint", info.fingerprint);
    } else {
        cJSON_AddNullToObject(doc, "certificate");
    }

    cloud_status_t status;
    cloud_get_status(&status);
    cJSON_AddBoolToObject(doc, "connected", status.connected);
    cJSON_AddStringToObject(doc, "error", status.error);
    return doc;
}

static esp_err_t cloud_get_handler(httpd_req_t *req)
{
    if (!web_ui_require_admin(req)) {
        return ESP_OK;
    }
    return web_ui_send_json(req, cloud_doc());
}

static const char *cert_error(identity_cert_check_t check)
{
    switch (check) {
    case IDENTITY_CERT_OK:
        return NULL;
    case IDENTITY_CERT_INVALID:
        return "This is not a certificate. Use the device certificate file from AWS IoT (...-certificate.pem.crt).";
    case IDENTITY_CERT_WRONG_KEY:
        return "This certificate is for another key. Create it in AWS IoT from this gateway's current CSR.";
    default:
        return "The gateway has no key yet.";
    }
}

// {"endpoint":..,"thing_name":..,"certificate":..}, without a certificate the current one is kept
static esp_err_t cloud_post_handler(httpd_req_t *req)
{
    if (!web_ui_require_admin(req)) {
        return ESP_OK;
    }
    cJSON *json = web_ui_recv_json(req, CLOUD_BODY_LEN);
    const cJSON *endpoint = cJSON_GetObjectItemCaseSensitive(json, "endpoint");
    const cJSON *thing = cJSON_GetObjectItemCaseSensitive(json, "thing_name");
    const cJSON *cert = cJSON_GetObjectItemCaseSensitive(json, "certificate");
    const char *cert_pem = cJSON_IsString(cert) && cert->valuestring[0] ? cert->valuestring : NULL;

    const char *error = NULL;
    if (!cJSON_IsString(endpoint) || !cJSON_IsString(thing) || !(cert == NULL || cJSON_IsString(cert))) {
        error = "invalid body";
    } else {
        identity_endpoint_normalize(endpoint->valuestring);
        if (!identity_endpoint_valid(endpoint->valuestring)) {
            error = "Enter the endpoint from the AWS IoT console (Settings, Device data endpoint).";
        } else if (!identity_thing_valid(thing->valuestring)) {
            error = "The thing name can only have letters, digits, '-', '_' and ':' (up to 128).";
        } else if (cert_pem) {
            error = cert_error(identity_check_cert(cert_pem));
        }
    }
    esp_err_t err = error ? ESP_OK : identity_set_cloud(endpoint->valuestring, thing->valuestring, cert_pem);
    cJSON_Delete(json);

    if (error) {
        return web_ui_send_error(req, "400 Bad Request", error);
    }
    if (err != ESP_OK) {
        return web_ui_send_error(req, "500 Internal Server Error", esp_err_to_name(err));
    }
    if (s_cfg.on_cloud_changed) {
        s_cfg.on_cloud_changed();
    }
    return web_ui_send_json(req, cloud_doc());
}

// ?thing_name=<name>, thing names only use characters that need no URL encoding
static esp_err_t csr_handler(httpd_req_t *req)
{
    if (!web_ui_require_admin(req)) {
        return ESP_OK;
    }
    char query[IDENTITY_THING_MAX + 16];
    char thing[IDENTITY_THING_MAX + 1];
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) != ESP_OK
        || httpd_query_key_value(query, "thing_name", thing, sizeof(thing)) != ESP_OK
        || !identity_thing_valid(thing)) {
        return web_ui_send_error(req, "400 Bad Request", "Enter the thing name first.");
    }

    char *csr = NULL;
    esp_err_t err = identity_csr(thing, &csr);
    if (err == ESP_ERR_INVALID_STATE) {
        return web_ui_send_error(req, "409 Conflict", "The gateway has no key yet.");
    }
    if (err != ESP_OK) {
        return web_ui_send_error(req, "500 Internal Server Error", esp_err_to_name(err));
    }
    char disposition[sizeof("attachment; filename=\".csr\"") + IDENTITY_THING_MAX];
    snprintf(disposition, sizeof(disposition), "attachment; filename=\"%s.csr\"", thing);
    httpd_resp_set_type(req, "application/pkcs10");
    httpd_resp_set_hdr(req, "Content-Disposition", disposition);
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    err = httpd_resp_sendstr(req, csr);
    free(csr);
    return err;
}

// Replaces the key, which also deletes the certificate. Takes {} as body, the JSON content type is what counts.
static esp_err_t key_handler(httpd_req_t *req)
{
    if (!web_ui_require_admin(req)) {
        return ESP_OK;
    }
    cJSON *json = web_ui_recv_json(req, WEB_UI_SMALL_BODY_LEN);
    if (json == NULL) {
        return web_ui_send_error(req, "400 Bad Request", "invalid body");
    }
    cJSON_Delete(json);

    esp_err_t err = identity_new_key();
    if (err != ESP_OK) {
        return web_ui_send_error(req, "500 Internal Server Error", esp_err_to_name(err));
    }
    if (s_cfg.on_cloud_changed) {
        s_cfg.on_cloud_changed();
    }
    return web_ui_send_json(req, cloud_doc());
}

/* ---- device ------------------------------------------------------------------------------------------------ */

static cJSON *device_doc(void)
{
    cJSON *doc = cJSON_CreateObject();
    if (doc == NULL) {
        return NULL;
    }
    identity_t id;
    identity_load(&id);
    cJSON_AddStringToObject(doc, "hostname", id.hostname ? id.hostname : "");
    cJSON_AddBoolToObject(doc, "ap_password", id.ap_password != NULL);
    identity_free(&id);
    return doc;
}

static esp_err_t device_get_handler(httpd_req_t *req)
{
    if (!web_ui_require_admin(req)) {
        return ESP_OK;
    }
    return web_ui_send_json(req, device_doc());
}

// {"hostname":..,"ap_password":..}, a missing value stays as it is, "" restores the default
static esp_err_t device_post_handler(httpd_req_t *req)
{
    if (!web_ui_require_admin(req)) {
        return ESP_OK;
    }
    cJSON *json = web_ui_recv_json(req, WEB_UI_SMALL_BODY_LEN);
    const cJSON *hostname = cJSON_GetObjectItemCaseSensitive(json, "hostname");
    const cJSON *password = cJSON_GetObjectItemCaseSensitive(json, "ap_password");

    const char *error = NULL;
    if (json == NULL || !(hostname == NULL || cJSON_IsString(hostname))
        || !(password == NULL || cJSON_IsString(password))) {
        error = "invalid body";
    } else if (hostname && hostname->valuestring[0] && !identity_hostname_valid(hostname->valuestring)) {
        error = "The name can only have letters, digits and hyphens (up to 63), no hyphen at either end.";
    } else if (password && password->valuestring[0] && !identity_ap_password_valid(password->valuestring)) {
        error = "The hotspot password needs 8 to 63 characters (letters, digits, punctuation).";
    }
    esp_err_t err = ESP_OK;
    if (error == NULL && hostname) {
        err = identity_set_hostname(hostname->valuestring);
    }
    if (error == NULL && password && err == ESP_OK) {
        err = identity_set_ap_password(password->valuestring);
    }
    if (cJSON_IsString(password)) {
        mbedtls_platform_zeroize(password->valuestring, strlen(password->valuestring));
    }
    cJSON_Delete(json);

    if (error) {
        return web_ui_send_error(req, "400 Bad Request", error);
    }
    if (err != ESP_OK) {
        return web_ui_send_error(req, "500 Internal Server Error", esp_err_to_name(err));
    }
    if (s_cfg.on_device_changed) {
        s_cfg.on_device_changed();
    }
    return web_ui_send_json(req, device_doc());
}

esp_err_t web_ui_admin_register(httpd_handle_t server, const web_ui_config_t *cfg)
{
    s_cfg = *cfg;
    const httpd_uri_t handlers[] = {
        { .uri = "/api/cloud", .method = HTTP_GET, .handler = cloud_get_handler },
        { .uri = "/api/cloud", .method = HTTP_POST, .handler = cloud_post_handler },
        { .uri = "/api/cloud/csr", .method = HTTP_GET, .handler = csr_handler },
        { .uri = "/api/cloud/key", .method = HTTP_POST, .handler = key_handler },
        { .uri = "/api/device", .method = HTTP_GET, .handler = device_get_handler },
        { .uri = "/api/device", .method = HTTP_POST, .handler = device_post_handler },
    };
    for (size_t i = 0; i < sizeof(handlers) / sizeof(handlers[0]); i++) {
        ESP_RETURN_ON_ERROR(httpd_register_uri_handler(server, &handlers[i]), TAG, "register %s", handlers[i].uri);
    }
    return ESP_OK;
}
