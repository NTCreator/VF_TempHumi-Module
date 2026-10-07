/**
  ******************************************************************************
  * @file    app_diag.h
  * @brief   What the board would say if it had a console.
  *
  * It does not have one: JP2 carries SWD only, and USART1 belongs to the
  * sensor. So the debugger is the console: CubeIDE, Debug, Live Expressions
  * (or halt and expand g_diag), read the numbers. The LED carries the same
  * verdict in coarser form when no debugger is attached.
  *
  * Global and not static on purpose. Every phase from here on reports its
  * measurements through this struct. Same idea as VF_Angle_Module's g_diag.
  ******************************************************************************
  */
#ifndef APP_APP_DIAG_H
#define APP_APP_DIAG_H

#include "app/app_can.h"
#include "app/app_th.h"

#include <stdint.h>

/* Lets a human confirm at a glance that the debugger is looking at the live
   struct and not at stale RAM or a wrong address. Reads as "THD1" in a hex
   view. Fixed across phases - it names the struct, not the build. */
#define APP_DIAG_MAGIC  0x54484431u

typedef struct
{
  uint32_t      magic;
  uint8_t       fw_version;              /* APP_FW_VERSION */
  uint8_t       fw_phase;                /* APP_FW_PHASE as a number */

  /* Phase 2: the sensor transactions - see app_th.h */
  app_th_diag_t th;

  /* Phase 3: CAN1 in silent loopback - see app_can.h */
  app_can_diag_t can;
} app_diag_t;

extern app_diag_t g_diag;

/** @brief Clear g_diag and stamp it. Call once, before the modules that fill it. */
void app_diag_init(void);

#endif /* APP_APP_DIAG_H */
