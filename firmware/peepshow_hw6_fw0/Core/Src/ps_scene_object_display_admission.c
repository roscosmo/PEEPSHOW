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
} ps_object_display_context_t;

static uint32_t PS_ObjectDisplay_Compose(void *opaque, uint32_t step,
  uint8_t *destination, uint32_t capacity)
{
  ps_object_display_context_t *context = opaque;
  context->result->failed_step = step;
  context->result->raster_status = UINT32_MAX;
  context->result->projection_status = PS_ObjectWaiting_Project(context->program,
    step, &context->workspace->snapshot, &context->workspace->model);
  if (context->result->projection_status != PS_OBJECT_WAITING_OK) { return 0UL; }
  context->result->raster_status = DisplayRenderer_CopyCandidateSceneFrame(
    &context->workspace->model, context->catalog, destination, capacity) ? 0UL : 1UL;
  return (context->result->raster_status == 0UL) ? 1UL : 0UL;
}

uint32_t PS_ObjectDisplay_CheckWaiting(const ps_object_waiting_program_t *program,
  const ps_egg_sprite_catalog_t *catalog, ps_object_display_workspace_t *workspace,
  ps_object_display_result_t *result)
{
  ps_object_display_context_t context;
  HAL_StatusTypeDef status;
  if (result == NULL) { return 1UL; }
  (void)memset(result, 0, sizeof(*result));
  result->projection_status = UINT32_MAX;
  result->raster_status = UINT32_MAX;
  result->failed_step = UINT32_MAX;
  result->payload.status = HAL_ERROR;
  result->payload.reason = PS_LPBAM_ADMISSION_REASON_ARGUMENT;
  if ((program == NULL) || (catalog == NULL) || (workspace == NULL)) { return 1UL; }
  context = (ps_object_display_context_t){program, catalog, workspace, result};
  status = PS_LpbamDisplay_CheckFullSceneAnimation(program->step_count,
    PS_ObjectDisplay_Compose, &context, &workspace->packing, &result->payload);
  result->frames_composed = workspace->packing.frames_composed;
  if (status == HAL_OK) { result->failed_step = UINT32_MAX; }
  return (status == HAL_OK) ? 0UL : 1UL;
}
