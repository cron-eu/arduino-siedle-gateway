#include <Arduino.h>
#include <serial-debug.h>

#include <WebServer.h>
#include <time.h>

#ifdef ARDUINO_ARCH_SAMD
#include <Adafruit_SleepyDog.h>
#endif

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
    Debug.begin();
    Debug.println("Booting..");

    LED.begin();
    RTCSync.begin();
    SiedleService.begin();
    WiFiManager.begin();
    RTCSync.begin();
#ifdef USE_MQTT
    MQTTService.begin();
#endif

#ifdef ARDUINO_ARCH_SAMD
    Watchdog.enable(WDT_TIMEOUT_MS);
#endif

    webServer.rootPageHandler = webUIHTMLHandler;
    webServer.begin();
}

#ifdef ARDUINO_ARCH_SAMD
void inline wdtLoop() {
    static unsigned long lastMillis = 0;
    if (millis() - lastMillis > 500) {
        lastMillis = millis();
        Watchdog.reset();
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

    #ifdef ARDUINO_ARCH_SAMD
    wdtLoop();
#endif
}
