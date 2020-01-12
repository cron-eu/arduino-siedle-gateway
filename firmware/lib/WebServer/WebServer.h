//
// Created by Remus Lazar on 11.01.20.
//

#ifndef FIRMWARE_WEBSERVER_H
#define FIRMWARE_WEBSERVER_H

#include <Arduino.h>
#include <WiFiNINA.h>

class WebServer {
public:
    WebServer(uint16_t);

    void begin();

    void loop();

private:
    WiFiServer _server;
};

#endif //FIRMWARE_WEBSERVER_H
