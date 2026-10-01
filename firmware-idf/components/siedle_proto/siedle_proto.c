#include "siedle_proto.h"

#define SIGNAL_SHIFT 25
#define SIGNAL_MASK  0xFu
#define DST_SHIFT    14
#define SRC_SHIFT    2
#define ADDR_MASK    0x1FFu

uint8_t siedle_cmd_signal(siedle_cmd_t cmd)
{
    return (cmd >> SIGNAL_SHIFT) & SIGNAL_MASK;
}

uint16_t siedle_cmd_dst(siedle_cmd_t cmd)
{
    return (cmd >> DST_SHIFT) & ADDR_MASK;
}

uint16_t siedle_cmd_src(siedle_cmd_t cmd)
{
    return (cmd >> SRC_SHIFT) & ADDR_MASK;
}

siedle_cmd_t siedle_cmd_make(uint8_t signal, uint16_t src, uint16_t dst)
{
    return (0x2u << 29)
           | ((siedle_cmd_t)(signal & SIGNAL_MASK) << SIGNAL_SHIFT)
           | ((siedle_cmd_t)(dst & ADDR_MASK) << DST_SHIFT)
           | (0x2u << 11)
           | ((siedle_cmd_t)(src & ADDR_MASK) << SRC_SHIFT);
}

const char *siedle_signal_name(uint8_t signal)
{
    switch (signal) {
    case SIEDLE_SIGNAL_TALK_START: return "talk start";
    case SIEDLE_SIGNAL_RING: return "ring";
    case SIEDLE_SIGNAL_DOOR_OPEN: return "door open";
    case SIEDLE_SIGNAL_TALK_END: return "talk end";
    case SIEDLE_SIGNAL_LIGHT: return "light";
    case SIEDLE_SIGNAL_RING_ALT: return "ring (alt)";
    default: return NULL;
    }
}

size_t siedle_encode_runs(siedle_cmd_t cmd, uint32_t bit_us, siedle_run_t *runs, size_t max_runs)
{
    size_t n = 0;
    for (int bit = SIEDLE_FRAME_BITS - 1; bit >= 0; bit--) {
        uint8_t level = (cmd >> bit) & 1u;
        if (n > 0 && runs[n - 1].level == level) {
            runs[n - 1].duration_us += bit_us;
            continue;
        }
        if (n == max_runs) {
            return 0;
        }
        runs[n++] = (siedle_run_t){ .level = level, .duration_us = bit_us };
    }
    return n;
}

siedle_decode_result_t siedle_decode_runs(const siedle_run_t *runs, size_t num_runs, uint32_t bit_us,
                                          siedle_cmd_t *out_cmd)
{
    if (num_runs == 0 || runs[0].level != 0) {
        return SIEDLE_DECODE_NO_START;
    }

    siedle_cmd_t cmd = 0;
    int remaining = SIEDLE_FRAME_BITS;

    for (size_t i = 0; i < num_runs && remaining > 0; i++) {
        uint8_t level = runs[i].level ? 1 : 0;
        uint32_t duration = runs[i].duration_us;
        int bits;

        if (duration == 0) {
            // open ended run: the bus stays at this level for the rest of the frame
            bits = remaining;
        } else {
            bits = (int)((duration + bit_us / 2) / bit_us);
            if (bits == 0) {
                return SIEDLE_DECODE_GLITCH;
            }
            if (bits > remaining) {
                bits = remaining;
            }
        }

        for (int b = 0; b < bits; b++) {
            cmd = (cmd << 1) | level;
        }
        remaining -= bits;

        if (remaining > 0 && i == num_runs - 1 && level == 0) {
            return SIEDLE_DECODE_TRUNCATED;
        }
    }

    // trailing bits not covered by any run are idle (high)
    while (remaining-- > 0) {
        cmd = (cmd << 1) | 1u;
    }

    *out_cmd = cmd;
    return SIEDLE_DECODE_OK;
}
