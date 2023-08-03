#include <Arduino.h>
#include <serial-debug.h>

#include <WebServer.h>
#include <time.h>

#ifdef ARDUINO_ARCH_SAMD
#include <WDTZero.h>
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
#ifdef ARDUINO_ARCH_SAMD
WDTZero Watchdog;
#endif

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
    Watchdog.setup(WDT_HARDCYCLE8S);
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

#ifdef ARDUINO_ARCH_SAMD
    wdtLoop();
#endif
}
