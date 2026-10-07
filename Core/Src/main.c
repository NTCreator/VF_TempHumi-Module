/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "can.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "app/app_board.h"
#include "app/app_config.h"
#include "app/app_diag.h"
#include "app/app_fault.h"
#include "app/app_led.h"
#include "app/app_th.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
/* Which build is on the chip. There is no console to print it on - JP2 carries
   SWD only and USART1 belongs to the sensor - so it lives in the image instead:
   a debugger watch on s_fw_id, or `strings VF_TempHumi_Module.elf`, shows it.
   main() anchors it for the linker - see USER CODE 2. */
static const char s_fw_id[] = "VF_TempHumi_Module phase " APP_FW_PHASE;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */
  /* BEFORE ANYTHING ELSE: release the RS485 bus. The board pulls MAX485 DE/RE
     up to +5 V through 5 kOhm (CLAUDE.md 4.3), so from power-on until this line
     the MAX485 is TRANSMITTING and holding the bus. This runs before HAL_Init on
     purpose: SystemClock_Config below can wait 100 ms for the crystal, and a
     dead crystal goes to Error_Handler before MX_GPIO_Init ever runs. */
  app_board_early_guard();

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_CAN_Init();
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */
  /* Restate the safe state instead of trusting the generated code for it.
     MX_GPIO_Init already drives these pins to the same levels, because the
     .ioc sets PB6 Low and PA10 High - but those are settings that last only
     until somebody forgets them on the next Generate:
       - PB6 high: the MAX485 holds the RS485 bus (CLAUDE.md 4.3);
       - PA10 low: the transceiver goes Normal and an unconfigured module ACKs
         on the measurement bus. A duplicate node_id takes the bus down, it
         does not just make the log messy (CLAUDE.md 1.2).

     CAN is initialised by the generated code but never started (phase 3), so
     the controller stays in init mode (CANTX recessive) behind a transceiver
     in standby - silent on CAN1 twice over. */
  app_board_enter_safe_state();

  /* Nothing reads s_fw_id, so --gc-sections would drop it, and this
     toolchain ignores __attribute__((retain)). An empty asm that takes its
     address is a reference neither the compiler nor the linker may remove:
     one load, no other effect. */
  __asm__ volatile ("" : : "r" (s_fw_id));

  /* g_diag first (the others fill it), the LED before anything that can
     raise a fault (its init clears pending codes). */
  app_diag_init();
  app_led_init(HAL_GetTick());
  app_th_init(HAL_GetTick());

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    /* Bare-metal super-loop (CLAUDE.md 6.3), paced by the 1 ms SysTick. Every
       periodic job is a task that takes the tick, does its work and returns.
       Nothing here may block: the Modbus transaction is a state machine that
       the loop advances, and every wait in it is bounded and counted. */
    const uint32_t now_ms = HAL_GetTick();

    app_th_task(now_ms);
    app_led_task(now_ms);
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USART1;
  PeriphClkInit.Usart1ClockSelection = RCC_USART1CLKSOURCE_PCLK1;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* A HAL init failed. From here: RS485 released, silent on CAN1, LED never
     solid, and the failure visible as blink code 1.

     The return address says which init failed: look it up in
     Debug/VF_TempHumi_Module.list (clear bit 0 first - it is the Thumb bit).
     Read it with a debugger through app_fault_detail() / s_detail. */
  const uint32_t caller = (uint32_t)__builtin_return_address(0);

  __disable_irq();

  /* Run the GPIO init again. If the failure was SystemClock_Config - a dead
     HSE crystal is the likely one on a new board - MX_GPIO_Init has not run
     yet and the LED pin is still an input: without this the LED could not
     blink at all. It only configures the three labelled pins, to their safe
     .ioc levels, and it cannot call Error_Handler itself. PB6 and PA10 are
     already safe from app_board_early_guard(). */
  MX_GPIO_Init();

  app_fault_fatal(APP_FAULT_HAL_INIT, caller);
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
