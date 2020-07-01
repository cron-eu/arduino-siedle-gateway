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

unsigned long getTime() {
    // get the current time from our RTC module
    return RTCSync.getEpoch();
}

#ifdef ARDUINO_ARCH_SAMD
const char broker[]      = SECRET_BROKER;
const char* certificate  = SECRET_CERTIFICATE;
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

#ifdef ARDUINO_ARCH_SAMD
MQTTServiceClass::MQTTServiceClass() : mqttTxQueue(), wifiClient(), sslClient(wifiClient), mqttClient(sslClient) { }
#elif defined(ARDUINO_ARCH_ESP8266)
MQTTServiceClass::MQTTServiceClass() : mqttTxQueue(), sslClient(), mqttClient(SECRET_BROKER, 8883, sslClient) { }
#endif

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

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
    }
    #elif defined(ARDUINO_ARCH_ESP8266)
    loadSSLConfiguration();
    #endif

    // Set the message callback, this function is
    // called when the MQTTClient receives a message
    #ifdef ARDUINO_ARCH_SAMD
    mqttClient.onMessage(_onMessageReceivedWrapper);
    #elif defined(ARDUINO_ARCH_ESP8266)
    mqttClient.setCallback([this](char *topic, uint8_t *payload, unsigned int length) {
        char *cmdString = (char*)malloc(length + 1);
        memcpy(cmdString, payload, length);
        cmdString[length] = 0; // null termination
        uint32_t cmd = atol(cmdString);
        free (cmdString);
        SiedleService.transmitAsync(cmd);
    });
    #endif
}

#ifdef ARDUINO_ARCH_ESP8266
void MQTTServiceClass::loadSSLConfiguration() {
    SPIFFS.begin();

    File cert = SPIFFS.open(F(MQTT_CERT_FILE), "r");
    File private_key = SPIFFS.open(F(MQTT_PRIV_FILE), "r");
    File ca = SPIFFS.open(F(MQTT_CA_FILE), "r");

    if (!cert || !private_key || !ca) {
        Debug.println(F("ERROR: load MQTT credentials failed."));
        return;
    }

    sslClient.setBufferSizes(512, 512);

    if (!sslClient.loadCertificate(cert)) {
        Debug.println(F("MQTT error: could not load the certificate"));
    }
    if (!sslClient.loadPrivateKey(private_key)) {
        Debug.println(F("MQTT error: could not load the private key"));

    }
    if (!sslClient.loadCACert(ca)) {
        Debug.println(F("MQTT error: could not load the CA cert"));
    }

    SPIFFS.end();
}
#endif

void MQTTServiceClass::loop() {
    unsigned long elapsed = millis() - reconnectMillis;

    if (!mqttClient.connected()) {
        if (elapsed > 10000 && RTCSync.initialized) { // we need a valid time to establish a SSL connection
            #ifdef ARDUINO_ARCH_SAMD
            auto connected = mqttClient.connect(broker, 8883);
            #elif defined(ARDUINO_ARCH_ESP8266)
            auto time = RTCSync.getEpoch();
            sslClient.setX509Time(time);
            auto name = String(F(MQTT_DEVICE_NAME));
            Debug.print(String(F("MQTT: connecting as ")) + name + F(" .. "));
            auto connected = mqttClient.connect(name.c_str());
            #endif
            mqttReconnects++;
            reconnectMillis = millis();
            if (connected) {
                Debug.println(String(F("ok!")));
                // subscribe to a topic
                mqttClient.subscribe(String(F("siedle/send")).c_str());
            } else {
                Debug.println(String(F("failed!")));
                #ifdef ARDUINO_ARCH_ESP8266
                char buf[256];
                sslClient.getLastSSLError(buf, sizeof(buf));
                Debug.println(String(F("Last SSL error: ")) + buf);
                #endif
                // early return, retry after reconnectMillis
                return;
            }
        }
    }

    // poll for new MQTT messages and send keep alives
    #ifdef ARDUINO_ARCH_SAMD
    mqttClient.poll();
    #elif defined(ARDUINO_ARCH_ESP8266)
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
        #elif defined(ARDUINO_ARCH_ESP8266)
        mqttClient.publish(String(F("siedle/received")).c_str(), buf);
        #endif
    }

}

bool MQTTServiceClass::isConnected() {
    return mqttClient.connected();
}

MQTTServiceClass MQTTService;
