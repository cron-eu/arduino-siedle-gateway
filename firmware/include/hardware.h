//
// Created by Remus Lazar on 27.01.20.
//
// Hardware related settings, GPIO Pin numbers etc.
//

#ifndef FIRMWARE_HARDWARE_H
#define FIRMWARE_HARDWARE_H

#define SIEDLE_A_IN A1
#ifdef DEVICE_ARDUINO_MKR_1010
#define SIEDLE_TX_PIN 0
#else
#define SIEDLE_TX_PIN 2
#endif
#define SIEDLE_TX_CARRIER_PIN 3

#endif //FIRMWARE_HARDWARE_H
