/**
 * @file t4.h
 * @brief Teste T4 (RF06): sequência de pacotes problemáticos (Seção 7, item 8)
 *
 * Só existe no build com CONFIG_FW_TEST_T4=y (sdkconfig.defaults.t4).
 */

#ifndef T4_H
#define T4_H

#include "sdkconfig.h"
#include <stdint.h>
#include <stdbool.h>
#include "protocol.h"

#if CONFIG_FW_TEST_T4

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    T4_WAIT = 0,     /**< aguardando T4_START_DELAY_MS após o boot */
    T4_OLD_SEQ,      /**< 1 pacote com sequência antiga        -> rejeitado */
    T4_BAD_RANGE,    /**< 1 pacote com x = 120                 -> rejeitado */
    T4_BAD_CRC,      /**< 1 pacote com CRC errado              -> rejeitado */
    T4_FWD_MAX,      /**< T4_FWD_MS de y = +100 (válido)       -> rampa */
    T4_REVERSE,      /**< T4_REV_MS de y = -100 (válido)       -> passa por zero */
    T4_DONE          /**< volta ao joystick */
} t4_step_t;

/**
 * @brief Chamado pelo timer de envio a cada pacote, já montado com o joystick.
 *        Durante a sequência, substitui o conteúdo do pacote (e o CRC).
 *        Não bloqueia.
 */
void t4_apply(uint32_t now_ms, cmd_packet_t *pkt);

/** @brief Passo que gerou o último pacote enviado (para log na tarefa de UI) */
t4_step_t t4_current_step(void);

/** @brief Nome do passo para log */
const char *t4_step_name(t4_step_t step);

#ifdef __cplusplus
}
#endif

#endif // CONFIG_FW_TEST_T4
#endif // T4_H
