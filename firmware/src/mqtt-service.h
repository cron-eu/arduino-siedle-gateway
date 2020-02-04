//
// Created by Remus Lazar on 04.02.20.
//

#ifndef FIRMWARE_MQTT_SERVICE_H
#define FIRMWARE_MQTT_SERVICE_H

#include <CircularBuffer.h>
#include "siedle-structs.h"
#include "settings.h"

class MQTTServiceClass {
public:
    void begin();
    void loop();
    unsigned int mqttReconnects;

    /**
     * Queue a Siedle Log Entry for sending
     *
     * This method will return instantly and save the data in an internal
     * circular buffer. The sending process is async.
     * @param data
     */
    void sendAsync(SiedleLogEntry data) {
        mqttTxQueue.push(data);
    }
    bool isConnected();

private:
    unsigned long reconnectMillis;
    unsigned long lastTxMillis;
    CircularBuffer<SiedleLogEntry, MQTT_TX_QUEUE_LEN> mqttTxQueue;
};

extern MQTTServiceClass MQTTService;

#endif //FIRMWARE_MQTT_SERVICE_H
