idf_component_register(
        SRCS "libsnowflakeid_esp.c"
        INCLUDE_DIRS "pub_includes"
        PRIV_INCLUDE_DIRS "priv_includes"
        PRIV_REQUIRES esp_timer)
