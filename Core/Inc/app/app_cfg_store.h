/**
  ******************************************************************************
  * @file    app_cfg_store.h
  * @brief   The config page in flash: read, write, erase. Nothing else.
  *
  * Shared by BOTH images: the application reads the page at boot, and the
  * bootloader's COMMIT (CAN2 0x20) writes it - through this very file, linked
  * into VF_TempHumi_Bootloader, never copied. So it depends on nothing but
  * app_cfg and the HAL flash driver: no g_diag, no fault module, no LED.
  * The application's use of the result (g_diag.cfg, fault code 3, the bench
  * switches) is in app_cfg_boot.
  *
  * The page is the last 2 KB of flash, APP_CFG_FLASH_ADDR. Reading is a plain
  * memcpy (flash is memory mapped). Writing erases the whole page, programs
  * 16 bytes, then reads them back.
  *
  * Between the erase and the end of programming the page holds a record that
  * fails its own CRC, so a power cut at that moment leaves an unconfigured -
  * and therefore silent - module (CAN2 section 5, invariant 3), never one
  * with a guessed node_id.
  ******************************************************************************
  */
#ifndef APP_APP_CFG_STORE_H
#define APP_APP_CFG_STORE_H

#include "app/app_cfg.h"

#include <stdint.h>

/* Result of a write or an erase. 0 = done and verified; anything else names
   the step that failed. The bootloader answers CAN2 0x03 for any non-zero. */
#define APP_CFG_WR_OK         0u
#define APP_CFG_WR_UNLOCK     1u   /* HAL_FLASH_Unlock refused */
#define APP_CFG_WR_ERASE      2u   /* page erase failed */
#define APP_CFG_WR_PROGRAM    3u   /* a half-word program failed */
#define APP_CFG_WR_LOCK       4u   /* HAL_FLASH_Lock refused */
#define APP_CFG_WR_VERIFY     5u   /* read back does not match - the only result that counts */

/** @brief Copy the 16 bytes at APP_CFG_FLASH_ADDR. A blank page reads 0xFF. */
void app_cfg_store_read(app_cfg_t *out);

/**
  * @brief Seal, erase the page, program, lock, read back and compare.
  * @return APP_CFG_WR_OK only if the bytes in flash now equal the sealed record.
  */
uint8_t app_cfg_store_write(app_cfg_t *c);

/** @brief Erase the page. APP_CFG_WR_OK only if the whole page reads blank after. */
uint8_t app_cfg_store_erase(void);

#endif /* APP_APP_CFG_STORE_H */
