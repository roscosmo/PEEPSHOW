#ifndef PS_SCENE_RENDER_MODEL_H
#define PS_SCENE_RENDER_MODEL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PS_SCENE_RENDER_MODEL_API_VERSION (2UL)
#define PS_SCENE_RENDER_MODEL_ELEMENT_MAX (12U)
#define PS_SCENE_RENDER_CANVAS_WIDTH      (168U)
#define PS_SCENE_RENDER_CANVAS_HEIGHT     (144U)

#define PS_SCENE_RENDER_TEXT_STATE_SCENE (1UL)
#define PS_SCENE_RENDER_TEXT_STATE_1     (2UL)
#define PS_SCENE_RENDER_TEXT_STATE_2     (3UL)
#define PS_SCENE_RENDER_TEXT_STATE_3     (4UL)
#define PS_SCENE_RENDER_SPRITE_DIAMOND   (100UL)

#define PS_SCENE_RENDER_ANIMATION_NONE   (0UL)
#define PS_SCENE_RENDER_ANIMATION_CURSOR (1UL)

typedef enum
{
  PS_SCENE_RENDER_ELEMENT_NONE = 0,
  PS_SCENE_RENDER_ELEMENT_OUTLINE_RECT,
  PS_SCENE_RENDER_ELEMENT_HORIZONTAL_LINE,
  PS_SCENE_RENDER_ELEMENT_TEXT,
  PS_SCENE_RENDER_ELEMENT_SPRITE_1BPP,
  PS_SCENE_RENDER_ELEMENT_FOCUS
} ps_scene_render_element_type_t;

typedef enum
{
  PS_SCENE_RENDER_LAYER_BACKGROUND = 0,
  PS_SCENE_RENDER_LAYER_SCENE,
  PS_SCENE_RENDER_LAYER_UI,
  PS_SCENE_RENDER_LAYER_OVERLAY,
  PS_SCENE_RENDER_LAYER_COUNT
} ps_scene_render_layer_t;

typedef enum
{
  PS_SCENE_RENDER_STYLE_NONE = 0,
  PS_SCENE_RENDER_STYLE_TEXT_2X_LEFT,
  PS_SCENE_RENDER_STYLE_TEXT_2X_CENTER
} ps_scene_render_style_t;

typedef struct
{
  uint32_t element_id;
  uint32_t type;
  uint32_t asset_id;
  uint32_t layer;
  uint32_t style_id;
  uint32_t animation_binding_id;
  uint32_t visible;
  uint16_t x;
  uint16_t y;
  uint16_t width;
  uint16_t height;
} ps_scene_render_element_t;

typedef struct
{
  uint32_t api_version;
  uint32_t scene_id;
  uint32_t state_id;
  uint32_t visual_binding_id;
  uint32_t content_revision;
  uint32_t timeline_revision;
  uint32_t focus_index;
  uint32_t element_count;
  ps_scene_render_element_t elements[PS_SCENE_RENDER_MODEL_ELEMENT_MAX];
  uint32_t waiting_sequence_step_count;
  uint32_t waiting_marker_enabled;
} ps_scene_render_model_t;

#ifdef __cplusplus
}
#endif

#endif /* PS_SCENE_RENDER_MODEL_H */
