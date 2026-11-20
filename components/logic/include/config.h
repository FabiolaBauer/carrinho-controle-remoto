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

/* MACs das placas: ficam em config_macs.h, que é local e ignorado pelo git.
 * Crie-o copiando config_macs.example.h (ver README, seção 5). */
#if defined(__has_include) && __has_include("config_macs.h")
#include "config_macs.h"
#else
#warning "config_macs.h nao encontrado: usando MACs zerados. Copie config_macs.example.h para config_macs.h."
#define CONTROLLER_MAC          { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }
#define CAR_MAC                 { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }
#endif

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
#define OBSTACLE_ENTER_CM       15      /**< Limiar para entrar em ST_OBSTACULO (RF04) */
#define OBSTACLE_EXIT_CM        20      /**< Limiar para sair de ST_OBSTACULO (histerese) */
#define SENSOR_VALID_MIN_CM     2       /**< Leitura mínima válida do HC-SR04 */
#define SENSOR_VALID_MAX_CM     400     /**< Leitura máxima válida do HC-SR04 */
#define SENSOR_FAIL_CYCLES      3       /**< Ciclos sem eco/inválidos para declarar sensor com defeito */
#define SENSOR_RECOVER_CYCLES   3       /**< Ciclos válidos para recuperar sensor */
#define OBSTACLE_EXIT_CYCLES    3       /**< Leituras válidas seguidas > OBSTACLE_EXIT_CM para sair do obstáculo */
#define HCSR04_TRIG_PULSE_US    10      /**< Largura do pulso de TRIG do HC-SR04 (µs) */
#define HCSR04_US_PER_CM        58      /**< Conversão largura do eco (µs) -> distância (cm) */

#define PWM_FREQ_HZ             5000    /**< Frequência PWM dos motores */
#define PWM_DUTY_MAX            255     /**< Duty máximo com LEDC_TIMER_8_BIT (2^8 - 1) */
#define PWM_MIN_PCT             60      /**< PWM mínimo mapeado para vencer atrito estático */
#define MAX_PWM_PCT             100     /**< PWM máximo permitido */
#define RAMP_UP_PCT_PER_S       200     /**< Rampa de aceleração: 200%/s (0 -> 100% em 0,5 s) */
#define REVERSE_DWELL_MS        50      /**< Tempo de pausa em 0 ao inverter sentido dos motores */

#define BATT_PERIOD_MS          200     /**< Período de leitura da bateria (Seção 8.1) */
#define BATT_SAMPLES            8       /**< Amostras por leitura da bateria (média) */
#define BATT_DIV_RATIO_X1000    3000    /**< Fator do divisor de tensão x1000 (inteiro: 3,000) [CONFIRMAR] */
#define ADC_RAW_MAX             4095    /**< Leitura máxima do ADC de 12 bits (ADC_BITWIDTH_DEFAULT no ESP32) */
#define ADC_APPROX_FULL_MV      3300    /**< Fundo de escala aproximado sem calibração (mV) */

/* Divisões do laço de controle (contadores de ticks, Seção 8.1) */
#define TELEM_EVERY_TICKS       (TELEM_PERIOD_MS / CONTROL_PERIOD_MS)  /**< 200/10 = 20 ticks */
#define BATT_EVERY_TICKS        (BATT_PERIOD_MS / CONTROL_PERIOD_MS)   /**< 200/10 = 20 ticks */
_Static_assert((TELEM_PERIOD_MS % CONTROL_PERIOD_MS) == 0, "TELEM_PERIOD_MS deve ser multiplo de CONTROL_PERIOD_MS");
_Static_assert((BATT_PERIOD_MS % CONTROL_PERIOD_MS) == 0, "BATT_PERIOD_MS deve ser multiplo de CONTROL_PERIOD_MS");

/* Filas e tarefas do carrinho */
#define CAR_RX_QUEUE_LEN        8       /**< Profundidade da fila de pacotes recebidos (Seção 8.2) */
#define CAR_LOG_QUEUE_LEN       16      /**< Profundidade da fila de eventos de log (Seção 8.6) */
#define CAR_TASK_STACK          3072    /**< Pilha das tarefas auxiliares (log, sensor, bancada), em bytes */
#define CAR_LOG_TASK_PRIO       3       /**< Prioridade da tarefa de log (baixa) */
#define CAR_BENCH_TASK_PRIO     4       /**< Prioridade da tarefa de simulação da bancada */
#define CAR_SONAR_TASK_PRIO     5       /**< Prioridade da tarefa do HC-SR04 */

/* DEBUG_PIN (Seção 8.6): muda de nível uma vez quando os motores chegam a zero */
#define DEBUG_PIN_LEVEL_MOVING  0       /**< Nível de repouso: motores girando */
#define DEBUG_PIN_LEVEL_STOPPED 1       /**< Nível ao chegar a zero (mesma borda do EVT motors_zero) */

/* Modo de bancada (Seção 15.4) */
#define BENCH_START_DELAY_MS    3000    /**< Espera antes de iniciar a sequência */
#define BENCH_ZERO_MS           2000    /**< Duração dos passos de zero */
#define BENCH_MOVE_MS           3000    /**< Duração dos passos de movimento */

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

/* =========================================================================
 * Parâmetros do Controle (Seção 7)
 * ========================================================================= */
/* Joystick / ADC */
#define CTRL_ADC_AVG_SAMPLES    4       /**< Média curta por leitura de eixo */
#define CTRL_ADC_MAX_RETRIES    3       /**< Tentativas extras em ESP_ERR_TIMEOUT por amostra */
#define CTRL_CAL_SAMPLES        25      /**< Amostras da calibração do centro no boot (~500 ms) */
#define CTRL_CAL_INTERVAL_MS    20      /**< Intervalo entre amostras da calibração */
#define CTRL_CENTER_MIN_RAW     1500    /**< Centro esperado: mínimo (12 bits) */
#define CTRL_CENTER_MAX_RAW     2600    /**< Centro esperado: máximo (12 bits) */

/* Envio e telemetria */
#define CTRL_MAX_SKIPPED_SENDS  4       /**< Ciclos seguidos sem callback de envio antes de destravar */
#define CTRL_TELEM_QUEUE_LEN    4       /**< Fila de telemetria recebida */
#define CTRL_TELEM_LOST_MS      1000    /**< Sem telemetria por mais que isso = "SEM TELEMETRIA" */
#define CTRL_NO_TELEM_LOG_MS    1000    /**< Período do log "SEM TELEMETRIA" */
#define CTRL_STATS_LOG_MS       10000   /**< Período do log de estatísticas de envio */
#define CTRL_JOY_LOG_MS         500     /**< Período do log de x/y no build CTRL_LED_JOYSTICK */
#define CTRL_UI_PERIOD_MS       10      /**< Período da tarefa de UI (resolução do LED) */
#define CTRL_UI_TASK_STACK      3072    /**< Pilha da tarefa de UI, em bytes */
#define CTRL_UI_TASK_PRIO       3       /**< Prioridade da tarefa de UI */

/* Padrões do LED (Seção 7, item 7) */
#define LED_SLOW_PERIOD_MS      1000    /**< OPERANDO: pisca a 1 Hz */
#define LED_FAST_PERIOD_MS      250     /**< OBSTACULO: pisca a 4 Hz */
#define LED_DOUBLE_PERIOD_MS    1000    /**< FALHA_COM: duas piscadas a cada 1 s */
#define LED_DOUBLE_ON_MS        100     /**< FALHA_COM: duração de cada piscada */
#define LED_DOUBLE_GAP_MS       100     /**< FALHA_COM: intervalo entre as duas piscadas */

/* Teste T4 (Seção 7, item 8; só com CONFIG_FW_TEST_T4) */
#define T4_START_DELAY_MS       10000   /**< Início da sequência após o boot */
#define T4_OLD_SEQ_BACK         10      /**< Pacote "sequência antiga": seq atual menos isso */
#define T4_BAD_X                120     /**< Pacote fora de faixa */
#define T4_FWD_MS               3000    /**< Duração da frente máxima */
#define T4_REV_MS               1000    /**< Duração da ré após a inversão direta */

#endif // CONFIG_H
