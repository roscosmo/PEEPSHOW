#ifndef PS_HW6_OBJECT_LATENCY_H
#define PS_HW6_OBJECT_LATENCY_H

#include <stdint.h>

typedef enum
{
  PS_OBJECT_LATENCY_RECEIVE = 0,
  PS_OBJECT_LATENCY_ADVANCE,
  PS_OBJECT_LATENCY_COPY_BEGIN,
  PS_OBJECT_LATENCY_COPY_DONE,
  PS_OBJECT_LATENCY_CLOCK_READY,
  PS_OBJECT_LATENCY_DECODE_DONE,
  PS_OBJECT_LATENCY_SCHEDULE_DONE,
  PS_OBJECT_LATENCY_RUNTIME_RELEASED,
  PS_OBJECT_LATENCY_CANDIDATE_SEND,
  PS_OBJECT_LATENCY_DISPLAY_START,
  PS_OBJECT_LATENCY_DISPLAY_CLOCK,
  PS_OBJECT_LATENCY_PACK_DONE,
  PS_OBJECT_LATENCY_DISPLAY_RELEASE,
  PS_OBJECT_LATENCY_ADMISSION_RETURN,
  PS_OBJECT_LATENCY_EVENT_DONE,
  PS_OBJECT_LATENCY_PRESENT_BEGIN,
  PS_OBJECT_LATENCY_PRESENT_SEND,
  PS_OBJECT_LATENCY_PANEL_START,
  PS_OBJECT_LATENCY_PANEL_DONE,
  PS_OBJECT_LATENCY_PUBLISH_DONE,
  PS_OBJECT_LATENCY_COMPLETE,
  PS_OBJECT_LATENCY_STAGE_COUNT
} ps_object_latency_stage_t;

/* One explicitly armed runtime press; releases/timers do not replace it.
 * Times start after input delivery, not at the physical edge or STOP2 wake.
 * thRuntime owns capture lifetime; thDisplay writes only matching token stages.
 * Read while halted after completion. Never re-arm an outstanding lease. */
typedef struct
{
  uint32_t api_version;
  uint32_t request;
  uint32_t active;
  uint32_t complete;
  uint32_t sequence;
  uint32_t button;
  uint32_t scene_before;
  uint32_t scene_after;
  uint32_t lp_enabled;
  uint32_t wfi_returns;
  uint32_t tick_hz;
  uint32_t event_result;
  uint32_t status;
  uint32_t candidate_token;
  uint32_t candidate_count;
  uint32_t render_token;
  uint32_t panel_status;
  uint32_t valid[PS_OBJECT_LATENCY_STAGE_COUNT];
  uint32_t tick[PS_OBJECT_LATENCY_STAGE_COUNT];
  uint32_t hclk_hz[PS_OBJECT_LATENCY_STAGE_COUNT];
} ps_hw6_object_latency_probe_t;

extern volatile ps_hw6_object_latency_probe_t g_ps_object_latency_probe;

#endif
