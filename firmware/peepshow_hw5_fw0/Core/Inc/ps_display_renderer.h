#ifndef PS_DISPLAY_RENDERER_H
#define PS_DISPLAY_RENDERER_H

#include "LS013B7DH05.h"

#include <stdint.h>

typedef struct {
  LS013B7DH05 *lcd;
  uint32_t dma_timeout_ms;
  uint8_t framebuffer[BUFFER_LENGTH];
  uint8_t dirty_rows[DISPLAY_HEIGHT];
  uint16_t dirty_count;
} PsDisplayRenderer;

HAL_StatusTypeDef PS_DisplayRenderer_Init(PsDisplayRenderer *renderer,
                                          LS013B7DH05 *lcd,
                                          uint32_t dma_timeout_ms);
HAL_StatusTypeDef PS_DisplayRenderer_Fill(PsDisplayRenderer *renderer,
                                          uint8_t value);
HAL_StatusTypeDef PS_DisplayRenderer_FillRowRange(PsDisplayRenderer *renderer,
                                                  uint16_t start_row,
                                                  uint16_t row_count,
                                                  uint8_t value);
HAL_StatusTypeDef PS_DisplayRenderer_Present(PsDisplayRenderer *renderer);
uint8_t *PS_DisplayRenderer_Framebuffer(PsDisplayRenderer *renderer);

#endif
