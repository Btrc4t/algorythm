idf_component_register(SRCS "coap_endpoints.c"
                    INCLUDE_DIRS "."
                    REQUIRES coap
                    PRIV_REQUIRES mdns
                    PRIV_REQUIRES rgb_leds
                    PRIV_REQUIRES log
                    REQUIRES freertos)

