/**
 * @file ui.h
 * @brief Tarefa de UI do controle: telemetria, log e LED (Seção 7, itens 6 e 7)
 */

#ifndef UI_H
#define UI_H

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Configura o LED e cria a tarefa de UI.
 * @param telem_queue Fila de telem_rx_t preenchida pelo callback ESP-NOW
 */
esp_err_t ui_start(QueueHandle_t telem_queue);

#ifdef __cplusplus
}
#endif

#endif // UI_H
