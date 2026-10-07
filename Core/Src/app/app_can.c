/**
  ******************************************************************************
  * @file    app_can.c
  * @brief   bxCAN: schedule, pack, send; NORMAL with a valid config,
  *          silent loopback without one.
  ******************************************************************************
  */

#include "app/app_can.h"

#include "app/app_board.h"
#include "app/app_cfg_boot.h"
#include "app/app_diag.h"
#include "app/app_fault.h"
#include "app/app_frame.h"
#include "app/app_meas.h"
#include "app/app_th.h"
#include "can.h"
#include "main.h"

#include <stdbool.h>
#include <string.h>

#define CAN_STEP_INIT                 1u
#define CAN_STEP_FILTER               2u
#define CAN_STEP_START                3u
#define CAN_DETAIL_STEP_SHIFT         8u
#define CAN_DETAIL_CODE_MASK          0xFFu

#define CAN_FILTER_BANK               0u
#define CAN_SLAVE_START_BANK          14u   /* unused with one CAN; HAL wants a value */
#define CAN_ACCEPT_ALL                0u    /* id and mask both 0: every ID passes */

/* A standard ID in a 32-bit filter register sits at bits 31:21 (STID), with
   IDE = 0 and RTR = 0 below it. FilterIdHigh holds bits 31:16, so the ID
   goes in shifted left by 5. In list mode the "mask" register is a second ID:
   the same one, so exactly one ID passes. */
#define CAN_FILTER_STDID_SHIFT        5u

/* FIFO0 holds at most 3 frames (bxCAN). The drain loop is bounded by that,
   with margin, so a misbehaving fill level can never spin forever. */
#define CAN_DRAIN_MAX                 4u

/* The last frame queued for one ID. In loopback it is matched against what
   comes back; on the wire it is the frame to abort if still pending when the
   next one of its kind is due. */
typedef struct
{
  uint32_t id;
  uint32_t mailbox;      /* CAN_TX_MAILBOXx it went into */
  bool     queued;       /* went into a mailbox and has not been aborted */
  bool     in_flight;    /* loopback only: queued and not yet come back */
  uint8_t  bytes[APP_FRAME_DLC];
} can_sent_t;

static bool       s_running;
static bool       s_live;         /* NORMAL on the wire, not loopback */
static uint8_t    s_node_id;
static uint32_t   s_cmd_id;
static uint32_t   s_next_ms;
static uint8_t    s_counter;
static can_sent_t s_sent_data;
static can_sent_t s_sent_status;

static void can_fail(uint8_t step)
{
  app_can_diag_t *const d = &g_diag.can;

  d->status = step;
  s_running = false;
  app_board_can_transceiver_standby();
  app_fault_raise(APP_FAULT_CAN,
                  ((uint32_t)step << CAN_DETAIL_STEP_SHIFT)
                  | (HAL_CAN_GetError(&hcan) & CAN_DETAIL_CODE_MASK));
}

static bool can_config_filter(bool list_only_cmd)
{
  CAN_FilterTypeDef f;
  (void)memset(&f, 0, sizeof(f));
  f.FilterBank           = CAN_FILTER_BANK;
  f.FilterScale          = CAN_FILTERSCALE_32BIT;
  f.FilterFIFOAssignment = CAN_FILTER_FIFO0;
  f.FilterActivation     = ENABLE;
  f.SlaveStartFilterBank = CAN_SLAVE_START_BANK;

  if (list_only_cmd)
  {
    /* On the wire, with up to 40 nodes: only our command ID, so the FIFO is
       not filled with everybody else's traffic. */
    const uint32_t word_high = s_cmd_id << CAN_FILTER_STDID_SHIFT;
    f.FilterMode       = CAN_FILTERMODE_IDLIST;
    f.FilterIdHigh     = word_high;
    f.FilterIdLow      = 0u;
    f.FilterMaskIdHigh = word_high;
    f.FilterMaskIdLow  = 0u;
  }
  else
  {
    /* Loopback: everything, so our own frames have somewhere to land. */
    f.FilterMode       = CAN_FILTERMODE_IDMASK;
    f.FilterIdHigh     = CAN_ACCEPT_ALL;
    f.FilterIdLow      = CAN_ACCEPT_ALL;
    f.FilterMaskIdHigh = CAN_ACCEPT_ALL;
    f.FilterMaskIdLow  = CAN_ACCEPT_ALL;
  }
  return HAL_CAN_ConfigFilter(&hcan, &f) == HAL_OK;
}

void app_can_init(uint32_t now_ms)
{
  app_can_diag_t *const d = &g_diag.can;
  const app_cfg_t *const cfg = app_cfg_active();

  s_running = false;
  s_counter = 0u;
  (void)memset(&s_sent_data, 0, sizeof(s_sent_data));
  (void)memset(&s_sent_status, 0, sizeof(s_sent_status));

  /* The one decision. A NULL config can only ever produce node 0xFF, which
     the gate refuses, and loopback, which has no path to the wire. */
  s_node_id = (cfg != NULL) ? cfg->node_id : (uint8_t)APP_NODE_ID_UNSET;
  s_live = (cfg != NULL) && (APP_BENCH_CAN_LOOPBACK == 0);
  s_cmd_id = APP_CAN_CMD_ID(s_node_id);
  s_next_ms = now_ms + APP_TX_PERIOD_MS;

  d->node_id = s_node_id;
  d->node_id_source = g_diag.cfg.node_id_source;

  app_board_can_transceiver_standby();

  hcan.Init.Mode = s_live ? CAN_MODE_NORMAL : CAN_MODE_SILENT_LOOPBACK;
  /* MX_CAN_Init (from the .ioc) has both of these off. On the wire that
     would lose TH_Data every time it loses arbitration - and 0x400 loses to
     every 0x2xx and 0x3xx frame of the other modules - and would leave a
     bus-off node silent until reset. Stale frames are handled by aborting
     them (can_send), not by never retrying. */
  hcan.Init.AutoRetransmission = s_live ? ENABLE : DISABLE;
  hcan.Init.AutoBusOff = s_live ? ENABLE : DISABLE;
  if (HAL_CAN_Init(&hcan) != HAL_OK)
  {
    can_fail(CAN_STEP_INIT);
    return;
  }
  d->mode = s_live ? APP_CAN_MODE_NORMAL : APP_CAN_MODE_SILENT_LOOPBACK;

  if (!can_config_filter(s_live))
  {
    can_fail(CAN_STEP_FILTER);
    return;
  }

  /* Transceiver BEFORE HAL_CAN_Start (phase 5; phase 4 had it after). bxCAN
     is still in init mode here, so CANTX is recessive and nothing reaches
     the wire. Start then waits for 11 recessive bits on the REAL bus before
     taking part. The other order lets bxCAN sync to the idle RXD of a
     standby transceiver, then join a busy bus in the middle of somebody
     else's frame - read garbage, and as an error-active node answer with an
     error flag that destroys that frame. The config is handed over as the
     proof the function demands. */
  d->transceiver_on = (s_live && app_board_can_transceiver_normal(cfg)) ? 1u : 0u;

  if (HAL_CAN_Start(&hcan) != HAL_OK)
  {
    can_fail(CAN_STEP_START);   /* puts the transceiver back in standby */
    d->transceiver_on = 0u;
    d->stb_level = app_board_can_stb_level();
    return;
  }

  d->stb_level = app_board_can_stb_level();
  d->status = 0u;
  s_running = true;
}

/* One frame read from FIFO0. */
static void can_check_rx(const CAN_RxHeaderTypeDef *h, const uint8_t data[APP_FRAME_DLC])
{
  app_can_diag_t *const d = &g_diag.can;
  const bool std_data = (h->IDE == CAN_ID_STD) && (h->RTR == CAN_RTR_DATA);

  d->rx_frames++;

  if (s_live)
  {
    /* The filter lets only our command ID through. Commands are phase 6:
       read out so the FIFO never fills, counted, dropped. */
    if (std_data && (h->StdId == s_cmd_id))
    {
      d->rx_cmd_seen++;
    }
    else
    {
      d->rx_unexpected++;
    }
    return;
  }

  can_sent_t *sent = NULL;
  if (std_data)
  {
    if (s_sent_data.in_flight && (h->StdId == s_sent_data.id))
    {
      sent = &s_sent_data;
    }
    else if (s_sent_status.in_flight && (h->StdId == s_sent_status.id))
    {
      sent = &s_sent_status;
    }
  }

  if (sent == NULL)
  {
    d->rx_unexpected++;
    return;
  }

  sent->in_flight = false;
  if ((h->DLC == APP_FRAME_DLC) && (memcmp(data, sent->bytes, APP_FRAME_DLC) == 0))
  {
    d->rx_match++;
  }
  else
  {
    d->rx_mismatch++;
  }
}

static void can_drain(void)
{
  app_can_diag_t *const d = &g_diag.can;

  for (uint32_t n = 0u; n < CAN_DRAIN_MAX; n++)
  {
    if (HAL_CAN_GetRxFifoFillLevel(&hcan, CAN_RX_FIFO0) == 0u)
    {
      return;
    }

    CAN_RxHeaderTypeDef h;
    uint8_t data[APP_FRAME_DLC];
    (void)memset(data, 0, sizeof(data));
    if (HAL_CAN_GetRxMessage(&hcan, CAN_RX_FIFO0, &h, data) != HAL_OK)
    {
      d->rx_errors++;
      return;
    }
    can_check_rx(&h, data);
  }
}

static void can_read_esr(void)
{
  app_can_diag_t *const d = &g_diag.can;
  const uint32_t esr = hcan.Instance->ESR;

  d->esr = esr;
  d->tec = (uint8_t)((esr & CAN_ESR_TEC_Msk) >> CAN_ESR_TEC_Pos);
  d->rec = (uint8_t)((esr & CAN_ESR_REC_Msk) >> CAN_ESR_REC_Pos);
  d->lec = (uint8_t)((esr & CAN_ESR_LEC_Msk) >> CAN_ESR_LEC_Pos);
  if ((esr & CAN_ESR_BOFF_Msk) != 0u)
  {
    d->bus_off_seen = 1u;   /* latched until reset: TH_Data Flags bit 5 */
  }
}

/* Queue one frame. Exactly one counter moves for the frame itself; an abort
   of its stale predecessor is counted on top. */
static void can_send(uint32_t id, const uint8_t bytes[APP_FRAME_DLC], can_sent_t *sent,
                     uint32_t *sent_counter)
{
  app_can_diag_t *const d = &g_diag.can;

  /* Never a stale frame on the wire: at most one frame of each kind waits in
     a mailbox. If last second's is still there - nobody ACKs, or it keeps
     losing arbitration - abort it; this second's replaces it, and Counter
     shows the gap. Without this, three old frames would go out first the
     moment the bus came back. */
  if (sent->queued && (HAL_CAN_IsTxMessagePending(&hcan, sent->mailbox) != 0u))
  {
    if (HAL_CAN_AbortTxRequest(&hcan, sent->mailbox) == HAL_OK)
    {
      d->tx_aborted++;
    }
    else
    {
      d->tx_errors++;
    }
  }
  sent->queued = false;

  if (HAL_CAN_GetTxMailboxesFreeLevel(&hcan) == 0u)
  {
    d->tx_full++;   /* dropped, not retried: a late frame behind a newer one is worse */
    return;
  }

  CAN_TxHeaderTypeDef h;
  uint32_t mailbox = 0u;
  (void)memset(&h, 0, sizeof(h));
  h.StdId = id;
  h.IDE = CAN_ID_STD;
  h.RTR = CAN_RTR_DATA;
  h.DLC = APP_FRAME_DLC;
  h.TransmitGlobalTime = DISABLE;

  if (HAL_CAN_AddTxMessage(&hcan, &h, bytes, &mailbox) != HAL_OK)
  {
    d->tx_errors++;
    return;
  }

  sent->id = id;
  sent->mailbox = mailbox;
  sent->queued = true;
  sent->in_flight = !s_live;
  (void)memcpy(sent->bytes, bytes, APP_FRAME_DLC);
  (*sent_counter)++;
}

static void can_send_both(uint32_t now_ms)
{
  app_can_diag_t *const d = &g_diag.can;
  const app_meas_t *const m = app_th_meas();

  /* One snapshot for both frames: StatusFlags == Flags of the same second. */
  const uint8_t flags = (uint8_t)(app_meas_flags(m, now_ms)
                                  | ((d->bus_off_seen != 0u) ? TH_FLAG_BUS_OFF_SEEN : 0u));

  /* Before the first valid read t_x10 / rh_x10 are 0 (app_meas_init): sent as
     0 / 0 with DataAge 0xFFFF and NEVER_READ - the contract as written
     (phase 3 plan, question 1, option A). */
  const app_frame_data_t data =
  {
    m->t_x10, m->rh_x10, app_meas_age_x100ms(m, now_ms), s_counter, flags
  };
  const app_frame_status_t status =
  {
    (uint8_t)APP_FW_VERSION, flags, s_node_id, (uint8_t)APP_TX_PERIOD_X10MS,
    app_meas_modbus_ok(m), app_meas_modbus_err(m), (uint8_t)APP_CMD_SEQ_NONE
  };

  uint8_t data_bytes[APP_FRAME_DLC];
  uint8_t status_bytes[APP_FRAME_DLC];
  app_frame_pack_data(&data, data_bytes);
  app_frame_pack_status(&status, status_bytes);

  /* Counter moves once per 1 s slot, whether or not a mailbox takes the
     frame (phase 3 plan, question 2, option A): a frame that came due and
     never reached the log always leaves a jump. */
  d->counter = s_counter;
  s_counter++;

  const uint32_t id_data = app_frame_id_data(s_node_id);
  const uint32_t id_status = app_frame_id_status(s_node_id);

  /* TH_Data first: with TransmitFifoPriority disabled bxCAN sends the lower
     ID first anyway, and 0x400 < 0x402. */
  can_send(id_data, data_bytes, &s_sent_data, &d->data_sent);
  can_send(id_status, status_bytes, &s_sent_status, &d->status_sent);

  d->last_data_id = id_data;
  d->last_status_id = id_status;
  (void)memcpy(d->last_data, data_bytes, APP_FRAME_DLC);
  (void)memcpy(d->last_status, status_bytes, APP_FRAME_DLC);
}

void app_can_task(uint32_t now_ms)
{
  app_can_diag_t *const d = &g_diag.can;

  if (s_running)
  {
    can_drain();   /* drain before send */
  }

  if ((int32_t)(now_ms - s_next_ms) < 0)
  {
    return;
  }
  s_next_ms = now_ms + APP_TX_PERIOD_MS;

  if (!s_running)
  {
    d->not_running++;
    return;
  }

  can_read_esr();

  /* The gate stands before anything that puts an ID on bxCAN. Without a valid
     config the node_id is 0xFF and every slot ends here. */
  if (!app_frame_node_ok(s_node_id))
  {
    d->gate_blocked++;
    return;
  }

  d->slots++;
  can_send_both(now_ms);
}
