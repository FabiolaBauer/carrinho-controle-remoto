/**
 * @file motors.c
 * @brief Implementação do driver de motores com LEDC PWM e GPIOs
 */

#include "motors.h"
#include "driver/ledc.h"
#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "motors";

#define LEDC_SPEED_MODE      LEDC_LOW_SPEED_MODE
#define LEDC_LEFT_CHANNEL    LEDC_CHANNEL_0
#define LEDC_RIGHT_CHANNEL   LEDC_CHANNEL_1

static int s_debug_pin_state = 0;

static uint32_t pct_to_duty(uint8_t abs_pct)
{
    if (abs_pct == 0) {
        return 0;
    }
    if (abs_pct > MAX_PWM_PCT) {
        abs_pct = MAX_PWM_PCT;
    }

    /* Mapeia 1..100% para PWM_MIN_PCT..MAX_PWM_PCT */
    uint32_t scaled_pct = PWM_MIN_PCT;
    if (abs_pct > 1) {
        scaled_pct += ((uint32_t)(abs_pct - 1) * (MAX_PWM_PCT - PWM_MIN_PCT)) / 99;
    }
    if (scaled_pct > 100) {
        scaled_pct = 100;
    }

    /* Duty de 8 bits: 0..255 */
    return (scaled_pct * 255) / 100;
}

esp_err_t motors_init(void)
{
    ESP_LOGI(TAG, "Inicializando GPIOs e LEDC para controle de motores...");

    /* 1. Configuração dos GPIOs de sentido com pull-down para segurança no boot */
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << CAR_PIN_MOTOR_LEFT_IN1) |
                        (1ULL << CAR_PIN_MOTOR_LEFT_IN2) |
                        (1ULL << CAR_PIN_MOTOR_RIGHT_IN3) |
                        (1ULL << CAR_PIN_MOTOR_RIGHT_IN4) |
                        (1ULL << CAR_PIN_DEBUG),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    ESP_ERROR_CHECK(gpio_config(&io_conf));

    /* Inicializa todas as saídas em nível baixo */
    gpio_set_level(CAR_PIN_MOTOR_LEFT_IN1, 0);
    gpio_set_level(CAR_PIN_MOTOR_LEFT_IN2, 0);
    gpio_set_level(CAR_PIN_MOTOR_RIGHT_IN3, 0);
    gpio_set_level(CAR_PIN_MOTOR_RIGHT_IN4, 0);
    gpio_set_level(CAR_PIN_DEBUG, 0);

    /* 2. Configuração do LEDC Timer */
    ledc_timer_config_t timer_conf = {
        .speed_mode = LEDC_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_8_BIT,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = PWM_FREQ_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
        .deconfigure = false
    };
    ESP_ERROR_CHECK(ledc_timer_config(&timer_conf));

    /* 3. Configuração dos Canais LEDC (ENA e ENB) */
    ledc_channel_config_t ch_left = {
        .gpio_num = CAR_PIN_MOTOR_LEFT_PWM,
        .speed_mode = LEDC_SPEED_MODE,
        .channel = LEDC_LEFT_CHANNEL,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER_0,
        .duty = 0,
        .hpoint = 0,
        .sleep_mode = LEDC_SLEEP_MODE_NO_ALIVE_NO_PD,
        .flags = { .output_invert = 0 },
        .deconfigure = false
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ch_left));

    ledc_channel_config_t ch_right = {
        .gpio_num = CAR_PIN_MOTOR_RIGHT_PWM,
        .speed_mode = LEDC_SPEED_MODE,
        .channel = LEDC_RIGHT_CHANNEL,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER_0,
        .duty = 0,
        .hpoint = 0,
        .sleep_mode = LEDC_SLEEP_MODE_NO_ALIVE_NO_PD,
        .flags = { .output_invert = 0 },
        .deconfigure = false
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ch_right));

    /* Inicia em freio seguro */
    motors_brake();

    ESP_LOGI(TAG, "Motores inicializados com sucesso.");
    return ESP_OK;
}

void motors_set(int8_t left_pct, int8_t right_pct)
{
    /* Aplica inversões lógicas se configurado */
    if (INVERT_LEFT) {
        left_pct = -left_pct;
    }
    if (INVERT_RIGHT) {
        right_pct = -right_pct;
    }

    /* Motor Esquerdo */
    if (left_pct > 0) {
        gpio_set_level(CAR_PIN_MOTOR_LEFT_IN1, 1);
        gpio_set_level(CAR_PIN_MOTOR_LEFT_IN2, 0);
        uint32_t duty = pct_to_duty((uint8_t)left_pct);
        ledc_set_duty(LEDC_SPEED_MODE, LEDC_LEFT_CHANNEL, duty);
        ledc_update_duty(LEDC_SPEED_MODE, LEDC_LEFT_CHANNEL);
    } else if (left_pct < 0) {
        gpio_set_level(CAR_PIN_MOTOR_LEFT_IN1, 0);
        gpio_set_level(CAR_PIN_MOTOR_LEFT_IN2, 1);
        uint32_t duty = pct_to_duty((uint8_t)(-left_pct));
        ledc_set_duty(LEDC_SPEED_MODE, LEDC_LEFT_CHANNEL, duty);
        ledc_update_duty(LEDC_SPEED_MODE, LEDC_LEFT_CHANNEL);
    } else {
        gpio_set_level(CAR_PIN_MOTOR_LEFT_IN1, 0);
        gpio_set_level(CAR_PIN_MOTOR_LEFT_IN2, 0);
        ledc_set_duty(LEDC_SPEED_MODE, LEDC_LEFT_CHANNEL, 0);
        ledc_update_duty(LEDC_SPEED_MODE, LEDC_LEFT_CHANNEL);
    }

    /* Motor Direito */
    if (right_pct > 0) {
        gpio_set_level(CAR_PIN_MOTOR_RIGHT_IN3, 1);
        gpio_set_level(CAR_PIN_MOTOR_RIGHT_IN4, 0);
        uint32_t duty = pct_to_duty((uint8_t)right_pct);
        ledc_set_duty(LEDC_SPEED_MODE, LEDC_RIGHT_CHANNEL, duty);
        ledc_update_duty(LEDC_SPEED_MODE, LEDC_RIGHT_CHANNEL);
    } else if (right_pct < 0) {
        gpio_set_level(CAR_PIN_MOTOR_RIGHT_IN3, 0);
        gpio_set_level(CAR_PIN_MOTOR_RIGHT_IN4, 1);
        uint32_t duty = pct_to_duty((uint8_t)(-right_pct));
        ledc_set_duty(LEDC_SPEED_MODE, LEDC_RIGHT_CHANNEL, duty);
        ledc_update_duty(LEDC_SPEED_MODE, LEDC_RIGHT_CHANNEL);
    } else {
        gpio_set_level(CAR_PIN_MOTOR_RIGHT_IN3, 0);
        gpio_set_level(CAR_PIN_MOTOR_RIGHT_IN4, 0);
        ledc_set_duty(LEDC_SPEED_MODE, LEDC_RIGHT_CHANNEL, 0);
        ledc_update_duty(LEDC_SPEED_MODE, LEDC_RIGHT_CHANNEL);
    }

    /* Se ambos chegarem a zero, alterna DEBUG_PIN (Seção 8.6) para medição */
    if (left_pct == 0 && right_pct == 0) {
        s_debug_pin_state = !s_debug_pin_state;
        gpio_set_level(CAR_PIN_DEBUG, s_debug_pin_state);
    }
}

void motors_brake(void)
{
    /* Seção 8.4: IN1 = IN2 = HIGH com EN em duty 255 */
    gpio_set_level(CAR_PIN_MOTOR_LEFT_IN1, 1);
    gpio_set_level(CAR_PIN_MOTOR_LEFT_IN2, 1);
    gpio_set_level(CAR_PIN_MOTOR_RIGHT_IN3, 1);
    gpio_set_level(CAR_PIN_MOTOR_RIGHT_IN4, 1);

    ledc_set_duty(LEDC_SPEED_MODE, LEDC_LEFT_CHANNEL, 255);
    ledc_update_duty(LEDC_SPEED_MODE, LEDC_LEFT_CHANNEL);

    ledc_set_duty(LEDC_SPEED_MODE, LEDC_RIGHT_CHANNEL, 255);
    ledc_update_duty(LEDC_SPEED_MODE, LEDC_RIGHT_CHANNEL);

    /* Alterna DEBUG_PIN indicando parada dos motores */
    s_debug_pin_state = !s_debug_pin_state;
    gpio_set_level(CAR_PIN_DEBUG, s_debug_pin_state);
}
