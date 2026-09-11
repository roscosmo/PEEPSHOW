#include "ps_scene_object_waiting.h"
#include <stddef.h>
#include <string.h>

static uint32_t PS_ObjectWaiting_Gcd(uint32_t a, uint32_t b)
{
  while (b != 0UL)
  {
    uint32_t remainder = a % b;
    a = b;
    b = remainder;
  }
  return a;
}

static uint32_t PS_ObjectWaiting_Animated(const ps_scene_object_snapshot_t *item)
{
  return ((item->effective.flags & 1U) != 0U) && (item->animation_visible != 0UL);
}

ps_object_waiting_status_t PS_ObjectWaiting_Build(const ps_scene_objects_t *bank,
  uint32_t scene_id, ps_object_waiting_program_t *program,
  ps_object_waiting_workspace_t *workspace)
{
  uint32_t index, quantum = 0UL, cycle = 0UL, count = 1UL, step;
  if (program == NULL) { return PS_OBJECT_WAITING_ARGUMENT; }
  (void)memset(program, 0, sizeof(*program));
  if ((bank == NULL) || (workspace == NULL) || (bank == &workspace->bank) ||
      (bank->suspended != 0U) || (scene_id == 0UL) ||
      (PS_SceneObjects_Snapshot(bank, &program->base) != PS_SCENE_OBJECTS_OK))
  { return PS_OBJECT_WAITING_ARGUMENT; }
  for (index = 0UL; index < program->base.count; ++index)
  {
    if (PS_ObjectWaiting_Animated(&program->base.objects[index]) != 0UL)
    {
      uint16_t clip_step;
      uint16_t steps = PS_EggObject_ClipStepCount(&bank->definition, (uint16_t)index);
      if ((steps == 0U) || (bank->objects[index].cycle_ms == 0UL))
      { return PS_OBJECT_WAITING_ARGUMENT; }
      for (clip_step = 0U; clip_step < steps; ++clip_step)
      {
        uint16_t frame;
        uint32_t duration;
        if ((PS_EggObject_GetClipStep(&bank->definition, (uint16_t)index,
              clip_step, &frame, &duration) == 0UL) || (duration == 0UL))
        { return PS_OBJECT_WAITING_ARGUMENT; }
        quantum = PS_ObjectWaiting_Gcd(quantum, duration);
      }
    }
  }
  for (index = 0UL; index < program->base.count; ++index)
  {
    if (PS_ObjectWaiting_Animated(&program->base.objects[index]) != 0UL)
    {
      uint32_t object_cycle = bank->objects[index].cycle_ms;
      uint64_t combined = (cycle == 0UL) ? object_cycle :
        (uint64_t)(cycle / PS_ObjectWaiting_Gcd(cycle, object_cycle)) * object_cycle;
      if ((combined > UINT32_MAX) ||
          (combined > (uint64_t)quantum * PS_SCENE_WAITING_VISUAL_SEQUENCE_MAX))
      { return PS_OBJECT_WAITING_CAPACITY; }
      cycle = (uint32_t)combined;
    }
  }
  program->scene_id = scene_id;
  program->quantum_ms = quantum;
  if (quantum != 0UL)
  {
    count = cycle / quantum;
    program->initial_remaining_ms = quantum - (uint32_t)(bank->elapsed_ms % quantum);
  }
  workspace->bank = *bank;
  workspace->snapshot = program->base;
  for (step = 0UL; step < count; ++step)
  {
    if (step != 0UL)
    {
      uint32_t delta = (step == 1UL) ? program->initial_remaining_ms : quantum;
      if (PS_SceneObjects_Advance(&workspace->bank, delta) != PS_SCENE_OBJECTS_OK)
      { return PS_OBJECT_WAITING_TIME; }
      if (PS_SceneObjects_Snapshot(&workspace->bank, &workspace->snapshot) != PS_SCENE_OBJECTS_OK)
      { return PS_OBJECT_WAITING_ARGUMENT; }
    }
    for (index = 0UL; index < program->base.count; ++index)
    { program->frames[step][index] = workspace->snapshot.objects[index].effective.frame; }
  }
  program->step_count = count;
  return PS_OBJECT_WAITING_OK;
}

ps_object_waiting_status_t PS_ObjectWaiting_Resolve(
  const ps_object_waiting_program_t *program, uint64_t elapsed_ms,
  uint32_t *step, uint32_t *remaining_ms)
{
  uint64_t delta, residual, quanta;
  if ((program == NULL) || (step == NULL) || (remaining_ms == NULL) ||
      (program->step_count == 0UL) ||
      (program->step_count > PS_SCENE_WAITING_VISUAL_SEQUENCE_MAX))
  { return PS_OBJECT_WAITING_ARGUMENT; }
  if (elapsed_ms < program->base.elapsed_ms) { return PS_OBJECT_WAITING_TIME; }
  if (program->quantum_ms == 0UL)
  {
    if (program->step_count != 1UL) { return PS_OBJECT_WAITING_ARGUMENT; }
    *step = 0UL;
    *remaining_ms = 0UL;
    return PS_OBJECT_WAITING_OK;
  }
  delta = elapsed_ms - program->base.elapsed_ms;
  residual = program->base.elapsed_ms % program->quantum_ms + delta % program->quantum_ms;
  quanta = delta / program->quantum_ms + residual / program->quantum_ms;
  *step = (uint32_t)(quanta % program->step_count);
  *remaining_ms = program->quantum_ms - (uint32_t)(residual % program->quantum_ms);
  return PS_OBJECT_WAITING_OK;
}

ps_object_waiting_status_t PS_ObjectWaiting_Project(
  const ps_object_waiting_program_t *program, uint32_t step,
  ps_scene_objects_snapshot_t *scratch, ps_scene_render_model_t *model)
{
  uint32_t index, unused_next;
  if ((program == NULL) || (scratch == NULL) || (scratch == &program->base) ||
      (model == NULL) || (program->step_count == 0UL) ||
      (program->step_count > PS_SCENE_WAITING_VISUAL_SEQUENCE_MAX) ||
      (step >= program->step_count) ||
      (program->base.count > PS_SCENE_RENDER_MODEL_ELEMENT_MAX))
  { return PS_OBJECT_WAITING_ARGUMENT; }
  *scratch = program->base;
  for (index = 0UL; index < scratch->count; ++index)
  { scratch->objects[index].effective.frame = program->frames[step][index]; }
  return (PS_SceneObjectRender_Project(scratch, program->scene_id, model, &unused_next) == 0UL) ?
    PS_OBJECT_WAITING_OK : PS_OBJECT_WAITING_ARGUMENT;
}
