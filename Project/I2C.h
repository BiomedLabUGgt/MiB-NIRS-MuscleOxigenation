/**
 * @file I2C.h
 * @brief I2C1 Master Communication Driver (400 kHz, STM32F303K8)
 * @details Low-level I2C driver for sensor communication using open-drain configuration.
 *
 * ### Hardware Configuration
 *  - **Peripheral**: I2C1 (APB1 @ 32 MHz)
 *  - **Pins**: PB6 (SCL), PB7 (SDA) - open-drain outputs
 *  - **Speed**: 400 kHz (Fast-mode compliant)
 *  - **Addressing**: 7-bit slave addressing (MSB first)
 *  - **Protocol**: Master-only; repeated START supported for register read
 *
 * ### Timing (400 kHz mode, APB1 = 32 MHz)
 *  - SCL period: 2.5 µs
 *  - SCL high time: ~1.5 µs
 *  - SCL low time: ~1.0 µs
 *  - Setup/hold times: Per I2C Fast-mode specification
 *
 * ### Driver Characteristics
 *  - **Write latency**: ~30-50 µs per byte (2 bytes minimum per transaction)
 *  - **Read latency**: ~100-150 µs per byte (repeated START adds ~50 µs overhead)
 *  - **Blocking**: Yes (waits for bus/flags; no interrupts or DMA)
 *  - **Thread-safe**: No (not safe for concurrent I2C accesses)
 *
 * ### Supported Transactions
 *  1. **Write**: Master writes register address + 1 data byte (MAX30101 registers)
 *  2. **Read**: Master writes address, repeated START, reads N bytes (FIFO streaming)
 *
 * @author Julio Fajardo
 * @date 2024-06-01
 * @version 2.0
 * @note For STM32F303K8 only. TIMINGR value 0x00C50F26 is specific to APB1 = 32 MHz
 * @todo Add error handling (NAK detection, bus timeout)
 * @todo Implement DMA for high-speed FIFO reads
 */

#ifndef I2C_H_
#define I2C_H_

#include <stdint.h>

/**
 * @brief Initialize I2C1 peripheral and GPIO pins
 * @details One-time configuration of I2C1 for master-mode 400 kHz operation.
 *          Must be called before any I2C1_Write() or I2C1_Read().
 */
void I2C1_Config(void);

/**
 * @brief Write single register to I2C slave device
 * @details Master writes 2-byte transaction: [register_addr] [data_byte]
 *          Uses AUTOEND flag for automatic STOP condition.
 * @param slave - 7-bit I2C slave address (e.g., 0x5D for MAX30101)
 * @param addr - Register address (0x00-0xFF)
 * @param data - Data byte to write
 * @return void
 * @note Blocking; typical latency 30-50 µs
 */
void I2C1_Write(uint8_t slave, uint8_t addr, uint8_t data);

/**
 * @brief Read multiple bytes from I2C slave register (repeated START)
 * @details Master performs write-read sequence without releasing bus:
 *          - Write: [slave_addr + W] [register_addr]
 *          - Repeated START: [slave_addr + R]
 *          - Read: [data_0] [data_1] ... [data_N]
 * @param slave - 7-bit I2C slave address
 * @param addr - Register address to read from
 * @param data - [out] Pointer to buffer for received bytes
 * @param size - [in] Number of bytes to read (typically 1-6 for MAX30101)
 * @return void
 * @note Blocking; latency ≈ (100 µs overhead) + (30 µs × size)
 * @warning Buffer overflow if size exceeds allocated data[] array
 */
void I2C1_Read(uint8_t slave, uint8_t addr, uint8_t *data, uint8_t size);

#endif /* I2C_H_ */    