idf_component_register(SRCS "test_libsnowflakeid_esp.c"
        REQUIRES test_utils
        INCLUDE_DIRS "."
        PRIV_REQUIRES libsnowflakeid-esp unity)
