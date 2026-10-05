/**
  ******************************************************************************
  * @file    app_led.h
  * @brief   The PB0 LED (L2), as the board's only voice before CAN exists.
  *
  * Two things it can say:
  *   boot   - an even 2 Hz flash: firmware is running, nothing has failed,
  *            the module is not measuring yet;
  *   code N - N flashes, a long pause, repeat: a fault is latched and N says
  *            which one (app_fault.h).
  *
  * Same language as VF_Angle_Module. Every pattern stays at or below
  * APP_LED_DUTY_PCT_MAX, checked at compile time (CLAUDE.md 6.5).
  ******************************************************************************
  */
#ifndef APP_APP_LED_H
#define APP_APP_LED_H

#include <stdint.h>

/**
  * @brief Start the boot pattern. Call once, after app_board_enter_safe_state()
  *        and before anything that can raise a fault - it clears any pending
  *        code.
  * @param now_ms current HAL_GetTick() value
  */
void app_led_init(uint32_t now_ms);

/**
  * @brief Switch to a blink code. Takes effect on the next app_led_task().
  *
  * Safe from an ISR: it writes one byte and returns. 0 is ignored. Values above
  * APP_LED_CODE_MAX are clamped, because a longer code would push the duty
  * cycle past the ceiling.
  */
void app_led_show_code(uint8_t blinks);

/**
  * @brief Advance the pattern. Call on every pass of the super-loop.
  *
  * Returns immediately, always: it never delays and never waits. From phase 2
  * on the same loop services the Modbus receiver.
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
