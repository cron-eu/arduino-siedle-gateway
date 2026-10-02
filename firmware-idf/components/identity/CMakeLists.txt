idf_component_register(
    SRCS "identity.c" "identity_validate.c"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES esp_timer mbedtls nvs_flash
)
