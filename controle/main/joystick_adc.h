/**
 * @file joystick_adc.h
 * @brief Leitura do joystick pelo ADC1 oneshot, calibração e normalização (Seção 7)
 */

#ifndef JOYSTICK_ADC_H
#define JOYSTICK_ADC_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Cria UM handle do ADC1 e configura os canais de X e Y.
 *
 * Unidade e canal vêm de adc_oneshot_io_to_channel(); os dois pinos devem
 * estar na mesma unidade (ADC1), senão retorna ESP_ERR_INVALID_ARG.
 */
esp_err_t joy_adc_init(void);

/**
 * @brief Calibra o centro dos dois eixos (bloqueante, só no boot).
 *
 * CTRL_CAL_SAMPLES leituras a cada CTRL_CAL_INTERVAL_MS com o joystick em
 * repouso. Centro fora de CTRL_CENTER_MIN_RAW..CTRL_CENTER_MAX_RAW gera aviso
 * no log, mas o firmware continua.
 */
void joy_calibrate(void);

/**
 * @brief Lê os dois eixos já normalizados (-100..100), invertidos se
 *        configurado e com zona morta (RF02).
 *
 * Não bloqueia (pode ser chamada no callback do esp_timer). Se o ADC falhar,
 * devolve x = y = 0 (neutro, seguro) e ESP_FAIL.
 */
esp_err_t joy_read(int8_t *x, int8_t *y);

/**
 * @brief Configura o botão SW do joystick (CTRL_PIN_JOY_BUTTON) como entrada
 *        com pull-up interno. O botão fecha para GND quando apertado.
 */
esp_err_t joy_button_init(void);

/** @brief true com o botão SW apertado (nível 0). Não bloqueia. */
bool joy_button_pressed(void);

/** @brief Leituras com falha desde o boot (diagnóstico) */
uint32_t joy_read_fail_count(void);

/**
 * @brief Últimos x e y devolvidos por joy_read() (diagnóstico)
 *
 * Escritos só por joy_read(); pode ser lida por outra tarefa.
 */
void joy_last_values(int8_t *x, int8_t *y);

#ifdef __cplusplus
}
#endif

#endif // JOYSTICK_ADC_H
