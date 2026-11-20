/**
 * @file ui.c
 * @brief Tarefa de UI do controle: telemetria, log e LED (Seção 7, itens 6 e 7)
 *
 * Todo o estado da UI é lido e escrito só por esta tarefa. O callback
 * ESP-NOW apenas copia os pacotes para a fila.
 */

#include "ui.h"
#include "sdkconfig.h"
#include "config.h"
#include "protocol.h"
#include "telem.h"
#include "led_pattern.h"
#include "radio.h"
#include "joystick_adc.h"
#include "t4.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "freertos/task.h"
#include <inttypes.h>

static const char *TAG = "ui";

static QueueHandle_t s_queue = NULL;
static const uint8_t s_car_mac[6] = CAR_MAC;

static uint32_t now_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000);
}

static void log_telem(const telem_packet_t *t)
{
    if (t->distance_cm == PROTO_DIST_INVALID) {
        ESP_LOGI(TAG, "estado=%s dist=invalido bat=%" PRIu16 "mV rej=%" PRIu16,
                 telem_state_name(t->state), t->battery_mv, t->rejected_cnt);
    } else {
        ESP_LOGI(TAG, "estado=%s dist=%" PRIu16 "cm bat=%" PRIu16 "mV rej=%" PRIu16,
                 telem_state_name(t->state), t->distance_cm, t->battery_mv, t->rejected_cnt);
    }
}

static void log_stats(uint32_t invalid_telem)
{
    radio_stats_t st;
    radio_get_stats(&st);
    ESP_LOGI(TAG, "envio: enviados=%" PRIu32 " ok=%" PRIu32 " falhas=%" PRIu32
             " erros=%" PRIu32 " pulados=%" PRIu32 " destravados=%" PRIu32
             " | telem: invalidas=%" PRIu32 " fila_cheia=%" PRIu32 " | adc_falhas=%" PRIu32,
             st.sent, st.cb_ok, st.cb_fail, st.send_err, st.skipped, st.unlocked,
             invalid_telem, st.rx_queue_full, joy_read_fail_count());
}

static void ui_task(void *arg)
{
    (void)arg;
    bool     ever_received = false;
    uint8_t  last_state = ST_INIT;
    uint32_t last_telem_ms = 0;
    uint32_t invalid_telem = 0;
    uint32_t start_ms = now_ms();
    uint32_t last_no_telem_log_ms = start_ms;
    uint32_t last_stats_ms = start_ms;
#if CONFIG_FW_TEST_T4
    t4_step_t logged_t4 = T4_WAIT;
#endif
#if CONFIG_CTRL_LED_JOYSTICK
    uint32_t last_joy_log_ms = start_ms;
#endif

    while (1) {
        /* Espera telemetria por até CTRL_UI_PERIOD_MS, depois esvazia a fila */
        telem_rx_t item;
        TickType_t wait = pdMS_TO_TICKS(CTRL_UI_PERIOD_MS);
        while (xQueueReceive(s_queue, &item, wait) == pdTRUE) {
            wait = 0;
            telem_packet_t t;
            telem_verdict_t v = telem_check(s_car_mac, item.src_mac, item.data, item.len, &t);
            if (v == TELEM_OK) {
                ever_received = true;
                last_state = t.state;
                last_telem_ms = now_ms();
                log_telem(&t);
            } else {
                invalid_telem++;
                ESP_LOGW(TAG, "Telemetria descartada (motivo %d, %u bytes)", (int)v, (unsigned)item.len);
            }
        }

        uint32_t now = now_ms();

        /* Telemetria recente: até CTRL_TELEM_LOST_MS desde a última válida */
        bool has_telem = ever_received && (now - last_telem_ms) <= CTRL_TELEM_LOST_MS;

        /* "SEM TELEMETRIA" uma vez por segundo, depois de mais de 1 s sem telemetria
         * (contando do boot enquanto nada chegou) */
        uint32_t ref = ever_received ? last_telem_ms : start_ms;
        if ((now - ref) > CTRL_TELEM_LOST_MS && (now - last_no_telem_log_ms) >= CTRL_NO_TELEM_LOG_MS) {
            ESP_LOGW(TAG, "SEM TELEMETRIA");
            last_no_telem_log_ms = now;
        }

#if CONFIG_CTRL_LED_JOYSTICK
        /* Diagnóstico: LED aceso com o joystick fora do centro (x ou y != 0) */
        (void)has_telem;    /* o LED não mostra o estado neste build */
        (void)last_state;
        int8_t jx = 0, jy = 0;
        joy_last_values(&jx, &jy);
        gpio_set_level(CTRL_PIN_LED, (jx != 0 || jy != 0) ? 1 : 0);
        if ((now - last_joy_log_ms) >= CTRL_JOY_LOG_MS) {
            ESP_LOGI(TAG, "joystick: x=%d y=%d", (int)jx, (int)jy);
            last_joy_log_ms = now;
        }
#else
        /* LED de estado (Seção 7, item 7) */
        gpio_set_level(CTRL_PIN_LED, led_pattern_level(has_telem, last_state, now) ? 1 : 0);
#endif

        if ((now - last_stats_ms) >= CTRL_STATS_LOG_MS) {
            log_stats(invalid_telem);
            last_stats_ms = now;
        }

#if CONFIG_FW_TEST_T4
        t4_step_t step = t4_current_step();
        if (step != logged_t4) {
            ESP_LOGW(TAG, "[%" PRIu32 " ms] T4: %s", now, t4_step_name(step));
            logged_t4 = step;
        }
#endif
    }
}

esp_err_t ui_start(QueueHandle_t telem_queue)
{
    s_queue = telem_queue;

    gpio_config_t io = {
        .pin_bit_mask = (1ULL << CTRL_PIN_LED),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&io));
    gpio_set_level(CTRL_PIN_LED, 0);   /* sem telemetria: apagado */

    if (xTaskCreate(ui_task, "ui_task", CTRL_UI_TASK_STACK, NULL, CTRL_UI_TASK_PRIO, NULL) != pdPASS) {
        ESP_LOGE(TAG, "Falha ao criar ui_task");
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}
