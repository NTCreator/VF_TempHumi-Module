/**
  ******************************************************************************
  * @file    boot_jump.c
  * @brief   After VF_Angle_Bootloader's boot_jump.c, minus HAL_DeInit():
  *          that board has no MAX485, this one does (phase 5 plan 2.5).
  ******************************************************************************
  */

#include "app/boot_jump.h"

#include "app/app_board.h"
#include "app/app_config.h"
#include "app/boot_config.h"
#include "app/boot_policy.h"

#include "can.h"
#include "main.h"

#include <stdint.h>

/* boot_config.h keeps the SRAM base as a plain number so boot_policy.h stays
   pure; the device header is the authority. */
_Static_assert(BOOT_SRAM_BASE == SRAM_BASE, "BOOT_SRAM_BASE must be the CMSIS SRAM_BASE");
_Static_assert(APP_BOOT_BASE == FLASH_BASE, "the bootloader starts flash (CMSIS FLASH_BASE)");

/* VTOR holds the table address in bits 31:9 on this core with 82 vectors:
   the application table must sit on a 512-byte boundary. */
#define VTOR_ALIGN   0x200u
_Static_assert((APP_IMAGE_BASE % VTOR_ALIGN) == 0u, "APP_IMAGE_BASE cannot be a vector table address");

#define NVIC_WORDS   (sizeof(NVIC->ICER) / sizeof(NVIC->ICER[0]))

bool boot_app_present(void)
{
  const volatile uint32_t *vectors = (const volatile uint32_t *)APP_IMAGE_BASE;

  return boot_policy_app_present(vectors[0], vectors[1]);
}

void boot_jump_to_app(void)
{
  const volatile uint32_t *vectors = (const volatile uint32_t *)APP_IMAGE_BASE;
  const uint32_t msp   = vectors[0];
  const uint32_t entry = vectors[1];

  /* 1. Transceiver to standby: from here until the application has its own
        config verdict, this module cannot reach CAN1. PB6 is NOT touched -
        it stays driven low, as it has been since app_board_early_guard(). */
  app_board_can_transceiver_standby();
  app_board_led_set(false);

  /* 2. Stop bxCAN and give PA11/PA12 back (HAL_CAN_DeInit stops it first,
        then HAL_CAN_MspDeInit gates its clock and resets only those two pins).
        A controller left running would keep ACKing frames for an image that
        no longer listens. HAL_CAN_DeInit returns HAL_OK for any non-NULL
        handle - it discards HAL_CAN_Stop's own status, which is an error when
        CAN never started (the Start-failed path) - so there is nothing to
        check, and nothing left to undo either way. */
  (void)HAL_CAN_DeInit(&hcan);

  /* 3. Clock back to HSI, the state the application's SystemClock_Config
        starts from after a reset - while SysTick still runs, because
        HAL_RCC_DeInit times out on HAL_GetTick(). It does not touch the
        peripheral enable or reset registers, so the GPIO ports keep driving.
        If it fails, jump anyway: the application's SystemClock_Config sets
        the same PLL configuration from any of these states, or lands in its
        own Error_Handler and shows code 1 - visible either way, whereas
        staying here would hide a working application behind a clock quirk. */
  (void)HAL_RCC_DeInit();

  /* 4. SysTick is core, not peripheral: nothing above stops it. One tick
        after the jump but before the application moves VTOR would run the
        BOOTLOADER's handler against the application's memory. */
  __disable_irq();
  SysTick->CTRL = 0u;
  SysTick->LOAD = 0u;
  SysTick->VAL  = 0u;
  SCB->ICSR = SCB_ICSR_PENDSTCLR_Msk;

  /* 5. Nothing enabled, nothing pending - the state a reset gives. */
  for (uint32_t i = 0u; i < NVIC_WORDS; i++)
  {
    NVIC->ICER[i] = 0xFFFFFFFFu;
    NVIC->ICPR[i] = 0xFFFFFFFFu;
  }

  /* 6. The table moves here AND in the application (main.c, USER CODE 1).
        Here closes the window between the jump and the application's own
        line; there makes a debugger-loaded image work. */
  SCB->VTOR = APP_IMAGE_BASE;

  /* Thread mode on MSP, privileged, no FP context active: CONTROL as a reset
     leaves it. */
  __set_CONTROL(0u);
  __DSB();
  __ISB();

  /* 7. Interrupts back on as a reset leaves them - every line is disabled
        and SysTick is off, so nothing can fire; the application's HAL_Init
        needs PRIMASK clear for its own SysTick. Then the application's stack
        pointer and its reset vector in ONE asm block, so the compiler cannot
        put a push between changing the stack and branching. */
  __enable_irq();
  __asm volatile (
      "msr msp, %0   \n"
      "bx  %1        \n"
      :
      : "r" (msp), "r" (entry)
      : "memory");

  for (;;)
  {
    /* not reached */
  }
}
