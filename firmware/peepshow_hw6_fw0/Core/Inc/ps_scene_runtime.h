#ifndef PS_SCENE_RUNTIME_H
#define PS_SCENE_RUNTIME_H

#include <stdint.h>

#include "ps_scene_render_model.h"
#include "ps_scene_waiting_visual.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PS_SCENE_RUNTIME_API_VERSION             (16UL)
#define PS_SCENE_RUNTIME_SCENE_TYPE_STATE        (1UL)
#define PS_SCENE_RUNTIME_STATUS_NOT_RUN          (0xFFFFFFFFUL)
#define PS_SCENE_RUNTIME_STATUS_OK               (0UL)
#define PS_SCENE_RUNTIME_STATUS_ERROR            (1UL)
#define PS_SCENE_RUNTIME_STATUS_NO_PACKAGE       (2UL)
#define PS_SCENE_RUNTIME_INDEX_INVALID           (0xFFFFFFFFUL)
#define PS_SCENE_RUNTIME_STATE_MAX               (8U)
#define PS_SCENE_RUNTIME_VISUAL_BINDING_MAX      (8U)
#define PS_SCENE_RUNTIME_INPUT_ROUTE_MAX         (8U)
#define PS_SCENE_RUNTIME_VARIABLE_MAX            (8U)
#define PS_SCENE_RUNTIME_GUARD_MAX               (16U)
#define PS_SCENE_RUNTIME_ACTION_MAX              (32U)
#define PS_SCENE_RUNTIME_WAITING_ANIMATION_MAX   (16U)
#define PS_SCENE_RUNTIME_TRANSITION_MAX          (16U)

#define PS_SCENE_RUNTIME_INPUT_ERROR             (0UL)
#define PS_SCENE_RUNTIME_INPUT_APPLIED           (1UL)
#define PS_SCENE_RUNTIME_INPUT_IGNORED           (2UL)
#define PS_SCENE_RUNTIME_INTERACTION_CONTINUOUS  (1UL)
#define PS_SCENE_RUNTIME_INTERACTION_TIMEOUT     (2UL)
#define PS_SCENE_RUNTIME_INACTIVE_PRESERVE       (1UL)
#define PS_SCENE_RUNTIME_INACTIVE_EXIT_SHELL     (2UL)

typedef enum
{
  PS_SCENE_RUNTIME_VALUE_NONE = 0,
  PS_SCENE_RUNTIME_VALUE_S32
} ps_scene_runtime_value_type_t;

typedef enum
{
  PS_SCENE_RUNTIME_COMPARE_NONE = 0,
  PS_SCENE_RUNTIME_COMPARE_EQ,
  PS_SCENE_RUNTIME_COMPARE_NE,
  PS_SCENE_RUNTIME_COMPARE_LT,
  PS_SCENE_RUNTIME_COMPARE_LE,
  PS_SCENE_RUNTIME_COMPARE_GT,
  PS_SCENE_RUNTIME_COMPARE_GE
} ps_scene_runtime_compare_t;

typedef enum
{
  PS_SCENE_RUNTIME_MUTATION_NONE = 0,
  PS_SCENE_RUNTIME_MUTATION_SET,
  PS_SCENE_RUNTIME_MUTATION_ADD,
  PS_SCENE_RUNTIME_MUTATION_SUBTRACT
} ps_scene_runtime_mutation_t;

typedef enum
{
  PS_SCENE_RUNTIME_ACTION_NONE = 0,
  PS_SCENE_RUNTIME_ACTION_SET_VARIABLE,
  PS_SCENE_RUNTIME_ACTION_SET_ELEMENT_VISIBILITY,
  PS_SCENE_RUNTIME_ACTION_SET_ELEMENT_POSITION,
  PS_SCENE_RUNTIME_ACTION_SET_ELEMENT_FRAME,
  PS_SCENE_RUNTIME_ACTION_SET_ELEMENT_WAITING_ANIMATION,
  PS_SCENE_RUNTIME_ACTION_PLAY_SFX
} ps_scene_runtime_action_kind_t;

typedef enum
{
  PS_SCENE_RUNTIME_TIMELINE_NONE = 0,
  PS_SCENE_RUNTIME_TIMELINE_PRESERVE,
  PS_SCENE_RUNTIME_TIMELINE_REBASE
} ps_scene_runtime_timeline_policy_t;

typedef struct
{
  uint32_t animation_id;
  uint32_t phase_quantum_ms;
  uint32_t sequence_step_count;
  uint32_t phase_count;
  uint32_t phase_visual_id[PS_SCENE_WAITING_VISUAL_PHASE_MAX];
  uint32_t sequence_phase[PS_SCENE_WAITING_VISUAL_SEQUENCE_MAX];
} ps_scene_runtime_waiting_animation_t;

typedef struct
{
  uint32_t state_id;
  uint32_t visual_binding_id;
  uint32_t focus_index;
} ps_scene_runtime_state_t;

typedef struct
{
  uint32_t visual_binding_id;
  uint32_t element_count;
  ps_scene_render_element_t elements[PS_SCENE_RENDER_MODEL_ELEMENT_MAX];
  ps_scene_waiting_visual_t waiting_visual;
} ps_scene_runtime_visual_binding_t;

typedef struct
{
  uint32_t logical_event;
  uint32_t input_id;
  uint32_t scene_event_id;
} ps_scene_runtime_input_route_t;

typedef struct
{
  uint32_t variable_id;
  uint32_t value_type;
  int32_t initial_value;
} ps_scene_runtime_variable_t;

typedef struct
{
  uint32_t variable_id;
  uint32_t compare;
  int32_t value;
} ps_scene_runtime_guard_t;

typedef struct
{
  uint32_t kind;
  uint32_t target_id;
  uint32_t target_element_id;
  uint32_t operation;
  int32_t value;
  int32_t secondary_value;
} ps_scene_runtime_action_t;

typedef struct
{
  uint32_t transition_id;
  uint32_t source_state_id;
  uint32_t scene_event_id;
  uint32_t first_guard;
  uint32_t guard_count;
  uint32_t first_action;
  uint32_t action_count;
  uint32_t target_state_id;
  uint32_t target_scene_id;
} ps_scene_runtime_transition_t;

typedef struct
{
  uint32_t api_version;
  uint32_t scene_id;
  uint32_t entry_state_id;
  uint32_t state_count;
  uint32_t visual_binding_count;
  uint32_t input_route_count;
  uint32_t variable_count;
  uint32_t guard_count;
  uint32_t action_count;
  uint32_t waiting_animation_count;
  uint32_t transition_count;
  uint32_t interaction_mode;
  uint32_t inactive_route;
  uint32_t meaningful_input_mask;
  ps_scene_runtime_state_t states[PS_SCENE_RUNTIME_STATE_MAX];
  ps_scene_runtime_visual_binding_t
    visual_bindings[PS_SCENE_RUNTIME_VISUAL_BINDING_MAX];
  ps_scene_runtime_input_route_t
    input_routes[PS_SCENE_RUNTIME_INPUT_ROUTE_MAX];
  ps_scene_runtime_variable_t variables[PS_SCENE_RUNTIME_VARIABLE_MAX];
  ps_scene_runtime_guard_t guards[PS_SCENE_RUNTIME_GUARD_MAX];
  ps_scene_runtime_action_t actions[PS_SCENE_RUNTIME_ACTION_MAX];
  ps_scene_runtime_waiting_animation_t
    waiting_animations[PS_SCENE_RUNTIME_WAITING_ANIMATION_MAX];
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
  uint32_t package_source;
  uint32_t package_source_status;
  uint32_t activation_status;
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
  uint32_t descriptor_visual_binding_count;
  uint32_t descriptor_input_route_count;
  uint32_t descriptor_variable_count;
  uint32_t descriptor_guard_count;
  uint32_t descriptor_action_count;
  uint32_t descriptor_waiting_animation_count;
  uint32_t descriptor_transition_count;
  uint32_t descriptor_interaction_mode;
  uint32_t descriptor_inactive_route;
  uint32_t descriptor_meaningful_input_mask;
  uint32_t scene_id;
  uint32_t state_id;
  uint32_t visual_binding_id;
  uint32_t transition_match_count;
  uint32_t transition_miss_count;
  uint32_t input_route_match_count;
  uint32_t input_route_miss_count;
  uint32_t guard_evaluate_count;
  uint32_t guard_pass_count;
  uint32_t guard_reject_count;
  uint32_t action_commit_count;
  uint32_t action_error_count;
  uint32_t element_action_commit_count;
  uint32_t waiting_animation_commit_count;
  uint32_t waiting_animation_rebase_count;
  uint32_t sfx_action_commit_count;
  uint32_t sfx_request_take_count;
  uint32_t last_sfx_cue_index;
  uint32_t last_element_action_kind;
  uint32_t last_element_action_binding_id;
  uint32_t last_element_action_id;
  int32_t last_element_action_value;
  int32_t last_element_action_secondary_value;
  uint32_t last_scene_event_id;
  uint32_t last_transition_id;
  uint32_t scene_replace_count;
  uint32_t scene_replace_fail_count;
  uint32_t scene_replace_source_id;
  uint32_t scene_replace_target_id;
  uint32_t scene_replace_status;
  int32_t primary_variable_value;
  uint32_t render_model_resolve_count;
  uint32_t render_model_scene_id;
  uint32_t render_model_state_id;
  uint32_t render_model_visual_binding_id;
  uint32_t render_model_content_revision;
  uint32_t render_model_timeline_revision;
  uint32_t render_model_focus_index;
  uint32_t render_model_element_count;
  uint32_t render_model_focus_element_id;
  uint32_t render_model_sprite_count;
  uint32_t render_model_status;
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
uint32_t PS_SceneRuntime_InteractionMode(void);
uint32_t PS_SceneRuntime_InactiveRoute(void);
uint32_t PS_SceneRuntime_InputIsMeaningful(uint32_t logical_event,
                                           uint32_t input_id);
const ps_scene_render_model_t *PS_SceneRuntime_ResolveStateSceneRenderModel(
  void);
uint32_t PS_SceneRuntime_HandleStateSceneInput(
  uint32_t logical_event,
  uint32_t input_id);
uint32_t PS_SceneRuntime_TakeSfxRequest(uint32_t *cue_index);
const ps_scene_waiting_visual_t *PS_SceneRuntime_ResolveStateSceneWaitingVisual(
  const ps_scene_render_model_t *model,
  const ps_scene_waiting_visual_bounds_t *cursor_bounds);

#ifdef __cplusplus
}
#endif

#endif /* PS_SCENE_RUNTIME_H */
