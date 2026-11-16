/**
 * @file telem.h
 * @brief Validação da telemetria recebida pelo controle (Seção 7, item 6)
 *
 * Função pura, sem dependência do ESP-IDF, testável no PC.
 */

#ifndef TELEM_H
#define TELEM_H

#include <stdint.h>
#include <stddef.h>
#include "protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    TELEM_OK = 0,           /**< Telemetria válida */
    TELEM_ERR_SRC_MAC,      /**< Origem diferente de CAR_MAC */
    TELEM_ERR_SIZE,         /**< Tamanho diferente de sizeof(telem_packet_t) */
    TELEM_ERR_MAGIC,        /**< magic diferente de PROTO_MAGIC */
    TELEM_ERR_TYPE,         /**< type diferente de PKT_TELEM */
    TELEM_ERR_CRC,          /**< CRC-8 errado */
    TELEM_ERR_STATE         /**< state fora de car_state_t */
} telem_verdict_t;

/**
 * @brief Valida um pacote de telemetria
 *
 * @param expected_mac MAC esperado (CAR_MAC), 6 bytes
 * @param src_mac      MAC de origem do pacote, 6 bytes
 * @param data         Dados recebidos
 * @param len          Tamanho recebido
 * @param out          Saída: pacote decodificado se TELEM_OK (pode ser NULL)
 * @return Veredito
 */
telem_verdict_t telem_check(const uint8_t *expected_mac, const uint8_t *src_mac,
                            const uint8_t *data, size_t len, telem_packet_t *out);

/** @brief Nome do estado para log ("PARADO", "OPERANDO", ...) */
const char *telem_state_name(uint8_t state);

#ifdef __cplusplus
}
#endif

#endif // TELEM_H
