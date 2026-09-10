#ifndef PS_HW6_OBJECT_DEVELOPMENT_H
#define PS_HW6_OBJECT_DEVELOPMENT_H
#include <stdint.h>

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
extern const uint8_t g_ps_object_development_egg[];
extern const uint32_t g_ps_object_development_egg_size;
#endif
