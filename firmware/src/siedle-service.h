//
// Created by Remus Lazar on 04.02.20.
//

#ifndef FIRMWARE_SIEDLE_SERVICE_H
#define FIRMWARE_SIEDLE_SERVICE_H

#include "settings.h"
#include "hardware.h"

#include <Arduino.h>
#include <CircularBuffer.h>
#include <SiedleClient.h>

#include "siedle-structs.h"
#include "mqtt-service.h"
#include "siedle-log.h"

// max siedle bus send rate
#define BUS_MAX_SEND_RATE_MS 800

class SiedleServiceClass {
public:
    SiedleServiceClass() : siedleClient(SIEDLE_A_IN, SIEDLE_TX_PIN, SIEDLE_TX_CARRIER_PIN), siedleTxQueue() { }
    void begin();
    void loop();

    /**
     * Queue a Siedle Command to be transmitted
     *
     * This method will return instantly and save the data in an internal
     * circular buffer. The sending process is async.
     * @param data
     */
    void transmitAsync(siedle_cmd_t cmd) {
        siedleTxQueue.push(cmd);
    }

    SiedleClient siedleClient;

private:
    CircularBuffer<siedle_cmd_t, SIEDLE_TX_QUEUE_LEN> siedleTxQueue;
    unsigned long lastTxMillis;
};

extern SiedleServiceClass SiedleService;

#endif //FIRMWARE_SIEDLE_SERVICE_H
