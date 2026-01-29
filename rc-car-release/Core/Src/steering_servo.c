#include "steering_servo.h"

#define SERVO_TIMER              htim3 // таймер, который генерирует ШИМ
#define SERVO_CHANNEL            TIM_CHANNEL_1
#define SERVO_MIN_PULSE          400   // угол поворота 0°
#define SERVO_MAX_PULSE          2380  // угол поворота 180°

extern TIM_HandleTypeDef htim3;

void SteeringServo_Init()
{
	#ifdef __LOGGING__
		save_log(SteeringServoInit);
	#endif

	TIM_OC_InitTypeDef sConfigOC = {0};
	sConfigOC.OCMode = TIM_OCMODE_PWM1;
	sConfigOC.Pulse = (SERVO_MIN_PULSE + SERVO_MAX_PULSE) / 2;
	sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
	sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;

	HAL_TIM_PWM_ConfigChannel(&SERVO_TIMER, &sConfigOC, SERVO_CHANNEL);
	HAL_TIM_PWM_Start(&SERVO_TIMER, SERVO_CHANNEL);
}

void SteeringServo_SetAngle(uint16_t angle)
{
	#ifdef __LOGGING__
		save_log(SteeringServoSetAngle);
	#endif
	if (angle > 130) angle = 130;
	if (angle < 50) angle = 50;

	uint32_t pulse = SERVO_MIN_PULSE + ((SERVO_MAX_PULSE - SERVO_MIN_PULSE) * angle) / 180; // рассчитываем ширину активного сигнала

	__HAL_TIM_SET_COMPARE(&SERVO_TIMER, SERVO_CHANNEL, pulse); // меняем параметр ШИМ
}


