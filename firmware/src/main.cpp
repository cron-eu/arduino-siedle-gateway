#include <Arduino.h>

#include <Scheduler.h>
#include <WiFiNINA.h>
#include <wifi_client_secrets.h>

#include <WebServer.h>
#include <SiedleClient.h>

#define SIEDLE_A_IN A0

#define LOG_SIZE 100

struct SiedleLogEntry {
    unsigned long timestamp;
    siedle_cmd_t cmd;
};

int status = WL_IDLE_STATUS;
WebServer webServer(80);
SiedleClient siedleClient(SIEDLE_A_IN);
SiedleLogEntry siedleLog[LOG_SIZE];
unsigned int siedleLogIndex = 0;

void saveSiedleLog(siedle_cmd_t cmd) {
    SiedleLogEntry entry = { millis(), cmd };

    if (siedleLogIndex < sizeof(siedleLog)) {
        siedleLog[siedleLogIndex++] = entry;
    } // else: buffer full
}

void statusLEDLoop() {
    if (status == WL_CONNECTED) {
        digitalWrite(LED_BUILTIN, LOW);
        delay(5000);
        return;
    }
    digitalWrite(LED_BUILTIN, HIGH);
    delay(250);
    digitalWrite(LED_BUILTIN, LOW);
    delay(250);
}

void siedleClientLoop() {
    siedleClient.loop();

    if (siedleClient.available()) {
        auto cmd = siedleClient.getCmd();
        saveSiedleLog(cmd);
    }
}

void printDebug(Print *handler) {
    for (int i = 0; i < siedleLogIndex; i++) {
        auto entry = siedleLog[i];
        handler->print(entry.timestamp);
        handler->print(": ");
        handler->println(entry.cmd, HEX);
    }
    handler->print("Rx Count: ");
    handler->println(siedleClient.getRxCount());

    handler->print("Bus Voltage: ");
    handler->println(siedleClient.getBusvoltage());

    handler->print("Siedle Client State: ");
    handler->println(siedleClient.getState());

    siedleLogIndex = 0;
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

    // print the received signal strength:
    long rssi = WiFi.RSSI();
    Serial.print("signal strength (RSSI):");
    Serial.print(rssi);
    Serial.println(" dBm");
    Serial.print("My IP address: ");
    Serial.println(ip);
}

#pragma clang diagnostic push
#pragma ide diagnostic ignored "OCUnusedGlobalDeclarationInspection"

void setup() {
    webServer.printDebug = printDebug;
    Scheduler.startLoop(statusLEDLoop);
    Serial.begin(115200);
    pinMode(LED_BUILTIN, OUTPUT);

    Serial.println("Booting..");

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
}

#pragma clang diagnostic pop

#pragma clang diagnostic push
#pragma ide diagnostic ignored "OCUnusedGlobalDeclarationInspection"

void loop() {
    delay(10000);
}

#pragma clang diagnostic pop
