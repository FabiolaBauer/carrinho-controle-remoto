/**
 * @file crc8.c
 * @brief Implementação de CRC-8 (Polinômio 0x07, init 0x00)
 */

#include "crc8.h"

uint8_t crc8_calc(const uint8_t *data, size_t len)
{
    uint8_t crc = 0x00;
    if (data == NULL) {
        return 0;
    }
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 0x80) {
                crc = (uint8_t)((crc << 1) ^ 0x07);
            } else {
                crc = (uint8_t)(crc << 1);
            }
        }
    }
    return crc;
}
