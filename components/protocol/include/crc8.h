/**
 * @file crc8.h
 * @brief Cálculo de CRC-8 (Polinômio 0x07, init 0x00, sem reflexão)
 *
 * Utilizado para validação de integridade dos pacotes do protocolo.
 */

#ifndef CRC8_H
#define CRC8_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Calcula o CRC-8 sobre um buffer de dados
 * @param data Ponteiro para os dados
 * @param len Tamanho do buffer em bytes
 * @return Valor do CRC-8 calculado
 */
uint8_t crc8_calc(const uint8_t *data, size_t len);

#ifdef __cplusplus
}
#endif

#endif // CRC8_H
