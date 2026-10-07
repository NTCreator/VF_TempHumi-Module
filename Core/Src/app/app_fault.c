/**
  ******************************************************************************
  * @file    app_fault.c
  ******************************************************************************
  */

#include "app/app_fault.h"

#include "app/app_board.h"
#include "app/app_config.h"
#include "app/app_led.h"
#include "main.h"

#include <stdbool.h>

/* A fault code IS its blink count, and app_led clamps anything above
   APP_LED_CODE_MAX down to it. A code past the ceiling would therefore not
   fail: it would blink exactly like a different fault and send whoever reads
   the LED after the wrong problem.

   _Static_assert rather than #if: the preprocessor cannot see enum constants,
   and in an #if APP_FAULT_CAN would quietly evaluate to 0. Keep this
   pointing at the highest code when appending one. */
_Static_assert((int)APP_FAULT_CAN <= (int)APP_LED_CODE_MAX,
               "Highest fault code exceeds APP_LED_CODE_MAX - the LED could not tell it apart");

static app_fault_t s_fault = APP_FAULT_NONE;
static uint32_t    s_detail;
static uint32_t    s_count;

void app_fault_raise(app_fault_t code, uint32_t detail)
{
  if (code == APP_FAULT_NONE)
  {
    return;
  }

  /* Check-then-set must not be split by an ISR raising its own fault, or the
     "first" fault would be whichever wrote last. Save and restore PRIMASK so
     this is correct from thread mode, from an ISR, and with interrupts
     already off (the fatal path). */
  const uint32_t primask = __get_PRIMASK();
  __disable_irq();

  if (s_count < UINT32_MAX)
  {
    s_count++;
  }

  bool latched_now = false;
  if (s_fault == APP_FAULT_NONE)
  {
    s_fault = code;
    s_detail = detail;
    latched_now = true;
  }

  __set_PRIMASK(primask);

  if (latched_now)
  {
    app_led_show_code((uint8_t)code);
  }
}

void app_fault_fatal(app_fault_t code, uint32_t detail)
{
  /* Interrupts off before anything else: from here on no ISR may run, and no
     code path exists that could raise the RS485 driver or put the transceiver
     into Normal. */
  __disable_irq();
  app_board_enter_safe_state();

  app_fault_raise(code, detail);

  app_led_code_forever((uint8_t)s_fault);
}

app_fault_t app_fault_get(void)
{
  return s_fault;
}

uint32_t app_fault_detail(void)
{
  return s_detail;
}

uint32_t app_fault_count(void)
{
  return s_count;
}
