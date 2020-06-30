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
#include <ArduinoMqttClient.h>
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <PubSubClient.h>
#endif


class MQTTServiceClass {
public:
#ifdef ARDUINO_ARCH_SAMD
    MQTTServiceClass() : mqttTxQueue(), wifiClient(), sslClient(wifiClient), mqttClient(sslClient) { }
#elif defined(ESP8266)
    MQTTServiceClass() : mqttTxQueue(), mqttClient(sslClient) { }
#endif
    void begin();
    void loop();
    unsigned int mqttReconnects;

    /**
     * Queue a Siedle Log Entry for sending
     *
     * This method will return instantly and save the data in an internal
     * circular buffer. The sending process is async.
     * @param data
     */
    void sendAsync(SiedleLogEntry data) {
        mqttTxQueue.push(data);
    }
    bool isConnected();
    void onMessageReceived(int messageSize);

private:
    unsigned long reconnectMillis;
    unsigned long lastTxMillis;
    CircularBuffer<SiedleLogEntry, MQTT_TX_QUEUE_LEN> mqttTxQueue;

    #ifdef ARDUINO_ARCH_SAMD
    WiFiClient    wifiClient;            // Used for the TCP socket connection
    BearSSLClient sslClient; // Used for SSL/TLS connection, integrates with ECC508
    MqttClient    mqttClient;
    #elif defined(ESP8266)
    WiFiClientSecure sslClient;
    PubSubClient mqttClient;
    #endif
};

extern MQTTServiceClass MQTTService;

#endif //FIRMWARE_MQTT_SERVICE_H
