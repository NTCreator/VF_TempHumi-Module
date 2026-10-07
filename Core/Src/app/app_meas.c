/**
  ******************************************************************************
  * @file    app_meas.c
  * @brief   Last valid reading, its age, flags and counters. Pure.
  ******************************************************************************
  */

#include "app/app_meas.h"

#include <string.h>

#define MEAS_REG_HUMIDITY       0u   /* reply order of a 2-register read, datasheet p.7 */
#define MEAS_REG_TEMPERATURE    1u
#define MEAS_ERR_U8_MAX         0xFFu
#define MEAS_STALE_AGE_X100MS   (APP_TH_STALE_MS / APP_TH_AGE_LSB_MS)   /* 60 = 6.0 s */

_Static_assert((APP_TH_STALE_MS % APP_TH_AGE_LSB_MS) == 0u, "STALE threshold must be a whole DataAge step");

/* The register layout above assumes the read starts at humidity and covers
   both registers - which is what app_th asks for. */
_Static_assert(SHT_READ_FIRST_REG == SHT_REG_HUMIDITY, "reply order assumes humidity first");
_Static_assert(SHT_REG_TEMPERATURE == SHT_REG_HUMIDITY + 1u, "temperature must follow humidity");
_Static_assert(SHT_READ_COUNT == 2u, "one transaction reads both registers");

static bool meas_in_range(int16_t t_x10, uint16_t rh_x10)
{
  return (t_x10 >= SHT_TEMP_MIN_X10) && (t_x10 <= SHT_TEMP_MAX_X10)
         && (rh_x10 <= (uint16_t)SHT_RH_MAX_X10);
  /* SHT_RH_MIN_X10 is 0: an unsigned value cannot be below it. */
}

void app_meas_init(app_meas_t *m)
{
  (void)memset(m, 0, sizeof(*m));
  m->last_status = (uint8_t)MB_OK;   /* no transaction yet: no TIMEOUT / CRC_ERR flag */
}

void app_meas_started(app_meas_t *m)
{
  m->started++;
}

void app_meas_record(app_meas_t *m, mb_status_t st, const mb_reply_t *reply, uint32_t now_ms)
{
  /* An out-of-range status value would index past count[]. It cannot happen
     with the enum as it is, but a counter that silently does not move is the
     failure this module exists to prevent: book it as a transport error. */
  if ((uint32_t)st >= (uint32_t)MB_STATUS_COUNT)
  {
    st = MB_ERR_UART;
  }

  m->count[st]++;
  m->last_status = (uint8_t)st;
  m->last_exception = 0u;

  if (st == MB_ERR_EXCEPTION)
  {
    m->last_exception = reply->exception;
    return;
  }
  if (st != MB_OK)
  {
    return;
  }

  m->rh_x10  = reply->regs[MEAS_REG_HUMIDITY];
  m->t_x10   = (int16_t)reply->regs[MEAS_REG_TEMPERATURE];   /* two's complement, p.7 */
  m->ok_ms   = now_ms;
  m->ever_ok = true;
  m->range   = !meas_in_range(m->t_x10, m->rh_x10);
}

uint16_t app_meas_age_x100ms(const app_meas_t *m, uint32_t now_ms)
{
  if (!m->ever_ok)
  {
    return APP_TH_AGE_NEVER;
  }

  /* Unsigned subtraction: correct across the 2^32 ms tick wrap. */
  const uint32_t age = (now_ms - m->ok_ms) / APP_TH_AGE_LSB_MS;
  return (age > APP_TH_AGE_MAX) ? (uint16_t)APP_TH_AGE_MAX : (uint16_t)age;
}

bool app_meas_fresh(const app_meas_t *m, uint32_t now_ms)
{
  /* Judged on the DataAge that goes on the bus, not on raw milliseconds.
     Otherwise a reply 6050 ms old would go out as DataAge 6.0 s with STALE
     set, next to another 6.0 s frame without it, and the log would contradict
     itself. This way STALE == (DataAge > 6.0 s), checkable from the .blf. */
  return m->ever_ok && (app_meas_age_x100ms(m, now_ms) <= MEAS_STALE_AGE_X100MS);
}

uint8_t app_meas_flags(const app_meas_t *m, uint32_t now_ms)
{
  uint8_t f = 0u;

  /* NEVER_READ and STALE are separate on purpose (proposal 5.1): "never
     talked to the sensor" and "talked to it, but long ago" send people to fix
     two different things. Never read is never also STALE. */
  if (!m->ever_ok)
  {
    f |= TH_FLAG_NEVER_READ;
  }
  else if (!app_meas_fresh(m, now_ms))
  {
    f |= TH_FLAG_STALE;
  }

  if (m->last_status == (uint8_t)MB_ERR_TIMEOUT)
  {
    f |= TH_FLAG_TIMEOUT;
  }
  if (m->last_status == (uint8_t)MB_ERR_CRC)
  {
    f |= TH_FLAG_CRC_ERR;
  }
  if (m->ever_ok && m->range)
  {
    f |= TH_FLAG_RANGE;
  }
  return f;
}

uint32_t app_meas_errors(const app_meas_t *m)
{
  uint32_t sum = 0u;

  for (uint32_t i = 0u; i < (uint32_t)MB_STATUS_COUNT; i++)
  {
    if (i != (uint32_t)MB_OK)
    {
      sum += m->count[i];
    }
  }
  return sum;
}

uint16_t app_meas_modbus_ok(const app_meas_t *m)
{
  return (uint16_t)(m->count[MB_OK] & 0xFFFFu);
}

uint8_t app_meas_modbus_err(const app_meas_t *m)
{
  /* Saturates, does not wrap: "255 errors" must read as badly broken, and a
     counter that wrapped to 3 would read as nearly fine (proposal 5.2). */
  const uint32_t e = app_meas_errors(m);
  return (e > MEAS_ERR_U8_MAX) ? (uint8_t)MEAS_ERR_U8_MAX : (uint8_t)e;
}
