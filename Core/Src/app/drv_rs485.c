/**
  ******************************************************************************
  * @file    drv_rs485.c
  * @brief   USART1 + MAX485 half-duplex transport.
  ******************************************************************************
  */

#include "app/drv_rs485.h"

#include "app/app_board.h"
#include "main.h"
#include "usart.h"

#include <string.h>

/* MAX485 DE high = driver on, transmitting (MAX481.PDF p.10, Table 1). /RE
   is tied to DE, so while the driver is on the receiver is off and RO is
   high-Z (Table 2): this module never hears its own request. R10 holds PB7
   high meanwhile, which the UART reads as an idle line. */
#define RS485_DRIVER_ON         GPIO_PIN_SET

#define RS485_RX_FLAGS_CLEAR    (UART_CLEAR_PEF | UART_CLEAR_FEF | UART_CLEAR_NEF | \
                                 UART_CLEAR_OREF | UART_CLEAR_IDLEF)

/* State shared with the UART interrupt. */
static volatile uint8_t  s_phase = (uint8_t)RS485_IDLE;
static volatile uint8_t  s_rx[MB_RX_BUF_LEN];
static volatile uint8_t  s_rx_len;
static volatile bool     s_overflow;
static volatile bool     s_uart_error;
static volatile uint32_t s_error_code;
static volatile uint32_t s_sent_ms;
static volatile uint32_t s_tc_ms;
static volatile uint32_t s_first_ms;
static volatile uint32_t s_last_ms;

/* HAL keeps pointers to these for the whole transfer, so they must outlive
   the call that starts it. */
static uint8_t s_tx[MB_READ_REQ_LEN];
static uint8_t s_rx_byte;

/* The only place in the firmware that raises DE. Static on purpose
   (CLAUDE.md 7.1): every path that raises it is in this file, and every one
   of them ends in app_board_rs485_release(). */
static void rs485_drive_bus(void)
{
  HAL_GPIO_WritePin(ENA_RS485_GPIO_Port, ENA_RS485_Pin, RS485_DRIVER_ON);
}

static bool rs485_arm_rx(void)
{
  return HAL_UART_Receive_IT(&huart1, &s_rx_byte, 1u) == HAL_OK;
}

void drv_rs485_init(void)
{
  app_board_rs485_release();
  s_phase = (uint8_t)RS485_IDLE;
}

bool drv_rs485_send(const uint8_t *req, uint8_t len, uint32_t now_ms)
{
  if ((s_phase != (uint8_t)RS485_IDLE) || (len == 0u) || (len > sizeof(s_tx)))
  {
    return false;
  }

  (void)memcpy(s_tx, req, len);

  s_rx_len     = 0u;
  s_overflow   = false;
  s_uart_error = false;
  s_error_code = 0u;
  s_sent_ms    = now_ms;
  s_tc_ms      = now_ms;
  s_first_ms   = now_ms;
  s_last_ms    = now_ms;

  /* Anything that reached the receiver between two transactions - a stray
     byte, line noise while the sensor was unplugged - is thrown away here.
     Left in place, it would set ORE and the next request would be booked as
     a UART error that never happened during it. */
  __HAL_UART_SEND_REQ(&huart1, UART_RXDATA_FLUSH_REQUEST);
  __HAL_UART_CLEAR_FLAG(&huart1, RS485_RX_FLAGS_CLEAR);

  /* Arm reception before the first byte goes out. */
  if (!rs485_arm_rx())
  {
    (void)drv_rs485_stop();
    return false;
  }

  s_phase = (uint8_t)RS485_TX;
  rs485_drive_bus();   /* driver enable time <= 70 ns, MAX481.PDF p.3: no wait needed */

  if (HAL_UART_Transmit_IT(&huart1, s_tx, len) != HAL_OK)
  {
    (void)drv_rs485_stop();
    return false;
  }
  return true;
}

void drv_rs485_snapshot(drv_rs485_snap_t *out)
{
  const uint32_t primask = __get_PRIMASK();
  __disable_irq();

  out->phase      = s_phase;
  out->len        = s_rx_len;
  out->overflow   = s_overflow;
  out->uart_error = s_uart_error;
  out->error_code = s_error_code;
  out->sent_ms    = s_sent_ms;
  out->tc_ms      = s_tc_ms;
  out->first_ms   = s_first_ms;
  out->last_ms    = s_last_ms;
  for (uint8_t i = 0u; i < s_rx_len; i++)
  {
    out->rx[i] = s_rx[i];
  }

  __set_PRIMASK(primask);
}

bool drv_rs485_stop(void)
{
  /* With interrupts off, the UART interrupt cannot re-arm reception between
     the abort and the state change. HAL_UART_Abort waits for nothing when
     DMA is not in use, so this section is short. */
  const uint32_t primask = __get_PRIMASK();
  __disable_irq();

  app_board_rs485_release();
  const HAL_StatusTypeDef st = HAL_UART_Abort(&huart1);
  s_phase = (uint8_t)RS485_IDLE;

  __set_PRIMASK(primask);
  return st == HAL_OK;
}

bool drv_rs485_set_baud(uint32_t baud)
{
  if (s_phase != (uint8_t)RS485_IDLE)
  {
    return false;
  }

  /* HAL_UART_Init on an initialised handle reconfigures it without calling
     the MSP init again (gState is not RESET). DE is low, so whatever the TX
     pin does while UE is off reaches nobody. */
  huart1.Init.BaudRate = baud;
  return HAL_UART_Init(&huart1) == HAL_OK;
}

uint32_t drv_rs485_baud(void)
{
  return huart1.Init.BaudRate;
}

/* -------------------------------------------------------------------------- */
/* HAL callbacks - interrupt context                                          */
/* -------------------------------------------------------------------------- */

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance != USART1)
  {
    return;
  }

  /* Called from the TC interrupt: the stop bit of the last byte has left the
     wire. Drop DE now - not earlier (cuts the last byte), not later (the
     sensor's answer would collide with our driver). */
  app_board_rs485_release();
  s_tc_ms = HAL_GetTick();
  s_phase = (uint8_t)RS485_RX;
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance != USART1)
  {
    return;
  }

  const uint32_t now = HAL_GetTick();

  if (s_rx_len < MB_RX_BUF_LEN)
  {
    s_rx[s_rx_len] = s_rx_byte;
    s_rx_len++;
  }
  else
  {
    s_overflow = true;
  }
  if (s_rx_len == 1u)
  {
    s_first_ms = now;
  }
  s_last_ms = now;

  if (!rs485_arm_rx())
  {
    s_uart_error = true;
  }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance != USART1)
  {
    return;
  }

  /* Any error ends the transaction - app_th sees the flag on its next pass
     and books MB_ERR_UART. Release the bus right away rather than wait. */
  s_error_code |= huart->ErrorCode;
  s_uart_error = true;
  app_board_rs485_release();
}
