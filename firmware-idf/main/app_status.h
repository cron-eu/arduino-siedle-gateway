/*
 * Device status as JSON, shared by the web UI (/api/status) and the retained MQTT status message.
 */
#pragma once

#include <stdbool.h>

#include "cJSON.h"

/** The hostname shown in the status (copied) */
void app_status_set_hostname(const char *hostname);

void app_status_set_time_synced(void);

bool app_status_time_synced(void);

/** Returns a new JSON object, the caller owns it. */
cJSON *app_status_build(void);
