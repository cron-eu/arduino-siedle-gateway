# Vendored from ESP-IDF v6.1 examples/protocols/http_server/captive_portal (Unlicense OR CC0-1.0).
idf_component_register(
    SRCS "dns_server.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_netif
)
