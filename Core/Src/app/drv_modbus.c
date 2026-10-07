/**
  ******************************************************************************
  * @file    drv_modbus.c
  * @brief   Modbus RTU framing. Pure: no HAL, no state.
  ******************************************************************************
  */

#include "app/drv_modbus.h"

#include <stdbool.h>

#define MB_FUNC_READ_HOLDING    SHT_FUNC_READ   /* 0x03, datasheet p.5 */
#define MB_CRC_INIT             0xFFFFu
#define MB_CRC_POLY_REFLECTED   0xA001u
#define MB_CRC_LEN              2u
#define MB_READ_HDR_LEN         3u              /* addr, func, byte count */
#define MB_BYTES_PER_REG        2u

/* Byte positions in a reply. */
#define MB_POS_ADDR             0u
#define MB_POS_FUNC             1u
#define MB_POS_COUNT            2u              /* byte count, or the exception code */
#define MB_POS_DATA             3u

uint16_t drv_modbus_crc16(const uint8_t *p, uint16_t n)
{
  uint16_t crc = MB_CRC_INIT;

  for (uint16_t i = 0u; i < n; i++)
  {
    crc = (uint16_t)(crc ^ p[i]);
    for (uint8_t bit = 0u; bit < 8u; bit++)
    {
      crc = ((crc & 1u) != 0u) ? (uint16_t)((crc >> 1) ^ MB_CRC_POLY_REFLECTED)
                               : (uint16_t)(crc >> 1);
    }
  }
  return crc;
}

static void mb_put_crc(uint8_t *frame, uint16_t len_without_crc)
{
  const uint16_t crc = drv_modbus_crc16(frame, len_without_crc);
  frame[len_without_crc]      = (uint8_t)(crc & 0xFFu);   /* low byte first */
  frame[len_without_crc + 1u] = (uint8_t)(crc >> 8);
}

static bool mb_crc_ok(const uint8_t *frame, uint16_t len_with_crc)
{
  const uint16_t body = (uint16_t)(len_with_crc - MB_CRC_LEN);
  const uint16_t crc  = drv_modbus_crc16(frame, body);
  return (frame[body] == (uint8_t)(crc & 0xFFu)) && (frame[body + 1u] == (uint8_t)(crc >> 8));
}

uint8_t drv_modbus_build_read(const mb_read_req_t *req, uint8_t out[MB_READ_REQ_LEN])
{
  if ((req->count == 0u) || (req->count > MB_READ_MAX_REGS))
  {
    return 0u;
  }

  out[0] = req->addr;
  out[1] = MB_FUNC_READ_HOLDING;
  out[2] = (uint8_t)(req->reg >> 8);       /* register address, big-endian */
  out[3] = (uint8_t)(req->reg & 0xFFu);
  out[4] = (uint8_t)(req->count >> 8);
  out[5] = (uint8_t)(req->count & 0xFFu);
  mb_put_crc(out, MB_READ_REQ_LEN - MB_CRC_LEN);
  return MB_READ_REQ_LEN;
}

uint16_t drv_modbus_expected_len(const uint8_t *p, uint8_t n)
{
  if (n <= MB_POS_FUNC)
  {
    return 0u;
  }
  if ((p[MB_POS_FUNC] & MB_FUNC_EXCEPTION_BIT) != 0u)
  {
    return MB_EXCEPTION_LEN;
  }
  if (p[MB_POS_FUNC] != MB_FUNC_READ_HOLDING)
  {
    return 0u;   /* unknown function: cannot size it, it ends on silence */
  }
  if (n <= MB_POS_COUNT)
  {
    return 0u;
  }
  return (uint16_t)(MB_READ_HDR_LEN + p[MB_POS_COUNT] + MB_CRC_LEN);
}

mb_status_t drv_modbus_parse_read(const uint8_t *p, uint8_t n,
                                  const mb_read_req_t *req, mb_reply_t *out)
{
  if (n < MB_FRAME_MIN_LEN)
  {
    return MB_ERR_SHORT;
  }

  /* Length first: the CRC sits in the last two bytes of the frame, so until
     the length is right there is no CRC to check. An unknown function code
     cannot be sized; take what arrived, and let the CRC decide whether it was
     a frame at all. */
  uint16_t len = drv_modbus_expected_len(p, n);
  if (len == 0u)
  {
    len = n;
  }
  if (n < len)
  {
    return MB_ERR_SHORT;
  }
  if (n > len)
  {
    return MB_ERR_LEN;
  }

  if (!mb_crc_ok(p, len))
  {
    return MB_ERR_CRC;
  }

  const uint8_t from = p[MB_POS_ADDR];
  if ((req->addr != MB_ADDR_ANY) && (from != req->addr))
  {
    return MB_ERR_ADDR;
  }
  out->addr = from;

  if ((p[MB_POS_FUNC] & MB_FUNC_EXCEPTION_BIT) != 0u)
  {
    out->exception = p[MB_POS_COUNT];
    return ((p[MB_POS_FUNC] & (uint8_t)~MB_FUNC_EXCEPTION_BIT) == MB_FUNC_READ_HOLDING)
           ? MB_ERR_EXCEPTION : MB_ERR_FUNC;
  }
  if (p[MB_POS_FUNC] != MB_FUNC_READ_HOLDING)
  {
    return MB_ERR_FUNC;
  }

  if ((req->count == 0u) || (req->count > MB_READ_MAX_REGS)
      || (p[MB_POS_COUNT] != (uint8_t)(req->count * MB_BYTES_PER_REG)))
  {
    return MB_ERR_LEN;
  }

  for (uint16_t i = 0u; i < req->count; i++)
  {
    const uint16_t at = (uint16_t)(MB_POS_DATA + i * MB_BYTES_PER_REG);
    out->regs[i] = (uint16_t)(((uint16_t)p[at] << 8) | p[at + 1u]);   /* big-endian */
  }
  return MB_OK;
}
