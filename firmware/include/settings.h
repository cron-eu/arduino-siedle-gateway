//
// Created by Remus Lazar on 27.01.20.
// Settings like Log Size and other tweaks
//

#ifndef FIRMWARE_SETTINGS_H
#define FIRMWARE_SETTINGS_H

// comment out to disable the MQTT functionality (does not make much sense, useful for debugging)
#define USE_MQTT

// Siedle Log size (shown in the Web UI)
#define LOG_SIZE 64

// Watchdog timer expire value (in ms)
#define WDT_TIMEOUT_MS 8000

#define MQTT_TX_QUEUE_LEN 32
#define SIEDLE_TX_QUEUE_LEN 32

#endif //FIRMWARE_SETTINGS_H
