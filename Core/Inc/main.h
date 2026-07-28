/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
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
#include "stm32g0xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "hal.h"

#include "app_system.h"
#include "app_signal_process.h"
#include "app_hardware.h"   // 必须声明硬件控制函数
#include "app_storage.h"    // 必须声明存储控制函数

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
uint8_t Calc_Checksum(uint8_t *data, uint16_t len);
/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define CON_X_Pin GPIO_PIN_0
#define CON_X_GPIO_Port GPIOA
#define CON_Y_Pin GPIO_PIN_1
#define CON_Y_GPIO_Port GPIOA
#define SDA_EE_Pin GPIO_PIN_11
#define SDA_EE_GPIO_Port GPIOA
#define LED_G_Pin GPIO_PIN_12
#define LED_G_GPIO_Port GPIOA
#define SCL_EE_Pin GPIO_PIN_3
#define SCL_EE_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
