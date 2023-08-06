//
// Created by Remus Lazar on 04.02.20.
//

#ifndef FIRMWARE_WEB_UI_H
#define FIRMWARE_WEB_UI_H

#include <Arduino.h>
#ifdef ARDUINO_ARCH_SAMD
#include <MemoryUtils.h>
#include <RTCZero.h>
#endif

#include "main_profiling.h"

/**
 * Determine the boot time of the system
 *
 * @return number of seconds since the system has booted up
 */
unsigned long getUptime() {
    if (RTCSync.bootEpoch > 0) { return RTCSync.getEpoch() - RTCSync.bootEpoch; }
    // else, NTP not initialized
    return millis() / 1000;
}

void webUIHTMLHandler(Print *handler) {

    time_t time;

    handler->print(F("<h3>Device Status</h3>"));

    handler->print(F("<dl><dt>Uptime</dt><dd>"));

    auto uptime = getUptime();
    if (uptime > 24 * 3600) {
        handler->print((float)uptime / (24 * 3600), 1);
        handler->print(F(" days"));
    } else if (uptime > 3600) {
        handler->print((float)uptime / 3600, 1);
        handler->print(F(" hr"));
    } else {
        handler->print((float)uptime / 60, 0);
        handler->print(F(" min"));
    }
    handler->print(F("</dd></dl>"));


    handler->print(F("<dl><dt>WiFi</dt><dd>"));

    handler->print(F("SSID: "));
    handler->print(WiFi.SSID());
    // print the received signal strength:
    long rssi = WiFi.RSSI();
    handler->print(F(", RSSI:"));
    handler->print(rssi);
    handler->println(F(" dBm"));
    handler->print(F("("));
    handler->print(WiFiManager.wifiReconnects);
    handler->println(F(")"));

    handler->print(F("</dd></dl>"));

    handler->print(F("<dl><dt>Free Memory</dt><dd>"));
    #ifdef ARDUINO_ARCH_SAMD
    handler->println(freeMemory());
    #elif defined(ESP8266)
    handler->println(ESP.getFreeHeap());
    #endif
    handler->print(F("</dd></dl>"));

    handler->print(F("<dl><dt>Date/Time (UTC)</dt><dd>"));
    time = RTCSync.getEpoch();
    handler->println(ctime(&time));
    handler->print(F("</dd></dl>"));

//    handler->print(F("<dl><dt>Siedle Bus IRQ Count</dt><dd>"));
//    handler->println(SiedleService.siedleClient.irq_count);
//    handler->print(F("</dd></dl>"));

#ifdef USE_MQTT
    handler->print(F("<dl><dt>MQTT Status (Reconnects)</dt><dd>"));
    switch (MQTTService.state) {
        case mqtt_not_connected:
            handler->println(F("Not Connected"));
            break;
        case mqtt_connected:
            handler->println(F("Connecting"));
            break;
        case mqtt_connected_and_subscribed:
            handler->println(F("Ready"));
            break;
    }
    handler->print(F("("));
    handler->print(MQTTService.mqttReconnects);
    handler->println(F(")"));
    handler->println(F("</dd>"));

    handler->println(F("<dt>MQTT Rx/Tx/Overruns Count</dt>"));
    handler->print(F("<dd>"));
    handler->print(MQTTService.rxCount);
    handler->print(F(" / "));
    handler->print(MQTTService.txCount);
    handler->print(F(" / "));
    handler->print(MQTTService.overruns);
    handler->println(F("</dd>"));

#endif

    handler->print(F("<dl><dt>Siedle Bus Rx/Tx/Overruns Count</dt><dd>"));
    handler->print(SiedleService.siedleClient.rxCount);
    handler->print(F(" / "));
    handler->print(SiedleService.siedleClient.txCount);
    handler->print(F(" / "));
    handler->print(SiedleService.overruns);
    handler->print(F("</dd></dl>"));

    handler->println(
        String(F("<dl><dt>Mainloop max ms</dt><dd>")) + max_main_loop_duration_ms + String(F("</dd></dl>"))
    );

    handler->print(F("<h3>Data</h3><table><tr><th>Timestamp</th><th>Direction</th><th>Command</th></tr>"));
    for (unsigned int i = 0; i < siedleRxTxLog.size(); i++) {
        auto entry = siedleRxTxLog[i];
        time = entry.log.timestamp;
        handler->print(F(""));

        auto html = String(F("<tr><td>")) + ctime(&time)
            + String(F("</td><td>")) + (entry.direction == rx ? F("&larr;") : F("&rarr;"))
            + String(F("</td><td><pre>"))
            + String(entry.log.cmd, 16) + F("</pre></td></tr>");

        handler->print(html);
    }

    handler->print(F("</table>"));
}

#endif //FIRMWARE_WEB_UI_H
