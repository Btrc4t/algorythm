idf_component_register(SRCS "rgb_leds.c"
                    INCLUDE_DIRS "."
                    REQUIRES driver
                    PRIV_REQUIRES spi_flash
                    PRIV_REQUIRES coap_endpoints)
