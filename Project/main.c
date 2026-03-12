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

#include "PLL.h"
#include "LED.h"
#include "I2C.h"
#include "MAX30101.h"

#include "arm_math.h"

#define BUFFER_SIZE         8  /**< Number of samples to read from FIFO per interrupt (max 32) */
#define SYSTICK_FREQ_HZ     20 /**< SysTick interrupt frequency (Hz) */

uint8_t sensor_mode = 0;  /**< Global variable to track current sensor mode (0 = SPO2, 1 = MuscleOx) */

uint32_t counter = 0;  /**< Debug counter for main loop iterations (unused in release) */
uint32_t ticks = 0;    /**< Interrupt tick counter (incremented per 50 ms SysTick at 20 Hz) */

/**
 * @brief FINAL PROCESSED DATA: Calibrated current in nanoamps (nA)
 * @details 32-sample buffer holding calibrated photodiode current values (0–2048 nA).
 *          This is the primary output buffer for post-processing and external transmission.
 *          Updated by SysTick ISR approximately every 100 ms (variable latency based on FIFO fill).
 *          @see SysTick_Handler
 *          @note MuscleOx mode memory: 8 samples × 12 bytes (3 × float32_t) = 96 bytes
 *          @note SpO2 mode memory: 8 samples × 8 bytes (2 × float32_t) = 64 bytes
 *          @note Typically accessed in main loop after ISR completion
 */
MAX30101_SampleCurrent MAX30101_SampleCurrentBuffer[BUFFER_SIZE];
MAX30101_SampleCurrentSpO2 MAX30101_SampleCurrentSpO2Buffer[BUFFER_SIZE];


/**
 * @brief System initialization and main control loop
 * @details Initializes all peripherals in sequence:
 *          1. **Clock**: PLL to 64 MHz (HSI 8 MHz × 16)
 *          2. **GPIO**: Status LED on PB3 (push-pull output)
 *          3. **I2C1**: 400 kHz speed on PB6 (SCL), PB7 (SDA)
 *          4. **Sensor**: MAX30101 NIRS/SpO2 configuration with appropriate sample rate
 *          5. **Timer**: SysTick configured for 50 ms interrupts (SYSTICK_FREQ_HZ = 20)
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
    if(sensor_mode)
        // Initialize MAX30101 for muscle oxygenation measurement with medium LED power
        MAX30101_InitMuscleOx(0x4B);
    else
        // Initialize MAX30101 for SPO2 measurement with low LED power
        MAX30101_InitSPO2Lite(0x4B);
    // Configure SysTick to generate an interrupt at SYSTICK_FREQ_HZ (default 20 Hz = 50 ms)
    SysTick_Config(SystemCoreClock / SYSTICK_FREQ_HZ);

    for (;;) {
        counter++;
    }
}

/**
 * @brief SysTick Timer Interrupt Service Routine (50 ms period)
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
 *       - Data remains in buffer until next ISR overwrites (50 ms window)
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
        if(sensor_mode) {
            // MuscleOx: 3-channel read path
            MAX30101_ReadFIFO_Current(MAX30101_SampleCurrentBuffer, available_samples);
        } else {
            // SPO2: 2-channel read path
            MAX30101_ReadFIFO_CurrentSpO2(MAX30101_SampleCurrentSpO2Buffer, available_samples);
        }
    }
    LED_Toggle();
}
