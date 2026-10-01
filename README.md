Arduino Doorbell
====

[![CircleCI](https://circleci.com/gh/cron-eu/arduino-siedle-gateway.svg?style=svg)](https://circleci.com/gh/cron-eu/arduino-siedle-gateway)

Abstract
----

This repository holds the documentation and software needed to build a Slack interface to our Siedle Doorbell-System, using an Arduino MKR 1010 WiFi board and some electronics.

The gateway is being migrated to ESP32 / ESP-IDF (`firmware-idf/`), which will also add audio support. Until the new firmware runs in production, the Arduino firmware stays in `firmware/`.


Project Folder Structure
---

* `firmware-idf/**` New firmware for ESP32 / ESP32-S3 boards (ESP-IDF, C), see [firmware-idf/README.md](firmware-idf/README.md)
* `firmware/**` Firmware for the Arduino MKR1010 WiFi Board (C++)
* `lambda/**` AWS Lambda functions (Slack integration)
* `hardware/**` Schematics (Eagle)
* `doc/*` Documentation Files (Markdown)


Author
----

Remus Lazar (rl -at- cron dot eu)
