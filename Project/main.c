/** 
    * @file main.c
    * @brief Main program for MAX30101 muscle oxygenation measurement
    * @author Julio Fajardo, PhD
    * @date 2024-06-01
    * 
    * This program initializes the MAX30101 sensor for muscle oxygenation measurement using the I2C interface. 
    * It configures the system clock to 64 MHz, sets up a GPIO pin for an LED indicator, and uses the SysTick timer to toggle the LED every 100 ms.
    * The MAX30101 is configured to use 3 LEDs (Red, IR, Green) with a sample rate of 100 Hz and medium LED power for optimal tissue penetration in muscle applications.
*/

#include "stm32f303x8.h"
#include <stdint.h>

#include "LED.h"
#include "I2C.h"
#include "MAX30101.h"

#include "arm_math.h"

uint32_t counter = 0;  /**< Debug counter for main loop iterations (unused in release) */
uint32_t ticks = 0;    /**< Interrupt tick counter (incremented per 100 ms SysTick) */

/**
 * @brief Raw FIFO data from sensor (intermediate, rarely needed directly)
 * @details 32-sample circular buffer to store raw 6-byte samples from MAX30101 FIFO.
 *          Typical usage: Diagnostic, direct ADC access, custom processing.
 *          @note Memory: 8 samples × 6 bytes = 48 bytes
 */
MAX30101_Sample MAX30101_FIFO_Buffer[8];

/**
 * @brief 16-bit ADC counts (intermediate format)
 * @details 32-sample buffer for unsigned integer ADC values (0–65535).
 *          Typical usage: Custom scaling, direct DAC output, calibration debug.
 *          @note Memory: 8 samples × 6 bytes = 48 bytes
 */
MAX30101_SampleData MAX30101_SampleDataBuffer[8];

/**
 * @brief FINAL PROCESSED DATA: Calibrated current in nanoamps (nA)
 * @details 32-sample buffer holding calibrated photodiode current values (0–2048 nA).
 *          This is the primary output buffer for post-processing and external transmission.
 *          Updated by SysTick ISR approximately every 100 ms (variable latency based on FIFO fill).
 *          @see SysTick_Handler
 *          @note Memory: 8 samples × 12 bytes (3 × float32_t) = 96 bytes
 *          @note Typically accessed in main loop after ISR completion
 */
MAX30101_SampleCurrent MAX30101_SampleCurrentBuffer[8];

void clk_config(void);

/**
 * @brief System initialization and main control loop
 * @details Initializes all peripherals in sequence:
 *          1. **Clock**: PLL to 64 MHz (HSI 8 MHz × 16)
 *          2. **GPIO**: Status LED on PB3 (push-pull output)
 *          3. **I2C1**: 400 kHz speed on PB6 (SCL), PB7 (SDA)
 *          4. **Sensor**: MAX30101 NIRS configuration with 100 Hz sampling
 *          5. **Timer**: SysTick configured for 100 ms interrupts
 *
 *          After initialization, enters infinite loop that continuously increments
 *          a debug counter. Real work happens in SysTick_Handler() ISR.
 *
 * @param None
 * @return int - Never returns (infinite loop)
 * @note Initialization order is critical; I2C must be ready before MAX30101 init.
 * @warning Upon return, interrupts are globally enabled and SysTick is running.
 * @execution
 *   - Blocking operations during init: Clock PLL lock, I2C configuration
 *   - Time to first ISR: ~100 ms after SysTick_Config()
 * @see clk_config, LED_config, I2C1_Config, MAX30101_InitMuscleOx, SysTick_Handler
 * @example
 *   // main() initializes and waits for interrupts
 *   // SysTick fires every 100 ms with MAXFIFO reads
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
 * @brief SysTick Timer Interrupt Service Routine (100 ms period)
 * @details Core real-time data acquisition routine:
 *          1. Increments `ticks` counter
 *          2. Queries MAX30101 FIFO for new samples
 *          3. If samples available: reads and converts to nanoamps in one call
 *          4. Toggles status LED (visual feedback)
 *
 *          This ISR runs non-preemptively (highest priority) approximately every
 *          100 milliseconds, making it ideal for time-critical sensor polling.
 *
 * @param None
 * @return void
 * @note ISR Context
 *       - Execution time: ~1–2 ms (I2C reads dominate; ~0.5 ms per sample)
 *       - Called at SysTick exception (cannot nest itself)
 *       - All registers preserved; no clobbering of main loop state
 *
 * @data_output
 *       Upon samples available:
 *       - Populates MAX30101_SampleCurrentBuffer[] with up to 32 calibrated samples
 *       - Sample count in local variable `available_samples`
 *       - Data remains in buffer until next ISR overwrites (100 ms window)
 *
 * @timing
 *       - Sample freshness: 0–100 ms (age of data in buffer)
 *       - FIFO latency: Variable; depends on sample rate (100 Hz) and read interval
 *       - At 100 Hz rate with 100 ms polling: Expect 8–10 samples per interrupt
 *
 * @warning
 *       - Race condition possible if main loop reads buffer while ISR writes
 *         (Consider using volatile or double-buffering in production)
 *       - I2C blocking: If I2C bus is busy, ISR may delay by several ms
 *
 * @see MAX30101_GetNumAvailableSamples, MAX30101_ReadFIFO_Current, LED_Toggle
 * @example
 *   // ISR fires every 100 ms:
 *   // ticks++ (incremented 10 times per second)
 *   // MAX30101_SampleCurrentBuffer[] updated with fresh nA values
 *   // LED toggles (100 ms on, 100 ms off = 5 Hz blink)
 */  

void SysTick_Handler(void) {
    ticks++;
    uint8_t available_samples = MAX30101_GetNumAvailableSamples();
    if (available_samples > 0) {
        // Read available samples from the MAX30101 FIFO in float format (nA) into the global buffer
        MAX30101_ReadFIFO_Current(MAX30101_SampleCurrentBuffer, available_samples);
    }
    LED_Toggle();

}

/**
 * @brief Configure STM32F303K8 system clock to 64 MHz via PLL
 * @details PLL configuration chain:
 *          - **Input**: 8 MHz HSI oscillator (internal, always available)
 *          - **Divider**: /2 in PLL block (built-in)
 *          - **Multiplier**: MUL[3:0] = 0x0E (multiply by 16)
 *          - **Output**: (8 MHz / 2) × 16 = 64 MHz
 *          - **System Clock**: PLL output becomes SYSCLK
 *          - **Flash Timing**: 2 wait states for 48 < HCLK ≤ 72 MHz
 *          - **APB1 Divider**: HCLK/2 (32 MHz for most peripherals, incl. I2C)
 *
 * @param None
 * @return void
 *
 * @operations
 *       1. RCC->CFGR |= 0xE<<18 (PLLMUL configuration)
 *       2. FLASH->ACR |= 0x2 (Latency = 2 cycles)
 *       3. RCC->CR |= RCC_CR_PLLON (Enable PLL oscillator)
 *       4. Wait for RCC_CR_PLLRDY flag
 *       5. RCC->CFGR |= 0x402 (SW[1:0]=10 for PLL, PPRE1[2:0]=100 for APB1/2)
 *       6. Wait for RCC_CFGR_SWS_PLL status
 *       7. SystemCoreClockUpdate() (Update HAL clock variable)
 *
 * @timing
 *       - PLL lock time: ~100 µs (measured in hardware)
 *       - Total configuration time: <1 ms
 *       - Blocking: Yes (waits for PLL_RDY and SWS flags)
 *
 * @side_effects
 *       - SYSCLK becomes 64 MHz (all core and bus clocks scale accordingly)
 *       - I2C1 clock: 32 MHz / (I2C prescaler) ≈ 400 kHz
 *       - SysTick frequency scales to HCLK (timer reload values adapt)
 *       - Power consumption increases (~60 mA typical at 64 MHz vs 30 mA at 8 MHz)
 *
 * @warning
 *       - PLL must be configured BEFORE any I2C or timer operations
 *       - Changing clock mid-operation may corrupt ongoing communications
 *       - Requires FLASH latency adjustment or reads may fail
 *
 * @perf_note
 *       - 64 MHz is ~8× faster than default 8 MHz HSI
 *       - I2C bus speed 400 kHz achievable only with sufficient SYSCLK
 *       - Sampling rate limited more by I2C than by CPU
 *
 * @example
 *   // Called from main() to enable high-speed operation
 *   clk_config();
 *   // Now SYSCLK = 64 MHz, I2C/SPI significantly faster
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