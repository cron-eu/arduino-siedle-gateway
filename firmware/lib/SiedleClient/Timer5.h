//
// Created by Remus Lazar on 28.01.20.
//

#ifndef FIRMWARE_TIMER5_H
#define FIRMWARE_TIMER5_H

#include <Arduino.h>

class Timer5 {
public:
    // Configure the timer to run at sample rate (in microseconds).
    // Note: the timer is not enabled by default. You need to call enable() manually
    static void configure(int sampleRate);

    static void onFire(void(*callback)(void));

    // Change the sample rate on the fly (in microseconds)
    static void changeSampleRate(int sampleRate);

    // Handler to be called when a timer fires
    static void (*handler)();

    // This function enables TC5 and waits for it to be ready. This also resets the timer counter register.
    static void enable();

    // Disable TC5
    static void disable();

private:
    // Reset TC5 (resets all registers to their default value)
    static void reset();

    static bool isSyncing();

};

#endif //FIRMWARE_TIMER5_H
