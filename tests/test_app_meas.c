/* app_meas: age, flags and counters - the part of this product that matters
   most (CLAUDE.md 1.1). Proposal 5.1 / 5.2 define every number checked here. */

#include "app/app_meas.h"
#include "test_check.h"

#include <string.h>

static mb_reply_t reply(uint16_t rh_x10, int16_t t_x10)
{
  mb_reply_t r;
  memset(&r, 0, sizeof(r));
  r.addr = 0x01u;
  r.regs[0] = rh_x10;
  r.regs[1] = (uint16_t)t_x10;
  return r;
}

static void one(app_meas_t *m, mb_status_t st, const mb_reply_t *r, uint32_t now)
{
  app_meas_started(m);
  app_meas_record(m, st, r, now);
}

static void test_never_read(void)
{
  app_meas_t m;
  app_meas_init(&m);

  CHECK_EQ(app_meas_age_x100ms(&m, 0u), APP_TH_AGE_NEVER);
  CHECK_EQ(app_meas_age_x100ms(&m, 123456u), APP_TH_AGE_NEVER);
  CHECK(!app_meas_fresh(&m, 0u));
  /* No transaction yet: only NEVER_READ. Never read is never also STALE. */
  CHECK_EQ(app_meas_flags(&m, 0u), TH_FLAG_NEVER_READ);
  CHECK_EQ(app_meas_flags(&m, 999999u), TH_FLAG_NEVER_READ);

  const mb_reply_t none = reply(0u, 0);
  one(&m, MB_ERR_TIMEOUT, &none, 2000u);
  CHECK_EQ(app_meas_flags(&m, 2000u), TH_FLAG_NEVER_READ | TH_FLAG_TIMEOUT);
  one(&m, MB_ERR_CRC, &none, 4000u);
  CHECK_EQ(app_meas_flags(&m, 4000u), TH_FLAG_NEVER_READ | TH_FLAG_CRC_ERR);
  CHECK_EQ(app_meas_age_x100ms(&m, 4000u), APP_TH_AGE_NEVER);
}

static void test_p7_values(void)
{
  app_meas_t m;
  app_meas_init(&m);
  const mb_reply_t r = reply(258u, -205);     /* datasheet p.7: 25.8 %RH, -20.5 C */
  one(&m, MB_OK, &r, 1000u);

  CHECK(m.ever_ok);
  CHECK_EQ(m.rh_x10, 258);
  CHECK_EQ(m.t_x10, -205);
  CHECK(!m.range);
  CHECK_EQ(app_meas_flags(&m, 1000u), 0);
}

static void test_age(void)
{
  app_meas_t m;
  app_meas_init(&m);
  const mb_reply_t r = reply(500u, 250);
  one(&m, MB_OK, &r, 10000u);

  CHECK_EQ(app_meas_age_x100ms(&m, 10000u), 0);
  CHECK_EQ(app_meas_age_x100ms(&m, 10099u), 0);
  CHECK_EQ(app_meas_age_x100ms(&m, 10100u), 1);
  CHECK_EQ(app_meas_age_x100ms(&m, 10000u + 1999u), 19);

  /* Saturates one below "never read". 0xFFFE * 100 ms = 6553.4 s. */
  CHECK_EQ(app_meas_age_x100ms(&m, 10000u + 6553400u), APP_TH_AGE_MAX);
  CHECK_EQ(app_meas_age_x100ms(&m, 10000u + 6553399u), 0xFFFDu);
  CHECK_EQ(app_meas_age_x100ms(&m, 10000u + 100000000u), APP_TH_AGE_MAX);

  /* A failed transaction does not touch the reading or its age. */
  const mb_reply_t junk = reply(1u, 1);
  one(&m, MB_ERR_TIMEOUT, &junk, 12000u);
  CHECK_EQ(m.rh_x10, 500);
  CHECK_EQ(m.t_x10, 250);
  CHECK_EQ(app_meas_age_x100ms(&m, 12000u), 20);

  /* The next valid reply restarts it. */
  one(&m, MB_OK, &r, 14000u);
  CHECK_EQ(app_meas_age_x100ms(&m, 14000u), 0);
}

static void test_tick_wrap(void)
{
  app_meas_t m;
  app_meas_init(&m);
  const mb_reply_t r = reply(500u, 250);
  one(&m, MB_OK, &r, 0xFFFFFF00u);             /* 256 ms before the 2^32 wrap */

  CHECK_EQ(app_meas_age_x100ms(&m, 0xFFFFFF00u), 0);
  CHECK_EQ(app_meas_age_x100ms(&m, 0x00000000u), 2);    /* 256 ms */
  CHECK_EQ(app_meas_age_x100ms(&m, 0x00001000u), 43);   /* 4352 ms */
  CHECK(app_meas_fresh(&m, 0x00001000u));
  CHECK(!app_meas_fresh(&m, 0x00001800u));              /* 6400 ms */
}

static void test_stale_boundary(void)
{
  app_meas_t m;
  app_meas_init(&m);
  const mb_reply_t r = reply(500u, 250);
  one(&m, MB_OK, &r, 0u);

  /* STALE == (DataAge > 6.0 s), judged on the DataAge that is sent, so a log
     never shows 6.0 s with STALE in one frame and without it in another. */
  CHECK(app_meas_fresh(&m, 6000u));
  CHECK(app_meas_fresh(&m, 6099u));            /* DataAge 60 = 6.0 s */
  CHECK_EQ(app_meas_flags(&m, 6099u) & TH_FLAG_STALE, 0);
  CHECK(!app_meas_fresh(&m, 6100u));           /* DataAge 61 = 6.1 s */
  CHECK_EQ(app_meas_flags(&m, 6100u), TH_FLAG_STALE);
  CHECK_EQ(app_meas_flags(&m, 6100u) & TH_FLAG_NEVER_READ, 0);

  /* Stale and the last transaction timed out: both say so. */
  one(&m, MB_ERR_TIMEOUT, &r, 8000u);
  CHECK_EQ(app_meas_flags(&m, 8000u), TH_FLAG_STALE | TH_FLAG_TIMEOUT);

  /* A good reply clears STALE and TIMEOUT together. */
  one(&m, MB_OK, &r, 10000u);
  CHECK_EQ(app_meas_flags(&m, 10000u), 0);
}

static void test_range(void)
{
  app_meas_t m;
  app_meas_init(&m);

  /* Datasheet p.1: -40..+60 C, 0..100 %RH. The edges are in range. */
  mb_reply_t r = reply(1000u, 600);
  one(&m, MB_OK, &r, 0u);
  CHECK(!m.range);
  r = reply(0u, -400);
  one(&m, MB_OK, &r, 0u);
  CHECK(!m.range);

  /* Outside: kept and flagged, not dropped (plan question 3), and its age
     restarts like any valid reply. */
  r = reply(500u, 605);
  one(&m, MB_OK, &r, 2000u);
  CHECK(m.range);
  CHECK_EQ(m.t_x10, 605);
  CHECK_EQ(app_meas_age_x100ms(&m, 2000u), 0);
  CHECK_EQ(app_meas_flags(&m, 2000u), TH_FLAG_RANGE);

  r = reply(1001u, 250);
  one(&m, MB_OK, &r, 4000u);
  CHECK(m.range);
  r = reply(500u, -401);
  one(&m, MB_OK, &r, 6000u);
  CHECK(m.range);

  /* RANGE belongs to the reading: a failed transaction does not clear it,
     the next in-range reading does. */
  one(&m, MB_ERR_CRC, &r, 8000u);
  CHECK(app_meas_flags(&m, 8000u) & TH_FLAG_RANGE);
  r = reply(500u, 250);
  one(&m, MB_OK, &r, 10000u);
  CHECK_EQ(app_meas_flags(&m, 10000u), 0);
}

static void test_counters(void)
{
  app_meas_t m;
  app_meas_init(&m);
  mb_reply_t r = reply(500u, 250);
  r.exception = 0x02u;

  /* Every outcome moves exactly one counter, and the books always balance:
     started == ok + errors. */
  for (uint32_t st = 0u; st < (uint32_t)MB_STATUS_COUNT; st++)
  {
    app_meas_t before = m;
    one(&m, (mb_status_t)st, &r, 1000u * st);
    for (uint32_t i = 0u; i < (uint32_t)MB_STATUS_COUNT; i++)
    {
      CHECK_EQ(m.count[i], before.count[i] + ((i == st) ? 1u : 0u));
    }
    CHECK_EQ(m.started, m.count[MB_OK] + app_meas_errors(&m));
  }
  CHECK_EQ(m.last_status, MB_ERR_TX);
  CHECK_EQ(m.count[MB_ERR_EXCEPTION], 1);

  /* The exception code is kept with the transaction that carried it. */
  one(&m, MB_ERR_EXCEPTION, &r, 20000u);
  CHECK_EQ(m.last_exception, 0x02);
  one(&m, MB_ERR_TIMEOUT, &r, 22000u);
  CHECK_EQ(m.last_exception, 0);

  /* A status outside the enum still moves a counter - never none. */
  const uint32_t uart_before = m.count[MB_ERR_UART];
  one(&m, (mb_status_t)200, &r, 24000u);
  CHECK_EQ(m.count[MB_ERR_UART], uart_before + 1u);
  CHECK_EQ(m.started, m.count[MB_OK] + app_meas_errors(&m));
}

static void test_contract_counters(void)
{
  app_meas_t m;
  app_meas_init(&m);
  const mb_reply_t r = reply(500u, 250);

  /* ModbusErr saturates at 255 (proposal 5.2). */
  for (uint32_t i = 0u; i < 254u; i++)
  {
    one(&m, MB_ERR_TIMEOUT, &r, i);
  }
  CHECK_EQ(app_meas_modbus_err(&m), 254);
  one(&m, MB_ERR_CRC, &r, 0u);
  CHECK_EQ(app_meas_modbus_err(&m), 255);
  for (uint32_t i = 0u; i < 1000u; i++)
  {
    one(&m, MB_ERR_SHORT, &r, i);
  }
  CHECK_EQ(app_meas_modbus_err(&m), 255);      /* not 1255 & 0xFF = 231 */
  CHECK_EQ(app_meas_errors(&m), 1255);

  /* ModbusOk wraps at 16 bits. */
  m.count[MB_OK] = 0xFFFFu;
  CHECK_EQ(app_meas_modbus_ok(&m), 0xFFFF);
  m.count[MB_OK] = 0x10003u;
  CHECK_EQ(app_meas_modbus_ok(&m), 3);
}

int main(void)
{
  test_never_read();
  test_p7_values();
  test_age();
  test_tick_wrap();
  test_stale_boundary();
  test_range();
  test_counters();
  test_contract_counters();
  return TEST_REPORT("test_app_meas");
}
