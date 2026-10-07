/**
  ******************************************************************************
  * @file    app_th.c
  * @brief   Poll schedule and transaction state machine for the SHT sensor.
  ******************************************************************************
  */

#include "app/app_th.h"

#include "app/app_config.h"
#include "app/app_diag.h"
#include "app/app_fault.h"
#include "app/app_led.h"
#include "app/drv_modbus.h"
#include "app/drv_rs485.h"

#include <stdbool.h>
#include <string.h>

#define TH_SCAN_NOT_TRIED   0xFFu

typedef enum
{
  TH_JOB_POLL = 0,   /* the measurement: counted in app_meas */
  TH_JOB_SCAN = 1    /* the diagnostic scan: counted in g_diag.th.scan */
} th_job_t;

static const mb_read_req_t k_poll = { SHT_SLAVE_ADDR_DEFAULT, SHT_READ_FIRST_REG, SHT_READ_COUNT };
static const mb_read_req_t k_scan = { MB_ADDR_ANY, SHT_REG_ADDRESS, 1u };
static const uint32_t      k_scan_bauds[APP_TH_SCAN_BAUDS] =
{
  SHT_BAUD_DEFAULT, SHT_BAUD_ALT_1, SHT_BAUD_ALT_2
};

static app_meas_t s_meas;

static uint8_t  s_poll_req[MB_READ_REQ_LEN];
static uint8_t  s_poll_req_len;
static uint8_t  s_scan_req[MB_READ_REQ_LEN];
static uint8_t  s_scan_req_len;

static bool     s_busy;          /* a transaction is in flight */
static th_job_t s_job;
static uint32_t s_boot_ms;
static uint32_t s_next_poll_ms;
static bool     s_scanning;
static uint8_t  s_scan_step;     /* index into k_scan_bauds */

/* "now has reached due", correct across the 2^32 ms tick wrap as long as the
   two are less than 24.8 days apart. */
static bool th_due(uint32_t now_ms, uint32_t due_ms)
{
  return (int32_t)(now_ms - due_ms) >= 0;
}

static void th_copy_books(void)
{
  app_th_diag_t *const d = &g_diag.th;

  d->started = s_meas.started;
  (void)memcpy(d->count, s_meas.count, sizeof(d->count));
  d->modbus_ok = app_meas_modbus_ok(&s_meas);
  d->modbus_err = app_meas_modbus_err(&s_meas);
  d->last_status = s_meas.last_status;
  d->last_exception = s_meas.last_exception;
}

/* -------------------------------------------------------------------------- */
/* Scan                                                                       */
/* -------------------------------------------------------------------------- */

static void th_scan_finish(uint32_t now_ms)
{
  app_th_scan_t *const sc = &g_diag.th.scan;

  /* Back to the factory default, whatever the scan found: it reports, it
     never adopts. A sensor configured differently is an installation error,
     and quietly following it would hide that. */
  if (!drv_rs485_set_baud(SHT_BAUD_DEFAULT))
  {
    /* USART1 is now at an unknown rate: no reading can be trusted to arrive.
       Latched, because nothing in this firmware can repair it. */
    app_fault_raise(APP_FAULT_HAL_INIT, APP_FAULT_DETAIL_UART_REINIT);
  }

  s_scanning = false;
  sc->done = 1u;
  s_next_poll_ms = now_ms + MB_POLL_PERIOD_MS;
}

static void th_scan_record(mb_status_t st, const mb_reply_t *r, uint32_t now_ms)
{
  app_th_scan_t *const sc = &g_diag.th.scan;

  sc->status[s_scan_step] = (uint8_t)st;
  if (st == MB_OK)
  {
    sc->found = 1u;
    sc->addr = r->addr;
    sc->reg_value = r->regs[0];
    sc->baud = k_scan_bauds[s_scan_step];
    th_scan_finish(now_ms);
    return;
  }

  s_scan_step++;
  if (s_scan_step >= APP_TH_SCAN_BAUDS)
  {
    th_scan_finish(now_ms);
  }
}

/* -------------------------------------------------------------------------- */
/* Transactions                                                               */
/* -------------------------------------------------------------------------- */

static void th_finish(mb_status_t st, const mb_reply_t *r, const drv_rs485_snap_t *snap,
                      uint32_t now_ms)
{
  app_th_diag_t *const d = &g_diag.th;

  if (!drv_rs485_stop())
  {
    d->abort_fail++;
  }
  s_busy = false;
  d->in_flight = 0u;

  if (s_job == TH_JOB_SCAN)
  {
    th_scan_record(st, r, now_ms);
    return;
  }

  app_meas_record(&s_meas, st, r, now_ms);

  d->last_rx_len = snap->len;
  (void)memcpy(d->last_rx, snap->rx, snap->len);
  d->last_uart_error = (st == MB_ERR_UART) ? snap->error_code : 0u;
  if (snap->len > 0u)
  {
    const uint32_t lat = snap->first_ms - snap->tc_ms;
    d->latency_ms = (uint16_t)((lat > 0xFFFFu) ? 0xFFFFu : lat);
    if (d->latency_ms > d->latency_max_ms)
    {
      d->latency_max_ms = d->latency_ms;
    }
  }
  th_copy_books();
}

static void th_start(th_job_t job, uint32_t now_ms)
{
  const uint8_t *req = (job == TH_JOB_POLL) ? s_poll_req : s_scan_req;
  const uint8_t  len = (job == TH_JOB_POLL) ? s_poll_req_len : s_scan_req_len;

  s_job = job;
  s_busy = true;
  /* Only polls are in the books; a scan step is counted in g_diag.th.scan. */
  g_diag.th.in_flight = (job == TH_JOB_POLL) ? 1u : 0u;

  if (job == TH_JOB_POLL)
  {
    app_meas_started(&s_meas);
    s_next_poll_ms = now_ms + MB_POLL_PERIOD_MS;
    /* Keep g_diag's books balanced while the request is in flight, not only
       once it ends: started == ok + errors + in_flight at every instant. */
    th_copy_books();
  }

  if (!drv_rs485_send(req, len, now_ms))
  {
    drv_rs485_snap_t none;
    mb_reply_t       nothing;
    (void)memset(&none, 0, sizeof(none));
    (void)memset(&nothing, 0, sizeof(nothing));
    th_finish(MB_ERR_TX, &nothing, &none, now_ms);
  }
}

static void th_parse_and_finish(const drv_rs485_snap_t *snap, uint32_t now_ms)
{
  mb_reply_t r;
  (void)memset(&r, 0, sizeof(r));

  const mb_read_req_t *const req = (s_job == TH_JOB_POLL) ? &k_poll : &k_scan;
  const mb_status_t st = drv_modbus_parse_read(snap->rx, snap->len, req, &r);
  th_finish(st, &r, snap, now_ms);
}

/* Decide whether the transaction in flight has ended, and how. Every branch
   that ends it goes through th_finish(), which books exactly one outcome. */
static void th_service(uint32_t now_ms)
{
  drv_rs485_snap_t snap;
  mb_reply_t       nothing;
  drv_rs485_snapshot(&snap);
  (void)memset(&nothing, 0, sizeof(nothing));

  if (snap.uart_error)
  {
    th_finish(MB_ERR_UART, &nothing, &snap, now_ms);
    return;
  }

  if (snap.phase == (uint8_t)RS485_TX)
  {
    /* Transmit complete never came: drop DE by force (th_finish stops the
       driver) rather than let this module hold the bus. */
    if ((now_ms - snap.sent_ms) > MB_TX_TIMEOUT_MS)
    {
      th_finish(MB_ERR_TX, &nothing, &snap, now_ms);
    }
    return;
  }

  if (snap.phase != (uint8_t)RS485_RX)
  {
    /* Busy here but the driver is idle: nothing will ever complete this
       transaction. Book it rather than wait forever. */
    th_finish(MB_ERR_TX, &nothing, &snap, now_ms);
    return;
  }

  if (snap.len == 0u)
  {
    if ((now_ms - snap.tc_ms) > MB_REPLY_TIMEOUT_MS)
    {
      th_finish(MB_ERR_TIMEOUT, &nothing, &snap, now_ms);
    }
    return;
  }

  const uint16_t expected = drv_modbus_expected_len(snap.rx, snap.len);
  const bool complete = (expected != 0u) && (snap.len >= expected);
  const bool silent   = (now_ms - snap.last_ms) > MB_GAP_MS;
  const bool full     = snap.overflow || (snap.len >= MB_RX_BUF_LEN);

  if (complete || silent || full)
  {
    th_parse_and_finish(&snap, now_ms);
  }
}

/* -------------------------------------------------------------------------- */
/* Public                                                                     */
/* -------------------------------------------------------------------------- */

void app_th_init(uint32_t now_ms)
{
  app_th_diag_t *const d = &g_diag.th;

  app_meas_init(&s_meas);
  drv_rs485_init();

  /* Both requests are constant: build them once. A zero length here would
     mean a request this parser cannot ask - it shows as MB_ERR_TX on every
     poll, never as silence. */
  s_poll_req_len = drv_modbus_build_read(&k_poll, s_poll_req);
  s_scan_req_len = drv_modbus_build_read(&k_scan, s_scan_req);

  s_busy = false;
  s_scanning = false;
  s_scan_step = 0u;
  s_boot_ms = now_ms;
  s_next_poll_ms = now_ms + MB_FIRST_POLL_MS;

  (void)memset(d->scan.status, TH_SCAN_NOT_TRIED, sizeof(d->scan.status));
  th_copy_books();
}

void app_th_task(uint32_t now_ms)
{
  app_th_diag_t *const d = &g_diag.th;

  if (s_busy)
  {
    th_service(now_ms);
  }
  else if (s_scanning)
  {
    /* One baud per transaction. The previous one ended on a 200 ms timeout or
       an 8 ms silence, so the 3.5-character gap Modbus asks for is already
       there. */
    if (drv_rs485_set_baud(k_scan_bauds[s_scan_step]))
    {
      th_start(TH_JOB_SCAN, now_ms);
    }
    else
    {
      d->scan.hal_err = 1u;
      th_scan_finish(now_ms);
    }
  }
  else if (th_due(now_ms, s_next_poll_ms))
  {
    const bool scan_due = !s_meas.ever_ok && (d->scan.ran == 0u)
                          && (s_meas.started >= APP_TH_SCAN_AFTER_TRIES);
    if (scan_due)
    {
      d->scan.ran = 1u;
      s_scanning = true;
      s_scan_step = 0u;
    }
    else
    {
      th_start(TH_JOB_POLL, now_ms);
    }
  }
  else
  {
    /* Nothing to do this pass. */
  }

  /* Live view and LED, every pass. */
  const bool fresh = app_meas_fresh(&s_meas, now_ms);
  const bool silent_since_boot = !s_meas.ever_ok && ((now_ms - s_boot_ms) >= APP_TH_SILENT_LED_MS);
  const bool silent = silent_since_boot || (s_meas.ever_ok && !fresh);

  d->t_x10 = s_meas.t_x10;
  d->rh_x10 = s_meas.rh_x10;
  d->age_x100ms = app_meas_age_x100ms(&s_meas, now_ms);
  d->flags = app_meas_flags(&s_meas, now_ms);
  d->fresh = fresh ? 1u : 0u;
  d->baud = drv_rs485_baud();

  app_led_set_status(silent ? (uint8_t)APP_FAULT_SENSOR : 0u);
  app_led_set_measuring(fresh);
}

const app_meas_t *app_th_meas(void)
{
  return &s_meas;
}
