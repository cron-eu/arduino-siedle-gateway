#include <Arduino.h>
#include "User_Setup.h"

#include <Scheduler.h>
#include <WiFiNINA.h>
#include "wifi_client_secrets.h"

#include "src/WebServer/WebServer.h"

int status = WL_IDLE_STATUS;
WebServer webServer(80);

void statusLEDLoop() {
    if (status == WL_CONNECTED) {
        digitalWrite(LED_BUILTIN, LOW);
        delay(5000);
        return;
    }

    digitalWrite(LED_BUILTIN, HIGH);
    delay(500);
    digitalWrite(LED_BUILTIN, LOW);
    delay(500);
}

void webServerLoop() {
    webServer.loop();
}

void printWifiStatus() {
    // print the SSID of the network you're attached to:
    Serial.print("SSID: ");
    Serial.println(WiFi.SSID());

    // print your board's IP address:
    IPAddress ip = WiFi.localIP();
    Serial.print("IP Address: ");
    Serial.println(ip);

    // print the received signal strength:
    long rssi = WiFi.RSSI();
    Serial.print("signal strength (RSSI):");
    Serial.print(rssi);
    Serial.println(" dBm");
    Serial.print("My IP address: ");
    Serial.println(ip);
}

void setup() {
    Scheduler.start(statusLEDLoop);

    char ssid[] = SECRET_SSID;    // network SSID (name)
    char pass[] = SECRET_PASS;    // network password (use for WPA, or use as key for WEP)

    // check for the WiFi module:
    if (WiFi.status() == WL_NO_MODULE) {
        Serial.println("Communication with WiFi module failed!");
        // don't continue

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-noreturn"
        while (true);
#pragma clang diagnostic pop
    }

    String fv = WiFi.firmwareVersion();
    if (fv < WIFI_FIRMWARE_LATEST_VERSION) {
        Serial.println("Please upgrade the firmware");
    }

    // attempt to connect to Wifi network:
    for(;;) {
        Serial.print("Attempting to connect to Network named: ");
        Serial.println(ssid);                   // print the network name (SSID);

        // Connect to WPA/WPA2 network. Change this line if using open or WEP network:
        status = WiFi.begin(ssid, pass);

        if (status == WL_AP_CONNECTED) {
            break;
        } else {
            // retry after 10s
            delay(10000);
        }
    }

    printWifiStatus();
    webServer.begin();

    Scheduler.startLoop(webServerLoop);
}

void loop() {
    delay(10000);
}
