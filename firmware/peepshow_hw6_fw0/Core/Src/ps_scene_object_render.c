#include "ps_scene_object_render.h"
#include "ps_egg_state_loader.h"
#include <stddef.h>
#include <string.h>

uint32_t PS_SceneObjectRender_Project(const ps_scene_objects_snapshot_t *snapshot,
  uint32_t scene_id, ps_scene_render_model_t *model, uint32_t *next_ms)
{
  static const uint32_t types[] = {
    PS_SCENE_RENDER_ELEMENT_NONE, PS_SCENE_RENDER_ELEMENT_SPRITE_1BPP,
    PS_SCENE_RENDER_ELEMENT_LINE, PS_SCENE_RENDER_ELEMENT_OUTLINE_RECT,
    PS_SCENE_RENDER_ELEMENT_FILLED_RECT, PS_SCENE_RENDER_ELEMENT_CIRCLE,
    PS_SCENE_RENDER_ELEMENT_ELLIPSE, PS_SCENE_RENDER_ELEMENT_FILLED_CIRCLE,
    PS_SCENE_RENDER_ELEMENT_FILLED_ELLIPSE
  };
  uint32_t index;
  if ((snapshot == NULL) || (model == NULL) || (next_ms == NULL) ||
      (snapshot->count == 0U) || (snapshot->count > PS_SCENE_RENDER_MODEL_ELEMENT_MAX) ||
      (snapshot->activation == 0UL)) { return 1UL; }
  for (index = 0UL; index < snapshot->count; ++index)
  {
    const ps_egg_object_definition_t *object = &snapshot->objects[index].effective;
    if ((object->kind == 0U) || (object->kind >= sizeof(types) / sizeof(types[0])) ||
        (object->x < 0) || (object->y < 0) ||
        ((uint32_t)object->x + object->width > PS_SCENE_RENDER_CANVAS_WIDTH) ||
        ((uint32_t)object->y + object->height > PS_SCENE_RENDER_CANVAS_HEIGHT))
    { return 1UL; }
  }
  (void)memset(model, 0, sizeof(*model));
  *next_ms = 0UL;
  model->api_version = PS_SCENE_RENDER_MODEL_API_VERSION;
  model->scene_id = scene_id;
  model->state_id = (uint32_t)snapshot->state + 1UL;
  model->content_revision = snapshot->content_revision;
  model->timeline_revision = snapshot->activation;
  model->element_count = snapshot->count;
  for (index = 0UL; index < snapshot->count; ++index)
  {
    const ps_scene_object_snapshot_t *item = &snapshot->objects[index];
    const ps_egg_object_definition_t *object = &item->effective;
    ps_scene_render_element_t *element = &model->elements[index];
    element->element_id = (uint32_t)object->id + 1UL;
    element->type = ((object->kind == 2U) && ((object->flags & 2U) != 0U)) ?
      PS_SCENE_RENDER_ELEMENT_LINE_UP_RIGHT : types[object->kind];
    element->asset_id = (object->kind == 1U) ?
      PS_EGG_STATE_LOADER_SPRITE_FRAME_ID_BASE + 1UL + object->frame : 0UL;
    element->visible = object->flags & 1U;
    element->layer = object->layer;
    element->z_order = object->z_order;
    element->x = (uint16_t)object->x;
    element->y = (uint16_t)object->y;
    element->width = object->width;
    element->height = object->height;
    if ((element->visible != 0UL) && (item->animation_visible != 0UL) &&
        (item->remaining_ms != 0UL) &&
        ((*next_ms == 0UL) || (item->remaining_ms < *next_ms)))
    { *next_ms = item->remaining_ms; }
  }
  return 0UL;
}
