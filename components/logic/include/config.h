/**
 * @file config.h
 * @brief Parâmetros de configuração e pinagem do sistema (Seção 10 e 5)
 *
 * Todos os valores numéricos do projeto centralizados para evitar números mágicos.
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>
#include <stdbool.h>

/* =========================================================================
 * Parâmetros de Comunicação e Protocolo
 * ========================================================================= */
#define CHANNEL                 1       /**< Canal Wi-Fi do ESP-NOW */
#define CMD_PERIOD_MS           50      /**< Período de envio do controle (20 Hz) */
#define TELEM_PERIOD_MS         200     /**< Período de telemetria do carrinho (5 Hz) */
#define LINK_TIMEOUT_MS         300     /**< Timeout do sinal de vida do enlace (RF05) */
#define DEADZONE_PCT            10      /**< Zona morta de 10% nos eixos do joystick (RF02) */

/* MACs padrão [CONFIRMAR: atualizar com os MACs reais lidos no boot] */
#define CONTROLLER_MAC          { 0x24, 0x6F, 0x28, 0x00, 0x00, 0x01 }
#define CAR_MAC                 { 0x24, 0x6F, 0x28, 0x00, 0x00, 0x02 }

/* Inversões lógicas */
#define INVERT_X                false
#define INVERT_Y                false
#define INVERT_LEFT             false
#define INVERT_RIGHT            false

/* =========================================================================
 * Parâmetros do Carrinho
 * ========================================================================= */
#define CONTROL_PERIOD_MS       10      /**< Período do laço de controle do carrinho (100 Hz) */
#define SENSOR_PERIOD_MS        60      /**< Período de amostragem do HC-SR04 */
#define SENSOR_ECHO_TIMEOUT_MS  30      /**< Timeout de espera do pulso de eco */
#define OBSTACLE_ENTER_CM       25      /**< Limiar para entrar em ST_OBSTACULO (RF04) */
#define OBSTACLE_EXIT_CM        30      /**< Limiar para sair de ST_OBSTACULO (histerese) */
#define SENSOR_VALID_MIN_CM     2       /**< Leitura mínima válida do HC-SR04 */
#define SENSOR_VALID_MAX_CM     400     /**< Leitura máxima válida do HC-SR04 */
#define SENSOR_FAIL_CYCLES      3       /**< Ciclos sem eco/inválidos para declarar sensor com defeito */
#define SENSOR_RECOVER_CYCLES   3       /**< Ciclos válidos para recuperar sensor */

#define PWM_FREQ_HZ             5000    /**< Frequência PWM dos motores */
#define PWM_MIN_PCT             30      /**< PWM mínimo mapeado para vencer atrito estático [CONFIRMAR] */
#define MAX_PWM_PCT             100     /**< PWM máximo permitido */
#define RAMP_UP_PCT_PER_S       200     /**< Rampa de aceleração: 200%/s (0 -> 100% em 0,5 s) */
#define REVERSE_DWELL_MS        50      /**< Tempo de pausa em 0 ao inverter sentido dos motores */

#define BATT_DIV_RATIO_X1000    3000    /**< Fator do divisor de tensão x1000 (inteiro: 3,000) [CONFIRMAR] */

/* =========================================================================
 * Pinagem do Carrinho (ESP32 #2) [CONFIRMAR]
 * ========================================================================= */
#define CAR_PIN_MOTOR_LEFT_PWM  25      /**< Ponte H ENA (PWM) */
#define CAR_PIN_MOTOR_LEFT_IN1  26      /**< Ponte H IN1 (Sentido) */
#define CAR_PIN_MOTOR_LEFT_IN2  27      /**< Ponte H IN2 (Sentido) */
#define CAR_PIN_MOTOR_RIGHT_PWM 13      /**< Ponte H ENB (PWM) */
#define CAR_PIN_MOTOR_RIGHT_IN3 32      /**< Ponte H IN3 (Sentido) */
#define CAR_PIN_MOTOR_RIGHT_IN4 33      /**< Ponte H IN4 (Sentido) */

#define CAR_PIN_HCSR04_TRIG     4       /**< HC-SR04 TRIG (Saída digital) */
#define CAR_PIN_HCSR04_ECHO     18      /**< HC-SR04 ECHO (Entrada com interrupção, nível 3,3V via divisor) */
#define CAR_PIN_BATT_ADC        34      /**< Tensão da bateria (ADC1 Canal 6) */
#define CAR_PIN_DEBUG           23      /**< Pino para osciloscópio / analisador lógico (Seção 8.6) */

/* =========================================================================
 * Pinagem do Controle (ESP32 #1) [CONFIRMAR]
 * ========================================================================= */
#define CTRL_PIN_JOYSTICK_X     34      /**< ADC1 */
#define CTRL_PIN_JOYSTICK_Y     35      /**< ADC1 */
#define CTRL_PIN_LED            2       /**< LED de estado onboard */

#endif // CONFIG_H
