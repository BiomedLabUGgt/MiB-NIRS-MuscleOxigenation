# MiB-NIRS-MuscleOxigenation
A dual-mode firmware application for real-time monitoring of muscle oxygenation (NIRS) and blood oxygen saturation (SpO2) using the Maxim Integrated MAX30101 optical sensor interfaced to the STM32F303K8 microcontroller for biomechanical and physiological analysis.

## Overview

This project implements dual-mode optical spectroscopy monitoring:
- **SpO2 Lite Mode** *(active)*: Pulse oximetry with dual-channel (Red/IR) measurement — current configuration at 1.6 mA per LED
- **MuscleOx Mode**: Near-Infrared Spectroscopy (NIRS) with tri-channel (Red/IR/Green) measurement for muscle oxygenation and hemodynamics

The system uses the Maxim Integrated MAX30101 optical sensor interfaced via I2C to the STM32F303K8 ARM Cortex-M4 microcontroller, achieving real-time 16-bit ADC sampling at 50 Hz with 31.25 pA resolution. Calibrated photodiode current values (nA) are streamed over UART as CSV at 460800 baud.

## Hardware Configuration

### Microcontroller
- **Device**: STM32F303K8T6 (ARM Cortex-M4, 64 KB Flash)
- **Clock**: PLL-configured to 64 MHz (HSI 8 MHz × PLL multiplier 16)
- **Status LED**: GPIO PB3 (push-pull output, 25 Hz blink via 20 ms SysTick toggle)

### Optical Sensor
- **Device**: Maxim Integrated MAX30101 (Pulse Oximetry / NIRS)
- **I2C Address**: 0xAE (7-bit: 0x57)
- **ADC**: 16-bit, 2048 nA full-scale, 31.25 pA LSB resolution
- **Sample Rate**: 50 Hz (ODR), 118 µs pulse width
- **FIFO**: 32-sample circular buffer, rollover enabled

### Communication Interfaces
- **I2C1** (sensor): 400 kHz Fast-mode
  - **SCL**: PB6 (open-drain, AF4)
  - **SDA**: PB7 (open-drain, AF4)
- **USART2** (data output): 460800 baud, 8N1, blocking TX
  - **TX**: PA2 (AF7)
  - **RX**: PA15 (AF7)

### Real-Time Timer
- **SysTick**: Configured for 50 Hz (20 ms period)
  - Macro: `#define SYSTICK_FREQ_HZ   50`
  - Drives sensor FIFO polling and LED heartbeat toggle

## Data Output

Samples are transmitted over USART2 at 460800 baud as ASCII CSV:

```
<Red_nA>,<IR_nA>\r\n
```

Example:
```
1234.567,2345.678
```

- One line per SysTick interrupt (~50 Hz)
- Values in nanoamps (float, 3 decimal places)
- Receive with any serial terminal at 460800 8N1
