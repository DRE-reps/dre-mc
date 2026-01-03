#pragma once
typedef unsigned char uint8_t;
#include "main.h"
//vars
/* update logger_init() && logger_send_errors_stat() func when touching this */
typedef struct
{
	uint32_t HAL_UART_ErrorCallback_errors;
} logged_errors;
//funcs
void save_log(uint8_t status);
void send_log(void);
void logger_init(void);
void logger_send_errors_stat(void);

#ifdef __LOGGING__
#define LOGGING_BUF_SIZE 128U //vbolbat: keep it less then 200 for safety cause of send_log() realization
#define GET_KALMAN_SPEED 0x00
#define SET_BUFFER 0x01
#define FLOAT2STR 0x02
#define HAL_UART_RXCPLTCALLBACK_ 0x03
#define HAL_UART_TXCPLTCALLBACK_ 0x04
#define HAL_UART_ERRORCALLBACK_ 0x05
#define ESC_INIT 0x06
#define CALIBRATE_ESC 0x07
#define ESC_UPDATE_PWM 0x08
#define MPU6050_GET_ACCELERATION 0x09
#define MPU6050_INIT 0x0A
#define MPU6050_READ_ALL 0x0B
#define ERROR_HANDLER_ 0x0C
//Misha-Ovs VL53L0X.c macro:
#define INIT_LOG 0x0D
#define SET_CONFIDENCE 0x0E
#define VLX_PULSE_PERIOD 0x0F
#define VLX_TIME_BETWEEN_MEAS 0x10
#define VLX_READ_DATA 0x11
//Alexander-Chv measure_speed_FC33.c macro:

//vbolbat again:
#define PARSE_UART__      0x12
#define CUSTOM_DELAY_20MS 0x13

#endif
