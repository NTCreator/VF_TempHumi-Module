/**
  ******************************************************************************
  * @file    boot_svc.h
  * @brief   The CAN2 service protocol, module side, for module type 0x03.
  *
  * Contract: contracts/VF_Main_CAN2_Service_Protocol.md v1.5, sections 3-5.
  *
  * Pure - no HAL. Flash and UID come in through hooks, so
  * tests/test_boot_svc.c runs every command on a PC against a flash that fails
  * on demand.
  *
  * Every parameter rule is app_cfg's (app_cfg_node_id_ok,
  * app_cfg_module_type_ok, app_cfg_check), linked from VF_TempHumi_Module: the
  * exact code the application validates its config with at boot. The
  * bootloader cannot accept a value the application would then refuse, or the
  * reverse (CAN2 section 5, invariant 5).
  *
  * SET_* change a working copy in RAM only; COMMIT writes it. A session that
  * is abandoned leaves nothing behind: the next power-up starts from flash.
  * Every request gets a response - on a point-to-point link, silence reads as
  * an unplugged connector.
  ******************************************************************************
  */
#ifndef BOOT_SVC_H
#define BOOT_SVC_H

#include "app/app_cfg.h"

#include <stdint.h>

/* Wire, CAN2 section 3. */
#define BOOT_SVC_REQ_ID              0x7E0u
#define BOOT_SVC_RSP_ID              0x7E8u
#define BOOT_SVC_DLC                 8u      /* always 8, padded with 0x00 */
#define BOOT_SVC_PROTO_VER           1u      /* HELLO byte 2 */

/* Commands, CAN2 section 4. */
#define BOOT_SVC_CMD_HELLO           0x01u
#define BOOT_SVC_CMD_GET_UID_LO      0x02u
#define BOOT_SVC_CMD_GET_UID_HI      0x03u
#define BOOT_SVC_CMD_SET_NODE_ID     0x10u
#define BOOT_SVC_CMD_SET_MODULE_TYPE 0x11u
#define BOOT_SVC_CMD_SET_TX_PERIOD   0x12u
#define BOOT_SVC_CMD_SET_FULL_SCALE  0x13u
#define BOOT_SVC_CMD_SET_GYRO_FS     0x14u
#define BOOT_SVC_CMD_COMMIT          0x20u
#define BOOT_SVC_CMD_RESET           0x30u

/* Status, CAN2 section 4. 0x04 (not allowed in this state) has no use here:
   every command is allowed whenever the bootloader is running. */
#define BOOT_SVC_ST_OK               0x00u
#define BOOT_SVC_ST_UNKNOWN_CMD      0x01u   /* this module type has no such command */
#define BOOT_SVC_ST_BAD_PARAM        0x02u   /* the command exists, the value is outside its subset */
#define BOOT_SVC_ST_FLASH_ERROR      0x03u

#define BOOT_SVC_UID_BYTES           12u
#define BOOT_SVC_UID_PER_FRAME       6u      /* response bytes 2..7 */

typedef struct
{
  void    (*read_cfg)(app_cfg_t *out);                   /* the page as it is */
  uint8_t (*write_cfg)(app_cfg_t *cfg);                  /* APP_CFG_WR_*; anything but OK -> 0x03 */
  void    (*read_uid)(uint8_t out[BOOT_SVC_UID_BYTES]);  /* byte 0 first, as at UID_BASE */
} boot_svc_hooks_t;

/* What the caller must do once the response has gone out. */
typedef enum
{
  BOOT_SVC_NONE  = 0,
  BOOT_SVC_RESET = 1    /* reset the module - AFTER the response (CAN2 section 5, invariant 4) */
} boot_svc_action_t;

/**
  * @brief Start: read the stored config. The working copy starts from it if
  *        it is valid, from the factory state (node 0xFF, type 0x03) if not.
  * @param hooks borrowed, must outlive the protocol
  */
void boot_svc_init(const boot_svc_hooks_t *hooks);

/**
  * @brief Handle one request and build its response. Always answers.
  * @param req  the request payload as received
  * @param len  its DLC; bytes past it read as 0x00, more than 8 are ignored
  * @param rsp  eight bytes for the 0x7E8 frame
  * @return boot_svc_action_t, as uint8_t
  */
uint8_t boot_svc_handle(const uint8_t *req, uint8_t len, uint8_t rsp[BOOT_SVC_DLC]);

/** @brief The working copy - for tests and the debugger. */
const app_cfg_t *boot_svc_working(void);

#endif /* BOOT_SVC_H */
