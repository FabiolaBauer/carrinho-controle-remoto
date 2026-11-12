/**
 * @file battery.h
 * @brief Leitura da tensão da bateria via ADC1 com calibração Line Fitting
 *
 * Atende à Seção 8.1 e RNF05.
 */

#ifndef BATTERY_H
#define BATTERY_H

#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Inicializa o ADC1 e esquema de calibração para o pino da bateria
 * @return ESP_OK em caso de sucesso
 */
esp_err_t battery_init(void);

/**
 * @brief Lê a tensão da bateria em milivolts (média de 8 amostras)
 * @return Tensão da bateria em mV
 */
uint16_t battery_read_mv(void);

#ifdef __cplusplus
}
#endif

#endif // BATTERY_H
