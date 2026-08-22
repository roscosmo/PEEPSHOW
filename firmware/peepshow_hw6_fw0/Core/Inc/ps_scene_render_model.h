#ifndef PS_SCENE_RENDER_MODEL_H
#define PS_SCENE_RENDER_MODEL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PS_SCENE_RENDER_MODEL_API_VERSION (1UL)
#define PS_SCENE_RENDER_MODEL_ROW_MAX     (3U)

#define PS_SCENE_RENDER_TEXT_STATE_SCENE (1UL)
#define PS_SCENE_RENDER_TEXT_STATE_1     (2UL)
#define PS_SCENE_RENDER_TEXT_STATE_2     (3UL)
#define PS_SCENE_RENDER_TEXT_STATE_3     (4UL)

typedef struct
{
  uint32_t api_version;
  uint32_t scene_id;
  uint32_t state_id;
  uint32_t visual_binding_id;
  uint32_t content_revision;
  uint32_t timeline_revision;
  uint32_t title_text_id;
  uint32_t row_count;
  uint32_t row_text_id[PS_SCENE_RENDER_MODEL_ROW_MAX];
  uint32_t selected_row;
  uint32_t waiting_sequence_step_count;
  uint32_t waiting_marker_enabled;
} ps_scene_render_model_t;

#ifdef __cplusplus
}
#endif

#endif /* PS_SCENE_RENDER_MODEL_H */
