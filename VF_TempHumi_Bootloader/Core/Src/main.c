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
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "app/app_board.h"       /* linked from VF_TempHumi_Module/Core/Src/app */
#include "app/app_cfg.h"         /* linked from VF_TempHumi_Module/Core/Src/app */
#include "app/app_cfg_store.h"   /* linked from VF_TempHumi_Module/Core/Src/app */
#include "app/boot_board.h"
#include "app/boot_can.h"
#include "app/boot_config.h"
#include "app/boot_jump.h"
#include "app/boot_led.h"
#include "app/boot_policy.h"
#include "app/boot_svc.h"

#include <stdbool.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
/* What the bootloader would say if it had a console - read it in a debug
   session of this project (Live Expressions: g_boot). Same idea as the
   application's g_diag. */
typedef struct
{
  uint32_t magic;            /* BOOT_DIAG_MAGIC */
  uint8_t  app_present;      /* 1 = a plausible vector table at APP_IMAGE_BASE */
  uint8_t  cfg_state;        /* app_cfg_check() of the page at power-up: 0 OK, 1 blank .. 7 */
  uint8_t  can_start;        /* BOOT_CAN_OK, or the step that failed */
  uint8_t  policy;           /* BOOT_POLICY_*: 0 listen, 1 rescue, 2 unconfigured */
  uint8_t  heard;            /* 1 once a 0x7E0 request has been served */
  uint8_t  last_cmd;         /* last request: command byte */
  uint8_t  last_status;      /* ... and the status answered */
  uint32_t requests;         /* 0x7E0 frames served */
  uint32_t rx_errors;        /* a frame was waiting but could not be read */
  uint32_t responses_ok;     /* 0x7E8 left and was ACKed */
  uint32_t responses_lost;   /* 0x7E8 not ACKed, or aborted after BOOT_SEND_TIMEOUT_MS */
} boot_diag_t;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* Reads as "THB1" in a hex view: the debugger is looking at the live struct.
   The application's g_diag reads "THD1". */
#define BOOT_DIAG_MAGIC   0x31424854u
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
/* Which bootloader is on the chip - `strings` on the .elf, or a debugger. */
static const char s_boot_id[] = BOOT_FW_ID;

boot_diag_t g_boot;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
/* The protocol's view of the world: the SAME flash code the application uses
   (app_cfg_store.c, linked from VF_TempHumi_Module), so what COMMIT writes is
   byte for byte what the application reads at boot. */
static const boot_svc_hooks_t k_svc_hooks =
{
  .read_cfg  = app_cfg_store_read,
  .write_cfg = app_cfg_store_write,
  .read_uid  = boot_board_read_uid,
};

/* Code 3 while nobody has spoken to a module that is waiting to be
   configured - the application's own code for "no valid config", so the
   operator learns no new pattern. The double flash in every other case, and
   as soon as the first 0x7E0 arrives: the operator sees the mainboard get
   through. Phase 5 plan 2.7. */
static void boot_led_task(void)
{
  const uint32_t now = HAL_GetTick();
  const bool on = ((g_boot.policy == BOOT_POLICY_STAY_UNCONFIGURED) && (g_boot.heard == 0u))
                  ? boot_led_code_on(BOOT_LED_CODE_UNCONFIGURED, now)
                  : boot_led_double_on(now);

  app_board_led_set(on);
}

/* One request, if there is one: answer it, and reset if it was RESET - after
   the answer has left, so the mainboard can tell "reset" from "dead" (CAN2
   section 5, invariant 4). Returns true if a request was served. */
static bool boot_serve_once(void)
{
  uint8_t req[BOOT_SVC_DLC];
  uint8_t rsp[BOOT_SVC_DLC];
  uint8_t len = 0u;

  const uint8_t rx = boot_can_recv(req, &len);
  if (rx == BOOT_CAN_RX_ERROR)
  {
    g_boot.rx_errors++;
    return false;
  }
  if (rx != BOOT_CAN_RX_FRAME)
  {
    return false;
  }

  g_boot.heard = 1u;
  g_boot.requests++;

  const uint8_t action = boot_svc_handle(req, len, rsp);
  g_boot.last_cmd    = rsp[0];
  g_boot.last_status = rsp[1];

  /* A lost response is recovered by the mainboard, which retries (CAN2 3). */
  if (boot_can_send(rsp))
  {
    g_boot.responses_ok++;
  }
  else
  {
    g_boot.responses_lost++;
  }

  if (action == (uint8_t)BOOT_SVC_RESET)
  {
    /* The response has left its mailbox (boot_can_send waited for it). Board
       safe, then reset: the next power-up decides afresh, and after a COMMIT
       it takes the listen-and-jump path. */
    app_board_enter_safe_state();
    NVIC_SystemReset();
  }
  return true;
}
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
     the MAX485 is TRANSMITTING. The bootloader runs first at every power-up and
     may stay for ever (no application, no config), so this is the line that
     matters most for the RS485 bus - the very function measured on the board
     in phase 1, linked from the application, not copied. Before HAL_Init: a
     dead crystal sends SystemClock_Config to Error_Handler before
     MX_GPIO_Init would ever run. */
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
  /* USER CODE BEGIN 2 */
  /*
   * THE WHOLE BOOTLOADER, IN ONE PAGE (phase 5 plan 2.2).
   *
   *   PB6 low (early guard), clocks, GPIO, CAN init
   *      |
   *      +- CAN start: filter 0x7E0, transceiver Normal, HAL_CAN_Start
   *      |     failed -> application + valid config ? jump : stay, LED code 5
   *      |
   *      +- is there an application at APP_IMAGE_BASE?
   *      |     no  -> stay for ever, serving CAN2   <- the rescue path
   *      |
   *      +- does the config page pass app_cfg_check() - the app's own rule?
   *      |     no  -> stay for ever, serving CAN2   <- not configured
   *      |
   *      +- listen for 0x7E0 for BOOT_LISTEN_WINDOW_MS (CAN2 5, invariant 5)
   *            heard one  -> stay, serving CAN2 until RESET or power-off
   *            silence    -> jump to the application
   *
   * The transceiver is Normal throughout, whatever the config says: CAN2 5.1
   * and 5.3 require an answer even from a blank page. What keeps CAN1 safe is
   * that this program transmits ONLY 0x7E8, ONLY in reply to 0x7E0, and 0x7E0
   * does not exist on CAN1. It never sends a TH_* frame. CLAUDE.md 6.5.
   */
  __asm__ volatile ("" : : "r" (s_boot_id));   /* keep the id in the image */

  /* Restate the safe state instead of trusting the generated levels: RS485
     released, transceiver standby, LED dark. */
  app_board_enter_safe_state();

  g_boot.magic = BOOT_DIAG_MAGIC;
  boot_svc_init(&k_svc_hooks);

  {
    /* Read here, not taken from boot_svc: the decision uses the page exactly
       as the application will read it after the jump. */
    app_cfg_t stored;
    app_cfg_store_read(&stored);
    g_boot.cfg_state   = app_cfg_check(&stored);
    g_boot.app_present = boot_app_present() ? 1u : 0u;
    g_boot.policy      = boot_policy_at_power_up(g_boot.app_present != 0u, g_boot.cfg_state);
  }

  g_boot.can_start = boot_can_start();
  if (g_boot.can_start != BOOT_CAN_OK)
  {
    /* No CAN: no protocol, so nothing can be served here. A module that
       would have jumped anyway still does - the application at least
       measures, and reports CAN on its own terms. Any other module stays and
       says why: code 5, as the application shows a CAN that will not start. */
    if (g_boot.policy == BOOT_POLICY_LISTEN)
    {
      boot_jump_to_app();
    }
    for (;;)
    {
      app_board_led_set(boot_led_code_on(BOOT_LED_CODE_CAN, HAL_GetTick()));
    }
  }

  if (g_boot.policy == BOOT_POLICY_LISTEN)
  {
    const uint32_t t0 = HAL_GetTick();
    bool stay = false;

    while (!stay && ((HAL_GetTick() - t0) < BOOT_LISTEN_WINDOW_MS))
    {
      boot_led_task();
      /* Somebody is talking: stop counting. A session is several commands
         and far longer than the window, and a jump between SET_NODE_ID and
         COMMIT would lose it. */
      stay = boot_serve_once();
    }

    if (!stay)
    {
      boot_jump_to_app();
    }
  }
  /* A session is open, there is nothing to jump to, or nothing configured to
     jump for. The only way out is a reset - RESET over CAN2, SW1, or a power
     cycle - and after a COMMIT the next power-up takes the window path. */
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    boot_led_task();
    (void)boot_serve_once();
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
  /* A generated init failed: SystemClock_Config (a dead crystal is the likely
     one on a new board) or MX_CAN_Init. Quiet and safe from here - RS485
     released, transceiver standby - and the failure visible as code 1, the
     application's code for the same thing.

     MX_GPIO_Init runs again because after a clock failure it has not run yet
     and the LED pin is still an input. It only drives the three labelled pins
     to their safe .ioc levels and cannot call Error_Handler itself. PB6 and
     PA10 are already safe from app_board_early_guard(). */
  __disable_irq();
  MX_GPIO_Init();
  app_board_enter_safe_state();
  boot_board_code_forever(BOOT_LED_CODE_HAL_INIT);
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
