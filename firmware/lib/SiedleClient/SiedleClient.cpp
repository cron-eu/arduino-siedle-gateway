//
// Created by Remus Lazar on 24.01.20.
//

#include <Arduino.h>
#include "SiedleClient.h"

#define R2_VAL 33000.0
#define R3_VAL 2200.0
#define BIT_DURATION 1980

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
    if (readBit() == HIGH) {
        yield();
        return;
    }

    // we got the start bit, lets read all 32 bits in
    uint32_t cmnd = 0x0;

    auto micros_next_bit = micros() + BIT_DURATION * 1.25;

    for (int i = 31; i >= 0; i--) {
        while (micros() < micros_next_bit) {
            yield();
        }
        bitWrite(cmnd, i, readBit());
        micros_next_bit += BIT_DURATION;
    }

    if (!buffer.isFull()) {
        buffer.store_char(cmnd);
    }

    // Make sure we wait until the master pushes up the bus voltage
    while(readBit() != HIGH) {
        delay(1);
    }
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

