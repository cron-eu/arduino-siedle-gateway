#ifndef FIRMWARE_IOT_SECRETS_H
#define FIRMWARE_IOT_SECRETS_H

// Fill in the hostname of your AWS IoT broker
#define SECRET_BROKER "xxxxxxxxxxxxxx.iot.xx-xxxx-x.amazonaws.com"

// Fill in the boards public certificate
const char SECRET_CERTIFICATE[] = R"(
-----BEGIN CERTIFICATE-----
-----END CERTIFICATE-----
)";

#if defined(ESP8266) || defined(ESP32)
// we store all certificates locally for this target
#define MQTT_CA_FILE "/ca.der"
#define MQTT_PRIV_FILE "/private.der"
#define MQTT_CERT_FILE "/cert.der"
#define MQTT_DEVICE_NAME "MyDeviceName"
#endif

#endif