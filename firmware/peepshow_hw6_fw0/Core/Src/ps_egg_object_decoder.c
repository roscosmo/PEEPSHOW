#include "ps_egg_object_decoder.h"

#include <stddef.h>
#include <string.h>

#include "ps_scene_render_model.h"

enum
{
  OBJECT_HEADER = 16, OBJECT_RECORD = 20, CONTROL_HEADER = 20,
  RANGE_RECORD = 4, CONTROL_RECORD = 16, STRING_HEADER = 12,
  ASSET_HEADER = 20, ASSET_RECORD = 36, CLIP_HEADER = 16, CLIP_RECORD = 16
};

static uint16_t U16(const uint8_t *p)
{
  return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static uint32_t U32(const uint8_t *p)
{
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
    ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static int32_t I32(const uint8_t *p)
{
  uint32_t value = U32(p);
  return (value <= INT32_MAX) ? (int32_t)value :
    -1 - (int32_t)(UINT32_MAX - value);
}

static int16_t I16(const uint8_t *p)
{
  uint16_t value = U16(p);
  return (value <= INT16_MAX) ? (int16_t)value :
    (int16_t)(-1 - (int32_t)(UINT16_MAX - value));
}

static uint32_t Header(ps_egg_object_bytes_t bytes, const char *magic,
  uint16_t size)
{
  return (bytes.data != NULL) && (bytes.size >= size) &&
    (memcmp(bytes.data, magic, 4U) == 0) && (U16(bytes.data + 4U) == 1U) &&
    (U16(bytes.data + 6U) == size);
}

static uint32_t StableId(ps_egg_object_bytes_t strings, uint16_t id,
  ps_egg_object_bytes_t *text)
{
  uint32_t count;
  uint32_t base;
  uint32_t first;
  uint32_t end;
  uint32_t index;
  if ((strings.data == NULL) || (strings.size < STRING_HEADER) ||
      (memcmp(strings.data, "STR1", 4U) != 0) ||
      (U16(strings.data + 4U) != 1U))
  {
    return 0U;
  }
  count = U16(strings.data + 6U);
  base = STRING_HEADER + (count + 1U) * 4U;
  if ((id >= count) || (base > strings.size) ||
      (U32(strings.data + 8U) != strings.size - base))
  {
    return 0U;
  }
  first = U32(strings.data + STRING_HEADER + (uint32_t)id * 4U);
  end = U32(strings.data + STRING_HEADER + ((uint32_t)id + 1U) * 4U);
  if ((end <= first) || (end - first > 64U) || (end > strings.size - base))
  {
    return 0U;
  }
  text->data = strings.data + base + first;
  text->size = end - first;
  for (index = 0U; index < text->size; ++index)
  {
    uint8_t c = text->data[index];
    if (((c < 'a') || (c > 'z')) &&
        ((index == 0U) || (((c < '0') || (c > '9')) &&
         (c != '_') && (c != '.') && (c != '-'))))
    {
      return 0U;
    }
  }
  return 1U;
}

static uint32_t Frame(ps_egg_object_bytes_t assets, uint16_t frame,
  const ps_egg_object_definition_t *object)
{
  const uint8_t *record;
  uint32_t count;
  if ((object->kind != 1U) || !Header(assets, "AST1", ASSET_HEADER))
  {
    return 0U;
  }
  count = U16(assets.data + 8U);
  if ((frame >= count) || (assets.size != ASSET_HEADER + count * ASSET_RECORD))
  {
    return 0U;
  }
  record = assets.data + ASSET_HEADER + (uint32_t)frame * ASSET_RECORD;
  return (U16(record + 4U) == object->width) &&
    (U16(record + 6U) == object->height);
}

static uint32_t Clip(ps_egg_object_bytes_t animations,
  ps_egg_object_bytes_t assets, const ps_egg_object_definition_t *object)
{
  const uint8_t *record;
  uint32_t count;
  uint32_t refs;
  uint32_t frames;
  uint32_t durations;
  uint32_t first;
  uint32_t length;
  uint32_t first_duration;
  uint32_t index;
  if (!Header(animations, "ANI1", CLIP_HEADER))
  {
    return 0U;
  }
  count = U16(animations.data + 8U);
  refs = U16(animations.data + 10U);
  frames = CLIP_HEADER + count * CLIP_RECORD;
  durations = frames + refs * 2U;
  if ((object->clip >= count) || (U16(animations.data + 12U) != refs) ||
      (U16(animations.data + 14U) != 0U) ||
      (animations.size != durations + refs * 4U))
  {
    return 0U;
  }
  record = animations.data + CLIP_HEADER + (uint32_t)object->clip * CLIP_RECORD;
  first = U16(record + 2U);
  length = U16(record + 4U);
  first_duration = U16(record + 6U);
  if ((length == 0U) || (first + length > refs) ||
      (first_duration + length > refs) || (U16(record + 8U) != 1U) ||
      (U16(record + 10U) != object->width) ||
      (U16(record + 12U) != object->height) || (U16(record + 14U) != 0U))
  {
    return 0U;
  }
  for (index = 0U; index < length; ++index)
  {
    uint32_t duration = U32(animations.data + durations + (first_duration + index) * 4U);
    if ((duration == 0U) || (duration > 60000U) ||
        !Frame(assets, U16(animations.data + frames + (first + index) * 2U), object))
    {
      return 0U;
    }
  }
  return 1U;
}

static uint32_t Position(const ps_egg_object_definition_t *object, int32_t x, int32_t y)
{
  return (x >= 0) && (y >= 0) &&
    (x <= (int32_t)PS_SCENE_RENDER_CANVAS_WIDTH - object->width) &&
    (y <= (int32_t)PS_SCENE_RENDER_CANVAS_HEIGHT - object->height);
}

uint32_t PS_EggObject_GetDefinition(const ps_egg_object_view_t *view,
  uint16_t index, ps_egg_object_definition_t *value)
{
  const uint8_t *p;
  if ((view == NULL) || (value == NULL) || (index >= view->object_count))
  {
    return 0U;
  }
  p = view->objects.data + OBJECT_HEADER + (uint32_t)index * OBJECT_RECORD;
  value->id = U16(p);
  value->kind = p[2];
  value->layer = p[3];
  value->z_order = p[4];
  value->flags = p[5];
  value->width = U16(p + 6U);
  value->height = U16(p + 8U);
  value->x = I16(p + 10U);
  value->y = I16(p + 12U);
  value->frame = U16(p + 14U);
  value->clip = U16(p + 16U);
  return 1U;
}

uint32_t PS_EggObject_GetOverride(const ps_egg_object_view_t *view,
  uint16_t state, uint16_t index, ps_egg_object_override_t *value)
{
  const uint8_t *p;
  uint32_t first;
  if ((view == NULL) || (value == NULL) || (state >= view->state_count))
  {
    return 0U;
  }
  p = view->controls.data + CONTROL_HEADER + (uint32_t)state * RANGE_RECORD;
  first = U16(p);
  if (index >= U16(p + 2U))
  {
    return 0U;
  }
  p = view->controls.data + view->override_offset + (first + index) * CONTROL_RECORD;
  value->object = U16(p);
  value->mask = U16(p + 2U);
  value->x = I32(p + 4U);
  value->y = I32(p + 8U);
  value->frame = U16(p + 12U);
  value->visible = U16(p + 14U);
  return 1U;
}

uint32_t PS_EggObject_GetOperation(const ps_egg_object_view_t *view,
  uint16_t index, ps_egg_object_operation_t *value)
{
  const uint8_t *p;
  if ((view == NULL) || (value == NULL) || (index >= view->operation_count))
  {
    return 0U;
  }
  p = view->controls.data + view->operation_offset + (uint32_t)index * CONTROL_RECORD;
  value->opcode = p[0];
  value->axis_mask = p[1];
  value->object = U16(p + 2U);
  value->x = I32(p + 4U);
  value->y = I32(p + 8U);
  value->frame = U16(p + 12U);
  value->visible = U16(p + 14U);
  return 1U;
}

static ps_egg_object_status_t Objects(const ps_egg_object_view_t *view,
  ps_egg_object_bytes_t strings, ps_egg_object_bytes_t assets,
  ps_egg_object_bytes_t animations)
{
  uint16_t index;
  for (index = 0U; index < view->object_count; ++index)
  {
    ps_egg_object_definition_t object;
    ps_egg_object_bytes_t id;
    uint16_t previous;
    (void)PS_EggObject_GetDefinition(view, index, &object);
    if (!StableId(strings, object.id, &id))
    {
      return PS_EGG_OBJECT_ID;
    }
    for (previous = 0U; previous < index; ++previous)
    {
      ps_egg_object_bytes_t other;
      uint16_t other_id = U16(view->objects.data + OBJECT_HEADER + (uint32_t)previous * OBJECT_RECORD);
      if (!StableId(strings, other_id, &other) ||
          ((id.size == other.size) && (memcmp(id.data, other.data, id.size) == 0)))
      {
        return PS_EGG_OBJECT_ID;
      }
    }
    if ((object.kind < 1U) || (object.kind > 8U) || (object.layer > 2U) ||
        ((object.flags & ~7U) != 0U) ||
        (((object.flags & 2U) != 0U) && (object.kind != 2U)) ||
        (U16(view->objects.data + OBJECT_HEADER + (uint32_t)index * OBJECT_RECORD + 18U) != 0U) ||
        (object.width == 0U) || (object.width > PS_SCENE_RENDER_CANVAS_WIDTH) ||
        (object.height == 0U) || (object.height > PS_SCENE_RENDER_CANVAS_HEIGHT) ||
        !Position(&object, object.x, object.y))
    {
      return PS_EGG_OBJECT_GEOMETRY;
    }
    if ((object.kind >= 5U) && ((object.width < 3U) || (object.height < 3U) ||
        ((object.width & 1U) == 0U) || ((object.height & 1U) == 0U) ||
        (((object.kind == 5U) || (object.kind == 7U)) && (object.width != object.height))))
    {
      return PS_EGG_OBJECT_GEOMETRY;
    }
    if (object.kind == 1U)
    {
      if (!Frame(assets, object.frame, &object) ||
          ((object.clip != PS_EGG_OBJECT_REF_NONE) && !Clip(animations, assets, &object)))
      {
        return PS_EGG_OBJECT_ASSET;
      }
    }
    else if ((object.frame != PS_EGG_OBJECT_REF_NONE) || (object.clip != PS_EGG_OBJECT_REF_NONE))
    {
      return PS_EGG_OBJECT_ASSET;
    }
  }
  return PS_EGG_OBJECT_OK;
}

static ps_egg_object_status_t Controls(const ps_egg_object_view_t *view,
  ps_egg_object_bytes_t assets)
{
  uint16_t state;
  uint16_t index;
  uint32_t cursor = 0U;
  for (state = 0U; state < view->state_count; ++state)
  {
    const uint8_t *p = view->controls.data + CONTROL_HEADER + (uint32_t)state * RANGE_RECORD;
    uint32_t count = U16(p + 2U);
    uint32_t seen = 0U;
    if ((U16(p) != cursor) || (count > view->object_count) ||
        (cursor + count > view->override_count))
    {
      return PS_EGG_OBJECT_OVERRIDE;
    }
    for (index = 0U; index < count; ++index)
    {
      ps_egg_object_override_t item;
      ps_egg_object_definition_t object;
      (void)PS_EggObject_GetOverride(view, state, index, &item);
      if ((item.object >= view->object_count) ||
          ((seen & (UINT32_C(1) << item.object)) != 0U) ||
          (item.mask == 0U) || (item.mask > 15U) ||
          (((item.mask & 1U) == 0U) && (item.x != 0)) ||
          (((item.mask & 2U) == 0U) && (item.y != 0)) ||
          (((item.mask & 4U) == 0U) && (item.frame != PS_EGG_OBJECT_REF_NONE)) ||
          (((item.mask & 8U) == 0U) && (item.visible != 0U)) || (item.visible > 1U))
      {
        return PS_EGG_OBJECT_OVERRIDE;
      }
      seen |= UINT32_C(1) << item.object;
      (void)PS_EggObject_GetDefinition(view, item.object, &object);
      if (!Position(&object, (item.mask & 1U) ? item.x : object.x,
                            (item.mask & 2U) ? item.y : object.y) ||
          (((item.mask & 4U) != 0U) && !Frame(assets, item.frame, &object)))
      {
        return PS_EGG_OBJECT_OVERRIDE;
      }
    }
    cursor += count;
  }
  if (cursor != view->override_count)
  {
    return PS_EGG_OBJECT_OVERRIDE;
  }
  for (index = 0U; index < view->operation_count; ++index)
  {
    ps_egg_object_operation_t item;
    ps_egg_object_definition_t object;
    (void)PS_EggObject_GetOperation(view, index, &item);
    if ((item.opcode < 1U) || (item.opcode > 5U) || (item.object >= view->object_count) ||
        ((item.opcode <= 2U) ? ((item.axis_mask == 0U) || (item.axis_mask > 3U)) : (item.axis_mask != 0U)) ||
        (((item.axis_mask & 1U) == 0U) && (item.x != 0)) ||
        (((item.axis_mask & 2U) == 0U) && (item.y != 0)) ||
        (item.visible > 1U) || ((item.opcode != 3U) && (item.visible != 0U)))
    {
      return PS_EGG_OBJECT_OPERATION;
    }
    (void)PS_EggObject_GetDefinition(view, item.object, &object);
    if ((item.opcode == 4U) ? !Frame(assets, item.frame, &object) :
                            (item.frame != PS_EGG_OBJECT_REF_NONE))
    {
      return PS_EGG_OBJECT_OPERATION;
    }
    if ((item.opcode == 5U) && (object.kind != 1U))
    {
      return PS_EGG_OBJECT_OPERATION;
    }
  }
  return PS_EGG_OBJECT_OK;
}

ps_egg_object_status_t PS_EggObject_Decode(
  ps_egg_object_bytes_t objects, ps_egg_object_bytes_t controls,
  ps_egg_object_bytes_t strings, ps_egg_object_bytes_t assets,
  ps_egg_object_bytes_t animations, ps_egg_object_view_t *view)
{
  ps_egg_object_view_t candidate = {0};
  ps_egg_object_status_t status;
  if (view == NULL)
  {
    return PS_EGG_OBJECT_ARGUMENT;
  }
  memset(view, 0, sizeof(*view));
  if (!Header(objects, "OBJ2", OBJECT_HEADER) ||
      !Header(controls, "OCT2", CONTROL_HEADER) ||
      (U16(objects.data + 10U) != OBJECT_RECORD) ||
      (U32(objects.data + 12U) != 0U) ||
      (U16(controls.data + 14U) != RANGE_RECORD) ||
      (U16(controls.data + 16U) != CONTROL_RECORD) ||
      (U16(controls.data + 18U) != CONTROL_RECORD))
  {
    return PS_EGG_OBJECT_HEADER;
  }
  candidate.object_count = U16(objects.data + 8U);
  candidate.state_count = U16(controls.data + 8U);
  candidate.override_count = U16(controls.data + 10U);
  candidate.operation_count = U16(controls.data + 12U);
  if ((candidate.object_count == 0U) || (candidate.object_count > PS_EGG_OBJECT_WIRE_MAX) ||
      (candidate.state_count == 0U) || (candidate.state_count > PS_EGG_OBJECT_STATE_WIRE_MAX) ||
      (candidate.override_count > PS_EGG_OBJECT_OVERRIDE_WIRE_MAX) ||
      (candidate.operation_count > PS_EGG_OBJECT_OPERATION_WIRE_MAX))
  {
    return PS_EGG_OBJECT_CAPACITY;
  }
  candidate.override_offset = CONTROL_HEADER + (uint32_t)candidate.state_count * RANGE_RECORD;
  candidate.operation_offset = candidate.override_offset + (uint32_t)candidate.override_count * CONTROL_RECORD;
  if ((objects.size != OBJECT_HEADER + (uint32_t)candidate.object_count * OBJECT_RECORD) ||
      (controls.size != candidate.operation_offset + (uint32_t)candidate.operation_count * CONTROL_RECORD))
  {
    return PS_EGG_OBJECT_HEADER;
  }
  candidate.objects = objects;
  candidate.controls = controls;
  status = Objects(&candidate, strings, assets, animations);
  if (status == PS_EGG_OBJECT_OK)
  {
    status = Controls(&candidate, assets);
  }
  if (status == PS_EGG_OBJECT_OK)
  {
    *view = candidate;
  }
  return status;
}
