//
// Created by Remus Lazar on 24.01.20.
//

#ifndef FIRMWARE_SIEDLECLIENT_H
#define FIRMWARE_SIEDLECLIENT_H

#define siedle_cmd_t uint32_t

#include <Arduino.h>

enum SiedleClientState {
    idle,
    receiving
};

class SiedleClient {
public:
    SiedleClient(uint8_t inputPin);
    /**
     * Tries to receive data, if available.
     *
     * This method should be put in a runloop, e.g. using the Scheduler library
     *
     * @return New Data available
     */
    bool receiveLoop();
    unsigned int rxCount = 0;
    // Last received command
    siedle_cmd_t cmd;
    SiedleClientState state = idle;
    float getBusvoltage();

private:
    int readBit();
    uint8_t inputPin;
};

#endif //FIRMWARE_SIEDLECLIENT_H
