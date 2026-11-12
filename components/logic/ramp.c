/**
 * @file ramp.c
 * @brief Implementação da rampa com intertravamento
 */

#include "ramp.h"

void ramp_reset(ramp_t *r, uint32_t now_ms)
{
    if (r == NULL) {
        return;
    }
    r->current = 0;
    r->current_milli = 0;
    r->last_time_ms = now_ms;
    r->dwell_start_ms = 0;
    r->in_dwell = false;
    r->initialized = true;
}

int8_t ramp_step(ramp_t *r, int8_t target, uint32_t now_ms)
{
    if (r == NULL) {
        return 0;
    }

    if (!r->initialized) {
        ramp_reset(r, now_ms);
    }

    /* Parada imediata */
    if (target == 0) {
        r->current = 0;
        r->current_milli = 0;
        r->in_dwell = false;
        r->last_time_ms = now_ms;
        return 0;
    }

    /* Intertravamento de inversão: sinal oposto ao da saída atual */
    bool opposite_sign = (r->current > 0 && target < 0) || (r->current < 0 && target > 0);
    if (opposite_sign) {
        r->current = 0;
        r->current_milli = 0;
        r->in_dwell = true;
        r->dwell_start_ms = now_ms;
        r->last_time_ms = now_ms;
        return 0;
    }

    /* Período de espera (dwell) de inversão */
    if (r->in_dwell) {
        if ((now_ms - r->dwell_start_ms) < REVERSE_DWELL_MS) {
            return 0;
        }
        r->in_dwell = false;
        r->last_time_ms = now_ms;
    }

    int32_t abs_curr = (r->current < 0) ? -r->current : r->current;
    int32_t abs_target = (target < 0) ? -target : target;

    /* Diminuição de módulo é imediata */
    if (abs_target <= abs_curr) {
        r->current = target;
        r->current_milli = (int32_t)target * 1000;
        r->last_time_ms = now_ms;
        return r->current;
    }

    /* Subida de módulo limitada pela rampa */
    uint32_t dt_ms = now_ms - r->last_time_ms;
    r->last_time_ms = now_ms;

    int32_t delta_milli = (int32_t)RAMP_UP_PCT_PER_S * (int32_t)dt_ms;

    if (target > 0) {
        r->current_milli += delta_milli;
        if (r->current_milli > (int32_t)target * 1000) {
            r->current_milli = (int32_t)target * 1000;
        }
    } else {
        r->current_milli -= delta_milli;
        if (r->current_milli < (int32_t)target * 1000) {
            r->current_milli = (int32_t)target * 1000;
        }
    }

    r->current = (int8_t)(r->current_milli / 1000);
    return r->current;
}
