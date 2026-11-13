/**
 * @file bench.c
 * @brief Implementação da sequência autônoma do modo de bancada (Seção 15.4)
 */

#include "bench.h"
#include "protocol.h"
#include "crc8.h"
#include "config.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/task.h"
#include <string.h>
#include <inttypes.h>

static const char *TAG = "bench";

#if CONFIG_CAR_BENCH_MODE

typedef struct {
    uint8_t  step_id;
    uint32_t duration_ms;
    int8_t   x;
    int8_t   y;
    const char *desc;
} bench_step_def_t;

static const bench_step_def_t s_bench_steps[] = {
    { 1, 2000,   0,   0, "1. Zero (arma) - 2s" },
    { 2, 3000,   0,  40, "2. Frente suave - 3s" },
    { 3, 2000,   0,   0, "3. Zero - 2s" },
    { 4, 3000,   0, -40, "4. Ré suave - 3s" },
    { 5, 2000,   0,   0, "5. Zero - 2s" },
    { 6, 3000,  40,  60, "6. Curva à direita - 3s" },
    { 7, 2000,   0,   0, "7. Zero - 2s" },
    { 8, 3000,  60,   0, "8. Giro no lugar - 3s" },
    { 9, 2000,   0,   0, "9. Zero - 2s" }
};

#define BENCH_STEP_COUNT (sizeof(s_bench_steps) / sizeof(s_bench_steps[0]))

static QueueHandle_t s_bench_cmd_queue = NULL;

static void bench_task(void *pvParameters)
{
    (void)pvParameters;
    ESP_LOGI(TAG, "Aguardando 3s antes de iniciar a sequência de bancada...");
    vTaskDelay(pdMS_TO_TICKS(3000));

    uint16_t seq = 1;

    while (1) {
        for (size_t s = 0; s < BENCH_STEP_COUNT; s++) {
            const bench_step_def_t *step = &s_bench_steps[s];
            uint32_t now_ms = (uint32_t)(esp_timer_get_time() / 1000);
            ESP_LOGI(TAG, "[%" PRIu32 " ms] EVT bench_step %d (%s)", now_ms, step->step_id, step->desc);

            uint32_t elapsed = 0;
            while (elapsed < step->duration_ms) {
                cmd_packet_t pkt;
                pkt.magic = PROTO_MAGIC;
                pkt.type = PKT_CMD;
                pkt.seq = seq++;
                pkt.x = step->x;
                pkt.y = step->y;
                pkt.crc = crc8_calc((const uint8_t *)&pkt, sizeof(cmd_packet_t) - 1);

                /* Empacota como rx_packet_t para a mesma fila do ESP-NOW */
                rx_packet_t rx_pkt;
                memset(&rx_pkt, 0, sizeof(rx_pkt));
                memcpy(rx_pkt.data, &pkt, sizeof(pkt));
                rx_pkt.len = sizeof(pkt);
                rx_pkt.rx_ms = (uint32_t)(esp_timer_get_time() / 1000);

                /* Injeta o pacote na mesma fila do laço de controle */
                if (s_bench_cmd_queue != NULL) {
                    xQueueSend(s_bench_cmd_queue, &rx_pkt, 0);
                }

                vTaskDelay(pdMS_TO_TICKS(CMD_PERIOD_MS)); // 20 Hz
                elapsed += CMD_PERIOD_MS;
            }
        }
    }
}

esp_err_t bench_mode_start(QueueHandle_t cmd_queue)
{
    s_bench_cmd_queue = cmd_queue;
    BaseType_t res = xTaskCreate(bench_task, "bench_task", 3072, NULL, 4, NULL);
    if (res != pdPASS) {
        ESP_LOGE(TAG, "Falha ao criar bench_task");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "Modo de bancada inicializado com sucesso.");
    return ESP_OK;
}

#endif // CONFIG_CAR_BENCH_MODE
