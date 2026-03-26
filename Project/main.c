/**
 * @file main.c
 * @brief Main program for MAX30101 muscle oxygenation measurement
 * @details Initializes MAX30101 sensor for NIRS muscle oxygenation using I2C interface.
 *          Configures system clock to 64 MHz, GPIO LED on PB3, and SysTick timer for 20 ms interrupts.
 *          MAX30101 configured with dual LEDs (Red, IR) at 50 Hz sample rate and medium LED power
 *          for optimal tissue penetration in muscle oxygenation applications.
 * @author Julio Fajardo, PhD
 * @date 2024-06-01
 * @version 1.0
 * @see clk_config, LED_config, I2C1_Config, MAX30101_InitNIRSLite, SysTick_Handler
 */

#include "arm_math_types.h"
#include "stm32f303x8.h"
#include <stdint.h>
#include <stdio.h>

#include "PLL.h"
#include "LED.h"
#include "I2C.h"
#include "MAX30101.h"
#include "UART.h"

#include "arm_math.h"

#define SYSTICK_FREQ_HZ     50 /**< SysTick interrupt frequency (Hz) */
#define IIR_NUM_SECTIONS    2  /**< Number of biquad sections in the IIR filter */
#define FILTER_TYPE         1  /**< Filter type identifier (1 for high-pass Chebyshev type II, 0 for First-Order IIR High-Pass (DC-Blocker): H(z) = (1 - z^-1) / (1 - alpha*z^-1) */
#define ALPHA               0.95f /**< Alpha coefficient for first-order IIR DC-Blocker (0.95 corresponds to fc ~0.4 Hz at 50 Hz sampling, 0.995 corresponds to fc ~0.04 Hz at 50 Hz sampling) */

volatile uint8_t data_ready = 0; /**< Flag set by SysTick_Handler when new data is available for processing in main loop */

char tx_buffer[128];  /**< General-purpose buffer for UART transmission */

/**
 * @brief FINAL PROCESSED DATA: Calibrated current in nanoamps (nA)
 * @details 32-sample buffer holding calibrated photodiode current values (0–4096nA).
 *          This is the primary output buffer for post-processing and external transmission.
 *          Updated by SysTick ISR approximately every 20 ms (variable latency based on FIFO fill).
 *          @see SysTick_Handler
 *          @note SpO2 mode memory: 8 samples × 8 bytes (2 × float32_t) = 64 bytes
 *          @note Typically accessed in main loop after ISR completion
 */

 /** Global variables for storing current samples */
volatile MAX30101_CurrentSample MAX30101_NIRS_SingleCurrentSample;
volatile MAX30101_CurrentSample MAX30101_NIRS_FilteredSingleCurrentSample;

/** Chebyshev High-pass (dc-blocker) IIR Filter Coefficients 
    * @details 4th-order Chebyshev type II high-pass filter with 0.04 Hz cutoff frequency, designed using MATLAB's fdesign.highpass and implemented as a cascade of biquads.
    *          Coefficients are in the form [b0, b1, b2, a1, a2] for each biquad section, with feedback coefficients negated for CMSIS-DSP compatibility.
    *          The filter is applied to the raw current samples to remove DC offset and low-frequency drift before further processing.
    *          @see iir_init, iir
    *          @note Designed for a sampling frequency of 50 Hz (matching MAX30101 ODR)
    *          @note Coefficients must be in single-precision float format for CMSIS-DSP
*/
const float32_t iirCoeffs[5 * IIR_NUM_SECTIONS] = {         
    0.98855555f,    -1.9770899f,    0.98855555f,    1.9766545f,     -0.97754645f,
    0.97310543f,    -1.9462072f,    0.97310543f,    1.9457787f,     -0.94663936f
 };

float32_t iirStatesRed[2 * IIR_NUM_SECTIONS] = {0}; /**< State buffer for the IIR filter, initialized to zero */
arm_biquad_cascade_df2T_instance_f32 IIR_Red; /**< CMSIS-DSP IIR filter instance structure */
float32_t iirStatesIR[2 * IIR_NUM_SECTIONS] = {0}; /**< State buffer for the IIR filter, initialized to zero */
arm_biquad_cascade_df2T_instance_f32 IIR_IR; /**< CMSIS-DSP IIR filter instance structure */

/* First-order DC-Blocker states */
float32_t w_red = 0.0f; /**< First-order DC-Blocker intermediate state for red channel */
float32_t w_ir  = 0.0f; /**< First-order DC-Blocker intermediate state for IR channel */

/**
 * @brief System initialization and main control loop
 * @details Initializes all peripherals in sequence:
 *          1. **Clock**: PLL to 64 MHz (HSI 8 MHz × 16)
 *          2. **GPIO**: Status LED on PB3 (push-pull output)
 *          3. **I2C1**: 400 kHz fast-mode on PB6 (SCL), PB7 (SDA)
 *          4. **Sensor**: MAX30101 NIRS Lite mode — Red + IR at 50 Hz, 10.0 mA each
 *          5. **UART**: USART2 at 460800 baud (PA2=TX, PA15=RX)
 *          6. **Timer**: SysTick at 50 Hz (20 ms period), enabling the acquisition ISR
 *
 *          After initialization, the main loop waits for data_ready (set by SysTick ISR),
 *          applies the selected high-pass filter to remove DC offset, and transmits each
 *          filtered Red/IR sample pair over UART as a CSV string.
 *          All sensor acquisition runs in the ISR; filtering and transmission run in main.
 *
 *          Two DC-removal filters are available, selected at compile time via FILTER_TYPE:
 *          - **FILTER_TYPE 0** (default): First-order IIR DC-Blocker H(z) = (1 - z^-1) / (1 - alpha*z^-1),
 *            alpha = 0.95, fc ~= 0.4 Hz, alpha = 0.995, fc ~= 0.04 Hz. Minimal CPU cost, suitable for resource-constrained operation.
 *          - **FILTER_TYPE 1**: 4th-order Chebyshev type II high-pass filter, fc = 0.04 Hz, implemented as a
 *            cascade of 2 biquad sections via CMSIS-DSP. Maximally flat passband; preferred for
 *            clean PPG signal extraction in NIRS applications.
 *
 * @param None
 * @return int - Never returns (infinite loop)
 * @note Initialization order is critical: I2C must be configured before MAX30101,
 *       and UART before SysTick to avoid transmitting before the port is ready.
 *       When FILTER_TYPE == 1, arm_biquad_cascade_df2T_init_f32() must be called
 *       after clk_config() to ensure the PLL and stack are stable.
 * @warning Enabling SysTick (last step) immediately arms the ISR. Any initialization
 *          that must complete before the first ISR fires should precede SysTick_Config().
 * @execution
 *   - Blocking operations during init: PLL lock (~few µs), I2C configuration
 *   - Time to first ISR: ~20 ms after SysTick_Config()
 * @see clk_config, LED_config, I2C1_Config, MAX30101_InitNIRSLite, SysTick_Handler
 * @example
 *   // After init, main loop outputs one filtered line per sample at 50 Hz:
 *   // "1234.567,2345.678\r\n"  (Red nA, IR nA -- DC removed)
 */
int main(void) {
    // Configure system clock to 64 MHz via PLL
    clk_config();
     #if FILTER_TYPE == 1
        // Coefficients already defined for high-pass Chebyshev type II
        arm_biquad_cascade_df2T_init_f32(&IIR_Red, IIR_NUM_SECTIONS, iirCoeffs, iirStatesRed);
        arm_biquad_cascade_df2T_init_f32(&IIR_IR, IIR_NUM_SECTIONS, iirCoeffs, iirStatesIR);
    #endif
    // Configure GPIO port B pin 3 as push-pull output for LED
    LED_config();
    // Configure I2C1 (400 kHz) for MAX30101 communication
    I2C1_Config();
    // Initialize MAX30101 for NIRS measurement with medium LED power
    MAX30101_InitNIRSLite(10.0f,10.0f);  // 10.0 mA LED current for low power operation (up to 51 mA max)
    // Configure USART2 (PA2=TX, PA15=RX) at 460800 baud for data transmission
    UART_Config(460800);
    // Configure SysTick for 20 ms interrupts (SYSTICK_FREQ_HZ = 50 Hz)
    SysTick_Config(SystemCoreClock / SYSTICK_FREQ_HZ);
    
    // Main loop: real work happens in SysTick_Handler ISR
    for (;;) {
        if(data_ready) {
            data_ready = 0; // Clear flag for next ISR cycle
            #if FILTER_TYPE == 1
                arm_biquad_cascade_df2T_f32(&IIR_Red, (float32_t *)&MAX30101_NIRS_SingleCurrentSample.red, (float32_t *)&MAX30101_NIRS_FilteredSingleCurrentSample.red, 1);
                arm_biquad_cascade_df2T_f32(&IIR_IR, (float32_t *)&MAX30101_NIRS_SingleCurrentSample.ir, (float32_t *)&MAX30101_NIRS_FilteredSingleCurrentSample.ir, 1);
            #else
                MAX30101_NIRS_FilteredSingleCurrentSample.red = MAX30101_FirstOrderDC_Blocker(MAX30101_NIRS_SingleCurrentSample.red, &w_red, ALPHA);
                MAX30101_NIRS_FilteredSingleCurrentSample.ir  = MAX30101_FirstOrderDC_Blocker(MAX30101_NIRS_SingleCurrentSample.ir,  &w_ir, ALPHA);
            #endif
            sprintf(tx_buffer, "%.4f,%.4f\r\n", MAX30101_NIRS_FilteredSingleCurrentSample.red, MAX30101_NIRS_FilteredSingleCurrentSample.ir);
            USART2_putString(tx_buffer);
        }
    }
}

/**
 * @brief SysTick Timer Interrupt Service Routine (20 ms period)
 * @details Core real-time data acquisition routine:
 *          1. Queries MAX30101 FIFO for available samples
 *          2. If samples available: reads one sample and converts to nanoamps
 *          3. Advances FIFO read pointer and signals main loop via data_ready flag
 *          4. Toggles status LED (visual heartbeat)
 *
 *          This ISR runs non-preemptively (highest priority) every 20 milliseconds,
 *          synchronized with the MAX30101 output data rate (50 Hz). In steady state,
 *          exactly one sample is available per interrupt.
 *
 * @param None
 * @return void
 * @note ISR Context
 *       - Execution time: ~1–2 ms (I2C reads dominate; ~0.5 ms per transaction)
 *       - Called at SysTick interrupt (cannot nest itself)
 *       - All registers preserved; no clobbering of main loop state
 *
 * @data_output
 *       Upon samples available:
 *       - Populates MAX30101_NIRS_SingleCurrentSample (Red, IR fields in nanoamps)
 *       - Sets data_ready = 1 to signal main loop
 *       - Data valid until next ISR fires (~20 ms window)
 *
 * @timing
 *       - ISR rate: 50 Hz (20 ms period), matching MAX30101 ODR of 50 Hz
 *       - Steady state: exactly 1 sample per interrupt
 *       - At startup: up to 2 samples may be present before the first ISR fires;
 *         only the most recent is read, the pointer advances past all pending samples
 *       - Sample age at read: 0–20 ms depending on arrival time within the period
 *
 * @warning
 *       - data_ready is declared volatile so the compiler does not cache it in a
 *         register across the ISR boundary. Without volatile, -O1 or higher can
 *         optimize away the flag check in the main loop (undefined behavior in C).
 *       - I2C blocking: If I2C bus is busy, ISR execution may extend by several ms
 *
 * @see MAX30101_GetNumAvailableSamples, MAX30101_ReadSingleCurrentData, LED_Toggle
 * @example
 *   // ISR fires every 20 ms (50 Hz), synchronized to sensor output
 *   // MAX30101_NIRS_SingleCurrentSample updated with one fresh Red/IR nA pair
 *   // LED toggles each tick → 25 Hz blink (20 ms on, 20 ms off)
 */

void SysTick_Handler(void) {
    uint8_t available_samples = MAX30101_GetNumAvailableSamples();
    if (available_samples > 0) {
        MAX30101_ReadSingleCurrentData((MAX30101_CurrentSample *)&MAX30101_NIRS_SingleCurrentSample);
        MAX30101_UpdateReadPointer(available_samples);
        data_ready = 1; // Set flag for main loop to process new data
    }
    LED_Toggle();
}

