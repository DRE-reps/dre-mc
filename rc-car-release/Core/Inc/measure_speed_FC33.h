#include "stm32l1xx_hal.h"
void MeasureSpeedFC33_Init(float wheel_diameter_сm_i, uint32_t timer_period_i, uint8_t pulses_per_revolution);
uint32_t MeasureSpeedFC33_GetRPM();
float MeasureSpeedFC33_GetSpeedKmh();

void MeasureSpeedFC33_GPIO_EXTI_Callback();
void MeasureSpeedFC33_TIM_PeriodElapsedCallback();

