/**
 * @file led_pattern.c
 * @brief Padrão do LED de estado do controle
 */

#include "led_pattern.h"
#include "config.h"

/* Aceso na primeira metade de cada período */
static bool square_wave(uint32_t now_ms, uint32_t period_ms)
{
    return (now_ms % period_ms) < (period_ms / 2);
}

bool led_pattern_level(bool has_telem, uint8_t state, uint32_t now_ms)
{
    if (!has_telem) {
        return false;
    }

    switch (state) {
        case ST_INIT:        /* transitório; tratado como parado [suposição] */
        case ST_PARADO:
            return true;
        case ST_OPERANDO:
            return square_wave(now_ms, LED_SLOW_PERIOD_MS);
        case ST_OBSTACULO:
            return square_wave(now_ms, LED_FAST_PERIOD_MS);
        case ST_FALHA_COM: {
            /* aceso em [0, ON) e [ON + GAP, 2*ON + GAP) de cada período */
            uint32_t t = now_ms % LED_DOUBLE_PERIOD_MS;
            bool first  = (t < LED_DOUBLE_ON_MS);
            bool second = (t >= LED_DOUBLE_ON_MS + LED_DOUBLE_GAP_MS) &&
                          (t < 2 * LED_DOUBLE_ON_MS + LED_DOUBLE_GAP_MS);
            return first || second;
        }
        default:
            return false;
    }
}
