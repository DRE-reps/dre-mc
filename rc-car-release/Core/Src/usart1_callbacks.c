#include "main.h"
#include "measure_speed_FC33.h"
#include "VL53L0X.h"
/* vbolbat includes */
#include "logger.h"
#include "esc.h"
#include "mpu6050.h"
#include "converters.h"
/* system includes */
#include "string.h"

static void set_buffer(uint8_t* buff, const char* string1, const char* string2, const char* string3, int size1, int size2, int size3);

extern UART_HandleTypeDef huart1;
extern uint8_t usart1_tx_buff[UART_TXBUF_SIZE];
extern uint8_t usart1_rx_buff[UART_RXBUF_SIZE];
//extern esc_t esc_struct;
extern MPU6050_t mpu6050_struct;
//extern uint8_t speed_calibration_buffer[2]; //Глобальная переменная для передачи скорости в колбек таймера esc
extern uint8_t  log_buf[LOGGING_BUF_SIZE];
extern uint32_t log_pointer;
/*temp var's*/
extern uint8_t current_pwm;
extern uint8_t current_direction;

/* VL51LOX (Dual Sensor Setup) */
// Теперь ссылаемся на объекты, объявленные в main.c
extern VL53L0X_Dev_t sensor1;
extern VL53L0X_Dev_t sensor2;
extern statInfo_t_VL53L0X distanceStr1;
extern statInfo_t_VL53L0X distanceStr2;

/*
 * CALLBACK ВЫЗЫВАЕТСЯ ТОЛЬКО ПО ЗАПОЛНЕНИЮ БУФФЕРА
 */
//  SYNT:
//  HAL_UART_Receive_IT(&usart1, usart1_rx_buff, UART_RXBUF_SIZE);
//  HAL_UART_Transmit_IT(&usart1, usart1_tx_buff, UART_TXBUF_SIZE);

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *usart)
{
  if (usart->Instance == USART1)
  {
#ifdef __LOGGING__
	save_log(HAL_UART_RXCPLTCALLBACK_);
#endif
	//Логика (strncmp -> Сравнение до \0) <6 symbols!>
	if (strncmp((const char*)usart1_rx_buff, "FC__MS" , UART_RXBUF_SIZE) == 0) {
		//set_buffer(usart1_tx_buff,"\r\nFC_33_MS: ",float2str(MeasureSpeedFC33_GetSpeedKmh()/3.6f,4),"\r\n",12,7,2);
		set_buffer(usart1_tx_buff,float2str(MeasureSpeedFC33_GetSpeedKmh()/3.6f,4),NULL,NULL,7,0,0);
	}
	else if (strncmp((const char*)usart1_rx_buff, "FC_RPM", UART_RXBUF_SIZE) == 0) {
		uint32_t var = MeasureSpeedFC33_GetRPM();
		if (uint_to_str(usart1_tx_buff, UART_TXBUF_SIZE, var) == 0 /* RESP_OK */)
		{
			;//В этот момент буффер уже заполнен, никаких действий не требуется
		}
		else /* RESP_ERR */
		{
			char var = UART_COMMAND_ERR;
			char* pvar = &var;
			set_buffer(usart1_tx_buff,pvar,NULL,NULL,1,0,0);
		}
	}
	else if (strncmp((const char*)usart1_rx_buff, "MPU_AX", UART_RXBUF_SIZE) == 0) { //mpu AX accceleration
		float var = MPU6050_get_acceleration(&hi2c2,&mpu6050_struct);
		//set_buffer(usart1_tx_buff,"\r\nMPU_AX: ",float2str(var,4),"\r\n",10,7,2); //in m per sec
		set_buffer(usart1_tx_buff,float2str(var,4),NULL,NULL,7,0,0);
	}
	else if (strncmp((const char*)usart1_rx_buff, "KSPEED", UART_RXBUF_SIZE) == 0) { //kalman speed
		//set_buffer(usart1_tx_buff,"\r\nNo realization!","\r","\n",17,1,1);
		char var = UART_COMMAND_ERR;
		char* pvar = &var;
		set_buffer(usart1_tx_buff,pvar,NULL,NULL,1,0,0);
	}
	else if (strncmp((const char*)usart1_rx_buff, "SPW", 3) == 0) { //set PWM percent
		//Сравнивает только первые 3 символов, дальше парсим
		//ПРИМЕР ВВОДА: SPW100 -> установить 100%
		//ПРИМЕР ВВОДА: SPW050 -> установить  50%
		int a = (usart1_rx_buff[3] - '0')*100;
		int b = (usart1_rx_buff[4] - '0')*10;
		int c = (usart1_rx_buff[5] - '0')*1;
		if (a+b+c <= 100 && a+b+c >= 0)
		{
			current_pwm = a+b+c;
			//set_buffer(usart1_tx_buff,"\r\nOK!","\r","\n",5,1,1);
			char var = UART_COMMAND_OK;
			char* pvar = &var;
			set_buffer(usart1_tx_buff,pvar,NULL,NULL,1,0,0);
		}
		else
		{
			//set_buffer(usart1_tx_buff,"\r\nError in command!","\r","\n",19,1,1);
			char var = UART_COMMAND_ERR;
			char* pvar = &var;
			set_buffer(usart1_tx_buff,pvar,NULL,NULL,1,0,0);
		}
	}
	else if (strncmp((const char*)usart1_rx_buff, "SDRCN", 5) == 0) { //set DIRECTION of machine
		//Сравнивает только первые 5 символов, дальше парсим
		//ПРИМЕР ВВОДА: SDRCN1 -> движение вперед
		//ПРИМЕР ВВОДА: SDRCN0 -> движение назад
		if ((usart1_rx_buff[5] - '0') == 1)
		{
			current_direction = 1;
			//set_buffer(usart1_tx_buff,"\r\nOK!","\r","\n",5,1,1);
			char var = UART_COMMAND_OK;
			char* pvar = &var;
			set_buffer(usart1_tx_buff,pvar,NULL,NULL,1,0,0);
		}
		else if ((usart1_rx_buff[5] - '0') == 0 ) //ACTIVATING REVERSE
		{
			current_direction = 0;
			//set_buffer(usart1_tx_buff,"\r\nOK!","\r","\n",5,1,1);
			char var = UART_COMMAND_OK;
			char* pvar = &var;
			set_buffer(usart1_tx_buff,pvar,NULL,NULL,1,0,0);
		}
		else
		{
			//set_buffer(usart1_tx_buff,"\r\nError in command!","\r","\n",19,1,1);
			char var = UART_COMMAND_ERR;
			char* pvar = &var;
			set_buffer(usart1_tx_buff,pvar,NULL,NULL,1,0,0);
		}
	}
	else if (strncmp((const char*)usart1_rx_buff, "SEELOG", UART_RXBUF_SIZE) == 0)
	{
		//дамп logger-a
#warning "No realization!"
	}
	else if (strncmp((const char*)usart1_rx_buff, "SEERAN", UART_RXBUF_SIZE) == 0)
	{
		// Чтение дальномеров
		uint16_t dist1_raw_mm = readRangeSingleMillimeters(&sensor1, &distanceStr1);
		uint16_t dist2_raw_mm = readRangeSingleMillimeters(&sensor2, &distanceStr2);

		// Конвертация (исправлена логика с 3.5 - теперь для двух)
		float distance1 = (float)dist1_raw_mm / 10.0f - 3.5f;
		float distance2 = (float)dist2_raw_mm / 10.0f - 3.5f;

		// Если датчики вернули ошибку (65535 или 8190/8191 обычно), можно обработать это.
		// Здесь просто выводим как есть.

		// Формируем строку вида "XX.XXXX YY.YYYY"
		// Функция set_buffer принимает 3 строки.
		// Используем временный буфер для склейки, если set_buffer не переписать.
		// Но float2str возвращает указатель на статический буфер внутри себя?
		// ОБЫЧНО float2str НЕ реентерабельна!
		// Нужно проверить реализацию float2str. Если она использует static char buf[],
		// то второй вызов перезапишет первый до вызова set_buffer.
		// Предположим худшее и скопируем строки.

		char d1_str[16];
		char d2_str[16];

		char* ptr1 = float2str(distance1, 4);
		strncpy(d1_str, ptr1, 15);

		char* ptr2 = float2str(distance2, 4);
		strncpy(d2_str, ptr2, 15);

		// Разделитель - пробел
		set_buffer(usart1_tx_buff, d1_str, " ", d2_str, strlen(d1_str), 1, strlen(d2_str));
	}
	else
	{
		//error in command;
		char var = UART_COMMAND_ERR;
		char* pvar = &var;
		set_buffer(usart1_tx_buff,pvar,NULL,NULL,1,0,0);
	}
	//Отправляем результат команды
	if (HAL_UART_Transmit_IT(&huart1, usart1_tx_buff, UART_TXBUF_SIZE) != HAL_OK)
	{
		Error_Handler();
	}
	//Обнуляем буфер
	for (int i = 0; i < UART_RXBUF_SIZE;i++)
	{
		*(usart1_rx_buff+i) = 0;
	}
    // USART1 завершил прием данных
#ifdef __DEBUG__
	printf("RX END\n");
#endif
	if (HAL_UART_Receive_IT(&huart1, usart1_rx_buff, UART_RXBUF_SIZE) != HAL_OK)
	{
		Error_Handler();
	}
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
    	if (HAL_UART_Receive_IT(&huart1, usart1_rx_buff, UART_RXBUF_SIZE) != HAL_OK)
    	{
    		Error_Handler();
    	}
        //Error_Handler();
    }
}

static void set_buffer(uint8_t* buff, const char* string1, const char* string2, const char* string3, int size1, int size2, int size3)
{
#ifdef __LOGGING__
	save_log(SET_BUFFER);
#endif
	//clear the buff before set
	for (int i = 0; i < UART_TXBUF_SIZE;i++)
	{
		*(buff+i) = 0;
	}
	if (size1 + size2 + size3 > UART_TXBUF_SIZE)
	{
#ifdef __DEBUG__
		printf("set_buffer error! \r\n");
#endif
		Error_Handler();
	}
	int i;
	for (i = 0; i < (size1); i++)
	{
		buff[i] = *(string1 + i);
	}
	for (; i < (size1+size2); i++)
	{
		buff[i] = *(string2 + i - size1);
	}
	for (; i < (size1+size2+size3); i++)
	{
		buff[i] = *(string3 + i - size1 - size2);
	}
}
