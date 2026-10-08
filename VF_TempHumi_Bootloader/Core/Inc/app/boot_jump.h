/**
  ******************************************************************************
  * @file    boot_jump.h
  * @brief   Is there an application, and handing over to it.
  ******************************************************************************
  */
#ifndef BOOT_JUMP_H
#define BOOT_JUMP_H

#include <stdbool.h>

/** @brief boot_policy_app_present() on the vector table at APP_IMAGE_BASE. */
bool boot_app_present(void);

/**
  * @brief Leave the bootloader and start the application. Never returns.
  *
  * Undoes what the bootloader switched on - CAN stopped and its pins released,
  * transceiver to standby, clock back to HSI, SysTick off, every NVIC line
  * disabled and cleared, VTOR moved - WITHOUT HAL_DeInit(). On the F3 that
  * call force-resets the AHB bus, where the GPIO ports are: PB6 would float,
  * R8/R9 would pull it to +5 V and the MAX485 would transmit until the
  * application's early guard (CLAUDE.md 4.3, 6.5). Here PB6, PA10 and PB0 are
  * driven without a break from the bootloader's early guard to the
  * application's.
  */
void boot_jump_to_app(void) __attribute__((noreturn));

#endif /* BOOT_JUMP_H */
