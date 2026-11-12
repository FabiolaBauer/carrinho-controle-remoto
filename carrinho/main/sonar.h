/**
 * @file sonar.h
 * @brief Driver do sensor ultrassônico HC-SR04 por interrupção de GPIO e esp_timer
 *
 * Atende ao RF04, RNF02, RNF05 (sem uso de esp_rom_delay_us).
 */

#ifndef SONAR_H
#define SONAR_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint16_t distance_cm;  /**< Distância medida em cm (0xFFFF se sem eco) */
    bool     echo_ok;      /**< Indica se houve eco válido */
    uint32_t timestamp_ms; /**< Timestamp da leitura em ms */
} sonar_reading_t;

/**
 * @brief Inicializa o driver do HC-SR04 e inicia a tarefa de amostragem
 *
 * @param out_queue Fila FreeRTOS de tamanho 1 para publicação das leituras
 * @return ESP_OK em caso de sucesso
 */
esp_err_t sonar_init(QueueHandle_t out_queue);

#ifdef __cplusplus
}
#endif

#endif // SONAR_H
