/**
 * @file PCA9548.c
 * @brief PCA9548 8-Channel I2C Switch Driver Implementation
 * @details Channel selection for the PCA9548A I2C bus multiplexer using I2C1_WriteByte.
 *          The PCA9548 uses a 1-byte protocol (no register address), so I2C1_WriteByte
 *          is used instead of I2C1_Write, which always sends 2 bytes and would cause
 *          a NACK on the second byte.
 * @author Julio Fajardo
 * @date 2026-05-11
 * @version 1.1
 */

#include "PCA9548.h"
#include "I2C.h"

void PCA9548_Init(void) {
    /* Disable all downstream channels: control byte = 0x00 */
    I2C1_WriteByte(PCA9548_ADDR, 0x00);
}

void PCA9548_SelectChannel(uint8_t channel) {
    /* Convert channel number (0-7) to bitmask and send as control byte */
    I2C1_WriteByte(PCA9548_ADDR, (uint8_t)(1U << channel));
}
