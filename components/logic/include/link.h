/**
 * @file link.h
 * @brief Validação e monitoramento do enlace de comando ESP-NOW
 *
 * Atende ao RF01, RF05, RF06 e RNF01.
 */

#ifndef LINK_H
#define LINK_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "protocol.h"
#include "config.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    LINK_OK = 0,            /**< Pacote válido e aceito */
    LINK_ERR_SRC_MAC,       /**< Origem diferente do MAC esperado */
    LINK_ERR_SIZE,          /**< Tamanho incompatível com cmd_packet_t */
    LINK_ERR_MAGIC,         /**< Magic number diferente de PROTO_MAGIC */
    LINK_ERR_TYPE,          /**< Tipo diferente de PKT_CMD */
    LINK_ERR_CRC,           /**< Falha no cálculo do CRC-8 */
    LINK_ERR_SEQ,           /**< Número de sequência antigo ou repetido */
    LINK_ERR_RANGE          /**< Eixo x ou y fora do intervalo -100..100 */
} link_verdict_t;

typedef struct {
    uint8_t  expected_mac[6]; /**< MAC esperado do controle */
    uint16_t last_seq;        /**< Último seq aceito */
    uint32_t last_rx_ms;      /**< Timestamp da última recepção válida */
    uint32_t boot_time_ms;    /**< Timestamp de inicialização */
    bool     has_received;    /**< Indica se já recebeu ao menos um pacote */
    uint16_t rejected_cnt;    /**< Total de pacotes/comandos rejeitados */
} link_rx_t;

/**
 * @brief Inicializa o estado de monitoramento do enlace
 *
 * @param rx Ponteiro para estrutura do receptor
 * @param expected_mac MAC de 6 bytes do transmissor esperado
 * @param now_ms Timestamp de boot em ms
 */
void link_init(link_rx_t *rx, const uint8_t *expected_mac, uint32_t now_ms);

/**
 * @brief Valida um pacote bruto recebido segundo as regras da Seção 8.2 (RF06)
 *
 * @param rx Ponteiro para estrutura do receptor
 * @param src_mac MAC de origem do pacote (pode ser NULL para modo de bancada)
 * @param data Buffer de dados
 * @param len Tamanho dos dados
 * @param now_ms Timestamp atual em ms
 * @param out_cmd Ponteiro de saída para armazenar o comando decodificado se válido
 * @return Veredito da validação
 */
link_verdict_t link_check_cmd(link_rx_t *rx, const uint8_t *src_mac, const uint8_t *data, size_t len, uint32_t now_ms, cmd_packet_t *out_cmd);

/**
 * @brief Verifica se o sinal de vida do enlace está ativo (now - last_rx <= 300 ms)
 *
 * @param rx Ponteiro para estrutura do receptor
 * @param now_ms Timestamp atual em ms
 * @return true se enlace ativo, false se perdido
 */
bool link_is_alive(const link_rx_t *rx, uint32_t now_ms);

#ifdef __cplusplus
}
#endif

#endif // LINK_H
