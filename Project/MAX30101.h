/**
 * @file MAX30101.h
 * @brief MAX30101 Optical Biosensor Driver for NIRS Muscle Oxygenation
 * @details Complete driver for Maxim Integrated MAX30101 optical sensor supporting:
 *          - Multi-LED mode (Red, IR, Green) for NIRS measurements
 *          - Dual-LED mode (Red, IR) for SpO2 measurements
 *          - Direct current readout in nanoamps (nA) with 7.81 pA resolution
 *          - 16-bit ADC with 2048 nA full-scale range
 *          - 32-sample FIFO with wrap-around support
 * @author Julio Fajardo, PhD
 * @date 2024-06-01
 * @version 2.0
 */

#ifndef MAX30101_H_
#define MAX30101_H_

#include <stdint.h>
#include "arm_math.h"
#include "arm_math_types.h"

#define		SENSOR_ADDR 		0xAE

#define		INTR_STATUS1		0x00
#define		INTR_STATUS2		0x01
#define		INTR_ENABLE1		0x02
#define		INTR_ENABLE2		0x03
#define 	FIFO_WRITPTR 	    0x04
#define 	OVRF_COUNTER  	    0x05
#define 	FIFO_READPTR  	    0x06
#define 	FIFO_DATAREG  	    0x07
#define 	FIFO_CONFIG 	 	0x08
#define     MODE_CONFIG			0x09
#define     SPO2_CONFIG			0x0A
#define     PULSEWIDTH_CONFIG	0x0B
#define     LED1_PAMPLI			0x0C
#define     LED2_PAMPLI			0x0D
#define     LED3_PAMPLI			0x0E
#define     LED4_PAMPLI			0x0F
#define     MLED_CONFG1			0x11
#define     MLED_CONFG2			0x12
#define     DIE_TEMPINT			0x1F
#define     DIE_TEMPFRC			0x20
#define     DIE_TEMPCFG			0x21

#define     BUFFERBLOCKSIZE     0x8
#define     MAX30101_ADC_VREF   3.3f        /**< ADC reference voltage in volts */
#define     MAX30101_ADC_BITS   16          /**< ADC resolution in bits */
#define     MAX30101_ADC_MAX    ((1 << MAX30101_ADC_BITS) - 1)  /**< Max ADC count (65535 for 16-bit) */
#define     MAX30101_CURRENT_LSB_PA  31.25f  /**< LSB size in picoamps (pA) */
#define     MAX30101_CURRENT_LSB_NA  (MAX30101_CURRENT_LSB_PA / 1000.0f)  /**< LSB size in nanoamps (nA) */
#define     MAX30101_CURRENT_FULLSCALE  2048.0f  /**< Full scale current range in nanoamps (nA) */

/**
 * @struct MAX30101_Sample
 * @brief Raw FIFO sample data directly read from sensor (6 bytes) in multi-LED mode
 * @details Intermediate storage for raw 2-byte ADC values from each LED channel.
 *          Format: 3 channels × 2 bytes (MSB, LSB) per sample
 *          Total: 6 bytes per complete sample in multi-LED mode
 * @note Use MAX30101_ReadFIFO() to populate this structure
 * @see MAX30101_ReadFIFO, MAX30101_ConvertSampleToUint16
 */
typedef struct {
    uint8_t red[2];      /**< Red LED ADC raw bytes (MSB, LSB) */
    uint8_t ir[2];       /**< IR LED ADC raw bytes (MSB, LSB) */
    uint8_t green[2];    /**< Green LED ADC raw bytes (MSB, LSB) */
} MAX30101_Sample;

/**
 * @struct MAX30101_SampleData
 * @brief 16-bit unsigned integer representation of ADC counts
 * @details Intermediate format after combining 2 raw bytes per channel.
 *          Range: 0 to 65535 (16-bit ADC counts)
 *          Represents: Photodiode current as raw ADC values
 * @note Use MAX30101_ConvertSampleToUint16() to convert from raw samples
 * @see MAX30101_ConvertSampleToUint16, MAX30101_ReadFIFO_Current
 */
typedef struct {
    uint16_t red;        /**< Red LED 16-bit value as uint16_t */
    uint16_t ir;         /**< IR LED 16-bit value as uint16_t */
    uint16_t green;      /**< Green LED 16-bit value as uint16_t */
} MAX30101_SampleData;

/**
 * @struct MAX30101_SampleCurrent
 * @brief Calibrated photodiode current in nanoamps (nA)
 * @details Final processed format: ADC values scaled to current using 7.81 pA LSB.
 *          Range: 0 to 2048 nA (full-scale current)
 *          Resolution: 0.00781 nA per LSB
 * @note Use MAX30101_ReadFIFO_Current() or MAX30101_ConvertUint16ToCurrent() to generate
 * @see MAX30101_ReadFIFO_Current, MAX30101_ConvertUint16ToCurrent
 */
typedef struct {
    float32_t red;       /**< Red LED current (0–2048 nA) */
    float32_t ir;        /**< IR LED current (0–2048 nA) */
    float32_t green;     /**<Green LED current (0–2048 nA) */
} MAX30101_SampleCurrent;

/**
 * @struct MAX30101_SampleSpO2
 * @brief Raw FIFO sample data for SpO2 mode (4 bytes)
 * @details Dual-LED mode uses Red + IR channels.
 *          Format: 2 channels × 2 bytes (MSB, LSB)
 *          Total: 4 bytes per sample.
 * @note Use MAX30101_ReadFIFO() with SpO2 mode or a dedicated SpO2 read function
 */
typedef struct {
    uint8_t red[2];      /**< Red LED raw bytes (MSB, LSB) */
    uint8_t ir[2];       /**< IR LED raw bytes (MSB, LSB) */
} MAX30101_SampleSpO2;

/**
 * @struct MAX30101_SampleDataSpO2
 * @brief 16-bit ADC counts for SpO2 mode
 * @details Dual-LED data format after byte packing.
 * @note Use MAX30101_ConvertSampleToUint16() with SpO2 struct or a SpO2 conversion routine
 */
typedef struct {
    uint16_t red;        /**< Red ADC 16-bit value */
    uint16_t ir;         /**< IR ADC 16-bit value */
} MAX30101_SampleDataSpO2;

/**
 * @struct MAX30101_SampleCurrentSpO2
 * @brief Calibrated photodiode current in nA for SpO2 mode
 * @details Same 7.81 pA LSB scaling as multi-LED mode.
 */
typedef struct {
    float32_t red;       /**< Red current (0–2048 nA) */
    float32_t ir;        /**< IR current (0–2048 nA) */
} MAX30101_SampleCurrentSpO2;

/**
 * @brief Initialize MAX30101 in SpO2 mode (dual-LED: Red + IR)
 * @details Configures sensor for blood oxygen measurement with low power consumption.
 *          Sample rate: 50 Hz, FIFO rollover enabled, configurable LED power.
 * @param ledPower_red - Red LED current value (0.0 to 51 mA range)
 * @param ledPower_ir - IR LED current value (0.0 to 51 mA range)
 * @note Call once at startup before MAX30101_ReadFIFO_CurrentSpO2()
 * @see MAX30101_InitMuscleOx
 */
void MAX30101_InitSPO2Lite(float32_t ledPower_red, float32_t ledPower_ir);

/**
 * @brief Initialize MAX30101 for NIRS muscle oxygenation (multi-LED: Red + IR + Green)
 * @details Configures sensor for tissue oxygenation measurement with 3 LEDs.
 *          Sample rate: 50 Hz, FIFO rollover enabled, configurable LED power.
 * @param ledPower_red - Red LED current value (0.0 to 51 mA range)
 * @param ledPower_ir - IR LED current value (0.0 to 51 mA range)
 * @param ledPower_green - Green LED current value (0.0 to 51 mA range)
 * @note Call once at startup before MAX30101_ReadFIFO_Current()
 * @see MAX30101_InitSPO2Lite
 */
void MAX30101_InitMuscleOx(float32_t ledPower_red, float32_t ledPower_ir, float32_t ledPower_green);

uint8_t MAX30101_GetNumAvailableSamples(void);
/**< Query number of unread samples in FIFO (0-32) */

void MAX30101_UpdateReadPointer(uint8_t num_samples);
/**< Advance FIFO read pointer by num_samples with wrap-around */

// Conversion functions for SpO2 mode (Red + IR)
/**
 * @brief Convert raw SpO2 bytes to 16-bit ADC counts (0-65535)
 * @param sample_in - [in] MAX30101_SampleSpO2 with raw byte pairs
 * @param sample_out - [out] MAX30101_SampleDataSpO2 with uint16 counts
 */
void MAX30101_ConvertSampleToUint16SpO2(MAX30101_SampleSpO2 *sample_in, MAX30101_SampleDataSpO2 *sample_out);

/**
 * @brief Convert SpO2 ADC counts to current in nanoamps (0-2048 nA)
 * @param sample_in - [in] MAX30101_SampleDataSpO2 with ADC counts
 * @param sample_out - [out] MAX30101_SampleCurrentSpO2 with nA values
 */
void MAX30101_ConvertUint16ToCurrentSpO2(MAX30101_SampleDataSpO2 *sample_in, MAX30101_SampleCurrentSpO2 *sample_out);

/**
 * @brief Read single SpO2 sample from FIFO with current conversion
 * @details Optimized single-sample read converting 4 bytes directly to nanoamps
 * @param sample - [out] MAX30101_SampleCurrentSpO2 (Red, IR nA values)
 * @see MAX30101_GetNumAvailableSamples to check for available data
 */
void MAX30101_ReadSingleCurrentSpO2(MAX30101_SampleCurrentSpO2 *sample);

/**
 * @brief Read single SpO2 sample from FIFO as 16-bit ADC counts
 * @details Optimized single-sample read returning raw ADC values (0-65535)
 * @param sample - [out] MAX30101_SampleDataSpO2 with 16-bit ADC counts
 * @see MAX30101_GetNumAvailableSamples to check for available data
 */
void MAX30101_ReadSingleDataSpO2(MAX30101_SampleDataSpO2 *sample);

// Conversion functions for multi-LED mode (Red + IR + Green)
/**
 * @brief Convert raw multi-LED bytes to 16-bit ADC counts (0-65535)
 * @param sample_in - [in] MAX30101_Sample with raw byte pairs (6 bytes)
 * @param sample_out - [out] MAX30101_SampleData with uint16 counts (3 channels)
 */
void MAX30101_ConvertSampleToUint16(MAX30101_Sample *sample_in, MAX30101_SampleData *sample_out);

/**
 * @brief Convert multi-LED ADC counts to current in nanoamps (0-2048 nA)
 * @param sample_in - [in] MAX30101_SampleData with ADC counts
 * @param sample_out - [out] MAX30101_SampleCurrent with nA values (Red, IR, Green)
 */
void MAX30101_ConvertUint16ToCurrent(MAX30101_SampleData *sample_in, MAX30101_SampleCurrent *sample_out);

/**
 * @brief Read single multi-LED sample from FIFO with current conversion
 * @details Optimized single-sample read converting 6 bytes directly to nanoamps
 * @param sample - [out] MAX30101_SampleCurrent (Red, IR, Green nA values)
 * @see MAX30101_GetNumAvailableSamples to check for available data
 */
void MAX30101_ReadSingleCurrent(MAX30101_SampleCurrent *sample);

#endif /* MAX30101_H_ */  
