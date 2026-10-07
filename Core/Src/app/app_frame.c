/**
  ******************************************************************************
  * @file    app_frame.c
  * @brief   TH_Data / TH_Status byte layout. Pure.
  ******************************************************************************
  */

#include "app/app_frame.h"

/* Byte positions = DBC start bit / 8. Intel order: low byte first. */
#define DATA_POS_TEMPERATURE    0u   /* SG_ Temperature : 0|16@1- */
#define DATA_POS_HUMIDITY       2u   /* SG_ Humidity : 16|16@1+ */
#define DATA_POS_AGE            4u   /* SG_ DataAge : 32|16@1+ */
#define DATA_POS_COUNTER        6u   /* SG_ Counter : 48|8@1+ */
#define DATA_POS_FLAGS          7u   /* SG_ Flags : 56|8@1+ */

#define STATUS_POS_FW           0u   /* SG_ FwVersion : 0|8@1+ */
#define STATUS_POS_FLAGS        1u   /* SG_ StatusFlags : 8|8@1+ */
#define STATUS_POS_NODE         2u   /* SG_ NodeId : 16|8@1+ */
#define STATUS_POS_PERIOD       3u   /* SG_ TxPeriod : 24|8@1+ (10,0) */
#define STATUS_POS_OK           4u   /* SG_ ModbusOk : 32|16@1+ */
#define STATUS_POS_ERR          6u   /* SG_ ModbusErr : 48|8@1+ */
#define STATUS_POS_SEQ          7u   /* SG_ CmdSeq : 56|8@1+ */

static void put_u16le(uint8_t *p, uint16_t v)
{
  p[0] = (uint8_t)(v & 0xFFu);
  p[1] = (uint8_t)(v >> 8);
}

static uint16_t get_u16le(const uint8_t *p)
{
  return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

bool app_frame_node_ok(uint8_t node_id)
{
  return APP_NODE_ID_OK(node_id);
}

uint32_t app_frame_id_data(uint8_t node_id)
{
  return (uint32_t)APP_CAN_BASE(node_id) + APP_CAN_DATA_OFFSET;
}

uint32_t app_frame_id_status(uint8_t node_id)
{
  return (uint32_t)APP_CAN_BASE(node_id) + APP_CAN_STATUS_OFFSET;
}

void app_frame_pack_data(const app_frame_data_t *d, uint8_t out[APP_FRAME_DLC])
{
  /* Temperature: the int16 goes out as its two's complement bit pattern,
     which is exactly what @1- means (datasheet p.7: -20.5 C = 0xFF33). */
  put_u16le(&out[DATA_POS_TEMPERATURE], (uint16_t)d->t_x10);
  put_u16le(&out[DATA_POS_HUMIDITY], d->rh_x10);
  put_u16le(&out[DATA_POS_AGE], d->age_x100ms);
  out[DATA_POS_COUNTER] = d->counter;
  out[DATA_POS_FLAGS] = d->flags;
}

void app_frame_pack_status(const app_frame_status_t *s, uint8_t out[APP_FRAME_DLC])
{
  out[STATUS_POS_FW] = s->fw_version;
  out[STATUS_POS_FLAGS] = s->status_flags;
  out[STATUS_POS_NODE] = s->node_id;
  out[STATUS_POS_PERIOD] = s->tx_period_x10ms;
  put_u16le(&out[STATUS_POS_OK], s->modbus_ok);
  out[STATUS_POS_ERR] = s->modbus_err;
  out[STATUS_POS_SEQ] = s->cmd_seq;
}

void app_frame_unpack_data(const uint8_t in[APP_FRAME_DLC], app_frame_data_t *d)
{
  d->t_x10 = (int16_t)get_u16le(&in[DATA_POS_TEMPERATURE]);
  d->rh_x10 = get_u16le(&in[DATA_POS_HUMIDITY]);
  d->age_x100ms = get_u16le(&in[DATA_POS_AGE]);
  d->counter = in[DATA_POS_COUNTER];
  d->flags = in[DATA_POS_FLAGS];
}

void app_frame_unpack_status(const uint8_t in[APP_FRAME_DLC], app_frame_status_t *s)
{
  s->fw_version = in[STATUS_POS_FW];
  s->status_flags = in[STATUS_POS_FLAGS];
  s->node_id = in[STATUS_POS_NODE];
  s->tx_period_x10ms = in[STATUS_POS_PERIOD];
  s->modbus_ok = get_u16le(&in[STATUS_POS_OK]);
  s->modbus_err = in[STATUS_POS_ERR];
  s->cmd_seq = in[STATUS_POS_SEQ];
}
