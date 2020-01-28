//
// Created by Remus Lazar on 28.01.20.
//

#include "Timer5.h"
#include <Arduino.h>

void (*_handler)() = NULL;

void __unused TC5_Handler (void) {
    _handler();
    TC5->COUNT16.INTFLAG.bit.MC0 = 1; //Writing a 1 to INTFLAG.bit.MC0 clears the interrupt so that it will run again
}

void Timer5::configure(int sampleRate) {
    // Enable GCLK for TCC2 and TC5 (timer counter input clock)
    GCLK->CLKCTRL.reg = (uint16_t) (GCLK_CLKCTRL_CLKEN | GCLK_CLKCTRL_GEN_GCLK0 | GCLK_CLKCTRL_ID(GCM_TC4_TC5));
    while (GCLK->STATUS.bit.SYNCBUSY);

    reset(); //reset TC5

    // Set Timer counter Mode to 16 bits
    TC5->COUNT16.CTRLA.reg |= TC_CTRLA_MODE_COUNT16;
    // Set TC5 mode as match frequency
    TC5->COUNT16.CTRLA.reg |= TC_CTRLA_WAVEGEN_MFRQ;
    // Set prescaler (/16)
    TC5->COUNT16.CTRLA.reg |= TCC_CTRLA_PRESCALER_DIV16;

    // Timer Frequency := SystemCoreClock / (16 * (CC0 + 1))
    // Timer Rate := (16 * (CC0 + 1)) / SystemCoreClock
    // CC0 := (Timer Rate * SystemCoreClock) / 16 - 1

    // CC0 := (timer_rate_in_us / 1_000_000) * 48_000_000 / 16 - 1

    TC5->COUNT16.CC[0].reg = (uint16_t) ( (SystemCoreClock / 16000000) * sampleRate - 1);
    while (isSyncing());

    // Configure interrupt request
    NVIC_DisableIRQ(TC5_IRQn);
    NVIC_ClearPendingIRQ(TC5_IRQn);
    NVIC_SetPriority(TC5_IRQn, 0);
    NVIC_EnableIRQ(TC5_IRQn);

    // Enable the TC5 interrupt request
    TC5->COUNT16.INTENSET.bit.MC0 = 1;
    while (isSyncing()); //wait until TC5 is done syncing
}

void Timer5::onFire(void (*callback)(void)) {
    _handler = callback;
}

void Timer5::changeSampleRate(int sampleRate) {
    TC5->COUNT16.CC[0].reg = (uint16_t) ( (SystemCoreClock / 16000000) * sampleRate - 1);
}

inline bool Timer5::isSyncing() {
    return TC5->COUNT16.STATUS.reg & TC_STATUS_SYNCBUSY;
}

//This function enables TC5 and waits for it to be ready
void Timer5::enable() {
    TC5->COUNT16.COUNT.reg = 0x0;
    TC5->COUNT16.CTRLA.reg |= TC_CTRLA_ENABLE; //set the CTRLA register
    while (isSyncing()); //wait until snyc'd
    TC5->COUNT16.INTENSET.bit.MC0 = 1;
}
// Reset TC5
void Timer5::reset() {
    TC5->COUNT16.CTRLA.reg = TC_CTRLA_SWRST;
    while (isSyncing());
    while (TC5->COUNT16.CTRLA.bit.SWRST);
}
//disable TC5
void Timer5::disable() {
    TC5->COUNT16.CTRLA.reg &= ~TC_CTRLA_ENABLE;
    while (isSyncing());
}
