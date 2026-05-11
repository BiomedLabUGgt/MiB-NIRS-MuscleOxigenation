/**
 * @file PCA9548.h
 * @brief PCA9548 8-Channel I2C Switch Driver
 * @details Controls the NXP PCA9548A I2C bus multiplexer, enabling communication
 *          with up to 8 independent I2C devices that share the same address space.
 *          A single control byte selects which downstream channel is active.
 *
 * ### Hardware Configuration
 *  - **Peripheral**: I2C1 (shared with other sensors)
 *  - **Default Address**: 0x70 (A0=A1=A2=GND), pre-shifted to 0xE0 for STM32 CR2
 *  - **Channels**: 8 (SD0/SC0 through SD7/SC7)
 *  - **Protocol**: Single control byte — no register address, just device address + 1 byte
 *
 * ### Usage
 *  ```c
 *  PCA9548_Init();           // Disable all channels on startup
 *  PCA9548_SelectChannel(0); // Activate channel 0 (e.g., first MAX30101)
 *  MAX30101_Read(...);
 *  PCA9548_SelectChannel(1); // Switch to channel 1 (e.g., second MAX30101)
 *  MAX30101_Read(...);
 *  ```
 *
 * @author Julio Fajardo
 * @date 2026-05-11
 * @version 1.1
 * @note Requires I2C1_Config() to be called before use.
 */

#ifndef PCA9548_H_
#define PCA9548_H_

#include <stdint.h>

/** @brief PCA9548 device address (0x70 << 1) for STM32 I2C CR2 SADD field */
#define PCA9548_ADDR    0xE0

/**
 * @brief Initialize PCA9548 — disable all downstream channels
 * @details Writes 0x00 to the PCA9548 control register, ensuring no channel
 *          is active at startup. Call once after I2C1_Config().
 * @return void
 */
void PCA9548_Init(void);

/**
 * @brief Activate one downstream I2C channel on the PCA9548
 * @details Writes a bitmask derived from the channel number to the PCA9548.
 *          The previously active channel is automatically deactivated.
 * @param channel - Channel number to enable (0–7)
 *                  e.g., 0 activates SD0/SC0, 7 activates SD7/SC7
 * @return void
 * @note Blocking; typical latency ~20-30 µs
 * @warning Only one channel should be active at a time when downstream devices
 *          share the same I2C address (e.g., multiple MAX30101 sensors).
 */
void PCA9548_SelectChannel(uint8_t channel);

#endif /* PCA9548_H_ */
