#ifndef _ESC_VBOLBAT_CONTROL_
#define _ESC_VBOLBAT_CONTROL_

typedef unsigned char uint8_t;

/* API: */
typedef struct
{
	uint8_t direction;   //1 -> forward, 0 -> backward
	uint8_t pwm_percent; //from 0 to 100 %
	float current_speed; //m/s
} esc_t;

void esc_init(esc_t* pesc_t);
void esc_update_pwm(esc_t* pesc_t);
void calibrate_esc();
/*
 * Алгоритм работы:
 * 1) Объявите структуру esc_t в main.
 * 2) Вызовите функцию esc_init() для инициализации таймера(PWM) и структуры.
 * 3) В обработчике прерывания !!!(обновляем данные внутри структуры)!!! и
 * вызываем функцию esc_update_pwm() для обновления.
 */

#endif
