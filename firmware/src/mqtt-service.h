//
// Created by Remus Lazar on 04.02.20.
//

#ifndef FIRMWARE_MQTT_SERVICE_H
#define FIRMWARE_MQTT_SERVICE_H

#include <CircularBuffer.h>
#include "siedle-structs.h"
#include "settings.h"

#include <WiFiNINA.h>
#include <ArduinoBearSSL.h>
#include <ArduinoECCX08.h>
#include <ArduinoMqttClient.h>

class MQTTServiceClass {
public:
    MQTTServiceClass() : mqttTxQueue(), wifiClient(), sslClient(wifiClient), mqttClient(sslClient) { }
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
    WiFiClient    wifiClient;            // Used for the TCP socket connection
    BearSSLClient sslClient; // Used for SSL/TLS connection, integrates with ECC508
    MqttClient    mqttClient;
};

extern MQTTServiceClass MQTTService;

#endif //FIRMWARE_MQTT_SERVICE_H
