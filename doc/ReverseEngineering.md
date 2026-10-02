Reverse Engineering Insights
====

Siedle 1+n protocol
----

Basically this uses different voltages to signalize the events, e.g. "ringing".

* [Offizielles Systemhandbuch](https://www.siedle.de/xs_db/DOKUMENT_DB/www/Systemhandbuch/1+n_2013/Systemhandbuch_1+n-Technik_136441_DE.pdf)
* [Klingelanlage: Siedle "1+n" Protokoll  (DE)](https://www.mikrocontroller.net/topic/264481)
* [Reverse engineering of the Siedle HTA 811-0 W (DE)](https://www.richis-lab.de/Siedle.htm)


Siedle In-Home-Bus Protokoll
----

Newer Siedle devices use a bus protocol called "In-Home-Bus". The bus is a 2-wire bus and uses a supply voltage of 28V. Devices do communicate via monitoring the bus voltage and also pulling down the bus to transmit individual bits.

The cheapest Siedle Device is the BTS 850 (~50 €).

There is a blog post on mikrocontroller.net about [building a WiFi enabled MQTT Gateway](https://www.mikrocontroller.net/topic/308271) based on an Arduino, the C source code also [being available](https://www.mikrocontroller.net/attachment/360089/siedle-client.ino).

### Gyrator Circuit

See the [Eingangskondensator dämpft Audio auf Busleitung](https://www.mikrocontroller.net/topic/343694) form article for details. Our design: [Bus-Power.md](Bus-Power.md).

### Findings from other projects

Collected from the sources below, not yet checked on our own bus (see [Bus-Measurements.md](Bus-Measurements.md)).

**Electrical**

- Ta is +, Tb is −. The bus floats against earth.
- At the power supply, Ta/Tb carry 26–29 V at idle, 28–32 V while ringing and 26–29 V while talking. The farthest
  device sees at least 16 V (system manual).
- Bus power supplies: BNG 650 27.5 V / 500 mA, BVNG 650 29 V / 1.2 A. An indoor station draws 6 mA at idle and 30 mA
  while talking, the door loudspeaker 10 mA and 80 mA (system manual). There is no official budget for add-on
  devices; the community mentions 15 mA up to "a few 10 mA".

**Telegrams**

- 32 bits of 2 ms, MSB first. During a telegram the power supply works as a current source: a 0 bit sits at about
  1.8–2.4 V, a 1 bit at about 7–8 V. So the bus stays below about 8 V for the whole telegram.
- A telegram starts with a 0 bit and ends with a "half 1" bit of about 1 ms. Afterwards the bus rises above its idle
  level for a few ms: that is what the Arduino firmware checks as "acknowledge", it marks the end of the telegram.
  Timing errors of about 5 % make telegrams fail.
- Layout: `010`, 6-bit command, 5-bit destination address, 4-bit destination line, `010`, 5-bit source address,
  4-bit source line, `00`. The 4-bit "signal" in our firmware and in `lambda/siedle.json` is the upper part of the
  6-bit command.
- Published commands: ONAIR `000001` (our signal 0, talk start), ONAIR_OFF `100001` (8, talk end), OPEN_DOOR
  `001100` (3), INCOMING_RING `110001` (12). The door ring in our installation arrives as signal 2, which matches
  none of them, probably another call type.
- Sending, as the Arduino hardware does: 200 Ω across the bus for the whole telegram ("carrier"), plus 10 Ω during
  each 0 bit, with a bit time of about 1980 µs.

**Speech**

- Plain analog audio on the same pair, in both directions: a 500 Hz test tone shows up 1:1 on the DC. Speech levels
  are about 50–200 mV, at most 250–300 mV (forum post, not stated whether peak or peak-to-peak).
- Speech only goes onto the bus after ONAIR. There is one speech channel per installation, whoever answers first
  gets it. Hands-free indoor stations offer a push-to-talk mode.
- Large input capacitors damp the speech for the whole bus (220 µF: the door speaker became "very, very quiet"),
  1–20 µF made telegrams abort after 2–3 bits. Siedle's own phones take their power through a current source
  (LM317, about 17 mA).

**Sources**

- [Systemhandbuch In-Home-Bus Audio 2021 (DE)](https://www.siedle.com/xs_db/DOKUMENT_DB/www/Systemhandbuch/In-Home_Bus_Audio_2021/Systemhandbuch_In-Home_Audio_2021_210011021-00_DE--.pdf): voltages, currents, power supplies
- [Planungs- und Installationsrichtlinien In-Home-Bus 2005 (DE)](https://www.siedle.de/xs_db/DOKUMENT_DB/www/Planungs_und_Installationsrichtlinien/Planung_Installation_189968_In-Home-Bus_2005.pdf)
- [mikrocontroller.net 308271, all pages (DE)](https://www.mikrocontroller.net/topic/308271?page=single): scope traces, the sending recipe, the audio measurement. Schematic and code of that WiFi/MQTT gateway are in [examples/mikrocontroller-net-308271](examples/mikrocontroller-net-308271). Its receive and transmit circuits sit behind the gyrator's input diode, where the MOSFET's body diode can feed the buck converter's input back to them: ours connect directly to the bus.
- [mbs38/siedle-in-home-bus-avr](https://github.com/mbs38/siedle-in-home-bus-avr): AVR library, current source supply
- [angelnu ESPHome PCB](https://github.com/angelnu/esphome/tree/master/devices/pcb-siedle-bus): ESP32, KiCad, powered from the bus, no audio
- [oskarn97/fhem-siedle-mqtt](https://github.com/oskarn97/fhem-siedle-mqtt): FHEM module by the author of the 308271 gateway
- [FWeinb/ha-sg-150 issue 96](https://github.com/FWeinb/ha-sg-150/issues/96): speech channel occupancy
