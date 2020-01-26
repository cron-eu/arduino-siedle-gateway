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
     * SiedleClient provides high level methods to send and receive data. The latter is being done using a ISR to avoid
     * timing issues.
     *
     * Important Note: This Class is not yet ready to be instantiate more than once! (because the ISR is not multi-
     * instance ready)
     *
     * @param inputPin analog input pin
     * @param outputPin digital out pin (*negated*)
     */
    SiedleClient(uint8_t inputPin, uint8_t outputPin);
    /**
     * Attach the ISR and start receiving data.
     *
     * @return false if there was an error
     */
    bool begin();
    void end();
    unsigned int rxCount = 0;
    unsigned int txCount = 0;

    // Send a command. Returns false if there was an error while trying to send, e.g. the bus master did not respond on time.
    bool sendCmd(siedle_cmd_t cmd);
    SiedleClientState state = idle;
    float getBusvoltage();
    void rxISR();
    bool available() {
        return _available;
    }
    siedle_cmd_t read() {
        _available = false;
        return cmd;
    }

private:
    int readBit();
    uint8_t inputPin;
    uint8_t outputPin;
    // Last received command
    volatile siedle_cmd_t cmd;
    volatile bool _available = false;
};

#endif //FIRMWARE_SIEDLECLIENT_H
