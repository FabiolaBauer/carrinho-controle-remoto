/**
 * @file radio.h
 * @brief Wi-Fi STA + ESP-NOW do controle (Seção 6 e Seção 7)
 */

#ifndef RADIO_H
#define RADIO_H

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

/** Item da fila de telemetria: o callback só copia (Seção 6) */
typedef struct {
    uint8_t src_mac[6];
    uint8_t data[sizeof(telem_packet_t)];
    size_t  len;   /**< tamanho real recebido (pode ser maior que data) */
} telem_rx_t;

/** Contadores de envio e recepção (diagnóstico) */
typedef struct {
    uint32_t sent;          /**< esp_now_send aceito */
    uint32_t cb_ok;         /**< callback de envio com sucesso */
    uint32_t cb_fail;       /**< callback de envio com falha */
    uint32_t send_err;      /**< esp_now_send retornou erro */
    uint32_t skipped;       /**< ciclos pulados: envio anterior sem callback */
    uint32_t unlocked;      /**< destravamentos após CTRL_MAX_SKIPPED_SENDS ciclos pulados */
    uint32_t rx_queue_full; /**< telemetrias descartadas por fila cheia */
} radio_stats_t;

/**
 * @brief NVS já inicializado. Sobe Wi-Fi STA e ESP-NOW no canal CHANNEL,
 *        loga o próprio MAC e adiciona CAR_MAC como peer.
 * @param telem_queue Fila (de telem_rx_t) que recebe as telemetrias
 */
esp_err_t radio_init(QueueHandle_t telem_queue);

/**
 * @brief Verifica se pode enviar neste ciclo (Seção 6: não enviar antes do
 *        callback do envio anterior). Se não puder, conta o ciclo pulado.
 *        Chamar só da tarefa do timer de envio.
 */
bool radio_can_send(void);

/** @brief Envia um comando ao carrinho. Chamar só depois de radio_can_send() == true. */
esp_err_t radio_send_cmd(const cmd_packet_t *pkt);

/** @brief Cópia dos contadores */
void radio_get_stats(radio_stats_t *out);

#ifdef __cplusplus
}
#endif

#endif // RADIO_H
