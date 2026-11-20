idf_component_register(SRCS "main.c" "motors.c" "sonar.c" "battery.c" "bench.c" "buzzer.c"
                       INCLUDE_DIRS "."
                       REQUIRES logic protocol
                       PRIV_REQUIRES esp_wifi esp_netif esp_event nvs_flash esp_adc
                                     esp_driver_ledc esp_driver_gpio esp_timer freertos log esp_hw_support)
