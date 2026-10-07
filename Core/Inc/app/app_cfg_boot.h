/**
  ******************************************************************************
  * @file    app_cfg_boot.h
  * @brief   The application's verdict on the config, taken once at boot.
  *
  * Application only - the bootloader never links this. It reads the page
  * through app_cfg_store, validates it with app_cfg (the same function the
  * bootloader uses), acts on the bench switches, fills g_diag.cfg, and latches
  * fault code 3 when there is no valid config. Every other module asks
  * app_cfg_active() and nothing else: NULL means silent.
  *
  * Split out of app_cfg_store in phase 5 so the bootloader can link the flash
  * access without dragging g_diag, the fault module and the LED along.
  * Behaviour unchanged from phase 4.
  ******************************************************************************
  */
#ifndef APP_APP_CFG_BOOT_H
#define APP_APP_CFG_BOOT_H

#include "app/app_cfg.h"

#include <stdint.h>

#define APP_CFG_SOURCE_NONE     0u   /* no valid config: silent */
/* 1 was phase 3's compile-time constant - retired, never reused. */
#define APP_CFG_SOURCE_CONFIG   2u   /* a valid config was already in flash */
#define APP_CFG_SOURCE_BENCH    3u   /* this build wrote it (APP_CFG_BENCH_WRITE) */

#define APP_CFG_OP_NOT_TRIED    0xFFu   /* write_status / erase_status: not attempted */

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
  uint8_t  write_status;     /* APP_CFG_WR_* (app_cfg_store.h), or APP_CFG_OP_NOT_TRIED */
  uint8_t  erase_status;     /* APP_CFG_WR_*, or APP_CFG_OP_NOT_TRIED */
  uint8_t  module_type;      /* as stored */
  uint16_t version;          /* as stored */
  uint32_t magic;            /* as stored: 0x31464354 = "TCF1", 0xFFFFFFFF = blank */
  uint32_t crc_stored;
  uint32_t crc_calc;         /* over the 12 stored bytes */
  uint32_t writes;           /* page writes this boot: 0 on every boot after the first */
} app_cfg_diag_t;

/**
  * @brief Read and validate the config once, at boot - and act on the bench
  *        switches. Call after app_led_init() (it may latch fault code 3)
  *        and before app_th_init() (an erase stalls the CPU).
  */
void app_cfg_boot(void);

/** @brief The validated config, or NULL. There is no path from NULL to a node_id. */
const app_cfg_t *app_cfg_active(void);

#endif /* APP_APP_CFG_BOOT_H */
