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
    if (siedleClient.available()) {
        SiedleLogEntry entry = { RTCSync.getEpoch(), siedleClient.read() };
        MQTTService.sendAsync(entry, received);
        siedleRxTxLog.push({ entry, rx });
    }

    // introduce some padding between the send requests
    if (!siedleTxQueue.isEmpty()) {
        auto now = millis();
        if (now - lastTxMillis > BUS_MAX_SEND_RATE_MS && siedleClient.state == idle) {
            auto cmd = siedleTxQueue.pop();
            MQTTService.sendAsync({RTCSync.getEpoch(), cmd}, sent);
            siedleClient.sendCmd(cmd);
            siedleRxTxLog.push({ { RTCSync.getEpoch(), cmd }, tx });
            lastTxMillis = now;
        }
    }
}
SiedleServiceClass SiedleService;
