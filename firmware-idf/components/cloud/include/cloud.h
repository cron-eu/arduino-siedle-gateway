/*
 * MQTT connection to AWS IoT Core (mutual TLS).
 *
 * Topics (prefix = CONFIG_CLOUD_TOPIC_PREFIX), the first three are unchanged from the Arduino firmware so the
 * Lambda functions keep working:
 *
 *   <prefix>/received               publish   {"ts":<unix time>,"cmd":<uint32>} for every frame received
 *   <prefix>/sent                   publish   same format, for every frame sent and acknowledged
 *   <prefix>/send                   subscribe decimal uint32 command to put on the bus
 *   <prefix>/<client_id>/status     publish   retained device status JSON, {"online":false} as last will
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#include "cJSON.h"
#include "esp_err.h"
#include "siedle_proto.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    CLOUD_BUS_RECEIVED,
    CLOUD_BUS_SENT,
} cloud_bus_event_t;

typedef struct {
    const char *uri;          /**< mqtts://<endpoint>:8883 */
    const char *client_id;
    const char *client_cert;  /**< PEM */
    const char *client_key;   /**< PEM */
    /** Called from the MQTT task for every command received on <prefix>/send */
    void (*on_send_request)(siedle_cmd_t cmd);
    /** Returns a new JSON object describing the device, published as status (ownership is transferred) */
    cJSON *(*build_status)(void);
} cloud_config_t;

typedef struct {
    bool configured;
    bool connected;
    uint32_t connects;
    uint32_t published;
    uint32_t dropped;
    uint32_t received;
} cloud_status_t;

/** Create the client. Messages published before cloud_start() are queued. */
esp_err_t cloud_init(const cloud_config_t *cfg);

/** Connect, call once the system time is valid (needed to check certificate validity). */
esp_err_t cloud_start(void);

void cloud_publish_bus_event(cloud_bus_event_t event, siedle_cmd_t cmd, time_t timestamp);

void cloud_get_status(cloud_status_t *out);

#ifdef __cplusplus
}
#endif
