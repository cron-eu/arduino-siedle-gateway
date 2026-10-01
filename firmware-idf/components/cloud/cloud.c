#include "cloud.h"

#include <errno.h>
#include <inttypes.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_check.h"
#include "esp_crt_bundle.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "mqtt_client.h"

#define PREFIX CONFIG_CLOUD_TOPIC_PREFIX
#define TOPIC_RECEIVED PREFIX "/received"
#define TOPIC_SENT     PREFIX "/sent"
#define TOPIC_SEND     PREFIX "/send"
#define QOS            1

static const char *TAG = "cloud";

static esp_mqtt_client_handle_t s_client;
static cloud_config_t s_cfg;
static char *s_topic_status;
static esp_timer_handle_t s_status_timer;

static atomic_bool s_connected;
static atomic_uint s_connects;
static atomic_uint s_published;
static atomic_uint s_dropped;
static atomic_uint s_received;

static const char LAST_WILL[] = "{\"online\":false}";

static void publish_status(void)
{
    cJSON *status = s_cfg.build_status ? s_cfg.build_status() : cJSON_CreateObject();
    if (status == NULL) {
        return;
    }
    cJSON_AddBoolToObject(status, "online", true);
    char *json = cJSON_PrintUnformatted(status);
    cJSON_Delete(status);
    if (json) {
        // non-blocking, this also runs on the shared esp_timer task
        esp_mqtt_client_enqueue(s_client, s_topic_status, json, 0, QOS, 1, true);
        free(json);
    }
}

static void status_timer_cb(void *arg)
{
    if (atomic_load(&s_connected)) {
        publish_status();
    }
}

static void handle_send(const char *data, int len)
{
    char buf[16];
    if (len <= 0 || len >= (int)sizeof(buf)) {
        ESP_LOGW(TAG, "send: invalid payload length %d", len);
        return;
    }
    memcpy(buf, data, len);
    buf[len] = '\0';

    char *end;
    errno = 0;
    unsigned long long value = strtoull(buf, &end, 10);
    while (*end == ' ' || *end == '\n' || *end == '\r') {
        end++;
    }
    if (errno != 0 || end == buf || *end != '\0' || buf[0] == '-' || value == 0 || value > UINT32_MAX) {
        ESP_LOGW(TAG, "send: invalid command '%s'", buf);
        return;
    }

    atomic_fetch_add(&s_received, 1);
    if (s_cfg.on_send_request) {
        s_cfg.on_send_request((siedle_cmd_t)value);
    }
}

static bool topic_is(const esp_mqtt_event_t *event, const char *topic)
{
    return event->topic_len == (int)strlen(topic) && strncmp(event->topic, topic, event->topic_len) == 0;
}

static void mqtt_event_handler(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    esp_mqtt_event_handle_t event = data;

    switch ((esp_mqtt_event_id_t)id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "connected");
        atomic_store(&s_connected, true);
        atomic_fetch_add(&s_connects, 1);
        esp_mqtt_client_subscribe(s_client, TOPIC_SEND, QOS);
        publish_status();
        break;

    case MQTT_EVENT_DISCONNECTED:
        if (atomic_exchange(&s_connected, false)) {
            ESP_LOGW(TAG, "disconnected");
        }
        break;

    case MQTT_EVENT_PUBLISHED:
        atomic_fetch_add(&s_published, 1);
        break;

    case MQTT_EVENT_DATA:
        if (event->current_data_offset != 0 || event->data_len != event->total_data_len) {
            ESP_LOGW(TAG, "ignoring fragmented message");
        } else if (topic_is(event, TOPIC_SEND)) {
            handle_send(event->data, event->data_len);
        }
        break;

    case MQTT_EVENT_DELETED:
        atomic_fetch_add(&s_dropped, 1);
        break;

    case MQTT_EVENT_ERROR:
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
            ESP_LOGW(TAG, "transport error: %s (tls 0x%x)", esp_err_to_name(event->error_handle->esp_tls_last_esp_err),
                     event->error_handle->esp_tls_stack_err);
        } else if (event->error_handle->error_type == MQTT_ERROR_TYPE_CONNECTION_REFUSED) {
            ESP_LOGW(TAG, "connection refused (%d), check the AWS IoT policy and certificate",
                     event->error_handle->connect_return_code);
        }
        break;

    default:
        break;
    }
}

esp_err_t cloud_init(const cloud_config_t *cfg)
{
    ESP_RETURN_ON_FALSE(cfg->uri && cfg->client_id && cfg->client_cert && cfg->client_key, ESP_ERR_INVALID_ARG,
                        TAG, "incomplete configuration");
    s_cfg = *cfg;

    if (asprintf(&s_topic_status, PREFIX "/%s/status", cfg->client_id) < 0) {
        return ESP_ERR_NO_MEM;
    }

    const esp_mqtt_client_config_t mqtt_cfg = {
        .broker = {
            .address.uri = cfg->uri,
            .verification.crt_bundle_attach = esp_crt_bundle_attach,
        },
        .credentials = {
            .client_id = cfg->client_id,
            .authentication = {
                .certificate = cfg->client_cert,
                .key = cfg->client_key,
            },
        },
        .session = {
            .keepalive = 60,
            .last_will = {
                .topic = s_topic_status,
                .msg = LAST_WILL,
                .msg_len = sizeof(LAST_WILL) - 1,
                .qos = QOS,
                .retain = 1,
            },
        },
        .network = {
            .reconnect_timeout_ms = 5000,
            .timeout_ms = 10000,
        },
        .outbox.limit = 8 * 1024, // bounded memory use while offline
    };

    s_client = esp_mqtt_client_init(&mqtt_cfg);
    ESP_RETURN_ON_FALSE(s_client, ESP_FAIL, TAG, "client init");
    ESP_RETURN_ON_ERROR(esp_mqtt_client_register_event(s_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL), TAG,
                        "register event");

    const esp_timer_create_args_t timer_args = { .callback = status_timer_cb, .name = "cloud_status" };
    return esp_timer_create(&timer_args, &s_status_timer);
}

esp_err_t cloud_start(void)
{
    ESP_RETURN_ON_FALSE(s_client, ESP_ERR_INVALID_STATE, TAG, "not initialized");
    ESP_LOGI(TAG, "connecting to %s as %s", s_cfg.uri, s_cfg.client_id);
    ESP_RETURN_ON_ERROR(esp_mqtt_client_start(s_client), TAG, "start");
    return esp_timer_start_periodic(s_status_timer, (uint64_t)CONFIG_CLOUD_STATUS_INTERVAL_SEC * 1000000);
}

void cloud_publish_bus_event(cloud_bus_event_t event, siedle_cmd_t cmd, time_t timestamp)
{
    if (s_client == NULL) {
        return;
    }
    char payload[48];
    int len = snprintf(payload, sizeof(payload), "{\"ts\":%lld,\"cmd\":%" PRIu32 "}", (long long)timestamp, cmd);
    const char *topic = event == CLOUD_BUS_RECEIVED ? TOPIC_RECEIVED : TOPIC_SENT;

    // queued in the outbox while offline, dropped after CONFIG_MQTT_OUTBOX_EXPIRED_TIMEOUT_MS
    if (esp_mqtt_client_enqueue(s_client, topic, payload, len, QOS, 0, true) < 0) {
        atomic_fetch_add(&s_dropped, 1);
    }
}

void cloud_get_status(cloud_status_t *out)
{
    *out = (cloud_status_t){
        .configured = s_client != NULL,
        .connected = atomic_load(&s_connected),
        .connects = atomic_load(&s_connects),
        .published = atomic_load(&s_published),
        .dropped = atomic_load(&s_dropped),
        .received = atomic_load(&s_received),
    };
}
