/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
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

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "measure_speed_FC33.h"
#include "VL53L0X.h"
#include "steering_servo.h"
/* vbolbat includes */
#include "logger.h"
#include "esc.h"
#include "mpu6050.h"
#include "converters.h"
#include "usart1_callbacks.h"
/* system includes */
#include "string.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
// Определяем пины управления питанием (XSHUT) датчиков
// Предполагаем, что они подключены к PB1 и PB2, которые у вас уже инициализированы
#define XSHUT_SENSOR1_PORT GPIOB
#define XSHUT_SENSOR1_PIN  GPIO_PIN_1

#define XSHUT_SENSOR2_PORT GPIOB
#define XSHUT_SENSOR2_PIN  GPIO_PIN_2

// Адреса датчиков (7-битные, сдвинутые библиотекой, или как принимает ваша либа)
// Стандартный адрес 0x29. Второй переназначим на 0x30.
#define SENSOR1_NEW_ADDR 0x30
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c1;
I2C_HandleTypeDef hi2c2;

TIM_HandleTypeDef htim2;
TIM_HandleTypeDef htim3;
TIM_HandleTypeDef htim4; /* used by vbolbat for esc control */
TIM_HandleTypeDef htim5;
TIM_HandleTypeDef htim6; /* used by vbolbat for esc && mpu6050 data update */

UART_HandleTypeDef huart1;
UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
//TIM_HandleTypeDef htim_esc_interrupt;
esc_t esc_struct;
MPU6050_t mpu6050_struct;
// --- Инициализация VL53L0X ---		
VL53L0X_Dev_t sensor1;
VL53L0X_Dev_t sensor2;

statInfo_t_VL53L0X distanceStr1;
statInfo_t_VL53L0X distanceStr2;

uint8_t speed_calibration_buffer[2]; //Глобальная переменная для передачи скорости в колбек таймера esc
extern uint8_t  log_buf[LOGGING_BUF_SIZE];
extern uint32_t log_pointer;
extern uint8_t rx_byte; /* for usart1 */
extern Command_State_t cmd_state; /* uart parser flags */
uint8_t current_pwm = 0;
uint8_t current_direction = 1;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);
static void MX_I2C2_Init(void);
static void MX_TIM3_Init(void);
static void MX_USART1_UART_Init(void);
//static void MX_USART2_UART_Init(void);
static void MX_TIM4_Init(void);
static void TIM5_Init(uint32_t timer_period_ms);
static void MX_TIM2_Init(void);
static void MX_TIM6_Init(void); /* used by vbolbat for esc && mpu6050 data update */
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
uint16_t dist1_mm = 0;
uint16_t dist2_mm = 0;
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

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
  MX_I2C1_Init();
  MX_I2C2_Init();
  MX_USART1_UART_Init();
  //MX_USART2_UART_Init();
  MX_TIM3_Init();
  MX_TIM4_Init();
  TIM5_Init(1000); // Параметр - период таймера в мс. Для датчика FC33
  MX_TIM2_Init();
  MX_TIM6_Init();
//#define TEST
#ifndef TEST
  SteeringServo_Init(); // Инициализация для сервопривода рулевого управления
  SteeringServo_SetAngle(90); // Выставление угла поворота колёс нейтральное положение (0 градусов)

  HAL_TIM_Base_Start_IT(&htim5);
  __HAL_RCC_SYSCFG_CLK_ENABLE();
  /* USER CODE BEGIN 2 */

  MeasureSpeedFC33_Init(6.5f, 1000, 1); //Диаметр колеса в см, период TIM5, кол-во прерываний за одно вращение

  // --- ЛОГИКА ИНИЦИАЛИЗАЦИИ ДВУХ ДАТЧИКОВ ---

  // 1. Сбрасываем оба датчика (ставим XSHUT в Low)
  HAL_GPIO_WritePin(XSHUT_SENSOR1_PORT, XSHUT_SENSOR1_PIN, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(XSHUT_SENSOR2_PORT, XSHUT_SENSOR2_PIN, GPIO_PIN_RESET);
  HAL_Delay(20);

  // 2. Включаем ПЕРВЫЙ датчик
  HAL_GPIO_WritePin(XSHUT_SENSOR1_PORT, XSHUT_SENSOR1_PIN, GPIO_PIN_SET);
  HAL_Delay(20); // Ждем загрузки

  // 3. Инициализируем первый датчик (он сейчас на адресе по умолчанию 0x29)
  // ВАЖНО: Функции initVL53L0X теперь нужно передавать указатель на структуру (&sensor1)
  if(initVL53L0X(&sensor1, 1, &hi2c1)) {
      // 4. МЕНЯЕМ АДРЕС первого датчика
      setAddress_VL53L0X(&sensor1, SENSOR1_NEW_ADDR);
  } else {
      // Ошибка инициализации датчика 1
  }

  // 5. Включаем ВТОРОЙ датчик (он проснется с адресом по умолчанию 0x29)
   HAL_GPIO_WritePin(XSHUT_SENSOR2_PORT, XSHUT_SENSOR2_PIN, GPIO_PIN_SET);
   HAL_Delay(20);

   // 6. Инициализируем второй датчик (адрес менять не надо, оставим 0x29)
   if(!initVL53L0X(&sensor2, 1, &hi2c1)) {
       // Ошибка инициализации датчика 2
   }

   // Настройка параметров для обоих датчиков
   setSignalRateLimit(&sensor1, 0.25); // Обратите внимание на аргумент &sensor1
   setVcselPulsePeriod(&sensor1, VcselPeriodPreRange, 10);
   setVcselPulsePeriod(&sensor1, VcselPeriodFinalRange, 14);
   setMeasurementTimingBudget(&sensor1, 30000); // 30ms для скорости

   setSignalRateLimit(&sensor2, 0.25);
   setVcselPulsePeriod(&sensor2, VcselPeriodPreRange, 10);
   setVcselPulsePeriod(&sensor2, VcselPeriodFinalRange, 14);
   setMeasurementTimingBudget(&sensor2, 30000);

   //vbolbat: MPU6050 init
   MPU6050_Init(&hi2c2);
#endif
  //vbolbat: ESC init
  esc_init(&esc_struct);

  //vbolbat: init the logger
  logger_init();

  /* vbolbat: Вызывать после всех инициализаций: */
  if (HAL_TIM_Base_Start_IT(&htim6) != HAL_OK)
  {
	  /* used by vbolbat for esc && mpu6050 data update */
	  Error_Handler();
  }
  /* vbolbat: Неблокирующий старт юарта на прием */
  if (HAL_UART_Receive_IT(&huart1, &rx_byte, 1) != HAL_OK)
  {
        Error_Handler();
  }
  /* USER CODE END 2 */
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_2, 1);
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3, 1);

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
	  /* vbolbat: обработка флагов с парсера */
	  process_parser_flags();
	  /* vbolbat: за sync с управляющим устройством считаю
	   * запрос телеметрии. В случае отстутствия синхронизации в течении
	   * 1 секунды машинка сбрасывает скорость. См калбек TIM6.
	   * Функционал в отладочных целях возможно отключить объявив макрос
	   * __DISABLE_AUTOBREAK__ в main.h. За расчет sync-ов отвечает переменная
	   * sync_count, объявленная в uart1_callback */

	  /* INTEGRATION TEST LOG
	   * ESC           x
	   * PARSER        x
	   * ERROR HANDLER x
	   * SERVO         x
	   * RANGE SENSORS x
	   * LOGGER        x
	   * TIM6ithandler x
	   * MPU (NEED STATIC FILTER) x
	   * REVIEV TIMER PERIOD      x
	   * REVIEV UART1 BAUD        x
	   */

    // Чтение дистанции в миллиметрах					
	//      // Чтение первого датчика
	//      dist1_mm = readRangeSingleMillimeters(&sensor1, &distanceStr1);
	//
	//      // Чтение второго датчика (если используете Single Shot режим, они будут измерять последовательно)
	//      // Для ускорения можно использовать Continuous режим, но это сложнее в настройке.
	//      dist2_mm = readRangeSingleMillimeters(&sensor2, &distanceStr2);
	//
	//      // Проверка на таймаут или ошибки
	//      if (sensor1.did_timeout) {
	//          // Обработка ошибки датчика 1
	//      }
	//      if (sensor2.did_timeout) {
	//           // Обработка ошибки датчика 2
	//      }
	    //       Для FC-33		
	    //     uint32_t pulses = MeasureSpeedFC33_GetRPM();		
	    //     float speed_kmh = MeasureSpeedFC33_GetSpeedKmh();
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
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

  /** Configure the main internal regulator output voltage
  */
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL8;
  RCC_OscInitStruct.PLL.PLLDIV = RCC_PLL_DIV3;
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
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.ClockSpeed = 400000;
  hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief I2C2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C2_Init(void)
{

  /* USER CODE BEGIN I2C2_Init 0 */

  /* USER CODE END I2C2_Init 0 */

  /* USER CODE BEGIN I2C2_Init 1 */

  /* USER CODE END I2C2_Init 1 */
  hi2c2.Instance = I2C2;
  hi2c2.Init.ClockSpeed = 400000;
  hi2c2.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c2.Init.OwnAddress1 = 0;
  hi2c2.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c2.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c2.Init.OwnAddress2 = 0;
  hi2c2.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c2.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C2_Init 2 */

  /* USER CODE END I2C2_Init 2 */

}

/**
  * @brief TIM2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM2_Init(void)
{

  /* USER CODE BEGIN TIM2_Init 0 */

  /* USER CODE END TIM2_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};

  /* USER CODE BEGIN TIM2_Init 1 */

  /* USER CODE END TIM2_Init 1 */
  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 31;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 19999;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 1410;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_PWM_ConfigChannel(&htim2, &sConfigOC, TIM_CHANNEL_2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM2_Init 2 */

  /* USER CODE END TIM2_Init 2 */
  HAL_TIM_MspPostInit(&htim2);

}

/**
  * @brief TIM3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM3_Init(void)
{

  /* USER CODE BEGIN TIM3_Init 0 */

  /* USER CODE END TIM3_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};

  /* USER CODE BEGIN TIM3_Init 1 */

  /* USER CODE END TIM3_Init 1 */
  htim3.Instance = TIM3;
  htim3.Init.Prescaler = 31;
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = 19999;
  htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim3) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim3, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_Init(&htim3) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 1000;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM3_Init 2 */

  /* USER CODE END TIM3_Init 2 */
  HAL_TIM_MspPostInit(&htim3);

}

/**
  * @brief TIM4 Initialization Function
  * @param None
  * @retval None
  */

static void MX_TIM4_Init(void)
{

  /* USER CODE BEGIN TIM4_Init 0 */
/* Needs 490 Hz -> T = 2040 mcs	   */
/* used by vbolbat for esc control */
  /* USER CODE END TIM4_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};

  /* USER CODE BEGIN TIM4_Init 1 */

  /* USER CODE END TIM4_Init 1 */
  htim4.Instance = TIM4;
  htim4.Init.Prescaler = 31; //32MHz / 32 = 1MHz
  htim4.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim4.Init.Period = 2040; //1MHz / 2040 = 490 Hz
  htim4.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim4.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim4) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim4, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_Init(&htim4) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim4, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_PWM_ConfigChannel(&htim4, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM4_Init 2 */

  /* USER CODE END TIM4_Init 2 */
  HAL_TIM_MspPostInit(&htim4);

}

/**
  * @brief TIM5 Initialization Function
  * @param None
  * @retval None
  */
static void TIM5_Init(uint32_t timer_period_ms)
{

  /* USER CODE BEGIN TIM5_Init 0 */

  /* USER CODE END TIM5_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM5_Init 1 */

  /* USER CODE END TIM5_Init 1 */
  htim5.Instance = TIM5;
  htim5.Init.Prescaler = 31999;
  htim5.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim5.Init.Period = timer_period_ms - 1;
  htim5.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim5.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim5) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim5, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_UPDATE;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim5, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM5_Init 2 */

  /* USER CODE END TIM5_Init 2 */

}

/**
  * @brief TIM6 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM6_Init(void)
{

  /* USER CODE BEGIN TIM6_Init 0 */
  /* used by vbolbat for esc && mpu6050 data update */
  /* updates 5 times per sec */
  /* USER CODE END TIM6_Init 0 */

  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM6_Init 1 */

  /* USER CODE END TIM6_Init 1 */
  htim6.Instance = TIM6;
  htim6.Init.Prescaler = 31999;
  htim6.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim6.Init.Period = 199;
  htim6.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim6) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_UPDATE;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim6, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM6_Init 2 */

  /* USER CODE END TIM6_Init 2 */

}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
/*
static void MX_USART2_UART_Init(void)
{


  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }


} */

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1|GPIO_PIN_2|GPIO_PIN_12, GPIO_PIN_RESET);

  /*Configure GPIO pin : PA0 */
  GPIO_InitStruct.Pin = GPIO_PIN_0;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : PB1 PB2 PB12 */
  GPIO_InitStruct.Pin = GPIO_PIN_1|GPIO_PIN_2|GPIO_PIN_12;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI0_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI0_IRQn);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if(htim->Instance == TIM5) {
    	MeasureSpeedFC33_TIM_PeriodElapsedCallback();
    }
    if(htim->Instance == TIM6) {

    }
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if(GPIO_Pin == GPIO_PIN_0)
    {
    	MeasureSpeedFC33_GPIO_EXTI_Callback();
    }
}



/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  /*
   * vbolbat: Ошибки, игнорируемые Error_Handler должны использовать
   * структуру logged_errors (logger.h) для отслеживания их количества.
   * Список:
   * 1) HAL_UART_ErrorCallback()
   */
	#ifdef __DEBUG__
	printf("Error_Handler called!\n");
#endif
  __disable_irq();
#ifdef __LOGGING__
	save_log(ERROR_HANDLER_);
#endif
  //Останавливаем машинку.
  esc_struct.pwm_percent = 0;
  esc_update_pwm(&esc_struct);
  while (1)
  {
	  //vbolbat: можно запросить состояние машинки. Управление невозможно
	  //Блокирующее ожидание приема команды
	  HAL_UART_Receive(&huart1, &rx_byte, 1, HAL_MAX_DELAY);
	  parse_uart_message();
	  //vbolbat: ограниченная версия парсера
	  if (cmd_state.get_telemetry_flag) {
		  Send_Telemetry();
	      cmd_state.get_telemetry_flag = 0;
	  }
	  if (cmd_state.read_log_flag) {
		  send_log();
	      cmd_state.read_log_flag = 0;
	  }
  }
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
