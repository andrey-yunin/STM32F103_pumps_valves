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
#include "stm32f1xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define GPIO_Pin GPIO_PIN_13
#define GPIO_GPIO_Port GPIOC
#define PUMP_1_Pin GPIO_PIN_0
#define PUMP_1_GPIO_Port GPIOA
#define PUMP_2_Pin GPIO_PIN_1
#define PUMP_2_GPIO_Port GPIOA
#define PUMP_3_Pin GPIO_PIN_2
#define PUMP_3_GPIO_Port GPIOA
#define PUMP_4_Pin GPIO_PIN_3
#define PUMP_4_GPIO_Port GPIOA
#define PUMP_5_Pin GPIO_PIN_4
#define PUMP_5_GPIO_Port GPIOA
#define PUMP_6_Pin GPIO_PIN_5
#define PUMP_6_GPIO_Port GPIOA
#define PUMP_7_Pin GPIO_PIN_6
#define PUMP_7_GPIO_Port GPIOA
#define PUMP_8_Pin GPIO_PIN_7
#define PUMP_8_GPIO_Port GPIOA
#define PUMP_13_Pin GPIO_PIN_0
#define PUMP_13_GPIO_Port GPIOB
#define VALVE_1_Pin GPIO_PIN_1
#define VALVE_1_GPIO_Port GPIOB
#define VALVE_2_Pin GPIO_PIN_2
#define VALVE_2_GPIO_Port GPIOB
#define PUMP_9_Pin GPIO_PIN_8
#define PUMP_9_GPIO_Port GPIOA
#define PUMP_10_Pin GPIO_PIN_9
#define PUMP_10_GPIO_Port GPIOA
#define PUMP_11_Pin GPIO_PIN_10
#define PUMP_11_GPIO_Port GPIOA
#define PUMP_12_Pin GPIO_PIN_15
#define PUMP_12_GPIO_Port GPIOA
#define VALVE_3_Pin GPIO_PIN_3
#define VALVE_3_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
