/**
 * @file LED.h
 * @brief GPIO-Based Status LED Control (PB3, STM32F303K8)
 * @details Simple push-pull GPIO driver for visual feedback LED connected to port PB3.
 *
 * ### Hardware Interface
 *  - **Port**: GPIOB
 *  - **Pin**: PB3 (GPIO pin 3)
 *  - **Mode**: Push-pull, no pull-up/pull-down
 *  - **Output Type**: Standard CMOS push-pull (not open-drain)
 *  - **Speed**: Default (Medium, ~2 MHz slew rate)
 *  - **Voltage**: 3.3V (STM32F303 I/O level)
 *
 * ### Electrical Characteristics
 *  - **Output HIGH**: 3.1-3.3 V (min ~3 mA source capability)
 *  - **Output LOW**: 0.0-0.2 V (min ~3 mA sink capability)
 *  - **Typical LED Circuit**: GPIO → 220Ω resistor → LED anode, LED cathode → GND
 *  - **Max current per pin**: ±25 mA (STM32F303 absolute max)
 *  - **Typical LED consumption**: ~10-20 mA (for standard 3mm/5mm LED)
 *
 * ### Usage Context
 *  - **Primary purpose**: SysTick ISR feedback (100 ms toggle = 5 Hz blink)
 *  - **Non-critical**: LED state does not affect sensor operation
 *  - **No blocking**: All operations are non-blocking GPIO writes
 *  - **Timing**: Register writes ~1 CPU cycle (<20 ns)
 *
 * ### Data Flow
 *  1. LED_config() → Enables GPIOB clock, sets PB3 as push-pull output
 *  2. LED_On() → Sets ODR bit 3 (output high, LED on)
 *  3. LED_Off() → Clears ODR bit 3 (output low, LED off)
 *  4. LED_Toggle() → XORs ODR bit 3 (inverts state, called from SysTick ISR)
 *
 * @author Julio Fajardo
 * @date 2024-06-01
 * @version 2.0
 * @note Non-critical component; LED failures do not impact sensor or data acquisition
 * @todo Add configurable blink patterns (multiple on/off cycles per ISR)
 * @todo Implement PWM dimming control (via timer PWM output)
 */

#ifndef LED_H_
#define LED_H_

/**
 * @brief Initialize GPIO port B, pin 3 as push-pull output
 * @details One-time configuration:
 *          1. Enable GPIOB clock (RCC->AHBENR |= RCC_AHBENR_GPIOBEN)
 *          2. Configure PB3 as general-purpose output (MODER[7:6] = 01)
 *          3. Implicitly push-pull (OTYPER[3] = 0 default)
 *
 * @param None
 * @return void
 *
 * @register_ops
 *  - RCC->AHBENR |= RCC_AHBENR_GPIOBEN  (Enable GPIOB clock)
 *  - Clear MODER[7:6] and MODER[9:8] (both PB3 and PB4 to avoid confusion)
 *  - Set MODER[7:6] = 01 (PB3 = general-purpose output)
 *  - OTYPER[3] remains 0 (push-pull, default)
 *  - OSPEEDR[3] = 0 (Medium speed ~2 MHz, default)
 *
 * @timing
 *  - Clock enable: ~3 AHB cycles
 *  - Register writes: ~1 CPU cycle each
 *  - Total init time: <1 µs
 *
 * @side_effects
 *  - PB3 becomes output GPIO (unavailable for other uses: ADC, alternate functions)
 *  - GPIOB clock always enabled (minor power increase <1 mA)
 *  - Initial PB3 state: LOW (LED off) after reset
 *
 * @note Call before using LED_On(), LED_Off(), or LED_Toggle()
 * @see LED_On, LED_Off, LED_Toggle
 */
void LED_config(void);

/**
 * @brief Drive LED output HIGH (turn on)
 * @details Sets GPIO output register bit to high state.
 *          Non-blocking; takes one write to GPIO ODR register (~20 ns).
 * @param None
 * @return void
 * @note Safe to call repeatedly (bitwise OR is idempotent for already-set bits)
 * @see LED_Off, LED_Toggle
 */
void LED_On(void);

/**
 * @brief Drive LED output LOW (turn off)
 * @details Clears GPIO output register bit to low state.
 *          Non-blocking; takes one write to GPIO ODR register (~20 ns).
 * @param None
 * @return void
 * @note Safe to call repeatedly (bitwise AND is idempotent for already-clear bits)
 * @see LED_On, LED_Toggle
 */
void LED_Off(void);

/**
 * @brief Invert LED output state (high ↔ low)
 * @details XORs GPIO output register bit; used in periodic ISR for blink effect.
 *          Non-blocking; takes one RMW to GPIO ODR register (~30 ns).
 * @param None
 * @return void
 * @note Called from SysTick_Handler() every 100 ms for 5 Hz visual feedback
 * @see LED_On, LED_Off
 */
void LED_Toggle(void);

#endif /* LED_H_ */    