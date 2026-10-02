#include "web_ui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_check.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "lwip/inet.h"
#include "lwip/sockets.h"
#include "siedle_log.h"
#include "wifi_mgr.h"

#define MAX_BODY_LEN  256
#define SCAN_MAX_APS  20

static const char *TAG = "web_ui";

extern const char index_html_start[] asm("_binary_index_html_start");
extern const char index_html_end[] asm("_binary_index_html_end");

static web_ui_config_t s_cfg;

static esp_err_t send_json(httpd_req_t *req, cJSON *json)
{
    char *body = json ? cJSON_PrintUnformatted(json) : NULL;
    cJSON_Delete(json);
    if (body == NULL) {
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "out of memory");
    }
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    esp_err_t err = httpd_resp_sendstr(req, body);
    free(body);
    return err;
}

static esp_err_t send_error(httpd_req_t *req, const char *status, const char *message)
{
    cJSON *json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "error", message);
    httpd_resp_set_status(req, status);
    return send_json(req, json);
}

// Whether the request came in through the setup hotspot. The station can be connected while the hotspot is open,
// and the setup endpoints must not be reachable from the regular network.
static bool via_hotspot(httpd_req_t *req)
{
    esp_netif_ip_info_t ap_ip;
    esp_netif_t *ap = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
    if (ap == NULL || esp_netif_get_ip_info(ap, &ap_ip) != ESP_OK) {
        return false;
    }
    struct sockaddr_storage local;
    socklen_t len = sizeof(local);
    if (getsockname(httpd_req_to_sockfd(req), (struct sockaddr *)&local, &len) != 0) {
        return false;
    }
    if (local.ss_family == AF_INET) {
        return ((struct sockaddr_in *)&local)->sin_addr.s_addr == ap_ip.ip.addr;
    }
    // the server socket is dual stack, IPv4 clients show up as ::ffff:a.b.c.d
    const struct sockaddr_in6 *local6 = (struct sockaddr_in6 *)&local;
    return local.ss_family == AF_INET6 && IN6_IS_ADDR_V4MAPPED(&local6->sin6_addr)
           && local6->sin6_addr.un.u32_addr[3] == ap_ip.ip.addr;
}

static esp_err_t index_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_send(req, index_html_start, index_html_end - index_html_start - 1); // drop trailing NUL
}

static esp_err_t status_handler(httpd_req_t *req)
{
    cJSON *status = s_cfg.build_status();
    if (status) {
        // tells the page whether to offer the setup
        cJSON_AddBoolToObject(status, "via_hotspot", via_hotspot(req));
    }
    return send_json(req, status);
}

static esp_err_t log_handler(httpd_req_t *req)
{
    siedle_log_entry_t *entries = calloc(CONFIG_SIEDLE_LOG_SIZE, sizeof(*entries));
    if (entries == NULL) {
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "out of memory");
    }
    size_t n = siedle_log_get(entries, CONFIG_SIEDLE_LOG_SIZE);

    cJSON *list = cJSON_CreateArray();
    for (size_t i = 0; i < n; i++) {
        const siedle_log_entry_t *e = &entries[i];
        cJSON *item = cJSON_CreateObject();
        cJSON_AddNumberToObject(item, "ts", (double)e->timestamp);
        cJSON_AddNumberToObject(item, "cmd", e->cmd);
        cJSON_AddStringToObject(item, "dir", e->dir == SIEDLE_LOG_RX ? "rx" : "tx");
        cJSON_AddBoolToObject(item, "ok", e->ok);
        cJSON_AddNumberToObject(item, "signal", siedle_cmd_signal(e->cmd));
        const char *name = siedle_signal_name(siedle_cmd_signal(e->cmd));
        if (name) {
            cJSON_AddStringToObject(item, "signal_name", name);
        }
        cJSON_AddNumberToObject(item, "src", siedle_cmd_src(e->cmd));
        cJSON_AddNumberToObject(item, "dst", siedle_cmd_dst(e->cmd));
        cJSON_AddItemToArray(list, item);
    }
    free(entries);
    return send_json(req, list);
}

static bool require_portal(httpd_req_t *req)
{
    if (wifi_mgr_portal_active() && via_hotspot(req)) {
        return true;
    }
    send_error(req, "403 Forbidden", "Wi-Fi setup is only available on the setup hotspot");
    return false;
}

static esp_err_t wifi_scan_handler(httpd_req_t *req)
{
    if (!require_portal(req)) {
        return ESP_OK;
    }
    wifi_mgr_ap_t *aps = calloc(SCAN_MAX_APS, sizeof(*aps));
    if (aps == NULL) {
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "out of memory");
    }
    size_t found = 0;
    esp_err_t err = wifi_mgr_scan(aps, SCAN_MAX_APS, &found);
    if (err != ESP_OK) {
        free(aps);
        return send_error(req, "503 Service Unavailable", esp_err_to_name(err));
    }

    cJSON *list = cJSON_CreateArray();
    for (size_t i = 0; i < found; i++) {
        cJSON *item = cJSON_CreateObject();
        cJSON_AddStringToObject(item, "ssid", aps[i].ssid);
        cJSON_AddNumberToObject(item, "rssi", aps[i].rssi);
        cJSON_AddBoolToObject(item, "secured", aps[i].secured);
        cJSON_AddItemToArray(list, item);
    }
    free(aps);
    return send_json(req, list);
}

static esp_err_t wifi_connect_handler(httpd_req_t *req)
{
    if (!require_portal(req)) {
        return ESP_OK;
    }
    if (req->content_len == 0 || req->content_len > MAX_BODY_LEN) {
        return send_error(req, "400 Bad Request", "invalid body");
    }

    char body[MAX_BODY_LEN + 1];
    size_t received = 0;
    while (received < req->content_len) {
        int ret = httpd_req_recv(req, body + received, req->content_len - received);
        if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
            continue;
        }
        if (ret <= 0) {
            return ESP_FAIL;
        }
        received += ret;
    }
    body[received] = '\0';

    cJSON *json = cJSON_Parse(body);
    const cJSON *ssid = cJSON_GetObjectItemCaseSensitive(json, "ssid");
    const cJSON *password = cJSON_GetObjectItemCaseSensitive(json, "password");
    esp_err_t err = ESP_ERR_INVALID_ARG;
    if (cJSON_IsString(ssid) && (password == NULL || cJSON_IsString(password))) {
        err = wifi_mgr_connect(ssid->valuestring, password ? password->valuestring : "");
    }
    cJSON_Delete(json);

    if (err != ESP_OK) {
        return send_error(req, "400 Bad Request",
                          "SSID must be 1-32 characters, the password empty or 8-64 characters");
    }
    httpd_resp_set_status(req, "202 Accepted");
    return send_json(req, cJSON_CreateObject());
}

// Captive portal: send every unknown URL from hotspot clients (OS connectivity checks) to the setup page
static esp_err_t not_found_handler(httpd_req_t *req, httpd_err_code_t code)
{
    if (!wifi_mgr_portal_active() || !via_hotspot(req)) {
        return httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "not found");
    }

    char location[32] = "/";
    esp_netif_ip_info_t ip;
    esp_netif_t *ap = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
    if (ap && esp_netif_get_ip_info(ap, &ip) == ESP_OK) {
        snprintf(location, sizeof(location), "http://" IPSTR "/", IP2STR(&ip.ip));
    }
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", location);
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    // iOS only detects the portal if the redirect has a body
    return httpd_resp_sendstr(req, "Redirecting to the setup page");
}

esp_err_t web_ui_start(const web_ui_config_t *cfg)
{
    s_cfg = *cfg;

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_open_sockets = 10;
    config.lru_purge_enable = true; // captive portal probes open lots of connections
    config.stack_size = 6144;

    // redirected portal probes produce lots of noise
    esp_log_level_set("httpd_uri", ESP_LOG_ERROR);
    esp_log_level_set("httpd_txrx", ESP_LOG_ERROR);
    esp_log_level_set("httpd_parse", ESP_LOG_ERROR);

    httpd_handle_t server = NULL;
    ESP_RETURN_ON_ERROR(httpd_start(&server, &config), TAG, "httpd start");

    const httpd_uri_t handlers[] = {
        { .uri = "/", .method = HTTP_GET, .handler = index_handler },
        { .uri = "/api/status", .method = HTTP_GET, .handler = status_handler },
        { .uri = "/api/log", .method = HTTP_GET, .handler = log_handler },
        { .uri = "/api/wifi/scan", .method = HTTP_GET, .handler = wifi_scan_handler },
        { .uri = "/api/wifi", .method = HTTP_POST, .handler = wifi_connect_handler },
    };
    for (size_t i = 0; i < sizeof(handlers) / sizeof(handlers[0]); i++) {
        ESP_RETURN_ON_ERROR(httpd_register_uri_handler(server, &handlers[i]), TAG, "register %s", handlers[i].uri);
    }
    ESP_RETURN_ON_ERROR(httpd_register_err_handler(server, HTTPD_404_NOT_FOUND, not_found_handler), TAG, "404");

    ESP_LOGI(TAG, "web UI started on port %d", config.server_port);
    return ESP_OK;
}
