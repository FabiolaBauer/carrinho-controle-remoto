/**
 * @file mixer.c
 * @brief Implementação do misturador de tração diferencial
 */

#include "mixer.h"

static inline int8_t clamp_100(int32_t val)
{
    if (val > 100) {
        return 100;
    }
    if (val < -100) {
        return -100;
    }
    return (int8_t)val;
}

motor_cmd_t mixer_mix(int8_t x, int8_t y)
{
    motor_cmd_t out;
    int32_t l = (int32_t)y + (int32_t)x;
    int32_t r = (int32_t)y - (int32_t)x;

    out.left = clamp_100(l);
    out.right = clamp_100(r);

    return out;
}
