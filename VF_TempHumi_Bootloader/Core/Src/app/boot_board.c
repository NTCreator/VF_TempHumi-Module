/**
  ******************************************************************************
  * @file    boot_board.c
  ******************************************************************************
  */

#include "app/boot_board.h"

#include "app/app_board.h"
#include "app/boot_led.h"

#include "main.h"

/* TJA1044 STB via R2: LOW = Normal (CLAUDE.md 4.1). The standby side is
   app_board_can_transceiver_standby(). */
#define BOOT_CAN_STB_NORMAL   GPIO_PIN_RESET

#define BOOT_UID_WORDS        3u
#define BOOT_BITS_PER_BYTE    8u

void boot_board_can_transceiver_normal(void)
{
  HAL_GPIO_WritePin(EN_CAN1_GPIO_Port, EN_CAN1_Pin, BOOT_CAN_STB_NORMAL);
}

void boot_board_read_uid(uint8_t out[12])
{
  /* UID_BASE (0x1FFFF7AC, stm32f303x8.h) read through HAL, word by word,
     little-endian: byte 0 is the low byte of word 0. That is memory order -
     the order the other modules' bootloaders report and the mainboard logs. */
  const uint32_t w[BOOT_UID_WORDS] = { HAL_GetUIDw0(), HAL_GetUIDw1(), HAL_GetUIDw2() };

  for (uint32_t i = 0u; i < (BOOT_UID_WORDS * sizeof(uint32_t)); i++)
  {
    out[i] = (uint8_t)(w[i / sizeof(uint32_t)] >> ((i % sizeof(uint32_t)) * BOOT_BITS_PER_BYTE));
  }
}

void boot_board_code_forever(uint32_t code)
{
  /* HAL_InitTick programs SysTick to wrap once per millisecond on the core
     clock - HSI if SystemClock_Config failed, PLL otherwise - and COUNTFLAG
     latches on each wrap whether or not the interrupt is taken. Reading CTRL
     clears it. The guard bounds each wait (CLAUDE.md 6.3): if SysTick is not
     running, a "millisecond" becomes a few ms of spinning and the pattern
     slows down, but it can never stop with the LED lit. */
  const uint32_t guard_per_ms = SystemCoreClock / 1000u;
  uint32_t ms = 0u;

  for (;;)
  {
    app_board_led_set(boot_led_code_on(code, ms));

    uint32_t guard = guard_per_ms;
    while (((SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk) == 0u) && (guard > 0u))
    {
      guard--;
    }
    ms++;
  }
}
