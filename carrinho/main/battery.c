/**
 * @file battery.c
 * @brief Implementação da medição da tensão da bateria
 */

#include "battery.h"
#include "config.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_log.h"

static const char *TAG = "battery";

static adc_oneshot_unit_handle_t s_adc_handle = NULL;
static adc_cali_handle_t s_cali_handle = NULL;
static adc_channel_t s_batt_channel;
static adc_unit_t s_batt_unit;
static bool s_calibrated = false;

esp_err_t battery_init(void)
{
    ESP_LOGI(TAG, "Inicializando medição de bateria no GPIO %d...", CAR_PIN_BATT_ADC);

    /* Obtém a unidade e o canal ADC1 a partir do pino */
    ESP_ERROR_CHECK(adc_oneshot_io_to_channel(CAR_PIN_BATT_ADC, &s_batt_unit, &s_batt_channel));

    adc_oneshot_unit_init_cfg_t init_cfg = {
        .unit_id = s_batt_unit,
        .ulp_mode = ADC_ULP_MODE_DISABLE
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_cfg, &s_adc_handle));

    adc_oneshot_chan_cfg_t chan_cfg = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(s_adc_handle, s_batt_channel, &chan_cfg));

    /* Inicialização do esquema de calibração Line Fitting */
#if ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    adc_cali_line_fitting_config_t cali_cfg = {
        .unit_id = s_batt_unit,
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .default_vref = 0
    };
    esp_err_t cal_res = adc_cali_create_scheme_line_fitting(&cali_cfg, &s_cali_handle);
    if (cal_res == ESP_OK) {
        s_calibrated = true;
        ESP_LOGI(TAG, "Calibração ADC Line Fitting ativada com sucesso");
    } else {
        ESP_LOGW(TAG, "eFuse não gravado para calibração, utilizando conversão aproximada");
    }
#endif

    return ESP_OK;
}

uint16_t battery_read_mv(void)
{
    if (s_adc_handle == NULL) {
        return 0;
    }

    int raw_accum = 0;
    int samples = 0;

    for (int i = 0; i < 8; i++) {
        int raw = 0;
        esp_err_t err = adc_oneshot_read(s_adc_handle, s_batt_channel, &raw);
        if (err == ESP_OK) {
            raw_accum += raw;
            samples++;
        }
    }

    if (samples == 0) {
        return 0;
    }

    int raw_avg = raw_accum / samples;
    int mv = 0;

    if (s_calibrated && s_cali_handle != NULL) {
        adc_cali_raw_to_voltage(s_cali_handle, raw_avg, &mv);
    } else {
        /* Aproximação 0..4095 para 0..3300 mV */
        mv = (raw_avg * 3300) / 4095;
    }

    /* Aplica o divisor de tensão: V_bat = V_pin * (BATT_DIV_RATIO_X1000 / 1000) */
    uint32_t v_bat = ((uint32_t)mv * BATT_DIV_RATIO_X1000) / 1000;
    return (uint16_t)v_bat;
}
