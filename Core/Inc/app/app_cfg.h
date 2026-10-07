/**
  ******************************************************************************
  * @file    app_cfg.h
  * @brief   The module config: 16 bytes in flash, and the one rule that
  *          decides whether this module may speak on CAN1.
  *
  * Pure - no HAL, nothing but app_config.h - because phase 5's bootloader links
  * THIS file. CAN2 protocol v1.5 section 5, invariant 5: "the config check the
  * bootloader uses must be exactly the function the application uses to
  * decide to stay silent". Two copies of the rule would give a module that
  * neither measures nor can be configured.
  *
  * The struct IS the flash format. Changing it silently shifts every field of
  * every module already in the field; _Static_assert pins size and offsets.
  * Changing it on purpose means bumping APP_CFG_VERSION AND updating the
  * bootloader at the same time (CAN2 section 5, the warning under invariant 5).
  ******************************************************************************
  */
#ifndef APP_APP_CFG_H
#define APP_APP_CFG_H

#include "app/app_config.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* "TCF1" as it sits in flash, low byte first: 'T' 'C' 'F' '1'. Different from
   the vibration module ("VCF1") and the angle module ("ACF1"), so a page
   written by another module type fails at the first check, and says why. */
#define APP_CFG_MAGIC            0x31464354u
#define APP_CFG_VERSION          1u
#define APP_CFG_RESERVED_LEN     4u
#define APP_CFG_BLANK_WORD       0xFFFFFFFFu   /* erased flash */

typedef struct
{
  uint32_t magic;                           /* APP_CFG_MAGIC */
  uint16_t version;                         /* APP_CFG_VERSION */
  uint8_t  node_id;                         /* 0x30..0x39; 0xFF = not configured */
  uint8_t  module_type;                     /* must be APP_MODULE_TYPE (0x03) */
  uint8_t  reserved[APP_CFG_RESERVED_LEN];  /* 0 */
  uint32_t crc32;                           /* CRC-32 over the 12 bytes above */
} app_cfg_t;

/* No transmit-period field: type 0x3 is fixed at 1000 ms (CAN2 4.2, option A).
   A field nobody can set is a place for a wrong value to hide. */

_Static_assert(sizeof(app_cfg_t) == 16u, "app_cfg_t is the flash format: 16 bytes");
_Static_assert(offsetof(app_cfg_t, magic) == 0u, "flash format: magic at 0");
_Static_assert(offsetof(app_cfg_t, version) == 4u, "flash format: version at 4");
_Static_assert(offsetof(app_cfg_t, node_id) == 6u, "flash format: node_id at 6");
_Static_assert(offsetof(app_cfg_t, module_type) == 7u, "flash format: module_type at 7");
_Static_assert(offsetof(app_cfg_t, reserved) == 8u, "flash format: reserved at 8");
_Static_assert(offsetof(app_cfg_t, crc32) == 12u, "flash format: crc32 at 12");

#define APP_CFG_CRC_SPAN         ((uint32_t)offsetof(app_cfg_t, crc32))   /* 12 */

/* Why a config is not valid - the FIRST check that failed, in this order.
   Values are stable: g_diag and the fault detail show them as numbers.
   Kept as uint8_t wherever stored (enums are 1 byte on this toolchain). */
typedef enum
{
  APP_CFG_OK           = 0,
  APP_CFG_BLANK        = 1,   /* magic reads erased flash: never configured */
  APP_CFG_BAD_MAGIC    = 2,   /* damaged, or written by another module type */
  APP_CFG_BAD_VERSION  = 3,   /* another layout */
  APP_CFG_BAD_CRC      = 4,   /* damaged, or power lost in the middle of a write */
  APP_CFG_BAD_TYPE     = 5,   /* module_type is not 0x03 */
  APP_CFG_BAD_NODE     = 6,   /* node_id outside 0x30..0x39, 0xFF included */
  APP_CFG_BAD_RESERVED = 7    /* reserved bytes not zero: an unknown layout */
} app_cfg_state_t;

/** @brief CRC-32 IEEE 802.3, reflected, poly 0xEDB88320, init and final XOR
  *        0xFFFFFFFF. "123456789" -> 0xCBF43926. Bit by bit: no table, no
  *        STM32 CRC peripheral - a PC and the bootloader must get the same. */
uint32_t app_cfg_crc32(const uint8_t *p, uint32_t n);

/** @brief Factory state: right magic, version, type and CRC, but node_id 0xFF -
  *        so it is NOT valid. Unconfigured means silent. */
void app_cfg_defaults(app_cfg_t *c);

/** @brief Set magic and version, zero the reserved bytes, compute the CRC. */
void app_cfg_seal(app_cfg_t *c);

/** @brief The first failing check, or APP_CFG_OK (app_cfg_state_t as uint8_t). */
uint8_t app_cfg_check(const app_cfg_t *c);

/** @brief app_cfg_check() == APP_CFG_OK. NULL is never valid. */
bool app_cfg_valid(const app_cfg_t *c);

/* One check per field, for the CAN2 SET_* commands of phase 5 to answer 0x02
   with the same rule the config check uses. */
bool app_cfg_node_id_ok(uint8_t node_id);
bool app_cfg_module_type_ok(uint8_t module_type);

#endif /* APP_APP_CFG_H */
