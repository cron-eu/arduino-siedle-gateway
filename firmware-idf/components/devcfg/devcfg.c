#include "devcfg.h"

#include <stdlib.h>
#include <string.h>

#include "esp_check.h"
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"

#define DEVCFG_PARTITION "devcfg"
#define DEVCFG_NAMESPACE "devcfg"

static const char *TAG = "devcfg";

static char *read_str(nvs_handle_t nvs, const char *key)
{
    size_t len = 0;
    if (nvs_get_str(nvs, key, NULL, &len) != ESP_OK || len <= 1) {
        return NULL;
    }
    char *value = malloc(len);
    if (value && nvs_get_str(nvs, key, value, &len) != ESP_OK) {
        free(value);
        value = NULL;
    }
    return value;
}

esp_err_t devcfg_load(devcfg_t *out)
{
    memset(out, 0, sizeof(*out));

    esp_err_t err = nvs_flash_init_partition(DEVCFG_PARTITION);
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // blank or foreign partition: never erase it, it is provisioned from the host
        ESP_LOGW(TAG, "partition not provisioned (%s)", esp_err_to_name(err));
        return ESP_OK;
    }
    ESP_RETURN_ON_ERROR(err, TAG, "init partition");

    nvs_handle_t nvs;
    err = nvs_open_from_partition(DEVCFG_PARTITION, DEVCFG_NAMESPACE, NVS_READONLY, &nvs);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "partition is empty");
        return ESP_OK;
    }
    ESP_RETURN_ON_ERROR(err, TAG, "open namespace");

    out->mqtt_uri = read_str(nvs, "mqtt_uri");
    out->client_id = read_str(nvs, "client_id");
    out->client_cert = read_str(nvs, "client_cert");
    out->client_key = read_str(nvs, "client_key");
    out->ap_password = read_str(nvs, "ap_pass");
    out->hostname = read_str(nvs, "hostname");
    nvs_close(nvs);

    ESP_LOGI(TAG, "cloud identity: %s", devcfg_has_cloud(out) ? out->client_id : "(none)");
    return ESP_OK;
}

bool devcfg_has_cloud(const devcfg_t *cfg)
{
    return cfg->mqtt_uri && cfg->client_id && cfg->client_cert && cfg->client_key;
}
