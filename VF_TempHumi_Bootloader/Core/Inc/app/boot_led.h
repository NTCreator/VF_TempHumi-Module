/**
  ******************************************************************************
  * @file    boot_led.h
  * @brief   The bootloader's LED patterns, as pure functions of the time.
  *
  * "Is the LED lit at time t?" - no state, so a pattern can never stick lit
  * (CLAUDE.md 6.5), and tests/test_boot_led.c measures every pattern on a PC.
  *
  *   double flash   listen window, session, rescue path (phase 5 plan 2.7)
  *   code N         the application's blink code, same timing (APP_LED_CODE_*):
  *                  N flashes, 2 s dark. The bootloader shows code 3 (no valid
  *                  config, nobody has spoken yet), code 5 (CAN would not
  *                  start) and code 1 (Error_Handler) - each meaning the same
  *                  as when the application shows it.
  *
  * Header only and pure (no HAL).
  ******************************************************************************
  */
#ifndef BOOT_LED_H
#define BOOT_LED_H

#include "app/app_config.h"
#include "app/app_fault.h"
#include "app/boot_config.h"

#include <stdbool.h>
#include <stdint.h>

/* The codes the bootloader shows, by their application names: the number IS
   the meaning, whichever image is showing it. */
#define BOOT_LED_CODE_HAL_INIT        ((uint32_t)APP_FAULT_HAL_INIT)   /* 1: Error_Handler */
#define BOOT_LED_CODE_UNCONFIGURED    ((uint32_t)APP_FAULT_CFG_CRC)    /* 3: no valid config */
#define BOOT_LED_CODE_CAN             ((uint32_t)APP_FAULT_CAN)        /* 5: CAN would not start */

/* One period of code N: N x ON, (N-1) x OFF, then GAP - the gap replaces the
   last OFF (app_config.h, "Fault code N"). */
#define BOOT_LED_CODE_PERIOD_MS(n)    (((n) * APP_LED_CODE_ON_MS) + (((n) - 1u) * APP_LED_CODE_OFF_MS) + \
                                       APP_LED_CODE_GAP_MS)

/* Duty ceiling (CLAUDE.md 6.5, APP_LED_DUTY_PCT_MAX), checked, not trusted. */
_Static_assert((2u * BOOT_LED_ON_MS * 100u) <= (APP_LED_DUTY_PCT_MAX * BOOT_LED_PERIOD_MS),
               "bootloader double flash exceeds APP_LED_DUTY_PCT_MAX");
_Static_assert((BOOT_LED_SECOND_AT_MS >= BOOT_LED_ON_MS) &&
               ((BOOT_LED_SECOND_AT_MS + BOOT_LED_ON_MS) < BOOT_LED_PERIOD_MS),
               "bootloader double flash: the two flashes must not touch and must fit the period");
_Static_assert((BOOT_LED_CODE_CAN * APP_LED_CODE_ON_MS * 100u) <=
               (APP_LED_DUTY_PCT_MAX * BOOT_LED_CODE_PERIOD_MS(BOOT_LED_CODE_CAN)),
               "bootloader code 5 exceeds APP_LED_DUTY_PCT_MAX");
_Static_assert((BOOT_LED_CODE_HAL_INIT < BOOT_LED_CODE_UNCONFIGURED) &&
               (BOOT_LED_CODE_UNCONFIGURED < BOOT_LED_CODE_CAN) &&
               (BOOT_LED_CODE_CAN <= APP_LED_CODE_MAX),
               "bootloader codes: 1 < 3 < 5 <= APP_LED_CODE_MAX (the longest is checked above)");

/** @brief Double flash: lit for BOOT_LED_ON_MS at 0 and at BOOT_LED_SECOND_AT_MS of each second. */
static inline bool boot_led_double_on(uint32_t now_ms)
{
  const uint32_t t = now_ms % BOOT_LED_PERIOD_MS;

  return (t < BOOT_LED_ON_MS) ||
         ((t >= BOOT_LED_SECOND_AT_MS) && (t < (BOOT_LED_SECOND_AT_MS + BOOT_LED_ON_MS)));
}

/**
  * @brief Code N, with the application's timing. Dark for N = 0 and for
  *        N > APP_LED_CODE_MAX: an impossible code says nothing rather than
  *        something wrong.
  */
static inline bool boot_led_code_on(uint32_t code, uint32_t now_ms)
{
  if ((code == 0u) || (code > APP_LED_CODE_MAX))
  {
    return false;
  }

  const uint32_t t = now_ms % BOOT_LED_CODE_PERIOD_MS(code);

  for (uint32_t i = 0u; i < code; i++)
  {
    const uint32_t start = i * (APP_LED_CODE_ON_MS + APP_LED_CODE_OFF_MS);
    if ((t >= start) && (t < (start + APP_LED_CODE_ON_MS)))
    {
      return true;
    }
  }
  return false;
}

#endif /* BOOT_LED_H */
