#include <Arduino.h>
#include <WiFiNINA.h>

#include <hardware.h>
#include <settings.h>

#ifdef USE_MQTT
#include <ArduinoBearSSL.h>
#include <ArduinoECCX08.h>
#include <ArduinoMqttClient.h>
#include <aws_iot_secrets.h>
// max MQTT send rate
#define MQTT_MAX_SEND_RATE_MS 600
#endif

#include <wifi_client_secrets.h>
#include <WebServer.h>
#include <SiedleClient.h>
#include <CircularBuffer.h>
#include <RTCZero.h>
#include <time.h>
#include <MemoryUtils.h>

// max siedle bus send rate
#define BUS_MAX_SEND_RATE_MS 800
#define SIEDLE_TX_QUEUE_LEN 16

typedef struct {
    unsigned long timestamp;
    siedle_cmd_t cmd;
} SiedleLogEntry;

int status = WL_IDLE_STATUS;
WebServer webServer(80);
SiedleClient siedleClient(SIEDLE_A_IN, SIEDLE_TX_PIN);
CircularBuffer<SiedleLogEntry, LOG_SIZE> siedleRxLog;
CircularBuffer<siedle_cmd_t, SIEDLE_TX_QUEUE_LEN> siedleTxQueue;

RTCZero rtc;

#ifdef USE_MQTT
WiFiClient    wifiClient;            // Used for the TCP socket connection
BearSSLClient sslClient(wifiClient); // Used for SSL/TLS connection, integrates with ECC508
MqttClient    mqttClient(sslClient);

const char* certificate  = SECRET_CERTIFICATE;
const char broker[]      = SECRET_BROKER;
#endif

const char ssid[] = SECRET_SSID;    // network SSID (name)
const char pass[] = SECRET_PASS;    // network password (use for WPA, or use as key for WEP)

unsigned long bootEpoch = 0;

/**
 * Determine the boot time of the system
 *
 * @return number of seconds since the system has booted up
 */
unsigned long getUptime() {
    if (bootEpoch > 0) { return rtc.getEpoch() - bootEpoch; }
    // else, NTP not initialized
    return millis() / 1000;
}

inline void statusLEDLoop() {
    static unsigned long lastMillis = 0;
    static int led = LOW;
    auto elapsed = millis() - lastMillis;
    unsigned long threshold;

    switch (status) {
        case WL_CONNECTED:
            threshold = led == LOW ? 3000 : 150;
            break;
        case WL_NO_MODULE:
            threshold = 100;
            break;
        default:
            threshold = 250;
    }

    if (elapsed >= threshold) {
        led = !led;
        digitalWrite(LED_BUILTIN, led);
        lastMillis = millis();
    }
}

// This loop synchronized the internal RTC with the time got from an NTP server, using the WiFi Library
inline void ntpLoop() {
    static unsigned long lastMillis = 0;
    static bool initialized = false;

    // if we're in the initializing phase, re-try every 3 seconds. Else use a longer sync interval
    unsigned long interval = !initialized ? 3000 : 5 * 60 * 1000;

    if (millis() - lastMillis >= interval) {
        auto epoch = WiFi.getTime();
        if (epoch != 0) {
            initialized = true;
            rtc.setEpoch(epoch);
            if (bootEpoch == 0) {
                bootEpoch = epoch;
            }
        }
        lastMillis = millis();
    }
}

inline void siedleClientLoop() {
    static unsigned long lastTxMillis = 0;

    if (siedleClient.available()) {
        siedleRxLog.push({ rtc.getEpoch(), siedleClient.read() });
    }

    // introduce some padding between the send requests
    if (!siedleTxQueue.isEmpty()) {
        auto now = millis();
        if (now - lastTxMillis > BUS_MAX_SEND_RATE_MS) {
            if (siedleClient.state == idle) {
                auto cmd = siedleTxQueue.shift();
                siedleClient.sendCmd(cmd);
            }
            lastTxMillis = now;
        }
    }
}

void printDebug(Print *handler) {

    time_t time;

    handler->print("<h3>Device Status</h3>");

    handler->print("<dl><dt>Uptime</dt><dd>");

    auto uptime = getUptime();
    if (uptime > 24 * 3600) {
        handler->print((float)uptime / (24 * 3600), 1);
        handler->print(" days");
    } else if (uptime > 3600) {
        handler->print((float)uptime / 3600, 1);
        handler->print(" hr");
    } else {
        handler->print((float)uptime / 60, 0);
        handler->print(" min");
    }

    handler->print("</dd></dl>");

    handler->print("<dl><dt>Free Memory</dt><dd>");
    handler->println(freeMemory());
    handler->print("</dd></dl>");

    handler->print("<dl><dt>Date/Time (UTC)</dt><dd>");
    time = rtc.getEpoch();
    handler->println(ctime(&time));
    handler->print("</dd></dl>");

//    handler->print("<dl><dt>Bus Voltage</dt><dd>");
//    handler->println(siedleClient.getBusvoltage());
//    handler->print(" V</dd></dl>");

#ifdef USE_MQTT
    handler->print("<dl><dt>AWS MQTT Link</dt><dd>");
    handler->println(mqttClient.connected() ? "OK" : "Not Connected");
    handler->print("</dd></dl>");
#endif

    handler->print("<dl><dt>Rx/Tx Count</dt><dd>");
    handler->print(siedleClient.rxCount);
    handler->print(" / ");
    handler->print(siedleClient.txCount);
    handler->print("</dd></dl>");

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

#ifdef USE_MQTT
unsigned long getTime() {
    // get the current time from the WiFi module
    return WiFi.getTime();
}

void onMessageReceived(int messageSize) {

    char payload[16];
    char *payload_p = payload;

    int rxlen = 0;
    while (mqttClient.available()) {
        char c = mqttClient.read();
        if (rxlen++ < (sizeof(payload)-1)) {
            *payload_p++ = c;
        }
    }
    *payload_p = 0; // terminate the string with the 0 byte
    uint32_t cmd = atol(payload);
    siedleTxQueue.push(cmd);
}

inline void setupMQTT() {
    ECCX08.begin();

    // Set a callback to get the current time
    // used to validate the servers certificate
    ArduinoBearSSL.onGetTime(getTime);

    // Set the ECCX08 slot to use for the private key
    // and the accompanying public certificate for it
    sslClient.setEccSlot(0, certificate);

    // Optional, set the client id used for MQTT,
    // each device that is connected to the broker
    // must have a unique client id. The MQTTClient will generate
    // a client id for you based on the millis() value if not set
    //
    // mqttClient.setId("clientId");

    // Set the message callback, this function is
    // called when the MQTTClient receives a message
    mqttClient.onMessage(onMessageReceived);

}
#endif

void __unused setup() {
    webServer.printDebug = printDebug;
    rtc.begin();
    Serial.begin(115200);
    pinMode(LED_BUILTIN, OUTPUT);

    Serial.println("Booting..");

    siedleClient.begin();

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

#ifdef USE_MQTT
    setupMQTT();
#endif
}

#ifdef USE_MQTT
void inline mqttLoop() {
    static unsigned long reconnectMillis = 0;
    static int mqttSentCount = 0;

    unsigned long elapsed = millis() - reconnectMillis;
    static unsigned long lastTxMillis = 0;

    if (!mqttClient.connected()) {
        if (elapsed > 10000) {
            auto connected = mqttClient.connect(broker, 8883);
            reconnectMillis = millis();
            if (connected) {
                // subscribe to a topic
                mqttClient.subscribe("siedle/send");
            }
        }
    } else {
        // poll for new MQTT messages and send keep alives
        mqttClient.poll();
        // check if we have some messages to send
        auto toSendCount = siedleClient.rxCount - mqttSentCount;

        if (toSendCount > 0) {
            // we want to limit the outgoing rate to avoid issues with the power management
            auto now = millis();
            if (now - lastTxMillis >= MQTT_MAX_SEND_RATE_MS) {
                lastTxMillis = now;
                auto entry = siedleRxLog.last();
                char buf[32];
                sprintf(buf, "{\"ts\":%lu,\"cmd\":%lu}", entry.timestamp, entry.cmd);

                mqttClient.beginMessage("siedle/received");
                mqttClient.print(buf);
                mqttClient.endMessage();

                mqttSentCount++;
            }
        }

        // reset the reconnect timer
        reconnectMillis = millis();
    }

}
#endif

void __unused loop() {
    statusLEDLoop();
    ntpLoop();
    webServer.loop();
    siedleClientLoop();
#ifdef USE_MQTT
    mqttLoop();
#endif
}
