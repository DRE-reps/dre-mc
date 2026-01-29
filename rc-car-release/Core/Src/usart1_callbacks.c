#include "main.h"
#include "VL53L0X.h"
#include "steering_servo.h"
#include "measure_speed_FC33.h"
/* vbolbat includes */
#include "logger.h"
#include "usart1_callbacks.h"
#include "esc.h"
#include "converters.h"
#include <string.h>
#include "mpu6050.h"

Command_State_t cmd_state = {0}; /* flags struct */
/* vars */
uint8_t sync_count = 0;
/* extern vars */
extern esc_t esc_struct;
extern UART_HandleTypeDef huart1;
extern uint8_t speed_calibration_buffer[2];
extern logged_errors logged_errors_obj;
extern MPU6050_t mpu6050_struct;
extern VL53L0X_Dev_t sensor1;
extern VL53L0X_Dev_t sensor2;
extern statInfo_t_VL53L0X distanceStr1;
extern statInfo_t_VL53L0X distanceStr2;
extern TIM_HandleTypeDef htim6;
/* private vars */
uint8_t rx_byte; // Принимаем по одному байту
static uint8_t packet[256]; // Буфер для сборки пакета
static uint8_t packet_idx = 0;

/* NEW PARSER */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    if (huart->Instance == USART1) {

	#ifdef __LOGGING__
    	save_log(HAL_UART_RXCPLTCALLBACK_);
	#endif

    	parse_uart_message();
        // USART1 завершил прием данных
    	#ifdef __DEBUG__
        	printf("RX END\n");
		#endif
        HAL_UART_Receive_IT(huart, &rx_byte, 1);
    }
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *usart)
{
  if (usart->Instance == USART1)
  {
#ifdef __LOGGING__
	save_log(HAL_UART_TXCPLTCALLBACK_);
#endif
    // USART1 завершил отправку данных
#ifdef __DEBUG__
	  printf("TX END\n");
#endif
  }
}

// Для обработки ошибок
void HAL_UART_ErrorCallback(UART_HandleTypeDef *usart) {
    if (usart->Instance == USART1) {
#ifdef __LOGGING__
	save_log(HAL_UART_ERRORCALLBACK_);
#endif
#ifdef __DEBUG__
        printf("UART_ERROR!!\r\n");
#endif
        //В случае ошибки прием возобновляется.
        logged_errors_obj.HAL_UART_ErrorCallback_errors++;
    	if (HAL_UART_Receive_IT(usart, &rx_byte, 1) != HAL_OK)
    	{
    		Error_Handler();
    	}
    }
}

void Send_Telemetry(void) {
    uint16_t FC33_RPM = (uint16_t)MeasureSpeedFC33_GetRPM();
    //packing uint16_t to uint8_t
    uint8_t FC33_RPM_send[2] = {(FC33_RPM >> 8)& 0xFF, (FC33_RPM & 0xFF)};
    //distance part
    uint16_t dist1_mm = readRangeSingleMillimeters(&sensor1, &distanceStr1);
    //packing uint16_t to uint8_t
    uint8_t dist1_mm_send[2] = {(dist1_mm >> 8)& 0xFF, (dist1_mm & 0xFF)};
	uint16_t dist2_mm = dist1_mm;//readRangeSingleMillimeters(&sensor2, &distanceStr2);
	//packing uint16_t to uint8_t
	uint8_t dist2_mm_send[2] = {(dist2_mm >> 8)& 0xFF, (dist2_mm & 0xFF)};
    float speed_kmh = MeasureSpeedFC33_GetSpeedKmh();
    float AX_mpu6050 = MPU6050_get_acceleration(&hi2c2,&mpu6050_struct); /* module of AX */
    uint8_t speed_khm_1 = (*((uint32_t *)&speed_kmh) >> 24) & 0xFF;
    uint8_t speed_khm_2 = (*((uint32_t *)&speed_kmh) >> 16) & 0xFF;
    uint8_t speed_khm_3 = (*((uint32_t *)&speed_kmh) >>  8) & 0xFF;
    uint8_t speed_khm_4 = (*((uint32_t *)&speed_kmh) >>  0) & 0xFF;

    uint8_t AX_mpu6050_1 = (*((uint32_t *)&AX_mpu6050) >> 24) & 0xFF;
    uint8_t AX_mpu6050_2 = (*((uint32_t *)&AX_mpu6050) >> 16) & 0xFF;
    uint8_t AX_mpu6050_3 = (*((uint32_t *)&AX_mpu6050) >>  8) & 0xFF;
    uint8_t AX_mpu6050_4 = (*((uint32_t *)&AX_mpu6050) >>  0) & 0xFF;
	//packet part
    uint8_t tx_pck[256];
    tx_pck[0] = 0xAC;
    tx_pck[1] = 0x53;
    tx_pck[2] = 9 + 8; // LEN: 9 + floats
    tx_pck[3] = 0x00; // SQN
    tx_pck[4] = 0x01; // ADDR
    tx_pck[5] = 0x15; // CODE
    tx_pck[6] = FC33_RPM_send[0];  //MSB
    tx_pck[7] = FC33_RPM_send[1];  //LSB
    tx_pck[8] = dist1_mm_send[0];  //MSB
    tx_pck[9] = dist1_mm_send[1];  //LSB
    tx_pck[10] = dist2_mm_send[0]; //MSB
    tx_pck[11] = dist2_mm_send[1]; //LSB

    tx_pck[12] = speed_khm_1;
    tx_pck[13] = speed_khm_2;
    tx_pck[14] = speed_khm_3;
    tx_pck[15] = speed_khm_4;

    tx_pck[16] = AX_mpu6050_1;
    tx_pck[17] = AX_mpu6050_2;
    tx_pck[18] = AX_mpu6050_3;
    tx_pck[19] = AX_mpu6050_4;

    // Считаем CRC (LEN включительно + SQN,ADDR и тд)
    uint8_t crc = Compute_CRC8(&tx_pck[2], tx_pck[2]);
    /*send PACK*/
    HAL_UART_Transmit(&huart1, tx_pck, 19, HAL_MAX_DELAY);
    /*send CRC*/
    HAL_UART_Transmit(&huart1, &crc, 1, HAL_MAX_DELAY);
}


uint8_t Compute_CRC8(uint8_t *data, uint16_t length)
{
    uint8_t crc = 0x00;
    for (uint16_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x80) {
                crc = (crc << 1) ^ 0x31; // Полином 0x31 (x8 + x5 + x4 + 1)
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

void process_parser_flags(void)
{
//vbolbat: no logging needed, func used in while(1) cycle...
    if (cmd_state.set_angle_flag) {
    	SteeringServo_SetAngle((uint16_t)cmd_state.wheel_angle);
        cmd_state.set_angle_flag = 0;
    }
    if (cmd_state.set_pwm_flag) {
        esc_struct.pwm_percent = cmd_state.esc_pwm;
        esc_update_pwm(&esc_struct);
        __HAL_TIM_SET_COUNTER(&htim6, 0);
        cmd_state.set_pwm_flag = 0;
    }
    if (cmd_state.set_direction_flag) {
    	esc_struct.direction = cmd_state.direction;
    	esc_update_pwm(&esc_struct);
        cmd_state.set_direction_flag = 0;
    }
    if (cmd_state.get_telemetry_flag) {
        Send_Telemetry();
        __HAL_TIM_SET_COUNTER(&htim6, 0);
        cmd_state.get_telemetry_flag = 0;
    }
    if (cmd_state.read_log_flag) {
        send_log();
        cmd_state.read_log_flag = 0;
    }
    if (cmd_state.read_errors_stat_flag) {
        //Не планирую использовать. Отменено.
    	logger_send_errors_stat(); //not working
    	cmd_state.read_errors_stat_flag = 0;
    }
}

void parse_uart_message(void)
{
#ifdef __LOGGING__
	save_log(PARSE_UART__);
#endif

    packet[packet_idx++] = rx_byte;

    if (packet_idx == 1 && packet[0] != 0xAC) packet_idx = 0;
    else if (packet_idx == 2 && packet[1] != 0x53) packet_idx = 0;
    else if (packet_idx >= 3) {
        uint8_t len = packet[2];
        if (packet_idx == (len + 4)) {
            // Проверка CRC8
            if (Compute_CRC8(&packet[2], (uint16_t)len) == packet[packet_idx - 1]) {

                uint8_t cmd_code = packet[5]; // Позиция CODE
                uint8_t payload  = packet[6]; // Первый байт DATA

                switch (cmd_code) {
					case 0x02: // CODE: Установка угла
						cmd_state.wheel_angle = (uint8_t)packet[6];
						cmd_state.set_angle_flag = 1;
						break;
                    case 0x03: // PWM
                        cmd_state.esc_pwm = payload;
                        cmd_state.set_pwm_flag = 1;
                        break;
                    case 0x01: // Направление
                        cmd_state.direction = payload;
                        cmd_state.set_direction_flag = 1;
                        break;
                    case 0x15: // Запрос телеметрии
                        cmd_state.get_telemetry_flag = 1;
                        break;
                    case 0x20: // Прочитать лог
                        cmd_state.read_log_flag = 1;
                        break;
                    case 0x30: // Отменено
                    	cmd_state.read_errors_stat_flag = 1;
                        break;
                    default:
                    	break;
                }
            }
            packet_idx = 0;
        }
    }
}

