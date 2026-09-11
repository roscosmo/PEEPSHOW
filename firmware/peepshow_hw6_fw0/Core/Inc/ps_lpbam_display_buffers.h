#ifndef PS_LPBAM_DISPLAY_BUFFERS_H
#define PS_LPBAM_DISPLAY_BUFFERS_H

#include "LS013B7DH05.h"
#include "stm32u5xx_hal.h"
#include <stdint.h>

#define PS_LPBAM_DISPLAY_ROWS        LCD_DMA_MAX_ROWS_PER_TRANSFER
#define PS_LPBAM_DISPLAY_SPATIAL_ROWS 28U
#define PS_LPBAM_DISPLAY_SPATIAL_CHUNK_COUNT \
  (DISPLAY_HEIGHT / PS_LPBAM_DISPLAY_SPATIAL_ROWS)
#define PS_LPBAM_DISPLAY_TRANSACTION_MAX_LEN (1U + ((PS_LPBAM_DISPLAY_ROWS + 1U) * (1U + LINE_WIDTH + 1U)) + 2U)
#define PS_LPBAM_DISPLAY_MAX_CHUNKS 18U
#define PS_LPBAM_DISPLAY_SEQUENCE_MAX 12U
#define PS_LPBAM_DISPLAY_PAYLOAD_SLOT_COUNT PS_LPBAM_DISPLAY_MAX_CHUNKS
#define PS_LPBAM_DISPLAY_GUARANTEED_FRAME_COUNT \
  (PS_LPBAM_DISPLAY_PAYLOAD_SLOT_COUNT / PS_LPBAM_DISPLAY_SPATIAL_CHUNK_COUNT)
#define PS_LPBAM_DISPLAY_PATTERN_SEQUENCE_COUNT 4U
#define PS_LPBAM_DISPLAY_ADMISSION_API_VERSION 4U
#define PS_LPBAM_ROW_WIRE_BYTES (1U + LINE_WIDTH + 1U)
#define PS_LPBAM_ALIGN4(value) (((value) + 3U) & ~3U)
#define PS_LPBAM_SPATIAL_PAYLOAD_BYTES \
  (1U + ((PS_LPBAM_DISPLAY_SPATIAL_ROWS + 1U) * PS_LPBAM_ROW_WIRE_BYTES) + 2U)
#define PS_LPBAM_SPATIAL_SLOT_BYTES PS_LPBAM_ALIGN4(PS_LPBAM_SPATIAL_PAYLOAD_BYTES)

#define PS_LPBAM_ADMISSION_REASON_NONE          0U
#define PS_LPBAM_ADMISSION_REASON_ARGUMENT      1U
#define PS_LPBAM_ADMISSION_REASON_SEQUENCE      2U
#define PS_LPBAM_ADMISSION_REASON_CHUNKS        3U
#define PS_LPBAM_ADMISSION_REASON_PAYLOAD       4U
#define PS_LPBAM_ADMISSION_REASON_BUILD         5U

#define PS_LPBAM_DISPLAY_CHUNK_MODE_OVERLAP_SEAMS 0U
#define PS_LPBAM_DISPLAY_CHUNK_MODE_CONTIGUOUS    1U
#define PS_LPBAM_DISPLAY_CHUNK_MODE               PS_LPBAM_DISPLAY_CHUNK_MODE_OVERLAP_SEAMS

#define PS_LPBAM_DISPLAY_UPDATE_MODE_FULL         0U
#define PS_LPBAM_DISPLAY_UPDATE_MODE_PARTIAL_DIFF 1U
#define PS_LPBAM_DISPLAY_UPDATE_MODE              PS_LPBAM_DISPLAY_UPDATE_MODE_PARTIAL_DIFF

typedef struct
{
  uint16_t first_chunk;
  uint16_t chunk_count;
  uint16_t dirty_start_row;
  uint16_t dirty_row_count;
} ps_lpbam_display_sequence_entry_t;

typedef struct
{
  uint32_t api_version;
  uint16_t sequence_used;
  uint16_t sequence_capacity;
  uint16_t chunk_used;
  uint16_t chunk_capacity;
  uint16_t payload_wire_bytes;
  uint16_t payload_used_bytes;
  uint16_t payload_capacity_bytes;
  uint16_t reserved;
  uint32_t status;
  uint32_t reason;
} ps_lpbam_display_admission_t;

/* Caller-owned ordinary RAM, never DMA-visible/live payload scratch. */
typedef struct
{
  uint8_t previous[DISPLAY_HEIGHT][LINE_WIDTH];
  uint8_t target[DISPLAY_HEIGHT][LINE_WIDTH];
  uint8_t payload[PS_LPBAM_DISPLAY_PAYLOAD_SLOT_COUNT][PS_LPBAM_SPATIAL_SLOT_BYTES];
  uint8_t wire[PS_LPBAM_DISPLAY_TRANSACTION_MAX_LEN];
  uint16_t length[PS_LPBAM_DISPLAY_PAYLOAD_SLOT_COUNT];
  uint8_t band[PS_LPBAM_DISPLAY_PAYLOAD_SLOT_COUNT];
  uint32_t frames_composed;
} ps_lpbam_display_check_workspace_t;

typedef uint32_t (*ps_lpbam_display_compose_fn)(void *context, uint32_t step,
  uint8_t *destination, uint32_t capacity);

/* thDisplay only. Exact full-scene band/chunk/payload admission, including wrap.
 * Calls compose synchronously at most sequence_count+1 times (1 means success).
 * Does not alter production buffers, descriptors, probes or readiness. Workspace,
 * result and callback outputs must not alias live display/LPBAM storage.
 */
HAL_StatusTypeDef PS_LpbamDisplay_CheckFullSceneAnimation(uint32_t sequence_count,
  ps_lpbam_display_compose_fn compose, void *context,
  ps_lpbam_display_check_workspace_t *workspace, ps_lpbam_display_admission_t *result);

extern uint8_t *ps_lpbam_display_tx[PS_LPBAM_DISPLAY_MAX_CHUNKS];
extern uint8_t *ps_lpbam_display_payload_slot[
  PS_LPBAM_DISPLAY_PAYLOAD_SLOT_COUNT];
extern uint8_t ps_lpbam_display_frame_a[DISPLAY_HEIGHT][LINE_WIDTH];
extern uint8_t ps_lpbam_display_frame_b[DISPLAY_HEIGHT][LINE_WIDTH];
extern uint16_t ps_lpbam_display_tx_len[PS_LPBAM_DISPLAY_MAX_CHUNKS];
extern uint16_t ps_lpbam_display_payload_slot_capacity[
  PS_LPBAM_DISPLAY_PAYLOAD_SLOT_COUNT];
extern uint16_t ps_lpbam_display_payload_slot_len[
  PS_LPBAM_DISPLAY_PAYLOAD_SLOT_COUNT];
extern uint8_t ps_lpbam_display_payload_slot_band[
  PS_LPBAM_DISPLAY_PAYLOAD_SLOT_COUNT];
extern uint8_t ps_lpbam_display_payload_slot_occupied[
  PS_LPBAM_DISPLAY_PAYLOAD_SLOT_COUNT];
extern uint8_t ps_lpbam_display_tx_payload_slot[
  PS_LPBAM_DISPLAY_MAX_CHUNKS];
extern ps_lpbam_display_sequence_entry_t
  ps_lpbam_display_sequence[PS_LPBAM_DISPLAY_SEQUENCE_MAX];
extern ps_lpbam_display_admission_t ps_lpbam_display_admission;
extern uint16_t ps_lpbam_display_active_sequence_count;
extern uint16_t ps_lpbam_display_active_chunk_count;
extern uint16_t ps_lpbam_display_payload_wire_bytes;
extern uint16_t ps_lpbam_display_sequence_start_frame;
extern uint16_t ps_lpbam_display_queue_start_slot;

void PS_LpbamDisplay_ComposeExperimentFrames(void);
void PS_LpbamDisplay_SetExperimentVariant(uint8_t variant);
uint8_t PS_LpbamDisplay_GetExperimentVariant(void);

HAL_StatusTypeDef PS_LpbamDisplay_BuildPatternBuffers(uint16_t start_row,
                                                       uint16_t row_count);
HAL_StatusTypeDef PS_LpbamDisplay_BeginPreparedAnimation(
  const uint16_t *candidate_rows,
  uint16_t candidate_row_count,
  uint16_t sequence_count,
  uint16_t sequence_start_frame);
HAL_StatusTypeDef PS_LpbamDisplay_AppendPreparedTransition(
  const uint8_t previous_frame[DISPLAY_HEIGHT][LINE_WIDTH],
  const uint8_t target_frame[DISPLAY_HEIGHT][LINE_WIDTH]);
HAL_StatusTypeDef PS_LpbamDisplay_FinishPreparedAnimation(void);

#endif /* PS_LPBAM_DISPLAY_BUFFERS_H */
