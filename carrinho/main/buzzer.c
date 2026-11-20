/**
 * @file buzzer.c
 * @brief Buzzer (buzina) do carrinho por PWM do LEDC
 *
 * Usa LEDC_TIMER_1 / LEDC_CHANNEL_2. Os motores usam LEDC_TIMER_0 com os
 * canais 0 e 1 (motors.c), então a frequência do buzzer não interfere no PWM
 * dos motores.
 */

#include "buzzer.h"
#include "config.h"
#include "driver/ledc.h"
#include "esp_log.h"

static const char *TAG = "buzzer";

#define BUZZER_SPEED_MODE   LEDC_LOW_SPEED_MODE
#define BUZZER_TIMER        LEDC_TIMER_1
#define BUZZER_CHANNEL      LEDC_CHANNEL_2
#define BUZZER_RESOLUTION   LEDC_TIMER_8_BIT
#define BUZZER_DUTY_ON      ((CAR_BUZZER_DUTY_PCT * PWM_DUTY_MAX) / 100)

esp_err_t buzzer_init(void)
{
    ledc_timer_config_t timer_conf = {
        .speed_mode = BUZZER_SPEED_MODE,
        .duty_resolution = BUZZER_RESOLUTION,
        .timer_num = BUZZER_TIMER,
        .freq_hz = CAR_BUZZER_FREQ_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
        .deconfigure = false
    };
    ESP_ERROR_CHECK(ledc_timer_config(&timer_conf));

    ledc_channel_config_t ch_conf = {
        .gpio_num = CAR_PIN_BUZZER,
        .speed_mode = BUZZER_SPEED_MODE,
        .channel = BUZZER_CHANNEL,
        .timer_sel = BUZZER_TIMER,
        .duty = 0,                 /* começa em silêncio */
        .hpoint = 0,
        .sleep_mode = LEDC_SLEEP_MODE_NO_ALIVE_NO_PD,
        .flags = { .output_invert = 0 },
        .deconfigure = false
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ch_conf));

    ESP_LOGI(TAG, "Buzzer no GPIO %d (%d Hz)", CAR_PIN_BUZZER, CAR_BUZZER_FREQ_HZ);
    return ESP_OK;
}

void buzzer_set(bool on)
{
    ledc_set_duty(BUZZER_SPEED_MODE, BUZZER_CHANNEL, on ? BUZZER_DUTY_ON : 0);
    ledc_update_duty(BUZZER_SPEED_MODE, BUZZER_CHANNEL);
}
