idf_component_register(
    SRCS "web_ui.c"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES esp_http_server esp_netif siedle_log siedle_proto wifi_mgr
    EMBED_TXTFILES "www/index.html"
)
