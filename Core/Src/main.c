/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Smart Food Freshness Monitor
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
#include "cmsis_os.h"

/* USER CODE BEGIN Includes */
#include "lcd_i2c.h"
#include "dht22.h"
#include "food_score.h"
#include <stdio.h>
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
ADC_HandleTypeDef hadc1;
DMA_HandleTypeDef hdma_adc1;
I2C_HandleTypeDef hi2c1;
TIM_HandleTypeDef htim2;
UART_HandleTypeDef huart2;

/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for SensorReadTask */
osThreadId_t SensorReadTaskHandle;
const osThreadAttr_t SensorReadTask_attributes = {
  .name = "SensorReadTask",
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for DisplayTask */
osThreadId_t DisplayTaskHandle;
const osThreadAttr_t DisplayTask_attributes = {
  .name = "DisplayTask",
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityBelowNormal,
};
/* Definitions for ButtonTask */
osThreadId_t ButtonTaskHandle;
const osThreadAttr_t ButtonTask_attributes = {
  .name = "ButtonTask",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityAboveNormal,
};

/* USER CODE BEGIN PV */
DHT22_Data_t  dht22Data;
uint16_t      adcBuffer[2];          /* [0] = MQ135 PA0,  [1] = MQ3 PA1        */

/* Initialize to FRESH so display shows correctly before first sensor cycle */
ScoreResult_t lastScore = { .score = 100, .status = STATUS_FRESH,
                             .mq135_pct = 0, .mq3_pct = 0 };

FoodMode_t    currentFoodMode = FOOD_MEAT;
int16_t       lastTemperature = 25;
uint8_t       lastHumidity    = 0;
osMutexId_t   lcdMutex;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_ADC1_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_I2C1_Init(void);
static void MX_TIM2_Init(void);
void StartDefaultTask(void *argument);
void StartSensorTask(void *argument);
void StartDisplayTask(void *argument);
void StartButtonTask(void *argument);

/* USER CODE BEGIN PFP */
/* USER CODE END PFP */

/* USER CODE BEGIN 0 */
/* USER CODE END 0 */

/* -- main ------------------------------------------------------------------- */
int main(void)
{
  /* USER CODE BEGIN 1 */
  /* USER CODE END 1 */

  HAL_Init();

  /* USER CODE BEGIN Init */
  /* USER CODE END Init */

  SystemClock_Config();

  /* USER CODE BEGIN SysInit */
  /* USER CODE END SysInit */

  MX_GPIO_Init();
  MX_DMA_Init();
  MX_ADC1_Init();
  MX_USART2_UART_Init();
  MX_I2C1_Init();
  MX_TIM2_Init();

  /* USER CODE BEGIN 2 */
  HAL_TIM_Base_Start(&htim2);   /* start free-running us counter for DHT22 */
  /* USER CODE END 2 */

  osKernelInitialize();

  /* USER CODE BEGIN RTOS_MUTEX */
  lcdMutex = osMutexNew(NULL);
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* USER CODE END RTOS_QUEUES */

  defaultTaskHandle    = osThreadNew(StartDefaultTask, NULL,
                                     &defaultTask_attributes);
  SensorReadTaskHandle = osThreadNew(StartSensorTask,  NULL,
                                     &SensorReadTask_attributes);
  DisplayTaskHandle    = osThreadNew(StartDisplayTask, NULL,
                                     &DisplayTask_attributes);
  ButtonTaskHandle     = osThreadNew(StartButtonTask,  NULL,
                                     &ButtonTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* USER CODE END RTOS_EVENTS */

  osKernelStart();

  /* Should never reach here */
  while (1)
  {
    /* USER CODE BEGIN 3 */
    /* USER CODE END 3 */
  }
}

/* -- SystemClock_Config ----------------------------------------------------- */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) != HAL_OK)
    Error_Handler();

  HAL_PWR_EnableBkUpAccess();
  __HAL_RCC_LSEDRIVE_CONFIG(RCC_LSEDRIVE_LOW);

  RCC_OscInitStruct.OscillatorType      = RCC_OSCILLATORTYPE_LSE
                                        | RCC_OSCILLATORTYPE_MSI;
  RCC_OscInitStruct.LSEState            = RCC_LSE_ON;
  RCC_OscInitStruct.MSIState            = RCC_MSI_ON;
  RCC_OscInitStruct.MSICalibrationValue = 0;
  RCC_OscInitStruct.MSIClockRange       = RCC_MSIRANGE_6;
  RCC_OscInitStruct.PLL.PLLState        = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource       = RCC_PLLSOURCE_MSI;
  RCC_OscInitStruct.PLL.PLLM           = 1;
  RCC_OscInitStruct.PLL.PLLN           = 40;
  RCC_OscInitStruct.PLL.PLLP           = RCC_PLLP_DIV7;
  RCC_OscInitStruct.PLL.PLLQ           = RCC_PLLQ_DIV2;
  RCC_OscInitStruct.PLL.PLLR           = RCC_PLLR_DIV2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    Error_Handler();

  RCC_ClkInitStruct.ClockType      = RCC_CLOCKTYPE_HCLK  | RCC_CLOCKTYPE_SYSCLK
                                   | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider  = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
    Error_Handler();

  HAL_RCCEx_EnableMSIPLLMode();
}

/* -- MX_ADC1_Init ----------------------------------------------------------- */
static void MX_ADC1_Init(void)
{
  ADC_ChannelConfTypeDef sConfig = {0};

  hadc1.Instance                   = ADC1;
  hadc1.Init.ClockPrescaler        = ADC_CLOCK_SYNC_PCLK_DIV4;
  hadc1.Init.Resolution            = ADC_RESOLUTION_12B;
  hadc1.Init.DataAlign             = ADC_DATAALIGN_RIGHT;
  hadc1.Init.ScanConvMode          = ADC_SCAN_ENABLE;
  hadc1.Init.EOCSelection          = ADC_EOC_SEQ_CONV;
  hadc1.Init.LowPowerAutoWait      = DISABLE;
  hadc1.Init.ContinuousConvMode    = ENABLE;
  hadc1.Init.NbrOfConversion       = 2;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConv      = ADC_SOFTWARE_START;
  hadc1.Init.ExternalTrigConvEdge  = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc1.Init.DMAContinuousRequests = ENABLE;
  hadc1.Init.Overrun               = ADC_OVR_DATA_OVERWRITTEN;
  hadc1.Init.OversamplingMode      = DISABLE;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
    Error_Handler();

  /* MQ135 on PA0   ADC1 Channel 5   Rank 1 */
  sConfig.Channel      = ADC_CHANNEL_5;
  sConfig.Rank         = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLETIME_47CYCLES_5;
  sConfig.SingleDiff   = ADC_SINGLE_ENDED;
  sConfig.OffsetNumber = ADC_OFFSET_NONE;
  sConfig.Offset       = 0;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
    Error_Handler();

  /* MQ3 on PA1   ADC1 Channel 6   Rank 2 */
  sConfig.Channel = ADC_CHANNEL_6;
  sConfig.Rank    = ADC_REGULAR_RANK_2;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
    Error_Handler();
}

/* -- MX_I2C1_Init ----------------------------------------------------------- */
static void MX_I2C1_Init(void)
{
  hi2c1.Instance              = I2C1;
  hi2c1.Init.Timing           = 0x10909CEC;
  hi2c1.Init.OwnAddress1      = 0;
  hi2c1.Init.AddressingMode   = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode  = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2      = 0;
  hi2c1.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  hi2c1.Init.GeneralCallMode  = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode    = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
    Error_Handler();
  if (HAL_I2CEx_ConfigAnalogFilter(&hi2c1, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
    Error_Handler();
  if (HAL_I2CEx_ConfigDigitalFilter(&hi2c1, 0) != HAL_OK)
    Error_Handler();
}

/* -- MX_TIM2_Init ----------------------------------------------------------- */
static void MX_TIM2_Init(void)
{
  TIM_ClockConfigTypeDef  sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig      = {0};

  htim2.Instance               = TIM2;
  htim2.Init.Prescaler         = 79;           /* 80MHz / 80 = 1MHz = 1us/tick */
  htim2.Init.CounterMode       = TIM_COUNTERMODE_UP;
  htim2.Init.Period            = 0xFFFFFFFF;
  htim2.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
  if (HAL_TIM_Base_Init(&htim2) != HAL_OK)
    Error_Handler();

  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig) != HAL_OK)
    Error_Handler();

  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode     = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
    Error_Handler();
}

/* -- MX_USART2_UART_Init ---------------------------------------------------- */
static void MX_USART2_UART_Init(void)
{
  huart2.Instance                    = USART2;
  huart2.Init.BaudRate               = 115200;
  huart2.Init.WordLength             = UART_WORDLENGTH_8B;
  huart2.Init.StopBits               = UART_STOPBITS_1;
  huart2.Init.Parity                 = UART_PARITY_NONE;
  huart2.Init.Mode                   = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl              = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling           = UART_OVERSAMPLING_16;
  huart2.Init.OneBitSampling         = UART_ONE_BIT_SAMPLE_DISABLE;
  huart2.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart2) != HAL_OK)
    Error_Handler();
}

/* -- MX_DMA_Init ------------------------------------------------------------ */
static void MX_DMA_Init(void)
{
  __HAL_RCC_DMA1_CLK_ENABLE();
  HAL_NVIC_SetPriority(DMA1_Channel1_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel1_IRQn);
}

/* -- MX_GPIO_Init ----------------------------------------------------------- */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /* LD3 output default LOW */
  HAL_GPIO_WritePin(LD3_GPIO_Port, LD3_Pin, GPIO_PIN_RESET);

  /* DHT22 DATA pin   PA6
   * Output open-drain so firmware can switch direction.
   * Pull-up on breakout module holds line HIGH at rest.
   * Set HIGH immediately as idle state.                                    */
  GPIO_InitStruct.Pin   = DHT22_DATA_Pin;
  GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_OD;
  GPIO_InitStruct.Pull  = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  HAL_GPIO_Init(DHT22_DATA_GPIO_Port, &GPIO_InitStruct);
  HAL_GPIO_WritePin(DHT22_DATA_GPIO_Port, DHT22_DATA_Pin, GPIO_PIN_SET);

  /* Push button   PB0
   * Falling edge interrupt (pin goes LOW when pressed).
   * Internal pull-up holds line HIGH at rest   no external resistor needed. */
  GPIO_InitStruct.Pin  = FOOD_BTN_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(FOOD_BTN_GPIO_Port, &GPIO_InitStruct);

  /* LD3 output */
  GPIO_InitStruct.Pin   = LD3_Pin;
  GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull  = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LD3_GPIO_Port, &GPIO_InitStruct);

  /* EXTI0 interrupt for button   priority 5 (safe for FreeRTOS API calls) */
  HAL_NVIC_SetPriority(EXTI0_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(EXTI0_IRQn);
}

/* -- USER CODE BEGIN 4 ------------------------------------------------------ */

/* Button ISR   disables IRQ immediately to block bounce, notifies ButtonTask */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  if (GPIO_Pin == FOOD_BTN_Pin)
  {
    HAL_NVIC_DisableIRQ(EXTI0_IRQn);
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    vTaskNotifyGiveFromISR(ButtonTaskHandle, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
  }
}

/* TIM1 period elapsed   used as HAL timebase (not SysTick, which = FreeRTOS) */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  if (htim->Instance == TIM1)
    HAL_IncTick();
}
/* -- USER CODE END 4 -------------------------------------------------------- */


/* ----------------------------------------------------------------------------
 * DEFAULT TASK   idle, does nothing useful
 * ---------------------------------------------------------------------------- */
void StartDefaultTask(void *argument)
{
  for(;;)
    osDelay(1000);
}


/* ----------------------------------------------------------------------------
 * SENSOR TASK
 * Waits 33s (30s warmup + 3s LCD init), then reads both MQ sensors from
 * the DMA buffer and computes the freshness score every 5 seconds.
 * Change osDelay(5000) ? osDelay(30000) for final deployment.
 * ---------------------------------------------------------------------------- */
void StartSensorTask(void *argument)
{
  /* Wait for warmup */
  osDelay(33000);

  for(;;)
  {
    SensorInput_t input;

    /* Read MQ sensors   adcBuffer is updated continuously by DMA */
    input.mq135_raw   = adcBuffer[0];   /* MQ135 ammonia/VOC on PA0 */
    input.mq3_raw     = adcBuffer[1];   /* MQ3   ethanol    on PA1 */

    /* Temperature from DHT22 on PA6 */
    DHT22_Status_t dht_status = DHT22_Read(&dht22Data);
    if (dht_status == DHT22_OK)
    {
        input.temperature = dht22Data.temp_negative
                            ? -(int16_t)dht22Data.temperature_int
                            :  (int16_t)dht22Data.temperature_int;
        input.humidity    = (uint8_t)dht22Data.humidity_int;
        lastTemperature   = input.temperature;
        lastHumidity      = input.humidity;
    }
    else
    {
        input.temperature = lastTemperature;
        input.humidity    = lastHumidity;
    }

    /* Compute freshness score using currently selected food mode */
    lastScore = ComputeFreshnessScore(currentFoodMode, &input);

    /* 5s during testing   change to 30000 for deployment */
    osDelay(5000);
  }
}


/* ----------------------------------------------------------------------------
 * DISPLAY TASK
 * Initialises LCD and ADC+DMA, shows 30s warmup countdown, then
 * refreshes every 1 second showing:
 *   Line 1: [MODE] [STATUS] [SCORE]%    e.g. "MEAT  FRESH  95%"
 *   Line 2: A:[mq135 raw] E:[mq3 raw]   e.g. "A:  62 E: 233"
 * All LCD writes protected by lcdMutex.
 * ---------------------------------------------------------------------------- */
void StartDisplayTask(void *argument)
{
  char line1[17];
  char line2[17];

  /* Initialise LCD   must be inside a task because LCD_Init uses osDelay */
  LCD_Init(&hi2c1);
  LCD_SetCursor(0, 0);
  LCD_Print("Food Monitor");
  LCD_SetCursor(1, 0);
  LCD_Print("Warming up...");

  /* Calibrate ADC then start DMA runs continuously from here on */
  HAL_ADCEx_Calibration_Start(&hadc1, ADC_SINGLE_ENDED);
  HAL_ADC_Start_DMA(&hadc1, (uint32_t*)adcBuffer, 2);

  /* 30 second warmup countdown */
  for (int i = 30; i > 0; i--)
  {
    osMutexAcquire(lcdMutex, osWaitForever);
    LCD_Clear();
    LCD_SetCursor(0, 0);
    LCD_Print("Warming sensors");
    snprintf(line2, sizeof(line2), "Wait: %2ds", i);
    LCD_SetCursor(1, 0);
    LCD_Print(line2);
    osMutexRelease(lcdMutex);

    osDelay(1000);
  }

  /* Main display loop */
  for(;;)
  {
    /* Map status to string */
    const char *status_str;
    if      (lastScore.status == STATUS_FRESH) status_str = "FRESH";
    else if (lastScore.status == STATUS_WARN)  status_str = "WARN ";
    else                                        status_str = "SPOIL";

    /* Line 1: "MEAT  FRESH  95%" */
    snprintf(line1, sizeof(line1), "%s %s %3d%%",
             FoodMode_ShortName(currentFoodMode),
             status_str,
             lastScore.score);

    /* Line 2: estimated PPM for each sensor
     * Formula: ppm = (raw - BASELINE) * PPM_FULLSCALE / RANGE
     * Constants mirror Food_Score.c: MQ135 baseline=57 range=4038 full scale=200
     *                                MQ3   baseline=247 range=3848 full scale=400
     * A = MQ135 (NH3/VOC), E = MQ3 (ethanol)                            */
    int16_t mq135_ppm = (int16_t)((int32_t)(adcBuffer[0] - 57)  * 200 / 343);
    int16_t mq3_ppm   = (int16_t)((int32_t)(adcBuffer[1] - 247) * 400 / 3848);
    if (mq135_ppm < 0)   mq135_ppm = 0;
    if (mq135_ppm > 200) mq135_ppm = 200;
    if (mq3_ppm   < 0)   mq3_ppm   = 0;
    snprintf(line2, sizeof(line2), "A:%3d E:%3d %3dC",
             mq135_ppm, mq3_ppm, lastTemperature);

    osMutexAcquire(lcdMutex, osWaitForever);
    LCD_Clear();
    LCD_SetCursor(0, 0);
    LCD_Print(line1);
    LCD_SetCursor(1, 0);
    LCD_Print(line2);
    osMutexRelease(lcdMutex);

    osDelay(1000);
  }
}


/* ----------------------------------------------------------------------------
 * BUTTON TASK
 * Blocks until ISR notification, debounces 50ms, confirms pin still LOW,
 * cycles food mode, shows new mode on LCD for 1s, waits for release,
 * debounces release, re-enables EXTI interrupt.
 * ---------------------------------------------------------------------------- */
void StartButtonTask(void *argument)
{
  for(;;)
  {
    /* Block until button ISR sends notification */
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    /* Wait for mechanical bounce to settle */
    osDelay(50);

    /* Confirm button is still pressed   reject noise spikes */
    if (HAL_GPIO_ReadPin(FOOD_BTN_GPIO_Port, FOOD_BTN_Pin) == GPIO_PIN_RESET)
    {
      /* Valid press   cycle food mode */
      currentFoodMode = (FoodMode_t)((currentFoodMode + 1) % FOOD_MODE_COUNT);

      /* Show new mode on LCD for 1 second */
      osMutexAcquire(lcdMutex, osWaitForever);
      LCD_Clear();
      LCD_SetCursor(0, 0);
      LCD_Print("Mode changed:");
      LCD_SetCursor(1, 0);
      LCD_Print(FoodMode_Name(currentFoodMode));
      osMutexRelease(lcdMutex);

      osDelay(1000);
    }

    /* Wait for button to be fully released */
    while (HAL_GPIO_ReadPin(FOOD_BTN_GPIO_Port, FOOD_BTN_Pin) == GPIO_PIN_RESET)
      osDelay(10);

    /* Extra delay for release bounce */
    osDelay(50);

    /* Re-enable button interrupt   ready for next press */
    HAL_NVIC_EnableIRQ(EXTI0_IRQn);
  }
}


/* -- Error handler ---------------------------------------------------------- */
void Error_Handler(void)
{
  __disable_irq();
  while (1) {}
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
  /* printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
}
#endif /* USE_FULL_ASSERT */