#pragma once
typedef unsigned char uint8_t;
//глобальные переменные в logger.c
void save_log(uint8_t status);

#define __LOGGING__
#ifdef __LOGGING__
#define LOGGING_BUF_SIZE 128U
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



#endif
