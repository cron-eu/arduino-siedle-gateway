//
// Created by Remus Lazar on 24.01.20.
//

#ifndef FIRMWARE_SIEDLECLIENT_H
#define FIRMWARE_SIEDLECLIENT_H

#define siedle_cmd_t uint32_t

// length of the cmd buffer
#define BUFLEN 32

#include <Arduino.h>

enum SiedleClientState {
    idle,
    receiving
};

class SiedleClient {
public:
    SiedleClient(uint8_t inputPin);
    void loop();
    float getBusvoltage();
    bool available();
    siedle_cmd_t getCmd();
    SiedleClientState getState() { return state; }
    int getRxCount() { return rxCount; }

private:
    // buffer is a circular buffer
    siedle_cmd_t buffer[BUFLEN];
    int read_index = 0;
    int write_index = 0;
    void putCmd(siedle_cmd_t cmd);
    SiedleClientState state = idle;
    int rxCount = 0;

    uint8_t inputPin;
    int readBit();
};


#endif //FIRMWARE_SIEDLECLIENT_H
