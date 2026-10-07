/**
  ******************************************************************************
  * @file    app_cfg_store.c
  * @brief   Config page: flash access and the boot-time verdict.
  ******************************************************************************
  */

#include "app/app_cfg_store.h"

#include "app/app_diag.h"
#include "app/app_fault.h"
#include "main.h"

#include <string.h>

/* One translation unit says it, so a bench build carries exactly one warning
   per switch - and a clean build carries none (CLAUDE.md 3: every warning
   explained). */
#if APP_CFG_BENCH_WRITE
#warning "APP_CFG_BENCH_WRITE = 1: this build writes a bench config to flash - never ship it"
#endif
#if APP_CFG_BENCH_ERASE
#warning "APP_CFG_BENCH_ERASE = 1: this build erases the config page at every boot - never ship it"
#endif
#if APP_BENCH_CAN_LOOPBACK
#warning "APP_BENCH_CAN_LOOPBACK = 1: this build never speaks on CAN1 - never ship it"
#endif

#define CFG_HALFWORDS        (sizeof(app_cfg_t) / sizeof(uint16_t))   /* 8 */
#define CFG_PAGE_BYTES       (APP_CFG_PAGE_KB * 1024u)
#define CFG_PAGE_WORDS       (CFG_PAGE_BYTES / sizeof(uint32_t))

_Static_assert((sizeof(app_cfg_t) % sizeof(uint16_t)) == 0u, "the record is programmed by half-word");
_Static_assert(CFG_PAGE_BYTES == FLASH_PAGE_SIZE, "the config page is exactly one F3 flash page");

static app_cfg_t s_cfg;      /* RAM copy of the record in flash */
static bool      s_valid;

void app_cfg_store_read(app_cfg_t *out)
{
  (void)memcpy(out, (const void *)APP_CFG_FLASH_ADDR, sizeof(*out));
}

static bool store_page_is_blank(void)
{
  const volatile uint32_t *const w = (const volatile uint32_t *)APP_CFG_FLASH_ADDR;

  for (uint32_t i = 0u; i < CFG_PAGE_WORDS; i++)
  {
    if (w[i] != APP_CFG_BLANK_WORD)
    {
      return false;
    }
  }
  return true;
}

/* Erase the page. The caller holds the flash unlocked. */
static bool store_erase_unlocked(void)
{
  FLASH_EraseInitTypeDef e;
  uint32_t page_error = 0u;

  (void)memset(&e, 0, sizeof(e));
  e.TypeErase   = FLASH_TYPEERASE_PAGES;
  e.PageAddress = APP_CFG_FLASH_ADDR;
  e.NbPages     = 1u;
  return HAL_FLASHEx_Erase(&e, &page_error) == HAL_OK;
}

bool app_cfg_store_erase(void)
{
  if (HAL_FLASH_Unlock() != HAL_OK)
  {
    return false;
  }
  const bool erased = store_erase_unlocked();
  const bool locked = (HAL_FLASH_Lock() == HAL_OK);

  return erased && locked && store_page_is_blank();
}

bool app_cfg_store_write(app_cfg_t *c)
{
  app_cfg_seal(c);

  if (HAL_FLASH_Unlock() != HAL_OK)
  {
    return false;
  }

  /* Every path from here goes through the lock below. */
  bool ok = store_erase_unlocked();

  uint16_t hw[CFG_HALFWORDS];
  (void)memcpy(hw, c, sizeof(hw));
  for (uint32_t i = 0u; ok && (i < CFG_HALFWORDS); i++)
  {
    ok = (HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD,
                            APP_CFG_FLASH_ADDR + (i * sizeof(uint16_t)),
                            (uint64_t)hw[i]) == HAL_OK);
  }

  const bool locked = (HAL_FLASH_Lock() == HAL_OK);

  /* Trust the flash, not the HAL return codes: read it back. */
  app_cfg_t back;
  app_cfg_store_read(&back);
  return ok && locked && (memcmp(&back, c, sizeof(back)) == 0);
}

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
  d->erase_status = app_cfg_store_erase() ? APP_CFG_OP_OK : APP_CFG_OP_FAILED;
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
    d->write_status = app_cfg_store_write(&bench) ? APP_CFG_OP_OK : APP_CFG_OP_FAILED;

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
