/**
  ******************************************************************************
  * @file    app_config.h
  * @brief   Every tunable and every hardware constant of the application, in
  *          one place (CLAUDE.md 6.2, 6.6).
  *
  * Constants taken from a document name it. "SHT datasheet" is
  * docs/[Hshop.vn] SHT RS485 Modbus-RTU Temperature Humidity Sensor.pdf, cited
  * by page and section heading (it has no section numbers). Contract files live
  * in contracts/ and are read-only copies (CLAUDE.md 2.1). "Proposal" is
  * contracts/VF_TH_CAN_Proposal.md, approved 2026-10-05, option A.
  ******************************************************************************
  */
#ifndef APP_APP_CONFIG_H
#define APP_APP_CONFIG_H

#include <stdint.h>

/* ---- Identity ---------------------------------------------------- */
/* There is no console on this board: JP2 carries SWD only, and USART1 is
   wired to the sensor, not out of the board. "Printed at boot" therefore means
   embedded in the image, where a debugger or `strings` on the .elf finds it -
   see s_fw_id in main.c. */
#define APP_FW_PHASE              "1"      /* bump each phase */
#define APP_FW_VERSION            1u       /* TH_Status byte 0, FwVersion (proposal 5.2) */

/* ---- Memory - STM32F303K8T6 -------------------------------------- */
/* Confirmed 2026-10-05 in the CubeMX MCU database, not taken from the IOC
 * guide: STM32CubeMX/db/mcu/STM32F303K(6-8)Tx.xml lists <Flash> 32 (K6) and
 * 64 (K8), <Ram> 12, <CCMRam> 4. The generated STM32F303K8TX_FLASH.ld agrees:
 * FLASH 64K @ 0x08000000, RAM 12K @ 0x20000000, CCMRAM 4K @ 0x10000000.
 * On the chip itself the flash size register at 0x1FFFF7CC reads 0x0040
 * (docs/VF_TH_Phase1_Test.md, test 1).
 *
 * Phase 5 splits flash as docs/VF_TH_IOC_Guide.md A9 decided: bootloader
 * 12 KB at 0x08000000, application at 0x08003000. Until then the application
 * links at 0x08000000 and must stay inside APP_APP_FLASH_KB so that moving it
 * later is a linker change, not a diet. */
#define APP_MCU_FLASH_KB          64u
#define APP_MCU_SRAM_KB           12u
#define APP_MCU_CCMRAM_KB         4u
#define APP_BOOT_FLASH_KB         12u
#define APP_APP_FLASH_KB          (APP_MCU_FLASH_KB - APP_BOOT_FLASH_KB)   /* 52 KB */

/* ---- Module type and node_id ------------------------------------- */
/* Contract: VF_Main_CAN_ID_Map.md v1.3 section 2.1, proposal section 2.
 * The high nibble of node_id IS the module type. Temperature-humidity is
 * type 0x3. APP_NODE_ID_OK() is the only place in the firmware that knows this
 * rule. Never re-derive the check anywhere else. */
#define APP_MODULE_TYPE           0x03u
#define APP_NODE_LABEL_MAX        10u      /* labels 1..10 -> low nibble 0..9 */
#define APP_NODE_ID_MIN           0x30u
#define APP_NODE_ID_MAX           0x39u
#define APP_NODE_ID_UNSET         0xFFu    /* ID map section 2: unset = silent */

#define APP_NODE_ID_OK(id) \
  ((((uint8_t)(id) >> 4) == APP_MODULE_TYPE) \
   && (((uint8_t)(id) & 0x0Fu) < APP_NODE_LABEL_MAX))

#define APP_NODE_ID_FOR_LABEL(label) \
  ((uint8_t)((APP_MODULE_TYPE << 4) | ((uint8_t)(label) - 1u)))

/* ---- CAN1 -------------------------------------------------------- */
/* Proposal section 2: base = 0x100 + node_id * 0x10, cmd = 0x010 + node_id.
 * NOTE: the heading of proposal section 5.3 reads "TH_Cmd - 0x040 + node_id".
 * That heading is a typo - section 2, the ID map table and the DBC
 * (BO_ 64 TH_Cmd_01) all give 0x010 + node_id = 0x040 for node 0x30. The
 * assertions at the end of this file pin the formula to that table. */
#define APP_CAN_BITRATE           500000u
#define APP_CAN_BASE(node_id)     ((uint16_t)(0x100u + (node_id) * 0x10u))
#define APP_CAN_CMD_ID(node_id)   ((uint16_t)(0x010u + (node_id)))

/* Transmit period: FIXED for type 0x3 - proposal section 4, option A, and
 * CAN2 protocol v1.5 section 4.2 (SET_TX_PERIOD answers BAD_PARAM to every
 * value). The sensor refreshes every 2 s, so faster would only repeat numbers. */
#define APP_TX_PERIOD_MS          1000u
#define APP_STATUS_PERIOD_MS      1000u
#define APP_TX_PERIOD_X10MS       (APP_TX_PERIOD_MS / 10u)  /* TH_Status TxPeriod_x10ms */

/* ---- SHT RS485 sensor - SHT datasheet ---------------------------- */
#define SHT_SLAVE_ADDR_DEFAULT    0x01u    /* p.6 "Note": factory address 1 */
#define SHT_BAUD_DEFAULT          4800u    /* p.6 "Note": factory baud 4800 */
#define SHT_FUNC_READ             0x03u    /* p.5 "Cau truc khung truyen du lieu": 0x03 Read */
#define SHT_REG_HUMIDITY          0x0000u  /* p.6 "Bang thanh ghi": RH x10, read only */
#define SHT_REG_TEMPERATURE       0x0001u  /* p.6 "Bang thanh ghi": T x10, read only */
#define SHT_REFRESH_MS            2000u    /* p.1 "Thoi gian lam moi": 2 s */
#define SHT_TEMP_MIN_X10          (-400)   /* p.1 "Moi truong lam viec": -40 C */
#define SHT_TEMP_MAX_X10          600      /* p.1 "Moi truong lam viec": +60 C */
#define SHT_RH_MIN_X10            0        /* p.1 "Moi truong lam viec": 0 %RH */
#define SHT_RH_MAX_X10            1000     /* p.1 "Moi truong lam viec": 100 %RH */

/* ---- Modbus RTU framing at 4800 8N1 ------------------------------ */
/* p.4 "Giao thuc truyen thong": 8 data bits, 1 stop bit, no parity. With the
 * start bit that is 10 bits per character: 10 / 4800 s = 2083 us. Modbus RTU
 * needs 3.5 characters of silence between frames: 7.29 ms. */
#define MB_BITS_PER_CHAR          10u
#define MB_CHAR_US                2083u
#define MB_SILENCE_US             ((MB_CHAR_US * 7u) / 2u)  /* 3.5 characters */
/* The datasheet gives no response time. 200 ms is this firmware's choice:
 * the request (8 characters) and reply (9 characters) together take 35 ms on
 * the wire, so this leaves the sensor over 160 ms to think, and still ends the
 * transaction far inside one poll period. Phase 2 measures the real figure. */
#define MB_REPLY_TIMEOUT_MS       200u
#define MB_POLL_PERIOD_MS         SHT_REFRESH_MS            /* faster is pointless */

/* ---- Age of the measurement -------------------------------------- */
/* Proposal 5.1, Flags bit 3 STALE: DataAge above 3 sensor refresh periods. */
#define APP_TH_STALE_MS           6000u

/* ---- LED (L2 on PB0) --------------------------------------------- */
/* L2 (red 0805) hangs from +3V3 through R5 = 330 ohm into PB0 (netlist): the
 * pin sinks, so LOW lights it, at roughly 4 mA. Blink at low duty and never
 * leave it solid on (CLAUDE.md 6.5). app_led.c checks every pattern below
 * against this ceiling at compile time. */
#define APP_LED_DUTY_PCT_MAX      10u

/* Boot pattern: an even 2 Hz flash, "firmware running, not measuring yet".
 * In phase 1 it is the only thing the LED ever shows. 50 / 500 = exactly 10%.
 * Same pattern and same numbers as VF_Angle_Module, so a technician reads
 * every module type the same way. */
#define APP_LED_BOOT_ON_MS        50u
#define APP_LED_BOOT_PERIOD_MS    500u

/* Fault code N: N flashes APP_LED_CODE_ON_MS long, APP_LED_CODE_OFF_MS apart,
 * then APP_LED_CODE_GAP_MS dark before the group repeats. One period is
 * N*ON + (N-1)*OFF + GAP - the gap replaces the last OFF, it does not follow
 * it. With these numbers the longest code (8) runs at 400 / 4150 = 9.6%. */
#define APP_LED_CODE_ON_MS        50u
#define APP_LED_CODE_OFF_MS       250u
#define APP_LED_CODE_GAP_MS       2000u
#define APP_LED_CODE_MAX          8u

/* ---- Compile-time cross-checks ------------------------------------ */
/* Proposal section 2 and ID map 2.1 list every node of this type. These pin
   the macros above to that table, so a slip in a macro fails the build rather
   than putting a module on somebody else's IDs. */
_Static_assert(APP_NODE_ID_MIN == (APP_MODULE_TYPE << 4), "type 0x3 owns node_id 0x30..");
_Static_assert(APP_NODE_ID_FOR_LABEL(1u) == APP_NODE_ID_MIN, "label 1 must be node 0x30");
_Static_assert(APP_NODE_ID_FOR_LABEL(APP_NODE_LABEL_MAX) == APP_NODE_ID_MAX, "label 10 must be node 0x39");
_Static_assert(APP_NODE_ID_OK(APP_NODE_ID_MIN) && APP_NODE_ID_OK(APP_NODE_ID_MAX), "0x30 and 0x39 must be accepted");
_Static_assert(!APP_NODE_ID_OK(0x2Fu) && !APP_NODE_ID_OK(0x3Au), "neighbours of the range must be refused");
_Static_assert(!APP_NODE_ID_OK(0x29u) && !APP_NODE_ID_OK(0x40u), "angle and type-4 nodes must be refused");
_Static_assert(!APP_NODE_ID_OK(APP_NODE_ID_UNSET), "an unset node_id must never pass");
_Static_assert(APP_CAN_BASE(0x30u) == 0x400u && APP_CAN_BASE(0x39u) == 0x490u, "proposal section 2: base 0x400..0x490");
_Static_assert(APP_CAN_CMD_ID(0x30u) == 0x040u && APP_CAN_CMD_ID(0x39u) == 0x049u, "proposal section 2: command 0x040..0x049");

/* Proposal 5.2: TxPeriod_x10ms is one byte and reads 100 for 1000 ms. */
_Static_assert(APP_TX_PERIOD_X10MS == 100u, "TH_Status TxPeriod_x10ms must read 100");
_Static_assert(APP_TX_PERIOD_X10MS <= 0xFFu, "TxPeriod_x10ms is a single byte");

/* The Modbus timing is derived from the baud rate, not typed twice. */
_Static_assert(MB_CHAR_US == (MB_BITS_PER_CHAR * 1000000u) / SHT_BAUD_DEFAULT,
               "MB_CHAR_US no longer matches SHT_BAUD_DEFAULT");
_Static_assert(MB_POLL_PERIOD_MS >= SHT_REFRESH_MS, "polling faster than the sensor refreshes reads old numbers");
_Static_assert(MB_REPLY_TIMEOUT_MS < MB_POLL_PERIOD_MS, "a transaction must end before the next one is due");
_Static_assert(APP_TH_STALE_MS == 3u * SHT_REFRESH_MS, "proposal 5.1: STALE = 3 refresh periods");

/* The application must already fit the region it will move to in phase 5. */
_Static_assert(APP_APP_FLASH_KB == 52u, "IOC guide A9: 64 KB - 12 KB bootloader");

#endif /* APP_APP_CONFIG_H */
