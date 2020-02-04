//
// Created by Remus Lazar on 04.02.20.
//

#include "mqtt-service.h"

#include <Arduino.h>
#include <WiFiNINA.h>
#include <CircularBuffer.h>

#include <ArduinoBearSSL.h>
#include <ArduinoECCX08.h>

#include <ArduinoMqttClient.h>
#include "siedle-service.h"
#include "rtc.h"

#include <aws_iot_secrets.h>

// max MQTT send rate
#define MQTT_MAX_SEND_RATE_MS 600

WiFiClient    wifiClient;            // Used for the TCP socket connection
BearSSLClient sslClient(wifiClient); // Used for SSL/TLS connection, integrates with ECC508
MqttClient    mqttClient(sslClient);

const char* certificate  = SECRET_CERTIFICATE;
const char broker[]      = SECRET_BROKER;

unsigned long getTime() {
    // get the current time from our RTC module
    return RTCSync.getEpoch();
}

void onMessageReceived(int messageSize) {

    char payload[16];
    char *payload_p = payload;

    unsigned int rxlen = 0;
    while (mqttClient.available() && messageSize--) {
        char c = mqttClient.read();
        if (rxlen++ < (sizeof(payload)-1)) {
            *payload_p++ = c;
        }
    }
    *payload_p = 0; // terminate the string with the 0 byte
    uint32_t cmd = atol(payload);

    SiedleService.transmitAsync(cmd);
}

void MQTTServiceClass::begin() {
    mqttReconnects = 0;
    reconnectMillis = 0;
    lastTxMillis = 0;

    ECCX08.begin();

    // Set a callback to get the current time
    // used to validate the servers certificate
    ArduinoBearSSL.onGetTime(getTime);

    // Set the ECCX08 slot to use for the private key
    // and the accompanying public certificate for it
    sslClient.setEccSlot(0, certificate);

    // Optional, set the client id used for MQTT,
    // each device that is connected to the broker
    // must have a unique client id. The MQTTClient will generate
    // a client id for you based on the millis() value if not set
    //
    // mqttClient.setId("clientId");

    // Set the message callback, this function is
    // called when the MQTTClient receives a message
    mqttClient.onMessage(onMessageReceived);
}

void MQTTServiceClass::loop() {
    unsigned long elapsed = millis() - reconnectMillis;

    if (!mqttClient.connected()) {
        if (elapsed > 10000) {
            auto connected = mqttClient.connect(broker, 8883);
            mqttReconnects++;
            reconnectMillis = millis();
            if (connected) {
                // subscribe to a topic
                mqttClient.subscribe("siedle/send");
            } else {
                // early return, retry after reconnectMillis
                return;
            }
        }
    }

    // poll for new MQTT messages and send keep alives
    mqttClient.poll();
    // check if we have some messages to send
    if (mqttTxQueue.size() && millis() - lastTxMillis >= MQTT_MAX_SEND_RATE_MS) {
        // we want to limit the outgoing rate to avoid issues with the power management
        auto entry = mqttTxQueue.shift();
        char buf[32];
        sprintf(buf, "{\"ts\":%lu,\"cmd\":%lu}", entry.timestamp, entry.cmd);

        mqttClient.beginMessage("siedle/received");
        mqttClient.print(buf);
        mqttClient.endMessage();
        lastTxMillis = millis();
    }

}

bool MQTTServiceClass::isConnected() {
    return mqttClient.connected();
}
