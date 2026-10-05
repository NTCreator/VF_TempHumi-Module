/**
  ******************************************************************************
  * @file    app_board.c
  * @brief   Pin level access for this board.
  ******************************************************************************
  */

#include "app/app_board.h"

#include "main.h"

/* Pin map: CLAUDE.md 4.1, derived from docs/..._VF_TempHumi_SCH.NET and
   cross-checked against the CubeMX MCU database in phase 1 (pin 14 = PB0,
   20 = PA10, 29 = PB6). The port/pin pairs come from the CubeMX user labels in
   main.h, so a pin move in the .ioc follows through here without an edit. */

/* MAX485 DE and /RE are tied together on ENA_RS485: HIGH = driver on
   (transmit), LOW = receiver on. R8/R9 pull the line to +5 V, so "receive" is
   the state firmware has to fight for. PB6 is five-volt tolerant. */
#define BOARD_RS485_RECEIVE     GPIO_PIN_RESET

/* TJA1044 STB via R2: LOW = Normal, HIGH = Standby (CLAUDE.md 4.1). Naming the
   level keeps the polarity from having to be remembered at a call site. */
#define BOARD_CAN_STB_STANDBY   GPIO_PIN_SET

/* L2 hangs from +3V3 through R5 into PB0: the pin sinks, so LOW lights it. */
#define BOARD_LED_LIT           GPIO_PIN_RESET
#define BOARD_LED_DARK          GPIO_PIN_SET

/* Latch first, mode second: a pin switched to output drives whatever its ODR
   bit already holds. Same settings as MX_GPIO_Init (gpio.c), so the generated
   init that follows changes nothing. */
static void board_drive_output(GPIO_TypeDef *port, uint16_t pin, GPIO_PinState level)
{
  GPIO_InitTypeDef init = {0};

  HAL_GPIO_WritePin(port, pin, level);

  init.Pin   = pin;
  init.Mode  = GPIO_MODE_OUTPUT_PP;
  init.Pull  = GPIO_NOPULL;
  init.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(port, &init);
}

void app_board_early_guard(void)
{
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();

  /* RS485 first: from power-on until this line the MAX485 has been driving the
     sensor bus. Nothing can shorten the window before main() - only moving
     R8/R9 to GND on the next PCB revision removes it. */
  board_drive_output(ENA_RS485_GPIO_Port, ENA_RS485_Pin, BOARD_RS485_RECEIVE);

  /* CAN second. Before this line PA10 floats behind R2, and the bxCAN is not
     even clocked - CANTX (PA12) is a floating input, not a driven dominant. */
  board_drive_output(EN_CAN1_GPIO_Port, EN_CAN1_Pin, BOARD_CAN_STB_STANDBY);
}

void app_board_enter_safe_state(void)
{
  HAL_GPIO_WritePin(ENA_RS485_GPIO_Port, ENA_RS485_Pin, BOARD_RS485_RECEIVE);
  HAL_GPIO_WritePin(EN_CAN1_GPIO_Port, EN_CAN1_Pin, BOARD_CAN_STB_STANDBY);
  HAL_GPIO_WritePin(LED_DEBUG_GPIO_Port, LED_DEBUG_Pin, BOARD_LED_DARK);
}

void app_board_led_set(bool on)
{
  HAL_GPIO_WritePin(LED_DEBUG_GPIO_Port, LED_DEBUG_Pin, on ? BOARD_LED_LIT : BOARD_LED_DARK);
}
