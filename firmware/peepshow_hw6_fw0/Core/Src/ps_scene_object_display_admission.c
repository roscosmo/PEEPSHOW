#include "ps_scene_object_display_admission.h"
#include "display_renderer.h"
#include <stddef.h>
#include <string.h>

typedef struct
{
  const ps_object_waiting_program_t *program;
  const ps_egg_sprite_catalog_t *catalog;
  ps_object_display_workspace_t *workspace;
  ps_object_display_result_t *result;
  ps_display_work_profile_t *profile;
} ps_object_display_context_t;

static uint32_t PS_ObjectDisplay_Compose(void *opaque, uint32_t step,
  uint8_t *destination, uint32_t capacity)
{
  ps_object_display_context_t *context = opaque;
  uint32_t started = PS_DisplayWork_Begin(context->profile);
  context->result->failed_step = step;
  context->result->raster_status = UINT32_MAX;
  context->result->projection_status = PS_ObjectWaiting_Project(context->program,
    step, &context->workspace->snapshot, &context->workspace->model);
  PS_DisplayWork_End(context->profile, PS_DISPLAY_WORK_PROJECT, started);
  if (context->result->projection_status != PS_OBJECT_WAITING_OK) { return 0UL; }
  started = PS_DisplayWork_Begin(context->profile);
  context->result->raster_status = DisplayRenderer_CopyCandidateSceneFrameCached(
    &context->workspace->model, context->catalog, &context->workspace->raster_cache,
    destination, capacity) ? 0UL : 1UL;
  PS_DisplayWork_End(context->profile, PS_DISPLAY_WORK_RASTER, started);
  return (context->result->raster_status == 0UL) ? 1UL : 0UL;
}

uint32_t PS_ObjectDisplay_CheckWaitingCached(const ps_object_waiting_program_t *program,
  const ps_egg_sprite_catalog_t *catalog, ps_object_display_workspace_t *workspace,
  ps_object_display_result_t *result, ps_display_work_profile_t *profile, uint32_t reuse)
{
  ps_object_display_context_t context;
  HAL_StatusTypeDef status;
  if (workspace != NULL)
  {
    if (reuse == 0UL) { workspace->raster_cache.valid = 0UL; }
    workspace->raster_cache.full_frames = 0UL;
    workspace->raster_cache.reused_frames = 0UL;
    workspace->raster_cache.elements_drawn = 0UL;
  }
  if (result == NULL)
  { if (workspace != NULL) { workspace->raster_cache.valid = 0UL; } return 1UL; }
  (void)memset(result, 0, sizeof(*result));
  result->projection_status = UINT32_MAX;
  result->raster_status = UINT32_MAX;
  result->failed_step = UINT32_MAX;
  result->payload.status = HAL_ERROR;
  result->payload.reason = PS_LPBAM_ADMISSION_REASON_ARGUMENT;
  if ((program == NULL) || (catalog == NULL) || (workspace == NULL))
  { if (workspace != NULL) { workspace->raster_cache.valid = 0UL; } return 1UL; }
  context = (ps_object_display_context_t){program, catalog, workspace, result, profile};
  status = PS_LpbamDisplay_CheckFullSceneAnimationProfiled(program->step_count,
    PS_ObjectDisplay_Compose, &context, &workspace->packing, &result->payload, profile);
  result->frames_composed = workspace->packing.frames_composed;
  if (status == HAL_OK) { result->failed_step = UINT32_MAX; }
  else { workspace->raster_cache.valid = 0UL; }
  return (status == HAL_OK) ? 0UL : 1UL;
}

uint32_t PS_ObjectDisplay_CheckWaitingProfiled(const ps_object_waiting_program_t *program,
  const ps_egg_sprite_catalog_t *catalog, ps_object_display_workspace_t *workspace,
  ps_object_display_result_t *result, ps_display_work_profile_t *profile)
{
  return PS_ObjectDisplay_CheckWaitingCached(program, catalog, workspace, result, profile, 0UL);
}

uint32_t PS_ObjectDisplay_CheckWaiting(const ps_object_waiting_program_t *program,
  const ps_egg_sprite_catalog_t *catalog, ps_object_display_workspace_t *workspace,
  ps_object_display_result_t *result)
{
  return PS_ObjectDisplay_CheckWaitingProfiled(program, catalog, workspace, result, NULL);
}
