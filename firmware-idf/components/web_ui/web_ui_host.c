#include "web_ui_host.h"

#include <string.h>
#include <strings.h>

#define LOCAL ".local"

bool web_ui_host_is_gateway(const char *host, const char *ip, const char *hostname)
{
    if (host == NULL) {
        return false;
    }
    size_t len = strcspn(host, ":"); // without the port
    if (ip && len == strlen(ip) && strncmp(host, ip, len) == 0) {
        return true;
    }
    size_t name_len = hostname ? strlen(hostname) : 0;
    return name_len > 0 && len == name_len + strlen(LOCAL) && strncasecmp(host, hostname, name_len) == 0
           && strncasecmp(host + name_len, LOCAL, strlen(LOCAL)) == 0;
}
