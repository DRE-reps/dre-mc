/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32l1xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */
typedef struct {
    uint8_t stage;           // Текущий этап (1-10)
    uint8_t s1_init_ok;      // 1 если датчик 1 инициализирован
    uint8_t s1_addr_ok;      // 1 если адрес датчика 1 изменен и проверен
    uint8_t s2_init_ok;      // 1 если датчик 2 инициализирован
    uint16_t last_line;      // Последняя пройденная строка кода
    HAL_StatusTypeDef last_i2c_status; // Статус последней операции I2C
    uint8_t last_reg;        // Последний регистр, к которому обращались
    uint8_t s1_readback_addr;// Что прочитали из регистра адреса датчика 1
    uint16_t i2c_error_cnt;  // Счетчик ошибок I2C
    uint8_t det_addr1;       // Первый найденный адрес
    uint8_t det_addr2;       // Второй найденный адрес
    uint8_t det_addr3;       // Третий найденный адрес
} VL53_Debug_t;
/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

void HAL_TIM_MspPostInit(TIM_HandleTypeDef *htim);

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/

/* USER CODE BEGIN Private defines */
//#define __DISABLE_AUTOBREAK__ //deactivates tim6 callback logic

//#define __DEBUG__
#ifdef __DEBUG__
#include "debug.h" //make printf works :O
#endif
#define __LOGGING__
#define MAX_PWM_PERCENT 58
#define _USE_TELEMETRY_
/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
