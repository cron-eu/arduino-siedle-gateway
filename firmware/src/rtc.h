//
// Created by Remus Lazar on 04.02.20.
//

#ifndef FIRMWARE_RTC_H
#define FIRMWARE_RTC_H

#include <Arduino.h>

#ifdef ARDUINO_ARCH_SAMD
#include <WiFiNINA.h>
#include <RTCZero.h>
#elif defined(ESP8266)
#include <NTPClient.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#endif

class RTCSyncClass {
public:
    #ifdef ARDUINO_ARCH_SAMD
    RTCSyncClass() : rtc() { }
    #elif defined(ESP8266)
    RTCSyncClass() : ntp(ntpUDP) { }
    #endif
    void begin() {
        lastMillis = 0;
        initialized = false;
        bootEpoch = 0;
        #ifdef ARDUINO_ARCH_SAMD
        rtc.begin();
        #elif defined(ESP8266)
        ntp.begin();
        #endif
    }

    void loop() {
        // if we're in the initializing phase, re-try every 3 seconds. Else use a longer sync interval
        unsigned long interval = !initialized ? 3000 : 5 * 60 * 1000;

        // bail out if we do not have internet connectivity
        if (WiFi.status() != WL_CONNECTED) { return; }


        if (millis() - lastMillis >= interval) {
            #ifdef ARDUINO_ARCH_SAMD
            auto epoch = WiFi.getTime();
            #elif defined(ESP8266)
            auto epoch = ntp.getEpochTime();
            #endif
            if (epoch != 0) {
                initialized = true;
                #ifdef ARDUINO_ARCH_SAMD
                rtc.setEpoch(epoch);
                #endif
                if (bootEpoch == 0) {
                    bootEpoch = epoch;
                }
            }
            lastMillis = millis();
        }

    }
    unsigned long bootEpoch;
    unsigned long getEpoch() {
        #ifdef ARDUINO_ARCH_SAMD
        return rtc.getEpoch();
        #elif defined(ESP8266)
        return ntp.getEpochTime();
        #endif
    }

private:
    #ifdef ARDUINO_ARCH_SAMD
    RTCZero rtc;
    #elif defined(ESP8266)
    WiFiUDP ntpUDP;
    NTPClient ntp;
    #endif
    unsigned long lastMillis;
    bool initialized;

};

extern RTCSyncClass RTCSync;
#endif //FIRMWARE_RTC_H
