idf_component_register(
    SRCS "app_main.c" "reset_button.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES nvs_flash esp_netif esp_event esp_driver_gpio esp_app_format
                  wifi_mgr
)
