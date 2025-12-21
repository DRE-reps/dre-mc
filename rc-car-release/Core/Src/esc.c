#include "esc.h"
#include "main.h"
#include "stdio.h"
#include "logger.h"

#define MIN_PWM    1000//mcs machine goes back
#define MIDDLE_PWM 1500//mcs machine stands
#define MAX_PWM    2000//mcs machine goes forward

extern TIM_HandleTypeDef htim4;
extern void Error_Handler(void);
/*private funcs*/
static void CustomDelay20ms(void);

/*
 * Алгоритм работы:
 * 1) Объявите структуру esc_t в main.
 * 2) Вызовите функцию esc_init() для инициализации таймера(PWM) и структуры.
 * 3) В обработчике прерывания !!!(обновляем данные внутри структуры)!!! и
 * вызываем функцию esc_update_pwm() для обновления.
 */

void esc_init(esc_t* pesc_t)
{
#ifdef __LOGGING__
	save_log(ESC_INIT);
#endif
	pesc_t->direction = 1;   //1 -> forward, 0 -> backward
	pesc_t->pwm_percent = 0; //from 0 to 100 %
	pesc_t->current_speed = 0;
	HAL_TIM_PWM_Start(&htim4,TIM_CHANNEL_1);
	__HAL_TIM_SET_COMPARE(&htim4,TIM_CHANNEL_1,MIDDLE_PWM); //changes pulse parameter
}



void calibrate_esc()
{
#ifdef __LOGGING__
	save_log(CALIBRATE_ESC);
#endif
	__HAL_TIM_SET_COMPARE(&htim4,TIM_CHANNEL_1,MIDDLE_PWM);
	HAL_Delay(5000); //connect ESC during this delay

	__HAL_TIM_SET_COMPARE(&htim4,TIM_CHANNEL_1,MAX_PWM);
	HAL_Delay(2000); //MAX

	__HAL_TIM_SET_COMPARE(&htim4,TIM_CHANNEL_1,MIDDLE_PWM);
	HAL_Delay(2000); //ZERO

	__HAL_TIM_SET_COMPARE(&htim4,TIM_CHANNEL_1,MIN_PWM);
	HAL_Delay(20); //MIN

	__HAL_TIM_SET_COMPARE(&htim4,TIM_CHANNEL_1,MIDDLE_PWM);
	HAL_Delay(20); //ZERO

	__HAL_TIM_SET_COMPARE(&htim4,TIM_CHANNEL_1,MIN_PWM);
	HAL_Delay(2000); //MIN

	__HAL_TIM_SET_COMPARE(&htim4,TIM_CHANNEL_1,MIDDLE_PWM);
	HAL_Delay(2000); //ZERO

	//SETUP COMPLETE
}

void esc_update_pwm(esc_t* pesc_t)
{
#ifdef __LOGGING__
	save_log(ESC_UPDATE_PWM);
#endif
	if (pesc_t->pwm_percent > 100)
	{
		pesc_t->pwm_percent = 100;
	}
	static unsigned int temp_var = 0;
	if (pesc_t->direction) //forward
	{
			if (pesc_t->pwm_percent > 0)
			{
				temp_var = MIDDLE_PWM + pesc_t->pwm_percent * 5;
			}
			else
			{
				temp_var = MIDDLE_PWM;
			}
		//}
		__HAL_TIM_SET_COMPARE(&htim4,TIM_CHANNEL_1,temp_var);
	}
	else                   //backward
	{

			if (pesc_t->pwm_percent > 0)
			{
				if (temp_var == MIDDLE_PWM)
					//если в прошлый момент времени машинка была в нейтральном положении
					//необходимо сделать кратковременное переключение для "выбора" заднего хода
				{
					__HAL_TIM_SET_COMPARE(&htim4,TIM_CHANNEL_1,MIN_PWM);
					CustomDelay20ms(); //холостой цикл
					__HAL_TIM_SET_COMPARE(&htim4,TIM_CHANNEL_1,MIDDLE_PWM);
					CustomDelay20ms();
					//HAL_Delay(20);
				}
				temp_var = MIDDLE_PWM - (pesc_t->pwm_percent * 5);
			}
			else
			{
				temp_var = MIDDLE_PWM;
			}
		//}
		__HAL_TIM_SET_COMPARE(&htim4,TIM_CHANNEL_1,temp_var);
	}
#ifdef __DEBUG__
	//printf("\n current_pwm = %d \n",temp_var);
	//printf("\n current_DIR = %d \n",pesc_t->direction);
#endif
}

//Custom 32 MHz delay
static void CustomDelay20ms(void)
{
	volatile uint32_t var = 0;
	for (;var < 320000*2/10; var++)
	{
		;
	}
}


