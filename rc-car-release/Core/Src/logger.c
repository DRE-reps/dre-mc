#include "logger.h"
#include "main.h"
//будет выдаваться при вызове Error_Handler(); и по запросу
uint8_t  log_buf[LOGGING_BUF_SIZE]; //хранилище логов
uint32_t log_pointer = 0;           //необходим для однозначного определения хронологического порядка записей

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
	/*EMPTY now*/
}
