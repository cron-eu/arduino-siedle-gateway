Documentation
====

The ESP-IDF firmware for ESP32 is replacing the Arduino firmware, and audio support (answering the door from a web
page) is being designed. Decisions, plan and open questions: [Audio.md](Audio.md). The firmware itself:
[firmware-idf/README.md](../firmware-idf/README.md).

ESP32 gateway
----

| Document | Contents |
|---|---|
| [Audio.md](Audio.md) | Answering the door from a web page: how the bus carries speech, architecture, hardware and firmware design, decisions, plan, open questions |
| [Bus-Power.md](Bus-Power.md) | Powering the gateway from the bus without damping the speech: gyrator schematic, parts list, simulation, breadboard bring-up |
| [PCB.md](PCB.md) | The interface board: revisions, what is settled, schematic outline, mechanics for the case, assembly and cost |
| [Bus-Measurements.md](Bus-Measurements.md) | Checklist for measuring the bus with the oscilloscope |
| [ReverseEngineering.md](ReverseEngineering.md) | Siedle 1+n and In-Home bus: protocol, levels and findings from other projects, with links |
| [firmware-idf/README.md](../firmware-idf/README.md) | Firmware: development setup, device setup, MQTT topics, OTA updates |
| [firmware-idf/docs/aws-iot.md](../firmware-idf/docs/aws-iot.md) | Setting up a gateway's AWS IoT identity on its setup page, with the AWS IoT console |

Arduino firmware (being replaced)
----

| Document | Contents |
|---|---|
| [Hardware.md](Hardware.md) | The Arduino MKR WiFi 1010 and Nano 33 IoT boards |
| [Setup.md](Setup.md) | PlatformIO and CLion setup |
| [Software.md](Software.md) | MQTT and SSL libraries |

Other material
----

- [examples/mikrocontroller-net-308271](examples/mikrocontroller-net-308271): schematic and code of the WiFi/MQTT gateway from mikrocontroller.net
- [images](images): figures used by these documents
- [hardware/bus-power/gyrator.cir](../hardware/bus-power/gyrator.cir): ngspice simulation of the power stage
- [hardware/interface-pcb](../hardware/interface-pcb): KiCad project of the interface board (schematic draft)
- [hardware/arduino-doorbell](../hardware/arduino-doorbell): Eagle schematics of the Arduino prototype
