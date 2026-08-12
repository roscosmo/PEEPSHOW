#ifndef PS_HW6_TRACE_H
#define PS_HW6_TRACE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PS_HW6_TRACE_API_VERSION     (2UL)
#define PS_HW6_TRACE_STATUS_NOT_RUN  (0xFFFFFFFFUL)
#define PS_HW6_TRACE_STATUS_DISABLED (0xFFFFFFFEUL)
#define PS_HW6_TRACE_STATUS_NOT_READY (0xFFFFFFFDUL)

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

#define PS_HW6_TRACE_SWO_TOKEN(a_, b_, c_) \
  (((uint32_t)(uint8_t)(a_)) | \
   (((uint32_t)(uint8_t)(b_)) << 8) | \
   (((uint32_t)(uint8_t)(c_)) << 16))

#define PS_HW6_TRACE_SWO_BOOT_DONE          \
  PS_HW6_TRACE_SWO_TOKEN('B', 'T', 'D')
#define PS_HW6_TRACE_SWO_STORAGE_READY      \
  PS_HW6_TRACE_SWO_TOKEN('R', 'D', 'Y')
#define PS_HW6_TRACE_SWO_FLASH_INIT_REQUEST \
  PS_HW6_TRACE_SWO_TOKEN('R', 'E', 'Q')
#define PS_HW6_TRACE_SWO_FLASH_WAKE_OK      \
  PS_HW6_TRACE_SWO_TOKEN('W', 'A', 'K')
#define PS_HW6_TRACE_SWO_FLASH_LAYOUT_OK    \
  PS_HW6_TRACE_SWO_TOKEN('L', 'A', 'Y')
#define PS_HW6_TRACE_SWO_FLASH_ERASE_START  \
  PS_HW6_TRACE_SWO_TOKEN('E', 'R', 'S')
#define PS_HW6_TRACE_SWO_FLASH_FORMAT_START \
  PS_HW6_TRACE_SWO_TOKEN('F', 'M', 'T')
#define PS_HW6_TRACE_SWO_FLASH_INIT_DONE    \
  PS_HW6_TRACE_SWO_TOKEN('D', 'O', 'N')
#define PS_HW6_TRACE_SWO_ERROR   \
  PS_HW6_TRACE_SWO_TOKEN('E', 'R', 'R')
#define PS_HW6_TRACE_SWO_MSC_EXPORT_START   \
  PS_HW6_TRACE_SWO_TOKEN('E', 'X', 'P')
#define PS_HW6_TRACE_SWO_MSC_OPEN_OK        \
  PS_HW6_TRACE_SWO_TOKEN('M', 'O', 'K')
#define PS_HW6_TRACE_SWO_MSC_RECOVERY_REQUIRED \
  PS_HW6_TRACE_SWO_TOKEN('R', 'E', 'C')
#define PS_HW6_TRACE_SWO_MSC_RECLAIM_START  \
  PS_HW6_TRACE_SWO_TOKEN('R', 'E', 'L')
#define PS_HW6_TRACE_SWO_MSC_RECLAIM_DONE   \
  PS_HW6_TRACE_SWO_TOKEN('R', 'D', 'N')

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
  uint32_t swo_emit_count;
  uint32_t swo_drop_count;
  uint32_t swo_disabled_count;
  uint32_t swo_last_token;
  uint32_t swo_last_status;
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
void PS_HW6_TraceSwoLifecycle(uint32_t token);

#ifdef __cplusplus
}
#endif

#endif /* PS_HW6_TRACE_H */
