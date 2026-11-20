/**
 * @file buzzer.h
 * @brief Buzzer (buzina) do carrinho por PWM do LEDC
 *
 * Gera uma onda quadrada de CAR_BUZZER_FREQ_HZ no CAR_PIN_BUZZER. Serve para
 * buzzer passivo (precisa do tom) e também para o ativo.
 */

#ifndef BUZZER_H
#define BUZZER_H

#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Configura o LEDC (timer e canal próprios, separados dos motores)
 *        e deixa o buzzer em silêncio.
 */
esp_err_t buzzer_init(void);

/**
 * @brief Liga ou desliga o tom. Não bloqueia; chamar só na mudança.
 */
void buzzer_set(bool on);

#ifdef __cplusplus
}
#endif

#endif // BUZZER_H
