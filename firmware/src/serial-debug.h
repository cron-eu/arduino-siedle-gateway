//
// Created by Remus Lazar on 30.06.20.
//

#ifndef FIRMWARE_SERIAL_DEBUG_H
#define FIRMWARE_SERIAL_DEBUG_H

#include <Arduino.h>

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

#endif //FIRMWARE_SERIAL_DEBUG_H
