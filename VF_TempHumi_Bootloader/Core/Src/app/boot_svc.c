/**
  ******************************************************************************
  * @file    boot_svc.c
  * @brief   The CAN2 service protocol for module type 0x03. Pure.
  ******************************************************************************
  */

#include "app/boot_svc.h"

#include "app/app_cfg_store.h"
#include "app/app_config.h"
#include "app/boot_config.h"

#include <stdbool.h>
#include <string.h>

static const boot_svc_hooks_t *s_hooks;
static app_cfg_t s_stored;   /* the page as of start-up or the last COMMIT */
static app_cfg_t s_work;     /* what SET_* edit */

static void svc_load(void)
{
  s_hooks->read_cfg(&s_stored);
  if (app_cfg_valid(&s_stored))
  {
    s_work = s_stored;
  }
  else
  {
    /* A blank or broken page: start from the factory state - node 0xFF, type
       0x03 - so a session of SET_NODE_ID + COMMIT alone produces a complete,
       valid config. */
    app_cfg_defaults(&s_work);
  }
}

void boot_svc_init(const boot_svc_hooks_t *hooks)
{
  s_hooks = hooks;
  svc_load();
}

const app_cfg_t *boot_svc_working(void)
{
  return &s_work;
}

static uint8_t svc_set_node_id(uint8_t id)
{
  /* CAN2 4.1: the high nibble IS the module type. app_cfg_node_id_ok() holds
     the rule for this firmware (0x30..0x39); the comparison with the working
     copy's type is the contract's own wording, checked as written. */
  if (!app_cfg_node_id_ok(id) || ((uint8_t)(id >> 4) != s_work.module_type))
  {
    return BOOT_SVC_ST_BAD_PARAM;
  }
  s_work.node_id = id;
  return BOOT_SVC_ST_OK;
}

static uint8_t svc_set_module_type(uint8_t type)
{
  /* This firmware measures temperature and humidity: a module of another type
     is not something it can become, and a config of another type is one the
     application would refuse (APP_CFG_BAD_TYPE). The pair must still agree -
     CAN2 4.1 checks it on both commands. */
  if (!app_cfg_module_type_ok(type))
  {
    return BOOT_SVC_ST_BAD_PARAM;
  }
  if ((s_work.node_id != (uint8_t)APP_NODE_ID_UNSET) && ((uint8_t)(s_work.node_id >> 4) != type))
  {
    return BOOT_SVC_ST_BAD_PARAM;
  }
  s_work.module_type = type;
  return BOOT_SVC_ST_OK;
}

static uint8_t svc_commit(void)
{
  app_cfg_t c = s_work;
  app_cfg_seal(&c);

  /* The whole config, judged exactly as the application will judge it at
     boot. A COMMIT that would leave the module silent - no node_id yet, a
     pair that does not agree - writes nothing and says so. */
  if (app_cfg_check(&c) != (uint8_t)APP_CFG_OK)
  {
    return BOOT_SVC_ST_BAD_PARAM;
  }

  const uint8_t wr = s_hooks->write_cfg(&c);

  /* From here on HELLO reports what the page holds, read back - not what was
     meant to be written. After a failed write that may be a blank page: the
     module is then unconfigured, which is the honest state. */
  svc_load();

  if (wr != APP_CFG_WR_OK)
  {
    return BOOT_SVC_ST_FLASH_ERROR;
  }
  if (!app_cfg_valid(&s_stored) || (memcmp(&s_stored, &c, sizeof(c)) != 0))
  {
    return BOOT_SVC_ST_FLASH_ERROR;
  }
  return BOOT_SVC_ST_OK;
}

uint8_t boot_svc_handle(const uint8_t *req, uint8_t len, uint8_t rsp[BOOT_SVC_DLC])
{
  /* Zero-padded rather than refused: a short request is a mainboard bug, and
     the protocol can only answer it - and so make it visible - if it sees it. */
  uint8_t r[BOOT_SVC_DLC];
  (void)memset(r, 0, sizeof(r));
  (void)memcpy(r, req, (len < BOOT_SVC_DLC) ? len : BOOT_SVC_DLC);

  uint8_t action = (uint8_t)BOOT_SVC_NONE;
  uint8_t status = BOOT_SVC_ST_OK;
  const uint8_t cmd = r[0];

  (void)memset(rsp, 0, BOOT_SVC_DLC);
  rsp[0] = cmd;

  switch (cmd)
  {
    case BOOT_SVC_CMD_HELLO:
      rsp[2] = BOOT_SVC_PROTO_VER;
      rsp[3] = (uint8_t)APP_MODULE_TYPE;
      /* The node_id this module would use if it started now: the stored one
         if the page passes app_cfg_check, 0xFF if not - never an uncommitted
         SET. */
      rsp[4] = app_cfg_valid(&s_stored) ? s_stored.node_id : (uint8_t)APP_NODE_ID_UNSET;
      rsp[5] = (uint8_t)BOOT_FW_MAJOR;
      rsp[6] = (uint8_t)BOOT_FW_MINOR;
      rsp[7] = (uint8_t)BOOT_FW_PATCH;
      break;

    case BOOT_SVC_CMD_GET_UID_LO:
    case BOOT_SVC_CMD_GET_UID_HI:
    {
      uint8_t uid[BOOT_SVC_UID_BYTES];
      s_hooks->read_uid(uid);
      (void)memcpy(&rsp[2],
                   &uid[(cmd == BOOT_SVC_CMD_GET_UID_LO) ? 0u : BOOT_SVC_UID_PER_FRAME],
                   BOOT_SVC_UID_PER_FRAME);
      break;
    }

    case BOOT_SVC_CMD_SET_NODE_ID:
      status = svc_set_node_id(r[1]);
      break;

    case BOOT_SVC_CMD_SET_MODULE_TYPE:
      status = svc_set_module_type(r[1]);
      break;

    case BOOT_SVC_CMD_SET_TX_PERIOD:
      /* Type 0x03: the set is EMPTY - fixed at 1000 ms, every value is
         BAD_PARAM. The contract says so in as many words (CAN2 4.2): the
         command exists, this module type supports none of its values. */
      status = BOOT_SVC_ST_BAD_PARAM;
      break;

    case BOOT_SVC_CMD_SET_FULL_SCALE:
    case BOOT_SVC_CMD_SET_GYRO_FS:
      /* "Not supported" for type 0x03 (CAN2 4.2): no accelerometer, no gyro.
         0x01 is "this module type has no such command" - the contract's own
         example is 0x14 sent to a vibration module (CAN2 4). Phase 5 plan,
         question 4, approved 2026-10-07. */
      status = BOOT_SVC_ST_UNKNOWN_CMD;
      break;

    case BOOT_SVC_CMD_COMMIT:
      status = svc_commit();
      break;

    case BOOT_SVC_CMD_RESET:
      action = (uint8_t)BOOT_SVC_RESET;
      break;

    default:
      /* Including 0x40+ (firmware download - not specified yet, CAN2 7). */
      status = BOOT_SVC_ST_UNKNOWN_CMD;
      break;
  }

  rsp[1] = status;
  return action;
}
