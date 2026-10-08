/**
  ******************************************************************************
  * @file    boot_board.h
  * @brief   What the bootloader needs from the board beyond app_board.
  *
  * The pins themselves go through the application's app_board.c, linked into
  * this project (docs/VF_TH_IOC_Guide.md B6): app_board_early_guard() - the
  * PB6 code measured on the board in phase 1 -, app_board_enter_safe_state(),
  * app_board_can_transceiver_standby() and app_board_led_set().
  *
  * The bootloader NEVER calls app_board_can_transceiver_normal(): that one
  * demands a valid config, and the bootloader must answer on CAN2 without one.
  * Its unconditional counterpart lives here, in the bootloader project only,
  * so the application cannot link it by mistake.
  ******************************************************************************
  */
#ifndef BOOT_BOARD_H
#define BOOT_BOARD_H

#include <stdint.h>

/**
  * @brief TJA1044 to Normal, regardless of the config. Bootloader only.
  *
  * CAN2 5.1 and 5.3: a module with a blank config page must answer 0x7E0,
  * and it has one bxCAN and one transceiver, with no way to know which bus it
  * is plugged into. What keeps CAN1 safe is that the bootloader transmits ONLY
  * 0x7E8, ONLY in reply to 0x7E0 - which does not exist on CAN1 (CAN2 3) - and
  * never a TH_* frame. Phase 5 plan, question 1, approved 2026-10-07;
  * CLAUDE.md 6.5.
  */
void boot_board_can_transceiver_normal(void);

/** @brief The 96-bit unique ID, byte 0 first, as it sits at UID_BASE. */
void boot_board_read_uid(uint8_t out[12]);

/**
  * @brief Show blink code N for ever with interrupts off. Never returns.
  *
  * For Error_Handler, where the tick may not be running: milliseconds are
  * counted on SysTick COUNTFLAG, bounded so a stopped SysTick slows the
  * pattern down instead of hanging it lit. Same method as the application's
  * app_led_code_forever().
  */
void boot_board_code_forever(uint32_t code) __attribute__((noreturn));

#endif /* BOOT_BOARD_H */
