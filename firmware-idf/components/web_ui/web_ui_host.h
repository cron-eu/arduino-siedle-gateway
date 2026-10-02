/*
 * Host header check for the setup endpoints, no ESP-IDF dependencies (host tested).
 */
#pragma once

#include <stdbool.h>

/**
 * Whether an HTTP Host header ("192.168.4.1", "siedle.local:80") names the gateway: its IP address, or its hostname
 * with ".local". Letter case and a port don't matter.
 */
bool web_ui_host_is_gateway(const char *host, const char *ip, const char *hostname);
