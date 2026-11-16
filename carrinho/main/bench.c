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

/* Todo o conteúdo só existe no build de bancada (Seção 15.4) */
#if CONFIG_CAR_BENCH_MODE

static const char *TAG = "bench";

typedef struct {
    uint8_t  step_id;
    uint32_t duration_ms;
    int8_t   x;
    int8_t   y;
    const char *desc;
} bench_step_def_t;

static const bench_step_def_t s_bench_steps[] = {
    /* Tabela da Seção 15.4: x e y são os valores do próprio ensaio */
    { 1, BENCH_ZERO_MS,   0,   0, "1. Zero (arma)" },
    { 2, BENCH_MOVE_MS,   0,  40, "2. Frente suave" },
    { 3, BENCH_ZERO_MS,   0,   0, "3. Zero" },
    { 4, BENCH_MOVE_MS,   0, -40, "4. Re suave" },
    { 5, BENCH_ZERO_MS,   0,   0, "5. Zero" },
    { 6, BENCH_MOVE_MS,  40,  60, "6. Curva a direita" },
    { 7, BENCH_ZERO_MS,   0,   0, "7. Zero" },
    { 8, BENCH_MOVE_MS,  60,   0, "8. Giro no lugar" },
    { 9, BENCH_ZERO_MS,   0,   0, "9. Zero" }
};

#define BENCH_STEP_COUNT (sizeof(s_bench_steps) / sizeof(s_bench_steps[0]))

static QueueHandle_t s_bench_cmd_queue = NULL;

static void bench_task(void *pvParameters)
{
    (void)pvParameters;
    ESP_LOGI(TAG, "Aguardando %d ms antes de iniciar a sequência de bancada...", BENCH_START_DELAY_MS);
    vTaskDelay(pdMS_TO_TICKS(BENCH_START_DELAY_MS));

    uint16_t seq = 1;
    TickType_t last_wake = xTaskGetTickCount();

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
                    xQueueSend(s_bench_cmd_queue, &rx_pkt, 0);   /* não bloqueia; fila cheia = pacote perdido */
                }

                xTaskDelayUntil(&last_wake, pdMS_TO_TICKS(CMD_PERIOD_MS)); // 20 Hz, como o controle
                elapsed += CMD_PERIOD_MS;
            }
        }
    }
}

esp_err_t bench_mode_start(QueueHandle_t cmd_queue)
{
    s_bench_cmd_queue = cmd_queue;
    BaseType_t res = xTaskCreate(bench_task, "bench_task", CAR_TASK_STACK, NULL, CAR_BENCH_TASK_PRIO, NULL);
    if (res != pdPASS) {
        ESP_LOGE(TAG, "Falha ao criar bench_task");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "Modo de bancada inicializado com sucesso.");
    return ESP_OK;
}

#endif // CONFIG_CAR_BENCH_MODE
