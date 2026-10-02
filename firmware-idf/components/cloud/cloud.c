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
#include "esp_tls_errors.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "mbedtls/platform_util.h"
#include "mqtt_client.h"
#include "ota.h"

#define PREFIX CONFIG_CLOUD_TOPIC_PREFIX
#define TOPIC_RECEIVED PREFIX "/received"
#define TOPIC_SENT     PREFIX "/sent"
#define TOPIC_SEND     PREFIX "/send"
#define QOS            1

static const char *TAG = "cloud";

// One identity and its MQTT client, replaced as a whole by cloud_configure()
typedef struct {
    esp_mqtt_client_handle_t client;
    char *uri;
    char *client_id;
    char *cert;
    char *key;
    char *topic_status;
    char *topic_ota;
    bool running;
    bool connecting; // a connection attempt is under way (MQTT task only)
    bool failed;     // and an error was reported for it (MQTT task only)
} connection_t;

static cloud_callbacks_t s_callbacks;
static SemaphoreHandle_t s_configure_lock; // serializes cloud_configure()
static SemaphoreHandle_t s_lock;           // guards s_conn and s_started, never held while waiting for the MQTT task
static connection_t *s_conn;
static bool s_started;
static esp_timer_handle_t s_status_timer;

static portMUX_TYPE s_info_mux = portMUX_INITIALIZER_UNLOCKED; // guards s_client_id and s_error
static char s_client_id[sizeof(((cloud_status_t *)0)->client_id)];
static char s_error[sizeof(((cloud_status_t *)0)->error)];

static atomic_bool s_connected;
static atomic_uint s_connects;
static atomic_uint s_published;
static atomic_uint s_dropped;
static atomic_uint s_received;

static const char LAST_WILL[] = "{\"online\":false}";

#define LOCK()   xSemaphoreTake(s_lock, portMAX_DELAY)
#define UNLOCK() xSemaphoreGive(s_lock)

static void set_error(const char *error)
{
    taskENTER_CRITICAL(&s_info_mux);
    strlcpy(s_error, error, sizeof(s_error));
    taskEXIT_CRITICAL(&s_info_mux);
}

static void publish_status(connection_t *conn)
{
    cJSON *status = s_callbacks.build_status ? s_callbacks.build_status() : cJSON_CreateObject();
    if (status == NULL) {
        return;
    }
    cJSON_AddBoolToObject(status, "online", true);
    char *json = cJSON_PrintUnformatted(status);
    cJSON_Delete(status);
    if (json) {
        // non-blocking, this also runs on the shared esp_timer task
        esp_mqtt_client_enqueue(conn->client, conn->topic_status, json, 0, QOS, 1, true);
        free(json);
    }
}

static void status_timer_cb(void *arg)
{
    LOCK();
    if (s_conn && atomic_load(&s_connected)) {
        publish_status(s_conn);
    }
    UNLOCK();
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
    if (s_callbacks.on_send_request) {
        s_callbacks.on_send_request((siedle_cmd_t)value);
    }
}

static void handle_ota(const char *data, int len)
{
    cJSON *msg = cJSON_ParseWithLength(data, len);
    const cJSON *url = cJSON_GetObjectItemCaseSensitive(msg, "url");
    if (!cJSON_IsString(url)) {
        ESP_LOGW(TAG, "ota: expected {\"url\":\"https://...\"}");
    } else {
        esp_err_t err = ota_start(url->valuestring);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "ota: %s", esp_err_to_name(err));
        }
    }
    cJSON_Delete(msg);
}

static bool topic_is(const esp_mqtt_event_t *event, const char *topic)
{
    return event->topic_len == (int)strlen(topic) && strncmp(event->topic, topic, event->topic_len) == 0;
}

// Why a connection attempt failed, for the setup page
static const char *error_text(const esp_mqtt_error_codes_t *error, bool connecting)
{
    if (error->error_type == MQTT_ERROR_TYPE_CONNECTION_REFUSED) {
        return "Connection refused by AWS IoT";
    }
    switch (error->esp_tls_last_esp_err) {
    case ESP_ERR_ESP_TLS_CANNOT_RESOLVE_HOSTNAME:
        return "Endpoint not found";
    case ESP_ERR_ESP_TLS_FAILED_CONNECT_TO_HOST:
    case ESP_ERR_ESP_TLS_CONNECTION_TIMEOUT:
        return "Endpoint not reachable";
    case ESP_ERR_MBEDTLS_SSL_HANDSHAKE_FAILED:
        // verify flags are about the server certificate, without them the server refused ours
        return error->esp_tls_cert_verify_flags ? "Endpoint not trusted, use the -ats endpoint"
                                                : "AWS IoT rejected the certificate, is it active?";
    case ESP_ERR_ESP_TLS_TCP_CLOSED_FIN:
    case ESP_ERR_MBEDTLS_SSL_READ_FAILED:
        // AWS IoT closes the connection instead of refusing it if the policy doesn't allow connecting
        return connecting ? "AWS IoT closed the connection, check the policy" : "Connection lost";
    default:
        return connecting ? "Connection failed" : "Connection lost";
    }
}

static void mqtt_event_handler(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    connection_t *conn = arg;
    esp_mqtt_event_handle_t event = data;

    switch ((esp_mqtt_event_id_t)id) {
    case MQTT_EVENT_BEFORE_CONNECT:
        conn->connecting = true;
        conn->failed = false;
        break;

    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "connected");
        conn->connecting = false;
        set_error("");
        atomic_store(&s_connected, true);
        atomic_fetch_add(&s_connects, 1);
        esp_mqtt_client_subscribe(conn->client, TOPIC_SEND, QOS);
        esp_mqtt_client_subscribe(conn->client, conn->topic_ota, QOS);
        publish_status(conn);
        // reaching the cloud proves a freshly updated image works
        ota_mark_valid();
        break;

    case MQTT_EVENT_DISCONNECTED:
        if (atomic_exchange(&s_connected, false)) {
            ESP_LOGW(TAG, "disconnected");
            set_error("Connection lost");
        } else if (conn->connecting && !conn->failed) {
            set_error("AWS IoT closed the connection, check the policy"); // no answer to CONNECT
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
        } else if (topic_is(event, conn->topic_ota)) {
            handle_ota(event->data, event->data_len);
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
        } else {
            break;
        }
        conn->failed = true;
        set_error(error_text(event->error_handle, conn->connecting));
        break;

    default:
        break;
    }
}

/* ---- connection -------------------------------------------------------------------------------------------- */

static void connection_destroy(connection_t *conn)
{
    if (conn->client) {
        if (atomic_load(&s_connected)) {
            // a clean disconnect doesn't trigger the last will, the status would keep saying online
            esp_mqtt_client_publish(conn->client, conn->topic_status, LAST_WILL, sizeof(LAST_WILL) - 1, QOS, 1);
        }
        esp_mqtt_client_destroy(conn->client); // stops the MQTT task, it may have to finish a connection attempt
    }
    if (conn->key) {
        mbedtls_platform_zeroize(conn->key, strlen(conn->key));
    }
    free(conn->uri);
    free(conn->client_id);
    free(conn->cert);
    free(conn->key);
    free(conn->topic_status);
    free(conn->topic_ota);
    free(conn);
}

static char *make_topic(const char *client_id, const char *name)
{
    char *topic;
    return asprintf(&topic, PREFIX "/%s/%s", client_id, name) < 0 ? NULL : topic;
}

static connection_t *connection_create(const cloud_config_t *cfg)
{
    connection_t *conn = calloc(1, sizeof(*conn));
    if (conn == NULL) {
        return NULL;
    }
    conn->uri = strdup(cfg->uri);
    conn->client_id = strdup(cfg->client_id);
    conn->cert = strdup(cfg->client_cert); // the client keeps pointers to certificate and key
    conn->key = strdup(cfg->client_key);
    conn->topic_status = make_topic(cfg->client_id, "status");
    conn->topic_ota = make_topic(cfg->client_id, "ota");
    if (!conn->uri || !conn->client_id || !conn->cert || !conn->key || !conn->topic_status || !conn->topic_ota) {
        connection_destroy(conn);
        return NULL;
    }

    const esp_mqtt_client_config_t mqtt_cfg = {
        .broker = {
            .address.uri = conn->uri,
            .verification.crt_bundle_attach = esp_crt_bundle_attach,
        },
        .credentials = {
            .client_id = conn->client_id,
            .authentication = {
                .certificate = conn->cert,
                .key = conn->key,
            },
        },
        .session = {
            .keepalive = 60,
            .last_will = {
                .topic = conn->topic_status,
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

    conn->client = esp_mqtt_client_init(&mqtt_cfg);
    if (conn->client == NULL
        || esp_mqtt_client_register_event(conn->client, ESP_EVENT_ANY_ID, mqtt_event_handler, conn) != ESP_OK) {
        connection_destroy(conn);
        return NULL;
    }
    return conn;
}

// (lock held)
static esp_err_t connection_start(connection_t *conn)
{
    if (conn->running) {
        return ESP_OK;
    }
    ESP_LOGI(TAG, "connecting to %s as %s", conn->uri, conn->client_id);
    ESP_RETURN_ON_ERROR(esp_mqtt_client_start(conn->client), TAG, "start");
    conn->running = true;
    return ESP_OK;
}

/* ---- public API -------------------------------------------------------------------------------------------- */

esp_err_t cloud_init(const cloud_callbacks_t *callbacks)
{
    s_callbacks = *callbacks;
    s_lock = xSemaphoreCreateMutex();
    s_configure_lock = xSemaphoreCreateMutex();
    ESP_RETURN_ON_FALSE(s_lock && s_configure_lock, ESP_ERR_NO_MEM, TAG, "mutex");

    const esp_timer_create_args_t timer_args = { .callback = status_timer_cb, .name = "cloud_status" };
    ESP_RETURN_ON_ERROR(esp_timer_create(&timer_args, &s_status_timer), TAG, "timer");
    return esp_timer_start_periodic(s_status_timer, (uint64_t)CONFIG_CLOUD_STATUS_INTERVAL_SEC * 1000000);
}

esp_err_t cloud_configure(const cloud_config_t *cfg)
{
    ESP_RETURN_ON_FALSE(cfg == NULL || (cfg->uri && cfg->client_id && cfg->client_cert && cfg->client_key),
                        ESP_ERR_INVALID_ARG, TAG, "incomplete configuration");
    xSemaphoreTake(s_configure_lock, portMAX_DELAY);

    // the old connection is closed before the new one starts, they may share the client id
    LOCK();
    connection_t *old = s_conn;
    s_conn = NULL;
    UNLOCK();
    if (old) {
        ESP_LOGI(TAG, "disconnecting %s", old->client_id);
        connection_destroy(old);
        atomic_store(&s_connected, false);
    }

    connection_t *conn = cfg ? connection_create(cfg) : NULL;
    esp_err_t err = cfg && conn == NULL ? ESP_ERR_NO_MEM : ESP_OK;

    taskENTER_CRITICAL(&s_info_mux);
    strlcpy(s_client_id, conn ? conn->client_id : "", sizeof(s_client_id));
    s_error[0] = '\0';
    taskEXIT_CRITICAL(&s_info_mux);

    if (conn) {
        LOCK();
        s_conn = conn;
        if (s_started) {
            err = connection_start(conn);
        }
        UNLOCK();
    }
    xSemaphoreGive(s_configure_lock);
    return err;
}

esp_err_t cloud_start(void)
{
    LOCK();
    s_started = true;
    esp_err_t err = s_conn ? connection_start(s_conn) : ESP_OK;
    UNLOCK();
    return err;
}

void cloud_publish_bus_event(cloud_bus_event_t event, siedle_cmd_t cmd, time_t timestamp)
{
    char payload[48];
    int len = snprintf(payload, sizeof(payload), "{\"ts\":%lld,\"cmd\":%" PRIu32 "}", (long long)timestamp, cmd);
    const char *topic = event == CLOUD_BUS_RECEIVED ? TOPIC_RECEIVED : TOPIC_SENT;

    LOCK();
    // queued in the outbox while offline, dropped after CONFIG_MQTT_OUTBOX_EXPIRED_TIMEOUT_MS
    if (s_conn && esp_mqtt_client_enqueue(s_conn->client, topic, payload, len, QOS, 0, true) < 0) {
        atomic_fetch_add(&s_dropped, 1);
    }
    UNLOCK();
}

void cloud_get_status(cloud_status_t *out)
{
    memset(out, 0, sizeof(*out));
    taskENTER_CRITICAL(&s_info_mux);
    strlcpy(out->client_id, s_client_id, sizeof(out->client_id));
    strlcpy(out->error, s_error, sizeof(out->error));
    taskEXIT_CRITICAL(&s_info_mux);

    out->configured = out->client_id[0] != '\0';
    out->connected = atomic_load(&s_connected);
    out->connects = atomic_load(&s_connects);
    out->published = atomic_load(&s_published);
    out->dropped = atomic_load(&s_dropped);
    out->received = atomic_load(&s_received);
}
