/**
  ******************************************************************************
  * @file    boot_config.h
  * @brief   The bootloader's own constants.
  *
  * Everything both images must agree on - the flash map, the config format
  * and its rules, the LED timing and duty ceiling - comes from the
  * application's app_config.h instead, through the include path
  * "${workspace_loc:/VF_TempHumi_Module/Core/Inc}" (docs/VF_TH_IOC_Guide.md
  * B6). One copy of each number, so the two images cannot drift apart.
  *
  * Pure: no HAL. tests/ compiles the pure bootloader modules on a PC.
  ******************************************************************************
  */
#ifndef BOOT_CONFIG_H
#define BOOT_CONFIG_H

#include "app/app_config.h"

#include <stdint.h>

/* ---- Identity ---------------------------------------------------- */
/* Reported in HELLO bytes 5..7 (CAN2 section 4) and embedded in the image as
   s_boot_id (main.c), where a debugger or `strings` on the .elf finds it.
   0.1.0 = phase 5, the first bootloader of this module type. */
#define BOOT_FW_MAJOR             0u
#define BOOT_FW_MINOR             1u
#define BOOT_FW_PATCH             0u
#define BOOT_FW_ID                "VF_TempHumi_Bootloader 0.1.0"

/* ---- CAN2 timing ------------------------------------------------- */
/* CAN2 section 5, invariant 5: listen for 0x7E0 at least this long after
   power-up before jumping. A contract term, not a tuning knob. Only a module
   with an application AND a valid config ever leaves through this window
   (boot_policy.h); every other module stays for ever. */
#define BOOT_LISTEN_WINDOW_MS     1000u

/* How long a 0x7E8 response may wait to leave its mailbox. CAN2 section 3
   gives the module 50 ms to answer. bxCAN runs here with automatic
   retransmission off (the generated MX_CAN_Init, NART = 1), so a frame
   normally leaves or fails within one frame time; the bound is for the case
   where it does neither. Then it is aborted, never left in a mailbox. */
#define BOOT_SEND_TIMEOUT_MS      20u
_Static_assert(BOOT_SEND_TIMEOUT_MS < 50u, "CAN2 section 3: the module must answer within 50 ms");

/* ---- Is there an application? ------------------------------------ */
/* An application's first vector is its initial stack pointer, which its
   linker script puts at the end of SRAM (_estack = 0x20003000). SRAM: 12 KB
   at 0x20000000 on the STM32F303K8 (app_config.h, "Memory"). boot_jump.c
   checks BOOT_SRAM_BASE against the CMSIS SRAM_BASE at compile time - here it
   is a plain number so boot_policy.h stays pure. */
#define BOOT_SRAM_BASE            0x20000000u
#define BOOT_SRAM_SIZE            (APP_MCU_SRAM_KB * 1024u)

/* ---- LED (L2 on PB0) --------------------------------------------- */
/* The bootloader's own pattern: a DOUBLE flash every second - 50 ms on,
   150 ms off, 50 ms on, 750 ms off. Different at a glance from everything the
   application shows (2 Hz = starting, one flash per 5 s = measuring, N
   flashes + 2 s dark = code N). Same pattern as VF_Angle_Bootloader, so a
   technician reads every module type the same way (phase 5 plan 2.7).
   2 x 50 ms per 1000 ms = exactly the 10% ceiling, checked in boot_led.h. */
#define BOOT_LED_PERIOD_MS        1000u
#define BOOT_LED_ON_MS            50u
#define BOOT_LED_SECOND_AT_MS     200u    /* start of the second flash */

#endif /* BOOT_CONFIG_H */
