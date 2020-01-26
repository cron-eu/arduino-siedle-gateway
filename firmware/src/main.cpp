#include <Arduino.h>

#include <Scheduler.h>
#include <WiFiNINA.h>
#include <wifi_client_secrets.h>

#include <WebServer.h>
#include <SiedleClient.h>
#include <CircularBuffer.h>
#include <RTCZero.h>
#include <time.h>

#define SIEDLE_A_IN A0
#define SIEDLE_TX_PIN 0

#define LOG_SIZE 100

typedef struct {
    unsigned long timestamp;
    siedle_cmd_t cmd;
} SiedleLogEntry;

int status = WL_IDLE_STATUS;
WebServer webServer(80);
SiedleClient siedleClient(SIEDLE_A_IN, SIEDLE_TX_PIN);
CircularBuffer<SiedleLogEntry, LOG_SIZE> siedleRxLog;
RTCZero rtc;

void statusLEDLoop() {
    if (status == WL_CONNECTED) {
        digitalWrite(LED_BUILTIN, HIGH);
        delay(150);
        digitalWrite(LED_BUILTIN, LOW);
        delay(3000);
        return;
    }

    if (status == WL_NO_MODULE) {
        digitalWrite(LED_BUILTIN, HIGH);
        delay(100);
        digitalWrite(LED_BUILTIN, LOW);
        delay(100);
        return;
    }

    digitalWrite(LED_BUILTIN, HIGH);
    delay(250);
    digitalWrite(LED_BUILTIN, LOW);
    delay(250);
}

// This loop synchronized the internal RTC with the time got from an NTP server, using the WiFi Library
void ntpLoop() {
    auto epoch = WiFi.getTime();
    if (epoch == 0) {
        delay(3000); // retry in 3 seconds
        return;
    }

    rtc.setEpoch(epoch);

    // next sync in 5 minutes
    delay(5 * 60 * 1000);
}

void siedleClientLoop() {
    if (siedleClient.receiveLoop()) {
        siedleRxLog.push({ rtc.getEpoch(), siedleClient.cmd });
    }
    yield();
}

void printDebug(Print *handler) {

    time_t time;

    handler->print("<h3>Device Status</h3>");

    handler->print("<dl><dt>Date/Time (UTC)</dt><dd>");
    time = rtc.getEpoch();
    handler->println(ctime(&time));
    handler->print("</dd></dl>");

    handler->print("<dl><dt>Bus Voltage</dt><dd>");
    handler->println(siedleClient.getBusvoltage());
    handler->print(" V</dd></dl>");

    auto size = min(siedleRxLog.capacity, siedleClient.rxCount);
    handler->print("<h3>Received Data</h3><table><tr><th>Timestamp</th><th>Command</th></tr>");
    for (unsigned int i = 0; i < size; i++) {
        SiedleLogEntry entry = siedleRxLog[i];
        time = entry.timestamp;
        char line[100];

        sprintf(line, "<tr><td>%s</th><td><pre>%08lx</pre></td></tr>",
                ctime(&time),
                entry.cmd
        );

        handler->print(line);
    }

    handler->print("</table>");
}

void webServerLoop() {
    webServer.loop();
    yield();
}

void printWifiStatus() {
    // print the SSID of the network you're attached to:
    Serial.print("SSID: ");
    Serial.println(WiFi.SSID());

    // print your board's IP address:
    IPAddress ip = WiFi.localIP();

    // print the received signal strength:
    long rssi = WiFi.RSSI();
    Serial.print("signal strength (RSSI):");
    Serial.print(rssi);
    Serial.println(" dBm");
    Serial.print("My IP address: ");
    Serial.println(ip);
}

void __unused setup() {
    webServer.printDebug = printDebug;
    Scheduler.startLoop(statusLEDLoop);
    rtc.begin();
    Serial.begin(115200);
    pinMode(LED_BUILTIN, OUTPUT);

    Serial.println("Booting..");

    char ssid[] = SECRET_SSID;    // network SSID (name)
    char pass[] = SECRET_PASS;    // network password (use for WPA, or use as key for WEP)

    // check for the WiFi module:
    if (WiFi.status() == WL_NO_MODULE) {
        Serial.println("Communication with WiFi module failed!");
        return;
    }

    String fv = WiFi.firmwareVersion();
    if (fv < WIFI_FIRMWARE_LATEST_VERSION) {
        Serial.println("Please upgrade the firmware");
    }

    // attempt to connect to Wifi network:
    while (status != WL_CONNECTED) {
        Serial.print("Attempting to connect to Network named: ");
        Serial.println(ssid);                   // print the network name (SSID);

        // Connect to WPA/WPA2 network. Change this line if using open or WEP network:
        status = WiFi.begin(ssid, pass);
        if (status == WL_CONNECTED) break;
        // wait 10 seconds for connection:
        delay(10000);
    }

    printWifiStatus();
    webServer.begin();
    WiFi.lowPowerMode();

    Scheduler.startLoop(webServerLoop);
    Scheduler.startLoop(siedleClientLoop);
    Scheduler.startLoop(ntpLoop);
}

void __unused loop() {
    delay(1000);
}
