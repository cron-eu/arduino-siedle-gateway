/*
 * Device status as JSON, shared by the web UI (/api/status) and the retained MQTT status message.
 */
#pragma once

#include <stdbool.h>

#include "cJSON.h"

void app_status_init(const char *hostname, const char *client_id);

void app_status_set_time_synced(void);

bool app_status_time_synced(void);

/** Returns a new JSON object, the caller owns it. */
cJSON *app_status_build(void);
