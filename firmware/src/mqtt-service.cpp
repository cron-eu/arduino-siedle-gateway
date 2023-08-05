//
// Created by Remus Lazar on 04.02.20.
//

#include "mqtt-service.h"
#include <Arduino.h>

#include "siedle-service.h"
#include "rtc.h"

#include <aws_iot_secrets.h>
#include <serial-debug.h>

#ifdef ESP8266
#include <LittleFS.h>
#endif

// max MQTT send rate
#define MQTT_MAX_SEND_RATE_MS 600

unsigned long getTime() {
    // get the current time from our RTC module
    return RTCSync.getEpoch();
}

#ifdef ARDUINO_ARCH_SAMD
const char broker[]      = SECRET_BROKER;
const char* certificate  = SECRET_CERTIFICATE;
#endif

void MQTTServiceClass::onMessageReceived(char* topic, byte* payload, unsigned int length) {
    char *cmdString = (char*)malloc(length + 1);
    memcpy(cmdString, payload, length);
    cmdString[length] = 0; // null termination
    uint32_t cmd = atol(cmdString);
    free (cmdString);
    SiedleService.transmitAsync(cmd);
    rxCount++;
}

void _onMessageReceivedWrapper(char* topic, byte* payload, unsigned int length) {
    MQTTService.onMessageReceived(topic, payload, length);
}

#ifdef ARDUINO_ARCH_SAMD
MQTTServiceClass::MQTTServiceClass() : mqttTxQueue(), wifiClient(), sslClient(wifiClient), mqttClient(sslClient) { }
#elif defined(ESP8266)
MQTTServiceClass::MQTTServiceClass() : mqttTxQueue(), sslClient(), mqttClient(SECRET_BROKER, 8883, sslClient) { }
#endif

void MQTTServiceClass::begin() {
    mqttReconnects = 0;
    reconnectAttemptMillis = 0;
    lastTxMillis = 0;
    state = mqtt_not_connected;

    #ifdef ARDUINO_ARCH_SAMD
    ECCX08.begin();

    // Set a callback to get the current time
    // used to validate the servers certificate
    ArduinoBearSSL.onGetTime(getTime);

    // Set the ECCX08 slot to use for the private key
    // and the accompanying public certificate for it
    sslClient.setEccSlot(0, certificate);
    mqttClient.setServer(broker, 8883);

    // bail out after 10 seconds to avoid issues with the hardware watchdog
    mqttClient.setSocketTimeout(MQTT_TIMEOUT_SEC);

    #elif defined(ESP8266)
    loadSSLConfiguration();
    #endif

    // Optional, set the client id used for MQTT,
    // each device that is connected to the broker
    // must have a unique client id. The MQTTClient will generate
    // a client id for you based on the millis() value if not set
    //
    // mqttClient.setId("clientId");
    mqttClient.setCallback(_onMessageReceivedWrapper);
}

#ifdef ESP8266
void MQTTServiceClass::loadSSLConfiguration() {
    LittleFS.begin();

    File cert_file = LittleFS.open(F(MQTT_CERT_FILE), "r");
    File private_key_file = LittleFS.open(F(MQTT_PRIV_FILE), "r");
    File ca_file = LittleFS.open(F(MQTT_CA_FILE), "r");

    if (!cert_file || !private_key_file || !ca_file) {
        Debug.println(F("ERROR: load MQTT credentials failed."));
        return;
    }

    BearSSL::X509List cert(ca_file);
    BearSSL::X509List client_crt(cert_file);
    BearSSL::PrivateKey key(private_key_file);

    sslClient.setBufferSizes(512, 512);

    sslClient.setTrustAnchors(&cert);
    sslClient.setClientRSACert(&client_crt, &key);

    LittleFS.end();
}
#endif

void MQTTServiceClass::loop() {
    unsigned long elapsed = millis() - reconnectAttemptMillis;

    // poll for new MQTT messages and send keep alives
    mqttClient.loop();

    if (elapsed > MQTT_RECONNECT_INTERVAL_MS && RTCSync.initialized) { // we need a valid time to establish a SSL connection
        reconnectAttemptMillis = millis();

        if (!mqttClient.connected()) {
            state = mqtt_not_connected;
            #ifdef ARDUINO_ARCH_SAMD
            auto connected = mqttClient.connect(MQTT_DEVICE_NAME);
            #elif defined(ESP8266)
            auto time = RTCSync.getEpoch();
            sslClient.setX509Time(time);
            auto name = String(F(MQTT_DEVICE_NAME));
            Debug.print(String(F("MQTT: connecting as ")) + name + F(" .. "));
            auto connected = mqttClient.connect(name.c_str());
            #endif
            mqttReconnects++;
            if (connected) {
                state = mqtt_connected;
                Debug.println("mqtt connected");
            } else {
                Debug.println("failed!");
                #ifdef ESP8266
                char buf[256];
                sslClient.getLastSSLError(buf, sizeof(buf));
                Debug.println(String(F("Last SSL error: ")) + buf);
                #endif
            }
        } else {
            // mqttClient.connected() == true
            if (state == mqtt_connected) {
                // subscribe to a topic
                auto success = mqttClient.subscribe("siedle/send");
                if (success) {
                    Debug.println("mqtt subscribed");
                    state = mqtt_connected_and_subscribed;
                }
            }
        }
    }

    // timestamp when the device state was last sent
    static unsigned long deviceStateLastMillis = 0;

    if (state != mqtt_not_connected) {

        bool sendDeviceStatusOverdue = millis() - deviceStateLastMillis > SEND_DEVICE_STATE_EVERY_SEC;

        // check if we have some messages to send
        if ((mqttTxQueue.size() || sendDeviceStatusOverdue) && millis() - lastTxMillis >= MQTT_MAX_SEND_RATE_MS) {
            // we want to limit the outgoing rate to avoid issues with the power management
            lastTxMillis = millis();

            if (mqttTxQueue.size()) {
                auto entry = mqttTxQueue.pop();
                char buf[32];
                sprintf(buf, "{\"ts\":%lu,\"cmd\":%lu}", entry.payload.timestamp, (unsigned long)entry.payload.cmd);

                #ifdef ARDUINO_ARCH_SAMD
                mqttClient.publish(entry.topic == received ? "siedle/received" : "siedle/sent", buf);
                txCount++;
                #elif defined(ESP8266)
                switch (entry.topic) {
                    case received:
                        mqttClient.publish(String(F("siedle/received")).c_str(), buf);
                        break;
                    case sent:
                        mqttClient.publish(String(F("siedle/sent")).c_str(), buf);
                        break;
                }
                #endif
            } else if (sendDeviceStatusOverdue) {
                deviceStateLastMillis = millis();

                auto payload = String("{\"fw\":\"") + AUTO_VERSION
                 + String("\", \"boot\":") + RTCSync.bootEpoch + String(",\"wifi\":\"")
                 + WiFi.SSID()
                 + String("\",\"name\":\"")
                 + MQTT_DEVICE_NAME
                 + String("\",\"ip\":\"")
                 + WiFi.localIP()
                 + String("\"}");

                 mqttClient.publish("siedle/status", payload.c_str(), true);

            }
        }
    }
}

bool MQTTServiceClass::isConnected() {
    return mqttClient.connected();
}

MQTTServiceClass MQTTService;
 