#ifndef PS_HW6_OBJECT_DEVELOPMENT_H
#define PS_HW6_OBJECT_DEVELOPMENT_H
#include <stdint.h>
#include "ps_scene_object_waiting.h"

#define PS_HW6_OBJECT_DEVELOPMENT_API_VERSION (4UL)

typedef struct
{
  uint32_t api_version;
  uint32_t launch_count;
  uint32_t launch_status;
  uint32_t render_request;
  uint32_t render_complete;
  uint32_t render_status;
  uint32_t queue_status;
  uint32_t wait_status;
  uint32_t lease_fault;
  uint32_t elapsed_ms;
  uint32_t next_tick;
  uint32_t state_id;
  uint32_t first_asset_id;
  uint32_t admission_blockers;
} ps_hw6_object_development_probe_t;

extern volatile ps_hw6_object_development_probe_t g_ps_object_development_probe;
extern volatile uint32_t g_ps_object_development_request;

#define PS_HW6_OBJECT_LPBAM_PREPARE_API_VERSION (1UL)
typedef struct
{
  uint32_t api_version;
  uint32_t request_count;
  uint32_t complete_count;
  uint32_t schedule_status;
  uint32_t payload_status;
  uint32_t reason;
  uint32_t step_count;
  uint32_t quantum_ms;
  uint32_t initial_remaining_ms;
  uint32_t frames_composed;
  uint32_t sequence_used;
  uint32_t chunk_used;
  uint32_t payload_used_bytes;
  uint32_t payload_capacity_bytes;
} ps_hw6_object_lpbam_prepare_probe_t;

extern volatile ps_hw6_object_lpbam_prepare_probe_t g_ps_object_lpbam_prepare_probe;
extern volatile uint32_t g_ps_object_lpbam_prepare_request;

/* Requests 1/2 launch the existing single-scene development subset awake/LPBAM.
 * Requests 3/4 explicitly select all-scene admission and fresh scene exits,
 * awake/LPBAM respectively. Neither authorizes installation or export. */
#define PS_HW6_OBJECT_LPBAM_API_VERSION (1UL)
typedef struct
{
  uint32_t api_version;
  uint32_t enabled;
  uint32_t fault;
  uint32_t publish_count;
  uint32_t publish_status;
  uint32_t deadline_tick;
  uint32_t commit_remaining_ticks;
  uint32_t compare_count;
  uint32_t wake_compare_count;
  uint32_t sleep_count;
  uint32_t physical_count;
  uint32_t clock_status;
  uint32_t sleep_ms;
  uint32_t reconciled_count;
  uint32_t barrier;
  uint32_t entry_baseline;
  uint64_t missing_ms;
} ps_hw6_object_lpbam_probe_t;
extern volatile ps_hw6_object_lpbam_probe_t g_ps_object_lpbam_probe;
uint32_t PS_HW6_RTOS_ObjectSleepClockBegin(void);
void PS_HW6_RTOS_ObjectSleepClockFinish(void);
extern const uint8_t g_ps_object_development_egg[];
extern const uint32_t g_ps_object_development_egg_size;
#endif
