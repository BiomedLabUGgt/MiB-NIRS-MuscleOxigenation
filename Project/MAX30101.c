/**
 * @file MAX30101.c
 * @brief MAX30101 optical biosensor driver implementation for NIRS measurements
 * @details Complete driver for Maxim Integrated MAX30101 optical sensor supporting
 *          multi-LED and SpO2 measurement modes with I2C communication interface.
 * @author Julio Fajardo, PhD
 * @date 2024-06-01
 * @version 2.0
 */

#include "MAX30101.h"
#include "I2C.h"
#include "arm_math_types.h"
#include <stdint.h>

/**
 * @brief Initialize MAX30101 in SpO2 mode (dual-LED configuration)
 * @details Configures sensor for blood oxygen (SpO2) measurement with low power consumption.
 *          - Mode: SpO2 (Red + IR LEDs)
 *          - Sample Rate: 50 Hz (via SPO2_CONFIG = 0x01)
 *          - ADC Resolution: 16-bit
 *          - FIFO Configuration: No averaging, rollover enabled
 * @param ledPower - LED current control register value (0x00 to 0xFF)
 *                  Range: 0.0 mA to 51.0 mA in ~0.2 mA steps
 *                  Typical: 0x4B (~15 mA), 0x32 (~10 mA) for low power
 * @return void
 * @note Suitable for battery-powered wearable applications.
 *       Call this once during initialization before reading samples.
 * @see MAX30101_InitMuscleOx
 * @example
 *   MAX30101_InitSPO2Lite(0x18);
 *   uint8_t samples = MAX30101_GetNumAvailableSamples();
 */
void MAX30101_InitSPO2Lite(float32_t ledPower_red, float32_t ledPower_ir) {
    // Configure FIFO: no averaging, rollover enabled
    I2C1_Write(SENSOR_ADDR, FIFO_CONFIG, 0x10);
    // Select SpO2 mode (Red + IR)
    I2C1_Write(SENSOR_ADDR, MODE_CONFIG, 0x03);
    // SpO2 config: 2048 nA range, 50 Hz sample rate, 118 µs pulse width
    I2C1_Write(SENSOR_ADDR, SPO2_CONFIG, 0x01);
    // Reset FIFO read pointer
    I2C1_Write(SENSOR_ADDR, FIFO_READPTR, 0x0);
    // Reset FIFO write pointer
    I2C1_Write(SENSOR_ADDR, FIFO_WRITPTR, 0x0);
    // Set Red LED power
    I2C1_Write(SENSOR_ADDR, LED1_PAMPLI, (uint8_t)(ledPower_red / 0.2f));  // Convert mA to register value (0.2 mA steps)
    // Set IR LED power
    I2C1_Write(SENSOR_ADDR, LED2_PAMPLI, (uint8_t)(ledPower_ir / 0.2f));  // Same LED power for IR
}

/**
 * @brief Initialize MAX30101 for NIRS muscle oxygenation measurement
 * @details Configures sensor for Near-Infrared Spectroscopy with 3 simultaneous LEDs.
 *          Optimal for non-invasive tissue oxygenation and perfusion monitoring.
 *          - Mode: Multi-LED (Red + IR + Green)
 *          - Sample Rate: 50 Hz (via SPO2_CONFIG = 0x01)
 *          - ADC Resolution: 16-bit
 *          - FIFO Configuration: No averaging, rollover enabled
 * @param ledPower - LED current control register value (0x00 to 0xFF)
 *                  Range: 0.0 mA to 51.0 mA in ~0.2 mA steps
 *                  Typical: 0x4B (~15 mA), 0x32 (~10 mA) for low power
 * @return void
 * @note Three-LED configuration increases tissue penetration depth for muscle assessment.
 *       Same 50 Hz sample rate as SPO2Lite but with additional Green LED for enhanced NIRS analysis.
 * @see MAX30101_InitSPO2Lite
 * @example
 *   MAX30101_InitMuscleOx(0x4B);  // 20 mA LED power
 *   uint8_t available = MAX30101_GetNumAvailableSamples();
 *   MAX30101_ReadFIFO_Current(current_buffer, available);
 */
void MAX30101_InitMuscleOx(float32_t ledPower_red, float32_t ledPower_ir, float32_t ledPower_green) {
    // Configure FIFO: no averaging, rollover enabled
    I2C1_Write(SENSOR_ADDR, FIFO_CONFIG, 0x10);
    // Select multi-LED mode (Red + IR + Green)
    I2C1_Write(SENSOR_ADDR, MODE_CONFIG, 0x07);
    // SpO2 config: 2048 nA range, 50 Hz sample rate, 118 µs pulse width
    I2C1_Write(SENSOR_ADDR, SPO2_CONFIG, 0x01);
    // Reset FIFO read pointer
    I2C1_Write(SENSOR_ADDR, FIFO_READPTR, 0x0);
    // Reset FIFO write pointer
    I2C1_Write(SENSOR_ADDR, FIFO_WRITPTR, 0x0);
    // Set Red LED power
    I2C1_Write(SENSOR_ADDR, LED1_PAMPLI, (uint8_t)(ledPower_red / 0.2f));
    // Set IR LED power
    I2C1_Write(SENSOR_ADDR, LED2_PAMPLI, (uint8_t)(ledPower_ir / 0.2f));
    // Set Green LED power
    I2C1_Write(SENSOR_ADDR, LED3_PAMPLI, (uint8_t)(ledPower_green / 0.2f));
}

/**
 * @brief Query FIFO status from MAX30101 sensor
 * @details Reads FIFO write and read pointer registers to determine number of unread samples.
 *          Accounts for circular 32-sample FIFO with pointer wrap-around.
 * @param None
 * @return uint8_t Number of complete samples available (0 to 32)
 *         - Returns 0 if FIFO empty or pointers equal
 *         - Returns 1 to 32 for available samples
 * @note Call this before MAX30101_ReadFIFO() or MAX30101_ReadFIFO_Current()
 *       to check if new data is ready.
 * @warning Multiple fast consecutive reads may show inconsistent results
 *          due to FIFO updates during pointer reads.
 * @see MAX30101_ReadFIFO, MAX30101_ReadFIFO_Current
 * @example
 *   uint8_t count = MAX30101_GetNumAvailableSamples();
 *   if (count > 0) {
 *       MAX30101_ReadFIFO_Current(samples, count);
 *   }
 */
uint8_t MAX30101_GetNumAvailableSamples(void) {
    uint8_t write_ptr = 0;
    uint8_t read_ptr = 0;
    uint8_t num_samples = 0;
    
    // Read FIFO write and read pointers from sensor
    I2C1_Read(SENSOR_ADDR, FIFO_WRITPTR, &write_ptr, 1);
    I2C1_Read(SENSOR_ADDR, FIFO_READPTR, &read_ptr, 1);
    
    // Mask to 5 bits (FIFO pointers are 5-bit: 0-31)
    write_ptr &= 0x1F;
    read_ptr &= 0x1F;
    
    // Calculate number of available samples (handles wrap-around)
    if (write_ptr >= read_ptr) {
        num_samples = write_ptr - read_ptr;
    } else {
        num_samples = (32 - read_ptr) + write_ptr;
    }
    
    return num_samples;
}

/**
 * @brief Update the FIFO read pointer
 * @details Advances the read pointer by a specified number of samples, wrapping around at 32.
 * @param num_samples - [in] Number of samples to advance the read pointer
 * @return void
 */
void MAX30101_UpdateReadPointer(uint8_t num_samples) {
    uint8_t read_ptr = 0;
    // Read current FIFO read pointer
    I2C1_Read(SENSOR_ADDR, FIFO_READPTR, &read_ptr, 1);
    // Advance pointer by num_samples with wrap-around at 32
    read_ptr = (read_ptr + num_samples) % 32;
    // Write updated pointer back to sensor
    I2C1_Write(SENSOR_ADDR, FIFO_READPTR, read_ptr);
}

/**
 * @brief Convert raw SpO2 sample bytes to 16-bit ADC counts
 * @details Combines 2-byte pairs (MSB, LSB) into 16-bit values per channel.
 *
 * @param sample_in - [in] Pointer to MAX30101_SampleSpO2 with raw data
 * @param sample_out - [out] Pointer to MAX30101_SampleDataSpO2 for counts (0-65535)
 * @return void
 * @see MAX30101_ConvertUint16ToCurrentSpO2
 */
void MAX30101_ConvertSampleToUint16SpO2(MAX30101_SampleSpO2 *sample_in, MAX30101_SampleDataSpO2 *sample_out) {
    // Convert Red LED bytes to 16-bit ADC count
    sample_out->red = ((uint16_t)sample_in->red[0] << 8) | ((uint16_t)sample_in->red[1]);
    // Convert IR LED bytes to 16-bit ADC count
    sample_out->ir = ((uint16_t)sample_in->ir[0] << 8) | ((uint16_t)sample_in->ir[1]);
}

/**
 * @brief Convert 16-bit SpO2 ADC samples to calibrated current in nanoamps
 * @details Scales ADC counts using 7.81 pA LSB to nA range (0-2048 nA).
 *
 * @param sample_in - [in] Pointer to MAX30101_SampleDataSpO2 with ADC counts
 * @param sample_out - [out] Pointer to MAX30101_SampleCurrentSpO2 for current (nA)
 * @return void
 * @see MAX30101_ReadFIFO_CurrentSpO2
 */
void MAX30101_ConvertUint16ToCurrentSpO2(MAX30101_SampleDataSpO2 *sample_in, MAX30101_SampleCurrentSpO2 *sample_out) {
    // Scale Red LED ADC count to current (nanoamps)
    sample_out->red = (float32_t)sample_in->red * MAX30101_CURRENT_LSB_NA;
    // Scale IR LED ADC count to current (nanoamps)
    sample_out->ir = (float32_t)sample_in->ir * MAX30101_CURRENT_LSB_NA;
}

/**
 * @brief Read single SpO2 sample from MAX30101 FIFO as 16-bit ADC counts
 * @details Optimized function for reading one sample at a time.
 *          Reads 4 bytes from FIFO and returns as raw 16-bit ADC values (0-65535).
 *
 * @param sample - [out] Pointer to MAX30101_SampleDataSpO2 for result
 * @return void
 * @see MAX30101_ReadFIFO_DataSpO2, MAX30101_GetNumAvailableSamples
 */
void MAX30101_ReadSingleDataSpO2(MAX30101_SampleDataSpO2 *sample) {
    uint8_t fifo_data[4];

    // Read 4 bytes from FIFO data register
    I2C1_Read(SENSOR_ADDR, FIFO_DATAREG, fifo_data, 4);

    // Convert Red LED: combine bytes to 16-bit unsigned value
    sample->red = ((uint16_t)fifo_data[0] << 8) | fifo_data[1];

    // Convert IR LED: combine bytes to 16-bit unsigned value
    sample->ir = ((uint16_t)fifo_data[2] << 8) | fifo_data[3];
}

/**
 * @brief Read single SpO2 sample from MAX30101 FIFO with current conversion
 * @details Optimized function for reading one sample at a time.
 *          Reads 4 bytes from FIFO and converts to nA immediately.
 *
 * @param sample - [out] Pointer to MAX30101_SampleCurrentSpO2 for result
 * @return void
 * @see MAX30101_ReadFIFO_CurrentSpO2, MAX30101_GetNumAvailableSamples
 */
void MAX30101_ReadSingleCurrentSpO2(MAX30101_SampleCurrentSpO2 *sample) {
    uint8_t fifo_data[4];
    uint16_t temp;

    // Read 4 bytes from FIFO data register
    I2C1_Read(SENSOR_ADDR, FIFO_DATAREG, fifo_data, 4);

    // Convert Red LED: combine bytes and scale to nanoamps
    temp = ((uint16_t)fifo_data[0] << 8) | fifo_data[1];
    sample->red = (float32_t)temp * MAX30101_CURRENT_LSB_NA;

    // Convert IR LED: combine bytes and scale to nanoamps
    temp = ((uint16_t)fifo_data[2] << 8) | fifo_data[3];
    sample->ir = (float32_t)temp * MAX30101_CURRENT_LSB_NA;
}

/**
 * @brief Convert raw byte pairs to 16-bit ADC counts
 * @details Combines 2 raw bytes (MSB, LSB) into 16-bit unsigned integers per channel.
 *          No scaling applied—output is raw ADC count (0–65535).
 * @param sample_in - [in] Pointer to MAX30101_Sample with raw 2-byte data
 * @param sample_out - [out] Pointer to MAX30101_SampleData for converted counts
 * @return void
 * @retval N/A
 * @note This is typically an intermediate step. For direct nanoamp values,
 *       use MAX30101_ConvertUint16ToCurrent() after this or use
 *       MAX30101_ReadFIFO_Current() to bypass intermediate storage.
 * @see MAX30101_ConvertUint16ToCurrent, MAX30101_ReadFIFO_Current
 * @example
 *   MAX30101_Sample raw;
 *   MAX30101_SampleData counts;
 *   MAX30101_ReadFIFO(&raw, 1);
 *   MAX30101_ConvertSampleToUint16(&raw, &counts);  // counts.red = 0-65535
 */
void MAX30101_ConvertSampleToUint16(MAX30101_Sample *sample_in, MAX30101_SampleData *sample_out) {
    // Convert Red LED bytes to 16-bit ADC count
    sample_out->red = ((uint16_t)sample_in->red[0] << 8) | 
                      ((uint16_t)sample_in->red[1]);
    
    // Convert IR LED bytes to 16-bit ADC count
    sample_out->ir = ((uint16_t)sample_in->ir[0] << 8) | 
                     ((uint16_t)sample_in->ir[1]);
    
    // Convert Green LED bytes to 16-bit ADC count
    sample_out->green = ((uint16_t)sample_in->green[0] << 8) | 
                        ((uint16_t)sample_in->green[1]);
}

/**
 * @brief Scale ADC counts to calibrated current (nanoamps)
 * @details Converts 16-bit ADC values to current using:
 *          Current (nA) = ADC_Count × 0.00781 nA
 *          Full range: 0 to 2048 nA
 *          LSB Resolution: 7.81 pA per count
 * @param sample_in - [in] Pointer to MAX30101_SampleData with ADC counts (0–65535)
 * @param sample_out - [out] Pointer to MAX30101_SampleCurrent for current in nanoamps
 * @return void
 * @retval N/A
 * @note Intermediate step for two-phase conversion pipeline. For single-step
 *       FIFO read + conversion, use MAX30101_ReadFIFO_Current() instead.
 * @see MAX30101_ReadFIFO_Current
 * @example
 *   MAX30101_SampleData adc_counts;
 *   MAX30101_SampleCurrent current;
 *   MAX30101_ConvertUint16ToCurrent(&adc_counts, &current);
 *   printf("Red LED: %f nA\n", current.red);  // Output in nanoamps
 */
void MAX30101_ConvertUint16ToCurrent(MAX30101_SampleData *sample_in, MAX30101_SampleCurrent *sample_out) {
    // Scale Red LED ADC count to current (nanoamps)
    sample_out->red = (float32_t)sample_in->red * MAX30101_CURRENT_LSB_NA;
    
    // Scale IR LED ADC count to current (nanoamps)
    sample_out->ir = (float32_t)sample_in->ir * MAX30101_CURRENT_LSB_NA;
    
    // Scale Green LED ADC count to current (nanoamps)
    sample_out->green = (float32_t)sample_in->green * MAX30101_CURRENT_LSB_NA;
}

/**
 * @brief Read single multi-LED sample from MAX30101 FIFO with current conversion
 * @details Optimized function for reading one sample at a time.
 *          Reads 6 bytes from FIFO (Red, IR, Green) and converts to nA immediately.
 *
 * @param sample - [out] Pointer to MAX30101_SampleCurrent for result
 * @return void
 * @see MAX30101_ReadFIFO_Current, MAX30101_GetNumAvailableSamples
 */
void MAX30101_ReadSingleCurrent(MAX30101_SampleCurrent *sample) {
    uint8_t fifo_data[6];
    uint16_t temp;

    // Read 6 bytes from FIFO data register
    I2C1_Read(SENSOR_ADDR, FIFO_DATAREG, fifo_data, 6);

    // Convert Red LED: combine bytes and scale to nanoamps
    temp = ((uint16_t)fifo_data[0] << 8) | fifo_data[1];
    sample->red = (float32_t)temp * MAX30101_CURRENT_LSB_NA;

    // Convert IR LED: combine bytes and scale to nanoamps
    temp = ((uint16_t)fifo_data[2] << 8) | fifo_data[3];
    sample->ir = (float32_t)temp * MAX30101_CURRENT_LSB_NA;

    // Convert Green LED: combine bytes and scale to nanoamps
    temp = ((uint16_t)fifo_data[4] << 8) | fifo_data[5];
    sample->green = (float32_t)temp * MAX30101_CURRENT_LSB_NA;
}