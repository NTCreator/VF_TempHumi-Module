/**
  ******************************************************************************
  * @file    boot_policy.h
  * @brief   What the bootloader does at power-up. Decided once.
  *
  * CAN2 section 5, invariant 5 (v1.4): listen for 0x7E0 at least 1000 ms,
  * then jump - EXCEPT in two cases, where the bootloader stays for ever:
  *
  *   no application        the rescue path: blank or broken application flash
  *                         must still answer, because that is when someone
  *                         needs to configure the module most;
  *   config not valid      the application would find no valid config and sit
  *                         silent on CAN1 (invariant 3), deaf to CAN2 - and J2
  *                         powers a module permanently, so a 1 s window closes
  *                         long before anyone types 'mod open' (CAN2 5.5).
  *
  * The config test is app_cfg_check(): the application's own rule, linked from
  * VF_TempHumi_Module (app_cfg.c), never copied. So, by construction:
  *
  *     the bootloader jumps  <=>  the application will find its config valid
  *                                 and transmit.
  *
  * Header only and pure (no HAL), so tests/test_boot_policy.c runs it on a PC.
  ******************************************************************************
  */
#ifndef BOOT_POLICY_H
#define BOOT_POLICY_H

#include "app/app_cfg.h"
#include "app/app_config.h"
#include "app/boot_config.h"

#include <stdbool.h>
#include <stdint.h>

/* Returned in a uint8_t: compared, never stored as an enum. */
#define BOOT_POLICY_LISTEN              0u   /* application + valid config: listen, then jump */
#define BOOT_POLICY_STAY_RESCUE         1u   /* no application: stay and serve CAN2 for ever */
#define BOOT_POLICY_STAY_UNCONFIGURED   2u   /* application, config not valid: stay for ever */

/**
  * @brief  Does the vector table at APP_IMAGE_BASE look like an application?
  * @param  msp           word 0: the initial stack pointer
  * @param  reset_vector  word 1: the reset handler
  *
  * Plausibility, not verification: the stack pointer must land inside SRAM
  * (the top end included - _estack is exactly the end of SRAM), the reset
  * vector must be odd (Thumb, the only state a Cortex-M4 has) and inside the
  * application region. Erased flash reads 0xFFFFFFFF and fails all three -
  * the case that matters. Not a CRC: a truncated image can pass. That gap
  * closes with firmware download over CAN2, when there is a length and a
  * checksum worth storing (CAN2 section 7).
  */
static inline bool boot_policy_app_present(uint32_t msp, uint32_t reset_vector)
{
  if ((msp < BOOT_SRAM_BASE) || (msp > (BOOT_SRAM_BASE + BOOT_SRAM_SIZE)))
  {
    return false;
  }
  if ((reset_vector & 1u) == 0u)
  {
    return false;
  }
  if ((reset_vector < APP_IMAGE_BASE) || (reset_vector >= (APP_IMAGE_BASE + APP_IMAGE_SIZE)))
  {
    return false;
  }
  return true;
}

/**
  * @brief  Decide once, at power-up, after CAN has started.
  * @param  app_present  boot_policy_app_present() of the vectors at APP_IMAGE_BASE
  * @param  cfg_state    app_cfg_check() of the config page as it is in flash
  * @return BOOT_POLICY_*
  *
  * No application wins over no config: there is nothing to jump to either way,
  * and the rescue path is the older, stronger statement.
  */
static inline uint8_t boot_policy_at_power_up(bool app_present, uint8_t cfg_state)
{
  if (!app_present)
  {
    return BOOT_POLICY_STAY_RESCUE;
  }
  if (cfg_state != (uint8_t)APP_CFG_OK)
  {
    return BOOT_POLICY_STAY_UNCONFIGURED;
  }
  return BOOT_POLICY_LISTEN;
}

#endif /* BOOT_POLICY_H */
