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
/* USER CODE BEGIN Includes */
#include "ps_lpbam_display_buffers.h"
#include <string.h>
/* USER CODE END Includes */

/* Private variables -------------------------------------------------------------------------------------------------*/
/* LPBAM variables declaration */
/* USER CODE BEGIN LpbamAp1_Scenario_Descs 0 */
#define PS_LPBAM_DESC_ATTR __attribute__((section(".sram4"))) __attribute__((aligned(32)))
#define PS_LPBAM_FIRST_FRAME_IMMEDIATE 0U
/* USER CODE END LpbamAp1_Scenario_Descs 0 */

/* USER CODE BEGIN Queue1_Q_DisplayBufA_Desc */
static LPBAM_SPI_TxDataDesc_t Queue1_Q_DisplayBuf_Desc[PS_LPBAM_DISPLAY_FRAME_COUNT][PS_LPBAM_DISPLAY_SEQUENCE_CHUNKS] PS_LPBAM_DESC_ATTR;
/* USER CODE END Queue1_Q_DisplayBufA_Desc */

/* USER CODE BEGIN LpbamAp1_Scenario_Descs 1 */

/* USER CODE END LpbamAp1_Scenario_Descs 1 */

/* Exported variables ------------------------------------------------------------------------------------------------*/
/* LPBAM queues declaration */
DMA_QListTypeDef Queue1_Q PS_LPBAM_DESC_ATTR;

/* External variables ------------------------------------------------------------------------------------------------*/
/* USER CODE BEGIN EV */
/* USER CODE END EV */

/* Private function prototypes ---------------------------------------------------------------------------------------*/
static void MX_Queue1_Q_Build(void);
/* USER CODE BEGIN PFP */
static LPBAM_Status_t PS_AppendDisplayChunk(uint8_t *buffer,
                                            uint16_t len,
                                            uint8_t wait_for_frame_trigger,
                                            LPBAM_SPI_TxDataDesc_t *descriptor,
                                            DMA_QListTypeDef *queue);
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
  memset(Queue1_Q_DisplayBuf_Desc, 0, sizeof(Queue1_Q_DisplayBuf_Desc));
  memset(&Queue1_Q, 0, sizeof(Queue1_Q));
  /* USER CODE END LpbamAp1_Scenario_Build 0 */

  MX_Queue1_Q_Build();

  /* USER CODE BEGIN LpbamAp1_Scenario_Build 1 */

  /* USER CODE END LpbamAp1_Scenario_Build 1 */
}

/* Private functions -------------------------------------------------------------------------------------------------*/
/**
  * @brief  Queue1 build
  * @param  None
  * @retval None
  */
static void MX_Queue1_Q_Build(void)
{
  /* USER CODE BEGIN Queue1_Q_Build_Clear */
  memset(Queue1_Q_DisplayBuf_Desc, 0, sizeof(Queue1_Q_DisplayBuf_Desc));
  memset(&Queue1_Q, 0, sizeof(Queue1_Q));
  /* USER CODE END Queue1_Q_Build_Clear */

  for (uint32_t frame = 0U; frame < ps_lpbam_display_active_frame_count; frame++)
  {
    uint32_t slot = (ps_lpbam_display_queue_start_slot + frame) % ps_lpbam_display_active_frame_count;
    uint8_t wait_for_frame = ((PS_LPBAM_FIRST_FRAME_IMMEDIATE != 0U) && (frame == 0U)) ? 0U : 1U;


    for (uint32_t chunk = 0U; chunk < ps_lpbam_display_active_chunk_count[slot]; chunk++)
    {
      uint8_t wait_for_chunk = (chunk == 0U) ? wait_for_frame : 0U;
      if (PS_AppendDisplayChunk(ps_lpbam_display_tx[slot][chunk],
                                ps_lpbam_display_tx_len[slot][chunk],
                                wait_for_chunk,
                                &Queue1_Q_DisplayBuf_Desc[frame][chunk],
                                &Queue1_Q) != LPBAM_OK)
      {
        Error_Handler();
      }
    }

  }

  if (HAL_DMAEx_List_SetCircularMode(&Queue1_Q) != HAL_OK)
  {
    Error_Handler();
  }
}
/* USER CODE BEGIN LpbamAp1_Scenario_Build */

static LPBAM_Status_t PS_AppendDisplayChunk(uint8_t *buffer,
                                            uint16_t len,
                                            uint8_t wait_for_frame_trigger,
                                            LPBAM_SPI_TxDataDesc_t *descriptor,
                                            DMA_QListTypeDef *queue)
{
  LPBAM_DMAListInfo_t dma_list = {0};
  LPBAM_SPI_DataAdvConf_t tx = {0};

  if ((buffer == NULL) || (descriptor == NULL) || (queue == NULL) || (len == 0U))
  {
    return LPBAM_ERROR;
  }

  dma_list.QueueType = LPBAM_LINEAR_ADDRESSING_Q;
  dma_list.pInstance = LPDMA1;

  tx.AutoModeConf.TriggerState = LPBAM_SPI_AUTO_MODE_DISABLE;
  tx.AutoModeConf.TriggerSelection = LPBAM_SPI_GRP2_LPTIM1_CH1_TRG;
  tx.AutoModeConf.TriggerPolarity = LPBAM_SPI_TRIG_POLARITY_RISING;
  tx.DataSize = LPBAM_SPI_DATASIZE_8BIT;
  tx.Size = len;
  tx.pTxData = buffer;

  if (ADV_LPBAM_SPI_Tx_SetDataQ(SPI3, &dma_list, &tx, descriptor, queue) != LPBAM_OK)
  {
    return LPBAM_ERROR;
  }

  if (wait_for_frame_trigger != 0U)
  {
    LPBAM_COMMON_TrigAdvConf_t trigger = {0};

    trigger.TriggerConfig.TriggerMode = LPBAM_DMA_TRIGM_LLI_LINK_TRANSFER;
    trigger.TriggerConfig.TriggerPolarity = LPBAM_DMA_TRIG_POLARITY_RISING;
    trigger.TriggerConfig.TriggerSelection = LPBAM_LPDMA1_TRIGGER_LPTIM1_CH1;

    if (ADV_LPBAM_Q_SetTriggerConfig(&trigger,
                                     LPBAM_SPI_TX_DATAQ_CONFIG_NODE,
                                     descriptor) != LPBAM_OK)
    {
      return LPBAM_ERROR;
    }
  }

  return LPBAM_OK;
}
/* USER CODE END LpbamAp1_Scenario_Build */


