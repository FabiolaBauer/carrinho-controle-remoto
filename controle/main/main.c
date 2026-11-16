/**
 * @file main.c
 * @brief Firmware do Controle (ESP32 #1), Seção 7
 *
 * Só faz E/S: boot, calibração e o timer de envio a 20 Hz. A decisão
 * (normalização, zona morta, validação de telemetria, LED) está em
 * components/logic.
 * Atende RF01 (envio a 20 Hz com seq), RF02 (zona morta) e RF06 (teste T4).
 */

#include <string.h>
#include <inttypes.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "nvs_flash.h"

#include "config.h"
#include "protocol.h"
#include "crc8.h"
#include "joystick_adc.h"
#include "radio.h"
#include "ui.h"
#include "t4.h"

static const char *TAG = "ctrl_main";

static esp_timer_handle_t s_send_timer = NULL;
static uint16_t s_seq = 0;   /* escrito só pelo timer de envio */

/* =========================================================================
 * Envio a 20 Hz (Seção 7, item 5). Roda na tarefa do esp_timer:
 * curto e sem bloqueio. Envia SEMPRE, mesmo com x = y = 0 (sinal de vida).
 * ========================================================================= */
static void send_timer_cb(void *arg)
{
    (void)arg;

    /* Não envia antes do callback do envio anterior: pula e conta (Seção 6) */
    if (!radio_can_send()) {
        return;
    }

    int8_t x = 0, y = 0;
    (void)joy_read(&x, &y);   /* em falha do ADC devolve neutro (0, 0) e conta */

    cmd_packet_t pkt;
    pkt.magic = PROTO_MAGIC;
    pkt.type = PKT_CMD;
    pkt.seq = s_seq++;        /* incrementa a cada pacote, com wraparound */
    pkt.x = x;
    pkt.y = y;
    pkt.crc = crc8_calc((const uint8_t *)&pkt, sizeof(pkt) - 1);

#if CONFIG_FW_TEST_T4
    t4_apply((uint32_t)(esp_timer_get_time() / 1000), &pkt);
#endif

    (void)radio_send_cmd(&pkt);   /* falha só é contada; nunca bloqueia */
}

void app_main(void)
{
    ESP_LOGI(TAG, "=== Firmware do Controle (ESP-IDF v6.1) ===");
#if CONFIG_FW_TEST_T4
    ESP_LOGW(TAG, "****************************************************");
    ESP_LOGW(TAG, "BUILD DE TESTE T4 (CONFIG_FW_TEST_T4=y)");
    ESP_LOGW(TAG, "Em %d ms envia a sequencia invalida e 3 s de frente maxima.", T4_START_DELAY_MS);
    ESP_LOGW(TAG, "ATENCAO: carrinho com as rodas fora do chao!");
    ESP_LOGW(TAG, "****************************************************");
#endif

    /* 1. NVS (usado pelo Wi-Fi) */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    /* 2. Wi-Fi STA + ESP-NOW (mesma ordem e canal do carrinho) */
    QueueHandle_t telem_queue = xQueueCreate(CTRL_TELEM_QUEUE_LEN, sizeof(telem_rx_t));
    if (telem_queue == NULL) {
        ESP_LOGE(TAG, "Falha ao criar fila de telemetria");
        ESP_ERROR_CHECK(ESP_ERR_NO_MEM);
    }
    ESP_ERROR_CHECK(radio_init(telem_queue));

    /* 3. ADC e calibração do centro (joystick em repouso) */
    ESP_ERROR_CHECK(joy_adc_init());
    joy_calibrate();

    /* 4. UI: telemetria, log e LED */
    ESP_ERROR_CHECK(ui_start(telem_queue));

    /* 5. Timer periódico de envio (CMD_PERIOD_MS, ESP_TIMER_TASK) */
    esp_timer_create_args_t args = {
        .callback = &send_timer_cb,
        .arg = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "send_timer",
        .skip_unhandled_events = true,
    };
    ESP_ERROR_CHECK(esp_timer_create(&args, &s_send_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(s_send_timer, (uint64_t)CMD_PERIOD_MS * 1000));

    ESP_LOGI(TAG, "Controle operacional: enviando a cada %d ms.", CMD_PERIOD_MS);
}
