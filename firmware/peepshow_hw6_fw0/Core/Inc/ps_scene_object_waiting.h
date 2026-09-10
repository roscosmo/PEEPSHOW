#ifndef PS_SCENE_OBJECT_WAITING_H
#define PS_SCENE_OBJECT_WAITING_H

#include "ps_scene_object_render.h"

typedef enum
{
  PS_OBJECT_WAITING_OK = 0,
  PS_OBJECT_WAITING_ARGUMENT,
  PS_OBJECT_WAITING_CAPACITY,
  PS_OBJECT_WAITING_TIME
} ps_object_waiting_status_t;

/* Pointer-free schedule. No pixel payloads, DMA descriptors or owner state. */
typedef struct
{
  ps_scene_objects_snapshot_t base;
  uint32_t scene_id;
  uint32_t quantum_ms;
  uint32_t initial_remaining_ms;
  uint32_t step_count;
  uint16_t frames[PS_SCENE_WAITING_VISUAL_SEQUENCE_MAX][PS_SCENE_RENDER_MODEL_ELEMENT_MAX];
} ps_object_waiting_program_t;

/* Caller-owned scratch, separate from source/program. Never pass a live bank. */
typedef struct
{
  ps_scene_objects_t bank;
  ps_scene_objects_snapshot_t snapshot;
} ps_object_waiting_workspace_t;

/* Pure compilation from validated, immutable package definitions. Failure clears
 * step_count; source bank is unchanged. Hidden/static-masked clips do not demand
 * a display cadence. HOLD is step_count=1, quantum/initial_remaining=0.
 * Reject schedules exceeding the existing combined-step ceiling; never drop
 * frames or approximate timing. This does NOT admit hardware LPBAM resources.
 */
ps_object_waiting_status_t PS_ObjectWaiting_Build(const ps_scene_objects_t *bank,
  uint32_t scene_id, ps_object_waiting_program_t *program,
  ps_object_waiting_workspace_t *workspace);

/* Reconcile absolute scene elapsed time, not wall time. Shell suspension pauses
 * scene time; STOP2 does not. Caller invalidates program on scene/object/state
 * changes and reconciles hardware transfer progress separately on actual wake.
 */
ps_object_waiting_status_t PS_ObjectWaiting_Resolve(
  const ps_object_waiting_program_t *program, uint64_t elapsed_ms,
  uint32_t *step, uint32_t *remaining_ms);

/* Complete ordered scene model, including static objects above animated ones.
 * scratch must not alias program->base. No partial-sprite overpainting implied.
 */
ps_object_waiting_status_t PS_ObjectWaiting_Project(
  const ps_object_waiting_program_t *program, uint32_t step,
  ps_scene_objects_snapshot_t *scratch, ps_scene_render_model_t *model);

#endif
