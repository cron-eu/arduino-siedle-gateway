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
    SiedleClient(uint8_t inputPin, uint8_t outputPin, uint8_t outputCarrierPin);
    /**
     * Attach the ISR and start receiving data.
     *
     * @return false if there was an error
     */
    bool begin();
    void end();
    volatile unsigned int rxCount = 0;
    volatile unsigned int txCount = 0;

    // Send a command. Returns false if there was an error while trying to send, e.g. the bus master did not respond on time.
    void sendCmd(siedle_cmd_t cmd);
    volatile SiedleClientState state = idle;
    float getBusvoltage();
    void rxISR();
    void bitTimerISR();
    volatile int irq_count = 0;

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
    uint8_t outputCarrierPin;
    // Last received command
    volatile siedle_cmd_t cmd;
    // Used as a temporary buffer from the rx isr
    volatile siedle_cmd_t cmd_rx_buf;
    // Used as a temporary buffer from the tx isr
    volatile siedle_cmd_t cmd_tx_buf;
    volatile bool _available = false;
    volatile int bitNumber = -1;
};

#endif //FIRMWARE_SIEDLECLIENT_H
