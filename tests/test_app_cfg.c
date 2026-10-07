/* app_cfg: the 16-byte flash record and the rule that decides whether this
   module may speak on CAN1. Phase 5's bootloader links the same file, so
   what is proven here is proven for both. */

#include "app/app_cfg.h"
#include "test_check.h"

#include <string.h>

static app_cfg_t valid_cfg(uint8_t node_id)
{
  app_cfg_t c;
  app_cfg_defaults(&c);
  c.node_id = node_id;
  app_cfg_seal(&c);
  return c;
}

static void test_crc32(void)
{
  /* The standard check value of CRC-32/ISO-HDLC (IEEE 802.3). */
  static const uint8_t check[9] = { '1', '2', '3', '4', '5', '6', '7', '8', '9' };
  CHECK_EQ(app_cfg_crc32(check, 9u), 0xCBF43926u);
  CHECK_EQ(app_cfg_crc32(check, 0u), 0x00000000u);
}

static void test_blank_and_zero(void)
{
  app_cfg_t c;

  memset(&c, 0xFF, sizeof(c));            /* erased flash */
  CHECK_EQ(app_cfg_check(&c), APP_CFG_BLANK);
  CHECK(!app_cfg_valid(&c));

  memset(&c, 0x00, sizeof(c));            /* damaged, not blank */
  CHECK_EQ(app_cfg_check(&c), APP_CFG_BAD_MAGIC);
  CHECK(!app_cfg_valid(&c));

  CHECK(!app_cfg_valid(NULL));
}

static void test_defaults_are_silent(void)
{
  app_cfg_t c;
  app_cfg_defaults(&c);
  /* Right magic, version, type and CRC - and still not valid: unconfigured
     means silent. */
  CHECK_EQ(c.magic, APP_CFG_MAGIC);
  CHECK_EQ(c.version, APP_CFG_VERSION);
  CHECK_EQ(c.module_type, APP_MODULE_TYPE);
  CHECK_EQ(c.node_id, APP_NODE_ID_UNSET);
  CHECK_EQ(c.crc32, app_cfg_crc32((const uint8_t *)&c, APP_CFG_CRC_SPAN));
  CHECK_EQ(app_cfg_check(&c), APP_CFG_BAD_NODE);
}

static void test_every_bit_flip(void)
{
  const app_cfg_t good = valid_cfg(0x30u);
  CHECK(app_cfg_valid(&good));

  /* CRC-32 catches every single-bit error; the earlier checks may catch it
     first. Either way, not one of the 128 flips may come out valid. */
  for (unsigned byte = 0u; byte < sizeof(app_cfg_t); byte++)
  {
    for (unsigned bit = 0u; bit < 8u; bit++)
    {
      app_cfg_t c = good;
      ((uint8_t *)&c)[byte] ^= (uint8_t)(1u << bit);
      CHECK(!app_cfg_valid(&c));
    }
  }
}

static void test_each_check(void)
{
  app_cfg_t c;

  c = valid_cfg(0x30u);
  c.version = 0u;
  CHECK_EQ(app_cfg_check(&c), APP_CFG_BAD_VERSION);
  c.version = 2u;
  CHECK_EQ(app_cfg_check(&c), APP_CFG_BAD_VERSION);

  /* CRC wrong, everything else right. */
  c = valid_cfg(0x30u);
  c.crc32 ^= 1u;
  CHECK_EQ(app_cfg_check(&c), APP_CFG_BAD_CRC);

  /* Another module type, with a CRC that is right for it: the type is
     checked for real, not only through the node_id nibble. */
  static const uint8_t types[] = { 0x00, 0x01, 0x02, 0x04, 0xFF };
  for (size_t i = 0; i < sizeof(types); i++)
  {
    c = valid_cfg(0x30u);
    c.module_type = types[i];
    c.crc32 = app_cfg_crc32((const uint8_t *)&c, APP_CFG_CRC_SPAN);
    CHECK_EQ(app_cfg_check(&c), APP_CFG_BAD_TYPE);
  }

  /* Reserved bytes not zero, CRC right for them: an unknown layout. */
  for (unsigned i = 0u; i < APP_CFG_RESERVED_LEN; i++)
  {
    c = valid_cfg(0x30u);
    c.reserved[i] = 0x01u;
    c.crc32 = app_cfg_crc32((const uint8_t *)&c, APP_CFG_CRC_SPAN);
    CHECK_EQ(app_cfg_check(&c), APP_CFG_BAD_RESERVED);
  }

  /* Magic of the angle and vibration modules: refused at the first check. */
  c = valid_cfg(0x30u);
  c.magic = 0x31464341u;   /* "ACF1" */
  CHECK_EQ(app_cfg_check(&c), APP_CFG_BAD_MAGIC);
  c.magic = 0x31464356u;   /* "VCF1" */
  CHECK_EQ(app_cfg_check(&c), APP_CFG_BAD_MAGIC);
}

static void test_node_sweep(void)
{
  /* All 256 node_ids, each sealed so only the node_id can fail: exactly the
     ten of type 0x3 pass. */
  unsigned valid = 0u;
  for (unsigned id = 0u; id < 256u; id++)
  {
    app_cfg_t c = valid_cfg((uint8_t)id);
    const int ok = app_cfg_valid(&c);
    const int want = (id >= 0x30u) && (id <= 0x39u);
    CHECK_EQ(ok, want);
    if (!ok)
    {
      CHECK_EQ(app_cfg_check(&c), APP_CFG_BAD_NODE);
    }
    valid += (unsigned)ok;
    CHECK_EQ(app_cfg_node_id_ok((uint8_t)id), want);
  }
  CHECK_EQ(valid, 10u);
}

static void test_layout_bytes(void)
{
  /* The bench config as it must sit in flash, little-endian, written by hand
     from the struct in app_cfg.h: "TCF1", version 1, node 0x30, type 3,
     reserved 0, then the CRC of those 12 bytes. */
  static const uint8_t want_head[12] =
  {
    0x54, 0x43, 0x46, 0x31,   /* 'T' 'C' 'F' '1' */
    0x01, 0x00,               /* version 1 */
    0x30,                     /* node_id */
    0x03,                     /* module_type */
    0x00, 0x00, 0x00, 0x00    /* reserved */
  };
  const app_cfg_t c = valid_cfg(0x30u);
  const uint8_t *const raw = (const uint8_t *)&c;

  CHECK(memcmp(raw, want_head, sizeof(want_head)) == 0);
  const uint32_t crc = app_cfg_crc32(want_head, sizeof(want_head));
  CHECK_EQ(raw[12], crc & 0xFFu);
  CHECK_EQ(raw[13], (crc >> 8) & 0xFFu);
  CHECK_EQ(raw[14], (crc >> 16) & 0xFFu);
  CHECK_EQ(raw[15], (crc >> 24) & 0xFFu);
  printf("  bench config CRC = 0x%08lX\n", (unsigned long)crc);
}

int main(void)
{
  test_crc32();
  test_blank_and_zero();
  test_defaults_are_silent();
  test_every_bit_flip();
  test_each_check();
  test_node_sweep();
  test_layout_bytes();
  return TEST_REPORT("test_app_cfg");
}
