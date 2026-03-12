# MiB-NIRS-MuscleOxigenation
A dual-mode firmware application for real-time monitoring of muscle oxygenation (NIRS) and blood oxygen saturation (SpO2) using the AD MAX30101 optical sensor integrated with the STM32F303K8 microcontroller for biomechanical and physiological analysis.

## Overview

This project implements dual-mode optical spectroscopy monitoring:
- **MuscleOx Mode**: Near-Infrared Spectroscopy (NIRS) with tri-channel (Red/IR/Green) measurement for muscle oxygenation and hemodynamics
- **SpO2 Mode**: Pulse oximetry with dual-channel (Red/IR) measurement for blood oxygen saturation and heart rate

The system uses the Maxim Integrated MAX30101 optical sensor interfaced via I2C to the STM32F303K8 ARM Cortex-M4 microcontroller, achieving real-time 16-bit ADC sampling with 7.81 pA resolution.

## Hardware Configuration

### Microcontroller
- **Device**: STM32F303K8T6 (ARM Cortex-M4, 64 KB Flash)
- **Clock**: PLL-configured to 64 MHz (HSI 8 MHz × PLL multiplier 16)
- **Status LED**: GPIO PB3 (push-pull output, 1 Hz toggle in 50 ms ISR)

### Optical Sensor
- **Device**: Analog Devices MAX30101 (Pulse Oximetry / NIRS)
- **I2C Address**: 0xAE (7-bit)
- **ADC**: 16-bit, 2048 nA full-scale, 7.81 pA LSB resolution
- **FIFO**: 32-sample buffer with programmable watermark

### Communication
- **I2C Interface**: I2C1 on STM32F303K8 (400 kHz)
  - **SCL**: PB6 (open-drain with pull-up)
  - **SDA**: PB7 (open-drain with pull-up)

### Real-Time Timer
- **SysTick**: Configured for 20 Hz (50 ms period)
  - Macro: `#define SYSTICK_FREQ_HZ   20`
  - Drives data acquisition ISR and LED toggle