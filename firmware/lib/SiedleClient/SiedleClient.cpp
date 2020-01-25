//
// Created by Remus Lazar on 24.01.20.
//

#include <Arduino.h>
#include "SiedleClient.h"

#define R2_VAL 10000.0
#define R3_VAL 1100.0
#define BIT_DURATION 2000

// Bus Voltage = ADC Raw Value * ADC_FACTOR; the ADC Raw Value being the raw value
// as returned by analogRead()
#define ADC_FACTOR ( (R2_VAL + R3_VAL) / R3_VAL * (3.3 / 4096.0 ) )

// Detect a logical HIGH for voltages ABOVE this threshold
#define ADC_HIGH_THRESHOLD_VOLTAGE ( 5.0 )

SiedleClient::SiedleClient(uint8_t inputPin) {
    pinMode(inputPin, INPUT);
    analogReadResolution(12);
    this->inputPin = inputPin;
}

void SiedleClient::loop() {

    unsigned long const sample_interval = BIT_DURATION;
    unsigned long sample_micros = micros();

    // analogRead on the MKR series takes about 500us, so after 2 reads we should be about fine
    for (int i = 0; i <= 1; i++) {
        if (readBit() == HIGH) {
            yield();
            return;
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

    putCmd(cmnd);
    rxCount++;

    // Make sure we wait until the master pushes up the bus voltage
    while(readBit() == LOW) {
        delay(1);
    }

    state = idle;
}

inline int SiedleClient::readBit() {
    return getBusvoltage() <= ADC_HIGH_THRESHOLD_VOLTAGE ? LOW : HIGH;
}

float SiedleClient::getBusvoltage() {
    auto a = analogRead(inputPin);
    return (float)a * (float)ADC_FACTOR;
}

bool SiedleClient::available() {
    return read_index != write_index;
}

siedle_cmd_t SiedleClient::getCmd() {
    if (!available()) { return 0; }
    auto payload = buffer[read_index];
    if (read_index++ == sizeof(buffer) - 1) { read_index = 0; }
    return payload;
}

void SiedleClient::putCmd(siedle_cmd_t cmd) {
    buffer[write_index] = cmd;
    if (write_index++ == sizeof(buffer) - 1) { write_index = 0; }
}
