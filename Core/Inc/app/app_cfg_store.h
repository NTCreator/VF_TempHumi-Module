/**
  ******************************************************************************
  * @file    app_cfg_store.h
  * @brief   The config page in flash: read, write, erase, and the boot-time
  *          verdict every other module asks for.
  *
  * The page is the last 2 KB of flash, APP_CFG_FLASH_ADDR. Reading is a plain
  * memcpy (flash is memory mapped). Writing erases the whole page and programs
  * 16 bytes, then reads them back.
  *
  * Between the erase and the end of programming the page holds a record that
  * fails its own CRC, so a power cut at that moment leaves an unconfigured -
  * and therefore silent - module (CAN2 section 5, invariant 3), never one
  * with a guessed node_id.
  *
  * In phase 4 only the bench switches write (app_config.h); in phase 5 the
  * bootloader's COMMIT will, while no application runs. Never while measuring.
  ******************************************************************************
  */
#ifndef APP_APP_CFG_STORE_H
#define APP_APP_CFG_STORE_H

#include "app/app_cfg.h"

#include <stdbool.h>
#include <stdint.h>

#define APP_CFG_SOURCE_NONE     0u   /* no valid config: silent */
/* 1 was phase 3's compile-time constant - retired, never reused. */
#define APP_CFG_SOURCE_CONFIG   2u   /* a valid config was already in flash */
#define APP_CFG_SOURCE_BENCH    3u   /* this build wrote it (APP_CFG_BENCH_WRITE) */

#define APP_CFG_OP_NOT_TRIED    0xFFu
#define APP_CFG_OP_OK           0u
#define APP_CFG_OP_FAILED       1u

/* g_diag.cfg */
typedef struct
{
  uint8_t  state;            /* app_cfg_state_t of the config in use */
  uint8_t  state_at_boot;    /* app_cfg_state_t as found, before any bench action */
  uint8_t  node_id;          /* from the config, or 0xFF */
  uint8_t  node_id_source;   /* APP_CFG_SOURCE_* */
  uint8_t  bench_write;      /* the switches this build was made with */
  uint8_t  bench_erase;
  uint8_t  bench_loopback;
  uint8_t  write_status;     /* APP_CFG_OP_* */
  uint8_t  erase_status;     /* APP_CFG_OP_* */
  uint8_t  module_type;      /* as stored */
  uint16_t version;          /* as stored */
  uint32_t magic;            /* as stored: 0x31464354 = "TCF1", 0xFFFFFFFF = blank */
  uint32_t crc_stored;
  uint32_t crc_calc;         /* over the 12 stored bytes */
  uint32_t writes;           /* page writes this boot: 0 on every boot after the first */
} app_cfg_diag_t;

/** @brief Copy the 16 bytes at APP_CFG_FLASH_ADDR. A blank page reads 0xFF. */
void app_cfg_store_read(app_cfg_t *out);

/**
  * @brief Seal, erase the page, program, lock, read back and compare.
  * @return true only if the bytes in flash now equal the sealed record.
  */
bool app_cfg_store_write(app_cfg_t *c);

/** @brief Erase the page (bench only). True if it reads blank afterwards. */
bool app_cfg_store_erase(void);

/**
  * @brief Read and validate the config once, at boot - and act on the bench
  *        switches. Call after app_led_init() (it may latch fault code 3)
  *        and before app_th_init() (an erase stalls the CPU).
  */
void app_cfg_boot(void);

/** @brief The validated config, or NULL. There is no path from NULL to a node_id. */
const app_cfg_t *app_cfg_active(void);

#endif /* APP_APP_CFG_STORE_H */
