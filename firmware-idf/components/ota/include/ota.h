/*
 * Firmware updates over HTTPS with rollback protection.
 *
 * A freshly installed image boots in "pending verify" state. It must call ota_mark_valid() once it proved to
 * work (e.g. connected to the cloud), otherwise the bootloader rolls back to the previous image on the next reset.
 */
#pragma once

#include <stdbool.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Arm the rollback deadline: if the running image is pending verification and ota_mark_valid() is not called
 * within CONFIG_OTA_CONFIRM_TIMEOUT_SEC, the device reboots into the previous image.
 */
esp_err_t ota_init(void);

/**
 * Start downloading and installing an image in the background, then reboot.
 *
 * The server certificate is verified against the built-in CA bundle and the image must belong to the same
 * project. Returns ESP_ERR_INVALID_STATE if an update is already running.
 */
esp_err_t ota_start(const char *url);

bool ota_in_progress(void);

/** Confirm the running image if it is pending verification, no-op otherwise. */
void ota_mark_valid(void);

#ifdef __cplusplus
}
#endif
