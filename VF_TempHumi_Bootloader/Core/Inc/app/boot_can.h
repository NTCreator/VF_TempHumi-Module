/**
  ******************************************************************************
  * @file    boot_can.h
  * @brief   bxCAN for the service protocol: one filter, polled, no interrupts.
  *
  * No CAN interrupt is enabled in the bootloader .ioc (docs/VF_TH_IOC_Guide.md
  * B2): every interrupt switched on here would be one more thing to undo
  * before the jump to the application.
  ******************************************************************************
  */
#ifndef BOOT_CAN_H
#define BOOT_CAN_H

#include <stdbool.h>
#include <stdint.h>

/* Which step of boot_can_start() failed - for the debugger, s_can_fail_step in
   main.c. Same numbering as the application's CAN fault detail. */
#define BOOT_CAN_OK            0u
#define BOOT_CAN_FAIL_FILTER   2u
#define BOOT_CAN_FAIL_START    3u

/**
  * @brief Filter on exactly 0x7E0 (standard data frame), transceiver Normal,
  *        then start - in that order (phase 5 plan 2.6).
  * @return BOOT_CAN_OK, or the step that failed. On failure the transceiver
  *         is back in standby.
  */
uint8_t boot_can_start(void);

/* boot_can_recv() results. */
#define BOOT_CAN_RX_NONE       0u   /* FIFO0 empty */
#define BOOT_CAN_RX_FRAME      1u   /* out8 / len hold a request */
#define BOOT_CAN_RX_ERROR      2u   /* a frame was waiting but could not be read */

/**
  * @brief One request, if one is waiting.
  * @param out8 receives the payload
  * @param len  receives its DLC (0..8)
  * @return BOOT_CAN_RX_*
  */
uint8_t boot_can_recv(uint8_t out8[8], uint8_t *len);

/**
  * @brief Send one 0x7E8 response and wait for it to leave, at most
  *        BOOT_SEND_TIMEOUT_MS; then it is aborted rather than left in a
  *        mailbox.
  * @return true if the frame left (was ACKed)
  */
bool boot_can_send(const uint8_t data8[8]);

#endif /* BOOT_CAN_H */
