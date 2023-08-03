//
// Created by Remus Lazar on 04.02.20.
//

#include "siedle-service.h"
#include "rtc.h"

void SiedleServiceClass::begin() {
    lastTxMillis = 0;
    siedleClient = SiedleClient(SIEDLE_A_IN, SIEDLE_TX_PIN, SIEDLE_TX_CARRIER_PIN);
    siedleClient.begin();
}

void SiedleServiceClass::loop() {

    noInterrupts();
    auto available = siedleClient.available();
    siedle_cmd_t read;
    if (available) {
        read = siedleClient.read();
    }
    interrupts();

    if (available) {
        SiedleLogEntry entry = { RTCSync.getEpoch(), read };
        MQTTService.sendAsync(entry, received);
        siedleRxTxLog.push({ entry, rx });
    }

    // introduce some padding between the send requests


    noInterrupts();
    auto now = millis();
    auto doSend = !siedleTxQueue.isEmpty()
        && (now - lastTxMillis > BUS_MAX_SEND_RATE_MS)
        && siedleClient.state == idle;
    siedle_cmd_t cmd;

    if (doSend) {
        cmd = siedleTxQueue.pop();
    }
    interrupts();

    if (doSend) {
        MQTTService.sendAsync({RTCSync.getEpoch(), cmd}, sent);
        siedleClient.sendCmd(cmd);
        siedleRxTxLog.push({ { RTCSync.getEpoch(), cmd }, tx });
        lastTxMillis = now;
    }
}
SiedleServiceClass SiedleService;
