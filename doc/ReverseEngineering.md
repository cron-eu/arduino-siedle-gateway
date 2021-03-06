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

See the [Eingangskondensator dämpft Audio auf Busleitung](https://www.mikrocontroller.net/topic/343694) form article for details.
