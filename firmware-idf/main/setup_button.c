#include "setup_button.h"

#include <inttypes.h>

#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "identity.h"
#include "sdkconfig.h"
#include "wifi_mgr.h"

#define POLL_MS 50

#if CONFIG_GATEWAY_SETUP_BUTTON_GPIO >= 0
_Static_assert(CONFIG_GATEWAY_SETUP_BUTTON_RESET_SEC > CONFIG_GATEWAY_SETUP_BUTTON_HOTSPOT_SEC,
               "the reset hold time must be longer than the hotspot hold time");

static const char *TAG = "setup_button";

static void button_task(void *arg)
{
    const gpio_num_t pin = (gpio_num_t)CONFIG_GATEWAY_SETUP_BUTTON_GPIO;
    const uint32_t hotspot_ms = CONFIG_GATEWAY_SETUP_BUTTON_HOTSPOT_SEC * 1000;
    const uint32_t reset_ms = CONFIG_GATEWAY_SETUP_BUTTON_RESET_SEC * 1000;
    uint32_t pressed_ms = 0;

    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(POLL_MS));
        if (gpio_get_level(pin) == 0) {
            pressed_ms += POLL_MS;
            if (pressed_ms == reset_ms) { // fires once per press, while the button is still held
                ESP_LOGW(TAG, "held for %d s, resetting the network settings", CONFIG_GATEWAY_SETUP_BUTTON_RESET_SEC);
                // a forgotten hotspot password would lock the owner out, and whoever can press the button can
                // reach the USB port as well
                identity_set_ap_password(NULL);
                wifi_mgr_set_ap_password(NULL);
                wifi_mgr_forget();
            }
            continue;
        }
        if (pressed_ms >= hotspot_ms && pressed_ms < reset_ms) {
            ESP_LOGI(TAG, "held for %" PRIu32 " s", pressed_ms / 1000);
            wifi_mgr_open_portal();
        }
        pressed_ms = 0;
    }
}
#endif

esp_err_t setup_button_start(void)
{
#if CONFIG_GATEWAY_SETUP_BUTTON_GPIO < 0
    return ESP_OK;
#else
    const gpio_config_t cfg = {
        .pin_bit_mask = 1ULL << CONFIG_GATEWAY_SETUP_BUTTON_GPIO,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&cfg), TAG, "gpio config");
    ESP_RETURN_ON_FALSE(xTaskCreate(button_task, "setup_button", 3072, NULL, 2, NULL) == pdPASS, ESP_ERR_NO_MEM, TAG,
                        "task");
    return ESP_OK;
#endif
}
