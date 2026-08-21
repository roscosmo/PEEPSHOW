#ifndef PS_LPBAM_DISPLAY_QUEUE_H
#define PS_LPBAM_DISPLAY_QUEUE_H

#include "stm32u5xx_hal.h"
#include <stdint.h>

#define PS_LPBAM_DISPLAY_PROGRESS_INVALID      0U
#define PS_LPBAM_DISPLAY_PROGRESS_WAITING      1U
#define PS_LPBAM_DISPLAY_PROGRESS_TRANSFERRING 2U

typedef struct
{
  uint16_t sequence_index;
  uint16_t sequence_frame;
  uint16_t node_index;
  uint16_t node_count;
  uint32_t state;
  uint32_t live_cllr;
} ps_lpbam_display_progress_t;

HAL_StatusTypeDef PS_LpbamDisplayQueue_Build(void);
HAL_StatusTypeDef PS_LpbamDisplayQueue_Link(DMA_HandleTypeDef *hdma);
uint32_t PS_LpbamDisplayQueue_GetNodeCount(void);
HAL_StatusTypeDef PS_LpbamDisplayQueue_SnapshotProgress(
  const DMA_HandleTypeDef *hdma,
  ps_lpbam_display_progress_t *progress);

#endif /* PS_LPBAM_DISPLAY_QUEUE_H */
