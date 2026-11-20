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
#include "protocol.h"

static const char *TAG = "sonar";

static SemaphoreHandle_t s_echo_sem = NULL;
static QueueHandle_t s_sonar_out_queue = NULL;
static volatile int64_t s_echo_start_us = 0;
static volatile uint32_t s_echo_width_us = 0;
/* true quando o ECHO subiu depois do último disparo: o sensor respondeu */
static volatile bool s_echo_started = false;

static void IRAM_ATTR echo_isr_handler(void *arg)
{
    (void)arg;
    int level = gpio_get_level(CAR_PIN_HCSR04_ECHO);
    if (level == 1) {
        s_echo_start_us = esp_timer_get_time();
        s_echo_started = true;
    } else if (s_echo_started) {
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

    /* Período fixo de SENSOR_PERIOD_MS (Seção 8.1), independente do tempo de
     * espera do eco. xTaskDelayUntil substitui vTaskDelayUntil no v6. */
    TickType_t last_wake = xTaskGetTickCount();
    uint32_t busy_cycles = 0;   /* ciclos seguidos com o ECHO ainda alto antes do disparo */

    while (1) {
        sonar_reading_t reading;
        reading.timestamp_ms = (uint32_t)(esp_timer_get_time() / 1000);

        /* ECHO ainda alto: pulso longo do ciclo anterior (nada no alcance;
         * alguns módulos seguram o ECHO por até ~200 ms). Não dispara e conta
         * como livre. Alto por mais de SENSOR_ECHO_BUSY_MAX_CYCLES ciclos
         * seguidos = ECHO preso, tratado como sensor inválido. */
        if (gpio_get_level(CAR_PIN_HCSR04_ECHO) == 1) {
            busy_cycles++;
            if (busy_cycles <= SENSOR_ECHO_BUSY_MAX_CYCLES) {
                reading.distance_cm = SENSOR_NO_ECHO_FAR_CM;
                reading.echo_ok = true;
            } else {
                reading.distance_cm = PROTO_DIST_INVALID;
                reading.echo_ok = false;
            }
            if (s_sonar_out_queue != NULL) {
                xQueueOverwrite(s_sonar_out_queue, &reading);
            }
            xTaskDelayUntil(&last_wake, pdMS_TO_TICKS(SENSOR_PERIOD_MS));
            continue;
        }
        busy_cycles = 0;

        /* Garante que o semáforo está limpo antes do disparo */
        xSemaphoreTake(s_echo_sem, 0);
        s_echo_started = false;

        /* Pulso de TRIG sem esp_rom_delay_us (API interna no v6.1, RNF05) */
        int64_t t0 = esp_timer_get_time();
        gpio_set_level(CAR_PIN_HCSR04_TRIG, 1);
        while ((esp_timer_get_time() - t0) < HCSR04_TRIG_PULSE_US) {
            /* Espera ativa curta (10 µs) */
        }
        gpio_set_level(CAR_PIN_HCSR04_TRIG, 0);

        /* Aguarda a descida do ECHO com timeout de SENSOR_ECHO_TIMEOUT_MS */
        if (xSemaphoreTake(s_echo_sem, pdMS_TO_TICKS(SENSOR_ECHO_TIMEOUT_MS)) == pdTRUE) {
            uint32_t width_us = s_echo_width_us;
            if (width_us > (uint32_t)SENSOR_VALID_MAX_CM * HCSR04_US_PER_CM) {
                /* Pulso mais longo que o alcance: nada no alcance, livre */
                reading.distance_cm = SENSOR_NO_ECHO_FAR_CM;
            } else {
                reading.distance_cm = (uint16_t)(width_us / HCSR04_US_PER_CM);
            }
            reading.echo_ok = true;
        } else if (s_echo_started) {
            /* O ECHO subiu, mas o eco não voltou a tempo: nada no alcance, livre */
            reading.distance_cm = SENSOR_NO_ECHO_FAR_CM;
            reading.echo_ok = true;
        } else {
            /* O ECHO nem subiu: sensor desligado ou mal ligado (inválido, RF04) */
            reading.distance_cm = PROTO_DIST_INVALID;
            reading.echo_ok = false;
        }

        /* Publica na fila do laço de controle (sobrescrevendo valor anterior) */
        if (s_sonar_out_queue != NULL) {
            xQueueOverwrite(s_sonar_out_queue, &reading);
        }

        xTaskDelayUntil(&last_wake, pdMS_TO_TICKS(SENSOR_PERIOD_MS));
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
    /* ESP_ERR_INVALID_STATE = serviço já instalado por outro módulo: aceitável */
    esp_err_t isr_err = gpio_install_isr_service(0);
    if (isr_err != ESP_OK && isr_err != ESP_ERR_INVALID_STATE) {
        ESP_ERROR_CHECK(isr_err);
    }
    ESP_ERROR_CHECK(gpio_isr_handler_add(CAR_PIN_HCSR04_ECHO, echo_isr_handler, NULL));

    /* Criação da tarefa do sensor com prioridade média */
    BaseType_t res = xTaskCreate(sonar_task, "sonar_task", CAR_TASK_STACK, NULL, CAR_SONAR_TASK_PRIO, NULL);
    if (res != pdPASS) {
        ESP_LOGE(TAG, "Falha ao criar sonar_task");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "HC-SR04 inicializado com sucesso.");
    return ESP_OK;
}
