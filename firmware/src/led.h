//
// Created by Remus Lazar on 04.02.20.
//

#include <Arduino.h>
#include <WiFiNINA.h>
#include <MqttClient.h>

#ifndef FIRMWARE_LED_H
#define FIRMWARE_LED_H

#include "mqtt-service.h"

class LEDClass {
public:
    void begin() {
        rateMillis = 0;
        lastMillis = 0;
        led = LOW;
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
                threshold = led == LOW ? 3000 : 150;
                break;
            case WL_NO_MODULE:
                threshold = 100;
                break;
            default:
                threshold = 500;
        }

        if (MQTTService.isConnected()) {
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
