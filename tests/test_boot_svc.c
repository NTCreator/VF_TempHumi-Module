/* boot_svc: the CAN2 service protocol, module side, type 0x03 (phase 5).
   Runs against a fake flash that fails on demand and a fake UID. The
   parameter rules are app_cfg.c's, linked here exactly as the bootloader
   links it. Contract: contracts/VF_Main_CAN2_Service_Protocol.md v1.5. */

#include "app/boot_svc.h"

#include "app/app_cfg.h"
#include "app/app_cfg_store.h"
#include "app/boot_config.h"
#include "test_check.h"

#include <string.h>

/* ---- Fake flash and UID ------------------------------------------- */

/* How the next write behaves. */
enum
{
  FAKE_OK = 0,           /* store the sealed record, report OK */
  FAKE_FAIL_KEEP,        /* report s_fail_code, page untouched (failed before the erase) */
  FAKE_FAIL_ERASED,      /* report s_fail_code, page left blank (failed after the erase) */
  FAKE_FAIL_WRITTEN,     /* report s_fail_code, page holds the record (HAL_FLASH_Lock failed) */
  FAKE_LIE               /* report OK, but the page holds something else */
};

static app_cfg_t s_page;
static int       s_writes;
static int       s_reads;
static int       s_mode;
static uint8_t   s_fail_code;

static const uint8_t k_uid[BOOT_SVC_UID_BYTES] =
{ 0xA0u, 0xA1u, 0xA2u, 0xA3u, 0xA4u, 0xA5u, 0xB6u, 0xB7u, 0xB8u, 0xB9u, 0xBAu, 0xBBu };

static void fake_read(app_cfg_t *out)
{
  s_reads++;
  *out = s_page;
}

static uint8_t fake_write(app_cfg_t *c)
{
  s_writes++;
  switch (s_mode)
  {
    case FAKE_FAIL_KEEP:
      return s_fail_code;
    case FAKE_FAIL_ERASED:
      memset(&s_page, 0xFF, sizeof(s_page));
      return s_fail_code;
    case FAKE_FAIL_WRITTEN:
      app_cfg_seal(c);
      s_page = *c;
      return s_fail_code;
    case FAKE_LIE:
      app_cfg_seal(c);
      s_page = *c;
      s_page.node_id = (uint8_t)(s_page.node_id ^ 0x01u);   /* a neighbour, CRC now wrong */
      return APP_CFG_WR_OK;
    default:
      app_cfg_seal(c);   /* app_cfg_store_write seals, too */
      s_page = *c;
      return APP_CFG_WR_OK;
  }
}

static void fake_uid(uint8_t out[BOOT_SVC_UID_BYTES])
{
  memcpy(out, k_uid, BOOT_SVC_UID_BYTES);
}

static const boot_svc_hooks_t k_hooks = { fake_read, fake_write, fake_uid };

static void page_blank(void)
{
  memset(&s_page, 0xFF, sizeof(s_page));
}

static app_cfg_t valid_cfg(uint8_t node_id)
{
  app_cfg_t c;
  app_cfg_defaults(&c);
  c.node_id = node_id;
  app_cfg_seal(&c);
  return c;
}

static void start(void)
{
  s_writes = 0;
  s_reads = 0;
  s_mode = FAKE_OK;
  s_fail_code = APP_CFG_WR_OK;
  boot_svc_init(&k_hooks);
}

/* One request of up to 8 bytes; returns the action, fills rsp. */
static uint8_t req2(uint8_t cmd, uint8_t p1, uint8_t rsp[BOOT_SVC_DLC])
{
  const uint8_t req[BOOT_SVC_DLC] = { cmd, p1, 0u, 0u, 0u, 0u, 0u, 0u };
  return boot_svc_handle(req, BOOT_SVC_DLC, rsp);
}

static uint8_t status_of(uint8_t cmd, uint8_t p1)
{
  uint8_t rsp[BOOT_SVC_DLC];
  (void)req2(cmd, p1, rsp);
  return rsp[1];
}

static uint8_t hello_node(void)
{
  uint8_t rsp[BOOT_SVC_DLC];
  (void)req2(BOOT_SVC_CMD_HELLO, 0u, rsp);
  return rsp[4];
}

/* ---- Tests -------------------------------------------------------- */

static void test_hello(void)
{
  uint8_t rsp[BOOT_SVC_DLC];

  /* Blank page: the factory answer. */
  page_blank();
  start();
  CHECK_EQ(req2(BOOT_SVC_CMD_HELLO, 0u, rsp), BOOT_SVC_NONE);
  CHECK_EQ(rsp[0], 0x01u);
  CHECK_EQ(rsp[1], BOOT_SVC_ST_OK);
  CHECK_EQ(rsp[2], 1u);                    /* proto_ver */
  CHECK_EQ(rsp[3], 0x03u);                 /* module_type */
  CHECK_EQ(rsp[4], 0xFFu);                 /* not configured */
  CHECK_EQ(rsp[5], BOOT_FW_MAJOR);
  CHECK_EQ(rsp[6], BOOT_FW_MINOR);
  CHECK_EQ(rsp[7], BOOT_FW_PATCH);
  CHECK_EQ(BOOT_FW_MAJOR, 0u);
  CHECK_EQ(BOOT_FW_MINOR, 1u);
  CHECK_EQ(BOOT_FW_PATCH, 0u);

  /* Valid page: its node. */
  s_page = valid_cfg(0x35u);
  start();
  CHECK_EQ(hello_node(), 0x35u);

  /* A page that fails app_cfg_check - one bit off - reads as unconfigured. */
  s_page = valid_cfg(0x35u);
  s_page.reserved[0] = 1u;
  start();
  CHECK_EQ(hello_node(), 0xFFu);

  /* A page with a good CRC but a foreign type: still 0xFF (the angle
     module's magic is different, but a record re-sealed with type 2 must not
     pass either). */
  s_page = valid_cfg(0x35u);
  s_page.module_type = 0x02u;
  s_page.node_id = 0x25u;
  app_cfg_seal(&s_page);
  start();
  CHECK_EQ(hello_node(), 0xFFu);
  CHECK_EQ(s_writes, 0);
}

static void test_uid(void)
{
  uint8_t rsp[BOOT_SVC_DLC];

  page_blank();
  start();
  (void)req2(BOOT_SVC_CMD_GET_UID_LO, 0u, rsp);
  CHECK_EQ(rsp[0], 0x02u);
  CHECK_EQ(rsp[1], BOOT_SVC_ST_OK);
  CHECK(memcmp(&rsp[2], &k_uid[0], 6u) == 0);

  (void)req2(BOOT_SVC_CMD_GET_UID_HI, 0u, rsp);
  CHECK_EQ(rsp[0], 0x03u);
  CHECK_EQ(rsp[1], BOOT_SVC_ST_OK);
  CHECK(memcmp(&rsp[2], &k_uid[6], 6u) == 0);
}

static void test_set_node_id_sweep(void)
{
  int accepted = 0;

  page_blank();
  start();
  for (unsigned id = 0u; id < 256u; id++)
  {
    const uint8_t before = boot_svc_working()->node_id;
    const uint8_t st = status_of(BOOT_SVC_CMD_SET_NODE_ID, (uint8_t)id);
    const int ok_expected = (id >= 0x30u) && (id <= 0x39u);

    CHECK_EQ(st, ok_expected ? BOOT_SVC_ST_OK : BOOT_SVC_ST_BAD_PARAM);
    if (ok_expected)
    {
      accepted++;
      CHECK_EQ(boot_svc_working()->node_id, id);
    }
    else
    {
      /* Refused: the working copy is untouched. */
      CHECK_EQ(boot_svc_working()->node_id, before);
    }
  }
  CHECK_EQ(accepted, 10);
  CHECK_EQ(s_writes, 0);   /* SET_* never write */
}

static void test_set_module_type(void)
{
  page_blank();
  start();
  for (unsigned t = 0u; t < 256u; t++)
  {
    CHECK_EQ(status_of(BOOT_SVC_CMD_SET_MODULE_TYPE, (uint8_t)t),
             (t == 0x03u) ? BOOT_SVC_ST_OK : BOOT_SVC_ST_BAD_PARAM);
    CHECK_EQ(boot_svc_working()->module_type, 0x03u);
  }

  /* With a node set, type 3 still agrees with it. */
  CHECK_EQ(status_of(BOOT_SVC_CMD_SET_NODE_ID, 0x37u), BOOT_SVC_ST_OK);
  CHECK_EQ(status_of(BOOT_SVC_CMD_SET_MODULE_TYPE, 0x03u), BOOT_SVC_ST_OK);
  CHECK_EQ(status_of(BOOT_SVC_CMD_SET_MODULE_TYPE, 0x01u), BOOT_SVC_ST_BAD_PARAM);
  CHECK_EQ(status_of(BOOT_SVC_CMD_SET_MODULE_TYPE, 0x02u), BOOT_SVC_ST_BAD_PARAM);
  CHECK_EQ(status_of(BOOT_SVC_CMD_SET_MODULE_TYPE, 0x04u), BOOT_SVC_ST_BAD_PARAM);
  CHECK_EQ(boot_svc_working()->node_id, 0x37u);
  CHECK_EQ(s_writes, 0);
}

static void test_tx_period_and_unsupported(void)
{
  page_blank();
  start();
  for (unsigned v = 0u; v < 256u; v++)
  {
    /* CAN2 4.2: the set is empty for type 0x03 - every value, the four
       legal-on-the-wire ones (5, 10, 20, 50) included. */
    CHECK_EQ(status_of(BOOT_SVC_CMD_SET_TX_PERIOD, (uint8_t)v), BOOT_SVC_ST_BAD_PARAM);
    /* No accelerometer, no gyro: "this module type has no such command". */
    CHECK_EQ(status_of(BOOT_SVC_CMD_SET_FULL_SCALE, (uint8_t)v), BOOT_SVC_ST_UNKNOWN_CMD);
    CHECK_EQ(status_of(BOOT_SVC_CMD_SET_GYRO_FS, (uint8_t)v), BOOT_SVC_ST_UNKNOWN_CMD);
  }

  /* None of them touched the working copy. */
  app_cfg_t factory;
  app_cfg_defaults(&factory);
  CHECK(memcmp(boot_svc_working(), &factory, sizeof(factory)) == 0);
  CHECK_EQ(s_writes, 0);
}

static void test_abandoned_session(void)
{
  s_page = valid_cfg(0x31u);
  const app_cfg_t before = s_page;

  start();
  CHECK_EQ(status_of(BOOT_SVC_CMD_SET_NODE_ID, 0x38u), BOOT_SVC_ST_OK);
  CHECK_EQ(hello_node(), 0x31u);           /* HELLO reports the page, never an uncommitted SET */

  /* Power cycle without COMMIT. */
  start();
  CHECK_EQ(s_writes, 0);
  CHECK(memcmp(&s_page, &before, sizeof(before)) == 0);
  CHECK_EQ(boot_svc_working()->node_id, 0x31u);
  CHECK_EQ(hello_node(), 0x31u);
}

static void test_commit(void)
{
  uint8_t rsp[BOOT_SVC_DLC];

  /* Blank page, no node yet: refused, nothing written. */
  page_blank();
  start();
  CHECK_EQ(status_of(BOOT_SVC_CMD_COMMIT, 0u), BOOT_SVC_ST_BAD_PARAM);
  CHECK_EQ(s_writes, 0);

  /* SET_NODE_ID + COMMIT alone: a complete, valid config. */
  CHECK_EQ(status_of(BOOT_SVC_CMD_SET_NODE_ID, 0x33u), BOOT_SVC_ST_OK);
  CHECK_EQ(req2(BOOT_SVC_CMD_COMMIT, 0u, rsp), BOOT_SVC_NONE);
  CHECK_EQ(rsp[0], 0x20u);
  CHECK_EQ(rsp[1], BOOT_SVC_ST_OK);
  CHECK_EQ(s_writes, 1);
  CHECK_EQ(app_cfg_check(&s_page), APP_CFG_OK);
  const app_cfg_t want = valid_cfg(0x33u);
  CHECK(memcmp(&s_page, &want, sizeof(want)) == 0);   /* byte for byte what the app expects */
  CHECK_EQ(hello_node(), 0x33u);

  /* The record a bench write of node 0x30 gives is the one the chip showed in
     phase 4 (CRC 0x2E9C0667): the bootloader writes the same bytes. */
  CHECK_EQ(status_of(BOOT_SVC_CMD_SET_NODE_ID, 0x30u), BOOT_SVC_ST_OK);
  CHECK_EQ(status_of(BOOT_SVC_CMD_COMMIT, 0u), BOOT_SVC_ST_OK);
  CHECK_EQ(s_page.crc32, 0x2E9C0667u);
  CHECK_EQ(s_writes, 2);

  /* COMMIT again with nothing changed: idempotent. */
  CHECK_EQ(status_of(BOOT_SVC_CMD_COMMIT, 0u), BOOT_SVC_ST_OK);
  CHECK_EQ(s_writes, 3);
  CHECK_EQ(hello_node(), 0x30u);

  /* A valid page, re-committed straight after power-up without any SET. */
  s_page = valid_cfg(0x39u);
  start();
  CHECK_EQ(status_of(BOOT_SVC_CMD_COMMIT, 0u), BOOT_SVC_ST_OK);
  CHECK_EQ(s_writes, 1);
  CHECK_EQ(hello_node(), 0x39u);
}

static void test_flash_errors(void)
{
  /* Every failing step of app_cfg_store_write is 0x03, page untouched. */
  for (uint8_t code = APP_CFG_WR_UNLOCK; code <= APP_CFG_WR_VERIFY; code++)
  {
    s_page = valid_cfg(0x31u);
    start();
    s_mode = FAKE_FAIL_KEEP;
    s_fail_code = code;
    CHECK_EQ(status_of(BOOT_SVC_CMD_SET_NODE_ID, 0x34u), BOOT_SVC_ST_OK);
    CHECK_EQ(status_of(BOOT_SVC_CMD_COMMIT, 0u), BOOT_SVC_ST_FLASH_ERROR);
    CHECK_EQ(s_writes, 1);
    CHECK_EQ(hello_node(), 0x31u);         /* HELLO says what the page holds */
  }

  /* Failed after the erase: the page is blank and HELLO says so - the
     module is now unconfigured, the honest state. */
  s_page = valid_cfg(0x31u);
  start();
  s_mode = FAKE_FAIL_ERASED;
  s_fail_code = APP_CFG_WR_PROGRAM;
  CHECK_EQ(status_of(BOOT_SVC_CMD_SET_NODE_ID, 0x34u), BOOT_SVC_ST_OK);
  CHECK_EQ(status_of(BOOT_SVC_CMD_COMMIT, 0u), BOOT_SVC_ST_FLASH_ERROR);
  CHECK_EQ(hello_node(), 0xFFu);
  /* A retry on a working flash recovers. */
  s_mode = FAKE_OK;
  CHECK_EQ(status_of(BOOT_SVC_CMD_SET_NODE_ID, 0x34u), BOOT_SVC_ST_OK);
  CHECK_EQ(status_of(BOOT_SVC_CMD_COMMIT, 0u), BOOT_SVC_ST_OK);
  CHECK_EQ(hello_node(), 0x34u);

  /* The record is in, but a later step failed (the lock): still 0x03 - the
     flash controller was left in a state nobody asked for, and the operator
     must hear about it. HELLO tells the truth about the page: it holds the
     new node. */
  s_page = valid_cfg(0x31u);
  start();
  s_mode = FAKE_FAIL_WRITTEN;
  s_fail_code = APP_CFG_WR_LOCK;
  CHECK_EQ(status_of(BOOT_SVC_CMD_SET_NODE_ID, 0x35u), BOOT_SVC_ST_OK);
  CHECK_EQ(status_of(BOOT_SVC_CMD_COMMIT, 0u), BOOT_SVC_ST_FLASH_ERROR);
  CHECK_EQ(hello_node(), 0x35u);

  /* The flash reports OK but holds something else: the read back decides. */
  s_page = valid_cfg(0x31u);
  start();
  s_mode = FAKE_LIE;
  CHECK_EQ(status_of(BOOT_SVC_CMD_SET_NODE_ID, 0x36u), BOOT_SVC_ST_OK);
  CHECK_EQ(status_of(BOOT_SVC_CMD_COMMIT, 0u), BOOT_SVC_ST_FLASH_ERROR);
  CHECK_EQ(hello_node(), 0xFFu);
}

static void test_reset(void)
{
  uint8_t rsp[BOOT_SVC_DLC];

  page_blank();
  start();
  CHECK_EQ(req2(BOOT_SVC_CMD_RESET, 0u, rsp), BOOT_SVC_RESET);
  CHECK_EQ(rsp[0], 0x30u);
  CHECK_EQ(rsp[1], BOOT_SVC_ST_OK);
  CHECK_EQ(s_writes, 0);
}

static void test_every_command_byte(void)
{
  /* All 256 command bytes: exactly the documented ones are understood, only
     RESET asks for an action, every response echoes the command. */
  page_blank();
  start();
  for (unsigned cmd = 0u; cmd < 256u; cmd++)
  {
    uint8_t rsp[BOOT_SVC_DLC];
    memset(rsp, 0xEE, sizeof(rsp));
    const uint8_t action = req2((uint8_t)cmd, 0u, rsp);

    CHECK_EQ(rsp[0], cmd);
    CHECK_EQ(action, (cmd == BOOT_SVC_CMD_RESET) ? BOOT_SVC_RESET : BOOT_SVC_NONE);

    const int known = (cmd == 0x01u) || (cmd == 0x02u) || (cmd == 0x03u) || (cmd == 0x10u) ||
                      (cmd == 0x11u) || (cmd == 0x12u) || (cmd == 0x20u) || (cmd == 0x30u);
    if (!known)
    {
      CHECK_EQ(rsp[1], BOOT_SVC_ST_UNKNOWN_CMD);
      for (unsigned i = 2u; i < BOOT_SVC_DLC; i++)
      {
        CHECK_EQ(rsp[i], 0u);              /* padded with 0x00 */
      }
    }
  }
  CHECK_EQ(s_writes, 0);                   /* COMMIT with no node: refused */
}

static void test_short_and_long_frames(void)
{
  uint8_t rsp[BOOT_SVC_DLC];

  page_blank();
  start();

  /* DLC 1: the parameter byte past the DLC is garbage and must read as 0x00,
     so SET_NODE_ID sees node 0x00 - refused - and not the 0x33 behind it. */
  const uint8_t req[BOOT_SVC_DLC] = { 0x10u, 0x33u, 0x55u, 0x55u, 0x55u, 0x55u, 0x55u, 0x55u };
  (void)boot_svc_handle(req, 1u, rsp);
  CHECK_EQ(rsp[0], 0x10u);
  CHECK_EQ(rsp[1], BOOT_SVC_ST_BAD_PARAM);
  CHECK_EQ(boot_svc_working()->node_id, 0xFFu);

  /* DLC 2: the parameter is in. */
  (void)boot_svc_handle(req, 2u, rsp);
  CHECK_EQ(rsp[1], BOOT_SVC_ST_OK);
  CHECK_EQ(boot_svc_working()->node_id, 0x33u);

  /* DLC 0: no command byte at all - command 0x00, unknown, still answered. */
  (void)boot_svc_handle(req, 0u, rsp);
  CHECK_EQ(rsp[0], 0x00u);
  CHECK_EQ(rsp[1], BOOT_SVC_ST_UNKNOWN_CMD);

  /* DLC above 8 (a CAN 2.0 frame may say 9..15): read as 8, nothing past it. */
  const uint8_t hello[BOOT_SVC_DLC] = { 0x01u, 0u, 0u, 0u, 0u, 0u, 0u, 0u };
  (void)boot_svc_handle(hello, 15u, rsp);
  CHECK_EQ(rsp[0], 0x01u);
  CHECK_EQ(rsp[1], BOOT_SVC_ST_OK);
}

int main(void)
{
  test_hello();
  test_uid();
  test_set_node_id_sweep();
  test_set_module_type();
  test_tx_period_and_unsupported();
  test_abandoned_session();
  test_commit();
  test_flash_errors();
  test_reset();
  test_every_command_byte();
  test_short_and_long_frames();
  return TEST_REPORT("test_boot_svc");
}
