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
#define LOG_SIZE 64

#define MQTT_TX_QUEUE_LEN 32
#define SIEDLE_TX_QUEUE_LEN 32

// MQTT connect retry interval in ms
#define MQTT_RECONNECT_INTERVAL_MS 10000

#endif //FIRMWARE_SETTINGS_H
