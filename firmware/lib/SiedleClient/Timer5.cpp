//
// Created by Remus Lazar on 28.01.20.
//

#include "Timer5.h"
#include <Arduino.h>

void (*_handler)() = NULL;

#ifdef ARDUINO_ARCH_SAMD
void __unused TC5_Handler (void) {
    _handler();
    TC5->COUNT16.INTFLAG.bit.MC0 = 1; //Writing a 1 to INTFLAG.bit.MC0 clears the interrupt so that it will run again
}
#elif defined(ARDUINO_ARCH_ESP8266)
void ICACHE_RAM_ATTR onTimerISR(){
    _handler();
}
#endif

void Timer5::configure(int sampleRate) {
    #ifdef ARDUINO_ARCH_SAMD
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

    #elif defined(ARDUINO_ARCH_ESP8266)
    timer1_attachInterrupt(onTimerISR);

    enable();
    changeSampleRate(sampleRate);
    #endif
}

void Timer5::onFire(void (*callback)(void)) {
    _handler = callback;
}

void Timer5::changeSampleRate(int sampleRate) {
    #ifdef ARDUINO_ARCH_SAMD
    TC5->COUNT16.CC[0].reg = (uint16_t) ( (SystemCoreClock / 16000000) * sampleRate - 1);
    #elif defined(ARDUINO_ARCH_ESP8266)
    // timer speed (Hz) = Timer clock speed (Mhz) / prescaler
    // => sample rate = prescaler / Timer Clock Speed
    timer1_write((CPU_CLK_FREQ / 16000000) * sampleRate);
    #endif
}

inline bool Timer5::isSyncing() {
    #ifdef ARDUINO_ARCH_SAMD
    return TC5->COUNT16.STATUS.reg & TC_STATUS_SYNCBUSY;
    #elif defined(ARDUINO_ARCH_ESP8266)
    return false;
    #endif
}

//This function enables TC5 and waits for it to be ready
void Timer5::enable() {
    #ifdef ARDUINO_ARCH_SAMD
    TC5->COUNT16.COUNT.reg = 0x0;
    TC5->COUNT16.CTRLA.reg |= TC_CTRLA_ENABLE; //set the CTRLA register
    while (isSyncing()); //wait until snyc'd
    TC5->COUNT16.INTENSET.bit.MC0 = 1;
    #elif defined(ARDUINO_ARCH_ESP8266)
    /* Dividers:
        TIM_DIV1 = 0,   //80MHz (80 ticks/us - 104857.588 us max)
        TIM_DIV16 = 1,  //5MHz (5 ticks/us - 1677721.4 us max)
        TIM_DIV256 = 3  //312.5Khz (1 tick = 3.2us - 26843542.4 us max)
       Reloads:
        TIM_SINGLE	0 //on interrupt routine you need to write a new value to start the timer again
        TIM_LOOP	1 //on interrupt the counter will start with the same value again
    */
    // set prescaler to /16
    timer1_enable(TIM_DIV16, TIM_EDGE, TIM_LOOP);
    #endif
}
// Reset TC5
void Timer5::reset() {
    #ifdef ARDUINO_ARCH_SAMD
    TC5->COUNT16.CTRLA.reg = TC_CTRLA_SWRST;
    while (isSyncing());
    while (TC5->COUNT16.CTRLA.bit.SWRST);
    #elif defined(ARDUINO_ARCH_ESP8266)
    disable();
    enable();
    #endif
}
//disable TC5
void Timer5::disable() {
    #ifdef ARDUINO_ARCH_SAMD
    TC5->COUNT16.CTRLA.reg &= ~TC_CTRLA_ENABLE;
    while (isSyncing());
    #elif defined(ARDUINO_ARCH_ESP8266)
    timer1_disable();
    #endif
}
