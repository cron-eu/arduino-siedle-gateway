Siedle Gateway Firmware (ESP-IDF)
====

Firmware for ESP32 / ESP32-S3 based gateways between the Siedle In-Home bus and AWS IoT. It replaces the Arduino
firmware in [`../firmware`](../firmware), keeping its MQTT topics so the Lambda functions keep working.

**Status:** phase 1. Connectivity is in place: Wi-Fi and AWS IoT identity set up on a captive portal, web UI, OTA
updates.
The bus driver (phase 2) and audio come next, see [Roadmap](#roadmap).

Hardware
----

Any ESP32 or ESP32-S3 board with at least 4 MB flash. The BOOT button (GPIO0) doubles as the setup button.

Development setup
----

ESP-IDF **v6.1**, installed with the [ESP-IDF Installation Manager](https://docs.espressif.com/projects/idf-im-ui/)
(EIM).

### VS Code

1. Install the recommended extension [ESP-IDF](https://marketplace.visualstudio.com/items?itemName=espressif.esp-idf-extension).
2. Open **this folder** (`firmware-idf`, not the repository root) and select the ESP-IDF v6.1 setup when asked.
3. Pick the target (`esp32` or `esp32s3`) and the serial port in the status bar, then use *Build*, *Flash* and
   *Monitor*.

### Command line

```bash
source ~/.espressif/tools/activate_idf_v6.1.sh

idf.py set-target esp32                        # once, or esp32s3
idf.py build
idf.py -p /dev/cu.usbserial-XXXX flash monitor
```

`sdkconfig` is generated from [`sdkconfig.defaults`](sdkconfig.defaults) and not committed. The project options
are in `idf.py menuconfig` under *Siedle Gateway*, *Wi-Fi manager*, *Cloud (AWS IoT)*, *OTA updates* and
*Siedle event log*.

Setting up a device
----

### Wi-Fi

A device without Wi-Fi credentials opens the hotspot **`Siedle-Setup-XXXX`**. Join it with a phone or laptop and
the setup page pops up (otherwise open `http://192.168.4.1`). Pick the network and enter the password. The page
then shows the new address, `http://siedle.local/`. Credentials are only stored after the connection succeeded.

- **Changing the network:** hold the BOOT button for 3 seconds and release it. The device opens the hotspot and
  stays on its current network until you pick another one. The hotspot closes after 10 minutes without clients.
- **Forgetting the network:** hold the BOOT button for 10 seconds. The device forgets the network and the hotspot
  password, and opens the hotspot.
- **Fallback:** if the configured network is unreachable for 5 minutes, the hotspot opens automatically. The
  device keeps retrying in the background and closes the hotspot once it is back online.
- **Security:** set a hotspot password on the setup page (**Gateway** card) to protect the hotspot with WPA2. On
  the regular network the web UI is read-only, so changes always need physical access to the hotspot or the
  button. The setup endpoints only answer requests that come in through the hotspot.

### Cloud

The device creates its own key on first boot. Its AWS IoT certificate, endpoint and thing name are set up on the
setup page (**Cloud** card) with the AWS IoT console, see [`docs/aws-iot.md`](docs/aws-iot.md). Since they control
the door, they can only be changed on a hotspot opened with the BOOT button or on a device without Wi-Fi, not on
the one that opens by itself after 5 minutes offline. The device connects once its clock is synchronized via NTP,
because certificate validity is checked.

The identity and the setup page settings are kept in the `identity` namespace of the `nvs` partition, so all
devices run the same firmware image and OTA updates keep them.

MQTT topics
----

| Topic                        | Direction | Payload                                     |
|------------------------------|-----------|---------------------------------------------|
| `siedle/received`            | publish   | `{"ts":1691240767,"cmd":1181356688}`        |
| `siedle/sent`                | publish   | same, after the bus master acknowledged     |
| `siedle/send`                | subscribe | `1181356688` (decimal command)              |
| `siedle/<client_id>/status`  | publish   | retained status JSON, `{"online":false}` as last will |
| `siedle/<client_id>/ota`     | subscribe | `{"url":"https://…/siedle-gateway.bin"}`    |

The first three are unchanged from the Arduino firmware. `siedle/send` is shared by all gateways. Until the old
gateway is retired, test a second gateway with a different topic prefix (*Cloud (AWS IoT)* → *MQTT topic prefix*),
so a command isn't put on the bus twice.

Firmware updates
----

Upload `build/siedle-gateway.bin` to any HTTPS location (e.g. a presigned S3 URL) and trigger the update:

```bash
aws iot-data publish --topic siedle/SiedleGateway/ota --cli-binary-format raw-in-base64-out \
  --payload '{"url":"https://example-bucket.s3.eu-central-1.amazonaws.com/siedle-gateway.bin?X-Amz-..."}'
```

The device checks the server certificate and that the image belongs to this project, installs it into the
inactive slot and reboots. The new firmware has to reach the cloud within 10 minutes. Otherwise the bootloader
switches back to the previous one.

Development
----

```bash
# host unit tests (protocol encoding/decoding cross-checked against lambda/siedle-lib.js, setup page input checks)
cmake -S test/host -B build-host -G Ninja && cmake --build build-host && ctest --test-dir build-host

# work on the web UI without hardware, with a simulated device API
python3 tools/mock_server.py --portal          # http://localhost:8080, a new device on its hotspot
python3 tools/mock_server.py --portal manual   # hotspot opened with the BOOT button
python3 tools/mock_server.py --portal fallback # hotspot opened by itself, cloud settings locked
```

To swap certificates on a development board without the BOOT button and the hotspot, build with
[`sdkconfig.dev`](sdkconfig.dev). It unlocks the cloud and gateway settings on the regular network
(*Web UI* → *Allow changing the cloud and gateway settings from the network*):

```bash
idf.py -B build-dev -D SDKCONFIG=build-dev/sdkconfig -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.dev" \
  -p /dev/cu.usbserial-XXXX flash monitor
```

Never install such a build: anyone on its network could point it at their own AWS account and open the door. It
warns at boot, shows a banner on the setup page and reports `"settings_from_network": true` in its status. The
option is off in `sdkconfig.defaults`, which release builds use, and not available with secure boot or flash
encryption.

After a crash, `idf.py coredump-info` reads the core dump stored in flash.

Layout
----

| Path                       | Purpose                                                                  |
|----------------------------|--------------------------------------------------------------------------|
| `main/`                    | boot sequence, status JSON, setup button                                 |
| `components/siedle_proto/` | frame format, encode/decode (no ESP-IDF dependencies, host tested)       |
| `components/siedle_log/`   | ring buffer of recent bus events                                         |
| `components/wifi_mgr/`     | station management, setup hotspot, captive portal DNS                    |
| `components/web_ui/`       | HTTP server, JSON API, `www/index.html`                                  |
| `components/cloud/`        | AWS IoT MQTT client                                                      |
| `components/ota/`          | HTTPS updates with rollback                                              |
| `components/identity/`     | key, CSR, certificate and settings in NVS, input checks host tested      |
| `components/dns_server/`   | captive portal DNS, vendored from the ESP-IDF examples (CC0)             |
| `docs/`                    | connecting a gateway to AWS IoT                                          |
| `test/host/`               | unit tests that run on the development machine                           |
| `tools/`                   | development helpers                                                      |

Roadmap
----

1. ~~Connectivity: Wi-Fi setup, AWS IoT, web UI, OTA~~
2. Bus driver: RMT based receive/transmit through a comparator front end, run in parallel with the old gateway
   until the logs match. See [`../doc/Bus-Measurements.md`](../doc/Bus-Measurements.md) for the measurements
   this depends on.
3. Bus power and audio hardware: gyrator power stage ([`../doc/Bus-Power.md`](../doc/Bus-Power.md)), I2S codec
   (ES8311) coupled to the bus
4. Calls from a web page behind the office proxy: answer the door, push-to-talk, open the door
   ([`../doc/Audio.md`](../doc/Audio.md))
5. Hardening: secure boot, flash and NVS encryption, custom PCB
