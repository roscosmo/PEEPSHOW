#ifndef PS_LPBAM_DISPLAY_BUFFERS_H
#define PS_LPBAM_DISPLAY_BUFFERS_H

#include "LS013B7DH05.h"
#include "stm32u5xx_hal.h"
#include <stdint.h>

#define PS_LPBAM_DISPLAY_ROWS        LCD_DMA_MAX_ROWS_PER_TRANSFER
#define PS_LPBAM_DISPLAY_CHUNK_COUNT ((DISPLAY_HEIGHT + PS_LPBAM_DISPLAY_ROWS - 1U) / PS_LPBAM_DISPLAY_ROWS)
#define PS_LPBAM_DISPLAY_TX_MAX_LEN  (1U + ((PS_LPBAM_DISPLAY_ROWS + 1U) * (1U + LINE_WIDTH + 1U)) + 2U)
#define PS_LPBAM_DISPLAY_FRAME_COUNT 4U
#define PS_LPBAM_DISPLAY_SEQUENCE_CHUNKS 3U

#define PS_LPBAM_DISPLAY_CHUNK_MODE_OVERLAP_SEAMS 0U
#define PS_LPBAM_DISPLAY_CHUNK_MODE_CONTIGUOUS    1U
#define PS_LPBAM_DISPLAY_CHUNK_MODE               PS_LPBAM_DISPLAY_CHUNK_MODE_OVERLAP_SEAMS

#define PS_LPBAM_DISPLAY_UPDATE_MODE_FULL         0U
#define PS_LPBAM_DISPLAY_UPDATE_MODE_PARTIAL_DIFF 1U
#define PS_LPBAM_DISPLAY_UPDATE_MODE              PS_LPBAM_DISPLAY_UPDATE_MODE_PARTIAL_DIFF

extern uint8_t *ps_lpbam_display_tx[PS_LPBAM_DISPLAY_FRAME_COUNT][PS_LPBAM_DISPLAY_SEQUENCE_CHUNKS];
extern uint8_t ps_lpbam_display_frames[PS_LPBAM_DISPLAY_FRAME_COUNT][DISPLAY_HEIGHT][LINE_WIDTH];
extern uint8_t ps_lpbam_display_frame_a[DISPLAY_HEIGHT][LINE_WIDTH];
extern uint8_t ps_lpbam_display_frame_b[DISPLAY_HEIGHT][LINE_WIDTH];
extern uint16_t ps_lpbam_display_tx_len[PS_LPBAM_DISPLAY_FRAME_COUNT][PS_LPBAM_DISPLAY_SEQUENCE_CHUNKS];
extern uint16_t ps_lpbam_display_frame_len;
extern uint16_t ps_lpbam_display_active_frame_count;
extern uint16_t ps_lpbam_display_active_chunk_count[PS_LPBAM_DISPLAY_FRAME_COUNT];
extern uint16_t ps_lpbam_display_dirty_start_row[PS_LPBAM_DISPLAY_FRAME_COUNT];
extern uint16_t ps_lpbam_display_dirty_row_count[PS_LPBAM_DISPLAY_FRAME_COUNT];
extern uint16_t ps_lpbam_display_sequence_start_frame;
extern uint16_t ps_lpbam_display_queue_start_slot;

void PS_LpbamDisplay_ComposeExperimentFrames(void);
void PS_LpbamDisplay_SetExperimentVariant(uint8_t variant);
uint8_t PS_LpbamDisplay_GetExperimentVariant(void);
void PS_LpbamDisplay_SetSequenceStartFrame(uint16_t frame_index);
void PS_LpbamDisplay_SetQueueStartSlot(uint16_t frame_slot);

HAL_StatusTypeDef PS_LpbamDisplay_BuildPayloadBuffersForRows(
  const uint16_t *candidate_rows,
  uint16_t candidate_row_count);
HAL_StatusTypeDef PS_LpbamDisplay_BuildPatternBuffers(uint16_t start_row,
                                                      uint16_t row_count);
HAL_StatusTypeDef PS_LpbamDisplay_BuildPreparedAnimationBuffers(
  uint16_t start_row,
  uint16_t row_count,
  uint16_t sequence_start_frame);

#endif /* PS_LPBAM_DISPLAY_BUFFERS_H */
