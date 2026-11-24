idf_component_register(
    SRCS "main.c"
    PRIV_REQUIRES
        esp_adc
        esp_http_server
        esp_event
        esp_netif
        freertos
        nvs_flash
        protocol_examples_common
)