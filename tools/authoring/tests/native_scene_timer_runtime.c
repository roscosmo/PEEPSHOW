#include <assert.h>
#include <stdio.h>
/* Runtime event tests do not use the hardware button API or its HAL include. */
#define PS_INPUT_BUTTONS_H
#include "ps_scene_runtime.c"
#include "ps_egg_object_decoder.c"
#include "ps_scene_objects.c"
#include "ps_scene_object_graph.c"
#include "ps_scene_object_render.c"

static ps_scene_runtime_state_scene_t fixture;
volatile ps_package_source_probe_t g_ps_package_source_probe;

uint32_t PS_PackageSource_Resolve(ps_package_source_view_t *view)
{
  (void)view; return 1;
}
uint32_t PS_EggStateLoader_Load(const uint8_t *blob, uint32_t size,
                               uint32_t resident, ps_scene_runtime_state_scene_t *scene)
{
  (void)blob; (void)size; (void)resident; (void)scene; return 1;
}
uint32_t PS_EggStateLoader_EntrySceneId(void) { return 1; }
uint32_t PS_EggStateLoader_DecodeDevelopmentScene(const uint8_t *blob,
  uint32_t size, uint32_t id, ps_scene_runtime_state_scene_t *scene)
{ (void)blob; (void)size; (void)id; (void)scene; return 1; }
uint32_t PS_EggStateLoader_LoadDevelopment(const uint8_t *blob,
  uint32_t size, ps_scene_runtime_state_scene_t *scene)
{ (void)blob; (void)size; (void)scene; return 1; }

uint32_t PS_EggStateLoader_SceneCount(void) { return 2U; }
uint32_t PS_EggStateLoader_LoadScene(uint32_t id, ps_scene_runtime_state_scene_t *scene)
{
  *scene = fixture;
  scene->scene_id = id;
  return 0;
}

int main(void)
{
  uint32_t scene_epoch, state_epoch, binding, kind, scope, start, delay;
  ps_scene_runtime_state_scene_t *scene = &s_ps_scene_runtime_scene_slots[0];
  fixture.api_version = PS_SCENE_RUNTIME_API_VERSION;
  fixture.scene_id = 1;
  fixture.entry_state_id = 1;
  fixture.state_count = 2;
  fixture.states[0] = (ps_scene_runtime_state_t){1, 1, 0};
  fixture.states[1] = (ps_scene_runtime_state_t){2, 1, 0};
  fixture.interaction_mode = PS_SCENE_RUNTIME_INTERACTION_CONTINUOUS;
  fixture.joystick_policy = PS_SCENE_RUNTIME_JOYSTICK_FOUR_WAY;
  fixture.visual_binding_count = 1;
  fixture.visual_bindings[0].visual_binding_id = 1;
  fixture.visual_bindings[0].element_count = 1;
  fixture.visual_bindings[0].elements[0] = (ps_scene_render_element_t){
    .element_id = 1, .type = PS_SCENE_RENDER_ELEMENT_FILLED_RECT,
    .visible = 1, .width = 10, .height = 10,
  };
  fixture.visual_bindings[0].waiting_visual.api_version = PS_SCENE_WAITING_VISUAL_API_VERSION;
  fixture.visual_bindings[0].waiting_visual.sequence_step_count = 1;
  fixture.variable_count = 1;
  fixture.variables[0] = (ps_scene_runtime_variable_t){1, PS_SCENE_RUNTIME_VALUE_S32, 0};
  fixture.event_binding_count = 1;
  fixture.event_bindings[0] = (ps_scene_runtime_event_binding_t){
    1, PS_SCENE_RUNTIME_EVENT_CLASS_TIMER, PS_SCENE_RUNTIME_TIMER_SCENE,
    PS_SCENE_RUNTIME_TIMER_START_ENTRY, 1000,
  };
  fixture.action_count = 2;
  fixture.actions[0] = (ps_scene_runtime_action_t){
    PS_SCENE_RUNTIME_ACTION_SET_VARIABLE, 1, 0, PS_SCENE_RUNTIME_MUTATION_ADD, 1, 0,
  };
  fixture.actions[1] = (ps_scene_runtime_action_t){PS_SCENE_RUNTIME_ACTION_START_TIMER, 0, 0, 0, 0, 0};
  fixture.transition_count = 1;
  fixture.transitions[0] = (ps_scene_runtime_transition_t){
    .transition_id = 1, .source_state_id = 0, .scene_event_id = 1,
    .first_action = 0, .action_count = 2,
  };
  *scene = fixture;
  scene->visual_bindings[0].element_count = 0;
  assert(PS_SceneRuntime_ValidateStateScene(scene) != 0);
  *scene = fixture;
  assert(PS_SceneRuntime_ValidateStateScene(scene) == 0);
  assert(PS_SceneRuntime_ActivateDecodedScene(scene, 0) == 0);
  scene_epoch = PS_SceneRuntime_SceneActivation();
  state_epoch = PS_SceneRuntime_StateActivation();
  assert(PS_SceneRuntime_TimerConfiguration(0, &scope, &start, &delay) == 1);
  assert(scope == PS_SCENE_RUNTIME_TIMER_SCENE && start == 0 && delay == 1000);
  assert(PS_SceneRuntime_HandleStateSceneEvent(0) == PS_SCENE_RUNTIME_INPUT_APPLIED);
  assert(s_ps_scene_runtime_variables[0] == 1);
  assert(PS_SceneRuntime_SceneActivation() == scene_epoch);
  assert(PS_SceneRuntime_StateActivation() == state_epoch);
  assert(g_ps_scene_runtime_probe.state_change_count == 0);
  assert(PS_SceneRuntime_TakeTimerAction(&binding, &kind) == 1);
  assert(binding == 0 && kind == PS_SCENE_RUNTIME_ACTION_START_TIMER);
  assert(PS_SceneRuntime_TakeTimerAction(&binding, &kind) == 0);

  scene->transitions[0].target_state_id = 2;
  assert(PS_SceneRuntime_HandleStateSceneEvent(0) == PS_SCENE_RUNTIME_INPUT_APPLIED);
  assert(g_ps_scene_runtime_probe.state_id == 2);
  assert(PS_SceneRuntime_StateActivation() == state_epoch + 1);
  assert(PS_SceneRuntime_SceneActivation() == scene_epoch);

  scene->guard_count = 1;
  scene->guards[0] = (ps_scene_runtime_guard_t){1, PS_SCENE_RUNTIME_COMPARE_EQ, 99};
  scene->transitions[0].guard_count = 1;
  assert(PS_SceneRuntime_HandleStateSceneEvent(0) == PS_SCENE_RUNTIME_INPUT_IGNORED);
  assert(s_ps_scene_runtime_variables[0] == 2);
  assert(PS_SceneRuntime_TakeTimerAction(&binding, &kind) == 0);
  scene->transitions[0].guard_count = 0;

  scene->transitions[0].source_state_id = 1;
  assert(PS_SceneRuntime_ValidateStateScene(scene) != 0);
  scene->transitions[0].source_state_id = 0;
  scene->actions[1].target_id = 15;
  assert(PS_SceneRuntime_ValidateStateScene(scene) != 0);
  scene->actions[1].target_id = 0;

  scene->transitions[0].action_count = 0;
  scene->transitions[0].target_state_id = 0;
  scene->transitions[0].target_scene_id = 2;
  assert(PS_SceneRuntime_HandleStateSceneEvent(0) == PS_SCENE_RUNTIME_INPUT_APPLIED);
  assert(PS_SceneRuntime_SceneActivation() == scene_epoch + 1);
  assert(PS_SceneRuntime_TakeTimerAction(&binding, &kind) == 0);
  puts("native scene runtime timer checks passed");
  return 0;
}
