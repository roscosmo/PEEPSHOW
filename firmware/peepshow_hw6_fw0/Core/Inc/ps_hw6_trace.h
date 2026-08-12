#ifndef PS_HW6_TRACE_H
#define PS_HW6_TRACE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PS_HW6_TRACE_API_VERSION     (1UL)
#define PS_HW6_TRACE_STATUS_NOT_RUN  (0xFFFFFFFFUL)
#define PS_HW6_TRACE_STATUS_DISABLED (0xFFFFFFFEUL)

/* ThreadX user events are valid in the 0x1000..0xFFFF range. */
#define PS_HW6_TRACE_EVENT_OWNER_STATE       (0x5101UL)
#define PS_HW6_TRACE_EVENT_OWNER_REJECT      (0x5102UL)
#define PS_HW6_TRACE_EVENT_UI_DISPATCH       (0x5110UL)
#define PS_HW6_TRACE_EVENT_INPUT_BUTTON      (0x5120UL)
#define PS_HW6_TRACE_EVENT_POWER_START       (0x5130UL)
#define PS_HW6_TRACE_EVENT_PMIC_INTERRUPT    (0x5140UL)
#define PS_HW6_TRACE_EVENT_SLEEP             (0x5150UL)
#define PS_HW6_TRACE_EVENT_CLOCK_POLICY      (0x5160UL)

#define PS_HW6_TRACE_SLEEP_STAGE_PREP_START  (1UL)
#define PS_HW6_TRACE_SLEEP_STAGE_ENTER_STOP2 (2UL)
#define PS_HW6_TRACE_SLEEP_STAGE_WAKE_STOP2  (3UL)
#define PS_HW6_TRACE_SLEEP_STAGE_RECOVER     (4UL)

typedef struct
{
  uint32_t api_version;
  uint32_t insert_count;
  uint32_t success_count;
  uint32_t skipped_count;
  uint32_t error_count;
  uint32_t last_event_id;
  uint32_t last_info1;
  uint32_t last_info2;
  uint32_t last_info3;
  uint32_t last_info4;
  uint32_t last_status;
} ps_hw6_trace_probe_t;

extern volatile ps_hw6_trace_probe_t g_ps_hw6_trace_probe;

void PS_HW6_TraceOwnerState(uint32_t owner_id,
                            uint32_t from_state,
                            uint32_t event,
                            uint32_t to_state);
void PS_HW6_TraceOwnerReject(uint32_t owner_id,
                             uint32_t from_state,
                             uint32_t event,
                             uint32_t status);
void PS_HW6_TraceUiDispatch(uint32_t event,
                            uint32_t from_page,
                            uint32_t to_page,
                            uint32_t status);
void PS_HW6_TraceInputButton(uint32_t button_id,
                             uint32_t destination_owner,
                             uint32_t send_status,
                             uint32_t reserved);
void PS_HW6_TracePowerStart(uint32_t event,
                            uint32_t hold_ticks,
                            uint32_t send_status,
                            uint32_t reserved);
void PS_HW6_TracePmicInterrupt(uint32_t pending_count,
                               uint32_t irq_count,
                               uint32_t level,
                               uint32_t snapshot_status);
void PS_HW6_TraceSleep(uint32_t stage,
                       uint32_t reason,
                       uint32_t state,
                       uint32_t status);
void PS_HW6_TraceClockPolicy(uint32_t profile,
                             uint32_t capabilities,
                             uint32_t status,
                             uint32_t sysclk_hz);

#ifdef __cplusplus
}
#endif

#endif /* PS_HW6_TRACE_H */
