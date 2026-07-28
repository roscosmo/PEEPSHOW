#include "ps_display_renderer.h"

#include <string.h>

static HAL_StatusTypeDef ps_display_renderer_mark_row(PsDisplayRenderer *renderer,
                                                      uint16_t row)
{
  if ((renderer == NULL) || (row == 0u) || (row > DISPLAY_HEIGHT)) {
    return HAL_ERROR;
  }

  uint16_t index = (uint16_t)(row - 1u);
  if (renderer->dirty_rows[index] == 0u) {
    renderer->dirty_rows[index] = 1u;
    renderer->dirty_count++;
  }

  return HAL_OK;
}

static void ps_display_renderer_clear_dirty(PsDisplayRenderer *renderer)
{
  memset(renderer->dirty_rows, 0, sizeof(renderer->dirty_rows));
  renderer->dirty_count = 0u;
}

HAL_StatusTypeDef PS_DisplayRenderer_Init(PsDisplayRenderer *renderer,
                                          LS013B7DH05 *lcd,
                                          uint32_t dma_timeout_ms)
{
  if ((renderer == NULL) || (lcd == NULL)) {
    return HAL_ERROR;
  }

  memset(renderer, 0, sizeof(*renderer));
  renderer->lcd = lcd;
  renderer->dma_timeout_ms = dma_timeout_ms;

  return HAL_OK;
}

HAL_StatusTypeDef PS_DisplayRenderer_Fill(PsDisplayRenderer *renderer,
                                          uint8_t value)
{
  if (renderer == NULL) {
    return HAL_ERROR;
  }

  memset(renderer->framebuffer, value, sizeof(renderer->framebuffer));
  memset(renderer->dirty_rows, 1, sizeof(renderer->dirty_rows));
  renderer->dirty_count = DISPLAY_HEIGHT;

  return HAL_OK;
}

HAL_StatusTypeDef PS_DisplayRenderer_FillRowRange(PsDisplayRenderer *renderer,
                                                  uint16_t start_row,
                                                  uint16_t row_count,
                                                  uint8_t value)
{
  if ((renderer == NULL) || (start_row == 0u) || (row_count == 0u)) {
    return HAL_ERROR;
  }

  uint32_t final_row = (uint32_t)start_row + row_count - 1u;
  if (final_row > DISPLAY_HEIGHT) {
    return HAL_ERROR;
  }

  uint8_t *dst = &renderer->framebuffer[((uint32_t)(start_row - 1u) * LINE_WIDTH)];
  memset(dst, value, (uint32_t)row_count * LINE_WIDTH);

  for (uint16_t row = start_row; row <= final_row; row++) {
    HAL_StatusTypeDef status = ps_display_renderer_mark_row(renderer, row);
    if (status != HAL_OK) {
      return status;
    }
  }

  return HAL_OK;
}

HAL_StatusTypeDef PS_DisplayRenderer_Present(PsDisplayRenderer *renderer)
{
  if ((renderer == NULL) || (renderer->lcd == NULL)) {
    return HAL_ERROR;
  }

  if (renderer->dirty_count == 0u) {
    return HAL_OK;
  }

  HAL_StatusTypeDef status;
  if (renderer->dirty_count == DISPLAY_HEIGHT) {
    status = LCD_PresentFull_DMA(renderer->lcd,
                                 renderer->framebuffer,
                                 renderer->dma_timeout_ms);
  } else {
    uint16_t rows[DISPLAY_HEIGHT];
    uint16_t count = 0u;

    for (uint16_t index = 0u; index < DISPLAY_HEIGHT; index++) {
      if (renderer->dirty_rows[index] != 0u) {
        rows[count++] = (uint16_t)(index + 1u);
      }
    }

    status = LCD_PresentRows_DMA(renderer->lcd,
                                 renderer->framebuffer,
                                 rows,
                                 count,
                                 renderer->dma_timeout_ms);
  }

  if (status == HAL_OK) {
    ps_display_renderer_clear_dirty(renderer);
  }

  return status;
}

uint8_t *PS_DisplayRenderer_Framebuffer(PsDisplayRenderer *renderer)
{
  if (renderer == NULL) {
    return NULL;
  }

  return renderer->framebuffer;
}
