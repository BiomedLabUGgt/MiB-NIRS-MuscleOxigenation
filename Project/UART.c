#include "UART.h"
#include "stm32f303x8.h"
#include "system_stm32f3xx.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/**
 * @brief Initialize USART2 for 230400 baud transmission
 * @details Configuration:
 *          - PA2 (TX) and PA15 (RX) as AF7 (Alternate Function 7)
 *          - 230400 baud rate configured via BRR register
 *          - Receiver interrupt enabled for future extensions
 *
 * @timing: < 1 ms total
 * @see USART2_Send, USART2_putString
 */
void UART_Config(uint32_t baud_rate) {
	RCC->AHBENR |= RCC_AHBENR_GPIOAEN;					        // Clock Enable GPIOA
	RCC->APB1ENR |= RCC_APB1ENR_USART2EN;				        // Clock Enable USART2
		
	GPIOA->MODER |= (0x02<<4) | (0x2<<30);			            // Alternate Function PA2 & PA15
	GPIOA->AFR [0] |= (0x07<<8);						        // PA2 as TX2
	GPIOA->AFR [1] |= (0x07<<28);						        // PA15 as RX2  
		
	USART2->BRR = (uint32_t)((SystemCoreClock/2)/baud_rate);	// Baud Rate = 230400 bps: (64MHz/2)/230400 ~139
	USART2->CR1 |= USART_CR1_RE | USART_CR1_TE;		            // Transmitter & Receiver enable
	USART2->CR1 |= USART_CR1_UE;						        // USART2 enable
	USART2->CR1 |= USART_CR1_RXNEIE;					        // Receiver interrupt enable
}

/**
 * @brief Send single character via UART
 * @details Blocks until previous transmission complete, then sends one byte via TDR
 *
 * @param c - Character byte to transmit
 * @timing: ~40-50 µs per byte at 230400 baud
 */
void USART2_Send(uint8_t c){
	while(!(USART2->ISR & USART_ISR_TC));
	USART2->TDR = c;
}

/**
 * @brief Send null-terminated string via UART
 * @details Transmits string character-by-character using USART2_Send()
 *
 * @param string - Pointer to null-terminated character string
 * @timing: ~40-50 µs per character at 230400 baud
 */
void USART2_putString(char *string){
	while(*string){
		USART2_Send(*string);
		string++;
	}
}