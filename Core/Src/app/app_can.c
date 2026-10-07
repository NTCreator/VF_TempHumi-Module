/**
  ******************************************************************************
  * @file    app_can.c
  * @brief   bxCAN in silent loopback: schedule, pack, send, compare.
  ******************************************************************************
  */

#include "app/app_can.h"

#include "app/app_diag.h"
#include "app/app_fault.h"
#include "app/app_frame.h"
#include "app/app_meas.h"
#include "app/app_th.h"
#include "can.h"
#include "main.h"

#include <stdbool.h>
#include <string.h>

#define CAN_MODE_IS_SILENT_LOOPBACK   1u
#define CAN_NODE_SOURCE_PHASE3        1u

#define CAN_STEP_INIT                 1u
#define CAN_STEP_FILTER               2u
#define CAN_STEP_START                3u
#define CAN_DETAIL_STEP_SHIFT         8u
#define CAN_DETAIL_CODE_MASK          0xFFu

#define CAN_FILTER_BANK               0u
#define CAN_SLAVE_START_BANK          14u   /* unused with one CAN; HAL wants a value */
#define CAN_ACCEPT_ALL                0u    /* id and mask both 0: every ID passes */

/* FIFO0 holds at most 3 frames (bxCAN). The drain loop is bounded by that,
   with margin, so a misbehaving fill level can never spin forever. */
#define CAN_DRAIN_MAX                 4u

/* One frame in flight per ID, at most: the copy of what was last sent. */
typedef struct
{
  uint32_t id;
  bool     in_flight;
  uint8_t  bytes[APP_FRAME_DLC];
} can_sent_t;

static bool       s_running;
static uint8_t    s_node_id;
static uint32_t   s_next_ms;
static uint8_t    s_counter;
static can_sent_t s_sent_data;
static can_sent_t s_sent_status;

static void can_fail(uint8_t step)
{
  app_can_diag_t *const d = &g_diag.can;

  d->status = step;
  s_running = false;
  app_fault_raise(APP_FAULT_CAN,
                  ((uint32_t)step << CAN_DETAIL_STEP_SHIFT)
                  | (HAL_CAN_GetError(&hcan) & CAN_DETAIL_CODE_MASK));
}

void app_can_init(uint32_t now_ms)
{
  app_can_diag_t *const d = &g_diag.can;

  s_running = false;
  s_counter = 0u;
  (void)memset(&s_sent_data, 0, sizeof(s_sent_data));
  (void)memset(&s_sent_status, 0, sizeof(s_sent_status));

  /* Phase 3 has no config store: the node_id is a compile-time constant
     (app_config.h). Phase 4 replaces this line with a CRC-checked config. */
  s_node_id = (uint8_t)APP_CAN_NODE_ID_PHASE3;
  d->node_id = s_node_id;
  d->node_id_source = CAN_NODE_SOURCE_PHASE3;
  s_next_ms = now_ms + APP_TX_PERIOD_MS;

  /* Silent loopback is a constant of phase 3, not a switch. */
  hcan.Init.Mode = CAN_MODE_SILENT_LOOPBACK;
  if (HAL_CAN_Init(&hcan) != HAL_OK)
  {
    can_fail(CAN_STEP_INIT);
    return;
  }
  d->mode = CAN_MODE_IS_SILENT_LOOPBACK;

  CAN_FilterTypeDef f;
  (void)memset(&f, 0, sizeof(f));
  f.FilterBank           = CAN_FILTER_BANK;
  f.FilterMode           = CAN_FILTERMODE_IDMASK;
  f.FilterScale          = CAN_FILTERSCALE_32BIT;
  f.FilterIdHigh         = CAN_ACCEPT_ALL;
  f.FilterIdLow          = CAN_ACCEPT_ALL;
  f.FilterMaskIdHigh     = CAN_ACCEPT_ALL;
  f.FilterMaskIdLow      = CAN_ACCEPT_ALL;
  f.FilterFIFOAssignment = CAN_FILTER_FIFO0;
  f.FilterActivation     = ENABLE;
  f.SlaveStartFilterBank = CAN_SLAVE_START_BANK;
  if (HAL_CAN_ConfigFilter(&hcan, &f) != HAL_OK)
  {
    can_fail(CAN_STEP_FILTER);
    return;
  }

  if (HAL_CAN_Start(&hcan) != HAL_OK)
  {
    can_fail(CAN_STEP_START);
    return;
  }

  d->status = 0u;
  s_running = true;
}

/* Compare one frame that came back with the copy of what went out. */
static void can_check_rx(const CAN_RxHeaderTypeDef *h, const uint8_t data[APP_FRAME_DLC])
{
  app_can_diag_t *const d = &g_diag.can;
  can_sent_t *sent = NULL;

  d->rx_frames++;

  if ((h->IDE == CAN_ID_STD) && (h->RTR == CAN_RTR_DATA))
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

/* Queue one frame. Exactly one counter moves on every path. */
static void can_send(uint32_t id, const uint8_t bytes[APP_FRAME_DLC], can_sent_t *sent,
                     uint32_t *sent_counter)
{
  app_can_diag_t *const d = &g_diag.can;

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
  sent->in_flight = true;
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
    can_drain();   /* drain before send: each ID has at most one frame in flight */
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

  /* The gate stands before anything that puts an ID on bxCAN. */
  if (!app_frame_node_ok(s_node_id))
  {
    d->gate_blocked++;
    return;
  }

  d->slots++;
  can_send_both(now_ms);
}
