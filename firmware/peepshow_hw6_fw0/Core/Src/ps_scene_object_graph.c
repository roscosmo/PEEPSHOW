#include "ps_scene_object_graph.h"

#include <limits.h>
#include <stddef.h>
#include <string.h>

static uint32_t VariableIndex(const ps_scene_runtime_state_scene_t *scene, uint32_t id)
{
  uint32_t index;
  for (index = 0UL; index < scene->variable_count; ++index)
  {
    if (scene->variables[index].variable_id == id) { return index; }
  }
  return PS_SCENE_RUNTIME_INDEX_INVALID;
}

static uint32_t GuardsPass(const ps_scene_object_graph_t *bank,
  const ps_scene_runtime_transition_t *transition)
{
  uint32_t index;
  for (index = 0UL; index < transition->guard_count; ++index)
  {
    const ps_scene_runtime_guard_t *guard = &bank->scene->guards[transition->first_guard + index];
    uint32_t variable = VariableIndex(bank->scene, guard->variable_id);
    int32_t value;
    if (variable == PS_SCENE_RUNTIME_INDEX_INVALID) { return 0UL; }
    value = bank->variables[variable];
    switch (guard->compare)
    {
      case PS_SCENE_RUNTIME_COMPARE_EQ: if (value != guard->value) { return 0UL; } break;
      case PS_SCENE_RUNTIME_COMPARE_NE: if (value == guard->value) { return 0UL; } break;
      case PS_SCENE_RUNTIME_COMPARE_LT: if (value >= guard->value) { return 0UL; } break;
      case PS_SCENE_RUNTIME_COMPARE_LE: if (value > guard->value) { return 0UL; } break;
      case PS_SCENE_RUNTIME_COMPARE_GT: if (value <= guard->value) { return 0UL; } break;
      case PS_SCENE_RUNTIME_COMPARE_GE: if (value < guard->value) { return 0UL; } break;
      default: return 0UL;
    }
  }
  return 1UL;
}

uint32_t PS_SceneObjectGraph_Init(ps_scene_object_graph_t *bank,
  const ps_scene_runtime_state_scene_t *scene, uint32_t package_scene_count,
  uint32_t activation)
{
  uint32_t index;
  if ((bank == NULL) || (scene == NULL) ||
      (scene->execution_model != PS_SCENE_RUNTIME_MODEL_OBJECTS) ||
      (PS_SceneRuntime_ValidateDescriptor(scene, package_scene_count) != 0UL) ||
      (PS_SceneObjects_Init(&bank->objects, &scene->object_definition, activation,
        (uint16_t)(scene->entry_state_id - 1UL)) != PS_SCENE_OBJECTS_OK))
  {
    return 1UL;
  }
  bank->scene = scene;
  (void)memset(bank->variables, 0, sizeof(bank->variables));
  for (index = 0UL; index < scene->variable_count; ++index)
  {
    bank->variables[index] = scene->variables[index].initial_value;
  }
  return 0UL;
}

void PS_SceneObjectGraph_Abort(ps_scene_object_graph_stage_t *stage)
{
  if (stage != NULL) { (void)memset(stage, 0, sizeof(*stage)); }
}

uint32_t PS_SceneObjectGraph_StageEvent(const ps_scene_object_graph_t *bank,
  uint32_t binding, ps_scene_object_graph_stage_t *stage)
{
  const ps_scene_runtime_state_scene_t *scene;
  uint32_t index;
  PS_SceneObjectGraph_Abort(stage);
  if ((bank == NULL) || (stage == NULL) || (bank->scene == NULL) ||
      (bank->objects.activation == 0UL) || (bank->objects.suspended != 0U) ||
      (binding >= bank->scene->event_binding_count))
  {
    return PS_SCENE_RUNTIME_INPUT_ERROR;
  }
  scene = bank->scene;
  for (index = 0UL; index < scene->transition_count; ++index)
  {
    const ps_scene_runtime_transition_t *transition = &scene->transitions[index];
    uint32_t action_index;
    if ((transition->scene_event_id != binding + 1UL) ||
        ((transition->source_state_id != 0UL) &&
         (transition->source_state_id != (uint32_t)bank->objects.state + 1UL)))
    {
      continue;
    }
    if (GuardsPass(bank, transition) == 0UL) { return PS_SCENE_RUNTIME_INPUT_IGNORED; }
    if (PS_SceneObjects_Begin(&bank->objects, &stage->objects) != PS_SCENE_OBJECTS_OK)
    {
      return PS_SCENE_RUNTIME_INPUT_ERROR;
    }
    stage->origin = bank;
    (void)memcpy(stage->original_variables, bank->variables, sizeof(bank->variables));
    (void)memcpy(stage->variables, bank->variables, sizeof(bank->variables));
    stage->effects.transition_id = transition->transition_id;
    stage->effects.target_scene_id = transition->target_scene_id;
    stage->effects.state_entered = (transition->target_state_id != 0UL);
    for (action_index = 0UL; action_index < transition->action_count; ++action_index)
    {
      const ps_scene_runtime_action_t *action = &scene->actions[transition->first_action + action_index];
      if (action->kind == PS_SCENE_RUNTIME_ACTION_OBJECT_OPERATION)
      {
        if (PS_SceneObjects_Apply(&stage->objects, (uint16_t)action->target_id) != PS_SCENE_OBJECTS_OK)
        {
          PS_SceneObjectGraph_Abort(stage);
          return PS_SCENE_RUNTIME_INPUT_ERROR;
        }
      }
      else if (action->kind == PS_SCENE_RUNTIME_ACTION_SET_VARIABLE)
      {
        uint32_t variable = VariableIndex(scene, action->target_id);
        int64_t result;
        if (variable == PS_SCENE_RUNTIME_INDEX_INVALID)
        {
          PS_SceneObjectGraph_Abort(stage);
          return PS_SCENE_RUNTIME_INPUT_ERROR;
        }
        result = stage->variables[variable];
        switch (action->operation)
        {
          case PS_SCENE_RUNTIME_MUTATION_SET: result = action->value; break;
          case PS_SCENE_RUNTIME_MUTATION_ADD: result += action->value; break;
          case PS_SCENE_RUNTIME_MUTATION_SUBTRACT: result -= action->value; break;
          default: PS_SceneObjectGraph_Abort(stage); return PS_SCENE_RUNTIME_INPUT_ERROR;
        }
        if ((result < INT32_MIN) || (result > INT32_MAX))
        {
          PS_SceneObjectGraph_Abort(stage);
          return PS_SCENE_RUNTIME_INPUT_ERROR;
        }
        stage->variables[variable] = (int32_t)result;
      }
      else if ((action->kind == PS_SCENE_RUNTIME_ACTION_PLAY_SFX) ||
               (action->kind == PS_SCENE_RUNTIME_ACTION_EXIT_TO_SHELL) ||
               ((action->kind >= PS_SCENE_RUNTIME_ACTION_START_TIMER) &&
                (action->kind <= PS_SCENE_RUNTIME_ACTION_CANCEL_TIMER)))
      {
        stage->effects.actions[stage->effects.count++] = *action;
      }
      else
      {
        PS_SceneObjectGraph_Abort(stage);
        return PS_SCENE_RUNTIME_INPUT_ERROR;
      }
    }
    if ((transition->target_state_id != 0UL) &&
        (PS_SceneObjects_SelectState(&stage->objects,
          (uint16_t)(transition->target_state_id - 1UL)) != PS_SCENE_OBJECTS_OK))
    {
      PS_SceneObjectGraph_Abort(stage);
      return PS_SCENE_RUNTIME_INPUT_ERROR;
    }
    return PS_SCENE_RUNTIME_INPUT_APPLIED;
  }
  return PS_SCENE_RUNTIME_INPUT_IGNORED;
}

uint32_t PS_SceneObjectGraph_Commit(ps_scene_object_graph_t *bank,
  ps_scene_object_graph_stage_t *stage, ps_scene_object_effects_t *effects)
{
  if ((bank == NULL) || (stage == NULL) || (effects == NULL) ||
      (stage->origin != bank) || (stage->effects.target_scene_id != 0UL) ||
      (memcmp(stage->original_variables, bank->variables, sizeof(bank->variables)) != 0) ||
      (PS_SceneObjects_Commit(&bank->objects, &stage->objects) != PS_SCENE_OBJECTS_OK))
  {
    PS_SceneObjectGraph_Abort(stage);
    return 1UL;
  }
  (void)memcpy(bank->variables, stage->variables, sizeof(bank->variables));
  *effects = stage->effects;
  PS_SceneObjectGraph_Abort(stage);
  return 0UL;
}
