#ifndef PS_HW6_RTOS_PROBE_H
#define PS_HW6_RTOS_PROBE_H

#include <stdint.h>

#include "tx_api.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PS_HW6_RTOS_PROBE_MAGIC          (0x48365254UL)
#define PS_HW6_RTOS_PROBE_VERSION        (8UL)
#define PS_HW6_RTOS_OWNER_COUNT          (9U)
#define PS_HW6_RTOS_QUEUE_COUNT          (9U)
#define PS_HW6_RTOS_EVENT_GROUP_COUNT    (4U)
#define PS_HW6_RTOS_MESSAGE_WORDS        (4U)
#define PS_HW6_RTOS_STORAGE_MSC_MAGIC    (0x4D534321UL)
#define PS_HW6_RTOS_STORAGE_MSC_TOKEN    (0x53544F52UL)
#define PS_HW6_RTOS_STORAGE_MSC_READ     (0x101UL)
#define PS_HW6_RTOS_STORAGE_MSC_WRITE    (0x102UL)
#define PS_HW6_RTOS_STORAGE_MSC_FLUSH    (0x103UL)
#define PS_HW6_RTOS_STORAGE_MSC_STATUS   (0x104UL)

typedef enum
{
  PS_HW6_RTOS_OWNER_POWER = 0,
  PS_HW6_RTOS_OWNER_AUDIO,
  PS_HW6_RTOS_OWNER_INPUT,
  PS_HW6_RTOS_OWNER_DISPLAY,
  PS_HW6_RTOS_OWNER_SENSOR,
  PS_HW6_RTOS_OWNER_STORAGE,
  PS_HW6_RTOS_OWNER_COMM,
  PS_HW6_RTOS_OWNER_UI,
  PS_HW6_RTOS_OWNER_RUNTIME
} PS_HW6_RTOS_OwnerId;

typedef struct
{
  uint32_t magic;
  uint32_t version;
  uint32_t phase;
  uint32_t init_complete;
  uint32_t runtime_complete;
  uint32_t boot_power_done;
  uint32_t boot_display_bootstrap_sent;
  uint32_t boot_home_suppressed;
  uint32_t boot_low_battery_ui_sent;
  uint32_t boot_low_battery_recover_ui_sent;
  uint32_t init_status;
  uint32_t init_error_step;
  uint32_t init_error_index;

  uint32_t ticks_per_second;
  uint32_t owner_count;
  uint32_t queue_count;
  uint32_t event_group_count;
  uint32_t owner_required_mask;
  uint32_t queue_required_mask;
  uint32_t event_required_mask;
  uint32_t owner_start_mask;
  uint32_t queue_selftest_mask;
  uint32_t event_selftest_mask;

  uint32_t pool_info_before_status;
  uint32_t pool_info_after_status;
  uint32_t pool_available_before;
  uint32_t pool_available_after;
  uint32_t pool_fragments_before;
  uint32_t pool_fragments_after;

  uint32_t stack_alloc_status[PS_HW6_RTOS_OWNER_COUNT];
  uint32_t queue_alloc_status[PS_HW6_RTOS_QUEUE_COUNT];
  uint32_t queue_create_status[PS_HW6_RTOS_QUEUE_COUNT];
  uint32_t queue_selftest_send_status[PS_HW6_RTOS_QUEUE_COUNT];
  uint32_t event_create_status[PS_HW6_RTOS_EVENT_GROUP_COUNT];
  uint32_t event_set_status[PS_HW6_RTOS_EVENT_GROUP_COUNT];
  uint32_t event_get_status[PS_HW6_RTOS_EVENT_GROUP_COUNT];
  uint32_t thread_create_status[PS_HW6_RTOS_OWNER_COUNT];

  uint32_t owner_heartbeat[PS_HW6_RTOS_OWNER_COUNT];
  uint32_t owner_last_tick[PS_HW6_RTOS_OWNER_COUNT];
  uint32_t queue_receive_count[PS_HW6_RTOS_QUEUE_COUNT];
  uint32_t queue_timeout_count[PS_HW6_RTOS_QUEUE_COUNT];
  uint32_t queue_message_error_count[PS_HW6_RTOS_QUEUE_COUNT];
  uint32_t queue_last_message[PS_HW6_RTOS_QUEUE_COUNT]
                             [PS_HW6_RTOS_MESSAGE_WORDS];

  uint32_t low_power_setup_count;
  uint32_t low_power_next_ticks;
  uint32_t low_power_enter_count;
  uint32_t low_power_exit_count;
  uint32_t low_power_adjust_count;

  uint32_t pwr_dbg_state;
  uint32_t pwr_dbg_toggle_count;
  uint32_t pwr_dbg_last_toggle_tick;

  uint32_t pmic_int_irq_count;
  uint32_t pmic_int_pending_count;
  uint32_t pmic_int_consumed_count;
  uint32_t pmic_int_last_pin;
  uint32_t pmic_int_last_level;
  uint32_t pmic_int_last_irq_tick;
  uint32_t pmic_int_last_consume_tick;

  uint32_t storage_clock_request_count;
  uint32_t storage_clock_release_count;
  uint32_t storage_clock_last_reason;
  uint32_t storage_clock_last_capabilities;
  uint32_t storage_clock_last_status;
  uint32_t storage_clock_export_status;
  uint32_t storage_clock_reclaim_status;
  uint32_t storage_clock_flash_init_status;
  uint32_t storage_clock_release_status;

  uint32_t ui_action_last;
  uint32_t ui_action_count;
  uint32_t ui_action_send_status;
  uint32_t ui_action_msc_enter_count;
  uint32_t ui_action_msc_exit_count;
  uint32_t ui_action_msc_exit_intercept_count;
  uint32_t ui_action_unsupported_count;
} PS_HW6_RTOS_Probe;

extern volatile PS_HW6_RTOS_Probe g_ps_hw6_rtos_probe;

UINT PS_HW6_RTOS_Init(TX_BYTE_POOL *pool);
UINT PS_HW6_RTOS_RequestUsbMscEnter(void);
UINT PS_HW6_RTOS_RequestUsbMscExit(void);
UINT PS_HW6_RTOS_DebugRequestUsbExport(void);
UINT PS_HW6_RTOS_DebugRequestUsbReclaim(void);
UINT PS_HW6_RTOS_DebugRequestStorageFlashInit(void);
void PS_HW6_RTOS_LowPowerTimerSetup(ULONG count);
void PS_HW6_RTOS_LowPowerEnter(void);
void PS_HW6_RTOS_LowPowerExit(void);
ULONG PS_HW6_RTOS_LowPowerTimerAdjust(void);
void PS_HW6_RTOS_RecordPmicIntExti(uint16_t gpio_pin, uint32_t level);

#ifdef __cplusplus
}
#endif

#endif /* PS_HW6_RTOS_PROBE_H */
