/**
  ******************************************************************************
  * @file    app_can.h
  * @brief   CAN1: TH_Data + TH_Status once a second.
  *
  * Every second, with the node_id gate open, one snapshot of the measurement
  * is taken and packed into both frames - so StatusFlags always equals Flags
  * of the same second. TH_Data goes out whether the sensor is alive or not:
  * that is the contract (proposal 5.1). A dead sensor still produces frames,
  * and DataAge in them says how old the numbers are.
  *
  * Two states, decided in one place (app_can_init), from the config:
  *
  *                    valid config              no valid config (blank too)
  *   node_id          from the config           0xFF
  *   bxCAN            NORMAL, auto-retransmit,  silent loopback (phase 3)
  *                    auto bus-off recovery
  *   transceiver      Normal (PA10 low)         standby (PA10 high)
  *   filter           list: 0x010 + node_id     open
  *   gate             open                      closed: no frame at all
  *
  * A missing config is stopped three independent ways: no path to the wire,
  * transceiver in standby, and the gate. APP_BENCH_CAN_LOOPBACK forces the
  * right-hand column for bxCAN and the transceiver even with a valid config.
  ******************************************************************************
  */
#ifndef APP_APP_CAN_H
#define APP_APP_CAN_H

#include "app/app_config.h"

#include <stdint.h>

#define APP_CAN_MODE_SILENT_LOOPBACK  1u
#define APP_CAN_MODE_NORMAL           2u

/* g_diag.can - see app_diag.h. */
typedef struct
{
  uint8_t  status;          /* 0 = running; otherwise the step that failed:
                               1 init, 2 filter, 3 start (also in app_fault detail) */
  uint8_t  mode;            /* APP_CAN_MODE_* */
  uint8_t  node_id;         /* from the config, or 0xFF */
  uint8_t  node_id_source;  /* APP_CFG_SOURCE_* (app_cfg_store.h) */
  uint8_t  transceiver_on;  /* 1 = TJA1044 in Normal */
  uint8_t  stb_level;       /* PA10 as read back after init: 1 standby, 0 Normal */

  uint32_t slots;           /* 1 s slots that came due with the gate open */
  uint32_t gate_blocked;    /* 1 s slots refused by the node_id gate */
  uint32_t not_running;     /* 1 s slots skipped because CAN failed to start */
  uint32_t data_sent;       /* TH_Data accepted by a mailbox */
  uint32_t status_sent;     /* TH_Status accepted by a mailbox */
  uint32_t tx_aborted;      /* an older frame of the same kind was still pending: aborted */
  uint32_t tx_full;         /* frames dropped: no free mailbox */
  uint32_t tx_errors;       /* HAL_CAN_AddTxMessage refused */

  uint32_t rx_frames;       /* frames read from FIFO0 */
  uint32_t rx_match;        /* loopback: byte for byte equal to what was sent */
  uint32_t rx_mismatch;     /* loopback: our ID, different bytes or DLC */
  uint32_t rx_cmd_seen;     /* NORMAL: a frame on our command ID (handled in phase 6) */
  uint32_t rx_unexpected;   /* anything else */
  uint32_t rx_errors;       /* HAL_CAN_GetRxMessage refused */

  uint8_t  counter;         /* Counter of the last TH_Data */
  uint8_t  bus_off_seen;    /* 1 = ESR.BOFF has been seen since boot: Flags bit 5 */
  uint8_t  tec;             /* ESR transmit error counter, read once a second */
  uint8_t  rec;             /* ESR receive error counter */
  uint8_t  lec;             /* ESR last error code */
  uint32_t esr;             /* ESR raw */

  uint32_t last_data_id;
  uint32_t last_status_id;
  uint8_t  last_data[APP_FRAME_DLC];    /* TH_Data as last queued */
  uint8_t  last_status[APP_FRAME_DLC];  /* TH_Status as last queued */
} app_can_diag_t;

/**
  * @brief Choose the state from app_cfg_active(), re-initialise bxCAN in it,
  *        set the filter, start - and only then, with a valid config, put the
  *        transceiver in Normal.
  *
  * MX_CAN_Init() set Normal mode with retransmission off; this sets mode,
  * retransmission and bus-off recovery again and calls HAL_CAN_Init() once
  * more before anything is started - no generated file is touched (CLAUDE.md
  * 6.1), and MspInit does not run twice (state is READY). A failure is
  * latched as APP_FAULT_CAN, not fatal: measuring goes on, the transceiver
  * stays in standby.
  */
void app_can_init(uint32_t now_ms);

/** @brief Drain FIFO0, and send both frames when the second is due. */
void app_can_task(uint32_t now_ms);

#endif /* APP_APP_CAN_H */
