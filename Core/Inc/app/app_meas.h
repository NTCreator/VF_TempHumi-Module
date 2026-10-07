/**
  ******************************************************************************
  * @file    app_meas.h
  * @brief   The last valid reading, how old it is, and what went wrong since.
  *
  * This is the part of the product that matters most (CLAUDE.md 1.1): a stale
  * number looks exactly like a fresh one, so every reading carries its age,
  * counted from the last VALID Modbus reply - not from the last request, not
  * from the last CAN frame. Proposal 5.1 defines the age and the flags; this
  * module is their only implementation, and phase 3 only packs what it says.
  *
  * Pure: no HAL, no clock of its own - the caller passes the tick in. All of
  * it runs on a PC (tests/test_app_meas.c), tick wrap included.
  ******************************************************************************
  */
#ifndef APP_APP_MEAS_H
#define APP_APP_MEAS_H

#include "app/drv_modbus.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
  /* The last valid reading. Kept as the sensor sent it: x10, no conversion
     (proposal 5.1). Never cleared once set - its age says how much it is
     still worth. */
  bool     ever_ok;
  int16_t  t_x10;          /* two's complement on the wire, datasheet p.7 */
  uint16_t rh_x10;
  uint32_t ok_ms;          /* tick of the last valid reply */
  bool     range;          /* that reading was outside the datasheet range */

  /* The most recent transaction, whatever its outcome. */
  uint8_t  last_status;    /* mb_status_t */
  uint8_t  last_exception;

  /* One counter per outcome. started == sum(count) + (1 if one is in flight). */
  uint32_t started;
  uint32_t count[MB_STATUS_COUNT];
} app_meas_t;

/** @brief Empty record: never read, all counters zero. */
void app_meas_init(app_meas_t *m);

/** @brief A request is about to go out. Pairs with exactly one app_meas_record(). */
void app_meas_started(app_meas_t *m);

/**
  * @brief Close the transaction opened by app_meas_started().
  *
  * Increments exactly one counter. On MB_OK the reading is taken from
  * reply->regs (humidity, then temperature - datasheet p.7) and its age
  * restarts; any other status leaves the last valid reading untouched.
  * An out-of-range reading is kept and flagged, not dropped (phase 2 plan,
  * question 3): the number on the bus stays the number in the sensor.
  */
void app_meas_record(app_meas_t *m, mb_status_t st, const mb_reply_t *reply, uint32_t now_ms);

/**
  * @brief DataAge in 0.1 s (proposal 5.1).
  * @return APP_TH_AGE_NEVER (0xFFFF) if nothing valid was ever read,
  *         otherwise at most APP_TH_AGE_MAX (0xFFFE).
  */
uint16_t app_meas_age_x100ms(const app_meas_t *m, uint32_t now_ms);

/**
  * @brief True if a valid reading exists and its DataAge is not above
  *        APP_TH_STALE_MS. Judged on DataAge itself, so STALE in a log always
  *        agrees with the DataAge next to it: 6099 ms is 6.0 s and fresh,
  *        6100 ms is 6.1 s and stale.
  */
bool app_meas_fresh(const app_meas_t *m, uint32_t now_ms);

/** @brief Flags byte bits 0..4 of proposal 5.1 (TH_FLAG_*). */
uint8_t app_meas_flags(const app_meas_t *m, uint32_t now_ms);

/** @brief Every transaction that did not end in MB_OK. */
uint32_t app_meas_errors(const app_meas_t *m);

/** @brief TH_Status ModbusOk: valid replies, 16 bits, wraps (proposal 5.2). */
uint16_t app_meas_modbus_ok(const app_meas_t *m);

/** @brief TH_Status ModbusErr: errors, 8 bits, SATURATES at 255 (proposal 5.2). */
uint8_t app_meas_modbus_err(const app_meas_t *m);

#endif /* APP_APP_MEAS_H */
