/*
 * Siedle In-Home bus <-> AWS IoT gateway.
 */
#include "esp_app_desc.h"
#include "esp_log.h"
#include "nvs_flash.h"

static const char *TAG = "main";

static esp_err_t init_nvs(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS partition is full or has a newer format, erasing it");
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    return err;
}

void app_main(void)
{
    const esp_app_desc_t *app = esp_app_get_description();
    ESP_LOGI(TAG, "%s %s", app->project_name, app->version);

    ESP_ERROR_CHECK(init_nvs());
}
