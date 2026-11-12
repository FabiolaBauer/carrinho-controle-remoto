/**
 * @file motors.h
 * @brief Driver de controle dos motores de tração via Ponte H (L298N/TB6612) e LEDC PWM
 *
 * Atende aos requisitos RF02, RF03, RNF01, RNF03, RNF04 e RNF05.
 */

#ifndef MOTORS_H
#define MOTORS_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "config.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Inicializa os pinos de sentido (GPIO), pinos de PWM (LEDC) e pino de debug
 * @return ESP_OK em caso de sucesso
 */
esp_err_t motors_init(void);

/**
 * @brief Aplica potências e sentidos aos motores esquerdo e direito
 *
 * Mapeia 1..100% para PWM_MIN_PCT..100% (duty 0..255). 0 para os motores.
 *
 * @param left_pct Potência motor esquerdo (-100..100)
 * @param right_pct Potência motor direito (-100..100)
 */
void motors_set(int8_t left_pct, int8_t right_pct);

/**
 * @brief Aciona freio elétrico ativo imediato em ambos os motores (Seção 8.4)
 *
 * Configura IN1=IN2=HIGH e IN3=IN4=HIGH com EN em duty 255.
 */
void motors_brake(void);

#ifdef __cplusplus
}
#endif

#endif // MOTORS_H
