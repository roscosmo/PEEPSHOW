#ifndef PS_HW6_OBJECT_CANDIDATE_H
#define PS_HW6_OBJECT_CANDIDATE_H

#include <stdint.h>

#define PS_HW6_OBJECT_CANDIDATE_API_VERSION (1UL)

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
} ps_hw6_object_candidate_probe_t;

extern volatile ps_hw6_object_candidate_probe_t g_ps_object_candidate_probe;

#endif
