/**
  ******************************************************************************
  * @file    app_th.h
  * @brief   The temperature-humidity measurement: ask the sensor every 2 s,
  *          keep the answer and its age.
  *
  * Runs one Modbus transaction at a time through drv_rs485, never blocks, and
  * books every outcome in app_meas. Everything a human may want to look at
  * goes into g_diag.th (app_diag.h) - read it with CubeIDE Live Expressions.
  *
  * The sensor refreshes every 2 s (datasheet p.1), so that is the poll period:
  * asking faster only re-reads the same number (CLAUDE.md 1.1).
  ******************************************************************************
  */
#ifndef APP_APP_TH_H
#define APP_APP_TH_H

#include "app/app_meas.h"

#include <stdint.h>

/* Diagnostic scan (phase 2 plan, question 2, option A). Report only: what it
   finds is never adopted, and nothing is ever written to the sensor. */
typedef struct
{
  uint8_t  ran;                          /* 1 = started this boot (at most once) */
  uint8_t  done;                         /* 1 = finished, back to normal polling */
  uint8_t  found;                        /* 1 = a sensor answered */
  uint8_t  addr;                         /* address byte of its reply */
  uint16_t reg_value;                    /* register 0x07D0 = its configured address */
  uint8_t  hal_err;                      /* 1 = a baud change failed, scan cut short */
  uint8_t  status[APP_TH_SCAN_BAUDS];    /* mb_status_t per baud, 0xFF = not tried */
  uint32_t baud;                         /* baud it answered at, 0 = none */
} app_th_scan_t;

/* g_diag.th - the console this board does not have. */
typedef struct
{
  /* Live, refreshed on every pass of the super-loop. */
  int16_t  t_x10;                        /* last valid reading, x10 C */
  uint16_t rh_x10;                       /* last valid reading, x10 %RH */
  uint16_t age_x100ms;                   /* DataAge: 0xFFFF = never read */
  uint8_t  flags;                        /* TH_FLAG_* (proposal 5.1) */
  uint8_t  fresh;                        /* 1 = LED shows "measuring" */
  uint32_t baud;                         /* USART1 baud right now */

  /* The last transaction, whatever its outcome (polls only, not the scan). */
  uint8_t  last_status;                  /* mb_status_t */
  uint8_t  last_exception;
  uint8_t  last_rx_len;
  uint8_t  last_rx[MB_RX_BUF_LEN];       /* the raw reply, good or bad */
  uint32_t last_uart_error;              /* HAL ErrorCode bits, on MB_ERR_UART */
  uint16_t latency_ms;                   /* transmit complete -> first reply byte */
  uint16_t latency_max_ms;

  /* Books: started == count[MB_OK] + errors + (1 if in flight). */
  uint32_t started;
  uint32_t count[MB_STATUS_COUNT];       /* indexed by mb_status_t */
  uint16_t modbus_ok;                    /* TH_Status ModbusOk as phase 3 will send it */
  uint8_t  modbus_err;                   /* TH_Status ModbusErr, saturating */
  uint8_t  in_flight;
  uint32_t abort_fail;                   /* HAL_UART_Abort did not return HAL_OK */

  app_th_scan_t scan;
} app_th_diag_t;

/** @brief Start the schedule. First poll MB_FIRST_POLL_MS from now. */
void app_th_init(uint32_t now_ms);

/** @brief Advance the transaction, start the next one when due. Never blocks. */
void app_th_task(uint32_t now_ms);

/** @brief The measurement record - what phase 3 packs into TH_Data. */
const app_meas_t *app_th_meas(void);

#endif /* APP_APP_TH_H */
