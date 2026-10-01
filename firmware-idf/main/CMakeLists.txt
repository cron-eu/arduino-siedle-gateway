idf_component_register(
    SRCS "app_main.c" "app_status.c" "reset_button.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES nvs_flash esp_netif esp_event esp_timer esp_driver_gpio esp_app_format
                  cloud devcfg ota siedle_log siedle_proto web_ui wifi_mgr
)
