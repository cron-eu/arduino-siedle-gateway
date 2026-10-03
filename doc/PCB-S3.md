Interface PCB, ESP32-S3 variant
====

The board to order: the interface board of [PCB.md](PCB.md) with an ESP32-S3-WROOM-1-N16R8 module soldered onto it
instead of the plug-in D1 Mini. The D1 Mini variant in [hardware/interface-pcb](../hardware/interface-pcb) stays for
development; this variant lives in [hardware/interface-pcb-s3](../hardware/interface-pcb-s3).

**Status:** schematic done, ERC without violations. The layout is still the D1 Mini's: it needs the module, the
USB-C connector and the new parts (see [Next](#next)). The firmware still targets the classic ESP32.

Why the S3, and one board
----

- **One board instead of two revisions.** The plan was a D1 Mini carrier first and a module board later. Going to the
  module now saves a whole board iteration. The module is the least risky part of the design: its circuit follows
  Espressif's reference. The risky parts are the analog values, and the board absorbs those (0805 parts, solder
  jumpers, unfitted options).
- **Audio headroom.** 8 MB PSRAM and the S3's vector instructions run ESP-SR's echo cancellation, which full duplex
  needs. The classic ESP32 on the D1 Mini has no PSRAM.
- **Availability.** An Espressif module from JLCPCB's stock (C2913202) instead of a dev board from one vendor.
- **Less power for the same work.** U1 delivers 3.3 V directly; the D1 Mini wasted about a third of its input in a
  5 V → 3.3 V linear regulator. In modem sleep the S3 draws about as much as the classic ESP32 (28–42 mA at 160 MHz,
  datasheet; more with PSRAM active), so automatic light sleep in the firmware remains the big lever.

What changed against the D1 Mini variant
----

Everything on the bus side, the data sheet and the audio sheet stays as it is, apart from the notes naming GPIOs.

**3.3 V only.** There is no 5 V rail anymore.

| | D1 Mini variant | S3 variant |
|---|---|---|
| U1 | R-78CK5.0-0.5 → 5 V → D1 Mini's linear regulator → 3.3 V | R-78CK3.3-0.5 → 3.3 V for everything |
| MCP6002 (audio send) | 5 V | 3.3 V: the current sink needs at most about 2.4 V at the base |
| JP1, D2 | cut the bus supply, block USB from feeding back into U1 | JP1 removed; D2 now feeds J2 into VOUT |
| J2 (not fitted) | external 5 V on the 5 V rail | external 9–24 V DC into VOUT through D2, ahead of U1 |
| USB | powers the D1 Mini | data only, VBUS does not power the board |

- **J2:** an isolated adapter only, since board ground is the bus minus (Tb). Above about 20 V on VOUT the gyrator
  stops delivering by itself, so no jumper is needed to separate the two supplies. Stay at or below 24 V for C2/C3.
- **Bench supply:** power the board through J1 from the bench supply, as in the bring-up in
  [Bus-Power.md](Bus-Power.md#bring-up), or through J2. USB alone does not power it.

**Controller sheet** (the root sheet):

- **U5:** ESP32-S3-WROOM-1-N16R8 (16 MB flash, 8 MB octal PSRAM), with C51 (22 µF) and C52 (100 nF) at its 3V3 pin.
- **Reset:** R52 (10 k) and C53 (1 µF) delay EN at power-on, as in Espressif's reference; SW2 resets.
- **SW1 moves to GPIO0,** the S3's BOOT pin. A short press opens the setup hotspot, as the firmware already does with
  the BOOT button; held during a reset it starts download mode. The D1 Mini had no BOOT button.
- **USB-C (J3)** to the S3's native USB on GPIO19/20, for flashing and logs. U6 (USBLC6-2SC6) protects D+/D−,
  R53/R54 (5.1 k on CC1/CC2) make the board a USB device, so C-to-C cables work too. On the bus, use the USB isolator:
  board ground is the bus minus.
- **TP15/TP16:** UART0 (TXD0/RXD0) for logs during bring-up, should USB not work.

Pin plan
----

| Function | GPIO | Why this pin |
|---|---|---|
| Setup button SW1 (BOOT) | 0 | the BOOT strapping pin: setup hotspot and download mode in one button |
| Data in (DATA_IN) | 4 | RMT receive, any pin |
| Acknowledge (ACK_IN) | 5 | |
| Data out, carrier (TX_CARRIER) | 6 | |
| VOUT monitor (VMON) | 7 | ADC1: works while Wi-Fi runs |
| Codec I2C (SDA, SCL) | 8, 9 | |
| Codec MCLK (optional, R31) | 10 | the S3 routes MCLK to any pin |
| Codec I2S (BCLK, WS, DOUT, DIN) | 11, 12, 13, 14 | |
| Send stage enable (TX_EN) | 15 | |
| Data out, 0 bits (TX_BIT) | 16 | |
| Status LED | 38 | next to the button on the module's right side |
| USB D−, D+ | 19, 20 | the S3's native USB |
| UART0 TXD0, RXD0 | 43, 44 | test points only |

The bus, data and supply pins sit on the module's left side, facing the bus circuits; the I2S and I2C pins on its
bottom side, facing the codec; the button, the LED and the UART test points on its right side, along the board
edge.

Unused on purpose: GPIO3, 45 and 46 (strapping pins), GPIO35–37 (taken by the octal PSRAM). Free for later: GPIO1,
2, 17, 18, 21, 39–42, 47, 48.

Mechanics
----

- **Antenna:** the module's antenna end at a board edge, with copper and parts kept clear on both layers, as for the
  D1 Mini.
- **USB-C at a board edge,** pointing outwards, so the case can have an opening and the board can be flashed and
  read without opening the case.
- **Flatter:** no D1 Mini on sockets (about 15 mm). The tallest parts are the lying capacitors C2/C3 at 13 mm
  ([Bus-Power.md](Bus-Power.md#c2-and-c3-at-35-v)).
- **Buttons:** SW1 (setup/BOOT) reachable through the case; SW2 (reset) may sit inside.

Cost
----

Against the D1 Mini variant in [PCB.md](PCB.md#assembly): JLCPCB places three more Extended parts (the module, the
USB-C connector, the ESD protection), about $9 in loading fees plus about $5 for each module. In return the D1 Mini
and its two 2 × 10 sockets drop out, and with them part of the hand soldering. Per order the two variants cost about
the same.

Next
----

1. Review the schematic.
2. Layout: place U5 with its antenna at an edge, J3 at an edge reachable through the case, the new parts; remove the
   D1 Mini sockets and MK1. Then DRC with schematic parity (today it reports the layout as out of date, which is
   expected).
3. Firmware: `idf.py set-target esp32s3`, the pin plan above, PSRAM, the console on USB, and the partition table for
   16 MB. Develop on an ESP32-S3-DevKitC-1 (N16R8) on the desk until the board exists.
