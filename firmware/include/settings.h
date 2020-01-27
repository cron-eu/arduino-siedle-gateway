//
// Created by Remus Lazar on 27.01.20.
// Settings like Log Size and other tweaks
//

#ifndef FIRMWARE_SETTINGS_H
#define FIRMWARE_SETTINGS_H

// comment out to disable the MQTT functionality (does not make much sense, useful for debugging)
#define USE_MQTT

// Siedle Log size (used for internal buffering in cases where the MQTT server is not reachable) and also for
// the Web UI
#define LOG_SIZE 32

#endif //FIRMWARE_SETTINGS_H
