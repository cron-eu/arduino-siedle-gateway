idf_component_register(
    SRCS "siedle_log.c"
    INCLUDE_DIRS "include"
    REQUIRES siedle_proto
    PRIV_REQUIRES freertos
)
