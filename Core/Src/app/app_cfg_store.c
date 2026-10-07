/**
  ******************************************************************************
  * @file    app_cfg_store.c
  * @brief   Config page: flash access only. Linked by the bootloader too.
  ******************************************************************************
  */

#include "app/app_cfg_store.h"

#include "main.h"

#include <stdbool.h>
#include <string.h>

#define CFG_HALFWORDS        (sizeof(app_cfg_t) / sizeof(uint16_t))   /* 8 */
#define CFG_PAGE_BYTES       (APP_CFG_PAGE_KB * 1024u)
#define CFG_PAGE_WORDS       (CFG_PAGE_BYTES / sizeof(uint32_t))

_Static_assert((sizeof(app_cfg_t) % sizeof(uint16_t)) == 0u, "the record is programmed by half-word");
_Static_assert(CFG_PAGE_BYTES == FLASH_PAGE_SIZE, "the config page is exactly one F3 flash page");

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

uint8_t app_cfg_store_erase(void)
{
  if (HAL_FLASH_Unlock() != HAL_OK)
  {
    return APP_CFG_WR_UNLOCK;
  }
  const bool erased = store_erase_unlocked();
  const bool locked = (HAL_FLASH_Lock() == HAL_OK);

  if (!erased)
  {
    return APP_CFG_WR_ERASE;
  }
  if (!locked)
  {
    return APP_CFG_WR_LOCK;
  }
  return store_page_is_blank() ? APP_CFG_WR_OK : APP_CFG_WR_VERIFY;
}

uint8_t app_cfg_store_write(app_cfg_t *c)
{
  app_cfg_seal(c);

  if (HAL_FLASH_Unlock() != HAL_OK)
  {
    return APP_CFG_WR_UNLOCK;
  }

  /* Every path from here goes through the lock below. */
  uint8_t result = store_erase_unlocked() ? APP_CFG_WR_OK : APP_CFG_WR_ERASE;

  uint16_t hw[CFG_HALFWORDS];
  (void)memcpy(hw, c, sizeof(hw));
  for (uint32_t i = 0u; (result == APP_CFG_WR_OK) && (i < CFG_HALFWORDS); i++)
  {
    if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD,
                          APP_CFG_FLASH_ADDR + (i * sizeof(uint16_t)),
                          (uint64_t)hw[i]) != HAL_OK)
    {
      result = APP_CFG_WR_PROGRAM;
    }
  }

  if ((HAL_FLASH_Lock() != HAL_OK) && (result == APP_CFG_WR_OK))
  {
    result = APP_CFG_WR_LOCK;
  }

  /* Trust the flash, not the HAL return codes: read it back. */
  app_cfg_t back;
  app_cfg_store_read(&back);
  if ((result == APP_CFG_WR_OK) && (memcmp(&back, c, sizeof(back)) != 0))
  {
    result = APP_CFG_WR_VERIFY;
  }
  return result;
}
