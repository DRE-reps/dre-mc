#pragma once

typedef struct {
		uint8_t set_angle_flag;
		uint8_t set_pwm_flag;
		uint8_t set_direction_flag;
		uint8_t get_telemetry_flag;
		uint8_t read_log_flag;
		uint8_t start_autopark_flag;

		int8_t  wheel_angle;
		uint8_t esc_pwm;
		uint8_t direction;
		uint16_t current_rpm;   // Для телеметрии
		uint8_t  current_speed; // Для телеметрии
} Command_State_t;

void Send_Telemetry(void);
