//
// Created by Remus Lazar on 04.02.20.
//

#include <Arduino.h>

#ifdef ARDUINO_ARCH_SAMD
#include <WiFiNINA.h>
#include <MqttClient.h>
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#endif


#ifndef FIRMWARE_LED_H
#define FIRMWARE_LED_H

#include "mqtt-service.h"

#ifdef ARDUINO_ARCH_SAMD
#define SIEDLE_LED_ON HIGH
#define SIEDLE_LED_OFF LOW

#elif defined(ESP8266)
#define SIEDLE_LED_ON LOW
#define SIEDLE_LED_OFF HIGH
#endif

class LEDClass {
public:
    void begin() {
        rateMillis = 0;
        lastMillis = 0;
        led = SIEDLE_LED_OFF;
        pinMode(LED_BUILTIN, OUTPUT);
    }

    void loop() {
        // early return to avoid doing to leave the main loop responsive
        if (millis() - rateMillis < 50) {
            return;
        } else {
            rateMillis = millis();
        }

        auto status = WiFi.status();
        unsigned long threshold;

        switch (status) {
            case WL_CONNECTED:
                threshold = led == SIEDLE_LED_OFF ? 3000 : 150;
                break;
            default:
                threshold = 500;
        }

        if (!MQTTService.isConnected()) {
            threshold = 250;
        }

        if (millis() - lastMillis >= threshold) {
            led = !led;
            digitalWrite(LED_BUILTIN, led);
            lastMillis = millis();
        }
    }

private:
    unsigned long rateMillis, lastMillis;
    int led;
};

extern LEDClass LED;

#endif //FIRMWARE_LED_H
