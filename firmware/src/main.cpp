#include <Arduino.h>

#include <Scheduler.h>
#include <WiFiNINA.h>

#define USE_MQTT

#ifdef USE_MQTT
#include <ArduinoBearSSL.h>
#include <ArduinoECCX08.h>
#include <ArduinoMqttClient.h>
#include <aws_iot_secrets.h>

#endif

#include <wifi_client_secrets.h>

#include <WebServer.h>
#include <SiedleClient.h>
#include <CircularBuffer.h>
#include <RTCZero.h>
#include <time.h>

#define SIEDLE_A_IN A1
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

#ifdef USE_MQTT
WiFiClient    wifiClient;            // Used for the TCP socket connection
BearSSLClient sslClient(wifiClient); // Used for SSL/TLS connection, integrates with ECC508
MqttClient    mqttClient(sslClient);

const char* certificate  = SECRET_CERTIFICATE;
const char broker[]      = SECRET_BROKER;
#endif

const char ssid[] = SECRET_SSID;    // network SSID (name)
const char pass[] = SECRET_PASS;    // network password (use for WPA, or use as key for WEP)

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
    if (siedleClient.available()) {
        siedleRxLog.push({ rtc.getEpoch(), siedleClient.read() });
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

//    handler->print("<dl><dt>Bus Voltage</dt><dd>");
//    handler->println(siedleClient.getBusvoltage());
//    handler->print(" V</dd></dl>");

#ifdef USE_MQTT
    handler->print("<dl><dt>AWS MQTT Link</dt><dd>");
    handler->println(mqttClient.connected() ? "OK" : "Not Connected");
    handler->print("</dd></dl>");
#endif

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

#ifdef USE_MQTT
void connectMQTT() {
    Serial.print("Attempting to MQTT broker: ");
    Serial.print(broker);
    Serial.println(" ");

    while (!mqttClient.connect(broker, 8883)) {
        // failed, retry
        Serial.print(".");
        delay(5000);
    }
    Serial.println();

    Serial.println("You're connected to the MQTT broker");
    Serial.println();

    // subscribe to a topic
    mqttClient.subscribe("arduinodoorbell/incoming");
}

unsigned long getTime() {
    // get the current time from the WiFi module
    return WiFi.getTime();
}

void onMessageReceived(int messageSize) {
    // we received a message, print out the topic and contents
    Serial.print("Received a message with topic '");
    Serial.print(mqttClient.messageTopic());
    Serial.print("', length ");
    Serial.print(messageSize);
    Serial.println(" bytes:");

    // use the Stream interface to print the contents
    while (mqttClient.available()) {
        Serial.print((char)mqttClient.read());
    }
    Serial.println();

    Serial.println();
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

    if (!mqttClient.connected()) {
        // MQTT client is disconnected, connect
        connectMQTT();
    }
}
#endif

void __unused setup() {
    webServer.printDebug = printDebug;
    Scheduler.startLoop(statusLEDLoop);
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

    Scheduler.startLoop(webServerLoop);

#ifdef USE_MQTT
    setupMQTT();
#endif

    Scheduler.startLoop(siedleClientLoop);
    Scheduler.startLoop(ntpLoop);
}

int mqttSentCount = 0;

unsigned long reconnectMillis = 0;

void __unused loop() {

    if (!mqttClient.connected()) {
        auto elapsed = millis() - reconnectMillis;
        if (elapsed > 30000) {
            mqttClient.connect(broker, 8883);
            reconnectMillis = millis();
            return;
        }
    }

#ifdef USE_MQTT
    // poll for new MQTT messages and send keep alives
    mqttClient.poll();

    // check if we have some messages to send
    auto toSendCount = siedleClient.rxCount - mqttSentCount;

    yield();

    if (toSendCount > 0) {
        auto entry = siedleRxLog.last();
        char buf[32];
        sprintf(buf, "{\"ts\":%lu,\"cmd\":%lu}", entry.timestamp, entry.cmd);

        mqttClient.beginMessage("siedle/received");
        mqttClient.print(buf);
        mqttClient.endMessage();

        mqttSentCount++;
    }

    yield();
#else
    delay(1000);
#endif
}
