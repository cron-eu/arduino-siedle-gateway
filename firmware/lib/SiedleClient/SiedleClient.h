//
// Created by Remus Lazar on 24.01.20.
//

#ifndef FIRMWARE_SIEDLECLIENT_H
#define FIRMWARE_SIEDLECLIENT_H

#include <Arduino.h>

class SiedleClient {
public:
    SiedleClient(uint8_t inputPin);
    RingBufferN<100> buffer;
    void loop();
    float getBusvoltage();

private:
    uint8_t inputPin;
    int readBit();
};


#endif //FIRMWARE_SIEDLECLIENT_H
