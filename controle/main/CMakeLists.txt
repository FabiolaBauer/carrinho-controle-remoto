idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES log esp_hw_support nvs_flash freertos
    PRIV_REQUIRES esp_wifi esp_netif esp_event
)
