#ifndef PS_SCENE_WAITING_VISUAL_H
#define PS_SCENE_WAITING_VISUAL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PS_SCENE_WAITING_VISUAL_API_VERSION       (2UL)
#define PS_SCENE_WAITING_VISUAL_PHASE_MAX         (4U)
#define PS_SCENE_WAITING_VISUAL_SEQUENCE_MAX      (12U)
#define PS_SCENE_WAITING_VISUAL_ELEMENT_MAX       (8U)

#define PS_SCENE_WAITING_VISUAL_SOURCE_NONE        (0UL)
#define PS_SCENE_WAITING_VISUAL_SOURCE_SHELL_CURSOR (1UL)
#define PS_SCENE_WAITING_VISUAL_SOURCE_THREE_PHASE_MARKER (2UL)

#define PS_SCENE_WAITING_VISUAL_CYCLE_LOOP         (1UL)
#define PS_SCENE_WAITING_VISUAL_REBASE_NEW_STATE   (1UL)

typedef struct
{
  uint16_t x;
  uint16_t y;
  uint16_t width;
  uint16_t height;
} ps_scene_waiting_visual_bounds_t;

typedef struct
{
  uint32_t element_id;
  uint32_t visual_source_id;
  uint32_t phase_count;
  uint32_t phase_visual_id[PS_SCENE_WAITING_VISUAL_PHASE_MAX];
  uint32_t sequence_phase[PS_SCENE_WAITING_VISUAL_SEQUENCE_MAX];
  ps_scene_waiting_visual_bounds_t logical_bounds;
} ps_scene_waiting_visual_element_t;

typedef struct
{
  uint32_t api_version;
  uint32_t presentation_id;
  uint32_t phase_quantum_ms;
  uint32_t sequence_step_count;
  uint32_t settled_sequence_step;
  uint32_t cycle_policy;
  uint32_t rebase_policy;
  uint32_t element_count;
  ps_scene_waiting_visual_element_t
    elements[PS_SCENE_WAITING_VISUAL_ELEMENT_MAX];
} ps_scene_waiting_visual_t;

#ifdef __cplusplus
}
#endif

#endif /* PS_SCENE_WAITING_VISUAL_H */
