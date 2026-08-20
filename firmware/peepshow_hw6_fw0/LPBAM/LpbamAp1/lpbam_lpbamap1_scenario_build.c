/* USER CODE BEGIN Header */
/**
  **********************************************************************************************************************
  * @file   lpbam_lpbamap1_scenario_build.c
  * @author MCD Application Team
  * @brief  Provides LPBAM LpbamAp1 application Scenario scenario build services
  **********************************************************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  **********************************************************************************************************************
  */
/* USER CODE END Header */
/* Includes ----------------------------------------------------------------------------------------------------------*/
#include "lpbam_lpbamap1.h"

/* Private variables -------------------------------------------------------------------------------------------------*/
/* LPBAM variables declaration */
/* USER CODE BEGIN LpbamAp1_Scenario_Descs 0 */

/* USER CODE END LpbamAp1_Scenario_Descs 0 */

/* USER CODE BEGIN Queue1_Q_Start_1_Desc */

/* USER CODE END Queue1_Q_Start_1_Desc */
static LPBAM_LPTIM_StartFullDesc_t Queue1_Q_Start_1_Desc;

/* USER CODE BEGIN Queue1_Q_Transmit_1_Desc */

/* USER CODE END Queue1_Q_Transmit_1_Desc */
static LPBAM_SPI_TxFullDesc_t Queue1_Q_Transmit_1_Desc;

/* USER CODE BEGIN Queue1_Q_Start_2_Desc */

/* USER CODE END Queue1_Q_Start_2_Desc */
static LPBAM_LPTIM_StartFullDesc_t Queue1_Q_Start_2_Desc;

/* USER CODE BEGIN Queue1_Q_Transmit_2_Desc */

/* USER CODE END Queue1_Q_Transmit_2_Desc */
static LPBAM_SPI_TxFullDesc_t Queue1_Q_Transmit_2_Desc;

/* USER CODE BEGIN LpbamAp1_Scenario_Descs 1 */

/* USER CODE END LpbamAp1_Scenario_Descs 1 */

/* Exported variables ------------------------------------------------------------------------------------------------*/
/* LPBAM queues declaration */
DMA_QListTypeDef Queue1_Q;

/* External variables ------------------------------------------------------------------------------------------------*/
/* USER CODE BEGIN EV */
/* USER CODE END EV */

/* Private function prototypes ---------------------------------------------------------------------------------------*/
static void MX_Queue1_Q_Build(void);

/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Exported functions ------------------------------------------------------------------------------------------------*/
/**
  * @brief LpbamAp1 application Scenario scenario build
  * @param None
  * @retval None
  */
void MX_LpbamAp1_Scenario_Build(void)
{
  /* USER CODE BEGIN LpbamAp1_Scenario_Build 0 */

  /* USER CODE END LpbamAp1_Scenario_Build 0 */

  /* LPBAM build Queue1 queue */
  MX_Queue1_Q_Build();

  /* USER CODE BEGIN LpbamAp1_Scenario_Build 1 */

  /* USER CODE END LpbamAp1_Scenario_Build 1 */
}

/* Private functions -------------------------------------------------------------------------------------------------*/

/**
  * @brief  LpbamAp1 application Scenario scenario Queue1 queue build
  * @param  None
  * @retval None
  */
static void MX_Queue1_Q_Build(void)
{
  /* LPBAM build variable */
  LPBAM_DMAListInfo_t pDMAListInfo_LPTIM = {0};
  LPBAM_LPTIM_StartFullAdvConf_t pStartFull_LPTIM = {0};
  LPBAM_DMAListInfo_t pDMAListInfo_SPI = {0};
  LPBAM_SPI_FullAdvConf_t pTxFull_SPI = {0};

  /**
    * Queue1 queue Start_1 build
    */
  pDMAListInfo_LPTIM.QueueType = LPBAM_LINEAR_ADDRESSING_Q;
  pDMAListInfo_LPTIM.pInstance = LPDMA1;
  pStartFull_LPTIM.StartMode = LPBAM_LPTIM_START_SINGLE;
  pStartFull_LPTIM.WakeupIT = LPBAM_LPTIM_IT_NONE;
  if (ADV_LPBAM_LPTIM_Start_SetFullQ(LPTIM1, &pDMAListInfo_LPTIM, &pStartFull_LPTIM, &Queue1_Q_Start_1_Desc, &Queue1_Q) != LPBAM_OK)
  {
    Error_Handler();
  }

  /**
    * Queue1 queue Transmit_1 build
    */
  pDMAListInfo_SPI.QueueType = LPBAM_LINEAR_ADDRESSING_Q;
  pDMAListInfo_SPI.pInstance = LPDMA1;
  pTxFull_SPI.CLKPolarity = LPBAM_SPI_POLARITY_LOW;
  pTxFull_SPI.CLKPhase = LPBAM_SPI_PHASE_1EDGE;
  pTxFull_SPI.FirstBit = LPBAM_SPI_FIRSTBIT_MSB;
  pTxFull_SPI.BaudRatePrescaler = LPBAM_SPI_BAUDRATEPRESCALER_BYPASS;
  pTxFull_SPI.DataSize = LPBAM_SPI_DATASIZE_8BIT;
  pTxFull_SPI.AutoModeConf.TriggerState = LPBAM_SPI_AUTO_MODE_ENABLE;
  pTxFull_SPI.AutoModeConf.TriggerSelection = LPBAM_SPI_GRP2_LPDMA_CH0_TCF_TRG;
  pTxFull_SPI.AutoModeConf.TriggerPolarity = LPBAM_SPI_TRIG_POLARITY_RISING;
  pTxFull_SPI.WakeupIT = LPBAM_SPI_IT_NONE;
  pTxFull_SPI.Size = 1;
  pTxFull_SPI.pTxData = (uint8_t*)&WRITEBUFFERNAME[WRITEBUFFEROFFSET];
  if (ADV_LPBAM_SPI_Tx_SetFullQ(SPI3, &pDMAListInfo_SPI, &pTxFull_SPI, &Queue1_Q_Transmit_1_Desc, &Queue1_Q) != LPBAM_OK)
  {
    Error_Handler();
  }

  /**
    * Queue1 queue Start_2 build
    */
  if (ADV_LPBAM_LPTIM_Start_SetFullQ(LPTIM1, &pDMAListInfo_LPTIM, &pStartFull_LPTIM, &Queue1_Q_Start_2_Desc, &Queue1_Q) != LPBAM_OK)
  {
    Error_Handler();
  }

  /**
    * Queue1 queue Transmit_2 build
    */
  if (ADV_LPBAM_SPI_Tx_SetFullQ(SPI3, &pDMAListInfo_SPI, &pTxFull_SPI, &Queue1_Q_Transmit_2_Desc, &Queue1_Q) != LPBAM_OK)
  {
    Error_Handler();
  }

  /**
    * Set circular mode
    */
  if (ADV_LPBAM_Q_SetCircularMode(&Queue1_Q_Start_1_Desc, LPBAM_LPTIM_START_FULLQ_CONFIG_NODE, &Queue1_Q) != LPBAM_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN LpbamAp1_Scenario_Build */

/* USER CODE END LpbamAp1_Scenario_Build */
