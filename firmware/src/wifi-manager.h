//
// Created by Remus Lazar on 04.02.20.
//

#ifndef FIRMWARE_WIFI_MANAGER_H
#define FIRMWARE_WIFI_MANAGER_H
#include <serial-debug.h>

class WiFiManagerClass {
public:
    void begin();
    void loop();
    unsigned int wifiReconnects;

private:
    unsigned long reconnectMillis;
    unsigned long connectionCheckMillis;
    static void printWifiStatus();
};

extern WiFiManagerClass WiFiManager;

#endif //FIRMWARE_WIFI_MANAGER_H
