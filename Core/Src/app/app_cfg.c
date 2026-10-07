/**
  ******************************************************************************
  * @file    app_cfg.c
  * @brief   Config format, CRC and validation. Pure - shared with the
  *          bootloader (phase 5).
  ******************************************************************************
  */

#include "app/app_cfg.h"

#include <string.h>

#define CFG_CRC_INIT     0xFFFFFFFFu
#define CFG_CRC_POLY     0xEDB88320u   /* IEEE 802.3, reflected */
#define CFG_CRC_XOROUT   0xFFFFFFFFu

uint32_t app_cfg_crc32(const uint8_t *p, uint32_t n)
{
  uint32_t crc = CFG_CRC_INIT;

  for (uint32_t i = 0u; i < n; i++)
  {
    crc ^= p[i];
    for (uint8_t bit = 0u; bit < 8u; bit++)
    {
      crc = ((crc & 1u) != 0u) ? ((crc >> 1) ^ CFG_CRC_POLY) : (crc >> 1);
    }
  }
  return crc ^ CFG_CRC_XOROUT;
}

static uint32_t cfg_crc_of(const app_cfg_t *c)
{
  return app_cfg_crc32((const uint8_t *)c, APP_CFG_CRC_SPAN);
}

void app_cfg_seal(app_cfg_t *c)
{
  c->magic = APP_CFG_MAGIC;
  c->version = (uint16_t)APP_CFG_VERSION;
  (void)memset(c->reserved, 0, sizeof(c->reserved));
  c->crc32 = cfg_crc_of(c);
}

void app_cfg_defaults(app_cfg_t *c)
{
  (void)memset(c, 0, sizeof(*c));
  c->node_id = (uint8_t)APP_NODE_ID_UNSET;
  c->module_type = (uint8_t)APP_MODULE_TYPE;
  app_cfg_seal(c);
}

bool app_cfg_node_id_ok(uint8_t node_id)
{
  return APP_NODE_ID_OK(node_id);
}

bool app_cfg_module_type_ok(uint8_t module_type)
{
  return module_type == (uint8_t)APP_MODULE_TYPE;
}

uint8_t app_cfg_check(const app_cfg_t *c)
{
  if (c == NULL)
  {
    return (uint8_t)APP_CFG_BLANK;
  }
  /* Blank before magic: "never configured" and "damaged" are both silent,
     but they send people to two different places. */
  if (c->magic == APP_CFG_BLANK_WORD)
  {
    return (uint8_t)APP_CFG_BLANK;
  }
  if (c->magic != APP_CFG_MAGIC)
  {
    return (uint8_t)APP_CFG_BAD_MAGIC;
  }
  if (c->version != (uint16_t)APP_CFG_VERSION)
  {
    return (uint8_t)APP_CFG_BAD_VERSION;
  }
  /* CRC before any field: a field of a damaged record means nothing. */
  if (c->crc32 != cfg_crc_of(c))
  {
    return (uint8_t)APP_CFG_BAD_CRC;
  }
  if (!app_cfg_module_type_ok(c->module_type))
  {
    return (uint8_t)APP_CFG_BAD_TYPE;
  }
  if (!app_cfg_node_id_ok(c->node_id))
  {
    return (uint8_t)APP_CFG_BAD_NODE;
  }
  for (uint32_t i = 0u; i < APP_CFG_RESERVED_LEN; i++)
  {
    if (c->reserved[i] != 0u)
    {
      return (uint8_t)APP_CFG_BAD_RESERVED;
    }
  }
  return (uint8_t)APP_CFG_OK;
}

bool app_cfg_valid(const app_cfg_t *c)
{
  return app_cfg_check(c) == (uint8_t)APP_CFG_OK;
}
