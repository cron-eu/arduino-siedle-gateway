/*
 * Setup button (BOOT on dev boards):
 *
 * - hold for CONFIG_GATEWAY_SETUP_BUTTON_HOTSPOT_SEC and release: opens the setup hotspot, the Wi-Fi network stays
 *   configured
 * - hold for CONFIG_GATEWAY_SETUP_BUTTON_RESET_SEC: forgets the Wi-Fi network and the hotspot password, and opens
 *   the setup hotspot. The cloud identity stays.
 */
#pragma once

#include "esp_err.h"

esp_err_t setup_button_start(void);
