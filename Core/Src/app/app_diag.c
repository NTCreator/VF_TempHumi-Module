/**
  ******************************************************************************
  * @file    app_diag.c
  ******************************************************************************
  */

#include "app/app_diag.h"

#include "app/app_config.h"

#include <string.h>

/* APP_FW_PHASE is a string literal ("2"); its first character as a digit. */
#define DIAG_FW_PHASE_NUM   ((uint8_t)(APP_FW_PHASE[0] - '0'))

app_diag_t g_diag;

void app_diag_init(void)
{
  (void)memset(&g_diag, 0, sizeof(g_diag));
  g_diag.magic = APP_DIAG_MAGIC;
  g_diag.fw_version = (uint8_t)APP_FW_VERSION;
  g_diag.fw_phase = DIAG_FW_PHASE_NUM;
}
