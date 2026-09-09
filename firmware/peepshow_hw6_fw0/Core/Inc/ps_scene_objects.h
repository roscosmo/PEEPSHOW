#ifndef PS_SCENE_OBJECTS_H
#define PS_SCENE_OBJECTS_H

#include "ps_egg_object_decoder.h"
#include "ps_scene_render_model.h"
#include "ps_scene_waiting_visual.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
  PS_SCENE_OBJECTS_OK = 0,
  PS_SCENE_OBJECTS_ARGUMENT,
  PS_SCENE_OBJECTS_CAPACITY,
  PS_SCENE_OBJECTS_TRANSACTION,
  PS_SCENE_OBJECTS_TIME
} ps_scene_objects_status_t;

typedef struct
{
  int16_t x;
  int16_t y;
  uint16_t static_frame;
  uint16_t visible;
  uint32_t cycle_ms;
} ps_scene_object_live_t;

/* Runtime-owner private. Successful decoder input must remain immutable. */
typedef struct
{
  ps_egg_object_view_t definition;
  ps_scene_object_live_t objects[PS_SCENE_RENDER_MODEL_ELEMENT_MAX];
  uint64_t elapsed_ms;
  uint64_t serial;
  uint32_t activation;
  uint32_t content_revision;
  uint16_t state;
  uint16_t suspended;
} ps_scene_objects_t;

typedef struct
{
  ps_scene_objects_t candidate;
  const ps_scene_objects_t *origin;
  uint64_t origin_serial;
  uint32_t action_count;
  uint32_t valid;
} ps_scene_objects_stage_t;

typedef struct
{
  ps_egg_object_definition_t effective;
  uint32_t step;
  uint32_t remaining_ms;
  uint32_t animation_visible;
} ps_scene_object_snapshot_t;

/* Pointer-free, immutable after publication. No shared mutable bank in a queue. */
typedef struct
{
  uint64_t elapsed_ms;
  uint32_t activation;
  uint32_t content_revision;
  uint16_t state;
  uint16_t count;
  ps_scene_object_snapshot_t objects[PS_SCENE_RENDER_MODEL_ELEMENT_MAX];
} ps_scene_objects_snapshot_t;

/*
 * Pure Engine core, not installed-package admission. Reuses existing object,
 * state, phase and sequence ceilings. Combined display schedule admission and
 * graph/variable/side-effect staging are the integrating caller's responsibility.
 * Init preserves bank on failure. Activation must be a fresh nonzero scene token.
 */
ps_scene_objects_status_t PS_SceneObjects_Init(ps_scene_objects_t *bank,
  const ps_egg_object_view_t *definition, uint32_t activation, uint16_t state);
void PS_SceneObjects_Clear(ps_scene_objects_t *bank);
ps_scene_objects_status_t PS_SceneObjects_Begin(const ps_scene_objects_t *bank,
  ps_scene_objects_stage_t *stage);
ps_scene_objects_status_t PS_SceneObjects_Apply(ps_scene_objects_stage_t *stage,
  uint16_t operation);
ps_scene_objects_status_t PS_SceneObjects_SelectState(ps_scene_objects_stage_t *stage,
  uint16_t state);
/* Commit once, only after all other staged actions and display admission pass. */
ps_scene_objects_status_t PS_SceneObjects_Commit(ps_scene_objects_t *bank,
  ps_scene_objects_stage_t *stage);
void PS_SceneObjects_Abort(ps_scene_objects_stage_t *stage);
/* Caller supplies elapsed milliseconds from the Platform's reconciled timebase.
 * Include STOP2 elapsed time. Suspension, unlike STOP2, pauses this clock.
 * These functions never read HAL/RTOS clocks or schedule hidden playback wakes.
 */
ps_scene_objects_status_t PS_SceneObjects_Advance(ps_scene_objects_t *bank,
  uint64_t elapsed_ms);
ps_scene_objects_status_t PS_SceneObjects_Suspend(ps_scene_objects_t *bank,
  uint32_t suspended);
ps_scene_objects_status_t PS_SceneObjects_Snapshot(const ps_scene_objects_t *bank,
  ps_scene_objects_snapshot_t *snapshot);

#ifdef __cplusplus
}
#endif
#endif
