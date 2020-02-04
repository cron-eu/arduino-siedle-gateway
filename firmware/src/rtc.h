//
// Created by Remus Lazar on 04.02.20.
//

#ifndef FIRMWARE_RTC_H
#define FIRMWARE_RTC_H

#include <Arduino.h>
#include <WiFiNINA.h>
#include <RTCZero.h>

class RTCSyncClass {
public:
    void begin() {
        lastMillis = 0;
        initialized = false;
        bootEpoch = 0;
    }

    void loop() {
        // if we're in the initializing phase, re-try every 3 seconds. Else use a longer sync interval
        unsigned long interval = !initialized ? 3000 : 5 * 60 * 1000;

        // bail out if we do not have internet connectivity
        if (WiFi.status() != WL_CONNECTED) { return; }


        if (millis() - lastMillis >= interval) {
            auto epoch = WiFi.getTime();
            if (epoch != 0) {
                initialized = true;
                rtc.setEpoch(epoch);
                if (bootEpoch == 0) {
                    bootEpoch = epoch;
                }
            }
            lastMillis = millis();
        }

    }
    unsigned long bootEpoch;
    unsigned long getEpoch() {
        return rtc.getEpoch();
    }

private:
    RTCZero rtc;
    unsigned long lastMillis;
    bool initialized;

};

extern RTCSyncClass RTCSync;
#endif //FIRMWARE_RTC_H
