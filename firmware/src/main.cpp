#include <Arduino.h>
#include <serial-debug.h>

#include <WebServer.h>
#include <time.h>

#ifdef ARDUINO_ARCH_SAMD
#include <WDTZero.h>
#endif
#include <ArduinoOTA.h>

#include "siedle-log.h"
#include "led.h"
#include "wifi-manager.h"
#include "rtc.h"
#include "siedle-service.h"
#include "web-ui.h"
#include "ota_secrets.h"

#ifdef USE_MQTT
#include "mqtt-service.h"
#endif

WebServer webServer(80);
#ifdef ARDUINO_ARCH_SAMD
WDTZero Watchdog;
#endif

static bool is_ota_initialized = false;
static unsigned long ota_init_attempt_last_millis = 0;

void __unused setup() {
    Debug.begin();

    Debug.println("Booting..");

    LED.begin();
    SiedleService.begin();

    #ifdef ARDUINO_ARCH_SAMD
    RTCSync.begin();
    WiFiManager.begin();
    #else
    WiFiManager.begin();
    RTCSync.begin();
    #endif

#ifdef USE_MQTT
    MQTTService.begin();
#endif

    webServer.rootPageHandler = webUIHTMLHandler;
    webServer.begin();
#ifdef ARDUINO_ARCH_SAMD
    Watchdog.setup(WDT_HARDCYCLE16S);
#endif

}

#ifdef ARDUINO_ARCH_SAMD
void inline wdtLoop() {
    static unsigned long lastMillis = 0;
    if (millis() - lastMillis > 250) {
        lastMillis = millis();
        Watchdog.clear();
    }
}
#endif

void __unused loop() {
    LED.loop();
    WiFiManager.loop();
    RTCSync.loop();
    SiedleService.loop();
#ifdef USE_MQTT
    MQTTService.loop();
#endif
    webServer.loop();

    if (!is_ota_initialized && (millis() - ota_init_attempt_last_millis) > 500) {
        ota_init_attempt_last_millis = millis();

        if (WiFi.status() == WL_CONNECTED) {
            ArduinoOTA.begin(WiFi.localIP(), OTA_USERNAME, OTA_PASSWORD, InternalStorage);
            is_ota_initialized = true;
        }
    }

    if (is_ota_initialized) {
        ArduinoOTA.poll();
    }

#ifdef ARDUINO_ARCH_SAMD
    wdtLoop();
#endif
}
