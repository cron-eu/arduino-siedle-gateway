//
// Created by Remus Lazar on 24.01.20.
//

#include <Arduino.h>
#include "SiedleClient.h"
#include "Timer5.h"
#include "avdweb_AnalogReadFast.h"

#define R2_VAL 10000.0
#define R3_VAL 1100.0

// Note: actually we should use 2000 to get a timer period of 2000us.
// For some unknown reason we have to use this value instead, else the sampling
// rate will be 2028us
#define BIT_DURATION 1972

#define DEBUG_SAMPLING
#define SIEDLE_TX_DEBUG_PIN 4

// Bus Voltage = ADC Raw Value * ADC_FACTOR; the ADC Raw Value being the raw value
// as returned by analogRead()
#define ADC_FACTOR ( (R2_VAL + R3_VAL) / R3_VAL * (3.3 / 1024.0 ) )

// Detect a logical HIGH for voltages ABOVE this threshold
#define ADC_HIGH_THRESHOLD_VOLTAGE ( 4.5 )

// Maximum bus voltage while transmitting a logical "H".
// If the voltage is greater than this threshold, we assume that there was an error.
#define ADC_CARRIER_HIGH_THRESHOLD_VOLTAGE ( 12.0 )

static SiedleClient *currentInstance = NULL;

// Just a wrapper to call the class instance method
static void _rxISR() {
    if (currentInstance) {
        currentInstance->rxISR();
    }
}

static void _timerISR() {
    if (currentInstance) {
        currentInstance->bitTimerISR();
    }
}

SiedleClient::SiedleClient(uint8_t inputPin, uint8_t outputPin, uint8_t outputCarrierPin) {
    pinMode(inputPin, INPUT);
    pinMode(outputPin, OUTPUT);
    pinMode(outputCarrierPin, OUTPUT);
    this->inputPin = inputPin;
    this->outputPin = outputPin;
    this->outputCarrierPin = outputCarrierPin;
}

bool SiedleClient::begin() {
    digitalWrite(outputPin, LOW);
    if (currentInstance) { return false; }
    currentInstance = this;
    attachInterrupt(digitalPinToInterrupt(inputPin), _rxISR, FALLING);
    Timer5::onFire(_timerISR);
    Timer5::configure(BIT_DURATION);
    return true;
}

void __unused SiedleClient::end() {
    detachInterrupt(digitalPinToInterrupt(inputPin));
}

void SiedleClient::bitTimerISR() {

    bool done = false;

    switch (state) {
        case idle: break;

        case receiving:
            if (bitNumber >= 0) {
                if (bitNumber == 31) {
                    Timer5::changeSampleRate(BIT_DURATION);
                }
#ifdef DEBUG_SAMPLING
                digitalWrite(SIEDLE_TX_DEBUG_PIN, HIGH);
                delayMicroseconds(10);
                digitalWrite(SIEDLE_TX_DEBUG_PIN, LOW);
#endif
                bitWrite(cmd_rx_buf, bitNumber--, readBit());
            }

            if (bitNumber == -1) {
                // we just read the last bit, we're done
                done = true;
                if (cmd_rx_buf != 0) {
                    cmd = cmd_rx_buf;
                    rxCount++;
                    _available = true;
                }
            }

            break;

        case transmitting:
            if (bitNumber >= 0) {
                auto bit = bitRead(cmd_tx_buf, bitNumber--);
                digitalWrite(outputPin, !bit);
            } else {
                digitalWrite(outputPin, LOW);
                digitalWrite(outputCarrierPin, LOW);

                // wait until the bus master raises the voltage above a given threshold
                // we timeout after 3 periods
                if (bitNumber-- < -3) {
                    // timeout
                    done = true;
                } else {
                    auto voltage = getBusvoltage();
                    if (voltage >= ADC_CARRIER_HIGH_THRESHOLD_VOLTAGE) {
                        txCount++;
                        done = true;
                    }
                }
            }

            break;
    }

    if (done) {

#ifdef DEBUG_SAMPLING
        pinMode(SIEDLE_TX_DEBUG_PIN, INPUT);
#endif
        state = idle;
        pinMode(inputPin, INPUT);
        Timer5::disable();
        delayMicroseconds(100);
        attachInterrupt(digitalPinToInterrupt(inputPin), _rxISR, FALLING);
    }
}

void SiedleClient::rxISR() {
    if (state == transmitting) { return; }

    // we detach the interrupt here because we want to do a analogRead() later on
    // we must re-enable the interrupt prior to leaving this method, else the ISR will never be called again!
    // use the bailout label to prematurely bail out if needed.
    detachInterrupt(digitalPinToInterrupt(inputPin));

    // wait for the FALLING slope
    int i = 0;
    while (readBit() == HIGH) {
        if (i++ >= 4) { goto bailout; }
        delayMicroseconds(50);
    }

    cmd_rx_buf = 0;
    state = receiving;

    // we know that the first bit is a logical 0, read the remaining bits using the ISR
    bitNumber = 31;
    // this will also reset and start the timer
    Timer5::changeSampleRate(BIT_DURATION / 4);
    Timer5::enable();
    return;

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

void SiedleClient::sendCmd(siedle_cmd_t tx_cmd) {
    // disable the rx irq while we are transmitting data
    detachInterrupt(digitalPinToInterrupt(inputPin));

    cmd_tx_buf = tx_cmd;
    state = transmitting;

    bitNumber = 31;

    // Output the first bit (#31). Remaining bits will be transmitted using the ISR
    auto bit = bitRead(cmd_tx_buf, bitNumber--);
    digitalWrite(outputCarrierPin, HIGH);
    digitalWrite(outputPin, !bit);

    // we did already transmit the first bit, transmit the remaining bits using the ISR
    Timer5::changeSampleRate(BIT_DURATION);
    Timer5::enable();
}
