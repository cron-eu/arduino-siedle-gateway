idf_component_register(
    SRCS "cloud.c"
    INCLUDE_DIRS "include"
    REQUIRES siedle_proto
    PRIV_REQUIRES esp_timer mbedtls
)
