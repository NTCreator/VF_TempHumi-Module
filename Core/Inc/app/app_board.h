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
  * The call that puts the transceiver in Normal exists from phase 4, and it
  * demands a valid config as its argument: "only a CRC-checked config may
  * pull PA10 low" is a property of the function, not a promise of its
  * callers. Deliberately missing: any exported call that raises DE. Raising DE lives in drv_rs485.c as a static function,
  * paired with the transmit-complete interrupt that drops it again
  * (docs/VF_TH_IOC_Guide.md A8). A function that does not exist cannot be
  * called by mistake. Releasing the bus is always safe, so that one is here.
  ******************************************************************************
  */
#ifndef APP_APP_BOARD_H
#define APP_APP_BOARD_H

#include "app/app_cfg.h"

#include <stdbool.h>
#include <stdint.h>

/**
  * @brief TJA1044 to Normal: this module can now drive CAN1. Phase 4.
  *
  * Takes the config as proof and checks it itself (app_cfg_valid). NULL or
  * anything invalid: the transceiver is put in standby and false returned.
  * Call only after HAL_CAN_Start() succeeded in NORMAL mode.
  */
bool app_board_can_transceiver_normal(const app_cfg_t *proof);

/** @brief TJA1044 to standby: TXD is cut from the bus. Always safe. */
void app_board_can_transceiver_standby(void);

/** @brief PA10 as the pin reads (IDR): 1 = standby, 0 = Normal. What a
  *        multimeter would show, from the firmware's side. */
uint8_t app_board_can_stb_level(void);

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
  * @brief MAX485 to receive: PB6 low, driver off, bus released.
  *
  * Always safe, from any context - drv_rs485 calls it from the UART
  * interrupt the moment the last stop bit has left the wire.
  */
void app_board_rs485_release(void);

/**
  * @brief Drive the L2 status LED on PB0. true = lit.
  *
  * Callers must keep the duty cycle at or below APP_LED_DUTY_PCT_MAX
  * (CLAUDE.md 6.5). Only app_led.c should call this.
  */
void app_board_led_set(bool on);

#endif /* APP_APP_BOARD_H */
