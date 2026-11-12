/**
 * @file ramp.h
 * @brief Rampa de aceleração com descida imediata e intertravamento de inversão
 *
 * Atende ao RF02, RF03, RNF01, RNF04 e RNF05.
 */

#ifndef RAMP_H
#define RAMP_H

#include <stdint.h>
#include <stdbool.h>
#include "config.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int8_t   current;          /**< Saída atual (-100..100) */
    int32_t  current_milli;    /**< Saída em milésimos de porcentagem (-100000..100000) */
    uint32_t last_time_ms;     /**< Timestamp da última avaliação em ms */
    uint32_t dwell_start_ms;   /**< Início do período de espera em inversão (dwell) */
    bool     in_dwell;         /**< Verdadeiro enquanto em período de intertravamento */
    bool     initialized;      /**< Verdadeiro após primeira chamada */
} ramp_t;

/**
 * @brief Reseta o estado da rampa para 0 imediatamente
 *
 * @param r Ponteiro para estrutura de rampa
 * @param now_ms Timestamp atual em ms
 */
void ramp_reset(ramp_t *r, uint32_t now_ms);

/**
 * @brief Executa um passo da rampa em direção ao valor alvo
 *
 * - Aumento de módulo limitado por RAMP_UP_PCT_PER_S
 * - Diminuição de módulo é imediata
 * - Inversão de sentido para imediatamente em 0 e aguarda REVERSE_DWELL_MS
 *
 * @param r Ponteiro para estrutura de rampa
 * @param target Valor alvo (-100..100)
 * @param now_ms Timestamp atual em ms
 * @return Saída atualizada (-100..100)
 */
int8_t ramp_step(ramp_t *r, int8_t target, uint32_t now_ms);

#ifdef __cplusplus
}
#endif

#endif // RAMP_H
