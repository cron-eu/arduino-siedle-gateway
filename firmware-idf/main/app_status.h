/*
 * Device status as JSON, served by the web UI (/api/status).
 */
#pragma once

#include "cJSON.h"

void app_status_init(const char *hostname);

/** Returns a new JSON object, the caller owns it. */
cJSON *app_status_build(void);
