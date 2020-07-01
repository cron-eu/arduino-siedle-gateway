//
// Created by Remus Lazar on 11.01.20.
//

#ifndef FIRMWARE_WEBSERVER_H
#define FIRMWARE_WEBSERVER_H

#include <Arduino.h>
#ifdef ARDUINO_ARCH_SAMD
#include <WiFiNINA.h>
#elif defined(ARDUINO_ARCH_ESP8266)
#include <ESP8266WiFi.h>
#elif defined(ARDUINO_ARCH_ESP32)
#include <WiFi.h>
#endif
class WebServer {
public:
    WebServer(uint16_t);

    void begin();
    void (*rootPageHandler)(Print *handler) = NULL;

    void loop();

private:
    WiFiServer _server;
};

#endif //FIRMWARE_WEBSERVER_H
