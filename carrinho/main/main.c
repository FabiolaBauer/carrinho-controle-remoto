/**
 * @file main.c
 * @brief Firmware principal do Carrinho (ESP32 #2)
 *
 * Implementa controle de motores, sensor ultrassônico HC-SR04, leitura de bateria,
 * protocolo ESP-NOW / modo de bancada e máquina de estados determinística.
 * Atende aos requisitos RF01 a RF07 e RNF01 a RNF05.
 */

#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_mac.h"
#include "nvs_flash.h"

#include "protocol.h"
#include "crc8.h"
#include "config.h"
#include "joystick.h"
#include "mixer.h"
#include "ramp.h"
#include "link.h"
#include "state_machine.h"
#include "motors.h"
#include "sonar.h"
#include "battery.h"
#include "bench.h"

#if !CONFIG_CAR_BENCH_MODE
#include "esp_wifi.h"
#include "esp_now.h"
#include "esp_netif.h"
#include "esp_event.h"
#endif

static const char *TAG = "car_main";

/* Estrutura para fila de pacotes recebidos do ESP-NOW */
typedef struct {
    uint8_t src_mac[6];
    uint8_t data[sizeof(cmd_packet_t)];
    size_t  len;
    uint32_t rx_ms;
} rx_packet_t;

/* Estrutura para fila de eventos de log assíncrono (Seção 8.6) */
typedef enum {
    EVT_STATE_CHANGE,
    EVT_LINK_LOST,
    EVT_CENTER_DETECTED,
    EVT_OBSTACLE_ENTER,
    EVT_MOTORS_ZERO,
    EVT_TELEM_LOG
} log_event_type_t;

typedef struct {
    log_event_type_t type;
    uint32_t timestamp_ms;
    car_state_t prev_state;
    car_state_t curr_state;
    uint16_t dist_cm;
    uint16_t batt_mv;
    uint16_t rejected_cnt;
} log_event_t;

/* Filas FreeRTOS */
static QueueHandle_t s_rx_queue = NULL;
static QueueHandle_t s_sonar_queue = NULL;
static QueueHandle_t s_log_queue = NULL;

/* Estado global do controle */
static link_rx_t    s_link_rx;
static car_sm_t     s_sm;
static ramp_t       s_ramp_left;
static ramp_t       s_ramp_right;
static esp_timer_handle_t s_control_timer = NULL;

static uint8_t s_controller_mac[6] = CONTROLLER_MAC;
static uint16_t s_latest_battery_mv = 0;
static bool s_prev_link_ok = true;
static bool s_prev_zero_state = false;

/* =========================================================================
 * Callbacks do ESP-NOW (somente no modo normal)
 * ========================================================================= */
#if !CONFIG_CAR_BENCH_MODE

static void on_esp_now_recv(const esp_now_recv_info_t *info, const uint8_t *data, int data_len)
{
    if (info == NULL || data == NULL || data_len <= 0) {
        return;
    }

    rx_packet_t pkt;
    if (info->src_addr != NULL) {
        memcpy(pkt.src_mac, info->src_addr, 6);
    } else {
        memset(pkt.src_mac, 0, 6);
    }

    size_t copy_len = (size_t)data_len;
    if (copy_len > sizeof(pkt.data)) {
        copy_len = sizeof(pkt.data);
    }
    memcpy(pkt.data, data, copy_len);
    pkt.len = (size_t)data_len;
    pkt.rx_ms = (uint32_t)(esp_timer_get_time() / 1000);

    /* Envia para a fila sem bloquear; se cheia, descarta e contabiliza */
    if (s_rx_queue != NULL) {
        if (xQueueSend(s_rx_queue, &pkt, 0) != pdTRUE) {
            s_link_rx.rejected_cnt++;
        }
    }
}

static void on_esp_now_send(const esp_now_send_info_t *tx_info, esp_now_send_status_t status)
{
    (void)tx_info;
    (void)status;
    /* O callback de envio apenas registra falhas se necessário, sem bloquear */
}

static esp_err_t wifi_esp_now_init(void)
{
    ESP_LOGI(TAG, "Inicializando Wi-Fi STA e ESP-NOW...");
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_set_channel(CHANNEL, WIFI_SECOND_CHAN_NONE));

    uint8_t my_mac[6];
    esp_wifi_get_mac(WIFI_IF_STA, my_mac);
    ESP_LOGI(TAG, "MAC do Carrinho: " MACSTR, MAC2STR(my_mac));
    ESP_LOGI(TAG, "Canal Wi-Fi: %d", CHANNEL);

    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_recv_cb(on_esp_now_recv));
    ESP_ERROR_CHECK(esp_now_register_send_cb(on_esp_now_send));

    /* Adiciona o controle como peer */
    esp_now_peer_info_t peer;
    memset(&peer, 0, sizeof(peer));
    memcpy(peer.peer_addr, s_controller_mac, 6);
    peer.channel = 0; // canal atual
    peer.ifidx = WIFI_IF_STA;
    peer.encrypt = false;
    ESP_ERROR_CHECK(esp_now_add_peer(&peer));
    ESP_LOGI(TAG, "Peer do Controle configurado: " MACSTR, MAC2STR(s_controller_mac));

    return ESP_OK;
}

#endif // !CONFIG_CAR_BENCH_MODE

/* =========================================================================
 * Tarefa assíncrona de Log (Seção 8.6)
 * ========================================================================= */
static const char *state_name(car_state_t st)
{
    switch (st) {
        case ST_INIT:      return "INIT";
        case ST_PARADO:    return "PARADO";
        case ST_OPERANDO:  return "OPERANDO";
        case ST_OBSTACULO: return "OBSTACULO";
        case ST_FALHA_COM: return "FALHA_COM";
        default:           return "DESCONHECIDO";
    }
}

static void log_task(void *pvParameters)
{
    (void)pvParameters;
    log_event_t evt;

    while (1) {
        if (xQueueReceive(s_log_queue, &evt, portMAX_DELAY) == pdTRUE) {
            switch (evt.type) {
                case EVT_STATE_CHANGE:
                    ESP_LOGI(TAG, "[%" PRIu32 " ms] EVT state %s->%s",
                             evt.timestamp_ms,
                             state_name(evt.prev_state),
                             state_name(evt.curr_state));
                    break;
                case EVT_LINK_LOST:
                    ESP_LOGW(TAG, "[%" PRIu32 " ms] EVT link_lost", evt.timestamp_ms);
                    break;
                case EVT_CENTER_DETECTED:
                    ESP_LOGD(TAG, "[%" PRIu32 " ms] EVT center_detected", evt.timestamp_ms);
                    break;
                case EVT_OBSTACLE_ENTER:
                    ESP_LOGW(TAG, "[%" PRIu32 " ms] EVT obstacle_enter dist=%" PRIu16 " cm",
                             evt.timestamp_ms, evt.dist_cm);
                    break;
                case EVT_MOTORS_ZERO:
                    ESP_LOGD(TAG, "[%" PRIu32 " ms] EVT motors_zero", evt.timestamp_ms);
                    break;
                case EVT_TELEM_LOG:
                    ESP_LOGI(TAG, "[%" PRIu32 " ms] TELEM: estado=%s dist=%" PRIu16 "cm bat=%" PRIu16 "mV rej=%" PRIu16,
                             evt.timestamp_ms,
                             state_name(evt.curr_state),
                             evt.dist_cm,
                             evt.batt_mv,
                             evt.rejected_cnt);
                    break;
            }
        }
    }
}

static inline void post_log_event(log_event_type_t type, car_state_t prev, car_state_t curr, uint16_t dist, uint16_t batt, uint16_t rej, uint32_t now_ms)
{
    if (s_log_queue == NULL) {
        return;
    }
    log_event_t ev = {
        .type = type,
        .timestamp_ms = now_ms,
        .prev_state = prev,
        .curr_state = curr,
        .dist_cm = dist,
        .batt_mv = batt,
        .rejected_cnt = rej
    };
    xQueueSend(s_log_queue, &ev, 0);
}

/* =========================================================================
 * Laço de Controle Periódico (10 ms / 100 Hz) via esp_timer
 * ========================================================================= */
static void control_timer_cb(void *arg)
{
    (void)arg;
    static uint32_t tick_count = 0;
    tick_count++;

    uint32_t now_ms = (uint32_t)(esp_timer_get_time() / 1000);

    /* 1. Recepção e validação do pacote de comando */
    rx_packet_t rx_pkt;
    cmd_packet_t cmd;
    bool has_cmd = false;

    if (xQueueReceive(s_rx_queue, &rx_pkt, 0) == pdTRUE) {
#if CONFIG_CAR_BENCH_MODE
        /* Modo de bancada aceita qualquer pacote injetado na fila */
        link_verdict_t v = link_check_cmd(&s_link_rx, NULL, rx_pkt.data, rx_pkt.len, now_ms, &cmd);
#else
        link_verdict_t v = link_check_cmd(&s_link_rx, rx_pkt.src_mac, rx_pkt.data, rx_pkt.len, now_ms, &cmd);
#endif
        if (v == LINK_OK) {
            has_cmd = true;
        }
    }

    /* 2. Verificação de status do enlace */
#if CONFIG_CAR_BENCH_MODE
    bool link_ok = true; // Em bancada o enlace é virtual e sempre ativo
#else
    bool link_ok = link_is_alive(&s_link_rx, now_ms);
#endif

    if (s_prev_link_ok && !link_ok) {
        post_log_event(EVT_LINK_LOST, s_sm.state, ST_FALHA_COM, 0, 0, 0, now_ms);
    }
    s_prev_link_ok = link_ok;

    /* 3. Leitura do sensor ultrassônico da fila */
    sonar_reading_t sonar_data;
    bool has_sonar = false;
    if (xQueueReceive(s_sonar_queue, &sonar_data, 0) == pdTRUE) {
        has_sonar = true;
    }

    /* 4. Leitura periódica da bateria (a cada 200 ms / 20 ticks) */
    if ((tick_count % 20) == 0) {
        s_latest_battery_mv = battery_read_mv();
    }

    /* 5. Execução da Máquina de Estados */
    car_inputs_t in = {
        .has_cmd = has_cmd,
        .cmd_x = has_cmd ? cmd.x : 0,
        .cmd_y = has_cmd ? cmd.y : 0,
        .link_ok = link_ok,
        .distance_cm = has_sonar ? sonar_data.distance_cm : 0xFFFF,
        .echo_ok = has_sonar ? sonar_data.echo_ok : false,
        .has_sensor_data = has_sonar,
        .now_ms = now_ms
    };

    car_outputs_t out;
    car_sm_step(&s_sm, &in, &out);

    /* Registro de eventos de transição e diagnóstico */
    if (out.state_changed) {
        post_log_event(EVT_STATE_CHANGE, out.prev_state, out.state, out.distance_cm, s_latest_battery_mv, s_link_rx.rejected_cnt + s_sm.rejected_cmd_cnt, now_ms);
        if (out.state == ST_OBSTACULO) {
            post_log_event(EVT_OBSTACLE_ENTER, out.prev_state, out.state, out.distance_cm, 0, 0, now_ms);
        }
    }

    if (has_cmd && cmd.x == 0 && cmd.y == 0) {
        post_log_event(EVT_CENTER_DETECTED, out.state, out.state, 0, 0, 0, now_ms);
    }

    /* 6. Atuação nos Motores com Rampa e Freio */
    if (out.brake_immediate) {
        motors_brake();
        ramp_reset(&s_ramp_left, now_ms);
        ramp_reset(&s_ramp_right, now_ms);
        if (!s_prev_zero_state) {
            post_log_event(EVT_MOTORS_ZERO, out.state, out.state, 0, 0, 0, now_ms);
            s_prev_zero_state = true;
        }
    } else {
        int8_t pwr_left = ramp_step(&s_ramp_left, out.target_left, now_ms);
        int8_t pwr_right = ramp_step(&s_ramp_right, out.target_right, now_ms);
        motors_set(pwr_left, pwr_right);

        if (pwr_left == 0 && pwr_right == 0) {
            if (!s_prev_zero_state) {
                post_log_event(EVT_MOTORS_ZERO, out.state, out.state, 0, 0, 0, now_ms);
                s_prev_zero_state = true;
            }
        } else {
            s_prev_zero_state = false;
        }
    }

    /* 7. Telemetria a cada 200 ms (5 Hz / 20 ticks) */
    if ((tick_count % 20) == 0) {
        uint16_t total_rejected = s_link_rx.rejected_cnt + s_sm.rejected_cmd_cnt;

#if CONFIG_CAR_BENCH_MODE
        /* No modo de bancada, a telemetria é apenas logada */
        post_log_event(EVT_TELEM_LOG, out.state, out.state, out.distance_cm, s_latest_battery_mv, total_rejected, now_ms);
#else
        /* No modo normal, monta e transmite pacote ESP-NOW */
        static uint16_t s_telem_seq = 0;
        telem_packet_t telem;
        telem.magic = PROTO_MAGIC;
        telem.type = PKT_TELEM;
        telem.seq = s_telem_seq++;
        telem.state = (uint8_t)out.state;
        telem.reserved = 0;
        telem.distance_cm = out.distance_cm;
        telem.battery_mv = s_latest_battery_mv;
        telem.rejected_cnt = total_rejected;
        telem.crc = crc8_calc((const uint8_t *)&telem, sizeof(telem) - 1);

        esp_now_send(s_controller_mac, (const uint8_t *)&telem, sizeof(telem));
#endif
    }
}

/* =========================================================================
 * Ponto de entrada (app_main)
 * ========================================================================= */
void app_main(void)
{
    ESP_LOGI(TAG, "=== Inicializando Firmware do Carrinho (ESP-IDF v6.1) ===");

    /* Inicialização do NVS */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    /* Criação das filas FreeRTOS */
    s_rx_queue = xQueueCreate(8, sizeof(rx_packet_t));
    s_sonar_queue = xQueueCreate(1, sizeof(sonar_reading_t));
    s_log_queue = xQueueCreate(16, sizeof(log_event_t));

    /* Inicialização dos módulos de hardware */
    ESP_ERROR_CHECK(motors_init());
    ESP_ERROR_CHECK(sonar_init(s_sonar_queue));
    ESP_ERROR_CHECK(battery_init());

    /* Inicialização da máquina de estados, rampa e enlace */
    uint32_t now_ms = (uint32_t)(esp_timer_get_time() / 1000);
    link_init(&s_link_rx, s_controller_mac, now_ms);
    car_sm_init(&s_sm);
    ramp_reset(&s_ramp_left, now_ms);
    ramp_reset(&s_ramp_right, now_ms);

    /* Inicia a tarefa de logging */
    xTaskCreate(log_task, "log_task", 3072, NULL, 3, NULL);

    /* Conclui autoteste e marca sistema como pronto */
    car_sm_set_ready(&s_sm);

#if CONFIG_CAR_BENCH_MODE
    ESP_LOGW(TAG, "****************************************************");
    ESP_LOGW(TAG, "MODO DE BANCADA ATIVADO (CONFIG_CAR_BENCH_MODE=y)");
    ESP_LOGW(TAG, "Wi-Fi e ESP-NOW desativados. Injetando comandos locais.");
    ESP_LOGW(TAG, "ATENCAO: Testar com o carrinho levantado (rodas livres)!");
    ESP_LOGW(TAG, "****************************************************");
    ESP_ERROR_CHECK(bench_mode_start(s_rx_queue));
#else
    ESP_ERROR_CHECK(wifi_esp_now_init());
#endif

    /* Inicia o laço de controle periódico de 10 ms (ESP_TIMER_TASK) */
    esp_timer_create_args_t timer_args = {
        .callback = &control_timer_cb,
        .arg = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "control_timer",
        .skip_unhandled_events = true
    };
    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &s_control_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(s_control_timer, CONTROL_PERIOD_MS * 1000));

    ESP_LOGI(TAG, "Sistema do carrinho operacional (Laço de controle: %d ms).", CONTROL_PERIOD_MS);
}
