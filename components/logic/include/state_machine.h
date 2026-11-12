/**
 * @file state_machine.h
 * @brief Máquina de estados finitos do carrinho (Seção 8.3)
 *
 * Estados: ST_INIT, ST_PARADO, ST_OPERANDO, ST_OBSTACULO, ST_FALHA_COM
 * Atende aos requisitos RF03, RF04, RF05, RF06, RF07, RNF01, RNF02 e RNF04.
 */

#ifndef STATE_MACHINE_H
#define STATE_MACHINE_H

#include <stdint.h>
#include <stdbool.h>
#include "protocol.h"
#include "config.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Entradas da máquina de estados a cada ciclo de controle
 */
typedef struct {
    bool        has_cmd;         /**< Novo comando recebido */
    int8_t      cmd_x;           /**< Valor X do comando (-100..100) */
    int8_t      cmd_y;           /**< Valor Y do comando (-100..100) */
    bool        link_ok;         /**< Status do enlace (now - last_rx <= 300 ms) */
    uint16_t    distance_cm;     /**< Distância do ultrassom (0xFFFF se sem eco) */
    bool        echo_ok;         /**< Eco válido recebido */
    bool        has_sensor_data; /**< Nova leitura do sensor disponível */
    uint32_t    now_ms;          /**< Timestamp atual em ms */
} car_inputs_t;

/**
 * @brief Saídas da máquina de estados
 */
typedef struct {
    car_state_t state;           /**< Estado atual */
    int8_t      target_left;     /**< Alvo para o motor esquerdo (-100..100) */
    int8_t      target_right;    /**< Alvo para o motor direito (-100..100) */
    bool        brake_immediate; /**< Indicação de parada imediata / freio */
    bool        state_changed;   /**< true se o estado mudou neste ciclo */
    car_state_t prev_state;      /**< Estado anterior */
    uint16_t    distance_cm;     /**< Distância filtrada atual */
    bool        sensor_valid;    /**< Sensor ultrassônico operacional */
    bool        obstacle;        /**< Obstáculo ativo */
    bool        armed;           /**< Sistema armado */
} car_outputs_t;

/**
 * @brief Estado interno da máquina de estados do carrinho
 */
typedef struct {
    car_state_t state;
    bool        armed;
    bool        centered;
    int8_t      last_x;
    int8_t      last_y;

    /* Filtro e diagnóstico do sensor ultrassônico */
    bool        sensor_valid;
    bool        obstacle;
    uint8_t     consecutive_valid_cnt;
    uint8_t     consecutive_fail_cnt;
    uint8_t     consecutive_clear_cnt;
    uint16_t    current_distance_cm;

    /* Contadores */
    uint16_t    rejected_cmd_cnt;
} car_sm_t;

/**
 * @brief Inicializa a máquina de estados em ST_INIT
 *
 * @param sm Ponteiro para a estrutura da máquina de estados
 */
void car_sm_init(car_sm_t *sm);

/**
 * @brief Notifica conclusão do autoteste inicial (vai para ST_PARADO desarmado)
 *
 * @param sm Ponteiro para a estrutura da máquina de estados
 */
void car_sm_set_ready(car_sm_t *sm);

/**
 * @brief Executa um passo da máquina de estados (Seção 8.3)
 *
 * @param sm Ponteiro para a estrutura da máquina de estados
 * @param in Entradas do ciclo de controle
 * @param out Saídas calculadas
 */
void car_sm_step(car_sm_t *sm, const car_inputs_t *in, car_outputs_t *out);

#ifdef __cplusplus
}
#endif

#endif // STATE_MACHINE_H
