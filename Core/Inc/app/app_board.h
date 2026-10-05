/**
  ******************************************************************************
  * @file    app_board.h
  * @brief   The few pins this board has, and the rules attached to them.
  *
  * Every other module goes through here instead of calling HAL_GPIO_WritePin
  * with a port and a pin number, so the rules below have exactly one place to
  * be enforced.
  *
  * Two pins on this board can hold a bus, and both default the wrong way:
  *
  *   PB6 ENA_RS485 - R8 and R9 (10k each, so 5k) pull MAX485 DE and /RE up to
  *                   +5 V (CLAUDE.md 4.3). Until firmware drives PB6 low, the
  *                   MAX485 is TRANSMITTING and holds the RS485 bus.
  *   PA10 EN_CAN1  - TJA1044 STB. LOW = Normal, HIGH = Standby. An
  *                   unconfigured module must never reach Normal: a duplicate
  *                   node_id takes the whole CAN1 bus down (CLAUDE.md 1.2).
  *
  * Deliberately missing, for now: a call that puts the transceiver in Normal
  * (phase 4, and it will demand a CRC-checked config as its argument) and a
  * call that raises DE to transmit (phase 2, inside the Modbus driver, paired
  * with the TC wait of docs/VF_TH_IOC_Guide.md A8). A function that does not
  * exist cannot be called by mistake.
  ******************************************************************************
  */
#ifndef APP_APP_BOARD_H
#define APP_APP_BOARD_H

#include <stdbool.h>

/**
  * @brief Release the RS485 bus and keep CAN1 in standby, before anything else.
  *
  * Call it first thing in main(), before HAL_Init(). SystemClock_Config() can
  * wait up to HSE_STARTUP_TIMEOUT (100 ms) for the crystal, and if the crystal
  * is dead it goes straight to Error_Handler() - MX_GPIO_Init() would then
  * never run and the MAX485 would hold the RS485 bus forever.
  *
  * Needs nothing from HAL_Init(): it enables the two GPIO clocks itself and
  * never touches SysTick. Writes the output latch before switching the pin to
  * output, so PB6 goes straight from "pulled to +5 V" to "driven low" with no
  * instant driven high.
  */
void app_board_early_guard(void);

/**
  * @brief Put the board in the state it must boot into and fall back to.
  *
  * RS485 driver off (PB6 low), CAN transceiver in standby (PA10 high), LED
  * dark. Call it right after the generated MX_*_Init functions, and from every
  * error path. Only writes GPIO output registers, so it is safe from any
  * context, a fault handler included.
  */
void app_board_enter_safe_state(void);

/**
  * @brief Drive the L2 status LED on PB0. true = lit.
  *
  * Callers must keep the duty cycle at or below APP_LED_DUTY_PCT_MAX
  * (CLAUDE.md 6.5). Only app_led.c should call this.
  */
void app_board_led_set(bool on);

#endif /* APP_APP_BOARD_H */
