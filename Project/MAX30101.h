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
#define     SPO2_CONFIG			0x0A
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
#define     MAX30101_CURRENT_LSB_PA  7.81f  /**< LSB size in picoamps (pA) */
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
    float32_t green;     /**< Green LED current (0–2048 nA) */
} MAX30101_SampleCurrent;

void MAX30101_InitSPO2Lite(void);
void MAX30101_InitMuscleOx(uint8_t ledPower);
uint8_t MAX30101_GetNumAvailableSamples(void);
void MAX30101_ReadFIFO(MAX30101_Sample *samples, uint8_t num_samples);
void MAX30101_ReadFIFO_Current(MAX30101_SampleCurrent *samples, uint8_t num_samples);
void MAX30101_ConvertSampleToUint16(MAX30101_Sample *sample_in, MAX30101_SampleData *sample_out);
void MAX30101_ConvertUint16ToCurrent(MAX30101_SampleData *sample_in, MAX30101_SampleCurrent *sample_out);
//void MAX30101_ReadTemp(float *temp);

#endif /* MAX30101_H_ */  
