/**
 * @file link.c
 * @brief Implementação da validação e monitoramento do enlace
 */

#include "link.h"
#include "crc8.h"
#include <string.h>

void link_init(link_rx_t *rx, const uint8_t *expected_mac, uint32_t now_ms)
{
    if (rx == NULL) {
        return;
    }
    if (expected_mac != NULL) {
        memcpy(rx->expected_mac, expected_mac, 6);
    } else {
        memset(rx->expected_mac, 0, 6);
    }
    rx->last_seq = 0;
    rx->last_rx_ms = now_ms;
    rx->boot_time_ms = now_ms;
    rx->has_received = false;
    rx->rejected_cnt = 0;
}

link_verdict_t link_check_cmd(link_rx_t *rx, const uint8_t *src_mac, const uint8_t *data, size_t len, uint32_t now_ms, cmd_packet_t *out_cmd)
{
    if (rx == NULL || data == NULL) {
        return LINK_ERR_SIZE;
    }

    /* 1. Validação de origem, formato, tamanho, magic, type e CRC */
    if (src_mac != NULL && memcmp(src_mac, rx->expected_mac, 6) != 0) {
        rx->rejected_cnt++;
        return LINK_ERR_SRC_MAC;
    }

    if (len != sizeof(cmd_packet_t)) {
        rx->rejected_cnt++;
        return LINK_ERR_SIZE;
    }

    const cmd_packet_t *pkt = (const cmd_packet_t *)data;

    if (pkt->magic != PROTO_MAGIC) {
        rx->rejected_cnt++;
        return LINK_ERR_MAGIC;
    }

    if (pkt->type != PKT_CMD) {
        rx->rejected_cnt++;
        return LINK_ERR_TYPE;
    }

    uint8_t calculated_crc = crc8_calc(data, sizeof(cmd_packet_t) - 1);
    if (pkt->crc != calculated_crc) {
        rx->rejected_cnt++;
        return LINK_ERR_CRC;
    }

    /* Enlace perdido (FALHA_COM): o controle pode ter reiniciado e voltado a
     * contar seq do início. Sem isso, os pacotes novos seriam rejeitados como
     * "sequência antiga" por até 32768 pacotes (~27 min) e o teste T1 falharia.
     * O próximo pacote íntegro passa a ser tratado como "primeiro pacote".
     * Isso não libera o carrinho: sair de FALHA_COM continua exigindo o
     * joystick no centro (RF07, state_machine.c). */
    if (rx->has_received && !link_is_alive(rx, now_ms)) {
        rx->has_received = false;
    }

    /* 2. Sequência antiga ou repetida (aceitar se diff > 0 ou for o 1º pacote) */
    if (rx->has_received) {
        int16_t diff = (int16_t)(pkt->seq - rx->last_seq);
        if (diff <= 0) {
            rx->rejected_cnt++;
            return LINK_ERR_SEQ;
        }
    }

    /* 3. Faixa de valores: x e y devem estar em -100..100 e só bits de
     * botão conhecidos podem estar ligados */
    if (pkt->x < PROTO_AXIS_MIN || pkt->x > PROTO_AXIS_MAX ||
        pkt->y < PROTO_AXIS_MIN || pkt->y > PROTO_AXIS_MAX ||
        (pkt->buttons & (uint8_t)~PROTO_BTN_MASK) != 0) {
        rx->rejected_cnt++;
        return LINK_ERR_RANGE;
    }

    /* 4. Pacote válido: renova o temporizador de enlace e atualiza sequência */
    rx->last_seq = pkt->seq;
    rx->last_rx_ms = now_ms;
    rx->has_received = true;

    if (out_cmd != NULL) {
        *out_cmd = *pkt;
    }

    return LINK_OK;
}

bool link_is_alive(const link_rx_t *rx, uint32_t now_ms)
{
    if (rx == NULL) {
        return false;
    }

    /* Sem pacote aceito desde o boot (ou desde a última queda), avalia a partir
     * do boot. Depois de uma queda, boot_time_ms já está mais de
     * LINK_TIMEOUT_MS no passado, então o enlace continua perdido até um
     * pacote válido ser aceito. */
    uint32_t ref_time = rx->has_received ? rx->last_rx_ms : rx->boot_time_ms;

    /* RF05: enlace perdido ao completar LINK_TIMEOUT_MS sem pacote válido.
     * Seção 9.1: FALHA_COM aos 300 ms e não aos 299 ms. */
    return (now_ms - ref_time) < LINK_TIMEOUT_MS;
}
