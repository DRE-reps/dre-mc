#include "logger.h"
#include "main.h"

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

#warning "What is sqn, addr & code in return message?? FIX CODE field???"
void send_log(void)
{
    uint8_t tx_pck[3 /*0xAC + 0x53 + LEN*/ + 4 /*LOG POINTER*/ + 3 /*SQN,ADDR,CODE*/ + LOGGING_BUF_SIZE /**/];
    tx_pck[0] = 0xAC;
    tx_pck[1] = 0x53;
    tx_pck[2] = 3 + 4 + LOGGING_BUF_SIZE; // LEN compute: SQN,ADDR,CODE + LOG POINTER + LOGGING BUF SIZE
    tx_pck[3] = 0x00; // SQN  ??
    tx_pck[4] = 0x01; // ADDR ??
    tx_pck[5] = 0x15; // CODE ??
    /*LOG POINTER*/
    //упаковываем uint32_t в uint8_t;
    //MSB first.
    uint8_t  plog_pointer[4] = {(log_pointer>>24)&0xFF,
    							(log_pointer>>16)&0xFF,
								(log_pointer>>8)&0xFF,
								(log_pointer>>0)&0xFF};
    tx_pck[6] = plog_pointer[0];
    tx_pck[7] = plog_pointer[1];
    tx_pck[8] = plog_pointer[2];
    tx_pck[9] = plog_pointer[3];
    /*LOGGER BUF*/
    for (int i = 0; i < LOGGING_BUF_SIZE; i++)
    {
    	tx_pck[10 + i] = log_buf[i];
    }
    /*COMPUTE CRC*/
    // Считаем CRC от  байт (начиная с LEN)
#warning "Есть ли ограничение на количество байт в crc? А ограничения на длину пакета?"
    uint8_t crc = Compute_CRC8(&tx_pck[2],
    		4 /*LEN + SQN + ADDR + CODE*/ + 4 /* LOG POINTER */
			+ LOGGING_BUF_SIZE /**/);
#warning "На что влияет таймаут в данном случае?"
    HAL_UART_Transmit(&huart1, tx_pck,
    		10 + LOGGING_BUF_SIZE,
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
