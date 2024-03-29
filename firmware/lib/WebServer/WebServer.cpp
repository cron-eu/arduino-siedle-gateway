#include "WebServer.h"

// close an open TCP connection after
#define WEB_SERVER_CONNECTION_TIMEOUT_MS 10000

// bail out after the client sends more than bytes for a single input line
#define WEB_SERVER_MAX_INPUT_LINE_BUF 2048

WebServer::WebServer(uint16_t port) : _server(port) {}

void WebServer::begin() {
    status = WebServerStatus::web_server_idle;
    _server.begin();
}

//float get_battery_voltage() {
//    auto sensorValue = analogRead(ADC_BATTERY);
//
//    // Convert the analog reading (which goes from 0 - 1023) to the battery voltage
//    // see https://content.arduino.cc/assets/MKRWiFi1010V2.0_sch.pdf
//    return (float) sensorValue * 3.3 / 1023.0 / (1200.0 / (1200.0 + 330.0));
//}

void WebServer::loop() {

    static int currentLineLength;
    static unsigned long connectMillis;

    switch (status) {
    case WebServerStatus::web_server_idle:

#ifdef ARDUINO_ARCH_SAMD
        client = _server.available();   // listen for incoming clients
#elif defined(ESP8266)
        client = _server.accept();   // listen for incoming clients
#endif

        if (client) {
            status = WebServerStatus::web_server_connected;
            connectMillis = millis();
            currentLineLength = 0;
        }
        break;

    case WebServerStatus::web_server_connected:

        if (millis() - connectMillis > WEB_SERVER_CONNECTION_TIMEOUT_MS) {
            status = WebServerStatus::web_server_close;
            break;
        }

        if (currentLineLength >= WEB_SERVER_MAX_INPUT_LINE_BUF) {
            status = WebServerStatus::web_server_close;
            break;
        }

        if (!client.connected()) {
            // bail out if the current connection has been closed by the client or due to network errors
            status = WebServerStatus::web_server_close;
            break;
        }

        if (client.available()) {             // if there's bytes to read from the client,
            char c = client.read();             // read a byte, then
            if (c == '\n') {                    // if the byte is a newline character

                // if the current line is blank, you got two newline characters in a row.
                // that's the end of the client HTTP request, so send a response:
                if (currentLineLength == 0) {
                    // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
                    // and a content-type so the client knows what's coming, then a blank line:
                    client.println("HTTP/1.1 200 OK");
                    client.println("Content-type:text/html");
                    client.println();

                    client.print("<h1>Arduino Doorbell</h1>");
//                        client.print("<h3>Battery Voltage</h3>");
//                        client.print("<dl>");
//                        client.print("<dt>ADC Output</dt>");
//                        client.print("<dd>");
//                        client.print(get_battery_voltage());
//                        client.print("</dd>");

                    #ifdef ARDUINO_ARCH_SAMD
                    client.print("<dl>");
                    client.print("<dt>Firmware Version</dt>");
                    client.print("<dd>");
                    client.print(AUTO_VERSION);
                    client.print("<dt>WiFiNINA Firmware Version</dt>");
                    client.print("<dd>");
                    client.print(WiFi.firmwareVersion());
                    client.print("</dd>");
                    #endif

                    if (rootPageHandler != NULL) {
                        rootPageHandler(&client);
                    }

                    // The HTTP response ends with another blank line:
                    client.println();
                    status = WebServerStatus::web_server_close;
                } else { // we got a newline
                    currentLineLength = 0;
                }
            } else if (c != '\r') {  // if you got anything else but a carriage return character,
                currentLineLength++;
            }
        }
        break;

    case WebServerStatus::web_server_close:
        if (client.connected()) {
            client.stop();
        }
        status = WebServerStatus::web_server_idle;
        break;

    } // switch
}
