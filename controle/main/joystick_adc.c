/**
 * @file joystick_adc.c
 * @brief Leitura do joystick pelo ADC1 oneshot (Seção 7, itens 1 a 4)
 */

#include "joystick_adc.h"
#include "config.h"
#include "joystick.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <inttypes.h>

static const char *TAG = "joystick";

static adc_oneshot_unit_handle_t s_adc = NULL;
static adc_channel_t s_ch_x;
static adc_channel_t s_ch_y;
static int s_center_x = (ADC_RAW_MAX + 1) / 2;   /* substituído pela calibração */
static int s_center_y = (ADC_RAW_MAX + 1) / 2;
static volatile uint32_t s_read_fail_cnt = 0;    /* escrito só por quem lê o ADC */

esp_err_t joy_adc_init(void)
{
    adc_unit_t unit_x, unit_y;
    ESP_ERROR_CHECK(adc_oneshot_io_to_channel(CTRL_PIN_JOYSTICK_X, &unit_x, &s_ch_x));
    ESP_ERROR_CHECK(adc_oneshot_io_to_channel(CTRL_PIN_JOYSTICK_Y, &unit_y, &s_ch_y));

    /* Só ADC1: o ADC2 é compartilhado com o Wi-Fi (Seção 5.1) */
    if (unit_x != ADC_UNIT_1 || unit_y != ADC_UNIT_1) {
        ESP_LOGE(TAG, "Joystick precisa estar no ADC1 (GPIO %d -> unidade %d, GPIO %d -> unidade %d)",
                 CTRL_PIN_JOYSTICK_X, (int)unit_x + 1, CTRL_PIN_JOYSTICK_Y, (int)unit_y + 1);
        return ESP_ERR_INVALID_ARG;
    }

    /* Um único handle para a unidade, com os dois canais configurados nele */
    adc_oneshot_unit_init_cfg_t unit_cfg = {
        .unit_id = ADC_UNIT_1,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&unit_cfg, &s_adc));

    adc_oneshot_chan_cfg_t chan_cfg = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,   /* 12 bits no ESP32 (SOC_ADC_RTC_MAX_BITWIDTH) */
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(s_adc, s_ch_x, &chan_cfg));
    ESP_ERROR_CHECK(adc_oneshot_config_channel(s_adc, s_ch_y, &chan_cfg));

    ESP_LOGI(TAG, "ADC1: X no GPIO %d (canal %d), Y no GPIO %d (canal %d)",
             CTRL_PIN_JOYSTICK_X, (int)s_ch_x, CTRL_PIN_JOYSTICK_Y, (int)s_ch_y);
    return ESP_OK;
}

/* Uma amostra, repetindo em ESP_ERR_TIMEOUT (no máximo CTRL_ADC_MAX_RETRIES vezes) */
static esp_err_t read_sample(adc_channel_t ch, int *raw)
{
    esp_err_t err = ESP_FAIL;
    for (int attempt = 0; attempt <= CTRL_ADC_MAX_RETRIES; attempt++) {
        err = adc_oneshot_read(s_adc, ch, raw);
        if (err != ESP_ERR_TIMEOUT) {
            break;
        }
    }
    return err;
}

/* Média de CTRL_ADC_AVG_SAMPLES amostras */
static esp_err_t read_avg(adc_channel_t ch, int *avg)
{
    int sum = 0;
    for (int i = 0; i < CTRL_ADC_AVG_SAMPLES; i++) {
        int raw = 0;
        esp_err_t err = read_sample(ch, &raw);
        if (err != ESP_OK) {
            return err;
        }
        sum += raw;
    }
    *avg = sum / CTRL_ADC_AVG_SAMPLES;
    return ESP_OK;
}

static void check_center(const char *axis, int center)
{
    if (center < CTRL_CENTER_MIN_RAW || center > CTRL_CENTER_MAX_RAW) {
        ESP_LOGW(TAG, "Centro do eixo %s fora do esperado: %d (esperado %d..%d). "
                 "Joystick em repouso? Seguindo com esse valor.",
                 axis, center, CTRL_CENTER_MIN_RAW, CTRL_CENTER_MAX_RAW);
    }
}

void joy_calibrate(void)
{
    ESP_LOGI(TAG, "Calibrando centro: NAO mexa no joystick (%d amostras a cada %d ms)...",
             CTRL_CAL_SAMPLES, CTRL_CAL_INTERVAL_MS);

    int32_t sum_x = 0, sum_y = 0;
    int ok = 0;
    for (int i = 0; i < CTRL_CAL_SAMPLES; i++) {
        int rx = 0, ry = 0;
        if (read_avg(s_ch_x, &rx) == ESP_OK && read_avg(s_ch_y, &ry) == ESP_OK) {
            sum_x += rx;
            sum_y += ry;
            ok++;
        }
        vTaskDelay(pdMS_TO_TICKS(CTRL_CAL_INTERVAL_MS));
    }

    if (ok == 0) {
        ESP_LOGE(TAG, "Calibração falhou (nenhuma leitura do ADC). Usando centro padrão %d.", s_center_x);
        return;
    }

    s_center_x = (int)(sum_x / ok);
    s_center_y = (int)(sum_y / ok);
    ESP_LOGI(TAG, "Centro calibrado: X=%d Y=%d (%d leituras validas)", s_center_x, s_center_y, ok);
    check_center("X", s_center_x);
    check_center("Y", s_center_y);
}

esp_err_t joy_read(int8_t *x, int8_t *y)
{
    int rx = 0, ry = 0;
    if (read_avg(s_ch_x, &rx) != ESP_OK || read_avg(s_ch_y, &ry) != ESP_OK) {
        s_read_fail_cnt++;
        *x = 0;   /* neutro: o carrinho para (RF03) */
        *y = 0;
        return ESP_FAIL;
    }

    /* Escalas separadas para cada lado: o centro físico vira 0 (Seção 7, item 1) */
    int8_t nx = joystick_normalize(rx, s_center_x, 0, ADC_RAW_MAX, INVERT_X);
    int8_t ny = joystick_normalize(ry, s_center_y, 0, ADC_RAW_MAX, INVERT_Y);

    /* Zona morta de DEADZONE_PCT com reescala contínua (RF02) */
    *x = joystick_deadzone(nx, DEADZONE_PCT);
    *y = joystick_deadzone(ny, DEADZONE_PCT);
    return ESP_OK;
}

uint32_t joy_read_fail_count(void)
{
    return s_read_fail_cnt;
}
