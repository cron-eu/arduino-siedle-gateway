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

// Maximum bus voltage while transmitting a logical "H".
// If the voltage is greater than this threshold, we assume that there was an error.
#define ADC_CARRIED_HIGH_THRESHOLD_VOLTAGE ( 12.0 )

static SiedleClient *currentInstance = NULL;

// Just a wrapper to call the class instance method
static void _rxISR() {
    if (currentInstance) {
        currentInstance->rxISR();
    }
}

SiedleClient::SiedleClient(uint8_t inputPin, uint8_t outputPin) {
    pinMode(inputPin, INPUT);
    pinMode(outputPin, OUTPUT);
    this->inputPin = inputPin;
    this->outputPin = outputPin;
}

bool SiedleClient::begin() {
    digitalWrite(outputPin, LOW);
    if (currentInstance) { return false; }
    currentInstance = this;
    attachInterrupt(digitalPinToInterrupt(inputPin), _rxISR, FALLING);
    return true;
}

void __unused SiedleClient::end() {
    detachInterrupt(digitalPinToInterrupt(inputPin));
}

void SiedleClient::rxISR() {
    unsigned long const sample_interval = BIT_DURATION;
    int i;
    uint32_t cmnd = 0x0;

    if (state == transmitting) { return; }

    // we detach the interrupt here because we want to do a analogRead() later on
    // we must re-enable the interrupt prior to leaving this method, else the ISR will never be called again!
    // use the bailout label to prematurely bail out if needed.
    detachInterrupt(digitalPinToInterrupt(inputPin));

    // wait for the FALLING slope
    i = 0;
    while (readBit() == HIGH) {
        if (i>10) { goto bailout; }
        delayMicroseconds(50);
    }

    delayMicroseconds(sample_interval * 1.5);

    // we do already got the first bit, this being a logical 0
    // read in the remaining 31 bits now
    state = receiving;
    for (i = 30; i >= 0; i--) {
        bitWrite(cmnd, i, readBit());
        delayMicroseconds(sample_interval - 10);
    }

    if (cmnd == 0) {
        state = idle;
        goto bailout;
    }

    cmd = cmnd;
    rxCount++;

    _available = true;
    state = idle;

    bailout:
    pinMode(inputPin, INPUT);
    delayMicroseconds(100);
    attachInterrupt(digitalPinToInterrupt(inputPin), _rxISR, FALLING);
}

inline int SiedleClient::readBit() {
    return getBusvoltage() <= ADC_HIGH_THRESHOLD_VOLTAGE ? LOW : HIGH;
}

float SiedleClient::getBusvoltage() {
    auto a = analogReadFast(inputPin);
    return (float)a * (float)ADC_FACTOR;
}

bool SiedleClient::sendCmd(siedle_cmd_t tx_cmd) {
    // disable the rx irq while we are transmitting data
    detachInterrupt(digitalPinToInterrupt(inputPin));
    bool retVal = true;

    state = transmitting;

    for (int i=31; i >= 0; i--) {
        auto bit = bitRead(tx_cmd, i);
        digitalWrite(outputPin, !bit);

        // Check if the bus master holds the bus voltage below a specific threshold
        if (i == 31) {
            delayMicroseconds(BIT_DURATION - 52);
            auto voltage = getBusvoltage();
            if (voltage >= ADC_CARRIED_HIGH_THRESHOLD_VOLTAGE) {
                // if not, bail out with an error
                retVal = false; // return with error
                goto bailout;
            }
        } else {
            delayMicroseconds(BIT_DURATION - 8);
        }
    }

    txCount++;

    bailout:
    digitalWrite(outputPin, LOW);
    pinMode(inputPin, INPUT);
    delayMicroseconds(100);
    attachInterrupt(digitalPinToInterrupt(inputPin), _rxISR, FALLING);

    state = idle;

    return retVal;
}
