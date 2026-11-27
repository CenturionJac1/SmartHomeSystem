idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "../include"
    PRIV_REQUIRES
        driver
        esp_event
        esp_http_server
        esp_netif
        esp_timer
        freertos
        nvs_flash
        protocol_examples_common
)