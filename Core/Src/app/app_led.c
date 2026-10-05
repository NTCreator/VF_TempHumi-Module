/**
  ******************************************************************************
  * @file    app_led.c
  * @brief   Non-blocking boot pattern and blink codes on the PB0 LED.
  ******************************************************************************
  */

#include "app/app_led.h"

#include "app/app_board.h"
#include "app/app_config.h"
#include "main.h"

#include <stdbool.h>

/* Duty is checked, not trusted: change a timing constant in app_config.h and a
   pattern that would leave L2 lit too long stops the build (CLAUDE.md 6.5). */
#if (APP_LED_BOOT_ON_MS * 100u) > (APP_LED_DUTY_PCT_MAX * APP_LED_BOOT_PERIOD_MS)
#error "Boot pattern duty cycle above APP_LED_DUTY_PCT_MAX"
#endif
#if APP_LED_BOOT_ON_MS >= APP_LED_BOOT_PERIOD_MS
#error "Boot pattern on-time must be shorter than its period"
#endif

/* A code of N flashes has period N*ON + (N-1)*OFF + GAP: the gap replaces the
   off-time after the last flash. Its duty is a ratio of two linear functions
   of N, so it is monotonic in N and the worst case is one of the two ends.
   Check both ends, and no N in between can exceed the ceiling. */
#define LED_CODE_PERIOD_MS(n) \
  (((n) * APP_LED_CODE_ON_MS) + (((n) - 1u) * APP_LED_CODE_OFF_MS) + APP_LED_CODE_GAP_MS)

#if (1u * APP_LED_CODE_ON_MS * 100u) > (APP_LED_DUTY_PCT_MAX * LED_CODE_PERIOD_MS(1u))
#error "Blink code 1 exceeds APP_LED_DUTY_PCT_MAX"
#endif
#if (APP_LED_CODE_MAX * APP_LED_CODE_ON_MS * 100u) > \
    (APP_LED_DUTY_PCT_MAX * LED_CODE_PERIOD_MS(APP_LED_CODE_MAX))
#error "Longest blink code exceeds APP_LED_DUTY_PCT_MAX - shorten it or lengthen the gap"
#endif

#define LED_BOOT_OFF_MS (APP_LED_BOOT_PERIOD_MS - APP_LED_BOOT_ON_MS)

static uint32_t s_phase_start_ms;
static uint32_t s_phase_len_ms;
static bool     s_is_on;

/* 0 means the boot pattern. Anything else is a blink code. */
static uint8_t  s_code;
static uint8_t  s_blinks_left;

/* Written by app_led_show_code(), which may run in an ISR; consumed by
   app_led_task() in the super-loop. One byte, one store: no critical section
   needed. */
static volatile uint8_t s_pending_code;

static void led_enter_phase(bool on, uint32_t len_ms, uint32_t now_ms)
{
  s_is_on = on;
  s_phase_len_ms = len_ms;
  s_phase_start_ms = now_ms;
  app_board_led_set(on);
}

static uint8_t led_clamp_code(uint8_t blinks)
{
  return (blinks > APP_LED_CODE_MAX) ? (uint8_t)APP_LED_CODE_MAX : blinks;
}

void app_led_init(uint32_t now_ms)
{
  s_code = 0u;
  s_blinks_left = 0u;
  s_pending_code = 0u;
  led_enter_phase(false, LED_BOOT_OFF_MS, now_ms);
}

void app_led_show_code(uint8_t blinks)
{
  if (blinks == 0u)
  {
    return;
  }
  s_pending_code = led_clamp_code(blinks);
}

void app_led_task(uint32_t now_ms)
{
  const uint8_t pending = s_pending_code;
  if ((pending != 0u) && (pending != s_code))
  {
    /* Start the new code from its pause, so the first group a human sees is a
       whole group and not the tail of one. */
    s_code = pending;
    s_blinks_left = pending;
    led_enter_phase(false, APP_LED_CODE_GAP_MS, now_ms);
    return;
  }

  /* Unsigned subtraction: stays correct across the 2^32 ms tick wrap
     (about 49.7 days) without a special case. */
  if ((now_ms - s_phase_start_ms) < s_phase_len_ms)
  {
    return;
  }

  if (s_code == 0u)
  {
    led_enter_phase(!s_is_on, s_is_on ? LED_BOOT_OFF_MS : APP_LED_BOOT_ON_MS, now_ms);
    return;
  }

  if (s_is_on)
  {
    s_blinks_left--;
    led_enter_phase(false,
                    (s_blinks_left > 0u) ? APP_LED_CODE_OFF_MS : APP_LED_CODE_GAP_MS,
                    now_ms);
  }
  else
  {
    if (s_blinks_left == 0u)
    {
      s_blinks_left = s_code;
    }
    led_enter_phase(true, APP_LED_CODE_ON_MS, now_ms);
  }
}

/* One millisecond, measured by SysTick with interrupts off.
   HAL_InitTick programs SysTick to wrap once per millisecond on the core clock,
   whichever clock that is - HSI 8 MHz if SystemClock_Config failed, PLL 72 MHz
   otherwise - and COUNTFLAG latches on each wrap whether or not the interrupt
   is taken. Reading CTRL clears it.

   The guard bounds the wait (CLAUDE.md 6.3): one core cycle per iteration at
   the very least, so it never cuts a real millisecond short. If SysTick is not
   running at all, each "millisecond" becomes a few ms of spinning: the pattern
   slows down, but on and off stretch equally, so the duty cycle is unchanged. */
static void led_poll_wait_ms(uint32_t ms)
{
  const uint32_t guard_per_ms = SystemCoreClock / 1000u;

  for (uint32_t i = 0u; i < ms; i++)
  {
    uint32_t guard = guard_per_ms;
    while (((SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk) == 0u) && (guard > 0u))
    {
      guard--;
    }
  }
}

void app_led_code_forever(uint8_t blinks)
{
  const uint8_t code = led_clamp_code(blinks);

  app_board_led_set(false);

  for (;;)
  {
    if (code == 0u)
    {
      /* Nothing to say, so say nothing - and never sit lit. */
      continue;
    }

    for (uint8_t i = 0u; i < code; i++)
    {
      app_board_led_set(true);
      led_poll_wait_ms(APP_LED_CODE_ON_MS);
      app_board_led_set(false);
      led_poll_wait_ms(((uint8_t)(i + 1u) < code) ? APP_LED_CODE_OFF_MS : APP_LED_CODE_GAP_MS);
    }
  }
}
