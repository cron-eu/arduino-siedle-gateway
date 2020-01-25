//
// Created by Remus Lazar on 24.01.20.
//

#ifndef FIRMWARE_SIEDLECLIENT_H
#define FIRMWARE_SIEDLECLIENT_H

#define siedle_cmd_t uint32_t

#include <Arduino.h>

enum SiedleClientState {
    idle,
    receiving,
    transmitting
};

class SiedleClient {
public:
    /**
     * @param inputPin analog input pin
     * @param outputPin digital out pin (*negated*)
     */
    SiedleClient(uint8_t inputPin, uint8_t outputPin);
    /**
     * Tries to receive data, if available.
     *
     * This method should be put in a runloop, e.g. using the Scheduler library
     *
     * @return New Data available
     */
    bool receiveLoop();
    unsigned int rxCount = 0;
    unsigned int txCount = 0;
    // Last received command
    siedle_cmd_t cmd;
    // Send a command. Returns false if there was an error while trying to send, e.g. the bus master did not respond on time.
    bool sendCmd(siedle_cmd_t cmd);
    SiedleClientState state = idle;
    float getBusvoltage();

private:
    int readBit();
    uint8_t inputPin;
    uint8_t outputPin;
};

#endif //FIRMWARE_SIEDLECLIENT_H
