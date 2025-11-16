#include "measure_speed_FC33.h"

static const float PI = 3.14159265358979323846f;

extern TIM_HandleTypeDef htim5;
static volatile uint32_t pulses_count = 0;
static volatile uint8_t pulses_per_revolution = 1;
static volatile uint32_t last_pulses = 0;
static volatile float last_speed_сmps = 0.0f;
static float wheel_diameter_cm = 6.5f;
static uint32_t timer_period_ms = 100;

//static void TIM5_Init(uint32_t timer_period_ms)
//{
//
//  /* USER CODE BEGIN TIM5_Init 0 */
//
//  /* USER CODE END TIM5_Init 0 */
//
//  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
//  TIM_MasterConfigTypeDef sMasterConfig = {0};
//
//  /* USER CODE BEGIN TIM5_Init 1 */
//
//  /* USER CODE END TIM5_Init 1 */
//  htim5.Instance = TIM5;
//  htim5.Init.Prescaler = 31999;
//  htim5.Init.CounterMode = TIM_COUNTERMODE_UP;
//  htim5.Init.Period = timer_period_ms - 1;
//  htim5.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
//  htim5.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
//  if (HAL_TIM_Base_Init(&htim5) != HAL_OK)
//  {
//    Error_Handler();
//  }
//  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
//  if (HAL_TIM_ConfigClockSource(&htim5, &sClockSourceConfig) != HAL_OK)
//  {
//    Error_Handler();
//  }
//  sMasterConfig.MasterOutputTrigger = TIM_TRGO_UPDATE;
//  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
//  if (HAL_TIMEx_MasterConfigSynchronization(&htim5, &sMasterConfig) != HAL_OK)
//  {
//    Error_Handler();
//  }
//  /* USER CODE BEGIN TIM5_Init 2 */
//
//  /* USER CODE END TIM5_Init 2 */
//
//}

//static void GPIO_EXTI_Init()
//{
//  GPIO_InitTypeDef GPIO_InitStruct = {0};
//  __HAL_RCC_GPIOA_CLK_ENABLE();
//  __HAL_RCC_GPIOC_CLK_ENABLE();
//  __HAL_RCC_SYSCFG_CLK_ENABLE();
//
//  GPIO_InitStruct.Pin = GPIO_PIN_0;
//  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
//  GPIO_InitStruct.Pull = GPIO_NOPULL;
//  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
//
//  HAL_NVIC_SetPriority(EXTI2_IRQn, 0, 0);
//  HAL_NVIC_EnableIRQ(EXTI2_IRQn);
//}


void MeasureSpeedFC33_Init(float wheel_diameter_сm_i, uint32_t timer_period_ms_i, uint8_t pulses_per_revolution_i)
{
  wheel_diameter_cm = wheel_diameter_сm_i;
  timer_period_ms = timer_period_ms_i;
  pulses_per_revolution = pulses_per_revolution_i;
  pulses_count = 0;
  last_pulses = 0;
  last_speed_сmps = 0.0f;

//  TIM5_Init(timer_period_ms);
//  GPIO_EXTI_Init();
}

uint32_t MeasureSpeedFC33_GetRPM()
{
  return (((float)last_pulses / pulses_per_revolution) / ((float)timer_period_ms / 60000.0f));
}

float MeasureSpeedFC33_GetSpeedKmh()
{
  return last_speed_сmps * 0.036f;
}

//void EXTI2_IRQHandler() { HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_2); }
//void TIM5_IRQHandler() { HAL_TIM_IRQHandler(&htim5); }

static uint32_t last_exti_time = 0;
void MeasureSpeedFC33_GPIO_EXTI_Callback()
{
	uint32_t now = HAL_GetTick();
	if (now - last_exti_time > 1) // Защита от дребезга
	{
		pulses_count++;
		last_exti_time = now;
	}
}


void MeasureSpeedFC33_TIM_PeriodElapsedCallback()
{
	last_pulses = pulses_count;
	float timer_period_s = (float)timer_period_ms / 1000.0f;
	float circumference = PI * wheel_diameter_cm;
	last_speed_сmps = (((float)pulses_count / pulses_per_revolution) / timer_period_s) * circumference;
	pulses_count = 0;
}
