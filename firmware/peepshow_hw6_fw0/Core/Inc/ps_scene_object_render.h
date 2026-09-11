#ifndef PS_SCENE_OBJECT_RENDER_H
#define PS_SCENE_OBJECT_RENDER_H

#include "ps_scene_objects.h"
#include "ps_scene_render_model.h"

/* Pure projection of an immutable object snapshot. Zero deadline means no
 * visible animation needs a redraw; hidden/masked clocks still advance. */
uint32_t PS_SceneObjectRender_Project(const ps_scene_objects_snapshot_t *snapshot,
  uint32_t scene_id, ps_scene_render_model_t *model, uint32_t *next_ms);

#endif
