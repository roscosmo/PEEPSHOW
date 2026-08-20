/*
 * User-owned replacements for CubeMX LPBAM node helpers. The STM32U5 HAL
 * does not reconstruct Init.Mode in HAL_DMAEx_List_GetNodeConfig(), so every
 * rebuilt node must set DMA_NORMAL explicitly.
 */
#include "stm32_ll_lpbam.h"

#ifdef LPBAM_COMMON_MODULE_ENABLED

LPBAM_Status_t LPBAM_SetDMATransferConfig(
  LPBAM_COMMON_TransferConfig_t const *const pTransferConfig,
  DMA_NodeTypeDef *const pDMANode)
{
  DMA_NodeConfTypeDef dma_node_conf;

  if (HAL_DMAEx_List_GetNodeConfig(&dma_node_conf, pDMANode) != HAL_OK)
  {
    return LPBAM_ERROR;
  }
  dma_node_conf.Init.Mode = DMA_NORMAL;

  if (pTransferConfig->UpdateSrcInc == ENABLE)
  {
    dma_node_conf.Init.SrcInc =
      pTransferConfig->TransferConfig.Transfer.SrcInc;
  }

  if (pTransferConfig->UpdateDestInc == ENABLE)
  {
    dma_node_conf.Init.DestInc =
      pTransferConfig->TransferConfig.Transfer.DestInc;
  }

  if (pTransferConfig->UpdateSrcDataWidth == ENABLE)
  {
    dma_node_conf.Init.SrcDataWidth =
      pTransferConfig->TransferConfig.Transfer.SrcDataWidth;
  }

  if (pTransferConfig->UpdateDestDataWidth == ENABLE)
  {
    dma_node_conf.Init.DestDataWidth =
      pTransferConfig->TransferConfig.Transfer.DestDataWidth;
  }

  if (pTransferConfig->UpdateTransferEventMode == ENABLE)
  {
    dma_node_conf.Init.TransferEventMode =
      pTransferConfig->TransferConfig.Transfer.TransferEventMode;
  }

#if defined(__ARM_FEATURE_CMSE) && (__ARM_FEATURE_CMSE == 3U)
  if (pTransferConfig->UpdateSrcSecure == ENABLE)
  {
    dma_node_conf.SrcSecure = pTransferConfig->TransferConfig.SrcSecure;
  }

  if (pTransferConfig->UpdateDestSecure == ENABLE)
  {
    dma_node_conf.DestSecure = pTransferConfig->TransferConfig.DestSecure;
  }
#endif

  if (HAL_DMAEx_List_BuildNode(&dma_node_conf, pDMANode) != HAL_OK)
  {
    return LPBAM_ERROR;
  }

  return LPBAM_OK;
}

LPBAM_Status_t LPBAM_SetDMATriggerConfig(
  LPBAM_COMMON_TrigConfig_t const *const pTrigConfig,
  DMA_NodeTypeDef *const pDMANode)
{
  DMA_NodeConfTypeDef dma_node_conf;

  if (HAL_DMAEx_List_GetNodeConfig(&dma_node_conf, pDMANode) != HAL_OK)
  {
    return LPBAM_ERROR;
  }
  dma_node_conf.Init.Mode = DMA_NORMAL;

  dma_node_conf.TriggerConfig.TriggerMode =
    pTrigConfig->TriggerConfig.TriggerMode;
  dma_node_conf.TriggerConfig.TriggerSelection =
    pTrigConfig->TriggerConfig.TriggerSelection;
  dma_node_conf.TriggerConfig.TriggerPolarity =
    pTrigConfig->TriggerConfig.TriggerPolarity;

  if (HAL_DMAEx_List_BuildNode(&dma_node_conf, pDMANode) != HAL_OK)
  {
    return LPBAM_ERROR;
  }

  return LPBAM_OK;
}

#endif /* LPBAM_COMMON_MODULE_ENABLED */

#ifdef LPBAM_SPI_MODULE_ENABLED

LPBAM_Status_t LPBAM_SPI_FillNodeConfig(
  LPBAM_SPI_ConfNode_t const *const pConfNode,
  DMA_NodeConfTypeDef *const pDMANodeConfig)
{
  LPBAM_Status_t status;
  LPBAM_InfoDesc_t desc_info;

  if ((pConfNode == NULL) || (pDMANodeConfig == NULL))
  {
    return LPBAM_ERROR;
  }

  pDMANodeConfig->NodeType = pConfNode->NodeDesc.NodeInfo.NodeType;
  pDMANodeConfig->Init.Request = DMA_REQUEST_SW;
  pDMANodeConfig->Init.BlkHWRequest = DMA_BREQ_SINGLE_BURST;
  pDMANodeConfig->Init.Direction = DMA_MEMORY_TO_MEMORY;
  pDMANodeConfig->Init.SrcInc = DMA_SINC_FIXED;
  pDMANodeConfig->Init.DestInc = DMA_DINC_FIXED;
  pDMANodeConfig->Init.SrcDataWidth = DMA_SRC_DATAWIDTH_WORD;
  pDMANodeConfig->Init.DestDataWidth = DMA_DEST_DATAWIDTH_WORD;
  pDMANodeConfig->Init.Priority = DMA_HIGH_PRIORITY;
  pDMANodeConfig->Init.Mode = DMA_NORMAL;

  if ((pDMANodeConfig->NodeType & DMA_CHANNEL_TYPE_GPDMA) ==
      DMA_CHANNEL_TYPE_GPDMA)
  {
    pDMANodeConfig->Init.SrcBurstLength = 1U;
    pDMANodeConfig->Init.DestBurstLength = 1U;
    pDMANodeConfig->Init.TransferAllocatedPort =
      DMA_SRC_ALLOCATED_PORT0 | DMA_DEST_ALLOCATED_PORT1;
  }
  pDMANodeConfig->Init.TransferEventMode = DMA_TCEM_LAST_LL_ITEM_TRANSFER;

  if ((pDMANodeConfig->NodeType & DMA_CHANNEL_TYPE_GPDMA) ==
      DMA_CHANNEL_TYPE_GPDMA)
  {
    pDMANodeConfig->DataHandlingConfig.DataExchange = DMA_EXCHANGE_NONE;
  }
  pDMANodeConfig->DataHandlingConfig.DataAlignment =
    DMA_DATA_RIGHTALIGN_ZEROPADDED;
  pDMANodeConfig->TriggerConfig.TriggerPolarity = DMA_TRIG_POLARITY_MASKED;

  if ((pDMANodeConfig->NodeType & DMA_CHANNEL_TYPE_2D_ADDR) ==
      DMA_CHANNEL_TYPE_2D_ADDR)
  {
    pDMANodeConfig->RepeatBlockConfig.RepeatCount = 1U;
    pDMANodeConfig->RepeatBlockConfig.SrcAddrOffset = 0;
    pDMANodeConfig->RepeatBlockConfig.DestAddrOffset = 0;
    pDMANodeConfig->RepeatBlockConfig.BlkSrcAddrOffset = 0;
    pDMANodeConfig->RepeatBlockConfig.BlkDestAddrOffset = 0;
  }

#if defined(__ARM_FEATURE_CMSE) && (__ARM_FEATURE_CMSE == 3U)
  pDMANodeConfig->SrcSecure = DMA_CHANNEL_SRC_SEC;
  pDMANodeConfig->DestSecure = DMA_CHANNEL_DEST_SEC;
#endif

  status = LPBAM_SPI_FillStructInfo(pConfNode, &desc_info);
  if (status != LPBAM_OK)
  {
    return status;
  }

  switch (pConfNode->NodeDesc.NodeInfo.NodeID)
  {
    case LPBAM_SPI_TRANSMIT_DATA_ID:
      pDMANodeConfig->Init.Request = desc_info.Request;
      pDMANodeConfig->Init.Direction = DMA_MEMORY_TO_PERIPH;
      pDMANodeConfig->Init.SrcInc = DMA_SINC_INCREMENTED;
      if (pConfNode->Config.DataSize == LPBAM_SPI_DATASIZE_8BIT)
      {
        pDMANodeConfig->Init.SrcDataWidth = DMA_SRC_DATAWIDTH_BYTE;
        pDMANodeConfig->Init.DestDataWidth = DMA_DEST_DATAWIDTH_BYTE;
      }
      else
      {
        pDMANodeConfig->Init.SrcDataWidth = DMA_SRC_DATAWIDTH_HALFWORD;
        pDMANodeConfig->Init.DestDataWidth = DMA_DEST_DATAWIDTH_HALFWORD;
      }
      break;

    case LPBAM_SPI_RECEIVE_DATA_ID:
      pDMANodeConfig->Init.Request = desc_info.Request;
      pDMANodeConfig->Init.Direction = DMA_PERIPH_TO_MEMORY;
      pDMANodeConfig->Init.DestInc = DMA_DINC_INCREMENTED;
      if (pConfNode->Config.DataSize == LPBAM_SPI_DATASIZE_8BIT)
      {
        pDMANodeConfig->Init.SrcDataWidth = DMA_SRC_DATAWIDTH_BYTE;
        pDMANodeConfig->Init.DestDataWidth = DMA_DEST_DATAWIDTH_BYTE;
      }
      else
      {
        pDMANodeConfig->Init.SrcDataWidth = DMA_SRC_DATAWIDTH_HALFWORD;
        pDMANodeConfig->Init.DestDataWidth = DMA_DEST_DATAWIDTH_HALFWORD;
      }
      break;

    default:
      break;
  }

  pDMANodeConfig->SrcAddress = desc_info.SrcAddr;
  pDMANodeConfig->DstAddress = desc_info.DstAddr;
  pDMANodeConfig->DataSize = desc_info.DataSize;
  return LPBAM_OK;
}

#endif /* LPBAM_SPI_MODULE_ENABLED */
