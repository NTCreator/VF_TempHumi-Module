/**
  ******************************************************************************
  * @file    app_can.h
  * @brief   CAN1: TH_Data + TH_Status once a second. Phase 3: silent loopback.
  *
  * Every second, with the node_id gate open, one snapshot of the measurement
  * is taken and packed into both frames - so StatusFlags always equals Flags
  * of the same second. TH_Data goes out whether the sensor is alive or not:
  * that is the contract (proposal 5.1). A dead sensor still produces frames,
  * and DataAge in them says how old the numbers are.
  *
  * Phase 3 never reaches the wire, for two independent reasons: bxCAN runs in
  * silent loopback (CANTX held recessive, no ACK given), and the transceiver
  * stays in standby - nothing in this firmware can put it in Normal. Frames
  * come back through the loopback and are compared byte for byte.
  ******************************************************************************
  */
#ifndef APP_APP_CAN_H
#define APP_APP_CAN_H

#include "app/app_config.h"

#include <stdint.h>

/* g_diag.can - see app_diag.h. */
typedef struct
{
  uint8_t  status;          /* 0 = running; otherwise the step that failed:
                               1 init, 2 filter, 3 start (also in app_fault detail) */
  uint8_t  mode;            /* 1 = silent loopback */
  uint8_t  node_id;
  uint8_t  node_id_source;  /* 1 = APP_CAN_NODE_ID_PHASE3 (phase 4: the config) */

  uint32_t slots;           /* 1 s slots that came due with the gate open */
  uint32_t gate_blocked;    /* 1 s slots refused by the node_id gate */
  uint32_t not_running;     /* 1 s slots skipped because CAN failed to start */
  uint32_t data_sent;       /* TH_Data accepted by a mailbox */
  uint32_t status_sent;     /* TH_Status accepted by a mailbox */
  uint32_t tx_full;         /* frames dropped: no free mailbox */
  uint32_t tx_errors;       /* HAL_CAN_AddTxMessage refused */

  uint32_t rx_frames;       /* frames that came back through the loopback */
  uint32_t rx_match;        /* byte for byte equal to what was sent */
  uint32_t rx_mismatch;     /* our ID, different bytes or DLC */
  uint32_t rx_unexpected;   /* an ID we did not send, or a frame for an ID with none in flight */
  uint32_t rx_errors;       /* HAL_CAN_GetRxMessage refused */

  uint8_t  counter;         /* Counter of the last TH_Data */
  uint8_t  bus_off_seen;    /* 1 = ESR.BOFF has been seen since boot: Flags bit 5 */
  uint8_t  tec;             /* ESR transmit error counter, read once a second */
  uint8_t  rec;             /* ESR receive error counter */
  uint8_t  lec;             /* ESR last error code */
  uint32_t esr;             /* ESR raw */

  uint32_t last_data_id;
  uint32_t last_status_id;
  uint8_t  last_data[APP_FRAME_DLC];    /* TH_Data as last sent */
  uint8_t  last_status[APP_FRAME_DLC];  /* TH_Status as last sent */
} app_can_diag_t;

/**
  * @brief Re-initialise bxCAN in silent loopback, accept-all filter, start.
  *
  * MX_CAN_Init() set Normal mode; this sets the mode again and calls
  * HAL_CAN_Init() once more before anything is started - no generated file
  * is touched (CLAUDE.md 6.1), and MspInit does not run twice (state is READY).
  * A failure is latched as APP_FAULT_CAN, not fatal: measuring goes on.
  */
void app_can_init(uint32_t now_ms);

/** @brief Drain the loopback, and send both frames when the second is due. */
void app_can_task(uint32_t now_ms);

#endif /* APP_APP_CAN_H */
