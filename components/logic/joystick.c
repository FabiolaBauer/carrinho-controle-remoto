/**
 * @file joystick.c
 * @brief Implementação de normalização e zona morta de joystick
 */

#include "joystick.h"

int8_t joystick_normalize(int raw, int center, int raw_min, int raw_max, bool invert)
{
    int32_t norm = 0;

    if (raw_max <= raw_min || center <= raw_min || center >= raw_max) {
        return 0;
    }

    if (raw >= center) {
        if (raw >= raw_max) {
            norm = 100;
        } else {
            norm = ((int32_t)(raw - center) * 100) / (raw_max - center);
        }
    } else {
        if (raw <= raw_min) {
            norm = -100;
        } else {
            norm = ((int32_t)(raw - center) * 100) / (center - raw_min);
        }
    }

    if (norm > 100) {
        norm = 100;
    } else if (norm < -100) {
        norm = -100;
    }

    if (invert) {
        norm = -norm;
    }

    return (int8_t)norm;
}

int8_t joystick_deadzone(int8_t v, uint8_t deadzone_pct)
{
    if (deadzone_pct >= 100) {
        return 0;
    }

    int32_t val = v;
    int32_t abs_val = (val < 0) ? -val : val;

    if (abs_val <= (int32_t)deadzone_pct) {
        return 0;
    }

    /* Reescala suave para não haver degrau ao sair da zona morta */
    int32_t rescaled = ((abs_val - (int32_t)deadzone_pct) * 100) / (100 - (int32_t)deadzone_pct);
    if (rescaled > 100) {
        rescaled = 100;
    }

    if (val < 0) {
        rescaled = -rescaled;
    }

    return (int8_t)rescaled;
}
