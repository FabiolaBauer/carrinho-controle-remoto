/**
 * @file led_pattern.h
 * @brief Padrão do LED de estado do controle (Seção 7, item 7)
 *
 * Função pura: recebe o estado informado pela telemetria e now_ms.
 */

#ifndef LED_PATTERN_H
#define LED_PATTERN_H

#include <stdint.h>
#include <stdbool.h>
#include "protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Nível do LED no instante now_ms
 *
 * - Sem telemetria: apagado.
 * - PARADO (e INIT): aceso fixo.
 * - OPERANDO: pisca a 1 Hz (metade do período aceso).
 * - OBSTACULO: pisca a 4 Hz.
 * - FALHA_COM: duas piscadas curtas a cada 1 s.
 *
 * @param has_telem true se há telemetria recente
 * @param state     Último estado recebido (car_state_t)
 * @param now_ms    Tempo atual em ms
 * @return true = aceso
 */
bool led_pattern_level(bool has_telem, uint8_t state, uint32_t now_ms);

#ifdef __cplusplus
}
#endif

#endif // LED_PATTERN_H
