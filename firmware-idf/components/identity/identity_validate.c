#include "identity_validate.h"

#include <string.h>

static bool is_alnum(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
}

// DNS label: 1-63 letters, digits and hyphens, no hyphen at either end
static bool label_valid(const char *label, size_t len)
{
    if (len == 0 || len > 63 || label[0] == '-' || label[len - 1] == '-') {
        return false;
    }
    for (size_t i = 0; i < len; i++) {
        if (!is_alnum(label[i]) && label[i] != '-') {
            return false;
        }
    }
    return true;
}

void identity_endpoint_normalize(char *endpoint)
{
    const char *start = endpoint + strspn(endpoint, " \t\r\n");
    const char *scheme = strstr(start, "://");
    if (scheme) {
        start = scheme + 3;
    }
    memmove(endpoint, start, strlen(start) + 1);
    endpoint[strcspn(endpoint, "/ \t\r\n")] = '\0';

    // the firmware always connects to the MQTT port
    size_t len = strlen(endpoint);
    if (len > 5 && strcmp(endpoint + len - 5, ":8883") == 0) {
        endpoint[len - 5] = '\0';
    }
    for (char *p = endpoint; *p; p++) {
        if (*p >= 'A' && *p <= 'Z') {
            *p += 'a' - 'A';
        }
    }
}

bool identity_endpoint_valid(const char *endpoint)
{
    if (endpoint == NULL || strlen(endpoint) > IDENTITY_ENDPOINT_MAX || strchr(endpoint, '.') == NULL) {
        return false;
    }
    for (const char *label = endpoint;;) {
        size_t len = strcspn(label, ".");
        if (!label_valid(label, len)) {
            return false;
        }
        if (label[len] == '\0') {
            return true;
        }
        label += len + 1;
    }
}

bool identity_thing_valid(const char *thing)
{
    if (thing == NULL) {
        return false;
    }
    size_t len = strlen(thing);
    if (len == 0 || len > IDENTITY_THING_MAX) {
        return false;
    }
    for (size_t i = 0; i < len; i++) {
        if (!is_alnum(thing[i]) && strchr("_-:", thing[i]) == NULL) {
            return false;
        }
    }
    return true;
}

bool identity_hostname_valid(const char *hostname)
{
    return hostname != NULL && label_valid(hostname, strlen(hostname));
}

bool identity_ap_password_valid(const char *password)
{
    if (password == NULL) {
        return false;
    }
    size_t len = strlen(password);
    if (len < 8 || len > IDENTITY_AP_PASSWORD_MAX) {
        return false;
    }
    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)password[i];
        if (c < 0x20 || c > 0x7e) {
            return false;
        }
    }
    return true;
}
