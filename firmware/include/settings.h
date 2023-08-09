//
// Created by Remus Lazar on 27.01.20.
// Settings like Log Size and other tweaks
//

#ifndef FIRMWARE_SETTINGS_H
#define FIRMWARE_SETTINGS_H

#define NTP_SERVER "de.pool.ntp.org"

// comment out to disable the MQTT functionality (does not make much sense, useful for debugging)
#define USE_MQTT

// Siedle Log size (shown in the Web UI)
#define LOG_SIZE 32
 
#define MQTT_TX_QUEUE_LEN 32
#define SIEDLE_TX_QUEUE_LEN 32

// MQTT connect retry interval in ms
#define MQTT_RECONNECT_INTERVAL_MS 5000

// MQTT socket timeout (in seconds)
// do not set this too high to avoid issues with the watchdog!
#define MQTT_TIMEOUT_SEC 10

// how often will the RTC synchronize with the WiFi clock
#define RTC_SYNC_INTERVAL_SEC 30

// use the WiFi low power mode on SAMD devices
#define USE_WIFI_LOW_POWER

// Use the DEBUG_PRINT() for debugging using the serial console
// #define USE_DEBUG_SERIAL_CONSOLE

#endif //FIRMWARE_SETTINGS_H

// MQTT buffer size in bytes
#define MQTT_BUF_SIZE 256