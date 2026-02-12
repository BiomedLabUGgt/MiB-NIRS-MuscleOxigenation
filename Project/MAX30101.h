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

void MAX30101_Init(void);
void MAX30101_InitSPO2Lite(void);
void MAX30101_InitMuscleOx(uint8_t ledPower);
//void MAX30101_ReadFIFO(uint8_t *buffer, uint8_t size);
//void MAX30101_ReadTemp(float *temp);

#endif /* MAX30101_H_ */  
