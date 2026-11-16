idf_component_register(SRCS "main.c" "joystick_adc.c" "radio.c" "ui.c" "t4.c"
                       INCLUDE_DIRS "."
                       REQUIRES logic protocol
                       PRIV_REQUIRES esp_wifi esp_netif esp_event nvs_flash esp_adc
                                     esp_driver_gpio esp_timer esp_hw_support freertos log)
# esp_now.h vem do componente esp_wifi (esp_wifi/include/esp_now.h, v6.1).
# esp_mac.h (MACSTR/MAC2STR) vem do esp_hw_support.
