#include "ps_lpbam_display_queue.h"

#include "ps_lpbam_display_buffers.h"
#include "stm32_lpbam.h"
#include <string.h>

#define PS_LPBAM_DESC_ATTR \
  __attribute__((section(".sram4"))) __attribute__((aligned(32)))
#define PS_LPBAM_FIRST_FRAME_IMMEDIATE 0U
#define PS_LPBAM_DMA_NODE_CBR1_INDEX   2U

static LPBAM_SPI_TxDataDesc_t
  Queue1_Q_DisplayBuf_Desc[PS_LPBAM_DISPLAY_FRAME_COUNT]
                              [PS_LPBAM_DISPLAY_SEQUENCE_CHUNKS]
    PS_LPBAM_DESC_ATTR;
static DMA_NodeTypeDef
  Queue1_Q_DisplayTail_Node[PS_LPBAM_DISPLAY_FRAME_COUNT]
                               [PS_LPBAM_DISPLAY_SEQUENCE_CHUNKS]
    PS_LPBAM_DESC_ATTR;

/* Kept as a named symbol so existing GDB queue inspection remains valid. */
DMA_QListTypeDef Queue1_Q PS_LPBAM_DESC_ATTR;

static LPBAM_Status_t PS_LpbamDisplayQueue_AppendChunk(
  uint8_t *buffer,
  uint16_t len,
  uint8_t wait_for_frame_trigger,
  LPBAM_SPI_TxDataDesc_t *descriptor,
  DMA_NodeTypeDef *tail_node,
  DMA_QListTypeDef *queue)
{
  LPBAM_DMAListInfo_t dma_list = {0};
  LPBAM_COMMON_DataAdvConf_t data_config = {0};
  LPBAM_SPI_DataAdvConf_t tx = {0};
  DMA_NodeConfTypeDef tail_config = {0};
  uint16_t tail_len;
  uint16_t word_len;

  if ((buffer == NULL) || (descriptor == NULL) || (tail_node == NULL) ||
      (queue == NULL) || (len == 0U) ||
      ((((uint32_t)buffer) & 0x3UL) != 0UL))
  {
    return LPBAM_ERROR;
  }

  word_len = (uint16_t)(len & (uint16_t)~0x3U);
  tail_len = (uint16_t)(len - word_len);

  dma_list.QueueType = LPBAM_LINEAR_ADDRESSING_Q;
  dma_list.pInstance = LPDMA1;

  tx.AutoModeConf.TriggerState = LPBAM_SPI_AUTO_MODE_DISABLE;
  tx.AutoModeConf.TriggerSelection = LPBAM_SPI_GRP2_LPTIM1_CH1_TRG;
  tx.AutoModeConf.TriggerPolarity = LPBAM_SPI_TRIG_POLARITY_RISING;
  tx.DataSize = LPBAM_SPI_DATASIZE_8BIT;
  tx.Size = len;
  tx.pTxData = buffer;

  if (ADV_LPBAM_SPI_Tx_SetDataQ(
        SPI3, &dma_list, &tx, descriptor, queue) != LPBAM_OK)
  {
    return LPBAM_ERROR;
  }

  if (word_len != 0U)
  {
    data_config.UpdateSrcDataWidth = ENABLE;
    data_config.UpdateDestDataWidth = ENABLE;
    data_config.TransferConfig.Transfer.SrcDataWidth =
      LPBAM_DMA_SRC_DATAWIDTH_WORD;
    data_config.TransferConfig.Transfer.DestDataWidth =
      LPBAM_DMA_DEST_DATAWIDTH_WORD;

    if (ADV_LPBAM_Q_SetDataConfig(&data_config,
                                  LPBAM_SPI_TX_DATAQ_DATA_NODE,
                                  descriptor) != LPBAM_OK)
    {
      return LPBAM_ERROR;
    }

    descriptor->pNodes[LPBAM_SPI_TX_DATAQ_DATA_NODE]
      .LinkRegisters[PS_LPBAM_DMA_NODE_CBR1_INDEX] =
      (descriptor->pNodes[LPBAM_SPI_TX_DATAQ_DATA_NODE]
         .LinkRegisters[PS_LPBAM_DMA_NODE_CBR1_INDEX] & ~DMA_CBR1_BNDT) |
      ((uint32_t)word_len & DMA_CBR1_BNDT);

    if (tail_len != 0U)
    {
      if (HAL_DMAEx_List_GetNodeConfig(
            &tail_config,
            &descriptor->pNodes[LPBAM_SPI_TX_DATAQ_DATA_NODE]) != HAL_OK)
      {
        return LPBAM_ERROR;
      }

      tail_config.Init.SrcDataWidth = DMA_SRC_DATAWIDTH_BYTE;
      tail_config.Init.DestDataWidth = DMA_DEST_DATAWIDTH_BYTE;
      tail_config.SrcAddress = (uint32_t)&buffer[word_len];
      tail_config.DataSize = tail_len;

      if (HAL_DMAEx_List_BuildNode(&tail_config, tail_node) != HAL_OK)
      {
        return LPBAM_ERROR;
      }

      if (HAL_DMAEx_List_InsertNode_Tail(queue, tail_node) != HAL_OK)
      {
        return LPBAM_ERROR;
      }
    }
  }

  if (wait_for_frame_trigger != 0U)
  {
    LPBAM_COMMON_TrigAdvConf_t trigger = {0};

    trigger.TriggerConfig.TriggerMode = LPBAM_DMA_TRIGM_LLI_LINK_TRANSFER;
    trigger.TriggerConfig.TriggerPolarity = LPBAM_DMA_TRIG_POLARITY_RISING;
    trigger.TriggerConfig.TriggerSelection =
      LPBAM_LPDMA1_TRIGGER_LPTIM1_CH1;

    if (ADV_LPBAM_Q_SetTriggerConfig(&trigger,
                                     LPBAM_SPI_TX_DATAQ_CONFIG_NODE,
                                     descriptor) != LPBAM_OK)
    {
      return LPBAM_ERROR;
    }
  }

  return LPBAM_OK;
}

HAL_StatusTypeDef PS_LpbamDisplayQueue_Build(void)
{
  uint32_t frame;

  if ((ps_lpbam_display_active_frame_count == 0U) ||
      (ps_lpbam_display_active_frame_count > PS_LPBAM_DISPLAY_FRAME_COUNT))
  {
    return HAL_ERROR;
  }

  memset(Queue1_Q_DisplayBuf_Desc, 0, sizeof(Queue1_Q_DisplayBuf_Desc));
  memset(Queue1_Q_DisplayTail_Node, 0, sizeof(Queue1_Q_DisplayTail_Node));
  memset(&Queue1_Q, 0, sizeof(Queue1_Q));

  for (frame = 0U; frame < ps_lpbam_display_active_frame_count; ++frame)
  {
    uint32_t chunk;
    uint32_t slot =
      (ps_lpbam_display_queue_start_slot + frame) %
      ps_lpbam_display_active_frame_count;
    uint8_t wait_for_frame =
      ((PS_LPBAM_FIRST_FRAME_IMMEDIATE != 0U) && (frame == 0U)) ? 0U : 1U;

    if ((ps_lpbam_display_active_chunk_count[slot] == 0U) ||
        (ps_lpbam_display_active_chunk_count[slot] >
         PS_LPBAM_DISPLAY_SEQUENCE_CHUNKS))
    {
      return HAL_ERROR;
    }

    for (chunk = 0U;
         chunk < ps_lpbam_display_active_chunk_count[slot];
         ++chunk)
    {
      uint8_t wait_for_chunk = (chunk == 0U) ? wait_for_frame : 0U;

      if (PS_LpbamDisplayQueue_AppendChunk(
            ps_lpbam_display_tx[slot][chunk],
            ps_lpbam_display_tx_len[slot][chunk],
            wait_for_chunk,
            &Queue1_Q_DisplayBuf_Desc[frame][chunk],
            &Queue1_Q_DisplayTail_Node[frame][chunk],
            &Queue1_Q) != LPBAM_OK)
      {
        return HAL_ERROR;
      }
    }
  }

  return HAL_DMAEx_List_SetCircularMode(&Queue1_Q);
}

HAL_StatusTypeDef PS_LpbamDisplayQueue_Link(DMA_HandleTypeDef *hdma)
{
  if (hdma == NULL)
  {
    return HAL_ERROR;
  }

  return HAL_DMAEx_List_LinkQ(hdma, &Queue1_Q);
}

uint32_t PS_LpbamDisplayQueue_GetNodeCount(void)
{
  return Queue1_Q.NodeNumber;
}
