#include "main.h"
#include "esc.h"
/* vbolbat includes */
#include "logger.h"
#include "usart1_callbacks.h"

static uint8_t Compute_CRC8(uint8_t *data, uint16_t length);
Command_State_t cmd_state = {0}; /* flags struct */
/* extern vars */
extern esc_t esc_struct;
extern UART_HandleTypeDef huart1;
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

        packet[packet_idx++] = rx_byte;

        if (packet_idx == 1 && packet[0] != 0xAC) packet_idx = 0;
        else if (packet_idx == 2 && packet[1] != 0x53) packet_idx = 0;
        else if (packet_idx >= 3) {
            uint8_t len = packet[2];
            if (packet_idx == (len + 3)) {
                // Проверка CRC8
                if (Compute_CRC8(&packet[2], len) == packet[packet_idx - 1]) {

                    uint8_t cmd_code = packet[5]; // Позиция CODE
                    uint8_t payload  = packet[6]; // Первый байт DATA

                    switch (cmd_code) {
						case 0x02: // CODE: Установка угла
							// Приводим payload к знаковому типу, чтобы 0xFF воспринималось как -1
							cmd_state.wheel_angle = (int8_t)packet[6];
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
                        case 0x30: // Автопарковка
                            cmd_state.start_autopark_flag = (packet[6] > 0); // 1 если байт > 0
                            break;
                        default:
                        	break;
                    }
                }
                packet_idx = 0;
            }
        }
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
#warning "Необходимо какое нибудь исключение для этого случая. Юзер должен знать об этом."
    	if (HAL_UART_Receive_IT(usart, &rx_byte, 1) != HAL_OK)
    	{
    		Error_Handler();
    	}
    }
}

void Send_Telemetry(void) {
    uint8_t tx_pck[8];
    tx_pck[0] = 0xAC;
    tx_pck[1] = 0x53;
    tx_pck[2] = 0x05; // LEN: SQN(1) + ADDR(1) + CODE(1) + DATA(2) = 5
    tx_pck[3] = 0x00; // SQN
    tx_pck[4] = 0x01; // ADDR
    tx_pck[5] = 0x15; // CODE
    tx_pck[6] = (uint8_t)esc_struct.current_speed;
    tx_pck[7] = (uint8_t)esc_struct.pwm_percent; // Временно вместо RPM

    // Считаем CRC от 5 байт (начиная с LEN)
    uint8_t crc = Compute_CRC8(&tx_pck[2], 6); // LEN + данные

    HAL_UART_Transmit(&huart1, tx_pck, 8, 10);
    HAL_UART_Transmit(&huart1, &crc, 1, 10); // Отправляем CRC 9-м байтом
}


static uint8_t Compute_CRC8(uint8_t *data, uint16_t length)
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




