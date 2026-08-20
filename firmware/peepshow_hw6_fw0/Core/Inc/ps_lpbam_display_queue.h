#ifndef PS_LPBAM_DISPLAY_QUEUE_H
#define PS_LPBAM_DISPLAY_QUEUE_H

#include "stm32u5xx_hal.h"
#include <stdint.h>

HAL_StatusTypeDef PS_LpbamDisplayQueue_Build(void);
HAL_StatusTypeDef PS_LpbamDisplayQueue_Link(DMA_HandleTypeDef *hdma);
uint32_t PS_LpbamDisplayQueue_GetNodeCount(void);

#endif /* PS_LPBAM_DISPLAY_QUEUE_H */
