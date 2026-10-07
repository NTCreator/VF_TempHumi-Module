/**
  ******************************************************************************
  * @file    app_cfg_boot.c
  * @brief   The application's config verdict at boot. Application only.
  ******************************************************************************
  */

#include "app/app_cfg_boot.h"

#include "app/app_cfg_store.h"
#include "app/app_diag.h"
#include "app/app_fault.h"

#include <stdbool.h>

/* One translation unit says it, so a bench build carries exactly one warning
   per switch - and a clean build carries none (CLAUDE.md 3: every warning
   explained). Here and not in app_cfg_store.c, which the bootloader links:
   the switches are the application's. */
#if APP_CFG_BENCH_WRITE
#warning "APP_CFG_BENCH_WRITE = 1: this build writes a bench config to flash - never ship it"
#endif
#if APP_CFG_BENCH_ERASE
#warning "APP_CFG_BENCH_ERASE = 1: this build erases the config page at every boot - never ship it"
#endif
#if APP_BENCH_CAN_LOOPBACK
#warning "APP_BENCH_CAN_LOOPBACK = 1: this build never speaks on CAN1 - never ship it"
#endif

static app_cfg_t s_cfg;      /* RAM copy of the record in flash */
static bool      s_valid;

void app_cfg_boot(void)
{
  app_cfg_diag_t *const d = &g_diag.cfg;

  s_valid = false;
  d->bench_write    = (uint8_t)APP_CFG_BENCH_WRITE;
  d->bench_erase    = (uint8_t)APP_CFG_BENCH_ERASE;
  d->bench_loopback = (uint8_t)APP_BENCH_CAN_LOOPBACK;
  d->write_status   = APP_CFG_OP_NOT_TRIED;
  d->erase_status   = APP_CFG_OP_NOT_TRIED;

#if APP_CFG_BENCH_ERASE
  d->erase_status = app_cfg_store_erase();
#endif

  app_cfg_store_read(&s_cfg);
  uint8_t state = app_cfg_check(&s_cfg);
  d->state_at_boot = state;

  uint8_t source = APP_CFG_SOURCE_CONFIG;

#if APP_CFG_BENCH_WRITE
  /* Only when the page holds no valid config: a bench build that finds one
     leaves it alone, so the page is written once, not on every boot. */
  if (state != (uint8_t)APP_CFG_OK)
  {
    app_cfg_t bench;
    app_cfg_defaults(&bench);
    bench.node_id = (uint8_t)APP_NODE_ID_BENCH;

    d->writes++;
    d->write_status = app_cfg_store_write(&bench);

    app_cfg_store_read(&s_cfg);
    state = app_cfg_check(&s_cfg);
    source = APP_CFG_SOURCE_BENCH;
  }
#endif

  s_valid = (state == (uint8_t)APP_CFG_OK);

  d->state       = state;
  d->magic       = s_cfg.magic;
  d->version     = s_cfg.version;
  d->module_type = s_cfg.module_type;
  d->crc_stored  = s_cfg.crc32;
  d->crc_calc    = app_cfg_crc32((const uint8_t *)&s_cfg, APP_CFG_CRC_SPAN);
  d->node_id     = s_valid ? s_cfg.node_id : (uint8_t)APP_NODE_ID_UNSET;
  d->node_id_source = s_valid ? source : APP_CFG_SOURCE_NONE;

  if (!s_valid)
  {
    /* Silent on CAN1 from here on (app_can asks app_cfg_active()). The LED
       is the only voice left, so it says why: code 3, detail = the reason. */
    app_fault_raise(APP_FAULT_CFG_CRC, state);
  }
}

const app_cfg_t *app_cfg_active(void)
{
  return s_valid ? &s_cfg : NULL;
}
