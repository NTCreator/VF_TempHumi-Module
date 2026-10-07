/**
  ******************************************************************************
  * @file    app_frame.h
  * @brief   TH_Data and TH_Status as bytes: IDs, the node_id gate, packing.
  *
  * Pure - no HAL - so the layout is checked on a PC twice over: against bytes
  * written by hand from the DBC (tests/test_app_frame.c), and against the DBC
  * file itself, read from contracts/ (tests/test_dbc_layout.c).
  *
  * Contract: contracts/VF_E-Drive_Data_Logger_v1.5.dbc, BO_ 1024 TH_Data_01
  * and BO_ 1026 TH_Status_01 (repeated for node 0x31..0x39). Every signal is
  * Intel byte order (@1), DLC 8. Signal names here follow the DBC, which is
  * what an analyst reads - not proposal 5.2, which calls StatusFlags "Flags"
  * and TxPeriod "TxPeriod_x10ms" (same bytes, same raw values).
  ******************************************************************************
  */
#ifndef APP_APP_FRAME_H
#define APP_APP_FRAME_H

#include "app/app_config.h"

#include <stdbool.h>
#include <stdint.h>

/* TH_Data, base + 0. Units are the wire units: no conversion anywhere. */
typedef struct
{
  int16_t  t_x10;        /* Temperature  0|16 @1-  x0.1 degC  - sensor register as is */
  uint16_t rh_x10;       /* Humidity    16|16 @1+  x0.1 %RH   - sensor register as is */
  uint16_t age_x100ms;   /* DataAge     32|16 @1+  x0.1 s     - 0xFFFF = never read */
  uint8_t  counter;      /* Counter     48|8                  - one per 1 s slot */
  uint8_t  flags;        /* Flags       56|8                  - TH_FLAG_* */
} app_frame_data_t;

/* TH_Status, base + 2. */
typedef struct
{
  uint8_t  fw_version;       /* FwVersion    0|8 */
  uint8_t  status_flags;     /* StatusFlags  8|8   - same bits as TH_Data Flags */
  uint8_t  node_id;          /* NodeId      16|8   - repeats the ID on purpose */
  uint8_t  tx_period_x10ms;  /* TxPeriod    24|8   x10 ms */
  uint16_t modbus_ok;        /* ModbusOk    32|16  - wraps */
  uint8_t  modbus_err;       /* ModbusErr   48|8   - saturates at 255 */
  uint8_t  cmd_seq;          /* CmdSeq      56|8 */
} app_frame_status_t;

/**
  * @brief The node_id gate. Nothing reaches bxCAN with a node_id that fails
  *        it. Same rule as APP_NODE_ID_OK - which stays its only definition.
  */
bool app_frame_node_ok(uint8_t node_id);

/** @brief TH_Data ID: 0x100 + node_id * 0x10 + 0 (ID map 2.1). */
uint32_t app_frame_id_data(uint8_t node_id);

/** @brief TH_Status ID: 0x100 + node_id * 0x10 + 2 (ID map 2.1). */
uint32_t app_frame_id_status(uint8_t node_id);

void app_frame_pack_data(const app_frame_data_t *d, uint8_t out[APP_FRAME_DLC]);
void app_frame_pack_status(const app_frame_status_t *s, uint8_t out[APP_FRAME_DLC]);

/* The inverse, for tests and for decoding what came back in loopback. */
void app_frame_unpack_data(const uint8_t in[APP_FRAME_DLC], app_frame_data_t *d);
void app_frame_unpack_status(const uint8_t in[APP_FRAME_DLC], app_frame_status_t *s);

#endif /* APP_APP_FRAME_H */
