//
// Created by Remus Lazar on 04.02.20.
//

#ifndef FIRMWARE_WEB_UI_H
#define FIRMWARE_WEB_UI_H

#include <Arduino.h>
#include <MemoryUtils.h>


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

    handler->print("<h3>Device Status</h3>");

    handler->print("<dl><dt>Uptime</dt><dd>");

    auto uptime = getUptime();
    if (uptime > 24 * 3600) {
        handler->print((float)uptime / (24 * 3600), 1);
        handler->print(" days");
    } else if (uptime > 3600) {
        handler->print((float)uptime / 3600, 1);
        handler->print(" hr");
    } else {
        handler->print((float)uptime / 60, 0);
        handler->print(" min");
    }
    handler->print("</dd></dl>");


    handler->print("<dl><dt>WiFi</dt><dd>");

    handler->print("SSID: ");
    handler->print(WiFi.SSID());
    // print the received signal strength:
    long rssi = WiFi.RSSI();
    handler->print(", RSSI:");
    handler->print(rssi);
    handler->println(" dBm");
    handler->print("(");
    handler->print(WiFiManager.wifiReconnects);
    handler->println(")");

    handler->print("</dd></dl>");

    handler->print("<dl><dt>Free Memory</dt><dd>");
    handler->println(freeMemory());
    handler->print("</dd></dl>");

    handler->print("<dl><dt>Date/Time (UTC)</dt><dd>");
    time = RTCSync.getEpoch();
    handler->println(ctime(&time));
    handler->print("</dd></dl>");

//    handler->print("<dl><dt>Bus Voltage</dt><dd>");
//    handler->println(siedleClient.getBusvoltage());
//    handler->print(" V</dd></dl>");

#ifdef USE_MQTT
    handler->print("<dl><dt>AWS MQTT Link</dt><dd>");
    handler->println(MQTTService.isConnected() ? "OK" : "Not Connected");
    handler->print("(");
    handler->print(MQTTService.mqttReconnects);
    handler->println(")");
    handler->print("</dd></dl>");
#endif

    handler->print("<dl><dt>Rx/Tx Count</dt><dd>");
    handler->print(SiedleService.siedleClient.rxCount);
    handler->print(" / ");
    handler->print(SiedleService.siedleClient.txCount);
    handler->print("</dd></dl>");

    handler->print("<h3>Data</h3><table><tr><th>Timestamp</th><th>Direction</th><th>Command</th></tr>");
    for (unsigned int i = 0; i < siedleRxTxLog.size(); i++) {
        auto entry = siedleRxTxLog[i];
        time = entry.log.timestamp;
        char line[100];

        sprintf(line, "<tr><td>%s</th><td>%s</td><td><pre>%08lx</pre></td></tr>",
                ctime(&time),
                entry.direction == rx ? "<" : ">",
                entry.log.cmd
        );

        handler->print(line);
    }

    handler->print("</table>");
}

#endif //FIRMWARE_WEB_UI_H
