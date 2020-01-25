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
#define ADC_HIGH_THRESHOLD_VOLTAGE ( 4.0 )

SiedleClient::SiedleClient(uint8_t inputPin) {
    pinMode(inputPin, INPUT);
    analogReadResolution(12);
    this->inputPin = inputPin;
}

void SiedleClient::loop() {

    // analogRead on the MKR series takes about 500us, so after 2 reads we should be about fine
    for (int i = 0; i <= 1; i++) {
        if (readBit() == HIGH) {
            yield();
            return;
        }
    }

    state = receiving;
    unsigned long sample_micros = micros();
    unsigned long sample_interval = BIT_DURATION;

    // we got the start bit, lets read all 32 bits in
    uint32_t cmnd = 0x0;

    // we do already have the first bit (#31, this being a logical 0), so let's read in the remaining 31 bits
    for (int i = 30; i >= 0; i--) {
        while (micros() - sample_micros < sample_interval) {
            yield();
        }
        bitWrite(cmnd, i, readBit());
        sample_micros += sample_interval;
    }

    putCmd(cmnd);

    // Make sure we wait until the master pushes up the bus voltage
    while(readBit() == LOW) {
        delay(1);
    }

    rxCount++;
    state = idle;
}

int SiedleClient::readBit() {
    auto a = analogRead(inputPin);
    float voltage = (float)a * ADC_FACTOR;
    return voltage >= ADC_HIGH_THRESHOLD_VOLTAGE ? HIGH : LOW;
}

float SiedleClient::getBusvoltage() {
    auto a = analogRead(inputPin);
    return (float)a * ADC_FACTOR;
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
