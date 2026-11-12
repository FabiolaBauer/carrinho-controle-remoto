/**
 * @file main.c
 * @brief Esqueleto do firmware do Controle (ESP32 #1) — Fase 1
 *
 * Na Fase 1, o controle apenas inicializa o Wi-Fi, lê e loga o próprio MAC
 * e aguarda indefinidamente. A lógica completa (joystick, ESP-NOW, telemetria)
 * será implementada na Fase 2.
 *
 * Requisito: RNF05 (APIs validadas via documentação oficial).
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "nvs_flash.h"

static const char *TAG = "ctrl_main";

/**
 * @brief Inicializa Wi-Fi em modo STA (somente para leitura de MAC e futura
 *        integração com ESP-NOW).
 */
static void wifi_init_sta(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
}

void app_main(void)
{
    ESP_LOGI(TAG, "=== Inicializando Firmware do Controle (ESP-IDF v6.1) ===");

    /* Inicialização do NVS (necessário para Wi-Fi) */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    /* Inicializa Wi-Fi para obter o MAC */
    wifi_init_sta();

    /* Lê e loga o MAC desta placa (ESP32 #1 — Controle) */
    uint8_t my_mac[6];
    ESP_ERROR_CHECK(esp_wifi_get_mac(WIFI_IF_STA, my_mac));
    ESP_LOGI(TAG, "MAC do Controle: " MACSTR, MAC2STR(my_mac));
    ESP_LOGI(TAG, ">> Configure CONTROLLER_MAC em components/logic/include/config.h com este valor <<");

    /* Fase 1: aguarda indefinidamente.
     * A implementação completa do controle (Fase 2) irá aqui. */
    ESP_LOGI(TAG, "Fase 1 — esqueleto ativo. Aguardando Fase 2...");
    vTaskDelay(portMAX_DELAY);
}
