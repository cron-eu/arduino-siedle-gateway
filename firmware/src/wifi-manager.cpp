//
// Created by Remus Lazar on 04.02.20.
//

#include "wifi-manager.h"

#include <Arduino.h>
#ifdef ARDUINO_ARCH_SAMD
#include <WiFiNINA.h>
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#endif

#include <wifi_client_secrets.h>

const char ssid[] = SECRET_SSID;    // network SSID (name)
const char pass[] = SECRET_PASS;    // network password (use for WPA, or use as key for WEP)

void WiFiManagerClass::loop() {
    if (millis() - connectionCheckMillis > 1000) { // check connection status every second
        connectionCheckMillis = millis();
        auto status = WiFi.status();
        if (status != WL_CONNECTED && millis() - reconnectMillis > 5000) { // reconnect every 5 seconds
            reconnectMillis = millis();
            status = WiFi.begin(ssid, pass);
            if (status == WL_CONNECTED) {
                wifiReconnects++;
                printWifiStatus();
            }
        }
    }
}

void WiFiManagerClass::begin() {
    reconnectMillis = 0;
    connectionCheckMillis = 0;
    wifiReconnects = 0;
#ifdef ARDUINO_ARCH_SAMD
    WiFi.lowPowerMode();
#endif
}

void WiFiManagerClass::printWifiStatus() {
    // print the SSID of the network you're attached to:
    Debug.print("SSID: ");
    Debug.println(WiFi.SSID());

    // print your board's IP address:
    IPAddress ip = WiFi.localIP();

    // print the received signal strength:
    long rssi = WiFi.RSSI();
    Debug.print("signal strength (RSSI):");
    Debug.print(rssi);
    Debug.println(" dBm");
    Debug.print("My IP address: ");
    Debug.println(ip);
}

WiFiManagerClass WiFiManager;
