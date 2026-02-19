/** 
    * @file main.c
    * @brief Main program for MAX30101 muscle oxygenation measurement
    * @author Julio Fajardo
    * @date 2024-06-01
    * 
    * This program initializes the MAX30101 sensor for muscle oxygenation measurement using the I2C interface. 
    * It configures the system clock to 64 MHz, sets up a GPIO pin for an LED indicator, and uses the SysTick timer to toggle the LED every 100 ms.
    * The MAX30101 is configured to use 3 LEDs (Red, IR, Green) with a sample rate of 100 Hz and medium LED power for optimal tissue penetration in muscle applications.
*/

#include "arm_math_types.h"
#include "stm32f303x8.h"
#include <stdint.h>

#include "LED.h"
#include "I2C.h"
#include "MAX30101.h"

#include "arm_math.h"

uint32_t counter = 0;
uint32_t ticks = 0;

// Global variable to hold the latest sample data in float format (current in nanoamps)
MAX30101_SampleCurrent MAX30101_SampleCurrentData = {0.0f, 0.0f, 0.0f};  /**< Global variable to hold converted current values in nanoamps (nA) */

void clk_config(void);

/** 
    * @brief Main function, initializes the GPIO and SysTick timer
    * @param None
    * @return None
*/
int main() {

    // Configure the system clock to 64 MHz
    clk_config();
    // Configure the GPIO pin for the LED on PB3
    LED_config();
    // Configure I2C1 for communication with the MAX30101 sensor
    I2C1_Config();
    // Initialize MAX30101 for muscle oxygenation with medium LED power
    MAX30101_InitMuscleOx(0x4B); 
    // Configure SysTick to generate an interrupt every 100 ms
    SysTick_Config(SystemCoreClock / 10);

    for (;;) {
        counter++;
    }
}

/** 
    * @brief SysTick interrupt handler, toggles the LED on PB3 every 100ms
    * @param None
    * @return None
*/  

void SysTick_Handler(void) {
    ticks++;
    LED_Toggle();
    
}

/** 
    * @brief Configures the system clock to 64 MHz using PLL with HSI as the source
    * @param None
    * @return None
*/
void clk_config(void){
	// PLLMUL <- 0x0E (PLL input clock x 16 --> (8 MHz / 2) * 16 = 64 MHz )  
	RCC->CFGR |= 0xE<<18;
	// Flash Latency, two wait states for 48<HCLK<=72 MHz
	FLASH->ACR |= 0x2;
	// PLLON <- 0x1 
    RCC->CR |= RCC_CR_PLLON;
	while (!(RCC->CR & RCC_CR_PLLRDY));	
	// SW<-0x02 (PLL as System Clock), HCLK not divided, PPRE1<-0x4 (APB1 <- HCLK/2), APB2 not divided 
	RCC->CFGR |= 0x402;
	while (!(RCC->CFGR & RCC_CFGR_SWS_PLL));
	SystemCoreClockUpdate();	
}