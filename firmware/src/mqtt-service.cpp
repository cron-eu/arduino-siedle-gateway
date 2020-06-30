//
// Created by Remus Lazar on 04.02.20.
//

#include "mqtt-service.h"
#include <Arduino.h>

#include "siedle-service.h"
#include "rtc.h"

#include <aws_iot_secrets.h>
#include <serial-debug.h>

// max MQTT send rate
#define MQTT_MAX_SEND_RATE_MS 600

const char* certificate  = SECRET_CERTIFICATE;
const char broker[]      = SECRET_BROKER;

unsigned long getTime() {
    // get the current time from our RTC module
    return RTCSync.getEpoch();
}

#ifdef ARDUINO_ARCH_SAMD
void MQTTServiceClass::onMessageReceived(int messageSize) {

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
#endif

void _onMessageReceivedWrapper(int count) {
    MQTTService.onMessageReceived(count);
}

void MQTTServiceClass::begin() {
    mqttReconnects = 0;
    reconnectMillis = 0;
    lastTxMillis = 0;

    #ifdef ARDUINO_ARCH_SAMD
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

    #elif defined(ESP8266)
    SPIFFS.begin();

    File cert = SPIFFS.open(MQTT_CERT_FILE, "r");
    File private_key = SPIFFS.open(MQTT_PRIV_FILE, "r");
    File ca = SPIFFS.open(MQTT_CA_FILE, "r");

    if (!cert || !private_key || !ca) {
        Debug.println("ERROR: load MQTT credentials failed.");
        return;
    }

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
    }

    sslClient.setBufferSizes(512, 512);
    sslClient.loadCertificate(cert);
    sslClient.loadPrivateKey(private_key);
    sslClient.loadCACert(ca);
    #endif

    // Set the message callback, this function is
    // called when the MQTTClient receives a message
    #ifdef ARDUINO_ARCH_SAMD
    mqttClient.onMessage(_onMessageReceivedWrapper);
    #elif defined(ESP8266)
    mqttClient.setCallback([this](char *topic, uint8_t *payload, unsigned int length) {
        char *cmdString = static_cast<char *>(malloc(length + 1));
        cmdString[length] = 0; // null termination
        uint32_t cmd = atol(cmdString);
        free (cmdString);
        SiedleService.transmitAsync(cmd);
    });
    #endif
}

void MQTTServiceClass::loop() {
    unsigned long elapsed = millis() - reconnectMillis;

    if (!mqttClient.connected()) {
        if (elapsed > 10000) {
            #ifdef ARDUINO_ARCH_SAMD
            auto connected = mqttClient.connect(broker, 8883);
            #elif defined(ESP8266)
            auto connected = mqttClient.connect(broker);
            #endif
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
    #ifdef ARDUINO_ARCH_SAMD
    mqttClient.poll();
    #elif defined(ESP8266)
    mqttClient.loop();
    #endif
    // check if we have some messages to send
    if (mqttTxQueue.size() && millis() - lastTxMillis >= MQTT_MAX_SEND_RATE_MS) {
        // we want to limit the outgoing rate to avoid issues with the power management
        auto entry = mqttTxQueue.shift();
        char buf[32];
        sprintf(buf, "{\"ts\":%lu,\"cmd\":%lu}", entry.timestamp, (unsigned long)entry.cmd);

        #ifdef ARDUINO_ARCH_SAMD
        mqttClient.beginMessage("siedle/received");
        mqttClient.print(buf);
        mqttClient.endMessage();
        lastTxMillis = millis();
        #elif defined(ESP8266)
        mqttClient.publish(String(F("siedle/received")).c_str(), buf);
        #endif
    }

}

bool MQTTServiceClass::isConnected() {
    return mqttClient.connected();
}

MQTTServiceClass MQTTService;
