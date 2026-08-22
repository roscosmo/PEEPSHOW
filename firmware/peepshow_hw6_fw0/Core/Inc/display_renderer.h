#ifndef DISPLAY_RENDERER_H
#define DISPLAY_RENDERER_H

#include <stdint.h>

#include "LS013B7DH05.h"
#include "ps_scene_waiting_visual.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DISPLAY_RENDERER_WIDTH         DISPLAY_HEIGHT
#define DISPLAY_RENDERER_HEIGHT        DISPLAY_WIDTH
#define DISPLAY_RENDERER_BUFFER_SIZE   BUFFER_LENGTH
#define DISPLAY_RENDERER_DIRTY_ROW_MAX DISPLAY_HEIGHT
#define DISPLAY_RENDERER_PRIMITIVE_NONE         (0UL)
#define DISPLAY_RENDERER_PRIMITIVE_LIST_FULL    (1UL)
#define DISPLAY_RENDERER_PRIMITIVE_LIST_FOCUS   (2UL)
#define DISPLAY_RENDERER_PRIMITIVE_CURSOR_BLINK (3UL)
#define DISPLAY_RENDERER_PRIMITIVE_PATTERN      (4UL)
#define DISPLAY_RENDERER_ANIMATION_NONE         (0UL)
#define DISPLAY_RENDERER_ANIMATION_CURSOR_BLINK (1UL)
#define DISPLAY_RENDERER_ANIMATION_COMPOSITE_TEST (2UL)
#define DISPLAY_RENDERER_ANIMATION_FULL_FRAME_TEST (3UL)
#define DISPLAY_RENDERER_WAITING_ELEMENT_CURSOR (1UL)
#define DISPLAY_RENDERER_WAITING_ELEMENT_MULTICHUNK_TEST (2UL)
#define DISPLAY_RENDERER_WAITING_ELEMENT_FULL_FRAME_TEST (3UL)
#define DISPLAY_RENDERER_WAITING_ELEMENT_THREE_PHASE_TEST (4UL)
#define DISPLAY_RENDERER_ROW_NONE               (0xFFFFFFFFUL)
#define DISPLAY_RENDERER_WAITING_PHASE_MAX      (12U)
#define DISPLAY_RENDERER_WAITING_ELEMENT_PHASE_MAX (4U)
#define DISPLAY_RENDERER_WAITING_SEQUENCE_MAX   (12U)
#define DISPLAY_RENDERER_WAITING_GUARANTEED_STEPS (3U)
#define DISPLAY_RENDERER_WAITING_ELEMENT_MAX    (8U)
#define DISPLAY_RENDERER_SCENE_WAITING_API_VERSION (1UL)
#define DISPLAY_RENDERER_SCENE_WAITING_STATUS_NOT_RUN (0xFFFFFFFFUL)

typedef struct
{
  uint32_t width;
  uint32_t height;
  uint32_t framebuffer_hash;
  uint32_t black_pixels;
  uint32_t dirty_row_count;
  uint32_t dirty_first_row;
  uint32_t dirty_last_row;
  uint32_t primitive_id;
  uint32_t previous_focus_row;
  uint32_t current_focus_row;
} display_renderer_stats_t;

typedef struct
{
  uint16_t start_row;
  uint16_t row_count;
  uint16_t start_column;
  uint16_t column_count;
} display_renderer_panel_region_t;

typedef uint32_t (*display_renderer_waiting_phase_composer_t)(
  const void *context,
  uint32_t phase,
  uint8_t *destination,
  uint32_t destination_size);

typedef struct
{
  uint32_t element_id;
  uint32_t source_primitive_id;
  uint32_t phase_count;
  uint32_t sequence_phase[DISPLAY_RENDERER_WAITING_SEQUENCE_MAX];
  display_renderer_panel_region_t panel_bounds;
  display_renderer_waiting_phase_composer_t compose_phase;
  const void *compose_context;
} display_renderer_waiting_element_t;

typedef struct
{
  uint32_t animation_id;
  uint32_t source_primitive_id;
  uint32_t focus_row;
  uint32_t phase_count;
  uint32_t sequence_frame_count;
  uint32_t cadence_ms;
  uint32_t current_phase;
  uint32_t next_deadline_tick;
  uint32_t sequence_start_frame;
  uint32_t sequence_phase[DISPLAY_RENDERER_WAITING_SEQUENCE_MAX];
  uint32_t element_count;
  display_renderer_waiting_element_t
    elements[DISPLAY_RENDERER_WAITING_ELEMENT_MAX];
  const uint16_t *candidate_rows;
  uint16_t candidate_row_count;
  display_renderer_panel_region_t panel_bounds;
} display_renderer_waiting_animation_t;

typedef struct
{
  uint32_t api_version;
  uint32_t publish_count;
  uint32_t reject_count;
  uint32_t clear_count;
  uint32_t resolve_count;
  uint32_t active;
  uint32_t presentation_id;
  uint32_t sequence_step_count;
  uint32_t element_count;
  uint32_t last_status;
  uint32_t last_resolve_status;
} display_renderer_scene_waiting_probe_t;

extern volatile uint32_t g_display_renderer_waiting_test_variant;
extern volatile display_renderer_scene_waiting_probe_t
  g_display_renderer_scene_waiting_probe;

void DisplayRenderer_ClearWhite(void);
const uint8_t *DisplayRenderer_GetBuffer(void);
uint32_t DisplayRenderer_GetDirtyRows(const uint16_t **rows);
void DisplayRenderer_CommitPresentedFrame(void);
uint32_t DisplayRenderer_PrepareCursorBlinkFrame(
  uint32_t visible,
  display_renderer_stats_t *stats);
uint32_t DisplayRenderer_FramebufferHash(void);
uint32_t DisplayRenderer_PublishSceneWaitingVisual(
  const ps_scene_waiting_visual_t *visual);
void DisplayRenderer_ClearSceneWaitingVisual(void);
const display_renderer_waiting_animation_t *DisplayRenderer_GetWaitingAnimation(
  uint32_t sequence_start_frame,
  uint32_t next_deadline_tick);
const display_renderer_waiting_animation_t *
DisplayRenderer_GetGuaranteedWaitingAnimation(
  const display_renderer_waiting_animation_t *preferred);
uint32_t DisplayRenderer_SelectWaitingAnimation(
  const display_renderer_waiting_animation_t *animation);
const display_renderer_waiting_animation_t *
DisplayRenderer_GetSelectedWaitingAnimation(void);
uint32_t DisplayRenderer_ResumePreferredWaitingAnimation(
  uint32_t selected_sequence_frame,
  uint32_t *preferred_sequence_frame,
  uint32_t *preferred_sequence_count);
uint32_t DisplayRenderer_ValidateWaitingAnimation(
  const display_renderer_waiting_animation_t *animation);
uint32_t DisplayRenderer_CopyWaitingAnimationFrame(
  const display_renderer_waiting_animation_t *animation,
  uint32_t sequence_frame,
  uint8_t *destination,
  uint32_t destination_size);
uint32_t DisplayRenderer_PrepareWaitingAnimationFrame(
  uint32_t sequence_frame,
  display_renderer_stats_t *stats);
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
