//
// Created by Remus Lazar on 11.01.20.
//

#ifndef FIRMWARE_WEBSERVER_H
#define FIRMWARE_WEBSERVER_H

#include <Arduino.h>
#ifdef ARDUINO_ARCH_SAMD
#include <WiFiNINA.h>
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#endif

enum WebServerStatus {
    // idle, waiting for requests
    web_server_idle,

    // currently connected to a client, reading from client and sending response
    web_server_connected,

    // the server has finished the current response, closing connection
    web_server_close,
};

class WebServer {
public:
    WebServer(uint16_t);

    void begin();
    void (*rootPageHandler)(Print *handler) = NULL;

    void loop();

private:
    WiFiServer _server;
    // active client
    WiFiClient client;
    WebServerStatus status;
};

#endif //FIRMWARE_WEBSERVER_H
