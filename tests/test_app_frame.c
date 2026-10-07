/* app_frame: IDs, the node_id gate, and the byte layout of TH_Data / TH_Status.

   A pack and an unpack that are wrong in the same way agree with each other
   perfectly, so a round trip alone proves nothing about the DBC. Every layout
   here is also checked against bytes written by hand from the DBC signal
   definitions (BO_ 1024 / BO_ 1026), and every field is pushed through on its
   own to show that no two fields overlap. tests/test_dbc_layout.c then checks
   the same layout against the DBC file itself. */

#include "app/app_frame.h"
#include "test_check.h"

#include <string.h>

static void check_bytes(const uint8_t got[APP_FRAME_DLC], const uint8_t want[APP_FRAME_DLC],
                        const char *what)
{
  const int same = (memcmp(got, want, APP_FRAME_DLC) == 0);
  if (!same)
  {
    printf("  %s\n    got ", what);
    for (unsigned i = 0u; i < APP_FRAME_DLC; i++) { printf(" %02X", got[i]); }
    printf("\n    want");
    for (unsigned i = 0u; i < APP_FRAME_DLC; i++) { printf(" %02X", want[i]); }
    printf("\n");
  }
  CHECK(same);
}

static void test_ids(void)
{
  /* ID map 2.1 / proposal section 2: node 0x30 -> 0x400 / 0x402 ... 0x39 -> 0x490 / 0x492. */
  CHECK_EQ(app_frame_id_data(0x30u), 0x400);
  CHECK_EQ(app_frame_id_status(0x30u), 0x402);
  CHECK_EQ(app_frame_id_data(0x35u), 0x450);
  CHECK_EQ(app_frame_id_status(0x35u), 0x452);
  CHECK_EQ(app_frame_id_data(0x39u), 0x490);
  CHECK_EQ(app_frame_id_status(0x39u), 0x492);
}

static void test_gate(void)
{
  for (unsigned id = 0x30u; id <= 0x39u; id++)
  {
    CHECK(app_frame_node_ok((uint8_t)id));
  }
  /* Neighbours, another type's range, the next type, unset. */
  static const uint8_t refused[] = { 0x2F, 0x3A, 0x3F, 0x29, 0x20, 0x19, 0x40, 0x00, 0xFF };
  for (size_t i = 0; i < sizeof(refused); i++)
  {
    CHECK(!app_frame_node_ok(refused[i]));
  }
}

static void test_hand_written(void)
{
  uint8_t out[APP_FRAME_DLC];

  /* Datasheet p.7 numbers: -20.5 C = 0xFF33, 25.8 %RH = 0x0102. Age 1.5 s. */
  const app_frame_data_t d1 = { -205, 258u, 15u, 0x2Au, 0x10u };
  static const uint8_t want_d1[APP_FRAME_DLC] = { 0x33, 0xFF, 0x02, 0x01, 0x0F, 0x00, 0x2A, 0x10 };
  app_frame_pack_data(&d1, out);
  check_bytes(out, want_d1, "TH_Data p.7 values");

  /* Never read: 0 / 0 (phase 3 plan, question 1, option A), DataAge 6553.5,
     NEVER_READ | TIMEOUT. */
  const app_frame_data_t d2 = { 0, 0u, 0xFFFFu, 0x00u, 0x03u };
  static const uint8_t want_d2[APP_FRAME_DLC] = { 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x03 };
  app_frame_pack_data(&d2, out);
  check_bytes(out, want_d2, "TH_Data never read");

  /* What the board showed in phase 2: 25.4 C, 51.0 %RH. */
  const app_frame_data_t d3 = { 254, 510u, 3u, 0x07u, 0x00u };
  static const uint8_t want_d3[APP_FRAME_DLC] = { 0xFE, 0x00, 0xFE, 0x01, 0x03, 0x00, 0x07, 0x00 };
  app_frame_pack_data(&d3, out);
  check_bytes(out, want_d3, "TH_Data phase 2 board values");

  const app_frame_status_t s1 = { 1u, 0x0Au, 0x30u, 100u, 0x1234u, 255u, 0u };
  static const uint8_t want_s1[APP_FRAME_DLC] = { 0x01, 0x0A, 0x30, 0x64, 0x34, 0x12, 0xFF, 0x00 };
  app_frame_pack_status(&s1, out);
  check_bytes(out, want_s1, "TH_Status");
}

/* Exactly bytes lo..hi are 0xFF, every other byte is 0. */
static void expect_only(const uint8_t out[APP_FRAME_DLC], unsigned lo, unsigned hi)
{
  for (unsigned b = 0u; b < APP_FRAME_DLC; b++)
  {
    const int inside = (b >= lo) && (b <= hi);
    CHECK_EQ(out[b], inside ? 0xFF : 0x00);
  }
}

/* Push 0xFF.. through one field at a time: exactly the bytes of that field
   may change. A round trip cannot see two fields sharing a byte. */
static void test_fields_isolated(void)
{
  uint8_t out[APP_FRAME_DLC];
  app_frame_data_t d;
  app_frame_status_t s;

#define DATA_ONE(field, value, lo, hi) \
  do { \
    memset(&d, 0, sizeof(d)); d.field = (value); app_frame_pack_data(&d, out); \
    expect_only(out, (lo), (hi)); \
  } while (0)

#define STATUS_ONE(field, value, lo, hi) \
  do { \
    memset(&s, 0, sizeof(s)); s.field = (value); app_frame_pack_status(&s, out); \
    expect_only(out, (lo), (hi)); \
  } while (0)

  DATA_ONE(t_x10, (int16_t)-1, 0u, 1u);
  DATA_ONE(rh_x10, 0xFFFFu, 2u, 3u);
  DATA_ONE(age_x100ms, 0xFFFFu, 4u, 5u);
  DATA_ONE(counter, 0xFFu, 6u, 6u);
  DATA_ONE(flags, 0xFFu, 7u, 7u);

  STATUS_ONE(fw_version, 0xFFu, 0u, 0u);
  STATUS_ONE(status_flags, 0xFFu, 1u, 1u);
  STATUS_ONE(node_id, 0xFFu, 2u, 2u);
  STATUS_ONE(tx_period_x10ms, 0xFFu, 3u, 3u);
  STATUS_ONE(modbus_ok, 0xFFFFu, 4u, 5u);
  STATUS_ONE(modbus_err, 0xFFu, 6u, 6u);
  STATUS_ONE(cmd_seq, 0xFFu, 7u, 7u);

#undef DATA_ONE
#undef STATUS_ONE
}

static void test_round_trip(void)
{
  uint8_t buf[APP_FRAME_DLC];
  const int16_t temps[] = { -400, -205, -1, 0, 1, 254, 600, INT16_MIN, INT16_MAX };

  for (size_t i = 0; i < sizeof(temps) / sizeof(temps[0]); i++)
  {
    const app_frame_data_t in = { temps[i], (uint16_t)(510u + i), (uint16_t)(0xFFF0u + i), (uint8_t)i, (uint8_t)(0x1Fu - i) };
    app_frame_data_t back;
    app_frame_pack_data(&in, buf);
    app_frame_unpack_data(buf, &back);
    CHECK(memcmp(&in, &back, sizeof(in)) == 0);
  }

  const app_frame_status_t in = { 1u, 0x2Au, 0x39u, 100u, 0xBEEFu, 0x7Fu, 0x55u };
  app_frame_status_t back;
  app_frame_pack_status(&in, buf);
  app_frame_unpack_status(buf, &back);
  CHECK(memcmp(&in, &back, sizeof(in)) == 0);
}

int main(void)
{
  test_ids();
  test_gate();
  test_hand_written();
  test_fields_isolated();
  test_round_trip();
  return TEST_REPORT("test_app_frame");
}
