/**
 * @file state_machine.c
 * @brief Implementação da máquina de estados e filtragem de obstáculos do carrinho
 */

#include "state_machine.h"
#include "mixer.h"

void car_sm_init(car_sm_t *sm)
{
    if (sm == NULL) {
        return;
    }
    sm->state = ST_INIT;
    sm->armed = false;
    sm->centered = true;
    sm->last_x = 0;
    sm->last_y = 0;
    sm->sensor_valid = true;
    sm->obstacle = false;
    sm->consecutive_valid_cnt = 0;
    sm->consecutive_fail_cnt = 0;
    sm->consecutive_clear_cnt = 0;
    sm->current_distance_cm = PROTO_DIST_INVALID; /* sem leitura ainda */
    sm->rejected_cmd_cnt = 0;
}

void car_sm_set_ready(car_sm_t *sm)
{
    if (sm == NULL) {
        return;
    }
    if (sm->state == ST_INIT) {
        sm->state = ST_PARADO;
        sm->armed = false;
    }
}

void car_sm_step(car_sm_t *sm, const car_inputs_t *in, car_outputs_t *out)
{
    if (sm == NULL || in == NULL || out == NULL) {
        return;
    }

    car_state_t prev_state = sm->state;
    bool just_entered_obstacle = false;

    /* 1. Atualização do diagnóstico e filtragem do sensor HC-SR04 (RF04) */
    if (in->has_sensor_data) {
        bool reading_valid = in->echo_ok &&
                             (in->distance_cm >= SENSOR_VALID_MIN_CM) &&
                             (in->distance_cm <= SENSOR_VALID_MAX_CM);

        if (reading_valid) {
            sm->consecutive_fail_cnt = 0;
            sm->consecutive_valid_cnt++;
            sm->current_distance_cm = in->distance_cm;

            if (!sm->sensor_valid && sm->consecutive_valid_cnt >= SENSOR_RECOVER_CYCLES) {
                sm->sensor_valid = true;
            }
        } else {
            sm->consecutive_valid_cnt = 0;
            sm->consecutive_fail_cnt++;
            sm->current_distance_cm = PROTO_DIST_INVALID;

            if (sm->sensor_valid && sm->consecutive_fail_cnt >= SENSOR_FAIL_CYCLES) {
                sm->sensor_valid = false;
            }
        }

        /* Contagem de ciclos limpos (> 30 cm e válido) */
        if (reading_valid && (in->distance_cm > OBSTACLE_EXIT_CM)) {
            sm->consecutive_clear_cnt++;
        } else {
            sm->consecutive_clear_cnt = 0;
        }

        /* Determinação do flag de obstáculo com histerese (25 cm / 30 cm) */
        if (!sm->sensor_valid) {
            sm->obstacle = true;
        } else {
            if (sm->current_distance_cm <= OBSTACLE_ENTER_CM) {
                sm->obstacle = true;
            } else if (sm->obstacle && sm->consecutive_clear_cnt >= OBSTACLE_EXIT_CYCLES) {
                sm->obstacle = false;
            }
        }
    }

    /* 2. Atualização de centralização */
    if (in->has_cmd) {
        sm->centered = (in->cmd_x == 0 && in->cmd_y == 0);
    }

    /* 3. Avaliação de perda de enlace (RF05) para qualquer estado ≠ INIT */
    if (sm->state != ST_INIT) {
        if (!in->link_ok) {
            sm->state = ST_FALHA_COM;
            sm->last_x = 0;
            sm->last_y = 0;
        }
    }

    /* 4. Transições de estados */
    switch (sm->state) {
        case ST_INIT:
            sm->last_x = 0;
            sm->last_y = 0;
            if (in->has_cmd) {
                sm->rejected_cmd_cnt++;
            }
            break;

        case ST_FALHA_COM:
            sm->last_x = 0;
            sm->last_y = 0;
            if (in->link_ok && sm->centered) {
                /* RF07: Retomada após falha exige comando de neutro */
                sm->state = ST_PARADO;
                sm->armed = true;
            } else if (in->has_cmd && !sm->centered) {
                sm->rejected_cmd_cnt++;
            }
            break;

        case ST_PARADO:
            sm->last_x = 0;
            sm->last_y = 0;
            if (sm->obstacle) {
                sm->state = ST_OBSTACULO;
                just_entered_obstacle = true;
            } else if (in->has_cmd) {
                if (!sm->armed) {
                    if (sm->centered) {
                        sm->armed = true;
                    } else {
                        sm->rejected_cmd_cnt++;
                    }
                } else {
                    if (!sm->centered) {
                        sm->state = ST_OPERANDO;
                        sm->last_x = in->cmd_x;
                        sm->last_y = in->cmd_y;
                    }
                }
            }
            break;

        case ST_OPERANDO:
            if (sm->obstacle) {
                sm->state = ST_OBSTACULO;
                sm->last_x = 0;
                sm->last_y = 0;
                just_entered_obstacle = true;
            } else if (in->has_cmd) {
                if (sm->centered) {
                    sm->state = ST_PARADO;
                    sm->last_x = 0;
                    sm->last_y = 0;
                } else {
                    sm->last_x = in->cmd_x;
                    sm->last_y = in->cmd_y;
                }
            }
            break;

        case ST_OBSTACULO:
            if (!sm->obstacle && sm->centered) {
                sm->state = ST_PARADO;
                sm->last_x = 0;
                sm->last_y = 0;
            } else if (in->has_cmd) {
                if (in->cmd_y > 0) {
                    /* RF04: Bloqueia avanço */
                    sm->rejected_cmd_cnt++;
                    sm->last_y = 0;
                } else {
                    /* Ré e giro livres */
                    sm->last_y = in->cmd_y;
                }
                sm->last_x = in->cmd_x;
            }
            break;
    }

    /* 5. Geração de saídas */
    out->state = sm->state;
    out->prev_state = prev_state;
    out->state_changed = (sm->state != prev_state);
    out->distance_cm = sm->current_distance_cm;
    out->sensor_valid = sm->sensor_valid;
    out->obstacle = sm->obstacle;
    out->armed = sm->armed;

    if (sm->state == ST_INIT || sm->state == ST_PARADO || sm->state == ST_FALHA_COM || just_entered_obstacle) {
        out->target_left = 0;
        out->target_right = 0;
        out->brake_immediate = true;
    } else {
        motor_cmd_t mix = mixer_mix(sm->last_x, sm->last_y);
        out->target_left = mix.left;
        out->target_right = mix.right;
        out->brake_immediate = (mix.left == 0 && mix.right == 0);
    }
}
