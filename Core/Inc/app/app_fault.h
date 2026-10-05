/**
  ******************************************************************************
  * @file    app_fault.h
  * @brief   The one place a failure is allowed to end up.
  *
  * CLAUDE.md 6.3 and 6.6: no HAL status gets dropped on the floor, and every
  * early return in a data path increments a counter. A failure nothing reports
  * is worse than a failure that stops the board, because the board keeps
  * producing numbers that look like measurements - for this module type that
  * is the one failure that really matters (CLAUDE.md 1.1).
  *
  * The first fault latches and is never cleared - only a reset clears it.
  * Later faults are counted but do not overwrite it: the first failure is the
  * one with the explanation in it, everything after is usually its wake.
  *
  * Phase 1 has nothing to report except a failed HAL init. The module exists
  * now because every driver from phase 2 on reports through it, and adding it
  * later would mean revisiting every call site.
  ******************************************************************************
  */
#ifndef APP_APP_FAULT_H
#define APP_APP_FAULT_H

#include <stdint.h>

/* The numeric value IS the LED blink count, so these are part of the product's
   diagnostic language, not an internal enum. Never renumber a released code;
   append instead, up to APP_LED_CODE_MAX (checked in app_fault.c).

   Where a meaning exists on VF_Angle_Module too, keep its number: the plan is
   2 = sensor not answering (phase 2) and 3 = no valid config (phase 4), as on
   the angle module. Not reserved here - a code exists only once something can
   raise it. */
typedef enum
{
  APP_FAULT_NONE      = 0,
  APP_FAULT_HAL_INIT  = 1   /* clock config or a generated MX_*_Init failed: Error_Handler.
                               Detail = return address into the failing init */
} app_fault_t;

/**
  * @brief Record a fault and show it on the LED.
  *
  * Safe from an ISR: a short critical section guards the latch, and the LED
  * is only handed a flag that app_led_task() picks up on its next pass.
  *
  * @param code   what failed; APP_FAULT_NONE is ignored
  * @param detail whatever number helps identify it later - a raw register
  *               value, a HAL status, a return address. Zero if nothing.
  */
void app_fault_raise(app_fault_t code, uint32_t detail);

/**
  * @brief Latch the fault, make the board safe, blink the latched code
  *        forever. Never returns.
  *
  * Disables interrupts first, so nothing else runs again until reset: in
  * particular nothing can take the CAN transceiver out of standby or raise the
  * RS485 driver. If an earlier fault is already latched, that earlier code is
  * what blinks.
  */
void app_fault_fatal(app_fault_t code, uint32_t detail) __attribute__((noreturn));

/** @brief The latched fault, or APP_FAULT_NONE. */
app_fault_t app_fault_get(void);

/** @brief The detail recorded with the latched fault. */
uint32_t app_fault_detail(void);

/** @brief Every fault raised since reset, the latched one included. Saturates. */
uint32_t app_fault_count(void);

#endif /* APP_APP_FAULT_H */
