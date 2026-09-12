#ifndef PS_HW6_OBJECT_CANDIDATE_H
#define PS_HW6_OBJECT_CANDIDATE_H

#include <stdint.h>

#define PS_HW6_OBJECT_CANDIDATE_API_VERSION (2UL)

/* Diagnostic only: 1 checks the embedded candidate; 2 deliberately removes its
 * private sprite catalog after validation to exercise display rejection. */
extern volatile uint32_t g_ps_object_candidate_request;

typedef struct
{
  uint32_t api_version;
  uint32_t request_id;
  uint32_t mode;
  uint32_t leased;
  uint32_t refused;
  uint32_t late_completions;
  uint32_t status;
  uint32_t profile_status;
  uint32_t profile_reason;
  uint32_t loader_reason;
  uint32_t graph_status;
  uint32_t schedule_status;
  uint32_t steps;
  uint32_t quantum_ms;
  uint32_t queue_status;
  uint32_t wait_status;
  uint32_t display_started;
  uint32_t display_complete;
  uint32_t display_status;
  uint32_t runtime_clock_status;
  uint32_t runtime_clock_release_status;
  uint32_t clock_status;
  uint32_t clock_release_status;
  uint32_t projection_status;
  uint32_t raster_status;
  uint32_t failed_step;
  uint32_t frames_composed;
  uint32_t payload_status;
  uint32_t payload_reason;
  uint32_t chunks;
  uint32_t bytes;
  uint32_t requested_scene;
  uint32_t scene_id;
  uint32_t scene_count;
} ps_hw6_object_candidate_probe_t;

extern volatile ps_hw6_object_candidate_probe_t g_ps_object_candidate_probe;

/* thRuntime only, serialized with existing candidate work. Preparation only:
 * checks a fresh initial presentation through thDisplay, never presents it or
 * changes the active scene/catalog. scene_id=0 selects package entry.
 * Returns 0 only after matching successful completion, 1 otherwise. Copies the
 * resident egg privately; timeout retains that copy until matching completion.
 * The caller's blob need only remain immutable through this synchronous call.
 * Existing installed/export single-scene restrictions remain unchanged. */
uint32_t PS_HW6_ObjectCandidate_CheckScene(const uint8_t *blob, uint32_t size,
  uint32_t scene_id);

typedef struct
{
  uint32_t scene_count;
  uint32_t checked;
  uint32_t failed_scene; /* Zero if refused before identifying a scene. */
  uint32_t request_id; /* Last child transaction started by this call, or zero. */
} ps_hw6_object_scene_set_result_t;

/* Same ownership; at most the existing loader scene limit of bounded checks.
 * Validates all initial presentations, not every possible state or mutation.
 * Stops at first failure, does not resume on late completion, and never retains
 * result. The input blob must stay immutable through the complete call. */
uint32_t PS_HW6_ObjectCandidate_CheckSceneSet(const uint8_t *blob, uint32_t size,
  ps_hw6_object_scene_set_result_t *result);

#endif
