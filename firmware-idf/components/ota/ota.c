#include "ota.h"

#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>

#include "esp_app_desc.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "ota";
static atomic_bool s_running;
static esp_timer_handle_t s_confirm_timer;

static bool running_image_pending(void)
{
    esp_ota_img_states_t state;
    const esp_partition_t *running = esp_ota_get_running_partition();
    return esp_ota_get_state_partition(running, &state) == ESP_OK && state == ESP_OTA_IMG_PENDING_VERIFY;
}

static void confirm_timeout_cb(void *arg)
{
    if (running_image_pending()) {
        ESP_LOGE(TAG, "new firmware was not confirmed within %d s, rolling back", CONFIG_OTA_CONFIRM_TIMEOUT_SEC);
        esp_ota_mark_app_invalid_rollback_and_reboot();
    }
}

esp_err_t ota_init(void)
{
    if (!running_image_pending()) {
        return ESP_OK;
    }
    ESP_LOGW(TAG, "running a new firmware, waiting %d s for confirmation", CONFIG_OTA_CONFIRM_TIMEOUT_SEC);
    const esp_timer_create_args_t args = { .callback = confirm_timeout_cb, .name = "ota_confirm" };
    esp_err_t err = esp_timer_create(&args, &s_confirm_timer);
    if (err == ESP_OK) {
        err = esp_timer_start_once(s_confirm_timer, (uint64_t)CONFIG_OTA_CONFIRM_TIMEOUT_SEC * 1000000);
    }
    return err;
}

static esp_err_t check_image(esp_https_ota_handle_t handle)
{
    esp_app_desc_t incoming;
    esp_err_t err = esp_https_ota_get_img_desc(handle, &incoming);
    if (err != ESP_OK) {
        return err;
    }

    const esp_app_desc_t *running = esp_app_get_description();
    ESP_LOGI(TAG, "running %s %s, incoming %s %s", running->project_name, running->version,
             incoming.project_name, incoming.version);

    if (strncmp(incoming.project_name, running->project_name, sizeof(incoming.project_name)) != 0) {
        ESP_LOGE(TAG, "image belongs to a different project");
        return ESP_ERR_INVALID_VERSION;
    }
    return ESP_OK;
}

static void ota_task(void *arg)
{
    char *url = arg;
    ESP_LOGI(TAG, "downloading %s", url);

    esp_http_client_config_t http_cfg = {
        .url = url,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms = 15000,
        .keep_alive_enable = true,
    };
    esp_https_ota_config_t ota_cfg = { .http_config = &http_cfg };

    esp_https_ota_handle_t handle = NULL;
    esp_err_t err = esp_https_ota_begin(&ota_cfg, &handle);
    if (err == ESP_OK) {
        err = check_image(handle);
    }
    while (err == ESP_OK) {
        err = esp_https_ota_perform(handle);
        if (err != ESP_ERR_HTTPS_OTA_IN_PROGRESS) {
            break;
        }
        err = ESP_OK;
    }
    if (err == ESP_OK && !esp_https_ota_is_complete_data_received(handle)) {
        err = ESP_ERR_INVALID_SIZE;
    }

    if (err == ESP_OK) {
        err = esp_https_ota_finish(handle);
    } else if (handle) {
        esp_https_ota_abort(handle);
    }
    free(url);

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "update installed, rebooting");
        vTaskDelay(pdMS_TO_TICKS(1000)); // let logs and MQTT flush
        esp_restart();
    }

    ESP_LOGE(TAG, "update failed: %s", esp_err_to_name(err));
    atomic_store(&s_running, false);
    vTaskDelete(NULL);
}

esp_err_t ota_start(const char *url)
{
    if (url == NULL || strncmp(url, "https://", 8) != 0) {
        ESP_LOGE(TAG, "only https:// URLs are accepted");
        return ESP_ERR_INVALID_ARG;
    }
    bool expected = false;
    if (!atomic_compare_exchange_strong(&s_running, &expected, true)) {
        return ESP_ERR_INVALID_STATE;
    }

    char *url_copy = strdup(url);
    if (url_copy == NULL || xTaskCreate(ota_task, "ota", 8192, url_copy, 5, NULL) != pdPASS) {
        free(url_copy);
        atomic_store(&s_running, false);
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

bool ota_in_progress(void)
{
    return atomic_load(&s_running);
}

void ota_mark_valid(void)
{
    if (running_image_pending()) {
        ESP_LOGI(TAG, "confirming new firmware, rollback cancelled");
        esp_ota_mark_app_valid_cancel_rollback();
    }
    if (s_confirm_timer) {
        esp_timer_stop(s_confirm_timer);
    }
}
