#include "siedle_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#define TIME_VALID_AFTER 1700000000 // anything before Nov 2023 means SNTP did not sync yet

static siedle_log_entry_t s_entries[CONFIG_SIEDLE_LOG_SIZE];
static size_t s_head; // next write position
static size_t s_count;
static SemaphoreHandle_t s_lock;

esp_err_t siedle_log_init(void)
{
    if (s_lock == NULL) {
        s_lock = xSemaphoreCreateMutex();
    }
    return s_lock ? ESP_OK : ESP_ERR_NO_MEM;
}

void siedle_log_add(siedle_cmd_t cmd, siedle_log_dir_t dir, bool ok)
{
    time_t now = time(NULL);
    siedle_log_entry_t entry = {
        .timestamp = now >= TIME_VALID_AFTER ? now : 0,
        .cmd = cmd,
        .dir = dir,
        .ok = ok,
    };

    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_entries[s_head] = entry;
    s_head = (s_head + 1) % CONFIG_SIEDLE_LOG_SIZE;
    if (s_count < CONFIG_SIEDLE_LOG_SIZE) {
        s_count++;
    }
    xSemaphoreGive(s_lock);
}

size_t siedle_log_get(siedle_log_entry_t *out, size_t max)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    size_t n = s_count < max ? s_count : max;
    for (size_t i = 0; i < n; i++) {
        size_t idx = (s_head + CONFIG_SIEDLE_LOG_SIZE - 1 - i) % CONFIG_SIEDLE_LOG_SIZE;
        out[i] = s_entries[idx];
    }
    xSemaphoreGive(s_lock);
    return n;
}
