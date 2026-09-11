#ifndef PS_SCENE_OBJECT_DISPLAY_ADMISSION_H
#define PS_SCENE_OBJECT_DISPLAY_ADMISSION_H

#include "ps_scene_object_waiting.h"
#include "ps_egg_state_loader.h"
#include "ps_lpbam_display_buffers.h"

typedef struct
{
  ps_scene_objects_snapshot_t snapshot;
  ps_scene_render_model_t model;
  ps_lpbam_display_check_workspace_t packing;
} ps_object_display_workspace_t;

typedef struct
{
  uint32_t projection_status;
  uint32_t raster_status;
  uint32_t failed_step;
  uint32_t frames_composed;
  ps_lpbam_display_admission_t payload;
} ps_object_display_result_t;

/* thDisplay only. Checks one immutable full-scene program, not all reachable
 * object states. Caller owns the workspace and leases the candidate blob/view
 * until completion; no active display storage may alias workspace/result.
 * Returns 0 on exact raster/payload success, 1 on rejection. No presentation,
 * active-catalog publication, DMA readiness or installation is implied.
 */
uint32_t PS_ObjectDisplay_CheckWaiting(const ps_object_waiting_program_t *program,
  const ps_egg_sprite_catalog_t *catalog, ps_object_display_workspace_t *workspace,
  ps_object_display_result_t *result);

#endif
