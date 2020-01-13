#include <ESP8266HTTPUpdateServer.h>
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>
#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <PubSubClient.h>
#include <WiFiManager.h>
#include <ArduinoJson.h>
#include <FS.h>

#define LOAD_PIN 5
#define WRITE_PIN 4
#define CARRIER_PIN 12
#define BIT_PIN 13
#define BIT_DURATION 1980

char mqtt_server[40] = "192.168.0.62";
char mqtt_port[6] = "8080";

bool shouldSaveConfig = false; //flag for saving data

WiFiClient espClient;
PubSubClient mqtt(espClient);
ESP8266WebServer server;
ESP8266HTTPUpdateServer httpUpdater;

void setup() {
  pinMode(LOAD_PIN, OUTPUT); //200 ohm
  pinMode(WRITE_PIN, OUTPUT); //10 ohm
  pinMode(CARRIER_PIN, INPUT); //carrier
  pinMode(BIT_PIN, INPUT); //bits

  digitalWrite(LOAD_PIN, LOW);
  digitalWrite(WRITE_PIN, LOW);

  Serial.begin(9600);

  if (SPIFFS.begin()) {
    Serial.println("mounted file system");
    if (SPIFFS.exists("/config.json")) {
      //file exists, reading and loading
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) {
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject(buf.get());
        json.printTo(Serial);
        if (json.success()) {
          strcpy(mqtt_server, json["mqtt_server"]);
        } else {
          Serial.println("failed to load json config");
        }
      }
    }
  }

  WiFiManagerParameter custom_mqtt_server("server", "mqtt server", mqtt_server, 40);
  WiFiManager wifiManager;
  wifiManager.setSaveConfigCallback(saveConfigCallback);

  wifiManager.addParameter(&custom_mqtt_server);
  wifiManager.setDebugOutput(true);
  wifiManager.setConfigPortalTimeout(180);

  if (!wifiManager.autoConnect("Siedle Gateway")) {
    Serial.println("failed to connect and hit timeout");
    delay(3000);
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(5000);
  }

  //read updated parameters
  strcpy(mqtt_server, custom_mqtt_server.getValue());

  if (shouldSaveConfig) {
    Serial.println("saving config");
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
    json["mqtt_server"] = mqtt_server;

    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile) {
      Serial.println("failed to open config file for writing");
    }

    json.printTo(Serial);
    json.printTo(configFile);
    configFile.close();
  }

  mqtt.setServer(mqtt_server, 1883);
  mqtt.setCallback(receivedMessage);

  MDNS.begin("siedle");
  httpUpdater.setup(&server);
  MDNS.addService("http", "tcp", 80);

  server.on("/", handleRoot);
  server.begin(); // Web server start

  ArduinoOTA.setHostname("siedle");
  ArduinoOTA.begin();
}

void loop() {
  server.handleClient();
  if (!mqtt.connected()) {
    reconnectMQTT();
  }
  mqtt.loop();
  ArduinoOTA.handle();

  readFromBus();
}

void handleRoot() {
  server.send(200, "text/html", "hallo");
}

void receivedMessage(char* topic_p, byte* payload, unsigned int length) {
  payload[length] = '\0';
  
  String topic = String((char*)topic_p);
  String payloads = String((char*)payload);
  Serial.println(topic + ": "+ payloads);
  if (topic == "siedle/cmnd/exec") {
    if (length < 8 || length > 10) return;
    uint32_t cmnd = strtoul((char*)payload, 0, 16);
    writeToBus(cmnd);
  }
}

void reconnectMQTT() {
  // Loop until we're reconnected
  while (!mqtt.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    // If you do not want to use a username and password, change next line to
    // if (client.connect("ESP8266Client")) {
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);
    if (mqtt.connect(clientId.c_str())) {
      Serial.println("connected");
      mqtt.subscribe("siedle/cmnd/exec");
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqtt.state());
      // Wait 20 seconds before retrying
      delay(20000);
    }
  }
}

void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

void writeToBus(uint32_t cmnd) {
  Serial.print("Writing ");
  Serial.print(cmnd, HEX);
  Serial.println(" to bus");
  digitalWrite(LOAD_PIN, HIGH);
  for (int i = 31; i >= 0; i--) {
    digitalWrite(WRITE_PIN, !bitRead(cmnd, i));
    delayMicroseconds(BIT_DURATION);
  }
  digitalWrite(WRITE_PIN, LOW);
  digitalWrite(LOAD_PIN, LOW);

  mqtt.publish("siedle/result", "ok", true);

  delay(3); //wait for bus to pull up
}

void readFromBus() {
  uint32_t cmnd;
  for (int i = 31; i >= 0; i--) {
    if (digitalRead(CARRIER_PIN) == LOW) return;
    if (i == 31) delayMicroseconds(BIT_DURATION / 4);
    bitWrite(cmnd, i, !digitalRead(BIT_PIN));
    delayMicroseconds(BIT_DURATION);
  }

  if(cmnd == 0) return;

  char value[9];
  String hex = String(cmnd, HEX);
  hex.toUpperCase();
  hex.toCharArray(value, 9);
  mqtt.publish("siedle/cmnd", value, true);
  
  Serial.print("detected command ");
  Serial.print(String(cmnd, HEX));
  Serial.println();
}

