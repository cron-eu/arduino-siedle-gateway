//
// Created by Remus Lazar on 04.02.20.
//

#ifndef FIRMWARE_SIEDLE_STRUCTS_H
#define FIRMWARE_SIEDLE_STRUCTS_H

#include <SiedleClient.h>

typedef struct {
    unsigned long timestamp;
    siedle_cmd_t cmd;
} SiedleLogEntry;


#endif //FIRMWARE_SIEDLE_STRUCTS_H
