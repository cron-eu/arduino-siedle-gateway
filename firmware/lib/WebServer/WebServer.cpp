#include "WebServer.h"

WebServer::WebServer(uint16_t port) : _server(port) {}

void WebServer::begin() {
    _server.begin();
}

float get_battery_voltage() {
    auto sensorValue = analogRead(ADC_BATTERY);

    // Convert the analog reading (which goes from 0 - 1023) to the battery voltage
    // see https://content.arduino.cc/assets/MKRWiFi1010V2.0_sch.pdf
    return (float) sensorValue * 3.3 / 1023.0 / (1200.0 / (1200.0 + 330.0));
}

void WebServer::loop() {
    WiFiClient client = _server.available();   // listen for incoming clients

    if (client) {                             // if you get a client,
        String currentLine = "";                // make a String to hold incoming data from the client
        while (client.connected()) {            // loop while the client's connected
            if (client.available()) {             // if there's bytes to read from the client,
                char c = client.read();             // read a byte, then
                if (c == '\n') {                    // if the byte is a newline character

                    // if the current line is blank, you got two newline characters in a row.
                    // that's the end of the client HTTP request, so send a response:
                    if (currentLine.length() == 0) {
                        // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
                        // and a content-type so the client knows what's coming, then a blank line:
                        client.println("HTTP/1.1 200 OK");
                        client.println("Content-type:text/html");
                        client.println();

                        client.print("<h1>Hello from Arduino MKR 1010 WiFi :-)</h1>");
                        client.print("<h3>Battery Voltage</h3>");
                        client.print("<dl>");
                        client.print("<dt>ADC Output</dt>");
                        client.print("<dd>");
                        client.print(get_battery_voltage());
                        client.print("</dd>");

                        client.print("<h3>Siedle Bus Debug</h3>");
                        client.print("<dl>");
                        client.print("<dt>Current Bus Voltage / V</dt>");
                        client.print("<dd>");
                        client.print(busVoltage);
                        client.print("</dd>");

                        client.print("<dt>Current ADC Value</dt>");
                        client.print("<dd>");
                        client.print(analogRead(A0));
                        client.print("</dd>");


                        client.print("<h4>Received:</h4>");

                        client.print("<pre>");
                        while (buffer.available()) {
                            client.print(buffer.read_char(), HEX);
                            client.println();
                        }
                        client.print("</pre>");

                        // The HTTP response ends with another blank line:
                        client.println();
                        // break out of the while loop:
                        break;
                    } else {    // if you got a newline, then clear currentLine:
                        currentLine = "";
                    }
                } else if (c != '\r') {  // if you got anything else but a carriage return character,
                    currentLine += c;      // add it to the end of the currentLine
                }
            }
        }
        // close the connection:
        client.stop();
    }
}
