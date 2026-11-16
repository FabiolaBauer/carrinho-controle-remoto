/**
 * @file motors.c
 * @brief Implementação do driver de motores com LEDC PWM e GPIOs
 */

#include "motors.h"
#include "protocol.h"
#include "driver/ledc.h"
#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "motors";

#define LEDC_SPEED_MODE      LEDC_LOW_SPEED_MODE
#define LEDC_LEFT_CHANNEL    LEDC_CHANNEL_0
#define LEDC_RIGHT_CHANNEL   LEDC_CHANNEL_1

static uint32_t pct_to_duty(uint8_t abs_pct)
{
    if (abs_pct == 0) {
        return 0;
    }
    if (abs_pct > PROTO_AXIS_MAX) {
        abs_pct = PROTO_AXIS_MAX;
    }

    /* Seção 8.4: módulo 1..100% vira PWM_MIN_PCT..MAX_PWM_PCT; 0 continua 0.
     * O limite MAX_PWM_PCT é aplicado na saída, para que 100% do joystick
     * corresponda exatamente a MAX_PWM_PCT. */
    uint32_t scaled_pct = PWM_MIN_PCT +
        ((uint32_t)(abs_pct - 1) * (MAX_PWM_PCT - PWM_MIN_PCT)) / (PROTO_AXIS_MAX - 1);

    return (scaled_pct * PWM_DUTY_MAX) / PROTO_AXIS_MAX;
}

esp_err_t motors_init(void)
{
    ESP_LOGI(TAG, "Inicializando GPIOs e LEDC para controle de motores...");

    /* 1. Configuração dos GPIOs de sentido com pull-down para segurança no boot */
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << CAR_PIN_MOTOR_LEFT_IN1) |
                        (1ULL << CAR_PIN_MOTOR_LEFT_IN2) |
                        (1ULL << CAR_PIN_MOTOR_RIGHT_IN3) |
                        (1ULL << CAR_PIN_MOTOR_RIGHT_IN4),
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

    /* 2. Configuração do LEDC Timer.
     * LEDC_AUTO_CLK existe no v6.1 para o esp32 (soc/esp32/include/soc/clk_tree_defs.h).
     * O campo intr_type de ledc_channel_config_t está deprecated no v6.1 e não é usado. */
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
}

void motors_brake(void)
{
    /* Seção 8.4: IN1 = IN2 = HIGH com EN em duty máximo (PWM_DUTY_MAX) */
    gpio_set_level(CAR_PIN_MOTOR_LEFT_IN1, 1);
    gpio_set_level(CAR_PIN_MOTOR_LEFT_IN2, 1);
    gpio_set_level(CAR_PIN_MOTOR_RIGHT_IN3, 1);
    gpio_set_level(CAR_PIN_MOTOR_RIGHT_IN4, 1);

    ledc_set_duty(LEDC_SPEED_MODE, LEDC_LEFT_CHANNEL, PWM_DUTY_MAX);
    ledc_update_duty(LEDC_SPEED_MODE, LEDC_LEFT_CHANNEL);

    ledc_set_duty(LEDC_SPEED_MODE, LEDC_RIGHT_CHANNEL, PWM_DUTY_MAX);
    ledc_update_duty(LEDC_SPEED_MODE, LEDC_RIGHT_CHANNEL);
}
