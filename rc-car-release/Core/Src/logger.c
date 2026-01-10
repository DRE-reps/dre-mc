#include "logger.h"
#include "main.h"
#include "usart1_callbacks.h"

uint8_t  log_buf[LOGGING_BUF_SIZE]; //хранилище логов
uint32_t log_pointer = 0;           //необходим для однозначного определения хронологического порядка записей
logged_errors logged_errors_obj;    //подсчет ошибок, не вызывающих Error_Handler;

extern UART_HandleTypeDef huart1;

void save_log(uint8_t status)
{
	if (log_pointer > (LOGGING_BUF_SIZE - 1))
	{
		log_pointer = 0;
	}
	//сохраняем статус в ячейку
	*(log_buf + log_pointer) = status;
	log_pointer++;
}

void send_log(void)
{
	uint8_t temp_log_buf[LOGGING_BUF_SIZE]; //Мы восстановим хронологический порядок событй в буфере:
	//Младшие байты - новые. Log pointer не придется отправлять.
	int iterator_for_new_buf = 0;
	for (uint32_t i = log_pointer; i >= 0; i--)
	{
		temp_log_buf[iterator_for_new_buf] = log_buf[i];
		iterator_for_new_buf++;
	}
	for (uint32_t i = (LOGGING_BUF_SIZE - 1); i > log_pointer; i--)
	{
		temp_log_buf[iterator_for_new_buf] = log_buf[i];
		iterator_for_new_buf++;
	}
    uint8_t tx_pck[3 /*0xAC + 0x53 + LEN*/ + 3 /*SQN,ADDR,CODE*/ + LOGGING_BUF_SIZE /**/];
    tx_pck[0] = 0xAC;
    tx_pck[1] = 0x53;
    tx_pck[2] = 3 + LOGGING_BUF_SIZE; // LEN compute: SQN,ADDR,CODE + LOGGING BUF SIZE
    tx_pck[3] = 0x00; // SQN  ??
    tx_pck[4] = 0x01; // ADDR ??
    tx_pck[5] = 0x20; // CODE ??
    /*LOGGER BUF*/
    for (int i = 0; i < LOGGING_BUF_SIZE; i++)
    {
    	tx_pck[6 + i] = temp_log_buf[i];
    }
    /*COMPUTE CRC*/
    // Считаем CRC от  байт (начиная с LEN)
    uint8_t crc = Compute_CRC8(&tx_pck[2],
    		4 /*LEN + SQN + ADDR + CODE*/
			+ LOGGING_BUF_SIZE /**/);
    HAL_UART_Transmit(&huart1, tx_pck,
    		6 + LOGGING_BUF_SIZE,
			HAL_MAX_DELAY); // Отправляем лог
    HAL_UART_Transmit(&huart1, &crc, 1, HAL_MAX_DELAY); // Отправляем CRC
}

void logger_init(void)
{
	logged_errors_obj.HAL_UART_ErrorCallback_errors = 0;
}

void logger_send_errors_stat(void)
{
	//no realization, do we need this?
    //uint8_t tx_pck[];
}
