/**
  ******************************************************************************
  * @file    boot_can.c
  ******************************************************************************
  */

#include "app/boot_can.h"

#include "app/app_board.h"
#include "app/boot_board.h"
#include "app/boot_config.h"
#include "app/boot_svc.h"

#include "can.h"
#include "main.h"

#include <string.h>

#define CAN_FILTER_BANK          0u
#define CAN_SLAVE_START_BANK     14u   /* unused with one CAN; HAL wants a value */

/* A standard ID in a 32-bit filter register sits at bits 31:21 (STID), with
   IDE (bit 2) and RTR (bit 1) below it. FilterIdHigh holds bits 31:16, so the
   ID goes in shifted left by 5. Mask mode, with the mask covering STID, IDE
   and RTR, and IDE = RTR = 0 in the ID: exactly a standard DATA frame on
   0x7E0 passes. */
#define CAN_FILTER_STDID_SHIFT   5u
#define CAN_FILTER_STDID_MASK    0x7FFu
#define CAN_FILTER_IDE_RTR       ((1u << 2) | (1u << 1))

uint8_t boot_can_start(void)
{
  CAN_FilterTypeDef f;
  (void)memset(&f, 0, sizeof(f));

  f.FilterBank           = CAN_FILTER_BANK;
  f.FilterMode           = CAN_FILTERMODE_IDMASK;
  f.FilterScale          = CAN_FILTERSCALE_32BIT;
  f.FilterIdHigh         = (uint32_t)BOOT_SVC_REQ_ID << CAN_FILTER_STDID_SHIFT;
  f.FilterIdLow          = 0u;
  f.FilterMaskIdHigh     = (uint32_t)CAN_FILTER_STDID_MASK << CAN_FILTER_STDID_SHIFT;
  f.FilterMaskIdLow      = CAN_FILTER_IDE_RTR;
  f.FilterFIFOAssignment = CAN_FILTER_FIFO0;
  f.FilterActivation     = ENABLE;
  f.SlaveStartFilterBank = CAN_SLAVE_START_BANK;
  if (HAL_CAN_ConfigFilter(&hcan, &f) != HAL_OK)
  {
    return BOOT_CAN_FAIL_FILTER;
  }

  /* Transceiver BEFORE the controller (phase 5 plan 2.6). Leaving init mode
     needs eleven recessive bits on RX: with the transceiver already Normal
     they are the real bus, so bxCAN joins between frames instead of in the
     middle of one, where it would see a stuff or form error and send an error
     flag over somebody else's frame. bxCAN is still in init mode here, so
     CANTX is recessive and nothing reaches the wire yet. */
  boot_board_can_transceiver_normal();

  if (HAL_CAN_Start(&hcan) != HAL_OK)
  {
    app_board_can_transceiver_standby();
    return BOOT_CAN_FAIL_START;
  }
  return BOOT_CAN_OK;
}

uint8_t boot_can_recv(uint8_t out8[8], uint8_t *len)
{
  if (HAL_CAN_GetRxFifoFillLevel(&hcan, CAN_RX_FIFO0) == 0u)
  {
    return BOOT_CAN_RX_NONE;
  }

  CAN_RxHeaderTypeDef h;
  (void)memset(out8, 0, BOOT_SVC_DLC);
  if (HAL_CAN_GetRxMessage(&hcan, CAN_RX_FIFO0, &h, out8) != HAL_OK)
  {
    return BOOT_CAN_RX_ERROR;
  }

  /* The filter passes data frames only, so DLC is the payload length. bxCAN
     reports DLC up to 15 for an 8-byte frame (CAN 2.0: 9..15 mean 8). */
  *len = (h.DLC < BOOT_SVC_DLC) ? (uint8_t)h.DLC : (uint8_t)BOOT_SVC_DLC;
  return BOOT_CAN_RX_FRAME;
}

bool boot_can_send(const uint8_t data8[8])
{
  CAN_TxHeaderTypeDef h;
  (void)memset(&h, 0, sizeof(h));
  h.StdId              = BOOT_SVC_RSP_ID;
  h.IDE                = CAN_ID_STD;
  h.RTR                = CAN_RTR_DATA;
  h.DLC                = BOOT_SVC_DLC;
  h.TransmitGlobalTime = DISABLE;

  uint8_t payload[BOOT_SVC_DLC];
  (void)memcpy(payload, data8, sizeof(payload));

  uint32_t mailbox = 0u;
  if (HAL_CAN_AddTxMessage(&hcan, &h, payload, &mailbox) != HAL_OK)
  {
    return false;
  }

  /* With automatic retransmission off (MX_CAN_Init, NART = 1) a mailbox
     empties after one attempt, ACKed or not. The bound covers the case where
     it does neither: abort, so stuck responses can never fill all three
     mailboxes and refuse every later one. A lost response is recovered by the
     mainboard, which retries up to three times (CAN2 section 3). */
  const uint32_t t0 = HAL_GetTick();
  while (HAL_CAN_IsTxMessagePending(&hcan, mailbox) != 0u)
  {
    if ((HAL_GetTick() - t0) >= BOOT_SEND_TIMEOUT_MS)
    {
      (void)HAL_CAN_AbortTxRequest(&hcan, mailbox);
      return false;
    }
  }

  /* Left the mailbox; TXOK says whether it was ACKed. */
  const uint32_t txok = (mailbox == CAN_TX_MAILBOX0) ? CAN_TSR_TXOK0 :
                        (mailbox == CAN_TX_MAILBOX1) ? CAN_TSR_TXOK1 : CAN_TSR_TXOK2;
  return (hcan.Instance->TSR & txok) != 0u;
}
