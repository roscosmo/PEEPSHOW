#include "ps_scene_objects.h"

#include <stddef.h>
#include <string.h>

#include "ps_scene_runtime.h"

static uint32_t Active(const ps_scene_objects_t *bank)
{
  return (bank != NULL) && (bank->activation != 0U) &&
    (bank->definition.object_count != 0U) &&
    (bank->definition.object_count <= PS_SCENE_RENDER_MODEL_ELEMENT_MAX) &&
    (bank->state < bank->definition.state_count);
}

static int16_t Clamp(int64_t value, uint16_t maximum)
{
  if (value < 0)
  {
    return 0;
  }
  return (value > maximum) ? (int16_t)maximum : (int16_t)value;
}

void PS_SceneObjects_Clear(ps_scene_objects_t *bank)
{
  if (bank != NULL)
  {
    memset(bank, 0, sizeof(*bank));
  }
}

ps_scene_objects_status_t PS_SceneObjects_Init(ps_scene_objects_t *bank,
  const ps_egg_object_view_t *definition, uint32_t activation, uint16_t state)
{
  ps_scene_objects_t candidate = {0};
  uint16_t index;
  uint16_t animated = 0U;
  if ((bank == NULL) || (definition == NULL) || (activation == 0U) ||
      (definition->object_count == 0U) || (state >= definition->state_count))
  {
    return PS_SCENE_OBJECTS_ARGUMENT;
  }
  if ((definition->object_count > PS_SCENE_RENDER_MODEL_ELEMENT_MAX) ||
      (definition->state_count > PS_SCENE_RUNTIME_STATE_MAX))
  {
    return PS_SCENE_OBJECTS_CAPACITY;
  }
  candidate.definition = *definition;
  candidate.activation = activation;
  candidate.serial = 1U;
  candidate.content_revision = 1U;
  candidate.state = state;
  for (index = 0U; index < definition->object_count; ++index)
  {
    ps_egg_object_definition_t object;
    ps_scene_object_live_t *live = &candidate.objects[index];
    uint16_t steps = PS_EggObject_ClipStepCount(definition, index);
    uint16_t phase_frames[PS_SCENE_WAITING_VISUAL_PHASE_MAX];
    uint16_t phases = 0U;
    uint16_t step;
    if (!PS_EggObject_GetDefinition(definition, index, &object))
    {
      return PS_SCENE_OBJECTS_ARGUMENT;
    }
    live->x = object.x;
    live->y = object.y;
    live->visible = object.flags & 1U;
    live->static_frame = PS_EGG_OBJECT_REF_NONE;
    if (steps == 0U)
    {
      continue;
    }
    if ((++animated > PS_SCENE_WAITING_VISUAL_ELEMENT_MAX) ||
        (steps > PS_SCENE_WAITING_VISUAL_SEQUENCE_MAX))
    {
      return PS_SCENE_OBJECTS_CAPACITY;
    }
    for (step = 0U; step < steps; ++step)
    {
      uint16_t frame;
      uint16_t phase;
      uint32_t duration;
      if (!PS_EggObject_GetClipStep(definition, index, step, &frame, &duration))
      {
        return PS_SCENE_OBJECTS_ARGUMENT;
      }
      for (phase = 0U; phase < phases; ++phase)
      {
        if (phase_frames[phase] == frame)
        {
          break;
        }
      }
      if (phase == phases)
      {
        if (phases >= PS_SCENE_WAITING_VISUAL_PHASE_MAX)
        {
          return PS_SCENE_OBJECTS_CAPACITY;
        }
        phase_frames[phases++] = frame;
      }
      live->cycle_ms += duration;
    }
  }
  *bank = candidate;
  return PS_SCENE_OBJECTS_OK;
}

void PS_SceneObjects_Abort(ps_scene_objects_stage_t *stage)
{
  if (stage != NULL)
  {
    memset(stage, 0, sizeof(*stage));
  }
}

ps_scene_objects_status_t PS_SceneObjects_Begin(const ps_scene_objects_t *bank,
  ps_scene_objects_stage_t *stage)
{
  if (stage == NULL)
  {
    return PS_SCENE_OBJECTS_ARGUMENT;
  }
  PS_SceneObjects_Abort(stage);
  if (!Active(bank) || bank->suspended)
  {
    return PS_SCENE_OBJECTS_ARGUMENT;
  }
  stage->candidate = *bank;
  stage->origin = bank;
  stage->origin_serial = bank->serial;
  stage->valid = 1U;
  return PS_SCENE_OBJECTS_OK;
}

ps_scene_objects_status_t PS_SceneObjects_Apply(ps_scene_objects_stage_t *stage,
  uint16_t operation)
{
  ps_egg_object_operation_t action;
  ps_egg_object_definition_t definition;
  ps_scene_object_live_t *object;
  if ((stage == NULL) || !stage->valid)
  {
    return PS_SCENE_OBJECTS_TRANSACTION;
  }
  if ((stage->action_count >= PS_SCENE_RUNTIME_ACTION_MAX) ||
      !PS_EggObject_GetOperation(&stage->candidate.definition, operation, &action) ||
      !PS_EggObject_GetDefinition(&stage->candidate.definition, action.object, &definition))
  {
    stage->valid = 0U;
    return PS_SCENE_OBJECTS_TRANSACTION;
  }
  object = &stage->candidate.objects[action.object];
  switch (action.opcode)
  {
    case 1U:
    case 2U:
      if (action.axis_mask & 1U)
      {
        int64_t x = (action.opcode == 1U) ? action.x : (int64_t)object->x + action.x;
        object->x = Clamp(x, PS_SCENE_RENDER_CANVAS_WIDTH - definition.width);
      }
      if (action.axis_mask & 2U)
      {
        int64_t y = (action.opcode == 1U) ? action.y : (int64_t)object->y - action.y;
        object->y = Clamp(y, PS_SCENE_RENDER_CANVAS_HEIGHT - definition.height);
      }
      break;
    case 3U:
      object->visible = action.visible;
      break;
    case 4U:
      object->static_frame = action.frame;
      break;
    case 5U:
      object->static_frame = PS_EGG_OBJECT_REF_NONE;
      break;
    default:
      stage->valid = 0U;
      return PS_SCENE_OBJECTS_TRANSACTION;
  }
  stage->action_count++;
  return PS_SCENE_OBJECTS_OK;
}

ps_scene_objects_status_t PS_SceneObjects_SelectState(ps_scene_objects_stage_t *stage,
  uint16_t state)
{
  if ((stage == NULL) || !stage->valid)
  {
    return PS_SCENE_OBJECTS_TRANSACTION;
  }
  if (state >= stage->candidate.definition.state_count)
  {
    stage->valid = 0U;
    return PS_SCENE_OBJECTS_TRANSACTION;
  }
  stage->candidate.state = state;
  return PS_SCENE_OBJECTS_OK;
}

ps_scene_objects_status_t PS_SceneObjects_Commit(ps_scene_objects_t *bank,
  ps_scene_objects_stage_t *stage)
{
  if ((stage == NULL) || !stage->valid)
  {
    return PS_SCENE_OBJECTS_TRANSACTION;
  }
  stage->valid = 0U;
  if (!Active(bank) || (stage->origin != bank) ||
      (bank->serial != stage->origin_serial) ||
      (bank->activation != stage->candidate.activation) ||
      (bank->serial == UINT64_MAX) || (bank->content_revision == UINT32_MAX))
  {
    return PS_SCENE_OBJECTS_TRANSACTION;
  }
  stage->candidate.serial++;
  stage->candidate.content_revision++;
  *bank = stage->candidate;
  return PS_SCENE_OBJECTS_OK;
}

ps_scene_objects_status_t PS_SceneObjects_Advance(ps_scene_objects_t *bank,
  uint64_t elapsed_ms)
{
  if (!Active(bank))
  {
    return PS_SCENE_OBJECTS_ARGUMENT;
  }
  if (bank->suspended || (elapsed_ms == 0U))
  {
    return PS_SCENE_OBJECTS_OK;
  }
  if ((elapsed_ms > UINT64_MAX - bank->elapsed_ms) || (bank->serial == UINT64_MAX))
  {
    return PS_SCENE_OBJECTS_TIME;
  }
  bank->elapsed_ms += elapsed_ms;
  bank->serial++;
  return PS_SCENE_OBJECTS_OK;
}

ps_scene_objects_status_t PS_SceneObjects_Suspend(ps_scene_objects_t *bank,
  uint32_t suspended)
{
  if (!Active(bank) || (suspended > 1U))
  {
    return PS_SCENE_OBJECTS_ARGUMENT;
  }
  if (bank->suspended == suspended)
  {
    return PS_SCENE_OBJECTS_OK;
  }
  if (bank->serial == UINT64_MAX)
  {
    return PS_SCENE_OBJECTS_TIME;
  }
  bank->suspended = (uint16_t)suspended;
  bank->serial++;
  return PS_SCENE_OBJECTS_OK;
}

ps_scene_objects_status_t PS_SceneObjects_Snapshot(const ps_scene_objects_t *bank,
  ps_scene_objects_snapshot_t *snapshot)
{
  ps_scene_objects_snapshot_t candidate = {0};
  uint16_t index;
  if (!Active(bank) || (snapshot == NULL))
  {
    return PS_SCENE_OBJECTS_ARGUMENT;
  }
  candidate.activation = bank->activation;
  candidate.content_revision = bank->content_revision;
  candidate.elapsed_ms = bank->elapsed_ms;
  candidate.state = bank->state;
  candidate.count = bank->definition.object_count;
  for (index = 0U; index < candidate.count; ++index)
  {
    const ps_scene_object_live_t *live = &bank->objects[index];
    ps_scene_object_snapshot_t *item = &candidate.objects[index];
    uint16_t steps = PS_EggObject_ClipStepCount(&bank->definition, index);
    uint16_t step;
    (void)PS_EggObject_GetDefinition(&bank->definition, index, &item->effective);
    item->effective.x = live->x;
    item->effective.y = live->y;
    item->effective.flags = (uint8_t)((item->effective.flags & ~1U) | live->visible);
    if (live->cycle_ms != 0U)
    {
      uint32_t phase_ms = (uint32_t)(bank->elapsed_ms % live->cycle_ms);
      for (step = 0U; step < steps; ++step)
      {
        uint32_t duration;
        uint16_t frame;
        (void)PS_EggObject_GetClipStep(&bank->definition, index, step, &frame, &duration);
        if (phase_ms < duration)
        {
          item->effective.frame = frame;
          item->step = step;
          item->remaining_ms = duration - phase_ms;
          item->animation_visible = 1U;
          break;
        }
        phase_ms -= duration;
      }
    }
    if (live->static_frame != PS_EGG_OBJECT_REF_NONE)
    {
      item->effective.frame = live->static_frame;
      item->animation_visible = 0U;
    }
  }
  for (index = 0U; index < candidate.count; ++index)
  {
    ps_egg_object_override_t override;
    ps_scene_object_snapshot_t *item;
    if (!PS_EggObject_GetOverride(&bank->definition, bank->state, index, &override))
    {
      break;
    }
    item = &candidate.objects[override.object];
    if (override.mask & 1U) item->effective.x = (int16_t)override.x;
    if (override.mask & 2U) item->effective.y = (int16_t)override.y;
    if (override.mask & 4U)
    {
      item->effective.frame = override.frame;
      item->animation_visible = 0U;
    }
    if (override.mask & 8U)
    {
      item->effective.flags = (uint8_t)((item->effective.flags & ~1U) | override.visible);
    }
  }
  for (index = 0U; index < candidate.count; ++index)
  {
    ps_scene_object_snapshot_t *item = &candidate.objects[index];
    item->animation_visible = item->animation_visible && (item->effective.flags & 1U);
  }
  *snapshot = candidate;
  return PS_SCENE_OBJECTS_OK;
}
