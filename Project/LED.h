/**
 * @file LED.h
 * @brief LED control module for STM32F303K8
 * @details This module provides functions to control an LED connected to GPIO pin PB3.
 * Supports turning on, off, and toggling the LED state.
 * @author Julio Fajardo
 */

#ifndef LED_H_
#define LED_H_

void LED_config(void);
void LED_On(void); 
void LED_Off(void);
void LED_Toggle(void);

#endif /* LED_H_ */    