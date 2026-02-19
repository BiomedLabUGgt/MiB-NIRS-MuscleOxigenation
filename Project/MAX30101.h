/**
 * @file MAX30101.h
 * @brief MAX30101 Pulse Oximetry and NIRS Module
 * @details This module provides functions to configure and control the AD MAX30101
 * optical sensor for pulse oximetry (SpO2), heart rate monitoring, and NIRS (Near-Infrared
 * Spectroscopy) measurements for muscle oxygenation analysis.
 * @author Julio Fajardo
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
 * @brief Holds one complete sample from MAX30101 FIFO in multi-LED mode
 * @details Each channel (Red, IR, Green) is stored as a 2-byte value (16-bit)
 */
typedef struct {
    uint8_t red[2];      /**< Red LED sample (2 bytes) */
    uint8_t ir[2];       /**< IR LED sample (2 bytes) */
    uint8_t green[2];    /**< Green LED sample (2 bytes) */
} MAX30101_Sample;

/**
 * @struct MAX30101_SampleData
 * @brief Holds one complete sample with 2-byte values converted to uint16_t
 * @details Each channel is converted from 2 bytes to a 16-bit value in uint16_t
 */
typedef struct {
    uint16_t red;        /**< Red LED 16-bit value as uint16_t */
    uint16_t ir;         /**< IR LED 16-bit value as uint16_t */
    uint16_t green;      /**< Green LED 16-bit value as uint16_t */
} MAX30101_SampleData;

/**
 * @struct MAX30101_SampleCurrent
 * @brief Holds one complete sample with data converted to current (nA)
 * @details Each channel value is converted to current in nanoamps using LSB size
 */
typedef struct {
    float32_t red;       /**< Red LED current in nanoamps (nA) */
    float32_t ir;        /**< IR LED current in nanoamps (nA) */
    float32_t green;     /**< Green LED current in nanoamps (nA) */
} MAX30101_SampleCurrent;

void MAX30101_InitSPO2Lite(void);
void MAX30101_InitMuscleOx(uint8_t ledPower);
uint8_t MAX30101_GetNumAvailableSamples(void);
void MAX30101_ReadFIFO(MAX30101_Sample *samples, uint8_t num_samples);
void MAX30101_ConvertSampleToUint16(MAX30101_Sample *sample_in, MAX30101_SampleData *sample_out);
void MAX30101_ConvertUint16ToCurrent(MAX30101_SampleData *sample_in, MAX30101_SampleCurrent *sample_out);
//void MAX30101_ReadTemp(float *temp);

#endif /* MAX30101_H_ */  
