/**
 * @file UART.h
 * @brief USART2 driver for MAX30101 data transmission
 * @details Configures USART2 (PA2=TX, PA15=RX) at 230400 baud with blocking transmission
 * @author Julio Fajardo, PhD
 * @date 2024-06-01
 */

#ifndef UART_H_
#define UART_H_

#include <stdint.h>
#include "MAX30101.h"

/**
 * @brief Initialize USART2 for 230400 baud transmission
 * @details Configuration sequence:
 *          1. Enable clocks: USART2, GPIOA
 *          2. Configure PA2 (TX) and PA15 (RX) as alternate function AF7
 *          3. Configure USART2: desired baud, 8-bit data, 1 stop bit
 *          4. Enable receiver interrupt
 *
 * @param baud_rate - Desired baud rate
 * @return void
 * @retval N/A
 *
 * @timing
 *  - Total init time: <1 ms
 *  - USART2 ready after: function completes
 *
 * @warning
 *  - Call once at system startup, BEFORE any USART2_Send operations
 *  - External USB-UART converter required on PA2/PA15
 *
 * @see USART2_Send, USART2_putString
 */
void UART_Config(uint32_t baud_rate);

/**
 * @brief Send single character via UART
 * @details Blocks until previous transmission complete, then sends one byte
 *
 * @param c - Character byte to transmit
 * @return void
 * @retval N/A (blocking)
 *
 * @timing
 *  - Per-byte latency: ~40-50 µs at 230400 baud
 *
 * @data_format
 *  - UART parameters: 8-bit, 1 stop bit, no parity
 *  - Baud rate: 230400
 *
 * @see UART_Config, USART2_putString
 */
void USART2_Send(uint8_t c);

/**
 * @brief Send null-terminated string via UART
 * @details Transmits string character-by-character using USART2_Send()
 *
 * @param string - Pointer to null-terminated character string
 * @return void
 * @retval N/A (blocking)
 *
 * @timing
 *  - Per-character latency: ~40-50 µs at 230400 baud
 *  - For 10-char string: ~400-500 µs total
 *
 * @data_format
 *  - String must be null-terminated
 *  - Each character transmitted as 8-bit value
 *
 * @usage_example
 *  ```
 *  USART2_putString("Hello UART\r");
 *  ```
 *
 * @see UART_Config, USART2_Send
 */
void USART2_putString(char *string);

#endif /* UART_H_ */
