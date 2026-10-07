/**
  ******************************************************************************
  * @file    app_led.h
  * @brief   The PB0 LED (L2), as the board's only voice before CAN exists.
  *
  * What it can say, strongest first:
  *   fault code N  - N flashes, a long pause, repeat: a fault is LATCHED
  *                   (app_fault.h) and stays until reset;
  *   status code N - same pattern, but LIVE: shown while a condition holds and
  *                   gone when it clears (phase 2: code 2, sensor silent);
  *   measuring     - one flash every 5 s: the reading is fresh;
  *   boot          - an even 2 Hz flash: running, nothing measured yet.
  *
  * Same patterns and numbers as VF_Angle_Module. Every pattern stays at or
  * below APP_LED_DUTY_PCT_MAX, checked at compile time (CLAUDE.md 6.5).
  ******************************************************************************
  */
#ifndef APP_APP_LED_H
#define APP_APP_LED_H

#include <stdbool.h>
#include <stdint.h>

/**
  * @brief Start the boot pattern. Call once, after app_board_enter_safe_state()
  *        and before anything that can raise a fault - it clears any pending
  *        code.
  * @param now_ms current HAL_GetTick() value
  */
void app_led_init(uint32_t now_ms);

/**
  * @brief Latch a fault code. Takes effect on the next app_led_task().
  *
  * Safe from an ISR: it writes one byte and returns. 0 is ignored. Values above
  * APP_LED_CODE_MAX are clamped, because a longer code would push the duty
  * cycle past the ceiling. Only app_fault should call this.
  */
void app_led_show_code(uint8_t blinks);

/**
  * @brief Set the live status code; 0 clears it. A latched fault code always
  *        wins over it. Call from the super-loop, as often as convenient.
  */
void app_led_set_status(uint8_t blinks);

/**
  * @brief Switch the quiet pattern: boot (2 Hz) or measuring (one flash every
  *        5 s). Codes, latched or live, always win over both.
  */
void app_led_set_measuring(bool measuring);

/**
  * @brief Advance the pattern. Call on every pass of the super-loop.
  *
  * Returns immediately, always: it never delays and never waits.
  *
  * @param now_ms current HAL_GetTick() value
  */
void app_led_task(uint32_t now_ms);

/**
  * @brief Blink a code forever without interrupts. Never returns.
  *
  * For the fatal path only (app_fault_fatal): interrupts are already off, so
  * HAL_GetTick() is frozen. Timing comes from polling the SysTick COUNTFLAG,
  * which keeps counting with interrupts disabled, so the pattern is the same
  * one app_led_task() draws. 0 keeps the LED dark.
  */
void app_led_code_forever(uint8_t blinks) __attribute__((noreturn));

#endif /* APP_APP_LED_H */
