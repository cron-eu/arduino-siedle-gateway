//
// Created by Remus Lazar on 04.02.20.
//

#ifndef FIRMWARE_SIEDLE_LOG_H
#define FIRMWARE_SIEDLE_LOG_H

#include <Arduino.h>
#include "siedle-structs.h"
#include <CircularBuffer.h>
#include "settings.h"

enum Direction {
    rx = 0,
    tx,
};

typedef struct {
    SiedleLogEntry log;
    Direction direction;
} SiedleRxTxLogEntry;

extern CircularBuffer<SiedleRxTxLogEntry, LOG_SIZE> siedleRxTxLog;

#endif //FIRMWARE_SIEDLE_LOG_H
