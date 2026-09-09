#ifndef PS_EGG_OBJECT_DECODER_H
#define PS_EGG_OBJECT_DECODER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Wire ceilings, not target admission limits or allocated runtime banks. */
#define PS_EGG_OBJECT_WIRE_MAX (32U)
#define PS_EGG_OBJECT_STATE_WIRE_MAX (64U)
#define PS_EGG_OBJECT_OVERRIDE_WIRE_MAX (2048U)
#define PS_EGG_OBJECT_OPERATION_WIRE_MAX (1152U)
#define PS_EGG_OBJECT_REF_NONE (0xFFFFU)

typedef enum
{
  PS_EGG_OBJECT_OK = 0,
  PS_EGG_OBJECT_ARGUMENT,
  PS_EGG_OBJECT_HEADER,
  PS_EGG_OBJECT_CAPACITY,
  PS_EGG_OBJECT_ID,
  PS_EGG_OBJECT_GEOMETRY,
  PS_EGG_OBJECT_ASSET,
  PS_EGG_OBJECT_OVERRIDE,
  PS_EGG_OBJECT_OPERATION
} ps_egg_object_status_t;

typedef struct
{
  const uint8_t *data;
  uint32_t size;
} ps_egg_object_bytes_t;

typedef struct
{
  uint16_t id;
  uint8_t kind;
  uint8_t layer;
  uint8_t z_order;
  uint8_t flags;
  uint16_t width;
  uint16_t height;
  int16_t x;
  int16_t y;
  uint16_t frame;
  uint16_t clip;
} ps_egg_object_definition_t;

typedef struct
{
  uint16_t object;
  uint16_t mask;
  int32_t x;
  int32_t y;
  uint16_t frame;
  uint16_t visible;
} ps_egg_object_override_t;

typedef struct
{
  uint8_t opcode;
  uint8_t axis_mask;
  uint16_t object;
  int32_t x;
  int32_t y;
  uint16_t frame;
  uint16_t visible;
} ps_egg_object_operation_t;

typedef struct
{
  ps_egg_object_bytes_t objects;
  ps_egg_object_bytes_t controls;
  uint32_t override_offset;
  uint32_t operation_offset;
  uint16_t object_count;
  uint16_t state_count;
  uint16_t override_count;
  uint16_t operation_count;
} ps_egg_object_view_t;

/*
 * Pure bounded decoder; no globals, hardware, allocation or package publication.
 * Input spans must remain immutable for the lifetime of a successful view.
 * strings/assets/animations are STR1/AST1/ANI1 payloads. Shared catalog integrity,
 * pixel data, container/scene/graph links and target admission remain the parent
 * loader's responsibility. This function checks all object/control records and
 * their referenced IDs, frame dimensions and looping clips, not a whole egg.
 * Failure clears view. Use getters only with a view returned successfully here.
 */
ps_egg_object_status_t PS_EggObject_Decode(
  ps_egg_object_bytes_t objects, ps_egg_object_bytes_t controls,
  ps_egg_object_bytes_t strings, ps_egg_object_bytes_t assets,
  ps_egg_object_bytes_t animations, ps_egg_object_view_t *view);

uint32_t PS_EggObject_GetDefinition(const ps_egg_object_view_t *view,
  uint16_t index, ps_egg_object_definition_t *value);
uint32_t PS_EggObject_GetOverride(const ps_egg_object_view_t *view,
  uint16_t state, uint16_t index, ps_egg_object_override_t *value);
uint32_t PS_EggObject_GetOperation(const ps_egg_object_view_t *view,
  uint16_t index, ps_egg_object_operation_t *value);

#ifdef __cplusplus
}
#endif
#endif
