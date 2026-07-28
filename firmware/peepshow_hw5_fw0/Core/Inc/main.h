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
#include "stm32u5xx_hal.h"

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

void HAL_LPTIM_MspPostInit(LPTIM_HandleTypeDef *hlptim);

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define PHOT_EN_Pin GPIO_PIN_2
#define PHOT_EN_GPIO_Port GPIOC
#define BTN_START_Pin GPIO_PIN_4
#define BTN_START_GPIO_Port GPIOA
#define BTN_START_EXTI_IRQn EXTI4_IRQn
#define NINA_SW1_Pin GPIO_PIN_4
#define NINA_SW1_GPIO_Port GPIOC
#define NINA_SW2_Pin GPIO_PIN_5
#define NINA_SW2_GPIO_Port GPIOC
#define BUZZ_Pin GPIO_PIN_2
#define BUZZ_GPIO_Port GPIOB
#define NINA_NRST_Pin GPIO_PIN_6
#define NINA_NRST_GPIO_Port GPIOC
#define NINA_DTR_Pin GPIO_PIN_7
#define NINA_DTR_GPIO_Port GPIOC
#define NINA_DSR_Pin GPIO_PIN_8
#define NINA_DSR_GPIO_Port GPIOC
#define SD_MODE_Pin GPIO_PIN_9
#define SD_MODE_GPIO_Port GPIOC
#define VLT_LCD_Pin GPIO_PIN_2
#define VLT_LCD_GPIO_Port GPIOD
#define BTN_A_Pin GPIO_PIN_5
#define BTN_A_GPIO_Port GPIOB
#define BTN_A_EXTI_IRQn EXTI5_IRQn
#define BTN_B_Pin GPIO_PIN_6
#define BTN_B_GPIO_Port GPIOB
#define BTN_B_EXTI_IRQn EXTI6_IRQn
#define BTN_L_Pin GPIO_PIN_7
#define BTN_L_GPIO_Port GPIOB
#define BTN_L_EXTI_IRQn EXTI7_IRQn
#define BTN_R_Pin GPIO_PIN_8
#define BTN_R_GPIO_Port GPIOB
#define BTN_R_EXTI_IRQn EXTI8_IRQn

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
