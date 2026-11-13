/**
 * @file bench.h
 * @brief Modo de bancada simulado para testes autônomos do carrinho (Seção 15.4)
 */

#ifndef BENCH_H
#define BENCH_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Estrutura unificada para fila de pacotes recebidos (ESP-NOW e Modo de Bancada)
 */
typedef struct {
    uint8_t  src_mac[6];
    uint8_t  data[sizeof(cmd_packet_t)];
    size_t   len;
    uint32_t rx_ms;
} rx_packet_t;

#if CONFIG_CAR_BENCH_MODE

/**
 * @brief Inicia a tarefa do modo de bancada que injeta pacotes na fila de comandos
 *
 * @param cmd_queue Fila de entrada de comandos do carrinho (recebe rx_packet_t)
 * @return ESP_OK em caso de sucesso
 */
esp_err_t bench_mode_start(QueueHandle_t cmd_queue);

#endif // CONFIG_CAR_BENCH_MODE

#ifdef __cplusplus
}
#endif

#endif // BENCH_H
