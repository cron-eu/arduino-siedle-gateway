//
// Created by Remus Lazar on 24.01.20.
//

#include <Arduino.h>
#include "SiedleClient.h"
#include "avdweb_AnalogReadFast.h"

#define R2_VAL 10000.0
#define R3_VAL 1100.0
#define BIT_DURATION 2000

// Bus Voltage = ADC Raw Value * ADC_FACTOR; the ADC Raw Value being the raw value
// as returned by analogRead()
#define ADC_FACTOR ( (R2_VAL + R3_VAL) / R3_VAL * (3.3 / 1024.0 ) )

// Detect a logical HIGH for voltages ABOVE this threshold
#define ADC_HIGH_THRESHOLD_VOLTAGE ( 4.5 )

SiedleClient::SiedleClient(uint8_t inputPin) {
    pinMode(inputPin, INPUT);
    this->inputPin = inputPin;
}

bool SiedleClient::receiveLoop() {

    unsigned long const sample_interval = BIT_DURATION;
    unsigned long sample_micros = micros();

    // analogRead on the MKR series takes about 500us, so after 2 reads we should be about fine
    for (int i = 0; i <= 1; i++) {
        if (readBit() == HIGH) {
            yield();
            return false;
        }
    }

    while (micros() - sample_micros < sample_interval / 2 ) { yield(); }

    sample_micros = micros();

    uint32_t cmnd = 0x0;

    // we do already got the first bit, this being a logical 0
    // read in the remaining 31 bits now
    state = receiving;
    for (int i = 30; i >= 0; i--) {
        // no worries about rollover
        // @see https://arduino.stackexchange.com/questions/12587/how-can-i-handle-the-millis-rollover
        while (micros() - sample_micros < sample_interval) { yield(); }
        bitWrite(cmnd, i, readBit());
        sample_micros += sample_interval;
    }

    // Make sure we wait until the master pushes up the bus voltage
    while(readBit() == LOW) {
        yield();
    }

    this->cmd = cmnd;
    rxCount++;

    state = idle;
}

inline int SiedleClient::readBit() {
    return getBusvoltage() <= ADC_HIGH_THRESHOLD_VOLTAGE ? LOW : HIGH;
}

float SiedleClient::getBusvoltage() {
    auto a = analogReadFast(inputPin);
    return (float)a * (float)ADC_FACTOR;
}
