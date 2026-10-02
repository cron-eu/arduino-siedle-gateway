/*
 * Shared between the web_ui sources, not part of the component's API.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "cJSON.h"
#include "esp_http_server.h"
#include "web_ui.h"

#define WEB_UI_SMALL_BODY_LEN 256

/** Send the JSON and delete it, NULL sends a 500 */
esp_err_t web_ui_send_json(httpd_req_t *req, cJSON *json);

esp_err_t web_ui_send_error(httpd_req_t *req, const char *status, const char *message);

/** The request body as JSON, NULL if it is missing, larger than max_len, not sent as application/json or invalid */
cJSON *web_ui_recv_json(httpd_req_t *req, size_t max_len);

/** Whether the cloud and device settings may be changed by this request, sends a 403 if not */
bool web_ui_require_admin(httpd_req_t *req);

/** Register the cloud and device settings endpoints (web_ui_admin.c) */
esp_err_t web_ui_admin_register(httpd_handle_t server, const web_ui_config_t *cfg);
