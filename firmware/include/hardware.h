//
// Created by Remus Lazar on 27.01.20.
//
// Hardware related settings, GPIO Pin numbers etc.
//

#ifndef FIRMWARE_HARDWARE_H
#define FIRMWARE_HARDWARE_H


#ifdef DEVICE_ARDUINO_MKR_1010
#define SIEDLE_A_IN A1
#define SIEDLE_TX_PIN 0
#define SIEDLE_TX_CARRIER_PIN 3

#elif defined(ARDUINO_SAMD_NANO_33_IOT)
#define SIEDLE_TX_PIN 2
#define SIEDLE_TX_CARRIER_PIN 3
#define SIEDLE_A_IN A1

#elif defined(ARDUINO_ESP8266_WEMOS_D1MINI)
#define SIEDLE_A_IN A0
#define SIEDLE_TX_PIN D5
#define SIEDLE_TX_CARRIER_PIN D6
#endif

#endif //FIRMWARE_HARDWARE_H
