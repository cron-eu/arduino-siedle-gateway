//
// Created by Remus Lazar on 04.02.20.
//

#ifndef FIRMWARE_RTC_H
#define FIRMWARE_RTC_H

#include <Arduino.h>
#include <serial-debug.h>
#include <settings.h>

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
    RTCSyncClass() : ntp(ntpUDP, NTP_SERVER) { }
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
        #ifdef ESP8266
        // because the NTP library has some issues, we do use NTP only once to initialize the time
        // we need the time only to perform SSL requests, must not be really accurate.
        if (initialized) { return; }
        #endif

        // if we're in the initializing phase, re-try every 3 seconds. Else use a longer sync interval
        unsigned long interval = !initialized ? 8000 : ( RTC_SYNC_INTERVAL_SEC * 1000 );

        if (millis() - lastMillis >= interval) {

            // bail out if we do not have internet connectivity
            if (WiFi.status() != WL_CONNECTED) {
                lastMillis = millis();
                return;
            }

            #ifdef ARDUINO_ARCH_SAMD
            auto epoch = WiFi.getTime();
            #elif defined(ESP8266)
            if (!initialized) {
                DEBUG_PRINT(F("NTP force update triggered .. "));
                auto success = ntp.forceUpdate();
                DEBUG_PRINTLN(success ? F("ok") : F("failed"));
            } else {
                ntp.update();
            }
            auto epoch = ntp.getEpochTime();
            #endif
            if (epoch > 10000) {
                if (!initialized) {
                    DEBUG_PRINTLN(String(F("Got NTP time: ")) + epoch);
                }
                initialized = true;
                #ifdef ARDUINO_ARCH_SAMD
                rtc.setEpoch(epoch);
                #elif defined(ESP8266)
                ntp.end();
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
    bool initialized;

private:
    #ifdef ARDUINO_ARCH_SAMD
    RTCZero rtc;
    #elif defined(ESP8266)
    WiFiUDP ntpUDP;
    NTPClient ntp;
    #endif
    unsigned long lastMillis;

};

extern RTCSyncClass RTCSync;
#endif //FIRMWARE_RTC_H
