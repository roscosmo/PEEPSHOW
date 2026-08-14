#ifndef PS_HW6_RTOS_PROBE_H
#define PS_HW6_RTOS_PROBE_H

#include <stdint.h>

#include "tx_api.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PS_HW6_RTOS_PROBE_MAGIC          (0x48365254UL)
#define PS_HW6_RTOS_PROBE_VERSION        (29UL)
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

typedef enum
{
  PS_HW6_RUNTIME_CLASS_NONE = 0,
  PS_HW6_RUNTIME_CLASS_SHELL,
  PS_HW6_RUNTIME_CLASS_LP_GRAPH,
  PS_HW6_RUNTIME_CLASS_LP_MODULE,
  PS_HW6_RUNTIME_CLASS_RT_SCENE,
  PS_HW6_RUNTIME_CLASS_INSTALLER
} ps_hw6_runtime_class_t;

typedef enum
{
  PS_HW6_RUNTIME_EXEC_NONE = 0,
  PS_HW6_RUNTIME_EXEC_REACTIVE,
  PS_HW6_RUNTIME_EXEC_REALTIME
} ps_hw6_runtime_execution_t;

typedef enum
{
  PS_HW6_RUNTIME_LIFECYCLE_NONE = 0,
  PS_HW6_RUNTIME_LIFECYCLE_MOUNTED,
  PS_HW6_RUNTIME_LIFECYCLE_RUNNING,
  PS_HW6_RUNTIME_LIFECYCLE_SUSPENDED,
  PS_HW6_RUNTIME_LIFECYCLE_STOPPING,
  PS_HW6_RUNTIME_LIFECYCLE_ERROR
} ps_hw6_runtime_lifecycle_t;

typedef enum
{
  PS_HW6_RUNTIME_EVENT_NONE = 0,
  PS_HW6_RUNTIME_EVENT_BOOT_SHELL,
  PS_HW6_RUNTIME_EVENT_INSTALLER_ENTER,
  PS_HW6_RUNTIME_EVENT_INSTALLER_COMPLETE,
  PS_HW6_RUNTIME_EVENT_INSTALLER_ERROR,
  PS_HW6_RUNTIME_EVENT_PACKAGE_ACTIVATE_STUB,
  PS_HW6_RUNTIME_EVENT_PACKAGE_REACTIVE_ACTIVATE_STUB,
  PS_HW6_RUNTIME_EVENT_PACKAGE_REALTIME_ACTIVATE_STUB,
  PS_HW6_RUNTIME_EVENT_PACKAGE_RETURN,
  PS_HW6_RUNTIME_EVENT_SUSPEND,
  PS_HW6_RUNTIME_EVENT_RESUME
} ps_hw6_runtime_event_t;

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
  uint32_t thread_stack_config_bytes[PS_HW6_RTOS_OWNER_COUNT];
  uint32_t thread_stack_start[PS_HW6_RTOS_OWNER_COUNT];
  uint32_t thread_stack_end[PS_HW6_RTOS_OWNER_COUNT];
  uint32_t thread_stack_size[PS_HW6_RTOS_OWNER_COUNT];
  uint32_t thread_stack_ptr[PS_HW6_RTOS_OWNER_COUNT];
  uint32_t thread_stack_highest_ptr[PS_HW6_RTOS_OWNER_COUNT];

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

  uint32_t stop2_eligibility_request_count;
  uint32_t stop2_eligibility_last_status;
  uint32_t stop2_eligibility_last_tick;
  uint32_t stop2_eligibility_ready;
  uint32_t stop2_eligibility_blocker_mask;
  uint32_t stop2_eligibility_pending_mask;
  uint32_t stop2_eligibility_clock_capabilities;
  uint32_t stop2_eligibility_clock_domains;
  uint32_t stop2_eligibility_readback_domains;
  uint32_t stop2_eligibility_lpbam_ready;
  uint32_t stop2_eligibility_power_state;
  uint32_t stop2_eligibility_pmic_state;
  uint32_t stop2_eligibility_battery_policy;
  uint32_t stop2_eligibility_runtime_class;
  uint32_t stop2_eligibility_runtime_execution;
  uint32_t stop2_eligibility_runtime_lifecycle;

  uint32_t stop2_control_request_count;
  uint32_t stop2_control_last_status;
  uint32_t stop2_control_last_tick;
  uint32_t stop2_control_eligibility_status;
  uint32_t stop2_control_eligibility_blocker_mask;
  uint32_t stop2_control_eligibility_pending_mask;
  uint32_t stop2_control_entry_attempt_count;
  uint32_t stop2_control_entry_status;
  uint32_t stop2_control_stop2_count_before;
  uint32_t stop2_control_stop2_count_after;

  uint32_t stop2_auto_enabled;
  uint32_t stop2_auto_check_count;
  uint32_t stop2_auto_entry_count;
  uint32_t stop2_auto_skip_count;
  uint32_t stop2_auto_last_status;
  uint32_t stop2_auto_last_tick;
  uint32_t stop2_auto_next_tick;
  uint32_t stop2_auto_idle_start_tick;
  uint32_t stop2_auto_idle_ticks;
  uint32_t stop2_auto_required_idle_ticks;
  uint32_t stop2_auto_blocker_mask;
  uint32_t stop2_auto_pending_mask;
  uint32_t stop2_auto_queue_pending_mask;
  uint32_t stop2_auto_eligibility_status;
  uint32_t stop2_auto_entry_status;

  uint32_t stop2_lpbam_prepare_request_count;
  uint32_t stop2_lpbam_prepare_last_tick;
  uint32_t stop2_lpbam_prepare_send_status;
  uint32_t stop2_lpbam_prepare_wait_status;
  uint32_t stop2_lpbam_prepare_ack_flags;
  uint32_t stop2_lpbam_prepare_owner_status;
  uint32_t stop2_lpbam_prepare_ready_after;
  uint32_t stop2_lpbam_prepare_display_clear_count;
  uint32_t stop2_lpbam_abort_request_count;
  uint32_t stop2_lpbam_abort_last_tick;
  uint32_t stop2_lpbam_abort_send_status;
  uint32_t stop2_lpbam_abort_wait_status;
  uint32_t stop2_lpbam_abort_ack_flags;
  uint32_t stop2_lpbam_abort_owner_status;

  uint32_t stop2_wake_classify_count;
  uint32_t stop2_wake_classify_tick;
  uint32_t stop2_wake_source_mask;
  uint32_t stop2_wake_primary_cause;
  uint32_t stop2_wake_unknown_count;
  uint32_t stop2_wake_start_count;
  uint32_t stop2_wake_button_count;
  uint32_t stop2_wake_joystick_count;
  uint32_t stop2_wake_sensor_count;
  uint32_t stop2_wake_pmic_count;
  uint32_t stop2_wake_rtc_count;
  uint32_t stop2_wake_usb_count;
  uint32_t stop2_wake_fault_count;
  uint32_t stop2_wake_exti_rising;
  uint32_t stop2_wake_exti_falling;
  uint32_t stop2_wake_exti_imr;
  uint32_t stop2_wake_gpioa_before_idr;
  uint32_t stop2_wake_gpiob_before_idr;
  uint32_t stop2_wake_gpioc_before_idr;
  uint32_t stop2_wake_gpioa_after_idr;
  uint32_t stop2_wake_gpiob_after_idr;
  uint32_t stop2_wake_gpioc_after_idr;
  uint32_t stop2_wake_button_edges_before;
  uint32_t stop2_wake_button_edges_after;
  uint32_t stop2_wake_pmic_edges_before;
  uint32_t stop2_wake_pmic_edges_after;
  uint32_t stop2_wake_dbgmcu_cr_before;
  uint32_t stop2_wake_dbgmcu_cr_after;
  uint32_t stop2_wake_scb_icsr_before;
  uint32_t stop2_wake_scb_icsr_after;
  uint32_t stop2_wake_scb_scr_before;
  uint32_t stop2_wake_scb_scr_after;
  uint32_t stop2_wake_scb_shcsr_before;
  uint32_t stop2_wake_scb_shcsr_after;
  uint32_t stop2_wake_pwr_sr_before;
  uint32_t stop2_wake_pwr_sr_after;
  uint32_t stop2_wake_pwr_wusr_before;
  uint32_t stop2_wake_pwr_wusr_after;
  uint32_t stop2_wake_pwr_wucr1;
  uint32_t stop2_wake_pwr_wucr2;
  uint32_t stop2_wake_pwr_wucr3;
  uint32_t stop2_wake_nvic_ispr0_before;
  uint32_t stop2_wake_nvic_ispr1_before;
  uint32_t stop2_wake_nvic_ispr2_before;
  uint32_t stop2_wake_nvic_ispr3_before;
  uint32_t stop2_wake_nvic_ispr0_after;
  uint32_t stop2_wake_nvic_ispr1_after;
  uint32_t stop2_wake_nvic_ispr2_after;
  uint32_t stop2_wake_nvic_ispr3_after;
  uint32_t stop2_wake_nvic_iabr0_after;
  uint32_t stop2_wake_nvic_iabr1_after;
  uint32_t stop2_wake_nvic_iabr2_after;
  uint32_t stop2_wake_nvic_iabr3_after;
  uint32_t stop2_wake_nvic_iser0_after;
  uint32_t stop2_wake_nvic_iser1_after;
  uint32_t stop2_wake_nvic_iser2_after;
  uint32_t stop2_wake_nvic_iser3_after;

  uint32_t audio_clock_request_count;
  uint32_t audio_clock_release_count;
  uint32_t audio_clock_last_reason;
  uint32_t audio_clock_last_capabilities;
  uint32_t audio_clock_last_status;
  uint32_t audio_clock_reactive_sfx_status;
  uint32_t audio_clock_realtime_status;
  uint32_t audio_clock_release_status;

  uint32_t storage_clock_request_count;
  uint32_t storage_clock_release_count;
  uint32_t storage_clock_last_reason;
  uint32_t storage_clock_last_capabilities;
  uint32_t storage_clock_last_status;
  uint32_t storage_clock_export_status;
  uint32_t storage_clock_reclaim_status;
  uint32_t storage_clock_flash_init_status;
  uint32_t storage_clock_release_status;

  uint32_t runtime_event_count;
  uint32_t runtime_last_event;
  uint32_t runtime_last_status;
  uint32_t runtime_last_tick;
  uint32_t runtime_current_class;
  uint32_t runtime_previous_class;
  uint32_t runtime_return_class;
  uint32_t runtime_execution;
  uint32_t runtime_lifecycle;
  uint32_t runtime_active_package_id;
  uint32_t runtime_active_unit_id;
  uint32_t runtime_return_page;
  uint32_t runtime_boot_shell_count;
  uint32_t runtime_installer_enter_count;
  uint32_t runtime_installer_complete_count;
  uint32_t runtime_installer_error_count;
  uint32_t runtime_package_activate_stub_count;
  uint32_t runtime_package_reactive_activate_stub_count;
  uint32_t runtime_package_realtime_activate_stub_count;
  uint32_t runtime_package_return_count;
  uint32_t runtime_suspend_count;
  uint32_t runtime_resume_count;
  uint32_t runtime_owner_request_count;
  uint32_t runtime_owner_request_status;
  uint32_t runtime_admission_request_count;
  uint32_t runtime_admission_last_class;
  uint32_t runtime_admission_last_execution;
  uint32_t runtime_admission_last_capabilities;
  uint32_t runtime_admission_last_status;
  uint32_t runtime_admission_reactive_status;
  uint32_t runtime_admission_realtime_status;
  uint32_t runtime_active_capabilities;
  uint32_t runtime_suspend_saved_class;
  uint32_t runtime_suspend_saved_execution;
  uint32_t runtime_suspend_saved_lifecycle;
  uint32_t runtime_suspend_saved_capabilities;
  uint32_t runtime_suspend_clock_release_status;
  uint32_t runtime_resume_clock_request_status;
  uint32_t runtime_return_clock_release_status;
  uint32_t runtime_clock_request_count;
  uint32_t runtime_clock_release_count;
  uint32_t runtime_clock_last_reason;
  uint32_t runtime_clock_last_capabilities;
  uint32_t runtime_clock_last_status;
  uint32_t runtime_clock_reactive_status;
  uint32_t runtime_clock_realtime_status;
  uint32_t runtime_clock_release_status;
  uint32_t ui_clock_request_count;
  uint32_t ui_clock_release_count;
  uint32_t ui_clock_last_reason;
  uint32_t ui_clock_last_capabilities;
  uint32_t ui_clock_last_status;
  uint32_t ui_clock_reactive_status;
  uint32_t ui_clock_release_status;
  uint32_t display_clock_request_count;
  uint32_t display_clock_release_count;
  uint32_t display_clock_last_reason;
  uint32_t display_clock_last_capabilities;
  uint32_t display_clock_last_status;
  uint32_t display_clock_transfer_status;
  uint32_t display_clock_release_status;
  uint32_t input_policy_api_version;
  uint32_t input_policy_event_count;
  uint32_t input_policy_deliver_count;
  uint32_t input_policy_suppress_count;
  uint32_t input_policy_ui_deliver_count;
  uint32_t input_policy_runtime_deliver_count;
  uint32_t input_policy_overlay_deliver_count;
  uint32_t input_policy_lock_active;
  uint32_t input_policy_last_event;
  uint32_t input_policy_last_button_id;
  uint32_t input_policy_last_mask;
  uint32_t input_policy_last_timestamp;
  uint32_t input_policy_last_target;
  uint32_t input_policy_last_reason;
  uint32_t input_policy_last_status;
  uint32_t input_policy_last_runtime_class;
  uint32_t input_policy_last_runtime_lifecycle;
  uint32_t input_policy_last_ui_page;
  uint32_t input_policy_last_package_state;
  uint32_t input_policy_last_shutdown_state;
  uint32_t runtime_input_event_count;
  uint32_t runtime_input_button_count;
  uint32_t runtime_input_last_event;
  uint32_t runtime_input_last_button_id;
  uint32_t runtime_input_last_mask;
  uint32_t runtime_input_last_status;
  uint32_t runtime_input_last_tick;
  uint32_t admission_api_version;
  uint32_t admission_request_count;
  uint32_t admission_allow_count;
  uint32_t admission_deny_count;
  uint32_t admission_suspend_count;
  uint32_t admission_resume_count;
  uint32_t admission_runtime_suspended_by_system;
  uint32_t admission_runtime_suspended_action;
  uint32_t admission_runtime_resume_reason;
  uint32_t admission_runtime_resume_status;
  uint32_t admission_last_action;
  uint32_t admission_last_result;
  uint32_t admission_last_reason;
  uint32_t admission_last_status;
  uint32_t admission_last_runtime_class;
  uint32_t admission_last_runtime_lifecycle;
  uint32_t admission_last_ui_page;
  uint32_t admission_last_package_state;
  uint32_t admission_last_shutdown_state;
  uint32_t admission_last_overlay_active;
  uint32_t admission_last_tick;
  uint32_t ui_action_last;
  uint32_t ui_action_count;
  uint32_t ui_action_send_status;
  uint32_t ui_action_msc_enter_count;
  uint32_t ui_action_msc_exit_count;
  uint32_t ui_action_msc_exit_intercept_count;
  uint32_t ui_action_package_install_stub_count;
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
void PS_HW6_RTOS_Stop2WakeClassifyBegin(void);
void PS_HW6_RTOS_Stop2WakeClassifyAfterWake(void);

#ifdef __cplusplus
}
#endif

#endif /* PS_HW6_RTOS_PROBE_H */
