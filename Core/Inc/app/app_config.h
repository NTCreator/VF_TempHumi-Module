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
#define APP_FW_PHASE              "4"      /* bump each phase */
#define APP_FW_VERSION            1u       /* TH_Status byte 0, FwVersion (proposal 5.2) */

/* ---- Memory - STM32F303K8T6 -------------------------------------- */
/* Confirmed 2026-10-05 in the CubeMX MCU database, not taken from the IOC
 * guide: STM32CubeMX/db/mcu/STM32F303K(6-8)Tx.xml lists <Flash> 32 (K6) and
 * 64 (K8), <Ram> 12, <CCMRam> 4. The generated STM32F303K8TX_FLASH.ld agreed:
 * FLASH 64K @ 0x08000000, RAM 12K @ 0x20000000, CCMRAM 4K @ 0x10000000
 * (FLASH cut to 50K by hand in phase 4 - see the flash map below).
 * On the chip itself the flash size register at 0x1FFFF7CC reads 0x0040
 * (docs/VF_TH_Phase1_Test.md, test 1).
 *
 * Flash map (phase 4 corrected the application size - phases 1-3 said 52 KB
 * and forgot the config page):
 *
 *   0x08000000  bootloader   12 KB   (phase 5; docs/VF_TH_IOC_Guide.md A9)
 *   0x08003000  application  50 KB   (links at 0x08000000 until phase 5)
 *   0x0800F800  config        2 KB   the last page - F3 pages are 2 KB
 *                                    (FLASH_PAGE_SIZE 0x800, stm32f3xx_hal_flash_ex.h)
 *
 * The linker script holds the application to 50K (user edit, phase 4), and
 * tools/check_linker.sh fails the build if a CubeMX Generate ever puts it
 * back to 64K: an application that reaches the config page would erase it
 * by being flashed. */
#define APP_MCU_FLASH_KB          64u
#define APP_MCU_SRAM_KB           12u
#define APP_MCU_CCMRAM_KB         4u
#define APP_BOOT_FLASH_KB         12u
#define APP_CFG_PAGE_KB           2u
#define APP_APP_FLASH_KB          (APP_MCU_FLASH_KB - APP_BOOT_FLASH_KB - APP_CFG_PAGE_KB)  /* 50 KB */
#define APP_FLASH_BASE_ADDR       0x08000000u
/* tools/check_linker.sh reads this line: keep it a plain hex literal. */
#define APP_CFG_FLASH_ADDR        0x0800F800u

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
#define APP_TX_PERIOD_X10MS       (APP_TX_PERIOD_MS / 10u)  /* TH_Status TxPeriod, DBC factor 10 ms */

/* Frames of this module inside its block (ID map 2.1): TH_Data at base + 0,
 * TH_Status at base + 2. base + 1 and base + 3 stay empty on purpose, so
 * Status sits at base + 2 for every module type. DLC is always 8 (DBC v1.5,
 * BO_ 1024 / BO_ 1026). */
#define APP_CAN_DATA_OFFSET       0u
#define APP_CAN_STATUS_OFFSET     2u
#define APP_FRAME_DLC             8u

/* The node_id comes from the CRC-checked config in flash (app_cfg_store) and
 * from nowhere else. Phase 3's compile-time constant is gone on purpose. */

/* TH_Status CmdSeq until commands exist (phase 6). */
#define APP_CMD_SEQ_NONE          0u

/* ---- Bench switches - ALL 0 IN ANY BUILD THAT LEAVES THE BENCH ----- */
/* Until phase 5 brings the bootloader and the CAN2 protocol, these are the
 * only way a config gets into flash. Each one that is set shows three ways:
 * a #warning when building app_cfg_store.c, a flag in g_diag.cfg, and a tag
 * in s_fw_id.
 *
 * APP_CFG_BENCH_WRITE  1 = at boot, if the stored config is NOT valid, write
 *                      the bench config (node APP_NODE_ID_BENCH). Default 0:
 *                      a blank page can be tested only once (the vibration
 *                      module lost it to a write switch that defaulted to 1),
 *                      so the first flash of phase 4 IS the blank-page test.
 * APP_CFG_BENCH_ERASE  1 = at boot, erase the config page: back to blank.
 * APP_BENCH_CAN_LOOPBACK 1 = even with a valid config, stay in silent
 *                      loopback with the transceiver in standby. Only ever
 *                      makes the module quieter, never louder. */
#define APP_CFG_BENCH_WRITE       0
#define APP_CFG_BENCH_ERASE       0
#define APP_BENCH_CAN_LOOPBACK    0
#define APP_NODE_ID_BENCH         0x30u    /* label 1 */

#if APP_CFG_BENCH_WRITE && APP_CFG_BENCH_ERASE
#error "BENCH_WRITE and BENCH_ERASE together erase and rewrite the page on every boot"
#endif

#if APP_CFG_BENCH_WRITE
#define APP_FW_TAG_WRITE          " BENCH_WRITE"
#else
#define APP_FW_TAG_WRITE          ""
#endif
#if APP_CFG_BENCH_ERASE
#define APP_FW_TAG_ERASE          " BENCH_ERASE"
#else
#define APP_FW_TAG_ERASE          ""
#endif
#if APP_BENCH_CAN_LOOPBACK
#define APP_FW_TAG_LOOPBACK       " BENCH_LOOPBACK"
#else
#define APP_FW_TAG_LOOPBACK       ""
#endif
#define APP_FW_BENCH_TAGS         APP_FW_TAG_WRITE APP_FW_TAG_ERASE APP_FW_TAG_LOOPBACK

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
/* p.7 "Vd: doc Nhiet do & Do am": one request reads both registers, humidity
 * first. Temperature below 0 C is two's complement (0xFF33 = -20.5 C). One
 * transaction gives two numbers taken at the same moment. */
#define SHT_READ_FIRST_REG        SHT_REG_HUMIDITY
#define SHT_READ_COUNT            2u
#define SHT_REG_ADDRESS           0x07D0u  /* p.6 "Bang thanh ghi": sensor address, R/W */
/* p.9 "De doc gia tri Dia chi": a read of 0x07D0 sent to address 0xFF is
 * answered by every sensor on the line, with its own address. Used by the
 * diagnostic scan only. */
#define SHT_ADDR_ANY              0xFFu
/* p.4 "Giao thuc truyen thong": the sensor can be set to 2400, 4800 or 9600.
 * The scan tries the factory default first. */
#define SHT_BAUD_ALT_1            9600u
#define SHT_BAUD_ALT_2            2400u

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

/* Frame sizes for function 0x03 (read holding registers). */
#define MB_READ_REQ_LEN           8u   /* addr, func, reg hi/lo, count hi/lo, crc lo/hi */
#define MB_READ_MAX_REGS          2u   /* the most this firmware ever asks for */
#define MB_EXCEPTION_LEN          5u   /* addr, func | 0x80, code, crc lo/hi */
#define MB_FRAME_MIN_LEN          MB_EXCEPTION_LEN
/* Receive buffer: larger than any reply this firmware accepts (9 bytes), so a
 * longer burst is seen and counted instead of silently clipped. */
#define MB_RX_BUF_LEN             16u
/* Request is 8 characters = 16.7 ms on the wire. If the transmit-complete
 * interrupt has not come by this time, something is wrong and DE is dropped
 * by force: otherwise this module would hold the RS485 bus itself. */
#define MB_TX_TIMEOUT_MS          50u
/* Silence that ends a frame early: 3.5 characters is 7.29 ms, rounded up to
 * the 1 ms tick. Between two bytes of one frame the line is quiet for at most
 * 1.5 characters (Modbus RTU), so this never splits a good frame. */
#define MB_GAP_MS                 8u
/* First poll one refresh period after boot: the sensor runs from its own
 * 12 V supply and the datasheet gives no start-up time. Polling at t = 0
 * would count a timeout on every boot for no fault at all. */
#define MB_FIRST_POLL_MS          SHT_REFRESH_MS

/* ---- Age of the measurement -------------------------------------- */
/* Proposal 5.1, Flags bit 3 STALE: DataAge above 3 sensor refresh periods. */
#define APP_TH_STALE_MS           6000u

/* Proposal 5.1: DataAge counts 0.1 s from the last valid Modbus reply.
 * 0xFFFF means "never read since boot". A very old reading saturates one
 * below that, so it can never be mistaken for "never read". */
#define APP_TH_AGE_LSB_MS         100u
#define APP_TH_AGE_NEVER          0xFFFFu
#define APP_TH_AGE_MAX            0xFFFEu

/* Proposal 5.1, TH_Data / TH_Status Flags byte. */
#define TH_FLAG_NEVER_READ        0x01u    /* bit 0: no valid read since boot */
#define TH_FLAG_TIMEOUT           0x02u    /* bit 1: last transaction got no reply */
#define TH_FLAG_CRC_ERR           0x04u    /* bit 2: last reply had a bad CRC */
#define TH_FLAG_STALE             0x08u    /* bit 3: read before, but DataAge > APP_TH_STALE_MS */
#define TH_FLAG_RANGE             0x10u    /* bit 4: last valid reading outside the datasheet range */
#define TH_FLAG_BUS_OFF_SEEN      0x20u    /* bit 5: CAN bus-off seen (phase 3+) */

/* LED code 2 "sensor not answering" (phase 2 plan, question 1, option A):
 * a LIVE status, not a latched fault. Shown when nothing valid has been read
 * this long after boot, or when the reading is STALE; it goes away by itself
 * when readings come back. The sensor sits on a plug-in cable and its own
 * 12 V supply, so a late or re-plugged sensor is normal during installation. */
#define APP_TH_SILENT_LED_MS      10000u

/* Diagnostic scan (phase 2 plan, question 2, option A): after this many
 * transactions without a single valid read, once per boot, ask address 0xFF
 * at each baud rate. Report only - never adopt what it finds, never write. */
#define APP_TH_SCAN_AFTER_TRIES   5u
#define APP_TH_SCAN_BAUDS         3u

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

/* Measuring pattern (phase 2 on): one flash every 5 s, 1%, while the reading
 * is fresh. Same numbers as VF_Angle_Module's measuring pattern. */
#define APP_LED_MEAS_ON_MS        50u
#define APP_LED_MEAS_PERIOD_MS    5000u

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

/* DBC CM_ SG_ 1026 TxPeriod: one byte, factor 10 ms, "always 100 x 10 ms =
   1000 ms for type 0x3". */
_Static_assert(APP_TX_PERIOD_X10MS == 100u, "TH_Status TxPeriod must read 100");
_Static_assert(APP_TX_PERIOD_X10MS <= 0xFFu, "TxPeriod is a single byte");
_Static_assert(APP_CAN_BASE(0x30u) + APP_CAN_DATA_OFFSET == 1024u, "DBC BO_ 1024 TH_Data_01");
_Static_assert(APP_CAN_BASE(0x30u) + APP_CAN_STATUS_OFFSET == 1026u, "DBC BO_ 1026 TH_Status_01");

/* The Modbus timing is derived from the baud rate, not typed twice. */
_Static_assert(MB_CHAR_US == (MB_BITS_PER_CHAR * 1000000u) / SHT_BAUD_DEFAULT,
               "MB_CHAR_US no longer matches SHT_BAUD_DEFAULT");
_Static_assert(MB_POLL_PERIOD_MS >= SHT_REFRESH_MS, "polling faster than the sensor refreshes reads old numbers");
_Static_assert(MB_REPLY_TIMEOUT_MS < MB_POLL_PERIOD_MS, "a transaction must end before the next one is due");
_Static_assert(APP_TH_STALE_MS == 3u * SHT_REFRESH_MS, "proposal 5.1: STALE = 3 refresh periods");
_Static_assert(MB_GAP_MS * 1000u >= MB_SILENCE_US, "frame-end silence shorter than 3.5 characters");
_Static_assert(MB_TX_TIMEOUT_MS * 1000u > MB_READ_REQ_LEN * MB_CHAR_US, "TX timeout shorter than the request itself");
_Static_assert(MB_RX_BUF_LEN > 3u + 2u * MB_READ_MAX_REGS + 2u, "RX buffer must exceed the longest accepted reply");
_Static_assert(APP_TH_AGE_MAX < APP_TH_AGE_NEVER, "a saturated age must never read as 'never read'");
_Static_assert(APP_TH_SILENT_LED_MS > MB_FIRST_POLL_MS, "LED code 2 must not show before the first poll");

/* The flash map adds up, and the config page is the last page. */
_Static_assert(APP_BOOT_FLASH_KB + APP_APP_FLASH_KB + APP_CFG_PAGE_KB == APP_MCU_FLASH_KB,
               "flash map: 12 + 50 + 2 = 64 KB");
_Static_assert(APP_CFG_FLASH_ADDR == APP_FLASH_BASE_ADDR + (APP_MCU_FLASH_KB - APP_CFG_PAGE_KB) * 1024u,
               "the config page is the last 2 KB page of 64 KB");
_Static_assert(APP_NODE_ID_OK(APP_NODE_ID_BENCH), "the bench config must be a valid node of type 0x3");

#endif /* APP_APP_CONFIG_H */
