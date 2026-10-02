/*
 * Checks for the identity settings entered on the setup page. No ESP-IDF dependencies, host tested.
 */
#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define IDENTITY_ENDPOINT_MAX    253
#define IDENTITY_THING_MAX       128
#define IDENTITY_HOSTNAME_MAX    63
#define IDENTITY_AP_PASSWORD_MAX 63

/**
 * Clean up an AWS IoT endpoint as copied from the console or the CLI, in place: surrounding white space, a scheme
 * (https://, mqtts://), a path and the MQTT port :8883 are removed, letters are lowercased.
 */
void identity_endpoint_normalize(char *endpoint);

/** Host name with at least two dot separated labels of letters, digits and hyphens. */
bool identity_endpoint_valid(const char *endpoint);

/** AWS IoT thing name, also the MQTT client id and part of topics: 1-128 letters, digits, '_', '-' and ':'. */
bool identity_thing_valid(const char *thing);

/** DHCP and mDNS hostname: 1-63 letters, digits and hyphens, no hyphen at either end. */
bool identity_hostname_valid(const char *hostname);

/** WPA2 passphrase: 8-63 printable ASCII characters. */
bool identity_ap_password_valid(const char *password);

#ifdef __cplusplus
}
#endif
