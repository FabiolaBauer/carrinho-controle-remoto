/**
 * @file protocol.h
 * @brief Definições do protocolo de comunicação ESP-NOW (Controle <-> Carrinho)
 *
 * Atende aos requisitos RF01, RF06 e RNF05.
 * Estruturas compactas (__attribute__((packed))), little-endian.
 */

#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define PROTO_MAGIC 0xA5

#define PROTO_AXIS_MIN      (-100)   /**< Valor mínimo de x e y */
#define PROTO_AXIS_MAX      100      /**< Valor máximo de x e y */
#define PROTO_DIST_INVALID  0xFFFF   /**< distance_cm quando o sensor está inválido */

#define PROTO_BTN_HORN      0x01     /**< cmd_packet_t.buttons: botão do joystick apertado (buzina) */
#define PROTO_BTN_MASK      (PROTO_BTN_HORN)  /**< Bits de botão conhecidos; os outros devem ser 0 */

/**
 * @brief Tipos de pacotes
 */
typedef enum {
    PKT_CMD   = 1, /**< Comando do controle para o carrinho (20 Hz) */
    PKT_TELEM = 2  /**< Telemetria do carrinho para o controle (5 Hz) */
} pkt_type_t;

/**
 * @brief Estados da máquina de estados do carrinho
 */
typedef enum {
    ST_INIT       = 0, /**< Inicialização de hardware e periféricos */
    ST_PARADO     = 1, /**< Motores parados, aguardando comando ou rearme */
    ST_OPERANDO   = 2, /**< Em movimento com controle e rampa ativos */
    ST_OBSTACULO  = 3, /**< Obstáculo detectado (avanço bloqueado) */
    ST_FALHA_COM  = 4  /**< Perda de enlace de comunicação (> 300 ms) */
} car_state_t;

/**
 * @brief Pacote de comando: Controle -> Carrinho (20 Hz)
 */
typedef struct __attribute__((packed)) {
    uint8_t  magic; /**< PROTO_MAGIC (0xA5) */
    uint8_t  type;  /**< PKT_CMD (1) */
    uint16_t seq;   /**< Número de sequência crescente (com wraparound) */
    int8_t   x;     /**< Direção / giro: -100..100 (+ = direita) */
    int8_t   y;     /**< Sentido / aceleração: -100..100 (+ = frente, - = ré) */
    uint8_t  buttons; /**< Bits de botão (PROTO_BTN_*); bits fora de PROTO_BTN_MASK = 0 */
    uint8_t  crc;   /**< CRC-8 de todos os bytes anteriores */
} cmd_packet_t;

/**
 * @brief Pacote de telemetria: Carrinho -> Controle (5 Hz)
 */
typedef struct __attribute__((packed)) {
    uint8_t  magic;        /**< PROTO_MAGIC (0xA5) */
    uint8_t  type;         /**< PKT_TELEM (2) */
    uint16_t seq;          /**< Número de sequência do carrinho */
    uint8_t  state;        /**< car_state_t atual */
    uint8_t  reserved;     /**< Reservado (0) */
    uint16_t distance_cm;  /**< Distância do ultrassom em cm (0xFFFF = inválido) */
    uint16_t battery_mv;   /**< Tensão da bateria em mV */
    uint16_t rejected_cnt; /**< Total de comandos/pacotes rejeitados */
    uint8_t  crc;          /**< CRC-8 de todos os bytes anteriores */
} telem_packet_t;

#endif // PROTOCOL_H
