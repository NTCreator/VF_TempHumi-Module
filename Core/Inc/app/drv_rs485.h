/**
  ******************************************************************************
  * @file    drv_rs485.h
  * @brief   Half-duplex RS485 on USART1 + MAX485, DE driven by software on PB6.
  *
  * Moves bytes, nothing more: it does not know what a Modbus frame is. It
  * transmits one request, drops DE at the exact moment the last stop bit has
  * left the wire, and collects whatever comes back with a tick per byte.
  * Deciding when a reply is complete, or late, is app_th's job.
  *
  * The DE rule (docs/VF_TH_IOC_Guide.md A8): HAL_UART_Transmit() returns
  * while the last byte is still in the shift register, and dropping DE then
  * cuts it off. Here DE drops in HAL_UART_TxCpltCallback, which HAL calls from
  * the TC (transmission complete) interrupt - after the stop bit, with no
  * polling loop anywhere.
  *
  * This file owns HAL_UART_TxCpltCallback, HAL_UART_RxCpltCallback and
  * HAL_UART_ErrorCallback. USART1 is the only UART on the board.
  ******************************************************************************
  */
#ifndef APP_DRV_RS485_H
#define APP_DRV_RS485_H

#include "app/app_config.h"

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
  RS485_IDLE = 0,   /* nothing in flight, DE low */
  RS485_TX   = 1,   /* request going out, DE high */
  RS485_RX   = 2    /* request sent, DE low, collecting the reply */
} drv_rs485_phase_t;

/* A consistent copy of the driver state, taken with interrupts briefly off. */
typedef struct
{
  uint8_t  phase;                 /* drv_rs485_phase_t */
  uint8_t  len;                   /* bytes received since the request */
  bool     overflow;              /* more bytes came than MB_RX_BUF_LEN */
  bool     uart_error;            /* PE / FE / NE / ORE, or RX could not be re-armed */
  uint32_t error_code;            /* HAL ErrorCode bits seen in this transaction */
  uint32_t sent_ms;               /* tick when the request was started */
  uint32_t tc_ms;                 /* tick of transmit complete = DE dropped */
  uint32_t first_ms;              /* tick of the first received byte */
  uint32_t last_ms;               /* tick of the latest received byte */
  uint8_t  rx[MB_RX_BUF_LEN];
} drv_rs485_snap_t;

/** @brief Put the driver in IDLE with the bus released. Call once at start-up. */
void drv_rs485_init(void);

/**
  * @brief Start one request: flush RX, arm reception, raise DE, transmit.
  *
  * Reception is armed BEFORE transmitting: the sensor may answer as soon as
  * the last stop bit is out. The request is copied, so the caller's buffer
  * may go away.
  *
  * @return false if the driver is busy or HAL refused; the bus is released
  *         and the caller counts it (MB_ERR_TX).
  */
bool drv_rs485_send(const uint8_t *req, uint8_t len, uint32_t now_ms);

/** @brief Copy the driver state. Safe against the UART interrupt. */
void drv_rs485_snapshot(drv_rs485_snap_t *out);

/**
  * @brief End the transaction: DE low, TX and RX aborted, back to IDLE.
  *        Idempotent; safe whatever state the driver is in.
  * @return false if HAL_UART_Abort reported a failure (counted by the caller).
  */
bool drv_rs485_stop(void);

/**
  * @brief Change the baud rate. Only in IDLE. Used by the diagnostic scan.
  * @return false if busy or HAL_UART_Init failed.
  */
bool drv_rs485_set_baud(uint32_t baud);

/** @brief Baud rate USART1 is currently set to. */
uint32_t drv_rs485_baud(void);

#endif /* APP_DRV_RS485_H */
