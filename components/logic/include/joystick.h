/**
 * @file joystick.h
 * @brief Funções puras de normalização e zona morta para eixos de joystick
 *
 * Atende ao RF02 e RNF05 (função pura sem chamadas ao ESP-IDF).
 */

#ifndef JOYSTICK_H
#define JOYSTICK_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Normaliza leitura bruta do ADC para a escala -100..100 com centro calibrado
 *
 * @param raw Leitura bruta do ADC
 * @param center Valor calibrado do centro
 * @param raw_min Valor mínimo do ADC
 * @param raw_max Valor máximo do ADC
 * @param invert Inverte polaridade do eixo se true
 * @return Valor normalizado em -100..100
 */
int8_t joystick_normalize(int raw, int center, int raw_min, int raw_max, bool invert);

/**
 * @brief Aplica zona morta e reescala suavemente fora da zona
 *
 * Se |v| <= deadzone_pct -> 0
 * Senão, reescala (|v| - deadzone_pct) * 100 / (100 - deadzone_pct) mantendo sinal.
 *
 * @param v Valor em -100..100
 * @param deadzone_pct Porcentagem de zona morta (ex: 10)
 * @return Valor com zona morta aplicada (-100..100)
 */
int8_t joystick_deadzone(int8_t v, uint8_t deadzone_pct);

#ifdef __cplusplus
}
#endif

#endif // JOYSTICK_H
