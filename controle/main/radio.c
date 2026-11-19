/**
 * @file radio.c
 * @brief Wi-Fi STA + ESP-NOW do controle (Seção 6 e Seção 7)
 */

#include "radio.h"
#include "config.h"
#include "esp_wifi.h"
#include "esp_now.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_mac.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "radio";

static const uint8_t s_car_mac[ESP_NOW_ETH_ALEN] = CAR_MAC;
static QueueHandle_t s_telem_queue = NULL;

/* Handshake de envio: o timer põe em true antes de enviar, o callback põe em false */
static volatile bool s_in_flight = false;

/* Contadores com um único escritor cada:
 *  - callbacks (task de Wi-Fi): cb_ok, cb_fail, rx_queue_full
 *  - timer de envio: sent, send_err, skipped, unlocked, consecutive_skips */
static volatile uint32_t s_cb_ok = 0;
static volatile uint32_t s_cb_fail = 0;
static volatile uint32_t s_rx_queue_full = 0;
static volatile uint32_t s_sent = 0;      /* volatile: lidos pela tarefa de UI */
static volatile uint32_t s_send_err = 0;
static volatile uint32_t s_skipped = 0;
static volatile uint32_t s_unlocked = 0;
static uint32_t s_consecutive_skips = 0;

static void on_recv(const esp_now_recv_info_t *info, const uint8_t *data, int data_len)
{
    /* Task de Wi-Fi de alta prioridade: só copia para a fila (Seção 6) */
    if (info == NULL || data == NULL || data_len <= 0 || s_telem_queue == NULL) {
        return;
    }
    telem_rx_t item;
    if (info->src_addr != NULL) {
        memcpy(item.src_mac, info->src_addr, sizeof(item.src_mac));
    } else {
        memset(item.src_mac, 0, sizeof(item.src_mac));
    }
    size_t n = (size_t)data_len;
    if (n > sizeof(item.data)) {
        n = sizeof(item.data);
    }
    memset(item.data, 0, sizeof(item.data));
    memcpy(item.data, data, n);
    item.len = (size_t)data_len;   /* tamanho real: a UI rejeita se != sizeof(telem_packet_t) */

    if (xQueueSend(s_telem_queue, &item, 0) != pdTRUE) {
        s_rx_queue_full++;
    }
}

static void on_send(const esp_now_send_info_t *tx_info, esp_now_send_status_t status)
{
    /* Só registra o resultado (Seção 6). 'status' será removido: usar tx_info->tx_status */
    (void)status;
    if (tx_info != NULL && tx_info->tx_status == WIFI_SEND_SUCCESS) {
        s_cb_ok++;
    } else {
        s_cb_fail++;
    }
    s_in_flight = false;
}

esp_err_t radio_init(QueueHandle_t telem_queue)
{
    s_telem_queue = telem_queue;

    /* Mesma ordem e canal do carrinho (exemplo oficial examples/wifi/espnow, v6.1) */
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_set_channel(CHANNEL, WIFI_SECOND_CHAN_NONE));

    uint8_t my_mac[6];
    ESP_ERROR_CHECK(esp_wifi_get_mac(WIFI_IF_STA, my_mac));
    ESP_LOGI(TAG, "MAC do Controle: " MACSTR, MAC2STR(my_mac));
    ESP_LOGI(TAG, ">> Copie este valor para CONTROLLER_MAC em components/logic/include/config_macs.h <<");
    ESP_LOGI(TAG, "Canal Wi-Fi: %d", CHANNEL);

    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_recv_cb(on_recv));
    ESP_ERROR_CHECK(esp_now_register_send_cb(on_send));

    esp_now_peer_info_t peer;
    memset(&peer, 0, sizeof(peer));
    memcpy(peer.peer_addr, s_car_mac, ESP_NOW_ETH_ALEN);
    peer.channel = 0;            /* canal atual */
    peer.ifidx = WIFI_IF_STA;
    peer.encrypt = false;
    ESP_ERROR_CHECK(esp_now_add_peer(&peer));
    ESP_LOGI(TAG, "Peer do Carrinho configurado: " MACSTR, MAC2STR(s_car_mac));

    return ESP_OK;
}

bool radio_can_send(void)
{
    if (!s_in_flight) {
        s_consecutive_skips = 0;
        return true;
    }

    /* Callback do envio anterior ainda não chegou: pula o ciclo e conta */
    s_skipped++;
    s_consecutive_skips++;

    /* Proteção: se o callback nunca chegar, o controle não pode parar de
     * enviar para sempre (o carrinho ficaria em FALHA_COM). Destrava depois
     * de CTRL_MAX_SKIPPED_SENDS ciclos; o próximo ciclo volta a enviar. */
    if (s_consecutive_skips >= CTRL_MAX_SKIPPED_SENDS) {
        s_in_flight = false;
        s_consecutive_skips = 0;
        s_unlocked++;
    }
    return false;
}

esp_err_t radio_send_cmd(const cmd_packet_t *pkt)
{
    s_in_flight = true;   /* antes do envio: o callback pode chegar antes do retorno */
    esp_err_t err = esp_now_send(s_car_mac, (const uint8_t *)pkt, sizeof(*pkt));
    if (err == ESP_OK) {
        s_sent++;
    } else {
        s_in_flight = false;   /* sem callback neste caso */
        s_send_err++;
    }
    return err;
}

void radio_get_stats(radio_stats_t *out)
{
    out->sent = s_sent;
    out->cb_ok = s_cb_ok;
    out->cb_fail = s_cb_fail;
    out->send_err = s_send_err;
    out->skipped = s_skipped;
    out->unlocked = s_unlocked;
    out->rx_queue_full = s_rx_queue_full;
}
