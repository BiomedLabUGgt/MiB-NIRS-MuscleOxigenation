/**
 * @file I2C.h
 * @brief I2C communication module for STM32F303K8
 * @details This module provides functions to configure and communicate with I2C1 peripheral
 * on pins PB6 (SCL) and PB7 (SDA) at 400 kHz operation speed.
 * @author Julio Fajardo
 */

#ifndef I2C_H_
#define I2C_H_

#include <stdint.h>

void I2C1_Config(void);
void I2C1_Write(uint8_t slave, uint8_t addr, uint8_t data);
void I2C1_Read(uint8_t slave, uint8_t addr, uint8_t *data, uint8_t size);

#endif /* I2C_H_ */    