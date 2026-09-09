#ifndef PS_SCENE_OBJECT_GRAPH_H
#define PS_SCENE_OBJECT_GRAPH_H

#include "ps_scene_runtime.h"
#include "ps_scene_objects.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Engine-only development execution. Descriptor and package stay immutable. */
typedef struct
{
  const ps_scene_runtime_state_scene_t *scene;
  ps_scene_objects_t objects;
  int32_t variables[PS_SCENE_RUNTIME_VARIABLE_MAX];
} ps_scene_object_graph_t;

typedef struct
{
  uint32_t transition_id;
  uint32_t target_scene_id;
  uint32_t state_entered;
  uint32_t count;
  ps_scene_runtime_action_t actions[PS_SCENE_RUNTIME_ACTION_MAX];
} ps_scene_object_effects_t;

typedef struct
{
  const ps_scene_object_graph_t *origin;
  ps_scene_objects_stage_t objects;
  int32_t original_variables[PS_SCENE_RUNTIME_VARIABLE_MAX];
  int32_t variables[PS_SCENE_RUNTIME_VARIABLE_MAX];
  ps_scene_object_effects_t effects;
} ps_scene_object_graph_stage_t;

uint32_t PS_SceneObjectGraph_Init(ps_scene_object_graph_t *bank,
  const ps_scene_runtime_state_scene_t *scene, uint32_t package_scene_count,
  uint32_t activation);
/* Zero-based binding for input or timer dispatch. INPUT_APPLIED means staged,
 * not committed. target_scene_id requires replacement admission; Commit refuses
 * that case until replacement orchestration exists. No effects have been sent.
 */
uint32_t PS_SceneObjectGraph_StageEvent(const ps_scene_object_graph_t *bank,
  uint32_t binding, ps_scene_object_graph_stage_t *stage);
/* Inspect objects.candidate with PS_SceneObjects_Snapshot, admit display/effects,
 * then commit. Failure leaves bank and effects unchanged. Runtime owner must
 * serialize staging/commit; this is not an inter-thread lock.
 */
uint32_t PS_SceneObjectGraph_Commit(ps_scene_object_graph_t *bank,
  ps_scene_object_graph_stage_t *stage, ps_scene_object_effects_t *effects);
void PS_SceneObjectGraph_Abort(ps_scene_object_graph_stage_t *stage);

#ifdef __cplusplus
}
#endif
#endif
