#include "ps_scene_runtime.h"

#include <string.h>

#include "knobs_autogen.h"

#define PS_SCENE_RUNTIME_PRESENTATION_SHELL_BASE (0x100UL)
#define PS_SCENE_RUNTIME_PRESENTATION_PROOF_BASE (0x200UL)
#define PS_SCENE_RUNTIME_PRESENTATION_STATE_BASE (0x30000000UL)
#define PS_SCENE_RUNTIME_ELEMENT_CURSOR          (1UL)
#define PS_SCENE_RUNTIME_ELEMENT_MARKER          (5UL)
#define PS_SCENE_RUNTIME_DEMO_SCENE_ID            (1UL)
#define PS_SCENE_RUNTIME_DEMO_STATE_1_ID          (101UL)
#define PS_SCENE_RUNTIME_DEMO_STATE_2_ID          (102UL)
#define PS_SCENE_RUNTIME_DEMO_STATE_3_ID          (103UL)
#define PS_SCENE_RUNTIME_DEMO_VISUAL_1_ID         (1001UL)
#define PS_SCENE_RUNTIME_DEMO_VISUAL_2_ID         (1002UL)
#define PS_SCENE_RUNTIME_DEMO_VISUAL_3_ID         (1003UL)

static ps_scene_waiting_visual_t s_ps_scene_runtime_waiting_visual;

static const ps_scene_runtime_state_scene_t s_ps_scene_runtime_state_scene =
{
  .api_version = PS_SCENE_RUNTIME_API_VERSION,
  .scene_id = PS_SCENE_RUNTIME_DEMO_SCENE_ID,
  .entry_state_id = PS_SCENE_RUNTIME_DEMO_STATE_1_ID,
  .state_count = 3UL,
  .transition_count = 6UL,
  .states =
  {
    { PS_SCENE_RUNTIME_DEMO_STATE_1_ID,
      PS_SCENE_RUNTIME_DEMO_VISUAL_1_ID, 0UL },
    { PS_SCENE_RUNTIME_DEMO_STATE_2_ID,
      PS_SCENE_RUNTIME_DEMO_VISUAL_2_ID, 1UL },
    { PS_SCENE_RUNTIME_DEMO_STATE_3_ID,
      PS_SCENE_RUNTIME_DEMO_VISUAL_3_ID, 2UL }
  },
  .transitions =
  {
    { PS_SCENE_RUNTIME_DEMO_STATE_1_ID, PS_SCENE_RUNTIME_ACTION_PREVIOUS,
      PS_SCENE_RUNTIME_DEMO_STATE_3_ID },
    { PS_SCENE_RUNTIME_DEMO_STATE_1_ID, PS_SCENE_RUNTIME_ACTION_NEXT,
      PS_SCENE_RUNTIME_DEMO_STATE_2_ID },
    { PS_SCENE_RUNTIME_DEMO_STATE_2_ID, PS_SCENE_RUNTIME_ACTION_PREVIOUS,
      PS_SCENE_RUNTIME_DEMO_STATE_1_ID },
    { PS_SCENE_RUNTIME_DEMO_STATE_2_ID, PS_SCENE_RUNTIME_ACTION_NEXT,
      PS_SCENE_RUNTIME_DEMO_STATE_3_ID },
    { PS_SCENE_RUNTIME_DEMO_STATE_3_ID, PS_SCENE_RUNTIME_ACTION_PREVIOUS,
      PS_SCENE_RUNTIME_DEMO_STATE_2_ID },
    { PS_SCENE_RUNTIME_DEMO_STATE_3_ID, PS_SCENE_RUNTIME_ACTION_NEXT,
      PS_SCENE_RUNTIME_DEMO_STATE_1_ID }
  }
};

volatile uint32_t g_ps_scene_runtime_waiting_demo_enable;
volatile ps_scene_runtime_probe_t g_ps_scene_runtime_probe =
{
  .api_version = PS_SCENE_RUNTIME_API_VERSION,
  .last_status = PS_SCENE_RUNTIME_STATUS_NOT_RUN
};

static uint32_t PS_SceneRuntime_FindStateIndex(
  const ps_scene_runtime_state_scene_t *scene,
  uint32_t state_id)
{
  uint32_t index;

  for (index = 0UL; index < scene->state_count; ++index)
  {
    if (scene->states[index].state_id == state_id)
    {
      return index;
    }
  }

  return PS_SCENE_RUNTIME_INDEX_INVALID;
}

static uint32_t PS_SceneRuntime_ValidateStateScene(
  const ps_scene_runtime_state_scene_t *scene)
{
  uint32_t state_index;
  uint32_t compare_index;
  uint32_t transition_index;

  g_ps_scene_runtime_probe.descriptor_validate_count++;
  if ((scene == NULL) ||
      (scene->api_version != PS_SCENE_RUNTIME_API_VERSION) ||
      (scene->scene_id == 0UL) ||
      (scene->state_count == 0UL) ||
      (scene->state_count > PS_SCENE_RUNTIME_STATE_MAX) ||
      (scene->transition_count > PS_SCENE_RUNTIME_TRANSITION_MAX))
  {
    return 1UL;
  }

  for (state_index = 0UL; state_index < scene->state_count; ++state_index)
  {
    const ps_scene_runtime_state_t *state = &scene->states[state_index];

    if ((state->state_id == 0UL) || (state->visual_binding_id == 0UL) ||
        (state->focus_index >= scene->state_count))
    {
      return 1UL;
    }
    for (compare_index = state_index + 1UL;
         compare_index < scene->state_count;
         ++compare_index)
    {
      if (state->state_id == scene->states[compare_index].state_id)
      {
        return 1UL;
      }
    }
  }

  if (PS_SceneRuntime_FindStateIndex(scene, scene->entry_state_id) ==
      PS_SCENE_RUNTIME_INDEX_INVALID)
  {
    return 1UL;
  }

  for (transition_index = 0UL;
       transition_index < scene->transition_count;
       ++transition_index)
  {
    const ps_scene_runtime_transition_t *transition =
      &scene->transitions[transition_index];

    if (((transition->action != PS_SCENE_RUNTIME_ACTION_PREVIOUS) &&
         (transition->action != PS_SCENE_RUNTIME_ACTION_NEXT)) ||
        (PS_SceneRuntime_FindStateIndex(scene, transition->source_state_id) ==
         PS_SCENE_RUNTIME_INDEX_INVALID) ||
        (PS_SceneRuntime_FindStateIndex(scene, transition->target_state_id) ==
         PS_SCENE_RUNTIME_INDEX_INVALID))
    {
      return 1UL;
    }
    for (compare_index = transition_index + 1UL;
         compare_index < scene->transition_count;
         ++compare_index)
    {
      if ((transition->source_state_id ==
           scene->transitions[compare_index].source_state_id) &&
          (transition->action == scene->transitions[compare_index].action))
      {
        return 1UL;
      }
    }
  }

  return 0UL;
}

static void PS_SceneRuntime_SelectState(uint32_t state_index)
{
  const ps_scene_runtime_state_t *state =
    &s_ps_scene_runtime_state_scene.states[state_index];

  g_ps_scene_runtime_probe.state_index = state_index;
  g_ps_scene_runtime_probe.state_id = state->state_id;
  g_ps_scene_runtime_probe.visual_binding_id = state->visual_binding_id;
  g_ps_scene_runtime_probe.focus_index = state->focus_index;
}

static const ps_scene_waiting_visual_t *PS_SceneRuntime_BuildWaitingVisual(
  uint32_t presentation_id,
  uint32_t sequence_step_count,
  uint32_t marker_enabled,
  const ps_scene_waiting_visual_bounds_t *cursor_bounds)
{
  ps_scene_waiting_visual_t *visual = &s_ps_scene_runtime_waiting_visual;
  ps_scene_waiting_visual_element_t *cursor;
  ps_scene_waiting_visual_element_t *marker;
  uint32_t step;

  if ((cursor_bounds == NULL) || (cursor_bounds->width == 0U) ||
      (cursor_bounds->height == 0U))
  {
    g_ps_scene_runtime_probe.reject_count++;
    g_ps_scene_runtime_probe.last_status = 1UL;
    return NULL;
  }

  (void)memset(visual, 0, sizeof(*visual));
  visual->api_version = PS_SCENE_WAITING_VISUAL_API_VERSION;
  visual->presentation_id = presentation_id;
  visual->phase_quantum_ms =
    (uint32_t)KNOB_DISPLAY_CURSOR_BLINK_PERIOD_MS;
  visual->sequence_step_count = sequence_step_count;
  visual->settled_sequence_step = 1UL;
  visual->cycle_policy = PS_SCENE_WAITING_VISUAL_CYCLE_LOOP;
  visual->rebase_policy = PS_SCENE_WAITING_VISUAL_REBASE_NEW_STATE;
  visual->element_count = (marker_enabled != 0UL) ? 2UL : 1UL;

  cursor = &visual->elements[0];
  cursor->element_id = PS_SCENE_RUNTIME_ELEMENT_CURSOR;
  cursor->visual_source_id = PS_SCENE_WAITING_VISUAL_SOURCE_SHELL_CURSOR;
  cursor->phase_count = 2UL;
  cursor->phase_visual_id[0] = 1UL;
  cursor->phase_visual_id[1] = 2UL;
  cursor->logical_bounds = *cursor_bounds;
  for (step = 0UL; step < visual->sequence_step_count; ++step)
  {
    cursor->sequence_phase[step] = step & 1UL;
  }

  if (visual->element_count == 2UL)
  {
    marker = &visual->elements[1];
    marker->element_id = PS_SCENE_RUNTIME_ELEMENT_MARKER;
    marker->visual_source_id =
      PS_SCENE_WAITING_VISUAL_SOURCE_THREE_PHASE_MARKER;
    marker->phase_count = 3UL;
    marker->phase_visual_id[0] = 10UL;
    marker->phase_visual_id[1] = 11UL;
    marker->phase_visual_id[2] = 12UL;
    marker->logical_bounds.x = 160U;
    marker->logical_bounds.y = 0U;
    marker->logical_bounds.width = 8U;
    marker->logical_bounds.height = 24U;
    for (step = 0UL; step < visual->sequence_step_count; ++step)
    {
      marker->sequence_phase[step] = (step + 2UL) % 3UL;
    }
  }

  g_ps_scene_runtime_probe.presentation_id = visual->presentation_id;
  g_ps_scene_runtime_probe.sequence_step_count =
    visual->sequence_step_count;
  g_ps_scene_runtime_probe.element_count = visual->element_count;
  g_ps_scene_runtime_probe.last_status = 0UL;
  return visual;
}

const ps_scene_waiting_visual_t *PS_SceneRuntime_ResolveShellStateWaitingVisual(
  uint32_t page,
  uint32_t focus_index,
  const ps_scene_waiting_visual_bounds_t *cursor_bounds)
{
  uint32_t proof_enabled =
    (g_ps_scene_runtime_waiting_demo_enable != 0UL) ? 1UL : 0UL;

  g_ps_scene_runtime_probe.resolve_count++;
  g_ps_scene_runtime_probe.scene_type = PS_SCENE_RUNTIME_SCENE_TYPE_STATE;
  g_ps_scene_runtime_probe.page = page;
  g_ps_scene_runtime_probe.focus_index = focus_index;
  g_ps_scene_runtime_probe.last_status = PS_SCENE_RUNTIME_STATUS_NOT_RUN;

  return PS_SceneRuntime_BuildWaitingVisual(
    ((proof_enabled != 0UL) ? PS_SCENE_RUNTIME_PRESENTATION_PROOF_BASE :
                              PS_SCENE_RUNTIME_PRESENTATION_SHELL_BASE) + page,
    (proof_enabled != 0UL) ? 6UL : 4UL,
    proof_enabled,
    cursor_bounds);
}

uint32_t PS_SceneRuntime_EnterStateScene(void)
{
  uint32_t entry_index;

  g_ps_scene_runtime_probe.enter_count++;
  g_ps_scene_runtime_probe.last_status = PS_SCENE_RUNTIME_STATUS_NOT_RUN;
  if (PS_SceneRuntime_ValidateStateScene(
        &s_ps_scene_runtime_state_scene) != 0UL)
  {
    g_ps_scene_runtime_probe.reject_count++;
    g_ps_scene_runtime_probe.active = 0UL;
    g_ps_scene_runtime_probe.last_status = 1UL;
    return PS_SCENE_RUNTIME_INDEX_INVALID;
  }

  entry_index = PS_SceneRuntime_FindStateIndex(
    &s_ps_scene_runtime_state_scene,
    s_ps_scene_runtime_state_scene.entry_state_id);
  g_ps_scene_runtime_probe.active = 1UL;
  g_ps_scene_runtime_probe.scene_type = PS_SCENE_RUNTIME_SCENE_TYPE_STATE;
  g_ps_scene_runtime_probe.descriptor_state_count =
    s_ps_scene_runtime_state_scene.state_count;
  g_ps_scene_runtime_probe.descriptor_transition_count =
    s_ps_scene_runtime_state_scene.transition_count;
  g_ps_scene_runtime_probe.scene_id = s_ps_scene_runtime_state_scene.scene_id;
  PS_SceneRuntime_SelectState(entry_index);
  g_ps_scene_runtime_probe.state_revision++;
  g_ps_scene_runtime_probe.timeline_revision++;
  g_ps_scene_runtime_probe.last_action = PS_SCENE_RUNTIME_ACTION_NONE;
  g_ps_scene_runtime_probe.last_status = 0UL;
  return g_ps_scene_runtime_probe.state_index;
}

void PS_SceneRuntime_ExitStateScene(void)
{
  g_ps_scene_runtime_probe.exit_count++;
  g_ps_scene_runtime_probe.active = 0UL;
  g_ps_scene_runtime_probe.last_status = 0UL;
}

uint32_t PS_SceneRuntime_StateSceneActive(void)
{
  return g_ps_scene_runtime_probe.active;
}

uint32_t PS_SceneRuntime_StateIndex(void)
{
  return g_ps_scene_runtime_probe.state_index;
}

uint32_t PS_SceneRuntime_StateFocusIndex(void)
{
  return g_ps_scene_runtime_probe.focus_index;
}

uint32_t PS_SceneRuntime_HandleStateSceneAction(
  ps_scene_runtime_action_t action)
{
  const ps_scene_runtime_state_scene_t *scene =
    &s_ps_scene_runtime_state_scene;
  uint32_t transition_index;

  g_ps_scene_runtime_probe.action_count++;
  g_ps_scene_runtime_probe.last_action = (uint32_t)action;
  if (g_ps_scene_runtime_probe.active == 0UL)
  {
    g_ps_scene_runtime_probe.last_status = 1UL;
    return 0UL;
  }

  if ((action != PS_SCENE_RUNTIME_ACTION_PREVIOUS) &&
      (action != PS_SCENE_RUNTIME_ACTION_NEXT))
  {
    g_ps_scene_runtime_probe.transition_miss_count++;
    g_ps_scene_runtime_probe.last_status = 1UL;
    return 0UL;
  }

  for (transition_index = 0UL;
       transition_index < scene->transition_count;
       ++transition_index)
  {
    const ps_scene_runtime_transition_t *transition =
      &scene->transitions[transition_index];

    if ((transition->source_state_id == g_ps_scene_runtime_probe.state_id) &&
        (transition->action == (uint32_t)action))
    {
      uint32_t target_index = PS_SceneRuntime_FindStateIndex(
        scene, transition->target_state_id);

      if (target_index == PS_SCENE_RUNTIME_INDEX_INVALID)
      {
        break;
      }
      PS_SceneRuntime_SelectState(target_index);
      g_ps_scene_runtime_probe.transition_match_count++;
      g_ps_scene_runtime_probe.state_change_count++;
      g_ps_scene_runtime_probe.state_revision++;
      g_ps_scene_runtime_probe.last_status = 0UL;
      return 1UL;
    }
  }

  g_ps_scene_runtime_probe.transition_miss_count++;
  g_ps_scene_runtime_probe.last_status = 1UL;
  return 0UL;
}

const ps_scene_waiting_visual_t *PS_SceneRuntime_ResolveStateSceneWaitingVisual(
  uint32_t focus_index,
  const ps_scene_waiting_visual_bounds_t *cursor_bounds)
{
  g_ps_scene_runtime_probe.resolve_count++;
  g_ps_scene_runtime_probe.last_status = PS_SCENE_RUNTIME_STATUS_NOT_RUN;
  if ((g_ps_scene_runtime_probe.active == 0UL) ||
      (focus_index != g_ps_scene_runtime_probe.focus_index))
  {
    g_ps_scene_runtime_probe.reject_count++;
    g_ps_scene_runtime_probe.last_status = 1UL;
    return NULL;
  }

  return PS_SceneRuntime_BuildWaitingVisual(
    PS_SCENE_RUNTIME_PRESENTATION_STATE_BASE |
      (g_ps_scene_runtime_probe.timeline_revision & 0x00FFFFFFUL),
    6UL,
    1UL,
    cursor_bounds);
}
