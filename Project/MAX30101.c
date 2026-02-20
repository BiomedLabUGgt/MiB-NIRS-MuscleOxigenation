#include "MAX30101.h"
#include "I2C.h"
#include <stdint.h>

/**
 * @brief Initialize MAX30101 in SpO2 mode (dual-LED configuration)
 * @details Configures sensor for blood oxygen (SpO2) measurement with low power consumption.
 *          - Mode: SpO2 (Red + IR LEDs)
 *          - Sample Rate: 50 Hz
 *          - ADC Resolution: 16-bit
 *          - FIFO Configuration: Averaging 8, rollover enabled
 *          - LED Power: Low (minimal power draw, suitable for wearables)
 *          - Temperature Sensor: Enabled
 * @param None
 * @return void
 * @note Suitable for battery-powered wearable applications.
 *       Call this once during initialization before reading samples.
 * @see MAX30101_InitMuscleOx
 * @example
 *   MAX30101_InitSPO2Lite();
 *   uint8_t samples = MAX30101_GetNumAvailableSamples();
 */
void MAX30101_InitSPO2Lite(void){
    I2C1_Write(SENSOR_ADDR, FIFO_CONFIG, 0x4F);      // FIFO avg 8, FIFO rollover enabled
    I2C1_Write(SENSOR_ADDR, MODE_CONFIG, 0x03);      // SPO2 Mode (Red + IR)
    I2C1_Write(SENSOR_ADDR, SPO2_CONFIG, 0x23);      // 2048 range, sample rate 50 Hz, pulse width 215 (16 bits)
    I2C1_Write(SENSOR_ADDR, FIFO_READPTR, 0x0);      // Reset FIFO read pointer
    I2C1_Write(SENSOR_ADDR, FIFO_WRITPTR, 0x0);      // Reset FIFO write pointer
    I2C1_Write(SENSOR_ADDR, LED1_PAMPLI, 0x18);      // Red LED power (low)
    I2C1_Write(SENSOR_ADDR, LED2_PAMPLI, 0x18);      // IR LED power (low)
    I2C1_Write(SENSOR_ADDR, DIE_TEMPCFG, 0x01);      // Enable temperature sensor
}

/**
 * @brief Initialize MAX30101 for NIRS muscle oxygenation measurement
 * @details Configures sensor for Near-Infrared Spectroscopy with 3 simultaneous LEDs.
 *          Optimal for non-invasive tissue oxygenation and perfusion monitoring.
 *          - Mode: Multi-LED (Red + IR + Green)
 *          - Sample Rate: 100 Hz
 *          - ADC Resolution: 16-bit
 *          - FIFO Configuration: Averaging 8, rollover enabled
 *          - Temperature Sensor: Enabled
 * @param ledPower - LED current control register value (0x00 to 0xFF)
 *                  Range: 4.4 mA to 50.6 mA in ~0.2 mA steps
 *                  Typical: 0x4B (~20 mA), 0x18 (~10 mA) for low power
 * @return void
 * @note Higher sample rate (100 Hz) vs SPO2Lite (50 Hz) provides better temporal resolution.
 *       Three-LED configuration increases tissue penetration depth for muscle assessment.
 * @see MAX30101_InitSPO2Lite
 * @example
 *   MAX30101_InitMuscleOx(0x4B);  // 20 mA LED power
 *   uint8_t available = MAX30101_GetNumAvailableSamples();
 *   MAX30101_ReadFIFO_Current(current_buffer, available);
 */
void MAX30101_InitMuscleOx(uint8_t ledPower){
    I2C1_Write(SENSOR_ADDR, FIFO_CONFIG, 0x4F);         // FIFO avg 8, FIFO rollover enabled
    I2C1_Write(SENSOR_ADDR, MODE_CONFIG, 0x07);         // Multi-LED mode (Red + IR + Green)
    I2C1_Write(SENSOR_ADDR, SPO2_CONFIG, 0x26);         // 2048 range, sample rate 100 Hz, pulse width 215 (16 bits)
    I2C1_Write(SENSOR_ADDR, FIFO_READPTR, 0x0);         // Reset FIFO read pointer
    I2C1_Write(SENSOR_ADDR, FIFO_WRITPTR, 0x0);         // Reset FIFO write pointer 
    I2C1_Write(SENSOR_ADDR, LED1_PAMPLI, ledPower);     // Red LED power (medium)
    I2C1_Write(SENSOR_ADDR, LED2_PAMPLI, ledPower);     // IR LED power (medium)
    I2C1_Write(SENSOR_ADDR, LED3_PAMPLI, ledPower);     // Green LED power (medium)
    I2C1_Write(SENSOR_ADDR, DIE_TEMPCFG, 0x01);         // Enable temperature sensor
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
uint8_t MAX30101_GetNumAvailableSamples(void){
    uint8_t write_ptr = 0;
    uint8_t read_ptr = 0;
    uint8_t num_samples = 0;
    
    // Read FIFO write pointer and read pointer from sensor
    I2C1_Read(SENSOR_ADDR, FIFO_WRITPTR, &write_ptr, 1);
    I2C1_Read(SENSOR_ADDR, FIFO_READPTR, &read_ptr, 1);
    
    // Mask to 5 bits since FIFO pointers are 5 bits (0-31)
    write_ptr &= 0x1F;
    read_ptr &= 0x1F;
    
    // Calculate available samples
    if (write_ptr >= read_ptr) {
        num_samples = write_ptr - read_ptr;
    } else {
        num_samples = (32 - read_ptr) + write_ptr;
    }
    
    return num_samples;
}

/**
 * @brief Read raw FIFO data from MAX30101 (intermediate format)
 * @details Streams raw 2-byte ADC samples sequentially from each channel.
 *          Data is organized as: Red(2B) IR(2B) Green(2B) repeating.
 *          Each complete sample requires 6 bytes from FIFO.
 * @param samples - [out] Pointer to buffer of MAX30101_Sample structures to populate
 * @param num_samples - [in] Number of complete samples to read (max 32)
 * @return void
 * @retval N/A
 * @note This is the lowest-level FIFO read. For processed data in nanoamps,
 *       use MAX30101_ReadFIFO_Current() instead to skip intermediate conversion.
 * @warning Ensure buffer has capacity for num_samples structures.
 * @see MAX30101_ReadFIFO_Current, MAX30101_ConvertSampleToUint16
 * @example
 *   MAX30101_Sample raw[32];
 *   MAX30101_ReadFIFO(raw, 5);  // Read 5 samples
 */
void MAX30101_ReadFIFO(MAX30101_Sample *samples, uint8_t num_samples){
    uint8_t i = 0;
    uint8_t fifo_data[8];  // 6 bytes per complete sample (3 channels × 2 bytes)
    
    for (i = 0; i < num_samples; i++) {
        // Read 6 consecutive bytes from FIFO_DATAREG
        // Each I2C read from address 0x07 gets the next byte in the FIFO
        I2C1_Read(SENSOR_ADDR, FIFO_DATAREG, fifo_data, 6);
        
        // Store the 2-byte samples for each channel
        samples[i].red[0] = fifo_data[0];
        samples[i].red[1] = fifo_data[1];
        
        samples[i].ir[0] = fifo_data[2];
        samples[i].ir[1] = fifo_data[3];
        
        samples[i].green[0] = fifo_data[4];
        samples[i].green[1] = fifo_data[5];
    }
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
void MAX30101_ConvertSampleToUint16(MAX30101_Sample *sample_in, MAX30101_SampleData *sample_out){
    // Convert Red LED: combine 2 bytes into 16-bit uint16_t
    sample_out->red = ((uint16_t)sample_in->red[0] << 8) | 
                      ((uint16_t)sample_in->red[1]);
    
    // Convert IR LED: combine 2 bytes into 16-bit uint16_t
    sample_out->ir = ((uint16_t)sample_in->ir[0] << 8) | 
                     ((uint16_t)sample_in->ir[1]);
    
    // Convert Green LED: combine 2 bytes into 16-bit uint16_t
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
void MAX30101_ConvertUint16ToCurrent(MAX30101_SampleData *sample_in, MAX30101_SampleCurrent *sample_out){
    // Convert Red channel ADC count to current in nanoamps
    sample_out->red = (float32_t)sample_in->red * MAX30101_CURRENT_LSB_NA;
    
    // Convert IR channel ADC count to current in nanoamps
    sample_out->ir = (float32_t)sample_in->ir * MAX30101_CURRENT_LSB_NA;
    
    // Convert Green channel ADC count to current in nanoamps
    sample_out->green = (float32_t)sample_in->green * MAX30101_CURRENT_LSB_NA;
}

/**
 * @brief Optimized single-step FIFO read with direct nanoamp conversion
 * @details Combines all processing in one loop:
 *          1. Read 6 raw bytes from FIFO per sample
 *          2. Combine bytes to 16-bit ADC counts
 *          3. Scale to current (nA) using LSB=7.81 pA
 *          Output directly in calibrated units without intermediate storage.
 * @param samples - [out] Pointer to buffer of MAX30101_SampleCurrent to populate
 * @param num_samples - [in] Number of complete samples to read and convert (max 32)
 * @return void
 * @retval N/A
 * @note RECOMMENDED function for typical usage. Minimizes memory usage and processing
 *       overhead compared to three-step pipeline (Read → uint16 → nanoamps).
 * @warning Ensure output buffer has capacity for num_samples structures.
 *          Total bytes read from FIFO: num_samples × 6
 * @performance ~15-20% faster and ~24 bytes less stack vs separate conversions
 * @see MAX30101_GetNumAvailableSamples
 * @example
 *   MAX30101_SampleCurrent buf[32];
 *   uint8_t available = MAX30101_GetNumAvailableSamples();
 *   if (available) {
 *       MAX30101_ReadFIFO_Current(buf, available);
 *       // buf[0].red contains Red LED current in nanoamps
 *       // buf[0].ir contains IR LED current in nanoamps
 *       // buf[0].green contains Green LED current in nanoamps
 *   }
 */
void MAX30101_ReadFIFO_Current(MAX30101_SampleCurrent *samples, uint8_t num_samples){
    uint8_t fifo_data[6];
    uint8_t i;
    uint16_t temp;
    
    for(i = 0; i < num_samples; i++){
        // read 6 bytes from FIFO
        I2C1_Read(SENSOR_ADDR, FIFO_DATAREG, fifo_data, 6);
        // combine bytes and convert right away
        temp = ((uint16_t)fifo_data[0] << 8) | fifo_data[1];
        samples[i].red = (float32_t)temp * MAX30101_CURRENT_LSB_NA;
        
        temp = ((uint16_t)fifo_data[2] << 8) | fifo_data[3];
        samples[i].ir = (float32_t)temp * MAX30101_CURRENT_LSB_NA;
        
        temp = ((uint16_t)fifo_data[4] << 8) | fifo_data[5];
        samples[i].green = (float32_t)temp * MAX30101_CURRENT_LSB_NA;
    }
}
