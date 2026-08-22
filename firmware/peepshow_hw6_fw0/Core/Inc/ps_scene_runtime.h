#ifndef PS_SCENE_RUNTIME_H
#define PS_SCENE_RUNTIME_H

#include <stdint.h>

#include "ps_scene_waiting_visual.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PS_SCENE_RUNTIME_API_VERSION             (1UL)
#define PS_SCENE_RUNTIME_SCENE_TYPE_STATE        (1UL)
#define PS_SCENE_RUNTIME_STATUS_NOT_RUN          (0xFFFFFFFFUL)

typedef struct
{
  uint32_t api_version;
  uint32_t resolve_count;
  uint32_t reject_count;
  uint32_t scene_type;
  uint32_t page;
  uint32_t focus_index;
  uint32_t presentation_id;
  uint32_t sequence_step_count;
  uint32_t element_count;
  uint32_t last_status;
} ps_scene_runtime_probe_t;

extern volatile uint32_t g_ps_scene_runtime_waiting_demo_enable;
extern volatile ps_scene_runtime_probe_t g_ps_scene_runtime_probe;

const ps_scene_waiting_visual_t *PS_SceneRuntime_ResolveShellStateWaitingVisual(
  uint32_t page,
  uint32_t focus_index,
  const ps_scene_waiting_visual_bounds_t *cursor_bounds);

#ifdef __cplusplus
}
#endif

#endif /* PS_SCENE_RUNTIME_H */
