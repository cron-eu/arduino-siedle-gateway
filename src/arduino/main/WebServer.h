#ifndef web_server_h
#define web_server_h

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

#endif
