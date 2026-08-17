#ifndef DISPLAY_RENDERER_H
#define DISPLAY_RENDERER_H

#include <stdint.h>

#include "LS013B7DH05.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DISPLAY_RENDERER_WIDTH         DISPLAY_HEIGHT
#define DISPLAY_RENDERER_HEIGHT        DISPLAY_WIDTH
#define DISPLAY_RENDERER_BUFFER_SIZE   BUFFER_LENGTH
#define DISPLAY_RENDERER_DIRTY_ROW_MAX DISPLAY_HEIGHT

typedef struct
{
  uint32_t width;
  uint32_t height;
  uint32_t framebuffer_hash;
  uint32_t black_pixels;
  uint32_t dirty_row_count;
  uint32_t dirty_first_row;
  uint32_t dirty_last_row;
} display_renderer_stats_t;

typedef struct
{
  uint16_t start_row;
  uint16_t row_count;
  uint16_t start_column;
  uint16_t column_count;
} display_renderer_panel_region_t;

void DisplayRenderer_ClearWhite(void);
const uint8_t *DisplayRenderer_GetBuffer(void);
uint32_t DisplayRenderer_GetDirtyRows(const uint16_t **rows);
uint32_t DisplayRenderer_FramebufferHash(void);
uint32_t DisplayRenderer_GetLpbamCursorPanelRegion(
  display_renderer_panel_region_t *region);
void DisplayRenderer_PreparePattern(display_renderer_stats_t *stats);
void DisplayRenderer_PrepareUIPage(
  uint32_t page,
  uint32_t calibration_page,
  uint32_t focus_index,
  uint32_t shutdown_state,
  uint32_t shutdown_countdown_seconds,
  display_renderer_stats_t *stats);

#ifdef __cplusplus
}
#endif

#endif /* DISPLAY_RENDERER_H */
