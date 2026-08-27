#include "ps_scene_runtime.h"

#include <string.h>

#include "knobs_autogen.h"
#include "ps_egg_state_loader.h"
#include "ps_input_buttons.h"
#include "ps_package_source.h"

#define PS_SCENE_RUNTIME_PRESENTATION_SHELL_BASE (0x100UL)
#define PS_SCENE_RUNTIME_PRESENTATION_PROOF_BASE (0x200UL)
#define PS_SCENE_RUNTIME_ELEMENT_CURSOR          (1UL)
#define PS_SCENE_RUNTIME_ELEMENT_MARKER          (5UL)

static ps_scene_waiting_visual_t s_ps_scene_runtime_waiting_visual;
static ps_scene_render_model_t s_ps_scene_runtime_render_model;
static int32_t
  s_ps_scene_runtime_variables[PS_SCENE_RUNTIME_VARIABLE_MAX];


static ps_scene_runtime_state_scene_t s_ps_scene_runtime_scene_slots[2];
static ps_scene_runtime_state_scene_t *s_ps_scene_runtime_state_scene =
  &s_ps_scene_runtime_scene_slots[0];
static uint32_t s_ps_scene_runtime_active_slot;

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

static uint32_t PS_SceneRuntime_FindVariableIndex(
  const ps_scene_runtime_state_scene_t *scene,
  uint32_t variable_id)
{
  uint32_t index;

  for (index = 0UL; index < scene->variable_count; ++index)
  {
    if (scene->variables[index].variable_id == variable_id)
    {
      return index;
    }
  }

  return PS_SCENE_RUNTIME_INDEX_INVALID;
}

static uint32_t PS_SceneRuntime_FindVisualBindingIndex(
  const ps_scene_runtime_state_scene_t *scene,
  uint32_t visual_binding_id)
{
  uint32_t index;

  for (index = 0UL; index < scene->visual_binding_count; ++index)
  {
    if (scene->visual_bindings[index].visual_binding_id == visual_binding_id)
    {
      return index;
    }
  }

  return PS_SCENE_RUNTIME_INDEX_INVALID;
}

static uint32_t PS_SceneRuntime_RangeValid(uint32_t first,
                                           uint32_t count,
                                           uint32_t total)
{
  return ((first <= total) && (count <= (total - first))) ? 1UL : 0UL;
}

static uint32_t PS_SceneRuntime_RenderElementValid(
  const ps_scene_render_element_t *element)
{
  uint32_t x_end;
  uint32_t y_end;

  if ((element == NULL) || (element->element_id == 0UL) ||
      (element->type <= PS_SCENE_RENDER_ELEMENT_NONE) ||
      (element->type > PS_SCENE_RENDER_ELEMENT_FOCUS) ||
      (element->layer >= PS_SCENE_RENDER_LAYER_COUNT) ||
      (element->visible > 1UL) ||
      (element->width == 0U) || (element->height == 0U))
  {
    return 0UL;
  }
  x_end = (uint32_t)element->x + (uint32_t)element->width;
  y_end = (uint32_t)element->y + (uint32_t)element->height;
  if ((x_end > PS_SCENE_RENDER_CANVAS_WIDTH) ||
      (y_end > PS_SCENE_RENDER_CANVAS_HEIGHT))
  {
    return 0UL;
  }

  if (element->type == PS_SCENE_RENDER_ELEMENT_TEXT)
  {
    return ((element->asset_id != 0UL) &&
            ((element->style_id == PS_SCENE_RENDER_STYLE_TEXT_2X_LEFT) ||
             (element->style_id ==
              PS_SCENE_RENDER_STYLE_TEXT_2X_CENTER))) ? 1UL : 0UL;
  }
  if (element->type == PS_SCENE_RENDER_ELEMENT_SPRITE_1BPP)
  {
    return ((element->asset_id != 0UL) &&
            (element->style_id == PS_SCENE_RENDER_STYLE_NONE)) ? 1UL : 0UL;
  }
  if (element->type == PS_SCENE_RENDER_ELEMENT_FOCUS)
  {
    return (element->style_id == PS_SCENE_RENDER_STYLE_NONE) ? 1UL : 0UL;
  }
  return ((element->asset_id == 0UL) &&
          (element->style_id == PS_SCENE_RENDER_STYLE_NONE)) ? 1UL : 0UL;
}

static uint32_t PS_SceneRuntime_ValidateStateScene(
  const ps_scene_runtime_state_scene_t *scene)
{
  uint32_t state_index;
  uint32_t visual_binding_index;
  uint32_t compare_index;
  uint32_t route_index;
  uint32_t variable_index;
  uint32_t guard_index;
  uint32_t action_index;
  uint32_t transition_index;

  g_ps_scene_runtime_probe.descriptor_validate_count++;
  if ((scene == NULL) ||
      (scene->api_version != PS_SCENE_RUNTIME_API_VERSION) ||
      (scene->scene_id == 0UL) ||
      (scene->state_count == 0UL) ||
      (scene->state_count > PS_SCENE_RUNTIME_STATE_MAX) ||
      (scene->visual_binding_count == 0UL) ||
      (scene->visual_binding_count > PS_SCENE_RUNTIME_VISUAL_BINDING_MAX) ||
      (scene->input_route_count > PS_SCENE_RUNTIME_INPUT_ROUTE_MAX) ||
      (scene->variable_count > PS_SCENE_RUNTIME_VARIABLE_MAX) ||
      (scene->guard_count > PS_SCENE_RUNTIME_GUARD_MAX) ||
      (scene->action_count > PS_SCENE_RUNTIME_ACTION_MAX) ||
      (scene->transition_count > PS_SCENE_RUNTIME_TRANSITION_MAX) ||
      ((scene->meaningful_input_mask >> scene->input_route_count) != 0UL) ||
      ((scene->interaction_mode == PS_SCENE_RUNTIME_INTERACTION_CONTINUOUS) &&
       (scene->inactive_route != 0UL)) ||
      ((scene->interaction_mode == PS_SCENE_RUNTIME_INTERACTION_TIMEOUT) &&
       ((scene->inactive_route < PS_SCENE_RUNTIME_INACTIVE_PRESERVE) ||
        (scene->inactive_route > PS_SCENE_RUNTIME_INACTIVE_EXIT_SHELL))) ||
      ((scene->interaction_mode != PS_SCENE_RUNTIME_INTERACTION_CONTINUOUS) &&
       (scene->interaction_mode != PS_SCENE_RUNTIME_INTERACTION_TIMEOUT)))
  {
    return 1UL;
  }

  for (state_index = 0UL; state_index < scene->state_count; ++state_index)
  {
    const ps_scene_runtime_state_t *state = &scene->states[state_index];
    uint32_t state_binding_index = PS_SceneRuntime_FindVisualBindingIndex(
      scene, state->visual_binding_id);

    if ((state->state_id == 0UL) || (state->visual_binding_id == 0UL) ||
        (state_binding_index == PS_SCENE_RUNTIME_INDEX_INVALID) ||
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

  for (visual_binding_index = 0UL;
       visual_binding_index < scene->visual_binding_count;
       ++visual_binding_index)
  {
    const ps_scene_runtime_visual_binding_t *binding =
      &scene->visual_bindings[visual_binding_index];
    uint32_t element_index;
    uint32_t focus_count = 0UL;

    if ((binding->visual_binding_id == 0UL) ||
        (binding->element_count == 0UL) ||
        (binding->element_count > PS_SCENE_RENDER_MODEL_ELEMENT_MAX) ||
        (binding->waiting_visual.api_version !=
         PS_SCENE_WAITING_VISUAL_API_VERSION) ||
        (binding->waiting_visual.sequence_step_count == 0UL) ||
        (binding->waiting_visual.sequence_step_count >
         PS_SCENE_WAITING_VISUAL_SEQUENCE_MAX) ||
        (binding->waiting_visual.element_count == 0UL) ||
        (binding->waiting_visual.element_count >
         PS_SCENE_WAITING_VISUAL_ELEMENT_MAX))
    {
      return 1UL;
    }
    for (element_index = 0UL; element_index < binding->element_count;
         ++element_index)
    {
      const ps_scene_render_element_t *element =
        &binding->elements[element_index];

      if (PS_SceneRuntime_RenderElementValid(element) == 0UL)
      {
        return 1UL;
      }
      if (element->type == PS_SCENE_RENDER_ELEMENT_FOCUS)
      {
        if ((element->visible == 0UL) ||
            (element->animation_binding_id !=
             PS_SCENE_RENDER_ANIMATION_CURSOR))
        {
          return 1UL;
        }
        focus_count++;
      }
      for (compare_index = element_index + 1UL;
           compare_index < binding->element_count;
           ++compare_index)
      {
        if (element->element_id ==
            binding->elements[compare_index].element_id)
        {
          return 1UL;
        }
      }
    }
    if (focus_count != 1UL)
    {
      return 1UL;
    }
    for (compare_index = visual_binding_index + 1UL;
         compare_index < scene->visual_binding_count;
         ++compare_index)
    {
      if (binding->visual_binding_id ==
          scene->visual_bindings[compare_index].visual_binding_id)
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

  for (route_index = 0UL;
       route_index < scene->input_route_count;
       ++route_index)
  {
    const ps_scene_runtime_input_route_t *route =
      &scene->input_routes[route_index];
    uint32_t event_found = 0UL;

    if ((route->logical_event == 0UL) || (route->input_id == 0UL) ||
        (route->scene_event_id == 0UL))
    {
      return 1UL;
    }
    for (compare_index = route_index + 1UL;
         compare_index < scene->input_route_count;
         ++compare_index)
    {
      if ((route->logical_event ==
           scene->input_routes[compare_index].logical_event) &&
          (route->input_id == scene->input_routes[compare_index].input_id))
      {
        return 1UL;
      }
    }
    for (transition_index = 0UL;
         transition_index < scene->transition_count;
         ++transition_index)
    {
      if (route->scene_event_id ==
          scene->transitions[transition_index].scene_event_id)
      {
        event_found = 1UL;
        break;
      }
    }
    if (event_found == 0UL)
    {
      return 1UL;
    }
  }

  for (variable_index = 0UL;
       variable_index < scene->variable_count;
       ++variable_index)
  {
    const ps_scene_runtime_variable_t *variable =
      &scene->variables[variable_index];

    if ((variable->variable_id == 0UL) ||
        (variable->value_type != PS_SCENE_RUNTIME_VALUE_S32))
    {
      return 1UL;
    }
    for (compare_index = variable_index + 1UL;
         compare_index < scene->variable_count;
         ++compare_index)
    {
      if (variable->variable_id ==
          scene->variables[compare_index].variable_id)
      {
        return 1UL;
      }
    }
  }

  for (guard_index = 0UL; guard_index < scene->guard_count; ++guard_index)
  {
    const ps_scene_runtime_guard_t *guard = &scene->guards[guard_index];

    if ((PS_SceneRuntime_FindVariableIndex(scene, guard->variable_id) ==
         PS_SCENE_RUNTIME_INDEX_INVALID) ||
        (guard->compare < PS_SCENE_RUNTIME_COMPARE_EQ) ||
        (guard->compare > PS_SCENE_RUNTIME_COMPARE_GE))
    {
      return 1UL;
    }
  }

  for (action_index = 0UL; action_index < scene->action_count; ++action_index)
  {
    const ps_scene_runtime_action_t *action = &scene->actions[action_index];

    if ((PS_SceneRuntime_FindVariableIndex(scene, action->variable_id) ==
         PS_SCENE_RUNTIME_INDEX_INVALID) ||
        (action->mutation < PS_SCENE_RUNTIME_MUTATION_SET) ||
        (action->mutation > PS_SCENE_RUNTIME_MUTATION_SUBTRACT))
    {
      return 1UL;
    }
  }

  for (transition_index = 0UL;
       transition_index < scene->transition_count;
       ++transition_index)
  {
    const ps_scene_runtime_transition_t *transition =
      &scene->transitions[transition_index];

    if ((transition->transition_id == 0UL) ||
        (transition->scene_event_id == 0UL) ||
        (PS_SceneRuntime_FindStateIndex(scene, transition->source_state_id) ==
         PS_SCENE_RUNTIME_INDEX_INVALID) ||
        (!(((transition->target_scene_id == 0UL) &&
            (PS_SceneRuntime_FindStateIndex(
               scene, transition->target_state_id) !=
             PS_SCENE_RUNTIME_INDEX_INVALID)) ||
           ((transition->target_scene_id != 0UL) &&
            (transition->target_state_id == 0UL) &&
            (transition->target_scene_id <=
             PS_EggStateLoader_SceneCount()) &&
            (transition->action_count == 0UL)))) ||
        (PS_SceneRuntime_RangeValid(transition->first_guard,
                                    transition->guard_count,
                                    scene->guard_count) == 0UL) ||
        (PS_SceneRuntime_RangeValid(transition->first_action,
                                    transition->action_count,
                                    scene->action_count) == 0UL))
    {
      return 1UL;
    }
    for (compare_index = transition_index + 1UL;
         compare_index < scene->transition_count;
         ++compare_index)
    {
      if ((transition->transition_id ==
           scene->transitions[compare_index].transition_id) ||
          ((transition->source_state_id ==
           scene->transitions[compare_index].source_state_id) &&
           (transition->scene_event_id ==
            scene->transitions[compare_index].scene_event_id)))
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
    &s_ps_scene_runtime_state_scene->states[state_index];

  g_ps_scene_runtime_probe.state_index = state_index;
  g_ps_scene_runtime_probe.state_id = state->state_id;
  g_ps_scene_runtime_probe.visual_binding_id = state->visual_binding_id;
  g_ps_scene_runtime_probe.focus_index = state->focus_index;
}

static uint32_t PS_SceneRuntime_GuardPasses(
  const ps_scene_runtime_state_scene_t *scene,
  const ps_scene_runtime_guard_t *guard)
{
  uint32_t variable_index = PS_SceneRuntime_FindVariableIndex(
    scene, guard->variable_id);
  int32_t actual;

  if (variable_index == PS_SCENE_RUNTIME_INDEX_INVALID)
  {
    return 0UL;
  }
  actual = s_ps_scene_runtime_variables[variable_index];

  switch ((ps_scene_runtime_compare_t)guard->compare)
  {
    case PS_SCENE_RUNTIME_COMPARE_EQ:
      return (actual == guard->value) ? 1UL : 0UL;
    case PS_SCENE_RUNTIME_COMPARE_NE:
      return (actual != guard->value) ? 1UL : 0UL;
    case PS_SCENE_RUNTIME_COMPARE_LT:
      return (actual < guard->value) ? 1UL : 0UL;
    case PS_SCENE_RUNTIME_COMPARE_LE:
      return (actual <= guard->value) ? 1UL : 0UL;
    case PS_SCENE_RUNTIME_COMPARE_GT:
      return (actual > guard->value) ? 1UL : 0UL;
    case PS_SCENE_RUNTIME_COMPARE_GE:
      return (actual >= guard->value) ? 1UL : 0UL;
    default:
      return 0UL;
  }
}

static uint32_t PS_SceneRuntime_TransitionGuardsPass(
  const ps_scene_runtime_state_scene_t *scene,
  const ps_scene_runtime_transition_t *transition)
{
  uint32_t index;

  for (index = 0UL; index < transition->guard_count; ++index)
  {
    const ps_scene_runtime_guard_t *guard =
      &scene->guards[transition->first_guard + index];

    g_ps_scene_runtime_probe.guard_evaluate_count++;
    if (PS_SceneRuntime_GuardPasses(scene, guard) == 0UL)
    {
      g_ps_scene_runtime_probe.guard_reject_count++;
      return 0UL;
    }
    g_ps_scene_runtime_probe.guard_pass_count++;
  }

  return 1UL;
}

static uint32_t PS_SceneRuntime_StageActions(
  const ps_scene_runtime_state_scene_t *scene,
  const ps_scene_runtime_transition_t *transition,
  int32_t *staged_variables)
{
  uint32_t index;

  (void)memcpy(staged_variables,
               s_ps_scene_runtime_variables,
               sizeof(s_ps_scene_runtime_variables));
  for (index = 0UL; index < transition->action_count; ++index)
  {
    const ps_scene_runtime_action_t *action =
      &scene->actions[transition->first_action + index];
    uint32_t variable_index = PS_SceneRuntime_FindVariableIndex(
      scene, action->variable_id);
    int64_t result;

    if (variable_index == PS_SCENE_RUNTIME_INDEX_INVALID)
    {
      g_ps_scene_runtime_probe.action_error_count++;
      return 0UL;
    }

    switch ((ps_scene_runtime_mutation_t)action->mutation)
    {
      case PS_SCENE_RUNTIME_MUTATION_SET:
        result = (int64_t)action->value;
        break;
      case PS_SCENE_RUNTIME_MUTATION_ADD:
        result = (int64_t)staged_variables[variable_index] +
                 (int64_t)action->value;
        break;
      case PS_SCENE_RUNTIME_MUTATION_SUBTRACT:
        result = (int64_t)staged_variables[variable_index] -
                 (int64_t)action->value;
        break;
      default:
        g_ps_scene_runtime_probe.action_error_count++;
        return 0UL;
    }

    if ((result < (int64_t)INT32_MIN) || (result > (int64_t)INT32_MAX))
    {
      g_ps_scene_runtime_probe.action_error_count++;
      return 0UL;
    }
    staged_variables[variable_index] = (int32_t)result;
  }

  return 1UL;
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

static uint32_t PS_SceneRuntime_ActivateDecodedScene(
  ps_scene_runtime_state_scene_t *scene,
  uint32_t slot)
{
  uint32_t entry_index;
  uint32_t variable_index;

  entry_index = PS_SceneRuntime_FindStateIndex(scene, scene->entry_state_id);
  if ((entry_index == PS_SCENE_RUNTIME_INDEX_INVALID) || (slot > 1UL))
  {
    return 1UL;
  }
  s_ps_scene_runtime_state_scene = scene;
  s_ps_scene_runtime_active_slot = slot;
  g_ps_scene_runtime_probe.active = 1UL;
  g_ps_scene_runtime_probe.scene_type = PS_SCENE_RUNTIME_SCENE_TYPE_STATE;
  g_ps_scene_runtime_probe.descriptor_state_count = scene->state_count;
  g_ps_scene_runtime_probe.descriptor_visual_binding_count =
    scene->visual_binding_count;
  g_ps_scene_runtime_probe.descriptor_input_route_count =
    scene->input_route_count;
  g_ps_scene_runtime_probe.descriptor_variable_count = scene->variable_count;
  g_ps_scene_runtime_probe.descriptor_guard_count = scene->guard_count;
  g_ps_scene_runtime_probe.descriptor_action_count = scene->action_count;
  g_ps_scene_runtime_probe.descriptor_transition_count =
    scene->transition_count;
  g_ps_scene_runtime_probe.descriptor_interaction_mode =
    scene->interaction_mode;
  g_ps_scene_runtime_probe.descriptor_inactive_route = scene->inactive_route;
  g_ps_scene_runtime_probe.descriptor_meaningful_input_mask =
    scene->meaningful_input_mask;
  g_ps_scene_runtime_probe.scene_id = scene->scene_id;
  (void)memset(s_ps_scene_runtime_variables,
               0,
               sizeof(s_ps_scene_runtime_variables));
  for (variable_index = 0UL;
       variable_index < scene->variable_count;
       ++variable_index)
  {
    s_ps_scene_runtime_variables[variable_index] =
      scene->variables[variable_index].initial_value;
  }
  g_ps_scene_runtime_probe.primary_variable_value =
    s_ps_scene_runtime_variables[0];
  PS_SceneRuntime_SelectState(entry_index);
  g_ps_scene_runtime_probe.state_revision++;
  g_ps_scene_runtime_probe.timeline_revision++;
  g_ps_scene_runtime_probe.last_action = 0UL;
  g_ps_scene_runtime_probe.last_status = PS_SCENE_RUNTIME_STATUS_OK;
  return 0UL;
}

static uint32_t PS_SceneRuntime_ReplaceStateScene(uint32_t target_scene_id)
{
  uint32_t target_slot = (s_ps_scene_runtime_active_slot == 0UL) ? 1UL : 0UL;
  ps_scene_runtime_state_scene_t *target =
    &s_ps_scene_runtime_scene_slots[target_slot];
  uint32_t source_scene_id = g_ps_scene_runtime_probe.scene_id;

  g_ps_scene_runtime_probe.scene_replace_count++;
  g_ps_scene_runtime_probe.scene_replace_source_id = source_scene_id;
  g_ps_scene_runtime_probe.scene_replace_target_id = target_scene_id;
  g_ps_scene_runtime_probe.scene_replace_status =
    PS_SCENE_RUNTIME_STATUS_NOT_RUN;
  if ((PS_EggStateLoader_LoadScene(target_scene_id, target) != 0UL) ||
      (PS_SceneRuntime_ValidateStateScene(target) != 0UL) ||
      (PS_SceneRuntime_ActivateDecodedScene(target, target_slot) != 0UL))
  {
    g_ps_scene_runtime_probe.scene_replace_fail_count++;
    g_ps_scene_runtime_probe.scene_replace_status =
      PS_SCENE_RUNTIME_STATUS_ERROR;
    return 1UL;
  }
  g_ps_scene_runtime_probe.exit_count++;
  g_ps_scene_runtime_probe.enter_count++;
  g_ps_scene_runtime_probe.scene_replace_status = PS_SCENE_RUNTIME_STATUS_OK;
  return 0UL;
}

uint32_t PS_SceneRuntime_EnterStateScene(void)
{
  ps_package_source_view_t package_view;
  uint32_t scene_index;
  uint32_t entry_scene_id;
  ps_scene_runtime_state_scene_t *entry_scene =
    &s_ps_scene_runtime_scene_slots[0];
  ps_scene_runtime_state_scene_t *validation_scene =
    &s_ps_scene_runtime_scene_slots[1];

  g_ps_scene_runtime_probe.enter_count++;
  g_ps_scene_runtime_probe.last_status = PS_SCENE_RUNTIME_STATUS_NOT_RUN;
  g_ps_scene_runtime_probe.activation_status =
    PS_SCENE_RUNTIME_STATUS_NOT_RUN;
  g_ps_scene_runtime_probe.package_source =
    (uint32_t)PS_PACKAGE_SOURCE_NONE;
  g_ps_scene_runtime_probe.package_source_status =
    PS_PACKAGE_SOURCE_STATUS_NOT_RUN;
  if (PS_PackageSource_Resolve(&package_view) != 0UL)
  {
    g_ps_scene_runtime_probe.package_source_status =
      g_ps_package_source_probe.last_status;
    g_ps_scene_runtime_probe.active = 0UL;
    if (g_ps_package_source_probe.reason ==
        (uint32_t)PS_PACKAGE_SOURCE_REASON_UNAVAILABLE)
    {
      g_ps_scene_runtime_probe.last_status =
        PS_SCENE_RUNTIME_STATUS_NO_PACKAGE;
      g_ps_scene_runtime_probe.activation_status =
        PS_SCENE_RUNTIME_STATUS_NO_PACKAGE;
      return PS_SCENE_RUNTIME_INDEX_INVALID;
    }
    g_ps_scene_runtime_probe.reject_count++;
    g_ps_scene_runtime_probe.last_status = PS_SCENE_RUNTIME_STATUS_ERROR;
    g_ps_scene_runtime_probe.activation_status =
      PS_SCENE_RUNTIME_STATUS_ERROR;
    return PS_SCENE_RUNTIME_INDEX_INVALID;
  }
  g_ps_scene_runtime_probe.package_source = package_view.source;
  g_ps_scene_runtime_probe.package_source_status =
    g_ps_package_source_probe.last_status;
  if (PS_EggStateLoader_Load(package_view.blob,
                             package_view.size,
                             entry_scene) != 0UL)
  {
    g_ps_scene_runtime_probe.reject_count++;
    g_ps_scene_runtime_probe.active = 0UL;
    g_ps_scene_runtime_probe.last_status = PS_SCENE_RUNTIME_STATUS_ERROR;
    g_ps_scene_runtime_probe.activation_status =
      PS_SCENE_RUNTIME_STATUS_ERROR;
    return PS_SCENE_RUNTIME_INDEX_INVALID;
  }
  entry_scene_id = PS_EggStateLoader_EntrySceneId();
  for (scene_index = 1UL;
       scene_index <= PS_EggStateLoader_SceneCount();
       ++scene_index)
  {
    if (scene_index == entry_scene_id)
    {
      continue;
    }
    if ((PS_EggStateLoader_LoadScene(scene_index, validation_scene) != 0UL) ||
        (PS_SceneRuntime_ValidateStateScene(validation_scene) != 0UL) ||
        (validation_scene->interaction_mode !=
         entry_scene->interaction_mode))
    {
      g_ps_scene_runtime_probe.reject_count++;
      g_ps_scene_runtime_probe.active = 0UL;
      g_ps_scene_runtime_probe.last_status = PS_SCENE_RUNTIME_STATUS_ERROR;
      g_ps_scene_runtime_probe.activation_status =
        PS_SCENE_RUNTIME_STATUS_ERROR;
      return PS_SCENE_RUNTIME_INDEX_INVALID;
    }
  }
  if ((PS_EggStateLoader_LoadScene(entry_scene_id, entry_scene) != 0UL) ||
      (PS_SceneRuntime_ValidateStateScene(entry_scene) != 0UL) ||
      (PS_SceneRuntime_ActivateDecodedScene(entry_scene, 0UL) != 0UL))
  {
    g_ps_scene_runtime_probe.reject_count++;
    g_ps_scene_runtime_probe.active = 0UL;
    g_ps_scene_runtime_probe.last_status = PS_SCENE_RUNTIME_STATUS_ERROR;
    g_ps_scene_runtime_probe.activation_status =
      PS_SCENE_RUNTIME_STATUS_ERROR;
    return PS_SCENE_RUNTIME_INDEX_INVALID;
  }
  g_ps_scene_runtime_probe.activation_status =
    PS_SCENE_RUNTIME_STATUS_OK;
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

uint32_t PS_SceneRuntime_InteractionMode(void)
{
  return (s_ps_scene_runtime_state_scene != NULL) ?
    s_ps_scene_runtime_state_scene->interaction_mode : 0UL;
}

uint32_t PS_SceneRuntime_InactiveRoute(void)
{
  return (s_ps_scene_runtime_state_scene != NULL) ?
    s_ps_scene_runtime_state_scene->inactive_route : 0UL;
}

uint32_t PS_SceneRuntime_InputIsMeaningful(uint32_t logical_event,
                                           uint32_t input_id)
{
  const ps_scene_runtime_state_scene_t *scene =
    s_ps_scene_runtime_state_scene;
  uint32_t route_index;

  if (scene == NULL)
  {
    return 0UL;
  }
  for (route_index = 0UL; route_index < scene->input_route_count;
       ++route_index)
  {
    if ((scene->input_routes[route_index].logical_event == logical_event) &&
        (scene->input_routes[route_index].input_id == input_id))
    {
      return ((scene->meaningful_input_mask & (1UL << route_index)) != 0UL) ?
        1UL : 0UL;
    }
  }
  return 0UL;
}

const ps_scene_render_model_t *PS_SceneRuntime_ResolveStateSceneRenderModel(
  void)
{
  const ps_scene_runtime_state_scene_t *scene =
    s_ps_scene_runtime_state_scene;
  const ps_scene_runtime_visual_binding_t *binding;
  ps_scene_render_model_t *model = &s_ps_scene_runtime_render_model;
  uint32_t binding_index;
  uint32_t element_index;
  uint32_t focus_element_id = 0UL;
  uint32_t sprite_count = 0UL;

  g_ps_scene_runtime_probe.render_model_resolve_count++;
  g_ps_scene_runtime_probe.render_model_status =
    PS_SCENE_RUNTIME_STATUS_NOT_RUN;
  if ((g_ps_scene_runtime_probe.active == 0UL) ||
      (g_ps_scene_runtime_probe.focus_index >= scene->state_count))
  {
    g_ps_scene_runtime_probe.reject_count++;
    g_ps_scene_runtime_probe.render_model_status = 1UL;
    return NULL;
  }

  binding_index = PS_SceneRuntime_FindVisualBindingIndex(
    scene, g_ps_scene_runtime_probe.visual_binding_id);
  if (binding_index == PS_SCENE_RUNTIME_INDEX_INVALID)
  {
    g_ps_scene_runtime_probe.reject_count++;
    g_ps_scene_runtime_probe.render_model_status = 1UL;
    return NULL;
  }
  binding = &scene->visual_bindings[binding_index];

  (void)memset(model, 0, sizeof(*model));
  model->api_version = PS_SCENE_RENDER_MODEL_API_VERSION;
  model->scene_id = g_ps_scene_runtime_probe.scene_id;
  model->state_id = g_ps_scene_runtime_probe.state_id;
  model->visual_binding_id = g_ps_scene_runtime_probe.visual_binding_id;
  model->content_revision = g_ps_scene_runtime_probe.state_revision;
  model->timeline_revision = g_ps_scene_runtime_probe.timeline_revision;
  model->focus_index = g_ps_scene_runtime_probe.focus_index;
  model->element_count = binding->element_count;
  for (element_index = 0UL; element_index < binding->element_count;
       ++element_index)
  {
    model->elements[element_index] = binding->elements[element_index];
    if (binding->elements[element_index].type ==
        PS_SCENE_RENDER_ELEMENT_FOCUS)
    {
      focus_element_id = binding->elements[element_index].element_id;
    }
    if (binding->elements[element_index].type ==
        PS_SCENE_RENDER_ELEMENT_SPRITE_1BPP)
    {
      sprite_count++;
    }
  }
  model->waiting_sequence_step_count =
    binding->waiting_visual.sequence_step_count;
  model->waiting_marker_enabled =
    (binding->waiting_visual.element_count > 1UL) ? 1UL : 0UL;

  g_ps_scene_runtime_probe.render_model_scene_id = model->scene_id;
  g_ps_scene_runtime_probe.render_model_state_id = model->state_id;
  g_ps_scene_runtime_probe.render_model_visual_binding_id =
    model->visual_binding_id;
  g_ps_scene_runtime_probe.render_model_content_revision =
    model->content_revision;
  g_ps_scene_runtime_probe.render_model_timeline_revision =
    model->timeline_revision;
  g_ps_scene_runtime_probe.render_model_focus_index = model->focus_index;
  g_ps_scene_runtime_probe.render_model_element_count = model->element_count;
  g_ps_scene_runtime_probe.render_model_focus_element_id = focus_element_id;
  g_ps_scene_runtime_probe.render_model_sprite_count = sprite_count;
  g_ps_scene_runtime_probe.render_model_status = 0UL;
  return model;
}

uint32_t PS_SceneRuntime_HandleStateSceneInput(uint32_t logical_event,
                                               uint32_t input_id)
{
  const ps_scene_runtime_state_scene_t *scene =
    s_ps_scene_runtime_state_scene;
  uint32_t route_index;
  uint32_t scene_event_id = 0UL;
  uint32_t transition_index;
  int32_t staged_variables[PS_SCENE_RUNTIME_VARIABLE_MAX];

  if (g_ps_scene_runtime_probe.active == 0UL)
  {
    g_ps_scene_runtime_probe.last_status = 1UL;
    return PS_SCENE_RUNTIME_INPUT_ERROR;
  }

  for (route_index = 0UL; route_index < scene->input_route_count; ++route_index)
  {
    const ps_scene_runtime_input_route_t *route =
      &scene->input_routes[route_index];

    if ((route->logical_event == logical_event) &&
        (route->input_id == input_id))
    {
      scene_event_id = route->scene_event_id;
      break;
    }
  }
  if (scene_event_id == 0UL)
  {
    g_ps_scene_runtime_probe.input_route_miss_count++;
    g_ps_scene_runtime_probe.last_status = 0UL;
    return PS_SCENE_RUNTIME_INPUT_IGNORED;
  }

  g_ps_scene_runtime_probe.input_route_match_count++;
  g_ps_scene_runtime_probe.action_count++;
  g_ps_scene_runtime_probe.last_action = scene_event_id;
  g_ps_scene_runtime_probe.last_scene_event_id = scene_event_id;

  for (transition_index = 0UL;
       transition_index < scene->transition_count;
       ++transition_index)
  {
    const ps_scene_runtime_transition_t *transition =
      &scene->transitions[transition_index];

    if ((transition->source_state_id == g_ps_scene_runtime_probe.state_id) &&
        (transition->scene_event_id == scene_event_id))
    {
      uint32_t target_index = PS_SCENE_RUNTIME_INDEX_INVALID;

      if (transition->target_scene_id == 0UL)
      {
        target_index = PS_SceneRuntime_FindStateIndex(
          scene, transition->target_state_id);
      }
      if ((transition->target_scene_id == 0UL) &&
          (target_index == PS_SCENE_RUNTIME_INDEX_INVALID))
      {
        g_ps_scene_runtime_probe.action_error_count++;
        g_ps_scene_runtime_probe.last_status = 1UL;
        return PS_SCENE_RUNTIME_INPUT_ERROR;
      }
      if (PS_SceneRuntime_TransitionGuardsPass(scene, transition) == 0UL)
      {
        g_ps_scene_runtime_probe.last_status = 0UL;
        return PS_SCENE_RUNTIME_INPUT_IGNORED;
      }
      if (PS_SceneRuntime_StageActions(scene,
                                       transition,
                                       staged_variables) == 0UL)
      {
        g_ps_scene_runtime_probe.last_status = 1UL;
        return PS_SCENE_RUNTIME_INPUT_ERROR;
      }

      if (transition->target_scene_id != 0UL)
      {
        if (PS_SceneRuntime_ReplaceStateScene(
              transition->target_scene_id) != 0UL)
        {
          g_ps_scene_runtime_probe.action_error_count++;
          g_ps_scene_runtime_probe.last_status =
            PS_SCENE_RUNTIME_STATUS_ERROR;
          return PS_SCENE_RUNTIME_INPUT_ERROR;
        }
      }
      else
      {
        (void)memcpy(s_ps_scene_runtime_variables,
                     staged_variables,
                     sizeof(s_ps_scene_runtime_variables));
        PS_SceneRuntime_SelectState(target_index);
        g_ps_scene_runtime_probe.state_revision++;
        g_ps_scene_runtime_probe.primary_variable_value =
          s_ps_scene_runtime_variables[0];
      }
      g_ps_scene_runtime_probe.transition_match_count++;
      g_ps_scene_runtime_probe.action_commit_count++;
      g_ps_scene_runtime_probe.state_change_count++;
      g_ps_scene_runtime_probe.last_action = scene_event_id;
      g_ps_scene_runtime_probe.last_transition_id = transition->transition_id;
      g_ps_scene_runtime_probe.last_status = 0UL;
      return PS_SCENE_RUNTIME_INPUT_APPLIED;
    }
  }

  g_ps_scene_runtime_probe.transition_miss_count++;
  g_ps_scene_runtime_probe.last_status = 0UL;
  return PS_SCENE_RUNTIME_INPUT_IGNORED;
}

const ps_scene_waiting_visual_t *PS_SceneRuntime_ResolveStateSceneWaitingVisual(
  const ps_scene_render_model_t *model,
  const ps_scene_waiting_visual_bounds_t *cursor_bounds)
{
  const ps_scene_runtime_state_scene_t *scene =
    s_ps_scene_runtime_state_scene;
  const ps_scene_runtime_visual_binding_t *binding;
  uint32_t binding_index;
  uint32_t element_index;

  g_ps_scene_runtime_probe.resolve_count++;
  g_ps_scene_runtime_probe.last_status = PS_SCENE_RUNTIME_STATUS_NOT_RUN;
  if ((model == NULL) ||
      (model->api_version != PS_SCENE_RENDER_MODEL_API_VERSION) ||
      (g_ps_scene_runtime_probe.active == 0UL) ||
      (model->scene_id != g_ps_scene_runtime_probe.scene_id) ||
      (model->state_id != g_ps_scene_runtime_probe.state_id) ||
      (model->visual_binding_id !=
       g_ps_scene_runtime_probe.visual_binding_id) ||
      (model->content_revision !=
       g_ps_scene_runtime_probe.state_revision) ||
      (model->timeline_revision !=
       g_ps_scene_runtime_probe.timeline_revision) ||
      (model->focus_index != g_ps_scene_runtime_probe.focus_index) ||
      (model->element_count == 0UL) ||
      (model->element_count > PS_SCENE_RENDER_MODEL_ELEMENT_MAX))
  {
    g_ps_scene_runtime_probe.reject_count++;
    g_ps_scene_runtime_probe.last_status = 1UL;
    return NULL;
  }
  binding_index = PS_SceneRuntime_FindVisualBindingIndex(
    scene, model->visual_binding_id);
  if ((binding_index == PS_SCENE_RUNTIME_INDEX_INVALID) ||
      (cursor_bounds == NULL))
  {
    g_ps_scene_runtime_probe.reject_count++;
    g_ps_scene_runtime_probe.last_status = 1UL;
    return NULL;
  }
  binding = &scene->visual_bindings[binding_index];
  (void)memcpy(&s_ps_scene_runtime_waiting_visual,
               &binding->waiting_visual,
               sizeof(s_ps_scene_runtime_waiting_visual));
  for (element_index = 0UL;
       element_index < s_ps_scene_runtime_waiting_visual.element_count;
       ++element_index)
  {
    if (s_ps_scene_runtime_waiting_visual.elements[element_index].
          visual_source_id == PS_SCENE_WAITING_VISUAL_SOURCE_SHELL_CURSOR)
    {
      s_ps_scene_runtime_waiting_visual.elements[element_index].logical_bounds =
        *cursor_bounds;
    }
  }
  g_ps_scene_runtime_probe.presentation_id =
    s_ps_scene_runtime_waiting_visual.presentation_id;
  g_ps_scene_runtime_probe.sequence_step_count =
    s_ps_scene_runtime_waiting_visual.sequence_step_count;
  g_ps_scene_runtime_probe.element_count =
    s_ps_scene_runtime_waiting_visual.element_count;
  g_ps_scene_runtime_probe.last_status = 0UL;
  return &s_ps_scene_runtime_waiting_visual;
}
