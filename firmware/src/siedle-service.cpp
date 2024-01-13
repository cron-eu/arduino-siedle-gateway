//
// Created by Remus Lazar on 04.02.20.
//

#include "siedle-service.h"
#include "rtc.h"

void SiedleServiceClass::begin() {
    lastTxMillis = 0;
    lastTxCounter = 0;
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

    if (siedleClient.state == idle) {
        if (siedleClient.txError) {
            if (retryCounter == -1) {   
                retryCounter = 3;   
            }   
        } else {
            retryCounter = -1;
        }
    }

    auto doSend = (!siedleTxQueue.isEmpty() || retryCounter > 0) 
        && (now - lastTxMillis > BUS_MAX_SEND_RATE_MS)
        && siedleClient.state == idle;
    siedle_cmd_t cmd;

    if (doSend) {
        if (retryCounter > 0) {
            retryCounter--;
            cmd = lastTxCmd;
        } else {
            cmd = siedleTxQueue.pop();            
        }
        siedleClient.sendCmdAsync(cmd);
        lastTxCmd = cmd;
    }

     // check if we have sent a new cmd since last run and, if so, send the cmd via MQTT
     // note: the siedle client will not increment the tx counter if there were errors!
    if (siedleClient.txCount > lastTxCounter) {
        lastTxCounter = siedleClient.txCount;
        MQTTService.sendAsync({RTCSync.getEpoch(), lastTxCmd}, sent);
    }
    interrupts();

    if (doSend) {
        siedleRxTxLog.push({ { RTCSync.getEpoch(), cmd }, tx });
        lastTxMillis = now;
    }
}
SiedleServiceClass SiedleService;
