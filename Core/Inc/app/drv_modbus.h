/**
  ******************************************************************************
  * @file    drv_modbus.h
  * @brief   Modbus RTU framing for the SHT sensor: CRC, request, reply.
  *
  * Pure functions - no HAL, no state, no clock - so every rule in here runs
  * on a PC against the example frames printed in the sensor datasheet
  * (tests/test_drv_modbus.c). The transport is drv_rs485; the scheduler and
  * the bookkeeping are app_th and app_meas.
  *
  * Only function 0x03 (read holding registers) exists here. This firmware
  * never writes to the sensor: a wrong write to 0x07D0 / 0x07D1 changes its
  * address or baud rate, and from then on nothing on this board can find it.
  ******************************************************************************
  */
#ifndef APP_DRV_MODBUS_H
#define APP_DRV_MODBUS_H

#include "app/app_config.h"

#include <stdint.h>

/* Every way a transaction can end. app_meas keeps one counter per value, and
   every path out of a transaction ends in exactly one of them (CLAUDE.md 6.3).
   Values are stable: g_diag shows them as numbers. Append only. */
typedef enum
{
  MB_OK            = 0,  /* right address, right function, right length, good CRC */
  MB_ERR_TIMEOUT   = 1,  /* not a single byte within MB_REPLY_TIMEOUT_MS (transport) */
  MB_ERR_SHORT     = 2,  /* bytes came, then silence before the frame was complete */
  MB_ERR_CRC       = 3,  /* complete frame, CRC wrong */
  MB_ERR_ADDR      = 4,  /* good CRC, answered by another address */
  MB_ERR_FUNC      = 5,  /* good CRC, a function code that was not asked */
  MB_ERR_LEN       = 6,  /* good CRC, wrong byte count - or more bytes than a frame */
  MB_ERR_EXCEPTION = 7,  /* the sensor answered with a Modbus exception */
  MB_ERR_UART      = 8,  /* parity / framing / noise / overrun (transport) */
  MB_ERR_TX        = 9,  /* request never left: HAL refused, or TC never came (transport) */
  MB_STATUS_COUNT  = 10
} mb_status_t;

#define MB_FUNC_EXCEPTION_BIT   0x80u
#define MB_ADDR_ANY             SHT_ADDR_ANY   /* datasheet p.9: any sensor answers */

/* What was asked - the parser checks the reply against it. */
typedef struct
{
  uint8_t  addr;     /* MB_ADDR_ANY accepts a reply from any address */
  uint16_t reg;
  uint16_t count;    /* 1..MB_READ_MAX_REGS */
} mb_read_req_t;

/* What came back. Only meaningful fields are written. */
typedef struct
{
  uint8_t  addr;                      /* who answered (on MB_OK and MB_ERR_EXCEPTION) */
  uint8_t  exception;                 /* exception code, on MB_ERR_EXCEPTION */
  uint16_t regs[MB_READ_MAX_REGS];    /* big-endian on the wire, host order here */
} mb_reply_t;

/** @brief CRC16/MODBUS: poly 0xA001 reflected, init 0xFFFF. Sent low byte first. */
uint16_t drv_modbus_crc16(const uint8_t *p, uint16_t n);

/**
  * @brief Build a function 0x03 request into out[MB_READ_REQ_LEN].
  * @return number of bytes written (MB_READ_REQ_LEN), or 0 if count is out of
  *         range - the caller counts that as MB_ERR_TX.
  */
uint8_t drv_modbus_build_read(const mb_read_req_t *req, uint8_t out[MB_READ_REQ_LEN]);

/**
  * @brief How long the frame that starts with p[0..n-1] will be.
  *
  * Known after byte 2 for an exception (5 bytes), after byte 3 for a 0x03
  * reply (3 + byte count + 2). 0 = not known yet, or a function code this
  * parser does not size - such a frame ends on silence instead.
  */
uint16_t drv_modbus_expected_len(const uint8_t *p, uint8_t n);

/**
  * @brief Check a complete reply against the request and extract it.
  *
  * Checks run in this order, so every frame gets the most specific verdict:
  * length (SHORT / LEN), CRC, address, exception, function, byte count.
  * A frame that fails the CRC is never looked at further - its address byte
  * may be the corrupted one.
  */
mb_status_t drv_modbus_parse_read(const uint8_t *p, uint8_t n,
                                  const mb_read_req_t *req, mb_reply_t *out);

#endif /* APP_DRV_MODBUS_H */
