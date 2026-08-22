#ifndef PS_SCENE_RUNTIME_H
#define PS_SCENE_RUNTIME_H

#include <stdint.h>

#include "ps_scene_waiting_visual.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PS_SCENE_RUNTIME_API_VERSION             (4UL)
#define PS_SCENE_RUNTIME_SCENE_TYPE_STATE        (1UL)
#define PS_SCENE_RUNTIME_STATUS_NOT_RUN          (0xFFFFFFFFUL)
#define PS_SCENE_RUNTIME_INDEX_INVALID           (0xFFFFFFFFUL)
#define PS_SCENE_RUNTIME_STATE_MAX               (8U)
#define PS_SCENE_RUNTIME_TRANSITION_MAX          (16U)

typedef enum
{
  PS_SCENE_RUNTIME_ACTION_NONE = 0,
  PS_SCENE_RUNTIME_ACTION_PREVIOUS,
  PS_SCENE_RUNTIME_ACTION_NEXT
} ps_scene_runtime_action_t;

typedef struct
{
  uint32_t state_id;
  uint32_t visual_binding_id;
  uint32_t focus_index;
} ps_scene_runtime_state_t;

typedef struct
{
  uint32_t source_state_id;
  uint32_t action;
  uint32_t target_state_id;
} ps_scene_runtime_transition_t;

typedef struct
{
  uint32_t api_version;
  uint32_t scene_id;
  uint32_t entry_state_id;
  uint32_t state_count;
  uint32_t transition_count;
  ps_scene_runtime_state_t states[PS_SCENE_RUNTIME_STATE_MAX];
  ps_scene_runtime_transition_t
    transitions[PS_SCENE_RUNTIME_TRANSITION_MAX];
} ps_scene_runtime_state_scene_t;

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
  uint32_t active;
  uint32_t enter_count;
  uint32_t exit_count;
  uint32_t action_count;
  uint32_t state_change_count;
  uint32_t state_index;
  uint32_t state_revision;
  uint32_t timeline_revision;
  uint32_t last_action;
  uint32_t descriptor_validate_count;
  uint32_t descriptor_state_count;
  uint32_t descriptor_transition_count;
  uint32_t scene_id;
  uint32_t state_id;
  uint32_t visual_binding_id;
  uint32_t transition_match_count;
  uint32_t transition_miss_count;
  uint32_t last_status;
} ps_scene_runtime_probe_t;

extern volatile uint32_t g_ps_scene_runtime_waiting_demo_enable;
extern volatile ps_scene_runtime_probe_t g_ps_scene_runtime_probe;

const ps_scene_waiting_visual_t *PS_SceneRuntime_ResolveShellStateWaitingVisual(
  uint32_t page,
  uint32_t focus_index,
  const ps_scene_waiting_visual_bounds_t *cursor_bounds);
uint32_t PS_SceneRuntime_EnterStateScene(void);
void PS_SceneRuntime_ExitStateScene(void);
uint32_t PS_SceneRuntime_StateSceneActive(void);
uint32_t PS_SceneRuntime_StateIndex(void);
uint32_t PS_SceneRuntime_StateFocusIndex(void);
uint32_t PS_SceneRuntime_HandleStateSceneAction(
  ps_scene_runtime_action_t action);
const ps_scene_waiting_visual_t *PS_SceneRuntime_ResolveStateSceneWaitingVisual(
  uint32_t focus_index,
  const ps_scene_waiting_visual_bounds_t *cursor_bounds);

#ifdef __cplusplus
}
#endif

#endif /* PS_SCENE_RUNTIME_H */
