/*
 * Long press on the reset button (BOOT on dev boards) forgets the Wi-Fi credentials and opens the setup hotspot.
 */
#pragma once

#include "esp_err.h"

esp_err_t reset_button_start(void);
