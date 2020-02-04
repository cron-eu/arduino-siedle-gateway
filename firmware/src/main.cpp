#include <Arduino.h>

#include <WebServer.h>
#include <time.h>
#include <Adafruit_SleepyDog.h>

#include "siedle-log.h"
#include "led.h"
#include "wifi-manager.h"
#include "rtc.h"
#include "siedle-service.h"
#include "web-ui.h"

#ifdef USE_MQTT
#include "mqtt-service.h"
#endif

WebServer webServer(80);

void __unused setup() {
    Serial.begin(115200);
    Serial.println("Booting..");

    LED.begin();
    RTCSync.begin();
    SiedleService.begin();
    WiFiManager.begin();
    RTCSync.begin();
#ifdef USE_MQTT
    MQTTService.begin();
#endif

    Watchdog.enable(WDT_TIMEOUT_MS);
    webServer.rootPageHandler = webUIHTMLHandler;
    webServer.begin();
}

void inline wdtLoop() {
    static unsigned long lastMillis = 0;
    if (millis() - lastMillis > 500) {
        lastMillis = millis();
        Watchdog.reset();
    }
}

void __unused loop() {
    LED.loop();
    WiFiManager.loop();
    RTCSync.loop();
    SiedleService.loop();
#ifdef USE_MQTT
    MQTTService.loop();
#endif
    webServer.loop();
    wdtLoop();
}
