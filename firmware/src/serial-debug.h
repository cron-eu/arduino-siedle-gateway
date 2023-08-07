//
// Created by Remus Lazar on 30.06.20.
//

#ifndef FIRMWARE_SERIAL_DEBUG_H
#define FIRMWARE_SERIAL_DEBUG_H

#include <Arduino.h>
#include "settings.h"

// This class will provide a Print compatible interface to send debug messages to the attached serial device
// If there is no serial device attached, it will be a no-op.
class SerialDebugClass : public Print {
public:
    void begin() {
        Serial.begin(115200);
    }

    size_t write(uint8_t uint8) override {
        if (Serial) {
            return Serial.write(uint8);
        }
        return 0;
    }

private:
};

extern SerialDebugClass Debug;

#ifdef USE_DEBUG_SERIAL_CONSOLE
#define DEBUG_PRINT(...) Debug.print(__VA_ARGS__)
#define DEBUG_PRINTLN(...) Debug.print(__VA_ARGS__)
#else
#define DEBUG_PRINT(...)
#define DEBUG_PRINTLN(...)
#endif

#endif //FIRMWARE_SERIAL_DEBUG_H
