/**
 * @file sonar.c
 * @brief Implementação do sensor HC-SR04 conforme Seção 8.1
 */

#include "sonar.h"
#include "config.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

static const char *TAG = "sonar";

static SemaphoreHandle_t s_echo_sem = NULL;
static QueueHandle_t s_sonar_out_queue = NULL;
static volatile int64_t s_echo_start_us = 0;
static volatile uint32_t s_echo_width_us = 0;

static void IRAM_ATTR echo_isr_handler(void *arg)
{
    (void)arg;
    int level = gpio_get_level(CAR_PIN_HCSR04_ECHO);
    if (level == 1) {
        s_echo_start_us = esp_timer_get_time();
    } else {
        int64_t now_us = esp_timer_get_time();
        if (now_us > s_echo_start_us) {
            s_echo_width_us = (uint32_t)(now_us - s_echo_start_us);
            BaseType_t high_task_wakeup = pdFALSE;
            xSemaphoreGiveFromISR(s_echo_sem, &high_task_wakeup);
            if (high_task_wakeup) {
                portYIELD_FROM_ISR();
            }
        }
    }
}

static void sonar_task(void *pvParameters)
{
    (void)pvParameters;
    ESP_LOGI(TAG, "Tarefa do sensor HC-SR04 iniciada (Período: %d ms)", SENSOR_PERIOD_MS);

    while (1) {
        /* Garante que o semáforo está limpo antes do disparo */
        xSemaphoreTake(s_echo_sem, 0);

        /* Pulso de TRIG de 10 µs sem esp_rom_delay_us (RNF05) */
        int64_t t0 = esp_timer_get_time();
        gpio_set_level(CAR_PIN_HCSR04_TRIG, 1);
        while ((esp_timer_get_time() - t0) < 10) {
            /* Espera ativa de 10 µs */
        }
        gpio_set_level(CAR_PIN_HCSR04_TRIG, 0);

        /* Aguarda eco com timeout de 30 ms */
        sonar_reading_t reading;
        reading.timestamp_ms = (uint32_t)(esp_timer_get_time() / 1000);

        if (xSemaphoreTake(s_echo_sem, pdMS_TO_TICKS(SENSOR_ECHO_TIMEOUT_MS)) == pdTRUE) {
            reading.distance_cm = (uint16_t)(s_echo_width_us / 58);
            reading.echo_ok = true;
        } else {
            /* Timeout: sem eco */
            reading.distance_cm = 0xFFFF;
            reading.echo_ok = false;
        }

        /* Publica na fila do laço de controle (sobrescrevendo valor anterior) */
        if (s_sonar_out_queue != NULL) {
            xQueueOverwrite(s_sonar_out_queue, &reading);
        }

        vTaskDelay(pdMS_TO_TICKS(SENSOR_PERIOD_MS));
    }
}

esp_err_t sonar_init(QueueHandle_t out_queue)
{
    s_sonar_out_queue = out_queue;
    s_echo_sem = xSemaphoreCreateBinary();
    if (s_echo_sem == NULL) {
        ESP_LOGE(TAG, "Falha ao criar semáforo de eco");
        return ESP_ERR_NO_MEM;
    }

    /* Configuração do pino TRIG como saída */
    gpio_config_t trig_conf = {
        .pin_bit_mask = (1ULL << CAR_PIN_HCSR04_TRIG),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    ESP_ERROR_CHECK(gpio_config(&trig_conf));
    gpio_set_level(CAR_PIN_HCSR04_TRIG, 0);

    /* Configuração do pino ECHO com interrupção em qualquer borda */
    gpio_config_t echo_conf = {
        .pin_bit_mask = (1ULL << CAR_PIN_HCSR04_ECHO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_ANYEDGE
    };
    ESP_ERROR_CHECK(gpio_config(&echo_conf));

    /* Instalação do serviço de ISR de GPIO e registro do handler */
    gpio_install_isr_service(0);
    ESP_ERROR_CHECK(gpio_isr_handler_add(CAR_PIN_HCSR04_ECHO, echo_isr_handler, NULL));

    /* Criação da tarefa do sensor com prioridade média */
    BaseType_t res = xTaskCreate(sonar_task, "sonar_task", 3072, NULL, 5, NULL);
    if (res != pdPASS) {
        ESP_LOGE(TAG, "Falha ao criar sonar_task");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "HC-SR04 inicializado com sucesso.");
    return ESP_OK;
}
