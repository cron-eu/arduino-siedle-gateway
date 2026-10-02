idf_component_register(
    SRCS "web_ui.c" "web_ui_admin.c"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES cloud esp_http_server esp_netif identity mbedtls siedle_log siedle_proto wifi_mgr
    EMBED_TXTFILES "www/index.html"
)
