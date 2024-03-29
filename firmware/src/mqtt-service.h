//
// Created by Remus Lazar on 04.02.20.
//

#ifndef FIRMWARE_MQTT_SERVICE_H
#define FIRMWARE_MQTT_SERVICE_H

#include <CircularBuffer.h>
#include "siedle-structs.h"
#include "settings.h"

#ifdef ARDUINO_ARCH_SAMD
#include <WiFiNINA.h>
#include <ArduinoBearSSL.h>
#include <ArduinoECCX08.h>
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#endif

#include <MQTTClient.h>

enum MQTTTopic {
    received = 0,
    sent = 1
};

enum MQTTState {
    mqtt_not_connected,
    mqtt_connected,
    mqtt_connected_and_subscribed,
};

typedef struct {
    SiedleLogEntry payload;
    MQTTTopic topic;
} MQTTSendItem;

class MQTTServiceClass {
public:
    MQTTServiceClass();
    void begin();
    void loop();
    unsigned int mqttReconnects;
    unsigned int rxCount = 0;
    unsigned int txCount = 0;
    unsigned int overruns = 0;

    /**
     * Queue a Siedle Log Entry for sending
     *
     * This method will return instantly and save the data in an internal
     * circular buffer. The sending process is async.
     * @param data
     */
    void sendAsync(SiedleLogEntry data, MQTTTopic topic) {
        if (mqttTxQueue.isFull()) {
            overruns++;
        } else {
            mqttTxQueue.unshift({data, topic});
        }
    }
    bool isConnected();
    MQTTState state;
    void onMessageReceived(String &topic, String &payload);

private:
    unsigned long reconnectAttemptMillis;
    unsigned long lastTxMillis;
    CircularBuffer<MQTTSendItem, MQTT_TX_QUEUE_LEN> mqttTxQueue;

    #ifdef ARDUINO_ARCH_SAMD
    WiFiClient    wifiClient;            // Used for the TCP socket connection
    BearSSLClient sslClient; // Used for SSL/TLS connection, integrates with ECC508
    #elif defined(ESP8266)
    WiFiClientSecure sslClient;
    void loadSSLConfiguration();
    #endif
    MQTTClient  mqttClient;
};

extern MQTTServiceClass MQTTService;

#endif //FIRMWARE_MQTT_SERVICE_H
