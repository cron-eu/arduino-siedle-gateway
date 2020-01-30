Arduino Doorbell Firmware
====

Abstract
----

Firmware related documentation, how to build and configure the Hardware for operation.

#### Development Setup

Instructions how to setup the whole project (install platformio, CLion related stuff) can be found [here](../doc/Setup.md).

Device Setup and Configuration
----

An Overview about how the software and hardware works and guide to setup and configure the hardware.

Technical Overview
---

### Receive

The Arduino uses a simple hardware for the power supply and also for converting the Siedle Bus voltage to a range compatible with the SAMD21 micro (which uses 3.3V supply).

Using an ISR (interrupt service routine) the Arduino Firmware listens on the Siedle bus and captures all incoming messages, posting them using MQTT.

The messaged posted are simple JSON messages like:

```json
{
 "ts": 1580122760,
 "cmd": 1151144720
}
``` 

`ts` being the unix timestamp (UTC, synchronized via NTP), the `cmd` being the command itself - which is a 32 Bit unsigned integer.

The communication with the AWS MQTT Broker is encrypted with an X.509 certificate (see below).

### Transmit

Currently not implemented, WIP.

### Crypto Chip, X.509 Certificate

To use X.509 certificates for authentication it is required that the ECC608A crypto element is configured and locked.

#### Generate the Private Key

Note: Since the private key is generated inside the crypto element it never leaves the device and is stored securely and cannot be read.

See [Configuring and Adding the Board to AWS IoT Core](https://create.arduino.cc/projecthub/Arduino_Genuino/securely-connecting-an-arduino-mkr-wifi-1010-to-aws-iot-core-a9f365#toc-configuring-and-adding-the-board-to-aws-iot-core-2) for how to do this.

In a nutshell:
* Open the Arduino IDE
* Install the `ArduinoECCX08` Library and dependencies
* Select the MKR WiFi 1010 Board from the menu
* RUN File -> Examples -> ArduinoECCX08 -> Tools -> ECCX08CSR
* Use the Name "ArduinoDoorbell" for the "Common Name", all other fields can be left blank
* Copy the generated CSR text including "-----BEGIN CERTIFICATE REQUEST-----" and "-----END CERTIFICATE REQUEST-----" and save it locally.

#### Existing Devices

* Arduino MKR WIFI 1010
  * Common Name: "ArduinoDoorbell"
* Arduino nano 33
  * Common Name: "ArduinoDoorbellNano"

"Every Day" Actions
----

### Build and Upload

There are two targets in the Makefile for that: `make build` and `make upload`.




