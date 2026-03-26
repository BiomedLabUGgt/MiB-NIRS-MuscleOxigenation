# MiB-NIRS-MuscleHemodynamics
A dual-mode firmware application for real-time monitoring of muscle hemodynamics through NIRS or blood oxygen saturation (SpO2) using the Maxim Integrated MAX30101 optical sensor interfaced to the STM32F303K8 microcontroller for biomechanical and physiological analysis.

## Overview

This project implements a single-mode optical spectroscopy monitoring:
- **NIRS Lite Mode**: NIRS-based hemodynamics monitoring or pulse oximetry with dual-channel (Red/IR) measurement — current configuration at 1.6 mA per LED

The system uses the Maxim Integrated MAX30101 optical sensor interfaced via I2C to the STM32F303K8 ARM Cortex-M4 microcontroller, achieving real-time 18-bit ADC sampling at 50 Hz with 15.625 pA resolution. Calibrated photodiode current values (nA) are streamed over UART as CSV at 460800 baud.

## Hardware Configuration

### Microcontroller
- **Device**: STM32F303K8T6 (ARM Cortex-M4, 64 KB Flash)
- **Clock**: PLL-configured to 64 MHz (HSI 8 MHz × PLL multiplier 16)
- **Status LED**: GPIO PB3 (push-pull output, 25 Hz blink via 20 ms SysTick toggle)

### Optical Sensor
- **Device**: Maxim Integrated MAX30101 (Pulse Oximetry / NIRS-based Hemodynamics)
- **I2C Address**: 0xAE (7-bit: 0x57)
- **ADC**: 18-bit, 4096 nA full-scale, 15.625 pA LSB resolution
- **Sample Rate**: 50 Hz (ODR), 411 µs pulse width
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

## Signal Processing

Two DC-removal high-pass filters are available, selected at compile time via the `FILTER_TYPE` macro in [Project/main.c](Project/main.c).

### First-Order IIR DC Blocker (`FILTER_TYPE 0` — default)

A minimal recursive high-pass filter implemented as a direct-form difference equation:

```
H(z) = (1 - z⁻¹) / (1 - α·z⁻¹)
```

In the time domain:

```
w[n]  = x[n] + α·w[n-1]
y[n]  = w[n] - w[n-1]
```

where `w[n]` is the internal state variable, `x[n]` is the raw input sample, and `y[n]` is the DC-blocked output. The pole at `z = α` sets the cutoff frequency:

```
fc = (1 - α) · fs / (2π)   (approximate, for α close to 1)
```

| Parameter | Value | Notes |
|-----------|-------|-------|
| `ALPHA` | 0.95 | fc ≈ 0.4 Hz at fs = 50 Hz |
| `ALPHA` | 0.995 | fc ≈ 0.04 Hz at fs = 50 Hz |
| State variables | `w_red`, `w_ir` | One per channel, initialized to 0 |

**Advantages**: Near-zero CPU cost, single multiply-add per sample, no CMSIS-DSP dependency. Suitable for resource-constrained operation.

---

### 4th-Order Chebyshev Type II High-Pass Filter (`FILTER_TYPE 1`)

A higher-order IIR filter implemented as a cascade of **2 biquad sections** (Direct Form II Transposed) via CMSIS-DSP `arm_biquad_cascade_df2T_f32`. Each biquad section has the transfer function:

```
H_k(z) = (b₀ + b₁·z⁻¹ + b₂·z⁻²) / (1 - a₁·z⁻¹ - a₂·z⁻²)
```

The overall filter is the product of both sections: `H(z) = H₁(z) · H₂(z)`.

**Filter specifications:**

| Parameter | Value |
|-----------|-------|
| Type | Chebyshev Type II (equiripple stopband) |
| Order | 4 (two 2nd-order biquad sections) |
| Topology | Cascade biquads, Direct Form II Transposed |
| Cutoff frequency (fc) | 0.04 Hz |
| Sampling frequency (fs) | 50 Hz |
| Stopband ripple | Equiripple (Chebyshev Type II characteristic) |
| Passband | Maximally flat above fc |

**Biquad coefficients** (CMSIS-DSP format `[b0, b1, b2, a1, a2]`, feedback negated):

| Section | b0 | b1 | b2 | a1 | a2 |
|---------|-----|-----|-----|-----|-----|
| 1 | 0.98855555 | −1.9770899 | 0.98855555 | 1.9766545 | −0.97754645 |
| 2 | 0.97310543 | −1.9462072 | 0.97310543 | 1.9457787 | −0.94663936 |

Coefficients were designed using MATLAB's `fdesign.highpass` with a Chebyshev Type II prototype.

**Advantages**: Maximally flat passband with equiripple stopband attenuation. Preferred for clean PPG/NIRS signal extraction where passband distortion must be minimized.

---

### Filter Selection

Set the `FILTER_TYPE` macro in [Project/main.c:29](Project/main.c#L29) before building:

```c
#define FILTER_TYPE  0   // First-order DC Blocker (default, low cost)
#define FILTER_TYPE  1   // 4th-order Chebyshev Type II (higher quality)
```

When `FILTER_TYPE == 1`, `arm_biquad_cascade_df2T_init_f32()` is called once after `clk_config()` to initialize the CMSIS-DSP filter instances for both Red and IR channels.
