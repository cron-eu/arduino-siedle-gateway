//
// Created by Remus Lazar on 04.02.20.
//

#include "wifi-manager.h"

#include <Arduino.h>
#ifdef ARDUINO_ARCH_SAMD
#include <WiFiNINA.h>
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#ifdef MDNS_HOSTNAME
#include <ESP8266mDNS.h>
#endif
#endif

#include <wifi_client_secrets.h>

void WiFiManagerClass::loop() {
    if (millis() - connectionCheckMillis > 250) { // check connection status periodically
        connectionCheckMillis = millis();

        auto status = WiFi.status();
        if ( (status == WL_CONNECTED) != connected) {
            connected = (status == WL_CONNECTED);
            if (connected) {
                printWifiStatus();
                #ifdef MDNS_HOSTNAME
                Debug.println(String(F("mDNS service started for hostname: ")) + F(MDNS_HOSTNAME));
                MDNS.begin(F(MDNS_HOSTNAME));
                #endif
            }
        }

        if (!connected && millis() - reconnectMillis > 5000) { // reconnect every 5 seconds
            reconnectMillis = millis();
            connect();
        }
    }
    #ifdef MDNS_HOSTNAME
    MDNS.update();
    #endif
}

void WiFiManagerClass::begin() {
    reconnectMillis = 0;
    connectionCheckMillis = 0;
    wifiReconnects = 0;
#ifdef ARDUINO_ARCH_SAMD
    WiFi.lowPowerMode();
#endif
    connect();
}

void WiFiManagerClass::connect() {
    String ssid = F(SECRET_SSID);
    String pass = F(SECRET_PASS);
    Debug.print(String(F("Connecting to WiFi Network '")) + ssid + "' .. ");
    WiFi.begin(ssid, pass); // this will run async
}

void WiFiManagerClass::printWifiStatus() {
    // print the SSID of the network you're attached to:
    Debug.println(F("done!"));

    // print ip and rssi of the current link
    Debug.println(String(F("Local IP: ")) + WiFi.localIP().toString()
                + F(", signal strength (RSSI): ") + WiFi.RSSI() + F(" dBm."));
}

WiFiManagerClass WiFiManager;
