/**
 * @file mixer.h
 * @brief Misturador diferencial para controle de direção e tração dos dois motores
 *
 * Atende ao RF02 e RNF05.
 */

#ifndef MIXER_H
#define MIXER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int8_t left;  /**< Potência e sentido motor esquerdo (-100..100) */
    int8_t right; /**< Potência e sentido motor direito (-100..100) */
} motor_cmd_t;

/**
 * @brief Calcula a mistura de direção e sentido para tração diferencial
 *
 * left  = clamp(y + x, -100, 100)
 * right = clamp(y - x, -100, 100)
 *
 * @param x Direção (-100..100, + = direita)
 * @param y Sentido (-100..100, + = frente, - = ré)
 * @return Comandos dos motores esquerdo e direito
 */
motor_cmd_t mixer_mix(int8_t x, int8_t y);

#ifdef __cplusplus
}
#endif

#endif // MIXER_H
