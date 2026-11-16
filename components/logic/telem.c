/**
 * @file telem.c
 * @brief Validação da telemetria recebida pelo controle
 */

#include "telem.h"
#include "crc8.h"
#include <string.h>

#define MAC_LEN 6

telem_verdict_t telem_check(const uint8_t *expected_mac, const uint8_t *src_mac,
                            const uint8_t *data, size_t len, telem_packet_t *out)
{
    if (data == NULL) {
        return TELEM_ERR_SIZE;
    }
    /* Ordem igual à validação de comandos do carrinho (Seção 8.2) */
    if (expected_mac == NULL || src_mac == NULL || memcmp(src_mac, expected_mac, MAC_LEN) != 0) {
        return TELEM_ERR_SRC_MAC;
    }
    if (len != sizeof(telem_packet_t)) {
        return TELEM_ERR_SIZE;
    }

    telem_packet_t pkt;
    memcpy(&pkt, data, sizeof(pkt));

    if (pkt.magic != PROTO_MAGIC) {
        return TELEM_ERR_MAGIC;
    }
    if (pkt.type != PKT_TELEM) {
        return TELEM_ERR_TYPE;
    }
    if (pkt.crc != crc8_calc(data, sizeof(telem_packet_t) - 1)) {
        return TELEM_ERR_CRC;
    }
    if (pkt.state > ST_FALHA_COM) {
        return TELEM_ERR_STATE;
    }

    if (out != NULL) {
        *out = pkt;
    }
    return TELEM_OK;
}

const char *telem_state_name(uint8_t state)
{
    switch (state) {
        case ST_INIT:      return "INIT";
        case ST_PARADO:    return "PARADO";
        case ST_OPERANDO:  return "OPERANDO";
        case ST_OBSTACULO: return "OBSTACULO";
        case ST_FALHA_COM: return "FALHA_COM";
        default:           return "DESCONHECIDO";
    }
}
