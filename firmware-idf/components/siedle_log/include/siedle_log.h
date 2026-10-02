/*
 * Thread safe ring buffer of the most recent bus events, shown in the web UI.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <time.h>

#include "esp_err.h"
#include "siedle_proto.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    SIEDLE_LOG_RX = 0, /**< received from the bus */
    SIEDLE_LOG_TX = 1, /**< sent to the bus */
} siedle_log_dir_t;

typedef struct {
    time_t timestamp; /**< UTC, 0 if the clock was not synchronized yet */
    siedle_cmd_t cmd;
    siedle_log_dir_t dir;
    bool ok;          /**< false if a transmission was not acknowledged by the bus master */
} siedle_log_entry_t;

esp_err_t siedle_log_init(void);

void siedle_log_add(siedle_cmd_t cmd, siedle_log_dir_t dir, bool ok);

/**
 * Copy the most recent entries, newest first.
 *
 * @return number of entries copied
 */
size_t siedle_log_get(siedle_log_entry_t *out, size_t max);

#ifdef __cplusplus
}
#endif
