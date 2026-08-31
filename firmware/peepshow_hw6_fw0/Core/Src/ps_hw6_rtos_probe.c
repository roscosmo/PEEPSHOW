#include "ps_hw6_rtos_probe.h"

#include <string.h>

#include "display_renderer.h"
#include "knobs_autogen.h"
#include "main.h"
#include "ps_hw6_clock_policy.h"
#include "ps_hw6_owner_services.h"
#include "ps_hw6_owner_state_machines.h"
#include "ps_hw6_trace.h"
#include "ps_input_buttons.h"
#include "ps_input_joystick.h"
#include "ps_input_logical.h"
#include "ps_lpbam_display_buffers.h"
#include "ps_egg_state_loader.h"
#include "ps_package_source.h"
#include "ps_power_state.h"
#include "ps_scene_runtime.h"
#include "ps_storage_filex_levelx.h"
#include "ps_storage_msc_bridge.h"
#include "ps_storage_state.h"
#include "ps_ui_router.h"

extern RTC_HandleTypeDef hrtc;

#define PS_HW6_RTOS_DEFAULT_STACK_BYTES  ((ULONG)KNOB_RTOS_DEFAULT_STACK_BYTES)
#define PS_HW6_RTOS_POWER_STACK_BYTES    ((ULONG)KNOB_RTOS_POWER_STACK_BYTES)
#define PS_HW6_RTOS_AUDIO_STACK_BYTES    ((ULONG)KNOB_RTOS_AUDIO_STACK_BYTES)
#define PS_HW6_RTOS_INPUT_STACK_BYTES    ((ULONG)KNOB_RTOS_INPUT_STACK_BYTES)
#define PS_HW6_RTOS_DISPLAY_STACK_BYTES  ((ULONG)KNOB_RTOS_DISPLAY_STACK_BYTES)
#define PS_HW6_RTOS_SENSOR_STACK_BYTES   ((ULONG)KNOB_RTOS_SENSOR_STACK_BYTES)
#define PS_HW6_RTOS_STORAGE_STACK_BYTES  ((ULONG)KNOB_RTOS_STORAGE_STACK_BYTES)
#define PS_HW6_RTOS_RUNTIME_STACK_BYTES  ((ULONG)KNOB_RTOS_RUNTIME_STACK_BYTES)
#define PS_HW6_RTOS_QUEUE_DEPTH          (8UL)
#define PS_HW6_RTOS_QUEUE_STORAGE_BYTES  (PS_HW6_RTOS_MESSAGE_WORDS * \
                                           PS_HW6_RTOS_QUEUE_DEPTH * \
                                           sizeof(ULONG))
#define PS_HW6_RTOS_OWNER_MASK           ((1UL << PS_HW6_RTOS_OWNER_COUNT) - 1UL)
#define PS_HW6_RTOS_EVENT_MASK           ((1UL << PS_HW6_RTOS_EVENT_GROUP_COUNT) - 1UL)
#define PS_HW6_RTOS_HEARTBEAT_TICKS       (25UL)
#define PS_HW6_RTOS_STARTUP_MAGIC         (0x52544F53UL)
#define PS_HW6_RTOS_STARTUP_KIND          (0x51554555UL)
#define PS_HW6_RTOS_COMMAND_MAGIC         (0x434D4421UL)
#define PS_HW6_RTOS_COMMAND_TOKEN         (0xC0DEC0DEUL)
#define PS_HW6_RTOS_DISPLAY_UI_MAGIC      (0x44554921UL)
#define PS_HW6_RTOS_UI_INPUT_MAGIC        (0x55494221UL)
#define PS_HW6_RTOS_UI_LIFECYCLE_MAGIC    (0x554C4621UL)
#define PS_HW6_RTOS_RUNTIME_INPUT_MAGIC   (0x52494221UL)
#define PS_HW6_RTOS_POWER_INPUT_MAGIC     (0x50574921UL)
#define PS_HW6_RTOS_INPUT_RAW_MAGIC       (0x49524157UL)
#define PS_HW6_RTOS_INPUT_JOYSTICK_MAGIC  (0x4A4F5921UL)
#define PS_HW6_RTOS_INPUT_RAW_BUTTON_MASK (0xFFUL)
#define PS_HW6_RTOS_INPUT_RAW_ACTIVE_SHIFT (8U)
#define PS_HW6_RTOS_INPUT_RAW_ACTIVE_MASK (0x1UL)
#define PS_HW6_RTOS_UI_INPUT_PRESS        (1UL)
#define PS_HW6_RTOS_UI_INPUT_JOYSTICK_STATE (2UL)
#define PS_HW6_RTOS_UI_INPUT_SOURCE_MASK  (0xFFUL)
#define PS_HW6_RTOS_UI_INPUT_CANDIDATE_SHIFT (8U)
#define PS_HW6_RTOS_UI_INPUT_CANDIDATE_MASK (0x0FUL)
#define PS_HW6_RTOS_UI_INPUT_RESOLVED_SHIFT (12U)
#define PS_HW6_RTOS_UI_INPUT_RESOLVED_MASK (0x0FUL)
#define PS_HW6_RTOS_UI_INPUT_DISPATCH_SHIFT (16U)
#define PS_HW6_RTOS_UI_INPUT_DISPATCH_MASK (0x01UL)
#define PS_HW6_RTOS_RUNTIME_INPUT_BUTTON_ID_MASK (0xFFUL)
#define PS_HW6_RTOS_RUNTIME_INPUT_BUTTON_MASK_SHIFT (8U)
#define PS_HW6_RTOS_RUNTIME_INPUT_BUTTON_MASK_MASK (0xFFUL)
#define PS_HW6_RTOS_DISPLAY_UI_FIELD_MASK  (0xFFUL)
#define PS_HW6_RTOS_DISPLAY_UI_CAL_SHIFT   (0U)
#define PS_HW6_RTOS_DISPLAY_UI_FOCUS_SHIFT (8U)
#define PS_HW6_RTOS_DISPLAY_UI_SHUTDOWN_SHIFT (16U)
#define PS_HW6_RTOS_DISPLAY_UI_COUNTDOWN_SHIFT (24U)
#define PS_HW6_RTOS_DISPLAY_UI_SHUTDOWN_MAX \
  ((uint32_t)PS_UI_ROUTER_SHUTDOWN_JOYSTICK_XYZ_ERROR)
#define PS_HW6_RTOS_COMMAND_POWER_WORKFLOW (1UL)
#define PS_HW6_RTOS_COMMAND_STABILIZE     (2UL)
#define PS_HW6_RTOS_COMMAND_RESUME        (3UL)
#define PS_HW6_RTOS_COMMAND_QUIESCE       (4UL)
#define PS_HW6_RTOS_COMMAND_POWER_QUIESCE (5UL)
#define PS_HW6_RTOS_COMMAND_POST_STOP_RESUME (6UL)
#define PS_HW6_RTOS_COMMAND_CLOCK_PROFILE (7UL)
#define PS_HW6_RTOS_COMMAND_USB_EXPORT (8UL)
#define PS_HW6_RTOS_COMMAND_USB_RECLAIM (9UL)
#define PS_HW6_RTOS_COMMAND_USB_BOOT_PARK (10UL)
#define PS_HW6_RTOS_COMMAND_STORAGE_FLASH_INIT (11UL)
#define PS_HW6_RTOS_COMMAND_PACKAGE_INSTALL_STUB (12UL)
#define PS_HW6_RTOS_COMMAND_RUNTIME_BOOT_SHELL (13UL)
#define PS_HW6_RTOS_COMMAND_RUNTIME_INSTALLER_ENTER (14UL)
#define PS_HW6_RTOS_COMMAND_RUNTIME_INSTALLER_COMPLETE (15UL)
#define PS_HW6_RTOS_COMMAND_RUNTIME_INSTALLER_ERROR (16UL)
#define PS_HW6_RTOS_COMMAND_RUNTIME_PACKAGE_ACTIVATE_STUB (17UL)
#define PS_HW6_RTOS_COMMAND_RUNTIME_PACKAGE_REACTIVE_STUB (18UL)
#define PS_HW6_RTOS_COMMAND_RUNTIME_PACKAGE_REALTIME_STUB (19UL)
#define PS_HW6_RTOS_COMMAND_RUNTIME_PACKAGE_RETURN (20UL)
#define PS_HW6_RTOS_COMMAND_RUNTIME_SUSPEND (21UL)
#define PS_HW6_RTOS_COMMAND_RUNTIME_RESUME (22UL)
#define PS_HW6_RTOS_COMMAND_DISPLAY_LPBAM_PREPARE (23UL)
#define PS_HW6_RTOS_COMMAND_DISPLAY_LPBAM_ABORT (24UL)
#define PS_HW6_RTOS_COMMAND_STORAGE_ATTACH (25UL)
#define PS_HW6_RTOS_COMMAND_COMM_BLE_MODE (26UL)
#define PS_HW6_RTOS_COMMAND_SENSOR_IMU_MODE (27UL)
#define PS_HW6_RTOS_COMMAND_DISPLAY_CURSOR_VISIBLE (28UL)
#define PS_HW6_RTOS_COMMAND_POWER_STOP2_RECHECK (29UL)
#define PS_HW6_RTOS_COMMAND_DISPLAY_LPBAM_EDGE_WAKE (30UL)
#define PS_HW6_RTOS_COMMAND_DISPLAY_LPBAM_WAKE_ABORT (31UL)
#define PS_HW6_RTOS_COMMAND_STORAGE_PACKAGE_INSTALL_EMBEDDED (32UL)
#define PS_HW6_RTOS_COMMAND_STORAGE_PACKAGE_LOAD_INSTALLED (33UL)
#define PS_HW6_RTOS_COMMAND_STORAGE_JOYSTICK_CALIBRATION_SAVE (34UL)
#define PS_HW6_RTOS_COMMAND_INPUT_JOYSTICK_WAKE_PUBLISH (35UL)
#define PS_HW6_RTOS_COMMAND_RUNTIME_INTERACTION_CUE (36UL)
#define PS_HW6_RTOS_COMMAND_RUNTIME_INTERACTION_START (37UL)
#define PS_HW6_RTOS_COMMAND_RUNTIME_INTERACTION_TIMEOUT (38UL)
#define PS_HW6_RTOS_COMMAND_RUNTIME_INTERACTION_INACTIVE (39UL)
#define PS_HW6_RTOS_COMMAND_AUDIO_PLAY_SFX (40UL)
#define PS_HW6_RTOS_COMMAND_PACKAGE_SCAN (41UL)
#define PS_HW6_RTOS_INTERACTION_ACTIVATION_FRAME_COUNT (3UL)
#define PS_HW6_RTOS_RTC_UNITS_PER_SECOND (256UL)
#define PS_HW6_RTOS_RTC_UNITS_PER_DAY \
  (86400UL * PS_HW6_RTOS_RTC_UNITS_PER_SECOND)
#define PS_HW6_RTOS_RTC_DIV16_COUNTS_PER_SECOND (2048UL)
#define PS_HW6_RTOS_RTC_DIV16_MAX_MS (32000UL)
#define PS_HW6_RTOS_EVENT_DEBUG_INDEX     (3U)
#define PS_HW6_RTOS_ACK_OWNER(owner_id)   (1UL << (owner_id))
#define PS_HW6_RTOS_CLOCK_ACK_SHIFT       (16U)
#define PS_HW6_RTOS_CLOCK_PROFILE_MASK    (0xFFUL)
#define PS_HW6_RTOS_CLOCK_CAP_SHIFT       (8U)
#define PS_HW6_RTOS_CLOCK_REQUESTER_SHIFT (16U)
#define PS_HW6_RTOS_CLOCK_REQUESTER_MASK  (0xFFUL)
#define PS_HW6_RTOS_OWNER_ACK_WAIT_TICKS  (1000UL)
#define PS_HW6_RTOS_STORAGE_STABILIZE_ACK_WAIT_TICKS (30000UL)
#define PS_HW6_RTOS_STATUS_NOT_RUN        (0xFFFFFFFFUL)
#define PS_HW6_RTOS_WAKE_SOURCE_START     (0x00000001UL)
#define PS_HW6_RTOS_WAKE_SOURCE_BUTTON    (0x00000002UL)
#define PS_HW6_RTOS_WAKE_SOURCE_JOYSTICK  (0x00000004UL)
#define PS_HW6_RTOS_WAKE_SOURCE_SENSOR    (0x00000008UL)
#define PS_HW6_RTOS_WAKE_SOURCE_PMIC      (0x00000010UL)
#define PS_HW6_RTOS_WAKE_SOURCE_RTC       (0x00000020UL)
#define PS_HW6_RTOS_WAKE_SOURCE_USB       (0x00000040UL)
#define PS_HW6_RTOS_WAKE_SOURCE_FAULT     (0x00000080UL)
#define PS_HW6_RTOS_WAKE_SOURCE_UNKNOWN   (0x80000000UL)
#define PS_HW6_RTOS_WAKE_CAUSE_NONE       (0UL)
#define PS_HW6_RTOS_WAKE_CAUSE_START      (1UL)
#define PS_HW6_RTOS_WAKE_CAUSE_BUTTON     (2UL)
#define PS_HW6_RTOS_WAKE_CAUSE_JOYSTICK   (3UL)
#define PS_HW6_RTOS_WAKE_CAUSE_SENSOR     (4UL)
#define PS_HW6_RTOS_WAKE_CAUSE_PMIC       (5UL)
#define PS_HW6_RTOS_WAKE_CAUSE_RTC        (6UL)
#define PS_HW6_RTOS_WAKE_CAUSE_USB        (7UL)
#define PS_HW6_RTOS_WAKE_CAUSE_FAULT      (8UL)
#define PS_HW6_RTOS_WAKE_CAUSE_UNKNOWN    (9UL)
#define PS_HW6_RTOS_WAKE_BUTTON_PIN_MASK \
  ((uint32_t)(BTN_A_Pin | BTN_B_Pin | BTN_L_Pin | BTN_R_Pin))
#define PS_HW6_RTOS_WAKE_KNOWN_EXTI_MASK \
  ((uint32_t)(BTN_START_Pin | BTN_A_Pin | BTN_B_Pin | BTN_L_Pin | \
              BTN_R_Pin | JOY_INT_Pin | MPU_INT_Pin | PMIC_INT_Pin))
#define PS_HW6_RTOS_AUDIO_CLOCK_REASON_NONE       (0UL)
#define PS_HW6_RTOS_AUDIO_CLOCK_REASON_REACTIVE_SFX (1UL)
#define PS_HW6_RTOS_AUDIO_CLOCK_REASON_REALTIME_MIXER (2UL)
#define PS_HW6_RTOS_AUDIO_CLOCK_REASON_RELEASE    (3UL)
#define PS_HW6_RTOS_AUDIO_CLOCK_SAI_CAPABILITIES \
  (PS_HW6_CLOCK_CAP_SAI_AUDIO_ACTIVE)
#define PS_HW6_RTOS_STORAGE_CLOCK_REASON_NONE       (0UL)
#define PS_HW6_RTOS_STORAGE_CLOCK_REASON_MSC_EXPORT (1UL)
#define PS_HW6_RTOS_STORAGE_CLOCK_REASON_MSC_RECLAIM (2UL)
#define PS_HW6_RTOS_STORAGE_CLOCK_REASON_FLASH_INIT (3UL)
#define PS_HW6_RTOS_STORAGE_CLOCK_REASON_RELEASE    (4UL)
#define PS_HW6_RTOS_STORAGE_CLOCK_REASON_ATTACH     (5UL)
#define PS_HW6_RTOS_STORAGE_CLOCK_REASON_POST_STOP_RESUME (6UL)
#define PS_HW6_RTOS_STORAGE_CLOCK_REASON_PACKAGE_LOAD (7UL)
#define PS_HW6_RTOS_STORAGE_CLOCK_REASON_POWER_QUIESCE (8UL)
#define PS_HW6_RTOS_STORAGE_CLOCK_REASON_JOYSTICK_CALIBRATION (9UL)
#define PS_HW6_RTOS_STORAGE_CLOCK_MSC_CAPABILITIES \
  (PS_HW6_CLOCK_CAP_USB_DEVICE_ACTIVE | PS_HW6_CLOCK_CAP_OCTOSPI_ACTIVE)
#define PS_HW6_RTOS_STORAGE_CLOCK_FLASH_CAPABILITIES \
  (PS_HW6_CLOCK_CAP_OCTOSPI_ACTIVE)
#define PS_HW6_RTOS_RUNTIME_CLOCK_REASON_NONE       (0UL)
#define PS_HW6_RTOS_RUNTIME_CLOCK_REASON_REACTIVE_TRANSACTION (1UL)
#define PS_HW6_RTOS_RUNTIME_CLOCK_REASON_REALTIME_DEADLINE (2UL)
#define PS_HW6_RTOS_RUNTIME_CLOCK_REASON_RELEASE    (3UL)
#define PS_HW6_RTOS_RUNTIME_CLOCK_REACTIVE_CAPABILITIES \
  (PS_HW6_CLOCK_CAP_REACTIVE_TRANSACTION_ACTIVE)
#define PS_HW6_RTOS_RUNTIME_CLOCK_REALTIME_CAPABILITIES \
  (PS_HW6_CLOCK_CAP_REALTIME_DEADLINE_ACTIVE)
#define PS_HW6_RTOS_UI_CLOCK_REASON_NONE       (0UL)
#define PS_HW6_RTOS_UI_CLOCK_REASON_REACTIVE_TRANSACTION (1UL)
#define PS_HW6_RTOS_UI_CLOCK_REASON_RELEASE    (2UL)
#define PS_HW6_RTOS_UI_CLOCK_REACTIVE_CAPABILITIES \
  (PS_HW6_CLOCK_CAP_REACTIVE_TRANSACTION_ACTIVE)

#define PS_HW6_RTOS_DISPLAY_CLOCK_REASON_NONE       (0UL)
#define PS_HW6_RTOS_DISPLAY_CLOCK_REASON_TRANSFER   (1UL)
#define PS_HW6_RTOS_DISPLAY_CLOCK_REASON_RELEASE    (2UL)
#define PS_HW6_RTOS_DISPLAY_CLOCK_TRANSFER_CAPABILITIES \
  (PS_HW6_CLOCK_CAP_DISPLAY_TRANSFER_ACTIVE)
#define PS_HW6_RTOS_LPBAM_HANDOFF_SOURCE_PHASE (1UL)
#define PS_HW6_RTOS_LPBAM_HANDOFF_TARGET_PHASE (0UL)

#define PS_HW6_RTOS_INPUT_POLICY_API_VERSION (2UL)
#define PS_HW6_RTOS_INPUT_POLICY_TARGET_NONE (0UL)
#define PS_HW6_RTOS_INPUT_POLICY_TARGET_UI   (1UL)
#define PS_HW6_RTOS_INPUT_POLICY_TARGET_RUNTIME (2UL)
#define PS_HW6_RTOS_INPUT_POLICY_REASON_NONE (0UL)
#define PS_HW6_RTOS_INPUT_POLICY_REASON_UI_FOCUS (1UL)
#define PS_HW6_RTOS_INPUT_POLICY_REASON_RUNTIME_NOT_READY (2UL)
#define PS_HW6_RTOS_INPUT_POLICY_REASON_LOCKED (3UL)
#define PS_HW6_RTOS_INPUT_POLICY_REASON_UNSUPPORTED_EVENT (4UL)
#define PS_HW6_RTOS_INPUT_POLICY_REASON_INVALID_BUTTON (5UL)
#define PS_HW6_RTOS_INPUT_POLICY_REASON_SEND_FAILED (6UL)
#define PS_HW6_RTOS_INPUT_POLICY_REASON_SYSTEM_OVERLAY (7UL)
#define PS_HW6_RTOS_INPUT_POLICY_REASON_RUNTIME_FOCUS (8UL)
#define PS_HW6_RTOS_INPUT_POLICY_REASON_UNSUPPORTED_CLASS (9UL)
#define PS_HW6_RTOS_INPUT_POLICY_REASON_RUNTIME_INACTIVE (10UL)
#define PS_HW6_RTOS_INPUT_POLICY_STATUS_SUPPRESSED (0xFFFFFFFEUL)

#define PS_HW6_RTOS_ADMISSION_API_VERSION (1UL)
#define PS_HW6_RTOS_ADMISSION_ACTION_NONE (0UL)
#define PS_HW6_RTOS_ADMISSION_ACTION_UI_MSC_ENTER (1UL)
#define PS_HW6_RTOS_ADMISSION_ACTION_UI_MSC_EXIT (2UL)
#define PS_HW6_RTOS_ADMISSION_ACTION_UI_PACKAGE_INSTALL_STUB (3UL)
#define PS_HW6_RTOS_ADMISSION_ACTION_POWER_SHUTDOWN_PREP (4UL)
#define PS_HW6_RTOS_ADMISSION_ACTION_POWER_BATTERY_CRITICAL_SHIP_PREP (5UL)
#define PS_HW6_RTOS_ADMISSION_ACTION_POWER_BOOT_LOW_BATTERY_SHIP_PREP (6UL)
#define PS_HW6_RTOS_ADMISSION_RESULT_DENY (0UL)
#define PS_HW6_RTOS_ADMISSION_RESULT_ALLOW (1UL)
#define PS_HW6_RTOS_ADMISSION_RESULT_ALLOW_AFTER_SUSPEND (2UL)
#define PS_HW6_RTOS_ADMISSION_REASON_NONE (0UL)
#define PS_HW6_RTOS_ADMISSION_REASON_UI_SHELL (1UL)
#define PS_HW6_RTOS_ADMISSION_REASON_SYSTEM_OVERLAY (2UL)
#define PS_HW6_RTOS_ADMISSION_REASON_RUNTIME_SUSPENDED (3UL)
#define PS_HW6_RTOS_ADMISSION_REASON_INSTALLER (4UL)
#define PS_HW6_RTOS_ADMISSION_REASON_SYSTEM_BUSY (5UL)
#define PS_HW6_RTOS_ADMISSION_REASON_SEND_FAILED (6UL)
#define PS_HW6_RTOS_ADMISSION_REASON_UNSUPPORTED (7UL)
#define PS_HW6_RTOS_ADMISSION_RESUME_REASON_NONE (0UL)
#define PS_HW6_RTOS_ADMISSION_RESUME_REASON_START_CANCEL (1UL)

#define PS_HW6_RTOS_STOP2_BLOCK_BOOT_NOT_READY        (1UL << 0)
#define PS_HW6_RTOS_STOP2_BLOCK_POWER_STATE           (1UL << 1)
#define PS_HW6_RTOS_STOP2_BLOCK_PMIC_STATE            (1UL << 2)
#define PS_HW6_RTOS_STOP2_BLOCK_BATTERY_POLICY        (1UL << 3)
#define PS_HW6_RTOS_STOP2_BLOCK_CLOCK_CAPABILITY      (1UL << 4)
#define PS_HW6_RTOS_STOP2_BLOCK_CLOCK_READBACK_DOMAIN (1UL << 5)
#define PS_HW6_RTOS_STOP2_BLOCK_AUTO_DISABLED         (1UL << 6)
#define PS_HW6_RTOS_STOP2_BLOCK_RUNTIME_BUSY          (1UL << 7)
#define PS_HW6_RTOS_STOP2_BLOCK_UI_BUSY               (1UL << 8)
#define PS_HW6_RTOS_STOP2_BLOCK_DISPLAY_PENDING       (1UL << 9)
#define PS_HW6_RTOS_STOP2_BLOCK_STORAGE_USB_BUSY      (1UL << 10)
#define PS_HW6_RTOS_STOP2_BLOCK_INPUT_PENDING         (1UL << 11)
#define PS_HW6_RTOS_STOP2_BLOCK_QUEUE_PENDING         (1UL << 12)
#define PS_HW6_RTOS_STOP2_BLOCK_LPBAM_NOT_READY       (1UL << 13)
#define PS_HW6_RTOS_STOP2_BLOCK_IDLE_PERIPH_NOT_PARKED (1UL << 14)
#define PS_HW6_RTOS_STOP2_PENDING_OWNER_QUIESCE       (1UL << 0)
#define PS_HW6_RTOS_STOP2_PENDING_LPBAM_VALIDATION    (1UL << 1)
#define PS_HW6_RTOS_STOP2_PENDING_IDLE_WINDOW         (1UL << 2)
#define PS_HW6_RTOS_STOP2_DISPLAY_BACKEND_NONE       (0UL)
#define PS_HW6_RTOS_STOP2_DISPLAY_BACKEND_HELD_FRAME (1UL)
#define PS_HW6_RTOS_STOP2_DISPLAY_BACKEND_LPBAM      (2UL)
#define PS_HW6_RTOS_STOP2_LPBAM_EDGE_IDLE            (0UL)
#define PS_HW6_RTOS_STOP2_LPBAM_EDGE_REQUESTED       (1UL)
#define PS_HW6_RTOS_STOP2_LPBAM_EDGE_ARMED           (2UL)
#define PS_HW6_RTOS_STOP2_LPBAM_EDGE_FAILED          (3UL)

#define PS_HW6_RTOS_PHASE_INIT            (0x6600UL)
#define PS_HW6_RTOS_PHASE_ALLOCATED       (0x6610UL)
#define PS_HW6_RTOS_PHASE_OBJECTS_CREATED (0x6620UL)
#define PS_HW6_RTOS_PHASE_READY           (0x66FFUL)

#define PS_HW6_RTOS_STEP_POOL_INFO        (1UL)
#define PS_HW6_RTOS_STEP_STACK_ALLOC      (2UL)
#define PS_HW6_RTOS_STEP_QUEUE_ALLOC      (3UL)
#define PS_HW6_RTOS_STEP_QUEUE_CREATE     (4UL)
#define PS_HW6_RTOS_STEP_EVENT_CREATE     (5UL)
#define PS_HW6_RTOS_STEP_EVENT_TEST       (6UL)
#define PS_HW6_RTOS_STEP_QUEUE_TEST       (7UL)
#define PS_HW6_RTOS_STEP_THREAD_CREATE    (8UL)

volatile PS_HW6_RTOS_Probe g_ps_hw6_rtos_probe;
volatile uint32_t g_ps_hw6_rtos_low_power_usb_skip_count;
volatile uint32_t g_ps_hw6_power_stop2_eligibility_request;
volatile uint32_t g_ps_hw6_power_stop2_controlled_entry_request;
volatile uint32_t g_ps_hw6_audio_clock_probe_request;
volatile uint32_t g_ps_hw6_audio_clock_probe_release_request;
volatile uint32_t g_ps_hw6_runtime_reactive_stub_request;
volatile uint32_t g_ps_hw6_runtime_interaction_test_continuous;
volatile uint32_t g_ps_hw6_runtime_realtime_stub_request;
volatile uint32_t g_ps_hw6_runtime_return_request;
volatile uint32_t g_ps_hw6_runtime_suspend_request;
volatile uint32_t g_ps_hw6_runtime_resume_request;
volatile uint32_t g_ps_hw6_admission_msc_enter_dry_run_request;
volatile uint32_t g_ps_hw6_admission_power_shutdown_dry_run_request;
volatile uint32_t g_ps_hw6_admission_power_battery_dry_run_request;
volatile uint32_t g_ps_hw6_admission_power_cancel_dry_run_request;
volatile uint32_t g_ps_hw6_power_stop2_auto_idle_dry_run_request;
volatile uint32_t g_ps_hw6_power_stop2_auto_idle_entry_request;
volatile uint32_t g_ps_hw6_power_stop2_lpbam_prepare_request;
volatile uint32_t g_ps_hw6_power_stop2_lpbam_abort_request;
volatile uint32_t g_ps_hw6_power_stop2_lpbam_abort_late_test_request;
volatile uint32_t g_ps_hw6_power_stop2_display_backend_override;
volatile uint32_t g_ps_hw6_power_stop2_lpbam_awake_hold_enable;

typedef UINT (*PS_HW6_RTOS_DebugCommandFn)(void);

static TX_THREAD ps_threads[PS_HW6_RTOS_OWNER_COUNT];
static TX_QUEUE ps_queues[PS_HW6_RTOS_QUEUE_COUNT];
static TX_EVENT_FLAGS_GROUP ps_event_groups[PS_HW6_RTOS_EVENT_GROUP_COUNT];
static VOID *ps_thread_stacks[PS_HW6_RTOS_OWNER_COUNT];
static VOID *ps_queue_storage[PS_HW6_RTOS_QUEUE_COUNT];
static volatile PS_HW6_RTOS_DebugCommandFn ps_debug_usb_export_anchor;
static volatile PS_HW6_RTOS_DebugCommandFn ps_debug_usb_reclaim_anchor;
static volatile PS_HW6_RTOS_DebugCommandFn ps_debug_storage_flash_init_anchor;
static volatile PS_HW6_RTOS_DebugCommandFn ps_debug_storage_attach_anchor;
static volatile PS_HW6_RTOS_DebugCommandFn ps_debug_comm_ble_shutdown_anchor;
static volatile PS_HW6_RTOS_DebugCommandFn ps_debug_comm_ble_stop_anchor;
static volatile PS_HW6_RTOS_DebugCommandFn ps_debug_comm_ble_searching_anchor;
static volatile PS_HW6_RTOS_DebugCommandFn ps_debug_comm_ble_pairing_anchor;
static volatile PS_HW6_RTOS_DebugCommandFn ps_debug_comm_ble_connected_anchor;
static volatile PS_HW6_RTOS_DebugCommandFn ps_debug_imu_off_anchor;
static volatile PS_HW6_RTOS_DebugCommandFn ps_debug_imu_low_rate_anchor;
static volatile PS_HW6_RTOS_DebugCommandFn ps_debug_imu_event_armed_anchor;
static volatile PS_HW6_RTOS_DebugCommandFn ps_debug_imu_step_counter_anchor;
static volatile PS_HW6_RTOS_DebugCommandFn ps_debug_imu_streaming_anchor;
static volatile PS_HW6_RTOS_DebugCommandFn
  ps_debug_input_diagnostic_anchor;
static uint32_t ps_ui_boot_complete_sent;
static uint32_t ps_boot_package_launch_after_calibration;
static uint32_t ps_power_boot_done;
static uint32_t ps_power_boot_idle_peripheral_park_done;
static uint32_t ps_display_bootstrap_sent;
static uint32_t ps_stop2_lpbam_abort_late_test_active;
static uint32_t ps_stop2_lpbam_late_blocker_armed;
static uint32_t ps_display_blink_next_tick;
static uint32_t ps_joystick_calibration_review_next_tick;
static uint32_t ps_joystick_calibration_progress_render_next_tick;
static uint32_t ps_joystick_calibration_boot_load_started;
static volatile uint32_t ps_joystick_calibration_apply_pending;
static volatile uint32_t ps_joystick_calibration_save_completion_pending;
static uint32_t ps_joystick_awake_poll_next_tick;
static uint32_t ps_joystick_awake_published_direction_mask;
static uint32_t ps_joystick_awake_published_candidate_mask;
static uint32_t ps_joystick_logical_active_direction_mask;
static uint32_t ps_joystick_logical_press_tick;
static uint32_t ps_joystick_logical_next_repeat_tick;
static uint32_t ps_joystick_logical_hold_sent;
static volatile uint32_t ps_joystick_awake_publish_reset_pending;
static volatile uint32_t ps_input_activity_generation;
static volatile uint32_t ps_runtime_interaction_state;
static volatile uint32_t ps_runtime_interaction_cue_active;
static volatile uint32_t ps_runtime_interaction_activation_active;
static uint32_t ps_runtime_interaction_mode;
static uint32_t ps_runtime_interaction_deadline_tick;
static uint32_t ps_runtime_interaction_cue_deadline_tick;
static uint32_t ps_runtime_interaction_activation_frame;
static uint32_t ps_runtime_interaction_activation_deadline_tick;
static volatile uint32_t ps_runtime_interaction_epoch;
static volatile uint32_t ps_runtime_interaction_timeout_forced;
static volatile uint32_t ps_runtime_interaction_timeout_forced_epoch;
static volatile uint32_t ps_runtime_interaction_rtc_armed;
static volatile uint32_t ps_runtime_interaction_rtc_armed_epoch;
static uint32_t ps_runtime_interaction_rtc_start_units;
static uint32_t ps_runtime_interaction_rtc_remaining_ticks;
static volatile uint32_t ps_runtime_interaction_rtc_irq_expired;
static volatile uint32_t ps_runtime_interaction_rtc_command_queued;
static uint32_t ps_power_input_activity_generation;
static uint32_t ps_display_blink_visible;
static uint32_t ps_display_waiting_sequence_frame;
static uint32_t ps_display_waiting_sequence_count;
static volatile uint32_t ps_audio_sfx_pending;
static uint32_t ps_display_blink_stop2_suppressed;
static volatile uint32_t ps_display_blink_transfer_active;
static uint32_t ps_stop2_lpbam_edge_request_pending;
static uint32_t ps_stop2_lpbam_edge_rearm_needed;
static uint32_t ps_stop2_lpbam_edge_target_tick;
static uint32_t ps_stop2_lpbam_edge_start_phase;
static uint32_t ps_stop2_lpbam_edge_target_sequence_frame;
static uint32_t ps_stop2_lpbam_edge_render_count;
static uint32_t ps_stop2_lpbam_edge_page;
static volatile uint32_t ps_pmic_int_pending_count;
static volatile uint32_t ps_pmic_int_irq_count;
static volatile uint32_t ps_pmic_int_last_pin;
static volatile uint32_t ps_pmic_int_last_level;
static volatile uint32_t ps_pmic_int_last_irq_tick;
static uint32_t ps_pmic_int_consumed_count;
static volatile uint32_t ps_joystick_int_irq_count;
static volatile uint32_t ps_joystick_int_pending_count;
static uint32_t ps_joystick_int_consumed_count;
static volatile uint32_t ps_stop2_wake_button_edges_before;
static volatile uint32_t ps_stop2_wake_joystick_edges_before;
static volatile uint32_t ps_stop2_wake_pmic_edges_before;
static volatile uint32_t ps_stop2_wake_gpioa_before_idr;
static volatile uint32_t ps_stop2_wake_gpiob_before_idr;
static volatile uint32_t ps_stop2_wake_gpioc_before_idr;
static volatile uint32_t ps_stop2_wake_dbgmcu_cr_before;
static volatile uint32_t ps_stop2_wake_scb_icsr_before;
static volatile uint32_t ps_stop2_wake_scb_scr_before;
static volatile uint32_t ps_stop2_wake_scb_shcsr_before;
static volatile uint32_t ps_stop2_wake_pwr_sr_before;
static volatile uint32_t ps_stop2_wake_pwr_wusr_before;
static volatile uint32_t ps_stop2_wake_nvic_ispr0_before;
static volatile uint32_t ps_stop2_wake_nvic_ispr1_before;
static volatile uint32_t ps_stop2_wake_nvic_ispr2_before;
static volatile uint32_t ps_stop2_wake_nvic_ispr3_before;

static void PS_HW6_RTOS_SendCurrentUiRenderCommand(void);
static void PS_HW6_RTOS_RuntimePackageReturn(void);
static void PS_HW6_RTOS_RuntimeInteractionBegin(uint32_t now_tick);
static void PS_HW6_RTOS_RuntimeInteractionEnd(void);
static void PS_HW6_RTOS_RuntimeInteractionShowCue(uint32_t now_tick);
static void PS_HW6_RTOS_RuntimeInteractionActivate(uint32_t now_tick);
static void PS_HW6_RTOS_RuntimeInteractionService(uint32_t now_tick);
static void PS_HW6_RTOS_RuntimeInteractionRefresh(uint32_t logical_event,
                                                   uint32_t input_id,
                                                   uint32_t now_tick);
static HAL_StatusTypeDef PS_HW6_RTOS_InteractionRtcReadUnits(
  uint32_t *units);
static UINT PS_HW6_RTOS_SendDisplayUiRenderCommand(
  uint32_t page,
  uint32_t calibration_page,
  uint32_t focus_index,
  uint32_t shutdown_state,
  uint32_t shutdown_countdown_seconds);
static UINT PS_HW6_RTOS_RequestRuntimeCommand(ULONG command);
static uint32_t PS_HW6_RTOS_BeginBootHome(void);
static UINT PS_HW6_RTOS_SendModeCommand(uint32_t owner_id,
                                         ULONG command,
                                         uint32_t mode);
static UINT PS_HW6_RTOS_RequestStoragePackageLoadAndWait(void);
static uint32_t PS_HW6_RTOS_Stop2DisplayLpbamReady(void);
static uint32_t PS_HW6_RTOS_UiMscExportActive(void);
static void PS_HW6_RTOS_HandleUiRouterAction(uint32_t action);
static void PS_HW6_RTOS_RunStoragePackageScanRequest(void);

static CHAR *const ps_owner_names[PS_HW6_RTOS_OWNER_COUNT] =
{
  "thPower", "thAudio", "thInput", "thDisplay", "thSensor",
  "thStorage", "thComm", "thUI", "thRuntime"
};

static CHAR *const ps_queue_names[PS_HW6_RTOS_QUEUE_COUNT] =
{
  "qSysEvents", "qAudioCmd", "qInputRaw", "qDisplayCmd", "qSensorReq",
  "qStorageReq", "qCommCmd", "qUIEvents", "qRuntimeEvents"
};

static CHAR *const ps_event_names[PS_HW6_RTOS_EVENT_GROUP_COUNT] =
{
  "egMode", "egPower", "egHealth", "egDebug"
};

static const UINT ps_owner_priorities[PS_HW6_RTOS_OWNER_COUNT] =
{
  5U, 6U, 7U, 8U, 9U, 10U, 11U, 12U, 13U
};

static const ULONG ps_owner_stack_bytes[PS_HW6_RTOS_OWNER_COUNT] =
{
  PS_HW6_RTOS_POWER_STACK_BYTES,
  PS_HW6_RTOS_AUDIO_STACK_BYTES,
  PS_HW6_RTOS_INPUT_STACK_BYTES,
  PS_HW6_RTOS_DISPLAY_STACK_BYTES,
  PS_HW6_RTOS_SENSOR_STACK_BYTES,
  PS_HW6_RTOS_STORAGE_STACK_BYTES,
  PS_HW6_RTOS_DEFAULT_STACK_BYTES,
  PS_HW6_RTOS_DEFAULT_STACK_BYTES,
  PS_HW6_RTOS_RUNTIME_STACK_BYTES
};

static void PS_HW6_RTOS_RecordThreadStackProbe(uint32_t owner_id)
{
  TX_THREAD *thread;

  if (owner_id >= PS_HW6_RTOS_OWNER_COUNT)
  {
    return;
  }

  thread = &ps_threads[owner_id];
  g_ps_hw6_rtos_probe.thread_stack_config_bytes[owner_id] =
    (uint32_t)ps_owner_stack_bytes[owner_id];
  g_ps_hw6_rtos_probe.thread_stack_start[owner_id] =
    (uint32_t)(uintptr_t)thread->tx_thread_stack_start;
  g_ps_hw6_rtos_probe.thread_stack_end[owner_id] =
    (uint32_t)(uintptr_t)thread->tx_thread_stack_end;
  g_ps_hw6_rtos_probe.thread_stack_size[owner_id] =
    (uint32_t)thread->tx_thread_stack_size;
  g_ps_hw6_rtos_probe.thread_stack_ptr[owner_id] =
    (uint32_t)(uintptr_t)thread->tx_thread_stack_ptr;
  g_ps_hw6_rtos_probe.thread_stack_highest_ptr[owner_id] =
    (uint32_t)(uintptr_t)thread->tx_thread_stack_highest_ptr;
}

static void PS_HW6_RTOS_RecordFirstError(UINT status,
                                         uint32_t step,
                                         uint32_t index)
{
  if ((status != TX_SUCCESS) &&
      (g_ps_hw6_rtos_probe.init_status == TX_SUCCESS))
  {
    g_ps_hw6_rtos_probe.init_status = status;
    g_ps_hw6_rtos_probe.init_error_step = step;
    g_ps_hw6_rtos_probe.init_error_index = index;
  }
}

void PS_HW6_RTOS_RecordPmicIntExti(uint16_t gpio_pin, uint32_t level)
{
  if (gpio_pin != PMIC_INT_Pin)
  {
    return;
  }

  ps_pmic_int_irq_count++;
  ps_pmic_int_pending_count++;
  ps_pmic_int_last_pin = (uint32_t)gpio_pin;
  ps_pmic_int_last_level = level;
  ps_pmic_int_last_irq_tick = HAL_GetTick();

  g_ps_hw6_rtos_probe.pmic_int_irq_count = ps_pmic_int_irq_count;
  g_ps_hw6_rtos_probe.pmic_int_pending_count =
    ps_pmic_int_pending_count - ps_pmic_int_consumed_count;
  g_ps_hw6_rtos_probe.pmic_int_last_pin = ps_pmic_int_last_pin;
  g_ps_hw6_rtos_probe.pmic_int_last_level = ps_pmic_int_last_level;
  g_ps_hw6_rtos_probe.pmic_int_last_irq_tick = ps_pmic_int_last_irq_tick;
}

static uint32_t PS_HW6_RTOS_Stop2WakePrimary(uint32_t source_mask)
{
  if ((source_mask & PS_HW6_RTOS_WAKE_SOURCE_START) != 0UL)
  {
    return PS_HW6_RTOS_WAKE_CAUSE_START;
  }
  if ((source_mask & PS_HW6_RTOS_WAKE_SOURCE_PMIC) != 0UL)
  {
    return PS_HW6_RTOS_WAKE_CAUSE_PMIC;
  }
  if ((source_mask & PS_HW6_RTOS_WAKE_SOURCE_BUTTON) != 0UL)
  {
    return PS_HW6_RTOS_WAKE_CAUSE_BUTTON;
  }
  if ((source_mask & PS_HW6_RTOS_WAKE_SOURCE_JOYSTICK) != 0UL)
  {
    return PS_HW6_RTOS_WAKE_CAUSE_JOYSTICK;
  }
  if ((source_mask & PS_HW6_RTOS_WAKE_SOURCE_SENSOR) != 0UL)
  {
    return PS_HW6_RTOS_WAKE_CAUSE_SENSOR;
  }
  if ((source_mask & PS_HW6_RTOS_WAKE_SOURCE_RTC) != 0UL)
  {
    return PS_HW6_RTOS_WAKE_CAUSE_RTC;
  }
  if ((source_mask & PS_HW6_RTOS_WAKE_SOURCE_USB) != 0UL)
  {
    return PS_HW6_RTOS_WAKE_CAUSE_USB;
  }
  if ((source_mask & PS_HW6_RTOS_WAKE_SOURCE_FAULT) != 0UL)
  {
    return PS_HW6_RTOS_WAKE_CAUSE_FAULT;
  }
  if ((source_mask & PS_HW6_RTOS_WAKE_SOURCE_UNKNOWN) != 0UL)
  {
    return PS_HW6_RTOS_WAKE_CAUSE_UNKNOWN;
  }
  return PS_HW6_RTOS_WAKE_CAUSE_NONE;
}

void PS_HW6_RTOS_Stop2WakeClassifyBegin(void)
{
  ps_stop2_wake_button_edges_before =
    g_ps_input_buttons_probe.isr_edge_count;
  ps_stop2_wake_joystick_edges_before = ps_joystick_int_irq_count;
  ps_stop2_wake_pmic_edges_before = ps_pmic_int_irq_count;
  ps_stop2_wake_gpioa_before_idr = GPIOA->IDR;
  ps_stop2_wake_gpiob_before_idr = GPIOB->IDR;
  ps_stop2_wake_gpioc_before_idr = GPIOC->IDR;
  ps_stop2_wake_dbgmcu_cr_before = DBGMCU->CR;
  ps_stop2_wake_scb_icsr_before = SCB->ICSR;
  ps_stop2_wake_scb_scr_before = SCB->SCR;
  ps_stop2_wake_scb_shcsr_before = SCB->SHCSR;
  ps_stop2_wake_pwr_sr_before = PWR->SR;
  ps_stop2_wake_pwr_wusr_before = PWR->WUSR;
  ps_stop2_wake_nvic_ispr0_before = NVIC->ISPR[0U];
  ps_stop2_wake_nvic_ispr1_before = NVIC->ISPR[1U];
  ps_stop2_wake_nvic_ispr2_before = NVIC->ISPR[2U];
  ps_stop2_wake_nvic_ispr3_before = NVIC->ISPR[3U];

  g_ps_hw6_rtos_probe.stop2_wake_source_mask = 0UL;
  g_ps_hw6_rtos_probe.stop2_wake_primary_cause =
    PS_HW6_RTOS_WAKE_CAUSE_NONE;
  g_ps_hw6_rtos_probe.stop2_wake_exti_rising = 0UL;
  g_ps_hw6_rtos_probe.stop2_wake_exti_falling = 0UL;
  g_ps_hw6_rtos_probe.stop2_wake_exti_imr = EXTI->IMR1;
  g_ps_hw6_rtos_probe.stop2_wake_gpioa_before_idr =
    ps_stop2_wake_gpioa_before_idr;
  g_ps_hw6_rtos_probe.stop2_wake_gpiob_before_idr =
    ps_stop2_wake_gpiob_before_idr;
  g_ps_hw6_rtos_probe.stop2_wake_gpioc_before_idr =
    ps_stop2_wake_gpioc_before_idr;
  g_ps_hw6_rtos_probe.stop2_wake_gpioa_after_idr = 0UL;
  g_ps_hw6_rtos_probe.stop2_wake_gpiob_after_idr = 0UL;
  g_ps_hw6_rtos_probe.stop2_wake_gpioc_after_idr = 0UL;
  g_ps_hw6_rtos_probe.stop2_wake_button_edges_before =
    ps_stop2_wake_button_edges_before;
  g_ps_hw6_rtos_probe.stop2_wake_button_edges_after =
    ps_stop2_wake_button_edges_before;
  g_ps_hw6_rtos_probe.stop2_wake_pmic_edges_before =
    ps_stop2_wake_pmic_edges_before;
  g_ps_hw6_rtos_probe.stop2_wake_pmic_edges_after =
    ps_stop2_wake_pmic_edges_before;
  g_ps_hw6_rtos_probe.stop2_wake_dbgmcu_cr_before =
    ps_stop2_wake_dbgmcu_cr_before;
  g_ps_hw6_rtos_probe.stop2_wake_dbgmcu_cr_after = 0UL;
  g_ps_hw6_rtos_probe.stop2_wake_scb_icsr_before =
    ps_stop2_wake_scb_icsr_before;
  g_ps_hw6_rtos_probe.stop2_wake_scb_icsr_after = 0UL;
  g_ps_hw6_rtos_probe.stop2_wake_scb_scr_before =
    ps_stop2_wake_scb_scr_before;
  g_ps_hw6_rtos_probe.stop2_wake_scb_scr_after = 0UL;
  g_ps_hw6_rtos_probe.stop2_wake_scb_shcsr_before =
    ps_stop2_wake_scb_shcsr_before;
  g_ps_hw6_rtos_probe.stop2_wake_scb_shcsr_after = 0UL;
  g_ps_hw6_rtos_probe.stop2_wake_pwr_sr_before =
    ps_stop2_wake_pwr_sr_before;
  g_ps_hw6_rtos_probe.stop2_wake_pwr_sr_after = 0UL;
  g_ps_hw6_rtos_probe.stop2_wake_pwr_wusr_before =
    ps_stop2_wake_pwr_wusr_before;
  g_ps_hw6_rtos_probe.stop2_wake_pwr_wusr_after = 0UL;
  g_ps_hw6_rtos_probe.stop2_wake_pwr_wucr1 = PWR->WUCR1;
  g_ps_hw6_rtos_probe.stop2_wake_pwr_wucr2 = PWR->WUCR2;
  g_ps_hw6_rtos_probe.stop2_wake_pwr_wucr3 = PWR->WUCR3;
  g_ps_hw6_rtos_probe.stop2_wake_nvic_ispr0_before =
    ps_stop2_wake_nvic_ispr0_before;
  g_ps_hw6_rtos_probe.stop2_wake_nvic_ispr1_before =
    ps_stop2_wake_nvic_ispr1_before;
  g_ps_hw6_rtos_probe.stop2_wake_nvic_ispr2_before =
    ps_stop2_wake_nvic_ispr2_before;
  g_ps_hw6_rtos_probe.stop2_wake_nvic_ispr3_before =
    ps_stop2_wake_nvic_ispr3_before;
  g_ps_hw6_rtos_probe.stop2_wake_nvic_ispr0_after = 0UL;
  g_ps_hw6_rtos_probe.stop2_wake_nvic_ispr1_after = 0UL;
  g_ps_hw6_rtos_probe.stop2_wake_nvic_ispr2_after = 0UL;
  g_ps_hw6_rtos_probe.stop2_wake_nvic_ispr3_after = 0UL;
  g_ps_hw6_rtos_probe.stop2_wake_nvic_iabr0_after = 0UL;
  g_ps_hw6_rtos_probe.stop2_wake_nvic_iabr1_after = 0UL;
  g_ps_hw6_rtos_probe.stop2_wake_nvic_iabr2_after = 0UL;
  g_ps_hw6_rtos_probe.stop2_wake_nvic_iabr3_after = 0UL;
  g_ps_hw6_rtos_probe.stop2_wake_nvic_iser0_after = 0UL;
  g_ps_hw6_rtos_probe.stop2_wake_nvic_iser1_after = 0UL;
  g_ps_hw6_rtos_probe.stop2_wake_nvic_iser2_after = 0UL;
  g_ps_hw6_rtos_probe.stop2_wake_nvic_iser3_after = 0UL;
}

void PS_HW6_RTOS_Stop2WakeClassifyAfterWake(void)
{
  uint32_t button_edges_after;
  uint32_t exti_falling;
  uint32_t exti_mask;
  uint32_t exti_rising;
  uint32_t gpioa_after;
  uint32_t gpiob_after;
  uint32_t gpioc_after;
  uint32_t joystick_edges_after;
  uint32_t nvic_iabr0_after;
  uint32_t nvic_iabr1_after;
  uint32_t nvic_iabr2_after;
  uint32_t nvic_iabr3_after;
  uint32_t nvic_iser0_after;
  uint32_t nvic_iser1_after;
  uint32_t nvic_iser2_after;
  uint32_t nvic_iser3_after;
  uint32_t nvic_ispr0_after;
  uint32_t nvic_ispr1_after;
  uint32_t nvic_ispr2_after;
  uint32_t nvic_ispr3_after;
  uint32_t pmic_edges_after;
  uint32_t source_mask = 0UL;

  exti_rising = EXTI->RPR1;
  exti_falling = EXTI->FPR1;
  exti_mask = exti_rising | exti_falling;
  gpioa_after = GPIOA->IDR;
  gpiob_after = GPIOB->IDR;
  gpioc_after = GPIOC->IDR;
  button_edges_after = g_ps_input_buttons_probe.isr_edge_count;
  joystick_edges_after = ps_joystick_int_irq_count;
  pmic_edges_after = ps_pmic_int_irq_count;
  nvic_ispr0_after = NVIC->ISPR[0U];
  nvic_ispr1_after = NVIC->ISPR[1U];
  nvic_ispr2_after = NVIC->ISPR[2U];
  nvic_ispr3_after = NVIC->ISPR[3U];
  nvic_iabr0_after = NVIC->IABR[0U];
  nvic_iabr1_after = NVIC->IABR[1U];
  nvic_iabr2_after = NVIC->IABR[2U];
  nvic_iabr3_after = NVIC->IABR[3U];
  nvic_iser0_after = NVIC->ISER[0U];
  nvic_iser1_after = NVIC->ISER[1U];
  nvic_iser2_after = NVIC->ISER[2U];
  nvic_iser3_after = NVIC->ISER[3U];

  if (((exti_mask & BTN_START_Pin) != 0UL) ||
      ((button_edges_after != ps_stop2_wake_button_edges_before) &&
       (g_ps_input_buttons_probe.last_button_id ==
        (uint32_t)PS_INPUT_BUTTON_ID_START)) ||
      (((ps_stop2_wake_gpioa_before_idr & BTN_START_Pin) != 0UL) &&
       ((gpioa_after & BTN_START_Pin) == 0UL)) ||
      ((gpioa_after & BTN_START_Pin) == 0UL))
  {
    source_mask |= PS_HW6_RTOS_WAKE_SOURCE_START;
  }

  if (((exti_mask & PS_HW6_RTOS_WAKE_BUTTON_PIN_MASK) != 0UL) ||
      ((button_edges_after != ps_stop2_wake_button_edges_before) &&
       (g_ps_input_buttons_probe.last_button_id >=
        (uint32_t)PS_INPUT_BUTTON_ID_A) &&
       (g_ps_input_buttons_probe.last_button_id <=
        (uint32_t)PS_INPUT_BUTTON_ID_R)) ||
      ((((ps_stop2_wake_gpiob_before_idr ^ gpiob_after) &
         PS_HW6_RTOS_WAKE_BUTTON_PIN_MASK) != 0UL) &&
       ((gpiob_after & PS_HW6_RTOS_WAKE_BUTTON_PIN_MASK) !=
        PS_HW6_RTOS_WAKE_BUTTON_PIN_MASK)))
  {
    source_mask |= PS_HW6_RTOS_WAKE_SOURCE_BUTTON;
  }

  if (((exti_mask & JOY_INT_Pin) != 0UL) ||
      (joystick_edges_after != ps_stop2_wake_joystick_edges_before) ||
      (((ps_stop2_wake_gpioc_before_idr ^ gpioc_after) &
        JOY_INT_Pin) != 0UL))
  {
    source_mask |= PS_HW6_RTOS_WAKE_SOURCE_JOYSTICK;
  }

  if (((exti_mask & MPU_INT_Pin) != 0UL) ||
      (((ps_stop2_wake_gpiob_before_idr ^ gpiob_after) &
        MPU_INT_Pin) != 0UL))
  {
    source_mask |= PS_HW6_RTOS_WAKE_SOURCE_SENSOR;
  }

  if (((exti_mask & PMIC_INT_Pin) != 0UL) ||
      (pmic_edges_after != ps_stop2_wake_pmic_edges_before) ||
      (((ps_stop2_wake_gpiob_before_idr & PMIC_INT_Pin) != 0UL) &&
       ((gpiob_after & PMIC_INT_Pin) == 0UL)))
  {
    source_mask |= PS_HW6_RTOS_WAKE_SOURCE_PMIC;
  }

  if (ps_runtime_interaction_rtc_irq_expired != 0UL)
  {
    source_mask |= PS_HW6_RTOS_WAKE_SOURCE_RTC;
  }

  if ((source_mask == 0UL) ||
      ((exti_mask & ~PS_HW6_RTOS_WAKE_KNOWN_EXTI_MASK) != 0UL))
  {
    source_mask |= PS_HW6_RTOS_WAKE_SOURCE_UNKNOWN;
    g_ps_hw6_rtos_probe.stop2_wake_unknown_count++;
  }

  if ((source_mask & PS_HW6_RTOS_WAKE_SOURCE_START) != 0UL)
  {
    g_ps_hw6_rtos_probe.stop2_wake_start_count++;
  }
  if ((source_mask & PS_HW6_RTOS_WAKE_SOURCE_BUTTON) != 0UL)
  {
    g_ps_hw6_rtos_probe.stop2_wake_button_count++;
  }
  if ((source_mask & PS_HW6_RTOS_WAKE_SOURCE_JOYSTICK) != 0UL)
  {
    g_ps_hw6_rtos_probe.stop2_wake_joystick_count++;
    ps_joystick_awake_publish_reset_pending = 1UL;
    g_ps_hw6_rtos_probe.joystick_logical_stop2_reset_request_count++;
    g_ps_hw6_rtos_probe.joystick_logical_stop2_reset_pending = 1UL;
  }
  if ((source_mask & PS_HW6_RTOS_WAKE_SOURCE_SENSOR) != 0UL)
  {
    g_ps_hw6_rtos_probe.stop2_wake_sensor_count++;
  }
  if ((source_mask & PS_HW6_RTOS_WAKE_SOURCE_PMIC) != 0UL)
  {
    g_ps_hw6_rtos_probe.stop2_wake_pmic_count++;
  }
  if ((source_mask & PS_HW6_RTOS_WAKE_SOURCE_RTC) != 0UL)
  {
    g_ps_hw6_rtos_probe.stop2_wake_rtc_count++;
  }
  if ((source_mask & PS_HW6_RTOS_WAKE_SOURCE_USB) != 0UL)
  {
    g_ps_hw6_rtos_probe.stop2_wake_usb_count++;
  }
  if ((source_mask & PS_HW6_RTOS_WAKE_SOURCE_FAULT) != 0UL)
  {
    g_ps_hw6_rtos_probe.stop2_wake_fault_count++;
  }

  g_ps_hw6_rtos_probe.stop2_wake_classify_count++;
  g_ps_hw6_rtos_probe.stop2_wake_classify_tick =
    (uint32_t)tx_time_get();
  g_ps_hw6_rtos_probe.stop2_wake_source_mask = source_mask;
  g_ps_hw6_rtos_probe.stop2_wake_primary_cause =
    PS_HW6_RTOS_Stop2WakePrimary(source_mask);
  g_ps_hw6_rtos_probe.stop2_wake_exti_rising = exti_rising;
  g_ps_hw6_rtos_probe.stop2_wake_exti_falling = exti_falling;
  g_ps_hw6_rtos_probe.stop2_wake_exti_imr = EXTI->IMR1;
  g_ps_hw6_rtos_probe.stop2_wake_gpioa_before_idr =
    ps_stop2_wake_gpioa_before_idr;
  g_ps_hw6_rtos_probe.stop2_wake_gpiob_before_idr =
    ps_stop2_wake_gpiob_before_idr;
  g_ps_hw6_rtos_probe.stop2_wake_gpioc_before_idr =
    ps_stop2_wake_gpioc_before_idr;
  g_ps_hw6_rtos_probe.stop2_wake_gpioa_after_idr = gpioa_after;
  g_ps_hw6_rtos_probe.stop2_wake_gpiob_after_idr = gpiob_after;
  g_ps_hw6_rtos_probe.stop2_wake_gpioc_after_idr = gpioc_after;
  g_ps_hw6_rtos_probe.stop2_wake_button_edges_before =
    ps_stop2_wake_button_edges_before;
  g_ps_hw6_rtos_probe.stop2_wake_button_edges_after =
    button_edges_after;
  g_ps_hw6_rtos_probe.stop2_wake_pmic_edges_before =
    ps_stop2_wake_pmic_edges_before;
  g_ps_hw6_rtos_probe.stop2_wake_pmic_edges_after =
    pmic_edges_after;
  g_ps_hw6_rtos_probe.stop2_wake_dbgmcu_cr_before =
    ps_stop2_wake_dbgmcu_cr_before;
  g_ps_hw6_rtos_probe.stop2_wake_dbgmcu_cr_after = DBGMCU->CR;
  g_ps_hw6_rtos_probe.stop2_wake_scb_icsr_before =
    ps_stop2_wake_scb_icsr_before;
  g_ps_hw6_rtos_probe.stop2_wake_scb_icsr_after = SCB->ICSR;
  g_ps_hw6_rtos_probe.stop2_wake_scb_scr_before =
    ps_stop2_wake_scb_scr_before;
  g_ps_hw6_rtos_probe.stop2_wake_scb_scr_after = SCB->SCR;
  g_ps_hw6_rtos_probe.stop2_wake_scb_shcsr_before =
    ps_stop2_wake_scb_shcsr_before;
  g_ps_hw6_rtos_probe.stop2_wake_scb_shcsr_after = SCB->SHCSR;
  g_ps_hw6_rtos_probe.stop2_wake_pwr_sr_before =
    ps_stop2_wake_pwr_sr_before;
  g_ps_hw6_rtos_probe.stop2_wake_pwr_sr_after = PWR->SR;
  g_ps_hw6_rtos_probe.stop2_wake_pwr_wusr_before =
    ps_stop2_wake_pwr_wusr_before;
  g_ps_hw6_rtos_probe.stop2_wake_pwr_wusr_after = PWR->WUSR;
  g_ps_hw6_rtos_probe.stop2_wake_pwr_wucr1 = PWR->WUCR1;
  g_ps_hw6_rtos_probe.stop2_wake_pwr_wucr2 = PWR->WUCR2;
  g_ps_hw6_rtos_probe.stop2_wake_pwr_wucr3 = PWR->WUCR3;
  g_ps_hw6_rtos_probe.stop2_wake_nvic_ispr0_before =
    ps_stop2_wake_nvic_ispr0_before;
  g_ps_hw6_rtos_probe.stop2_wake_nvic_ispr1_before =
    ps_stop2_wake_nvic_ispr1_before;
  g_ps_hw6_rtos_probe.stop2_wake_nvic_ispr2_before =
    ps_stop2_wake_nvic_ispr2_before;
  g_ps_hw6_rtos_probe.stop2_wake_nvic_ispr3_before =
    ps_stop2_wake_nvic_ispr3_before;
  g_ps_hw6_rtos_probe.stop2_wake_nvic_ispr0_after =
    nvic_ispr0_after;
  g_ps_hw6_rtos_probe.stop2_wake_nvic_ispr1_after =
    nvic_ispr1_after;
  g_ps_hw6_rtos_probe.stop2_wake_nvic_ispr2_after =
    nvic_ispr2_after;
  g_ps_hw6_rtos_probe.stop2_wake_nvic_ispr3_after =
    nvic_ispr3_after;
  g_ps_hw6_rtos_probe.stop2_wake_nvic_iabr0_after =
    nvic_iabr0_after;
  g_ps_hw6_rtos_probe.stop2_wake_nvic_iabr1_after =
    nvic_iabr1_after;
  g_ps_hw6_rtos_probe.stop2_wake_nvic_iabr2_after =
    nvic_iabr2_after;
  g_ps_hw6_rtos_probe.stop2_wake_nvic_iabr3_after =
    nvic_iabr3_after;
  g_ps_hw6_rtos_probe.stop2_wake_nvic_iser0_after =
    nvic_iser0_after;
  g_ps_hw6_rtos_probe.stop2_wake_nvic_iser1_after =
    nvic_iser1_after;
  g_ps_hw6_rtos_probe.stop2_wake_nvic_iser2_after =
    nvic_iser2_after;
  g_ps_hw6_rtos_probe.stop2_wake_nvic_iser3_after =
    nvic_iser3_after;
}

static UINT PS_HW6_RTOS_SnapshotPool(TX_BYTE_POOL *pool,
                                      uint32_t *available,
                                      uint32_t *fragments)
{
  ULONG available_bytes = 0UL;
  ULONG fragment_count = 0UL;
  UINT status;

  status = tx_byte_pool_info_get(pool, TX_NULL, &available_bytes,
                                 &fragment_count, TX_NULL, TX_NULL, TX_NULL);
  *available = (uint32_t)available_bytes;
  *fragments = (uint32_t)fragment_count;
  return status;
}

static uint32_t PS_HW6_RTOS_MsToTicks(uint32_t ms)
{
  uint64_t scaled;

  scaled = (((uint64_t)ms * (uint64_t)TX_TIMER_TICKS_PER_SECOND) + 999ULL) /
    1000ULL;
  if (scaled == 0ULL)
  {
    return 1UL;
  }
  if (scaled > 0xffffffffULL)
  {
    return 0xffffffffUL;
  }
  return (uint32_t)scaled;
}

static uint32_t PS_HW6_RTOS_TimeReached(uint32_t now_tick,
                                        uint32_t deadline_tick)
{
  return (((int32_t)(now_tick - deadline_tick)) >= 0) ? 1UL : 0UL;
}

static void PS_HW6_RTOS_ResetProbe(void)
{
  uint32_t i;

  (void)memset((void *)&g_ps_hw6_rtos_probe, 0,
               sizeof(g_ps_hw6_rtos_probe));
  g_ps_hw6_rtos_low_power_usb_skip_count = 0UL;
  g_ps_hw6_power_stop2_eligibility_request = 0UL;
  g_ps_hw6_power_stop2_controlled_entry_request = 0UL;
  g_ps_hw6_rtos_probe.boot_home_suppressed = 0UL;
  g_ps_hw6_rtos_probe.boot_low_battery_ui_sent = 0UL;
  g_ps_hw6_rtos_probe.boot_low_battery_recover_ui_sent = 0UL;
  ps_ui_boot_complete_sent = 0UL;
  ps_boot_package_launch_after_calibration = 0UL;
  ps_power_boot_done = 0UL;
  ps_power_boot_idle_peripheral_park_done = 0UL;
  ps_display_bootstrap_sent = 0UL;
  ps_pmic_int_pending_count = 0UL;
  ps_pmic_int_irq_count = 0UL;
  ps_pmic_int_last_pin = 0UL;
  ps_pmic_int_last_level = 0UL;
  ps_pmic_int_last_irq_tick = 0UL;
  ps_pmic_int_consumed_count = 0UL;
  ps_stop2_wake_button_edges_before = 0UL;
  ps_stop2_wake_pmic_edges_before = 0UL;
  ps_stop2_wake_gpioa_before_idr = 0UL;
  ps_stop2_wake_gpiob_before_idr = 0UL;
  ps_stop2_wake_gpioc_before_idr = 0UL;
  ps_stop2_wake_dbgmcu_cr_before = 0UL;
  ps_stop2_wake_scb_icsr_before = 0UL;
  ps_stop2_wake_scb_scr_before = 0UL;
  ps_stop2_wake_scb_shcsr_before = 0UL;
  ps_stop2_wake_pwr_sr_before = 0UL;
  ps_stop2_wake_pwr_wusr_before = 0UL;
  ps_stop2_wake_nvic_ispr0_before = 0UL;
  ps_stop2_wake_nvic_ispr1_before = 0UL;
  ps_stop2_wake_nvic_ispr2_before = 0UL;
  ps_stop2_wake_nvic_ispr3_before = 0UL;
  g_ps_hw6_audio_clock_probe_request = 0UL;
  g_ps_hw6_audio_clock_probe_release_request = 0UL;
  g_ps_hw6_runtime_reactive_stub_request = 0UL;
  g_ps_hw6_runtime_interaction_test_continuous = 0UL;
  g_ps_hw6_runtime_realtime_stub_request = 0UL;
  g_ps_hw6_runtime_return_request = 0UL;
  g_ps_hw6_runtime_suspend_request = 0UL;
  g_ps_hw6_runtime_resume_request = 0UL;
  g_ps_hw6_admission_msc_enter_dry_run_request = 0UL;
  g_ps_hw6_admission_power_shutdown_dry_run_request = 0UL;
  g_ps_hw6_admission_power_battery_dry_run_request = 0UL;
  g_ps_hw6_admission_power_cancel_dry_run_request = 0UL;
  g_ps_hw6_power_stop2_auto_idle_dry_run_request = 0UL;
  g_ps_hw6_power_stop2_auto_idle_entry_request = 0UL;
  g_ps_hw6_power_stop2_lpbam_prepare_request = 0UL;
  g_ps_hw6_power_stop2_lpbam_abort_request = 0UL;
  g_ps_hw6_power_stop2_lpbam_abort_late_test_request = 0UL;
  g_ps_hw6_power_stop2_display_backend_override = 0UL;
  g_ps_hw6_power_stop2_lpbam_awake_hold_enable = 0UL;
  ps_stop2_lpbam_abort_late_test_active = 0UL;
  ps_stop2_lpbam_late_blocker_armed = 0UL;
  ps_display_blink_next_tick = 0UL;
  ps_joystick_awake_poll_next_tick = 0UL;
  ps_joystick_calibration_boot_load_started = 0UL;
  ps_joystick_calibration_apply_pending = 0UL;
  ps_joystick_calibration_save_completion_pending = 0UL;
  ps_joystick_awake_published_direction_mask = 0UL;
  ps_joystick_awake_published_candidate_mask = 0UL;
  ps_joystick_logical_active_direction_mask = 0UL;
  ps_joystick_logical_press_tick = 0UL;
  ps_joystick_logical_next_repeat_tick = 0UL;
  ps_joystick_logical_hold_sent = 0UL;
  ps_joystick_awake_publish_reset_pending = 0UL;
  g_ps_hw6_rtos_probe.joystick_logical_wake_capture_status =
    PS_HW6_RTOS_STATUS_NOT_RUN;
  ps_input_activity_generation = 0UL;
  ps_runtime_interaction_state = PS_HW6_RUNTIME_INTERACTION_STATE_NONE;
  ps_runtime_interaction_cue_active = 0UL;
  ps_runtime_interaction_activation_active = 0UL;
  ps_runtime_interaction_mode = 0UL;
  ps_runtime_interaction_deadline_tick = 0UL;
  ps_runtime_interaction_cue_deadline_tick = 0UL;
  ps_runtime_interaction_activation_frame = 0UL;
  ps_runtime_interaction_activation_deadline_tick = 0UL;
  ps_runtime_interaction_epoch = 0UL;
  ps_runtime_interaction_timeout_forced = 0UL;
  ps_runtime_interaction_timeout_forced_epoch = 0UL;
  ps_runtime_interaction_rtc_armed = 0UL;
  ps_runtime_interaction_rtc_armed_epoch = 0UL;
  ps_runtime_interaction_rtc_start_units = 0UL;
  ps_runtime_interaction_rtc_remaining_ticks = 0UL;
  ps_runtime_interaction_rtc_irq_expired = 0UL;
  ps_runtime_interaction_rtc_command_queued = 0UL;
  ps_power_input_activity_generation = 0UL;
  ps_display_blink_visible = 1UL;
  ps_display_waiting_sequence_frame = 0UL;
  ps_display_waiting_sequence_count = 0UL;
  ps_audio_sfx_pending = 0UL;
  ps_display_blink_stop2_suppressed = 0UL;
  ps_stop2_lpbam_edge_request_pending = 0UL;
  ps_stop2_lpbam_edge_rearm_needed = 0UL;
  ps_stop2_lpbam_edge_target_tick = 0UL;
  ps_stop2_lpbam_edge_start_phase = 0UL;
  ps_stop2_lpbam_edge_render_count = 0UL;
  ps_stop2_lpbam_edge_page = 0UL;
  g_ps_hw6_rtos_probe.magic = PS_HW6_RTOS_PROBE_MAGIC;
  g_ps_hw6_rtos_probe.version = PS_HW6_RTOS_PROBE_VERSION;
  g_ps_hw6_rtos_probe.phase = PS_HW6_RTOS_PHASE_INIT;
  g_ps_hw6_rtos_probe.input_raw_last_send_status =
    PS_HW6_RTOS_STATUS_NOT_RUN;
  g_ps_hw6_rtos_probe.stop2_final_input_last_status =
    PS_HW6_RTOS_STATUS_NOT_RUN;
  g_ps_hw6_rtos_probe.stop2_lpbam_edge_rearm_pending = 0UL;
  g_ps_hw6_rtos_probe.stop2_lpbam_edge_rearm_count = 0UL;
  g_ps_hw6_rtos_probe.stop2_lpbam_edge_rearm_tick = 0UL;
  g_ps_hw6_rtos_probe.display_waiting_preserve_status =
    PS_HW6_RTOS_STATUS_NOT_RUN;
  g_ps_hw6_rtos_probe.input_policy_api_version =
    PS_HW6_RTOS_INPUT_POLICY_API_VERSION;
  g_ps_hw6_rtos_probe.input_policy_last_status =
    PS_HW6_RTOS_STATUS_NOT_RUN;
  g_ps_hw6_rtos_probe.joystick_awake_poll_last_status =
    PS_HW6_RTOS_STATUS_NOT_RUN;
  g_ps_hw6_rtos_probe.joystick_logical_last_status =
    PS_HW6_RTOS_STATUS_NOT_RUN;
  g_ps_hw6_rtos_probe.runtime_input_last_status =
    PS_HW6_RTOS_STATUS_NOT_RUN;
  g_ps_hw6_rtos_probe.runtime_interaction_start_status =
    PS_HW6_RTOS_STATUS_NOT_RUN;
  g_ps_hw6_rtos_probe.runtime_interaction_manual_request_status =
    PS_HW6_RTOS_STATUS_NOT_RUN;
  g_ps_hw6_rtos_probe.runtime_interaction_package_start_status =
    PS_HW6_RTOS_STATUS_NOT_RUN;
  g_ps_hw6_rtos_probe.runtime_interaction_activation_render_status =
    PS_HW6_RTOS_STATUS_NOT_RUN;
  g_ps_hw6_rtos_probe.runtime_interaction_joystick_wake_allowed = 1UL;
  g_ps_hw6_rtos_probe.runtime_interaction_rtc_arm_status =
    PS_HW6_RTOS_STATUS_NOT_RUN;
  g_ps_hw6_rtos_probe.runtime_interaction_rtc_command_status =
    PS_HW6_RTOS_STATUS_NOT_RUN;
  g_ps_hw6_rtos_probe.admission_api_version =
    PS_HW6_RTOS_ADMISSION_API_VERSION;
  g_ps_hw6_rtos_probe.admission_last_status =
    PS_HW6_RTOS_STATUS_NOT_RUN;
  g_ps_hw6_rtos_probe.admission_runtime_resume_status =
    PS_HW6_RTOS_STATUS_NOT_RUN;
  g_ps_hw6_rtos_probe.init_status = TX_SUCCESS;
  g_ps_hw6_rtos_probe.init_error_step = PS_HW6_RTOS_STATUS_NOT_RUN;
  g_ps_hw6_rtos_probe.init_error_index = PS_HW6_RTOS_STATUS_NOT_RUN;
  g_ps_hw6_rtos_probe.boot_idle_peripheral_park_last_status =
    PS_HW6_RTOS_STATUS_NOT_RUN;
  g_ps_hw6_rtos_probe.boot_idle_peripheral_park_ble_send_status =
    PS_HW6_RTOS_STATUS_NOT_RUN;
  g_ps_hw6_rtos_probe.boot_idle_peripheral_park_ble_wait_status =
    PS_HW6_RTOS_STATUS_NOT_RUN;
  g_ps_hw6_rtos_probe.boot_idle_peripheral_park_imu_send_status =
    PS_HW6_RTOS_STATUS_NOT_RUN;
  g_ps_hw6_rtos_probe.boot_idle_peripheral_park_imu_wait_status =
    PS_HW6_RTOS_STATUS_NOT_RUN;
  g_ps_hw6_rtos_probe.audio_clock_last_status =
    PS_HW6_RTOS_STATUS_NOT_RUN;
  g_ps_hw6_rtos_probe.audio_clock_reactive_sfx_status =
    PS_HW6_RTOS_STATUS_NOT_RUN;
  g_ps_hw6_rtos_probe.audio_clock_realtime_status =
    PS_HW6_RTOS_STATUS_NOT_RUN;
  g_ps_hw6_rtos_probe.audio_clock_release_status =
    PS_HW6_RTOS_STATUS_NOT_RUN;
  g_ps_hw6_rtos_probe.audio_sfx_send_status =
    PS_HW6_RTOS_STATUS_NOT_RUN;
  g_ps_hw6_rtos_probe.audio_sfx_owner_status =
    PS_HW6_RTOS_STATUS_NOT_RUN;
  g_ps_hw6_rtos_probe.audio_sfx_clock_request_status =
    PS_HW6_RTOS_STATUS_NOT_RUN;
  g_ps_hw6_rtos_probe.audio_sfx_clock_release_status =
    PS_HW6_RTOS_STATUS_NOT_RUN;
  g_ps_hw6_rtos_probe.audio_sfx_grant_policy_status =
    PS_HW6_RTOS_STATUS_NOT_RUN;
  g_ps_hw6_rtos_probe.audio_sfx_grant_policy_stage =
    PS_HW6_RTOS_STATUS_NOT_RUN;
  g_ps_hw6_rtos_probe.audio_sfx_last_cue_index =
    PS_HW6_RTOS_STATUS_NOT_RUN;
  g_ps_hw6_rtos_probe.storage_clock_last_status =
    PS_HW6_RTOS_STATUS_NOT_RUN;
  g_ps_hw6_rtos_probe.storage_clock_export_status =
    PS_HW6_RTOS_STATUS_NOT_RUN;
  g_ps_hw6_rtos_probe.storage_clock_reclaim_status =
    PS_HW6_RTOS_STATUS_NOT_RUN;
  g_ps_hw6_rtos_probe.storage_clock_flash_init_status =
    PS_HW6_RTOS_STATUS_NOT_RUN;
  g_ps_hw6_rtos_probe.storage_clock_attach_status =
    PS_HW6_RTOS_STATUS_NOT_RUN;
  g_ps_hw6_rtos_probe.storage_clock_post_stop_resume_status =
    PS_HW6_RTOS_STATUS_NOT_RUN;
  g_ps_hw6_rtos_probe.storage_clock_release_status =
    PS_HW6_RTOS_STATUS_NOT_RUN;
  g_ps_hw6_rtos_probe.runtime_last_status = PS_HW6_RTOS_STATUS_NOT_RUN;
  g_ps_hw6_rtos_probe.runtime_owner_request_status =
    PS_HW6_RTOS_STATUS_NOT_RUN;
  g_ps_hw6_rtos_probe.runtime_admission_last_status =
    PS_HW6_RTOS_STATUS_NOT_RUN;
  g_ps_hw6_rtos_probe.runtime_admission_reactive_status =
    PS_HW6_RTOS_STATUS_NOT_RUN;
  g_ps_hw6_rtos_probe.runtime_admission_realtime_status =
    PS_HW6_RTOS_STATUS_NOT_RUN;
  g_ps_hw6_rtos_probe.runtime_suspend_clock_release_status =
    PS_HW6_RTOS_STATUS_NOT_RUN;
  g_ps_hw6_rtos_probe.runtime_resume_clock_request_status =
    PS_HW6_RTOS_STATUS_NOT_RUN;
  g_ps_hw6_rtos_probe.runtime_return_clock_release_status =
    PS_HW6_RTOS_STATUS_NOT_RUN;
  g_ps_hw6_rtos_probe.runtime_clock_last_status =
    PS_HW6_RTOS_STATUS_NOT_RUN;
  g_ps_hw6_rtos_probe.runtime_clock_reactive_status =
    PS_HW6_RTOS_STATUS_NOT_RUN;
  g_ps_hw6_rtos_probe.runtime_clock_realtime_status =
    PS_HW6_RTOS_STATUS_NOT_RUN;
  g_ps_hw6_rtos_probe.runtime_clock_release_status =
    PS_HW6_RTOS_STATUS_NOT_RUN;
  g_ps_hw6_rtos_probe.ui_clock_last_status =
    PS_HW6_RTOS_STATUS_NOT_RUN;
  g_ps_hw6_rtos_probe.ui_clock_reactive_status =
    PS_HW6_RTOS_STATUS_NOT_RUN;
  g_ps_hw6_rtos_probe.ui_clock_release_status =
    PS_HW6_RTOS_STATUS_NOT_RUN;
  g_ps_hw6_rtos_probe.display_clock_last_status =
    PS_HW6_RTOS_STATUS_NOT_RUN;
  g_ps_hw6_rtos_probe.display_clock_transfer_status =
    PS_HW6_RTOS_STATUS_NOT_RUN;
  g_ps_hw6_rtos_probe.display_clock_release_status =
    PS_HW6_RTOS_STATUS_NOT_RUN;
  g_ps_hw6_rtos_probe.ui_action_send_status = PS_HW6_RTOS_STATUS_NOT_RUN;
  g_ps_hw6_rtos_probe.stop2_eligibility_last_status =
    PS_HW6_RTOS_STATUS_NOT_RUN;
  g_ps_hw6_rtos_probe.stop2_control_last_status =
    PS_HW6_RTOS_STATUS_NOT_RUN;
  g_ps_hw6_rtos_probe.stop2_control_eligibility_status =
    PS_HW6_RTOS_STATUS_NOT_RUN;
  g_ps_hw6_rtos_probe.stop2_control_entry_status =
    PS_HW6_RTOS_STATUS_NOT_RUN;
  g_ps_hw6_rtos_probe.stop2_auto_last_status =
    PS_HW6_RTOS_STATUS_NOT_RUN;
  g_ps_hw6_rtos_probe.stop2_auto_eligibility_status =
    PS_HW6_RTOS_STATUS_NOT_RUN;
  g_ps_hw6_rtos_probe.stop2_auto_entry_status =
    PS_HW6_RTOS_STATUS_NOT_RUN;
  g_ps_hw6_rtos_probe.stop2_auto_debug_force_enable =
    0UL;
  g_ps_hw6_rtos_probe.stop2_blink_handoff_send_status =
    PS_HW6_RTOS_STATUS_NOT_RUN;
  g_ps_hw6_rtos_probe.stop2_blink_handoff_wait_status =
    PS_HW6_RTOS_STATUS_NOT_RUN;
  g_ps_hw6_rtos_probe.stop2_blink_handoff_owner_status =
    PS_HW6_RTOS_STATUS_NOT_RUN;
  g_ps_hw6_rtos_probe.stop2_blink_handoff_status =
    PS_HW6_RTOS_STATUS_NOT_RUN;
  g_ps_hw6_rtos_probe.stop2_display_wait_backend_status =
    PS_HW6_RTOS_STATUS_NOT_RUN;
  g_ps_hw6_rtos_probe.stop2_lpbam_prepare_send_status =
    PS_HW6_RTOS_STATUS_NOT_RUN;
  g_ps_hw6_rtos_probe.stop2_lpbam_prepare_wait_status =
    PS_HW6_RTOS_STATUS_NOT_RUN;
  g_ps_hw6_rtos_probe.stop2_lpbam_prepare_owner_status =
    PS_HW6_RTOS_STATUS_NOT_RUN;
  g_ps_hw6_rtos_probe.stop2_lpbam_prepare_display_clear_count =
    PS_HW6_RTOS_STATUS_NOT_RUN;
  g_ps_hw6_rtos_probe.stop2_lpbam_edge_status =
    PS_HW6_RTOS_STATUS_NOT_RUN;
  g_ps_hw6_rtos_probe.stop2_lpbam_edge_render_status =
    PS_HW6_RTOS_STATUS_NOT_RUN;
  g_ps_hw6_rtos_probe.stop2_lpbam_edge_wake_send_status =
    PS_HW6_RTOS_STATUS_NOT_RUN;
  g_ps_hw6_rtos_probe.stop2_lpbam_power_recheck_send_status =
    PS_HW6_RTOS_STATUS_NOT_RUN;
  g_ps_hw6_rtos_probe.stop2_lpbam_abort_send_status =
    PS_HW6_RTOS_STATUS_NOT_RUN;
  g_ps_hw6_rtos_probe.stop2_lpbam_abort_wait_status =
    PS_HW6_RTOS_STATUS_NOT_RUN;
  g_ps_hw6_rtos_probe.stop2_lpbam_abort_owner_status =
    PS_HW6_RTOS_STATUS_NOT_RUN;
  g_ps_hw6_rtos_probe.stop2_lpbam_abort_resume_status =
    PS_HW6_RTOS_STATUS_NOT_RUN;
  g_ps_hw6_rtos_probe.stop2_lpbam_abort_late_test_status =
    PS_HW6_RTOS_STATUS_NOT_RUN;
  g_ps_hw6_rtos_probe.stop2_lpbam_awake_hold_enabled = 0UL;
  g_ps_hw6_rtos_probe.stop2_lpbam_awake_hold_active = 0UL;
  g_ps_hw6_rtos_probe.stop2_lpbam_awake_hold_count = 0UL;
  g_ps_hw6_rtos_probe.stop2_lpbam_awake_hold_start_tick = 0UL;
  g_ps_hw6_rtos_probe.stop2_auto_required_idle_ticks =
    PS_HW6_RTOS_MsToTicks((uint32_t)KNOB_POWER_AUTO_STOP2_IDLE_MS);
  g_ps_hw6_rtos_probe.ticks_per_second = TX_TIMER_TICKS_PER_SECOND;
  g_ps_hw6_rtos_probe.owner_count = PS_HW6_RTOS_OWNER_COUNT;
  g_ps_hw6_rtos_probe.queue_count = PS_HW6_RTOS_QUEUE_COUNT;
  g_ps_hw6_rtos_probe.event_group_count = PS_HW6_RTOS_EVENT_GROUP_COUNT;
  g_ps_hw6_rtos_probe.owner_required_mask = PS_HW6_RTOS_OWNER_MASK;
  g_ps_hw6_rtos_probe.queue_required_mask = PS_HW6_RTOS_OWNER_MASK;
  g_ps_hw6_rtos_probe.event_required_mask = PS_HW6_RTOS_EVENT_MASK;

  for (i = 0U; i < PS_HW6_RTOS_OWNER_COUNT; ++i)
  {
    g_ps_hw6_rtos_probe.stack_alloc_status[i] = PS_HW6_RTOS_STATUS_NOT_RUN;
    g_ps_hw6_rtos_probe.queue_alloc_status[i] = PS_HW6_RTOS_STATUS_NOT_RUN;
    g_ps_hw6_rtos_probe.queue_create_status[i] = PS_HW6_RTOS_STATUS_NOT_RUN;
    g_ps_hw6_rtos_probe.queue_selftest_send_status[i] = PS_HW6_RTOS_STATUS_NOT_RUN;
    g_ps_hw6_rtos_probe.thread_create_status[i] = PS_HW6_RTOS_STATUS_NOT_RUN;
    g_ps_hw6_rtos_probe.thread_stack_config_bytes[i] =
      (uint32_t)ps_owner_stack_bytes[i];
    g_ps_hw6_rtos_probe.thread_stack_start[i] = 0UL;
    g_ps_hw6_rtos_probe.thread_stack_end[i] = 0UL;
    g_ps_hw6_rtos_probe.thread_stack_size[i] = 0UL;
    g_ps_hw6_rtos_probe.thread_stack_ptr[i] = 0UL;
    g_ps_hw6_rtos_probe.thread_stack_highest_ptr[i] = 0UL;
    ps_thread_stacks[i] = TX_NULL;
    ps_queue_storage[i] = TX_NULL;
  }


  for (i = 0U; i < PS_HW6_RTOS_EVENT_GROUP_COUNT; ++i)
  {
    g_ps_hw6_rtos_probe.event_create_status[i] = PS_HW6_RTOS_STATUS_NOT_RUN;
    g_ps_hw6_rtos_probe.event_set_status[i] = PS_HW6_RTOS_STATUS_NOT_RUN;
    g_ps_hw6_rtos_probe.event_get_status[i] = PS_HW6_RTOS_STATUS_NOT_RUN;
  }
}

static uint32_t PS_HW6_RTOS_MessageIsValid(uint32_t owner_id,
                                           const ULONG *message)
{
  return ((message[0] == PS_HW6_RTOS_STARTUP_MAGIC) &&
          (message[1] == owner_id) &&
          (message[2] == PS_HW6_RTOS_STARTUP_KIND) &&
          (message[3] == (~((ULONG)owner_id)))) ? 1UL : 0UL;
}

void PS_HW6_RTOS_RecordJoystickExti(uint16_t gpio_pin, uint32_t level)
{
  ULONG message[PS_HW6_RTOS_MESSAGE_WORDS];
  UINT status = TX_NOT_AVAILABLE;
  uint32_t enqueue_required;

  if ((KNOB_INPUT_JOYSTICK_STOP2_WAKE_ENABLE == 0) ||
      (PS_HW6_OwnerStateMachines_JoystickStop2WakeAllowed() == 0UL) ||
      (gpio_pin != JOY_INT_Pin) ||
      (level != 0UL))
  {
    return;
  }

  enqueue_required =
    (ps_joystick_int_pending_count == ps_joystick_int_consumed_count) ?
      1UL : 0UL;
  ps_joystick_int_irq_count++;
  ps_joystick_int_pending_count++;
  g_ps_hw6_joystick_cardinal_request = 1UL;
  g_ps_hw6_rtos_probe.joystick_irq_count = ps_joystick_int_irq_count;
  g_ps_hw6_rtos_probe.joystick_irq_pending_count =
    ps_joystick_int_pending_count - ps_joystick_int_consumed_count;
  g_ps_hw6_rtos_probe.joystick_irq_last_level = level;
  g_ps_hw6_rtos_probe.joystick_irq_last_tick = HAL_GetTick();

  if (enqueue_required == 0UL)
  {
    g_ps_hw6_rtos_probe.joystick_irq_coalesce_count++;
    return;
  }

  if (g_ps_hw6_rtos_probe.init_complete != 0UL)
  {
    message[0] = PS_HW6_RTOS_INPUT_JOYSTICK_MAGIC;
    message[1] = PS_HW6_RTOS_OWNER_INPUT;
    message[2] = 0UL;
    message[3] = (ULONG)g_ps_hw6_rtos_probe.joystick_irq_last_tick;
    status = tx_queue_send(
      &ps_queues[PS_HW6_RTOS_OWNER_INPUT], message, TX_NO_WAIT);
  }
  g_ps_hw6_rtos_probe.joystick_irq_last_send_status = (uint32_t)status;
  if (status == TX_SUCCESS)
  {
    g_ps_hw6_rtos_probe.joystick_irq_enqueue_count++;
  }
  else
  {
    g_ps_hw6_rtos_probe.joystick_irq_drop_count++;
  }
}

static uint32_t PS_HW6_RTOS_InputRawMessageIsValid(
  uint32_t owner_id,
  const ULONG *message)
{
  uint32_t packed_edge;
  uint32_t button_id;

  if ((owner_id != PS_HW6_RTOS_OWNER_INPUT) ||
      (message[0] != PS_HW6_RTOS_INPUT_RAW_MAGIC) ||
      (message[1] != PS_HW6_RTOS_OWNER_INPUT))
  {
    return 0UL;
  }

  packed_edge = (uint32_t)message[2];
  button_id = packed_edge & PS_HW6_RTOS_INPUT_RAW_BUTTON_MASK;
  if ((button_id < (uint32_t)PS_INPUT_BUTTON_ID_A) ||
      (button_id > (uint32_t)PS_INPUT_BUTTON_ID_R) ||
      ((packed_edge & ~(PS_HW6_RTOS_INPUT_RAW_BUTTON_MASK |
                        (PS_HW6_RTOS_INPUT_RAW_ACTIVE_MASK <<
                         PS_HW6_RTOS_INPUT_RAW_ACTIVE_SHIFT))) != 0UL))
  {
    return 0UL;
  }

  return 1UL;
}

static uint32_t PS_HW6_RTOS_InputJoystickMessageIsValid(
  uint32_t owner_id,
  const ULONG *message)
{
  return ((owner_id == PS_HW6_RTOS_OWNER_INPUT) &&
          (message[0] == PS_HW6_RTOS_INPUT_JOYSTICK_MAGIC) &&
          (message[1] == PS_HW6_RTOS_OWNER_INPUT) &&
          (message[2] == 0UL)) ? 1UL : 0UL;
}

static uint32_t PS_HW6_RTOS_ClockProfilePayload(uint32_t requester_id,
                                                uint32_t profile,
                                                uint32_t capabilities)
{
  return (uint32_t)
    ((profile & PS_HW6_RTOS_CLOCK_PROFILE_MASK) |
     ((capabilities & PS_HW6_CLOCK_CAP_ALL) <<
      PS_HW6_RTOS_CLOCK_CAP_SHIFT) |
     ((requester_id & PS_HW6_RTOS_CLOCK_REQUESTER_MASK) <<
      PS_HW6_RTOS_CLOCK_REQUESTER_SHIFT));
}

static uint32_t PS_HW6_RTOS_ClockPayloadProfile(uint32_t payload)
{
  return payload & PS_HW6_RTOS_CLOCK_PROFILE_MASK;
}

static uint32_t PS_HW6_RTOS_ClockPayloadCapabilities(uint32_t payload)
{
  return (payload >> PS_HW6_RTOS_CLOCK_CAP_SHIFT) & PS_HW6_CLOCK_CAP_ALL;
}

static uint32_t PS_HW6_RTOS_ClockPayloadRequester(uint32_t payload)
{
  return (payload >> PS_HW6_RTOS_CLOCK_REQUESTER_SHIFT) &
         PS_HW6_RTOS_CLOCK_REQUESTER_MASK;
}

static ULONG PS_HW6_RTOS_ClockAckFlag(uint32_t requester_id)
{
  if (requester_id >= PS_HW6_CLOCK_REQUESTER_COUNT)
  {
    return 0UL;
  }

  return 1UL << (PS_HW6_RTOS_CLOCK_ACK_SHIFT + requester_id);
}

static uint32_t PS_HW6_RTOS_ClockProfileRequestIsValid(uint32_t payload)
{
  uint32_t profile = PS_HW6_RTOS_ClockPayloadProfile(payload);
  uint32_t capabilities = PS_HW6_RTOS_ClockPayloadCapabilities(payload);
  uint32_t requester_id = PS_HW6_RTOS_ClockPayloadRequester(payload);

  if ((payload & ~(PS_HW6_RTOS_CLOCK_PROFILE_MASK |
                   (PS_HW6_CLOCK_CAP_ALL <<
                    PS_HW6_RTOS_CLOCK_CAP_SHIFT) |
                   (PS_HW6_RTOS_CLOCK_REQUESTER_MASK <<
                    PS_HW6_RTOS_CLOCK_REQUESTER_SHIFT))) != 0UL)
  {
    return 0UL;
  }

  if (((capabilities & ~PS_HW6_CLOCK_CAP_ALL) != 0UL) ||
      (requester_id >= PS_HW6_CLOCK_REQUESTER_COUNT))
  {
    return 0UL;
  }

  return (profile <= (uint32_t)PS_HW6_CLOCK_PROFILE_STOP_PREP) ? 1UL : 0UL;
}

static uint32_t PS_HW6_RTOS_CommandIsValid(uint32_t owner_id,
                                           const ULONG *message)
{
  uint32_t cycle_index;

  if ((message[0] != PS_HW6_RTOS_COMMAND_MAGIC) ||
      (message[1] != owner_id))
  {
    return 0UL;
  }

  if ((owner_id == PS_HW6_RTOS_OWNER_POWER) &&
      (message[2] == PS_HW6_RTOS_COMMAND_POWER_WORKFLOW) &&
      (message[3] == PS_HW6_RTOS_COMMAND_TOKEN))
  {
    return 1UL;
  }
  if ((owner_id == PS_HW6_RTOS_OWNER_POWER) &&
      (message[2] == PS_HW6_RTOS_COMMAND_POWER_STOP2_RECHECK) &&
      (message[3] == PS_HW6_RTOS_COMMAND_TOKEN))
  {
    return 1UL;
  }
  if ((owner_id == PS_HW6_RTOS_OWNER_POWER) &&
      (message[2] == PS_HW6_RTOS_COMMAND_CLOCK_PROFILE) &&
      (PS_HW6_RTOS_ClockProfileRequestIsValid(
        (uint32_t)(message[3] ^ PS_HW6_RTOS_COMMAND_TOKEN)) != 0UL))
  {
    return 1UL;
  }
  if ((owner_id == PS_HW6_RTOS_OWNER_STORAGE) &&
      ((message[2] == PS_HW6_RTOS_COMMAND_USB_EXPORT) ||
       (message[2] == PS_HW6_RTOS_COMMAND_USB_RECLAIM) ||
       (message[2] == PS_HW6_RTOS_COMMAND_USB_BOOT_PARK) ||
       (message[2] == PS_HW6_RTOS_COMMAND_STORAGE_FLASH_INIT) ||
       (message[2] == PS_HW6_RTOS_COMMAND_STORAGE_ATTACH) ||
       (message[2] ==
        PS_HW6_RTOS_COMMAND_STORAGE_PACKAGE_INSTALL_EMBEDDED) ||
       (message[2] ==
        PS_HW6_RTOS_COMMAND_STORAGE_PACKAGE_LOAD_INSTALLED) ||
       (message[2] ==
        PS_HW6_RTOS_COMMAND_STORAGE_JOYSTICK_CALIBRATION_SAVE) ||
       (message[2] == PS_HW6_RTOS_COMMAND_PACKAGE_SCAN) ||
       (message[2] == PS_HW6_RTOS_COMMAND_PACKAGE_INSTALL_STUB)) &&
      (message[3] == PS_HW6_RTOS_COMMAND_TOKEN))
  {
    return 1UL;
  }
  if ((owner_id == PS_HW6_RTOS_OWNER_DISPLAY) &&
      ((message[2] == PS_HW6_RTOS_COMMAND_DISPLAY_LPBAM_PREPARE) ||
       (message[2] == PS_HW6_RTOS_COMMAND_DISPLAY_LPBAM_ABORT) ||
       (message[2] == PS_HW6_RTOS_COMMAND_DISPLAY_LPBAM_WAKE_ABORT) ||
       (message[2] == PS_HW6_RTOS_COMMAND_DISPLAY_LPBAM_EDGE_WAKE) ||
       (message[2] == PS_HW6_RTOS_COMMAND_DISPLAY_CURSOR_VISIBLE)) &&
      (message[3] == PS_HW6_RTOS_COMMAND_TOKEN))
  {
    return 1UL;
  }
  if ((owner_id == PS_HW6_RTOS_OWNER_INPUT) &&
      (message[2] == PS_HW6_RTOS_COMMAND_INPUT_JOYSTICK_WAKE_PUBLISH) &&
      (message[3] == PS_HW6_RTOS_COMMAND_TOKEN))
  {
    return 1UL;
  }
  if ((owner_id == PS_HW6_RTOS_OWNER_RUNTIME) &&
      ((message[2] == PS_HW6_RTOS_COMMAND_RUNTIME_BOOT_SHELL) ||
       (message[2] == PS_HW6_RTOS_COMMAND_RUNTIME_INSTALLER_ENTER) ||
       (message[2] == PS_HW6_RTOS_COMMAND_RUNTIME_INSTALLER_COMPLETE) ||
       (message[2] == PS_HW6_RTOS_COMMAND_RUNTIME_INSTALLER_ERROR) ||
       (message[2] == PS_HW6_RTOS_COMMAND_RUNTIME_PACKAGE_ACTIVATE_STUB) ||
       (message[2] == PS_HW6_RTOS_COMMAND_RUNTIME_PACKAGE_REACTIVE_STUB) ||
       (message[2] == PS_HW6_RTOS_COMMAND_RUNTIME_PACKAGE_REALTIME_STUB) ||
       (message[2] == PS_HW6_RTOS_COMMAND_RUNTIME_PACKAGE_RETURN) ||
       (message[2] == PS_HW6_RTOS_COMMAND_RUNTIME_SUSPEND) ||
       (message[2] == PS_HW6_RTOS_COMMAND_RUNTIME_RESUME) ||
       (message[2] == PS_HW6_RTOS_COMMAND_RUNTIME_INTERACTION_CUE) ||
       (message[2] == PS_HW6_RTOS_COMMAND_RUNTIME_INTERACTION_START) ||
       (message[2] == PS_HW6_RTOS_COMMAND_RUNTIME_INTERACTION_TIMEOUT) ||
       (message[2] == PS_HW6_RTOS_COMMAND_RUNTIME_INTERACTION_INACTIVE)) &&
      (message[3] == PS_HW6_RTOS_COMMAND_TOKEN))
  {
    return 1UL;
  }
  cycle_index = (uint32_t)(message[3] ^ PS_HW6_RTOS_COMMAND_TOKEN);
  if ((owner_id == PS_HW6_RTOS_OWNER_AUDIO) &&
      (message[2] == PS_HW6_RTOS_COMMAND_AUDIO_PLAY_SFX) &&
      (cycle_index < PS_EGG_STATE_LOADER_AUDIO_CUE_MAX))
  {
    return 1UL;
  }
  if ((owner_id == PS_HW6_RTOS_OWNER_COMM) &&
      (message[2] == PS_HW6_RTOS_COMMAND_COMM_BLE_MODE) &&
      (cycle_index <= (uint32_t)PS_HW6_COMM_BLE_MODE_CONNECTED))
  {
    return 1UL;
  }
  if ((owner_id == PS_HW6_RTOS_OWNER_SENSOR) &&
      (message[2] == PS_HW6_RTOS_COMMAND_SENSOR_IMU_MODE) &&
      (cycle_index <= (uint32_t)PS_HW6_IMU_MODE_STREAMING))
  {
    return 1UL;
  }
  if ((owner_id > PS_HW6_RTOS_OWNER_POWER) &&
      (owner_id <= PS_HW6_RTOS_OWNER_COMM))
  {
    if ((message[2] == PS_HW6_RTOS_COMMAND_STABILIZE) &&
        (message[3] == PS_HW6_RTOS_COMMAND_TOKEN))
    {
      return 1UL;
    }

    cycle_index = (uint32_t)(message[3] ^ PS_HW6_RTOS_COMMAND_TOKEN);
    if ((message[2] == PS_HW6_RTOS_COMMAND_POWER_QUIESCE) &&
        (cycle_index > (uint32_t)PS_HW6_POWER_QUIESCE_REASON_NONE) &&
        (cycle_index <=
         (uint32_t)PS_HW6_POWER_QUIESCE_REASON_SLEEP_PREP))
    {
      return 1UL;
    }
    if ((message[2] == PS_HW6_RTOS_COMMAND_POST_STOP_RESUME) &&
        (message[3] == PS_HW6_RTOS_COMMAND_TOKEN))
    {
      return 1UL;
    }
    if (((message[2] == PS_HW6_RTOS_COMMAND_RESUME) ||
         (message[2] == PS_HW6_RTOS_COMMAND_QUIESCE)) &&
        (cycle_index < PS_HW6_OWNER_SM_CYCLE_COUNT))
    {
      return 1UL;
    }
  }

  return 0UL;
}

static uint32_t PS_HW6_RTOS_StorageMscCommandIsValid(uint32_t owner_id,
                                                      const ULONG *message)
{
  if ((owner_id != PS_HW6_RTOS_OWNER_STORAGE) ||
      (message[0] != PS_HW6_RTOS_STORAGE_MSC_MAGIC) ||
      (message[1] != PS_HW6_RTOS_OWNER_STORAGE) ||
      (message[3] != PS_HW6_RTOS_STORAGE_MSC_TOKEN))
  {
    return 0UL;
  }

  return ((message[2] == PS_HW6_RTOS_STORAGE_MSC_READ) ||
          (message[2] == PS_HW6_RTOS_STORAGE_MSC_WRITE) ||
          (message[2] == PS_HW6_RTOS_STORAGE_MSC_FLUSH) ||
          (message[2] == PS_HW6_RTOS_STORAGE_MSC_STATUS)) ? 1UL : 0UL;
}

static ULONG PS_HW6_RTOS_DisplayUiPackedState(
  uint32_t calibration_page,
  uint32_t focus_index,
  uint32_t shutdown_state,
  uint32_t shutdown_countdown_seconds)
{
  return (ULONG)
    (((calibration_page & PS_HW6_RTOS_DISPLAY_UI_FIELD_MASK) <<
      PS_HW6_RTOS_DISPLAY_UI_CAL_SHIFT) |
     ((focus_index & PS_HW6_RTOS_DISPLAY_UI_FIELD_MASK) <<
      PS_HW6_RTOS_DISPLAY_UI_FOCUS_SHIFT) |
     ((shutdown_state & PS_HW6_RTOS_DISPLAY_UI_FIELD_MASK) <<
      PS_HW6_RTOS_DISPLAY_UI_SHUTDOWN_SHIFT) |
     ((shutdown_countdown_seconds & PS_HW6_RTOS_DISPLAY_UI_FIELD_MASK) <<
      PS_HW6_RTOS_DISPLAY_UI_COUNTDOWN_SHIFT));
}

static uint32_t PS_HW6_RTOS_DisplayUiPackedCalibration(ULONG packed_state)
{
  return (uint32_t)
    ((packed_state >> PS_HW6_RTOS_DISPLAY_UI_CAL_SHIFT) &
     PS_HW6_RTOS_DISPLAY_UI_FIELD_MASK);
}

static uint32_t PS_HW6_RTOS_DisplayUiPackedFocus(ULONG packed_state)
{
  return (uint32_t)
    ((packed_state >> PS_HW6_RTOS_DISPLAY_UI_FOCUS_SHIFT) &
     PS_HW6_RTOS_DISPLAY_UI_FIELD_MASK);
}

static uint32_t PS_HW6_RTOS_DisplayUiPackedShutdown(ULONG packed_state)
{
  return (uint32_t)
    ((packed_state >> PS_HW6_RTOS_DISPLAY_UI_SHUTDOWN_SHIFT) &
     PS_HW6_RTOS_DISPLAY_UI_FIELD_MASK);
}

static uint32_t PS_HW6_RTOS_DisplayUiPackedCountdown(ULONG packed_state)
{
  return (uint32_t)
    ((packed_state >> PS_HW6_RTOS_DISPLAY_UI_COUNTDOWN_SHIFT) &
     PS_HW6_RTOS_DISPLAY_UI_FIELD_MASK);
}

static uint32_t PS_HW6_RTOS_DisplayUiCommandIsValid(uint32_t owner_id,
                                                     const ULONG *message)
{
  uint32_t page = (uint32_t)message[2];
  uint32_t focus = PS_HW6_RTOS_DisplayUiPackedFocus(message[3]);
  uint32_t focus_max = (page ==
                        (uint32_t)PS_UI_ROUTER_PAGE_PACKAGE_BROWSER) ?
                       (PS_UI_ROUTER_PACKAGE_MENU_FOCUS_BASE + 1UL) : 2UL;

  if ((owner_id != PS_HW6_RTOS_OWNER_DISPLAY) ||
      (message[0] != PS_HW6_RTOS_DISPLAY_UI_MAGIC) ||
      (message[1] != PS_HW6_RTOS_OWNER_DISPLAY) ||
      (page > PS_UI_ROUTER_PAGE_INPUT_DIAGNOSTIC) ||
      (PS_HW6_RTOS_DisplayUiPackedCalibration(message[3]) >
       PS_UI_ROUTER_CAL_JOYSTICK_LEFT) ||
      (focus > focus_max) ||
      (PS_HW6_RTOS_DisplayUiPackedShutdown(message[3]) >
       PS_HW6_RTOS_DISPLAY_UI_SHUTDOWN_MAX) ||
      (PS_HW6_RTOS_DisplayUiPackedCountdown(message[3]) > 9UL))
  {
    return 0UL;
  }

  return 1UL;
}

static uint32_t PS_HW6_RTOS_LogicalSourceIsButton(uint32_t source)
{
  return ((source >= (uint32_t)PS_INPUT_LOGICAL_SOURCE_BUTTON_A) &&
          (source <= (uint32_t)PS_INPUT_LOGICAL_SOURCE_START)) ? 1UL : 0UL;
}

static uint32_t PS_HW6_RTOS_LogicalSourceIsJoystickDirection(
  uint32_t source)
{
  return ((source >= (uint32_t)PS_INPUT_LOGICAL_SOURCE_JOY_LEFT) &&
          (source <=
           (uint32_t)PS_INPUT_LOGICAL_SOURCE_JOY_DOWN_RIGHT)) ? 1UL : 0UL;
}

static uint32_t PS_HW6_RTOS_LogicalSourceForJoystickDirection(
  uint32_t direction_mask);

static uint32_t PS_HW6_RTOS_JoystickCandidateMaskIsValid(uint32_t mask)
{
  return (((mask & ~0x0FUL) == 0UL) &&
          ((mask & 0x03UL) != 0x03UL) &&
          ((mask & 0x0CUL) != 0x0CUL)) ? 1UL : 0UL;
}

static uint32_t PS_HW6_RTOS_JoystickResolvedMaskIsValid(uint32_t mask)
{
  return (((mask & ~0x0FUL) == 0UL) &&
          ((mask == 0UL) || ((mask & (mask - 1UL)) == 0UL))) ? 1UL : 0UL;
}

static uint32_t PS_HW6_RTOS_UiInputPackedSource(uint32_t packed)
{
  return packed & PS_HW6_RTOS_UI_INPUT_SOURCE_MASK;
}

static uint32_t PS_HW6_RTOS_UiInputPackedCandidate(uint32_t packed)
{
  return (packed >> PS_HW6_RTOS_UI_INPUT_CANDIDATE_SHIFT) &
         PS_HW6_RTOS_UI_INPUT_CANDIDATE_MASK;
}

static uint32_t PS_HW6_RTOS_UiInputPackedResolved(uint32_t packed)
{
  return (packed >> PS_HW6_RTOS_UI_INPUT_RESOLVED_SHIFT) &
         PS_HW6_RTOS_UI_INPUT_RESOLVED_MASK;
}

static uint32_t PS_HW6_RTOS_UiInputPackedDispatch(uint32_t packed)
{
  return (packed >> PS_HW6_RTOS_UI_INPUT_DISPATCH_SHIFT) &
         PS_HW6_RTOS_UI_INPUT_DISPATCH_MASK;
}

static uint32_t PS_HW6_RTOS_UiInputCommandIsValid(uint32_t owner_id,
                                                  const ULONG *message)
{
  uint32_t command = (uint32_t)message[2];
  uint32_t packed = (uint32_t)message[3];
  uint32_t source = PS_HW6_RTOS_UiInputPackedSource(packed);
  uint32_t candidate = PS_HW6_RTOS_UiInputPackedCandidate(packed);
  uint32_t resolved = PS_HW6_RTOS_UiInputPackedResolved(packed);
  uint32_t dispatch = PS_HW6_RTOS_UiInputPackedDispatch(packed);

  if ((owner_id != PS_HW6_RTOS_OWNER_UI) ||
      (message[0] != PS_HW6_RTOS_UI_INPUT_MAGIC) ||
      (message[1] != PS_HW6_RTOS_OWNER_UI))
  {
    return 0UL;
  }

  if (command == PS_HW6_RTOS_UI_INPUT_PRESS)
  {
    return ((packed == source) &&
            (PS_HW6_RTOS_LogicalSourceIsButton(source) != 0UL)) ? 1UL : 0UL;
  }

  if ((command != PS_HW6_RTOS_UI_INPUT_JOYSTICK_STATE) ||
      (PS_HW6_RTOS_JoystickCandidateMaskIsValid(candidate) == 0UL) ||
      (PS_HW6_RTOS_JoystickResolvedMaskIsValid(resolved) == 0UL) ||
      ((dispatch == 0UL) &&
       (source != (uint32_t)PS_INPUT_LOGICAL_SOURCE_NONE)) ||
      ((dispatch != 0UL) &&
       (PS_HW6_RTOS_LogicalSourceIsJoystickDirection(source) == 0UL)) ||
      ((dispatch != 0UL) &&
       (PS_HW6_RTOS_LogicalSourceForJoystickDirection(resolved) != source)))
  {
    return 0UL;
  }

  return 1UL;
}

static uint32_t PS_HW6_RTOS_UiLifecycleCommandIsValid(
  uint32_t owner_id,
  const ULONG *message)
{
  uint32_t event = (uint32_t)message[2];

  return ((owner_id == PS_HW6_RTOS_OWNER_UI) &&
          (message[0] == PS_HW6_RTOS_UI_LIFECYCLE_MAGIC) &&
          (message[1] == PS_HW6_RTOS_OWNER_UI) &&
          (message[3] == 0UL) &&
          (event >= (uint32_t)PS_UI_ROUTER_EVENT_MSC_EXPORT) &&
          (event <= (uint32_t)PS_UI_ROUTER_EVENT_MSC_RECOVERY)) ? 1UL : 0UL;
}

static uint32_t PS_HW6_RTOS_RuntimeInputCommandIsValid(
  uint32_t owner_id,
  const ULONG *message)
{
  uint32_t event;
  uint32_t packed_button;
  uint32_t button_id;
  uint32_t button_mask;

  if ((owner_id != PS_HW6_RTOS_OWNER_RUNTIME) ||
      (message[0] != PS_HW6_RTOS_RUNTIME_INPUT_MAGIC) ||
      (message[1] != PS_HW6_RTOS_OWNER_RUNTIME))
  {
    return 0UL;
  }

  event = (uint32_t)message[2];
  packed_button = (uint32_t)message[3];
  button_id = packed_button & PS_HW6_RTOS_RUNTIME_INPUT_BUTTON_ID_MASK;
  button_mask = (packed_button >> PS_HW6_RTOS_RUNTIME_INPUT_BUTTON_MASK_SHIFT) &
                PS_HW6_RTOS_RUNTIME_INPUT_BUTTON_MASK_MASK;

  if ((event < (uint32_t)PS_INPUT_BUTTON_LOGICAL_EVENT_PRESS) ||
      (event > (uint32_t)PS_INPUT_BUTTON_LOGICAL_EVENT_REPEAT) ||
      ((PS_HW6_RTOS_LogicalSourceIsButton(button_id) == 0UL) &&
       (PS_HW6_RTOS_LogicalSourceIsJoystickDirection(button_id) == 0UL)) ||
      ((button_id == (uint32_t)PS_INPUT_LOGICAL_SOURCE_START) &&
       (event != (uint32_t)PS_INPUT_BUTTON_LOGICAL_EVENT_PRESS)) ||
      (button_mask == 0UL) ||
      ((button_mask & ~0x1FUL) != 0UL))
  {
    return 0UL;
  }

  return 1UL;
}

static void PS_HW6_RTOS_HandleRuntimeInput(const ULONG *message)
{
  uint32_t event;
  uint32_t packed_button;
  uint32_t button_id;
  uint32_t button_mask;
  UINT status = TX_SUCCESS;

  event = (uint32_t)message[2];
  packed_button = (uint32_t)message[3];
  button_id = packed_button & PS_HW6_RTOS_RUNTIME_INPUT_BUTTON_ID_MASK;
  button_mask = (packed_button >> PS_HW6_RTOS_RUNTIME_INPUT_BUTTON_MASK_SHIFT) &
                PS_HW6_RTOS_RUNTIME_INPUT_BUTTON_MASK_MASK;

  if ((g_ps_hw6_rtos_probe.runtime_current_class ==
       (uint32_t)PS_HW6_RUNTIME_CLASS_LP_GRAPH) &&
      (PS_SceneRuntime_StateSceneActive() != 0UL))
  {
    uint32_t scene_result;

    scene_result = PS_SceneRuntime_HandleStateSceneInput(event, button_id);
    if (scene_result == PS_SCENE_RUNTIME_INPUT_APPLIED)
    {
      uint32_t cue_index;
      uint32_t shell_exit_requested;

      PS_HW6_RTOS_RuntimeInteractionRefresh(event,
                                             button_id,
                                             (uint32_t)tx_time_get());
      shell_exit_requested = PS_SceneRuntime_TakeShellExitRequest();
      if (shell_exit_requested != 0UL)
      {
        status = PS_HW6_RTOS_RequestRuntimeCommand(
          PS_HW6_RTOS_COMMAND_RUNTIME_PACKAGE_RETURN);
      }
      else if (PS_SceneRuntime_TakeSfxRequest(&cue_index) != 0UL)
      {
        UINT sfx_status;

        g_ps_hw6_rtos_probe.audio_sfx_dispatch_count++;
        g_ps_hw6_rtos_probe.audio_sfx_last_cue_index = cue_index;
        if (ps_audio_sfx_pending == 0UL)
        {
          ps_audio_sfx_pending = 1UL;
          sfx_status = PS_HW6_RTOS_SendModeCommand(
            PS_HW6_RTOS_OWNER_AUDIO,
            PS_HW6_RTOS_COMMAND_AUDIO_PLAY_SFX,
            cue_index);
          if (sfx_status != TX_SUCCESS)
          {
            ps_audio_sfx_pending = 0UL;
          }
        }
        else
        {
          sfx_status = TX_QUEUE_FULL;
        }
        g_ps_hw6_rtos_probe.audio_sfx_send_status =
          (uint32_t)sfx_status;
      }
      if (shell_exit_requested == 0UL)
      {
        status = PS_HW6_RTOS_SendDisplayUiRenderCommand(
          (uint32_t)PS_UI_ROUTER_PAGE_RUNTIME_HANDOFF,
          (uint32_t)PS_UI_ROUTER_CAL_NONE,
          PS_SceneRuntime_StateFocusIndex(),
          (uint32_t)PS_UI_ROUTER_SHUTDOWN_NONE,
          0UL);
      }
    }
    else if (scene_result == PS_SCENE_RUNTIME_INPUT_ERROR)
    {
      status = TX_CALLER_ERROR;
    }
  }

  g_ps_hw6_rtos_probe.runtime_input_event_count++;
  if (event == (uint32_t)PS_INPUT_BUTTON_LOGICAL_EVENT_PRESS)
  {
    g_ps_hw6_rtos_probe.runtime_input_button_count++;
  }
  g_ps_hw6_rtos_probe.runtime_input_last_event = event;
  g_ps_hw6_rtos_probe.runtime_input_last_button_id = button_id;
  g_ps_hw6_rtos_probe.runtime_input_last_mask = button_mask;
  g_ps_hw6_rtos_probe.runtime_input_last_status = (uint32_t)status;
  g_ps_hw6_rtos_probe.runtime_input_last_tick = (uint32_t)tx_time_get();
  PS_HW6_TraceInputButton(button_id,
                          PS_HW6_RTOS_OWNER_RUNTIME,
                          (uint32_t)status,
                          button_mask);
}

static uint32_t PS_HW6_RTOS_PowerInputCommandIsValid(uint32_t owner_id,
                                                     const ULONG *message)
{
  if ((owner_id != PS_HW6_RTOS_OWNER_POWER) ||
      (message[0] != PS_HW6_RTOS_POWER_INPUT_MAGIC) ||
      (message[1] != PS_HW6_RTOS_OWNER_POWER) ||
      (message[2] < PS_INPUT_START_POWER_EVENT_SHIP_PREP) ||
      (message[2] > PS_INPUT_START_POWER_EVENT_RELEASED_BEFORE_SHIP))
  {
    return 0UL;
  }

  return 1UL;
}

static uint32_t PS_HW6_RTOS_RouterEventForLogicalSource(uint32_t source)
{
  if (source == (uint32_t)PS_INPUT_LOGICAL_SOURCE_BUTTON_A)
  {
    return PS_UI_ROUTER_EVENT_INPUT_BTN_A;
  }
  if (source == (uint32_t)PS_INPUT_LOGICAL_SOURCE_BUTTON_B)
  {
    return PS_UI_ROUTER_EVENT_INPUT_BTN_B;
  }
  if (source == (uint32_t)PS_INPUT_LOGICAL_SOURCE_BUTTON_L)
  {
    return PS_UI_ROUTER_EVENT_INPUT_BTN_L;
  }
  if (source == (uint32_t)PS_INPUT_LOGICAL_SOURCE_BUTTON_R)
  {
    return PS_UI_ROUTER_EVENT_INPUT_BTN_R;
  }
  if (source == (uint32_t)PS_INPUT_LOGICAL_SOURCE_JOY_LEFT)
  {
    return PS_UI_ROUTER_EVENT_INPUT_JOY_LEFT;
  }
  if (source == (uint32_t)PS_INPUT_LOGICAL_SOURCE_JOY_RIGHT)
  {
    return PS_UI_ROUTER_EVENT_INPUT_JOY_RIGHT;
  }
  if (source == (uint32_t)PS_INPUT_LOGICAL_SOURCE_JOY_UP)
  {
    return PS_UI_ROUTER_EVENT_INPUT_JOY_UP;
  }
  if (source == (uint32_t)PS_INPUT_LOGICAL_SOURCE_JOY_DOWN)
  {
    return PS_UI_ROUTER_EVENT_INPUT_JOY_DOWN;
  }

  return 0UL;
}

static uint32_t PS_HW6_RTOS_RouterEventForStartPower(
  uint32_t start_power_event)
{
  if (start_power_event ==
      (uint32_t)PS_INPUT_START_POWER_EVENT_SHIP_PREP)
  {
    return PS_UI_ROUTER_EVENT_SHUTDOWN_PREP;
  }
  if (start_power_event ==
      (uint32_t)PS_INPUT_START_POWER_EVENT_SHIP_WARNING)
  {
    return PS_UI_ROUTER_EVENT_SHUTDOWN_WARNING;
  }
  if (start_power_event ==
      (uint32_t)PS_INPUT_START_POWER_EVENT_SHIP_IMMINENT)
  {
    return PS_UI_ROUTER_EVENT_SHUTDOWN_IMMINENT;
  }
  if (start_power_event ==
      (uint32_t)PS_INPUT_START_POWER_EVENT_RELEASED_BEFORE_SHIP)
  {
    return PS_UI_ROUTER_EVENT_SHUTDOWN_CANCEL;
  }

  return 0UL;
}

static uint32_t PS_HW6_RTOS_RouterEventForCalibrationCapture(
  uint32_t calibration_page)
{
  if (calibration_page == PS_UI_ROUTER_CAL_JOYSTICK_NEUTRAL)
  {
    return PS_UI_ROUTER_EVENT_CAL_JOYSTICK_NEUTRAL_ACCEPT;
  }
  if (calibration_page == PS_UI_ROUTER_CAL_JOYSTICK_UP)
  {
    return PS_UI_ROUTER_EVENT_CAL_JOYSTICK_UP_ACCEPT;
  }
  if (calibration_page == PS_UI_ROUTER_CAL_JOYSTICK_RIGHT)
  {
    return PS_UI_ROUTER_EVENT_CAL_JOYSTICK_RIGHT_ACCEPT;
  }
  if (calibration_page == PS_UI_ROUTER_CAL_JOYSTICK_DOWN)
  {
    return PS_UI_ROUTER_EVENT_CAL_JOYSTICK_DOWN_ACCEPT;
  }
  if (calibration_page == PS_UI_ROUTER_CAL_JOYSTICK_LEFT)
  {
    return PS_UI_ROUTER_EVENT_CAL_JOYSTICK_LEFT_ACCEPT;
  }
  if (calibration_page == PS_UI_ROUTER_CAL_JOYSTICK_SWEEP)
  {
    return PS_UI_ROUTER_EVENT_CAL_JOYSTICK_SWEEP_ACCEPT;
  }
  if (calibration_page == PS_UI_ROUTER_CAL_JOYSTICK_REVIEW)
  {
    return PS_UI_ROUTER_EVENT_CAL_JOYSTICK_REVIEW_ACCEPT;
  }
  return 0UL;
}

static uint32_t PS_HW6_RTOS_RequestJoystickCalibrationCapture(
  uint32_t button_id)
{
  uint32_t calibration_page = g_ps_ui_router_probe.calibration_page;

  if ((button_id == PS_INPUT_BUTTON_ID_A) &&
      (g_ps_ui_router_probe.current_page == PS_UI_ROUTER_PAGE_CALIBRATION))
  {
    if (PS_HW6_OwnerStateMachines_JoystickCalibrationCaptureActive() != 0UL)
    {
      return 1UL;
    }
    if (PS_HW6_RTOS_RouterEventForCalibrationCapture(calibration_page) == 0UL)
    {
      return 0UL;
    }
    g_ps_hw6_joystick_calibration_capture_page = calibration_page;
    g_ps_hw6_joystick_calibration_capture_request = 1UL;
    return 1UL;
  }

  return 0UL;
}

static uint32_t PS_HW6_RTOS_CommandCycleIndex(const ULONG *message)
{
  return (uint32_t)(message[3] ^ PS_HW6_RTOS_COMMAND_TOKEN);
}

static uint32_t PS_HW6_RTOS_QueueInputRawEdge(
  ps_input_button_id_t button_id,
  uint32_t active,
  uint32_t timestamp)
{
  ULONG message[PS_HW6_RTOS_MESSAGE_WORDS];
  UINT status;
  uint32_t enqueued;

  if ((button_id < PS_INPUT_BUTTON_ID_A) ||
      (button_id > PS_INPUT_BUTTON_ID_R))
  {
    return (uint32_t)TX_PTR_ERROR;
  }

  active = (active != 0UL) ? 1UL : 0UL;
  message[0] = PS_HW6_RTOS_INPUT_RAW_MAGIC;
  message[1] = PS_HW6_RTOS_OWNER_INPUT;
  message[2] = ((ULONG)button_id & PS_HW6_RTOS_INPUT_RAW_BUTTON_MASK) |
    ((ULONG)active << PS_HW6_RTOS_INPUT_RAW_ACTIVE_SHIFT);
  message[3] = (ULONG)timestamp;

  status = tx_queue_send(&ps_queues[PS_HW6_RTOS_OWNER_INPUT],
                         message,
                         TX_NO_WAIT);
  g_ps_hw6_rtos_probe.input_raw_last_send_status = (uint32_t)status;
  g_ps_hw6_rtos_probe.input_raw_last_button_id = (uint32_t)button_id;
  g_ps_hw6_rtos_probe.input_raw_last_active = active;
  g_ps_hw6_rtos_probe.input_raw_last_timestamp = timestamp;
  if (status == TX_SUCCESS)
  {
    g_ps_hw6_rtos_probe.input_raw_enqueue_count++;
    enqueued = (uint32_t)
      ps_queues[PS_HW6_RTOS_OWNER_INPUT].tx_queue_enqueued;
    if (enqueued > g_ps_hw6_rtos_probe.input_raw_queue_high_water)
    {
      g_ps_hw6_rtos_probe.input_raw_queue_high_water = enqueued;
    }
  }
  else
  {
    g_ps_hw6_rtos_probe.input_raw_drop_count++;
  }

  return (uint32_t)status;
}

static UINT PS_HW6_RTOS_SendCommand(uint32_t owner_id,
                                     ULONG command)
{
  ULONG message[PS_HW6_RTOS_MESSAGE_WORDS];

  if (owner_id >= PS_HW6_RTOS_QUEUE_COUNT)
  {
    return TX_QUEUE_ERROR;
  }

  message[0] = PS_HW6_RTOS_COMMAND_MAGIC;
  message[1] = (ULONG)owner_id;
  message[2] = command;
  message[3] = PS_HW6_RTOS_COMMAND_TOKEN;
  return tx_queue_send(&ps_queues[owner_id], message, TX_NO_WAIT);
}

static UINT PS_HW6_RTOS_SendCycleCommand(uint32_t owner_id,
                                          ULONG command,
                                          uint32_t cycle_index)
{
  ULONG message[PS_HW6_RTOS_MESSAGE_WORDS];

  if ((owner_id >= PS_HW6_RTOS_QUEUE_COUNT) ||
      (cycle_index >= PS_HW6_OWNER_SM_CYCLE_COUNT))
  {
    return TX_QUEUE_ERROR;
  }

  message[0] = PS_HW6_RTOS_COMMAND_MAGIC;
  message[1] = (ULONG)owner_id;
  message[2] = command;
  message[3] = PS_HW6_RTOS_COMMAND_TOKEN ^ (ULONG)cycle_index;
  return tx_queue_send(&ps_queues[owner_id], message, TX_NO_WAIT);
}
static UINT PS_HW6_RTOS_SendModeCommand(uint32_t owner_id,
                                         ULONG command,
                                         uint32_t mode)
{
  ULONG message[PS_HW6_RTOS_MESSAGE_WORDS];

  if (owner_id >= PS_HW6_RTOS_QUEUE_COUNT)
  {
    return TX_QUEUE_ERROR;
  }

  message[0] = PS_HW6_RTOS_COMMAND_MAGIC;
  message[1] = (ULONG)owner_id;
  message[2] = command;
  message[3] = PS_HW6_RTOS_COMMAND_TOKEN ^ (ULONG)mode;
  return tx_queue_send(&ps_queues[owner_id], message, TX_NO_WAIT);
}
static UINT PS_HW6_RTOS_SendPowerQuiesceCommand(uint32_t owner_id,
                                                 uint32_t reason)
{
  ULONG message[PS_HW6_RTOS_MESSAGE_WORDS];

  if ((owner_id >= PS_HW6_RTOS_QUEUE_COUNT) ||
      (reason <= (uint32_t)PS_HW6_POWER_QUIESCE_REASON_NONE) ||
      (reason > (uint32_t)PS_HW6_POWER_QUIESCE_REASON_SLEEP_PREP))
  {
    return TX_QUEUE_ERROR;
  }

  message[0] = PS_HW6_RTOS_COMMAND_MAGIC;
  message[1] = (ULONG)owner_id;
  message[2] = PS_HW6_RTOS_COMMAND_POWER_QUIESCE;
  message[3] = PS_HW6_RTOS_COMMAND_TOKEN ^ (ULONG)reason;
  return tx_queue_send(&ps_queues[owner_id], message, TX_NO_WAIT);
}
static UINT PS_HW6_RTOS_SendPostStopResumeCommand(uint32_t owner_id)
{
  ULONG message[PS_HW6_RTOS_MESSAGE_WORDS];

  if ((owner_id <= PS_HW6_RTOS_OWNER_POWER) ||
      (owner_id >= PS_HW6_RTOS_QUEUE_COUNT))
  {
    return TX_QUEUE_ERROR;
  }

  message[0] = PS_HW6_RTOS_COMMAND_MAGIC;
  message[1] = (ULONG)owner_id;
  message[2] = PS_HW6_RTOS_COMMAND_POST_STOP_RESUME;
  message[3] = PS_HW6_RTOS_COMMAND_TOKEN;
  return tx_queue_send(&ps_queues[owner_id], message, TX_NO_WAIT);
}
static UINT PS_HW6_RTOS_RequestRuntimeCommand(ULONG command)
{
  UINT status;

  status = PS_HW6_RTOS_SendCommand(PS_HW6_RTOS_OWNER_RUNTIME, command);
  g_ps_hw6_rtos_probe.runtime_owner_request_count++;
  g_ps_hw6_rtos_probe.runtime_owner_request_status = (uint32_t)status;
  return status;
}

static uint32_t PS_HW6_RTOS_BeginBootHome(void)
{
  UINT status;

  if (g_ps_hw6_owner_sm_probe
        .joystick_calibration_persistent_load_available == 0UL)
  {
    ps_status_t router_status = PS_UIRouter_Dispatch(
      PS_UI_ROUTER_EVENT_NAV_CALIBRATION);

    if (router_status == PS_STATUS_OK)
    {
      router_status = PS_UIRouter_Dispatch(
        PS_UI_ROUTER_EVENT_CAL_JOYSTICK_START);
    }
    ps_boot_package_launch_after_calibration =
      (router_status == PS_STATUS_OK) ? 1UL : 0UL;
    return 1UL;
  }

  ps_boot_package_launch_after_calibration = 0UL;
  status = PS_HW6_RTOS_RequestRuntimeCommand(
    PS_HW6_RTOS_COMMAND_RUNTIME_PACKAGE_REACTIVE_STUB);
  if (status == TX_SUCCESS)
  {
    return 0UL;
  }

  (void)PS_UIRouter_Dispatch(PS_UI_ROUTER_EVENT_RUNTIME_UNAVAILABLE);
  return 1UL;
}

static UINT PS_HW6_RTOS_RequestRuntimeCommandAndWait(ULONG command)
{
  ULONG actual_flags = 0UL;
  UINT status;

  (void)tx_event_flags_get(
    &ps_event_groups[PS_HW6_RTOS_EVENT_DEBUG_INDEX],
    PS_HW6_RTOS_ACK_OWNER(PS_HW6_RTOS_OWNER_RUNTIME),
    TX_AND_CLEAR,
    &actual_flags,
    TX_NO_WAIT);

  status = PS_HW6_RTOS_RequestRuntimeCommand(command);
  if (status != TX_SUCCESS)
  {
    return status;
  }

  return tx_event_flags_get(
    &ps_event_groups[PS_HW6_RTOS_EVENT_DEBUG_INDEX],
    PS_HW6_RTOS_ACK_OWNER(PS_HW6_RTOS_OWNER_RUNTIME),
    TX_AND_CLEAR,
    &actual_flags,
    PS_HW6_RTOS_OWNER_ACK_WAIT_TICKS);
}

static uint32_t PS_HW6_RTOS_SystemOverlayActive(void)
{
  return ((g_ps_ui_router_probe.current_page ==
           (uint32_t)PS_UI_ROUTER_PAGE_SHUTDOWN) ||
          (ps_runtime_interaction_cue_active != 0UL) ||
          (ps_runtime_interaction_activation_active != 0UL) ||
          (g_ps_ui_router_probe.shutdown_state !=
           (uint32_t)PS_UI_ROUTER_SHUTDOWN_NONE) ||
          (g_ps_hw6_owner_sm_probe.usb_host_msc_active != 0UL) ||
          (g_ps_storage_msc_bridge_probe.export_enabled != 0UL)) ?
         1UL : 0UL;
}

static uint32_t PS_HW6_RTOS_RuntimePackageActive(void)
{
  uint32_t runtime_class = g_ps_hw6_rtos_probe.runtime_current_class;

  if (g_ps_hw6_rtos_probe.runtime_lifecycle !=
      PS_HW6_RUNTIME_LIFECYCLE_RUNNING)
  {
    return 0UL;
  }

  return ((runtime_class == PS_HW6_RUNTIME_CLASS_LP_GRAPH) ||
          (runtime_class == PS_HW6_RUNTIME_CLASS_LP_MODULE) ||
          (runtime_class == PS_HW6_RUNTIME_CLASS_RT_SCENE)) ?
         1UL : 0UL;
}

static uint32_t PS_HW6_RTOS_AdmissionActionIsPowerAction(uint32_t action)
{
  return ((action == PS_HW6_RTOS_ADMISSION_ACTION_POWER_SHUTDOWN_PREP) ||
          (action ==
           PS_HW6_RTOS_ADMISSION_ACTION_POWER_BATTERY_CRITICAL_SHIP_PREP) ||
          (action ==
           PS_HW6_RTOS_ADMISSION_ACTION_POWER_BOOT_LOW_BATTERY_SHIP_PREP)) ?
         1UL : 0UL;
}

static uint32_t PS_HW6_RTOS_AdmissionActionNeedsRuntimeSuspend(
  uint32_t action)
{
  return ((action == PS_HW6_RTOS_ADMISSION_ACTION_UI_MSC_ENTER) ||
          (action ==
           PS_HW6_RTOS_ADMISSION_ACTION_UI_PACKAGE_INSTALL_STUB) ||
          (PS_HW6_RTOS_AdmissionActionIsPowerAction(action) != 0UL)) ?
         1UL : 0UL;
}

static uint32_t PS_HW6_RTOS_AdmissionActionForPowerQuiesceReason(
  uint32_t reason)
{
  if (reason == (uint32_t)PS_HW6_POWER_QUIESCE_REASON_START_SHUTDOWN)
  {
    return PS_HW6_RTOS_ADMISSION_ACTION_POWER_SHUTDOWN_PREP;
  }
  if (reason == (uint32_t)PS_HW6_POWER_QUIESCE_REASON_BATTERY_CRITICAL)
  {
    return PS_HW6_RTOS_ADMISSION_ACTION_POWER_BATTERY_CRITICAL_SHIP_PREP;
  }
  if (reason == (uint32_t)PS_HW6_POWER_QUIESCE_REASON_BOOT_LOW_BATTERY)
  {
    return PS_HW6_RTOS_ADMISSION_ACTION_POWER_BOOT_LOW_BATTERY_SHIP_PREP;
  }

  return PS_HW6_RTOS_ADMISSION_ACTION_NONE;
}

static void PS_HW6_RTOS_RecordAdmission(
  uint32_t action,
  uint32_t result,
  uint32_t reason,
  UINT status,
  uint32_t overlay_active)
{
  g_ps_hw6_rtos_probe.admission_request_count++;
  if (result == PS_HW6_RTOS_ADMISSION_RESULT_DENY)
  {
    g_ps_hw6_rtos_probe.admission_deny_count++;
  }
  else
  {
    g_ps_hw6_rtos_probe.admission_allow_count++;
  }

  if (result == PS_HW6_RTOS_ADMISSION_RESULT_ALLOW_AFTER_SUSPEND)
  {
    g_ps_hw6_rtos_probe.admission_suspend_count++;
  }

  g_ps_hw6_rtos_probe.admission_last_action = action;
  g_ps_hw6_rtos_probe.admission_last_result = result;
  g_ps_hw6_rtos_probe.admission_last_reason = reason;
  g_ps_hw6_rtos_probe.admission_last_status = (uint32_t)status;
  g_ps_hw6_rtos_probe.admission_last_runtime_class =
    g_ps_hw6_rtos_probe.runtime_current_class;
  g_ps_hw6_rtos_probe.admission_last_runtime_lifecycle =
    g_ps_hw6_rtos_probe.runtime_lifecycle;
  g_ps_hw6_rtos_probe.admission_last_ui_page =
    g_ps_ui_router_probe.current_page;
  g_ps_hw6_rtos_probe.admission_last_package_state =
    g_ps_ui_router_probe.package_state;
  g_ps_hw6_rtos_probe.admission_last_shutdown_state =
    g_ps_ui_router_probe.shutdown_state;
  g_ps_hw6_rtos_probe.admission_last_overlay_active = overlay_active;
  g_ps_hw6_rtos_probe.admission_last_tick = (uint32_t)tx_time_get();
}

static uint32_t PS_HW6_RTOS_AdmissionActionForUiRouterAction(
  uint32_t action)
{
  if (action == (uint32_t)PS_UI_ROUTER_ACTION_MSC_ENTER)
  {
    return PS_HW6_RTOS_ADMISSION_ACTION_UI_MSC_ENTER;
  }
  if (action == (uint32_t)PS_UI_ROUTER_ACTION_MSC_EXIT)
  {
    return PS_HW6_RTOS_ADMISSION_ACTION_UI_MSC_EXIT;
  }
  if (action == (uint32_t)PS_UI_ROUTER_ACTION_PACKAGE_SCAN)
  {
    return PS_HW6_RTOS_ADMISSION_ACTION_UI_PACKAGE_INSTALL_STUB;
  }
  if (action == (uint32_t)PS_UI_ROUTER_ACTION_PACKAGE_INSTALL_STUB)
  {
    return PS_HW6_RTOS_ADMISSION_ACTION_UI_PACKAGE_INSTALL_STUB;
  }

  return PS_HW6_RTOS_ADMISSION_ACTION_NONE;
}

static UINT PS_HW6_RTOS_AdmitSystemAction(uint32_t action)
{
  uint32_t overlay_active = PS_HW6_RTOS_SystemOverlayActive();
  uint32_t power_action = PS_HW6_RTOS_AdmissionActionIsPowerAction(action);
  UINT status = TX_SUCCESS;

  if (action == PS_HW6_RTOS_ADMISSION_ACTION_NONE)
  {
    PS_HW6_RTOS_RecordAdmission(
      action,
      PS_HW6_RTOS_ADMISSION_RESULT_DENY,
      PS_HW6_RTOS_ADMISSION_REASON_UNSUPPORTED,
      TX_QUEUE_ERROR,
      overlay_active);
    return TX_QUEUE_ERROR;
  }

  if (action == PS_HW6_RTOS_ADMISSION_ACTION_UI_MSC_EXIT)
  {
    PS_HW6_RTOS_RecordAdmission(
      action,
      PS_HW6_RTOS_ADMISSION_RESULT_ALLOW,
      PS_HW6_RTOS_ADMISSION_REASON_SYSTEM_OVERLAY,
      TX_SUCCESS,
      overlay_active);
    return TX_SUCCESS;
  }

  if ((overlay_active != 0UL) && (power_action == 0UL))
  {
    PS_HW6_RTOS_RecordAdmission(
      action,
      PS_HW6_RTOS_ADMISSION_RESULT_DENY,
      PS_HW6_RTOS_ADMISSION_REASON_SYSTEM_BUSY,
      TX_NOT_DONE,
      overlay_active);
    return TX_NOT_DONE;
  }

  if ((PS_HW6_RTOS_RuntimePackageActive() != 0UL) &&
      (PS_HW6_RTOS_AdmissionActionNeedsRuntimeSuspend(action) != 0UL))
  {
    status = PS_HW6_RTOS_RequestRuntimeCommandAndWait(
      PS_HW6_RTOS_COMMAND_RUNTIME_SUSPEND);
    if (status != TX_SUCCESS)
    {
      PS_HW6_RTOS_RecordAdmission(
        action,
        PS_HW6_RTOS_ADMISSION_RESULT_DENY,
        PS_HW6_RTOS_ADMISSION_REASON_SEND_FAILED,
        status,
        overlay_active);
      return status;
    }

    if (power_action != 0UL)
    {
      g_ps_hw6_rtos_probe.admission_runtime_suspended_by_system = 1UL;
      g_ps_hw6_rtos_probe.admission_runtime_suspended_action = action;
    }
    PS_HW6_RTOS_RecordAdmission(
      action,
      PS_HW6_RTOS_ADMISSION_RESULT_ALLOW_AFTER_SUSPEND,
      PS_HW6_RTOS_ADMISSION_REASON_RUNTIME_SUSPENDED,
      TX_SUCCESS,
      overlay_active);
    return TX_SUCCESS;
  }

  PS_HW6_RTOS_RecordAdmission(
    action,
    PS_HW6_RTOS_ADMISSION_RESULT_ALLOW,
    PS_HW6_RTOS_ADMISSION_REASON_UI_SHELL,
    TX_SUCCESS,
    overlay_active);
  return TX_SUCCESS;
}
static UINT PS_HW6_RTOS_ResumePowerSuspendedRuntime(uint32_t reason)
{
  UINT status = TX_SUCCESS;

  g_ps_hw6_rtos_probe.admission_resume_count++;
  g_ps_hw6_rtos_probe.admission_runtime_resume_reason = reason;

  if ((g_ps_hw6_rtos_probe.admission_runtime_suspended_by_system != 0UL) &&
      (g_ps_hw6_rtos_probe.admission_runtime_suspended_action ==
       PS_HW6_RTOS_ADMISSION_ACTION_POWER_SHUTDOWN_PREP))
  {
    status = PS_HW6_RTOS_RequestRuntimeCommandAndWait(
      PS_HW6_RTOS_COMMAND_RUNTIME_RESUME);
    if (status == TX_SUCCESS)
    {
      g_ps_hw6_rtos_probe.admission_runtime_suspended_by_system = 0UL;
      g_ps_hw6_rtos_probe.admission_runtime_suspended_action =
        PS_HW6_RTOS_ADMISSION_ACTION_NONE;
    }
  }

  g_ps_hw6_rtos_probe.admission_runtime_resume_status = (uint32_t)status;
  return status;
}

static HAL_StatusTypeDef PS_HW6_RTOS_RequestPowerSystemAdmission(
  uint32_t reason)
{
  UINT status;
  uint32_t action = PS_HW6_RTOS_AdmissionActionForPowerQuiesceReason(reason);

  status = PS_HW6_RTOS_AdmitSystemAction(action);
  return (status == TX_SUCCESS) ? HAL_OK : HAL_ERROR;
}

UINT PS_HW6_RTOS_RequestUsbMscEnter(void)
{
  return PS_HW6_RTOS_SendCommand(
    PS_HW6_RTOS_OWNER_STORAGE,
    PS_HW6_RTOS_COMMAND_USB_EXPORT);
}

UINT PS_HW6_RTOS_RequestUsbMscExit(void)
{
  return PS_HW6_RTOS_SendCommand(
    PS_HW6_RTOS_OWNER_STORAGE,
    PS_HW6_RTOS_COMMAND_USB_RECLAIM);
}

UINT PS_HW6_RTOS_DebugRequestUsbExport(void)
{
  return PS_HW6_RTOS_RequestUsbMscEnter();
}

UINT PS_HW6_RTOS_DebugRequestUsbReclaim(void)
{
  return PS_HW6_RTOS_RequestUsbMscExit();
}

static UINT PS_HW6_RTOS_RequestPackageInstallStub(void)
{
  return PS_HW6_RTOS_SendCommand(
    PS_HW6_RTOS_OWNER_STORAGE,
    PS_HW6_RTOS_COMMAND_PACKAGE_INSTALL_STUB);
}

static UINT PS_HW6_RTOS_RequestPackageScan(void)
{
  return PS_HW6_RTOS_SendCommand(
    PS_HW6_RTOS_OWNER_STORAGE,
    PS_HW6_RTOS_COMMAND_PACKAGE_SCAN);
}

UINT PS_HW6_RTOS_DebugRequestStorageFlashInit(void)
{
  return PS_HW6_RTOS_SendCommand(
    PS_HW6_RTOS_OWNER_STORAGE,
    PS_HW6_RTOS_COMMAND_STORAGE_FLASH_INIT);
}

UINT PS_HW6_RTOS_DebugRequestStorageAttach(void)
{
  return PS_HW6_RTOS_SendCommand(
    PS_HW6_RTOS_OWNER_STORAGE,
    PS_HW6_RTOS_COMMAND_STORAGE_ATTACH);
}

static UINT PS_HW6_RTOS_RequestStoragePackageLoadAndWait(void)
{
  ULONG actual_flags = 0UL;
  UINT status;

  g_ps_hw6_rtos_probe.runtime_package_load_request_count++;
  (void)tx_event_flags_get(
    &ps_event_groups[PS_HW6_RTOS_EVENT_DEBUG_INDEX],
    PS_HW6_RTOS_ACK_OWNER(PS_HW6_RTOS_OWNER_STORAGE),
    TX_AND_CLEAR,
    &actual_flags,
    TX_NO_WAIT);

  status = PS_HW6_RTOS_SendCommand(
    PS_HW6_RTOS_OWNER_STORAGE,
    PS_HW6_RTOS_COMMAND_STORAGE_PACKAGE_LOAD_INSTALLED);
  g_ps_hw6_rtos_probe.runtime_package_load_send_status =
    (uint32_t)status;
  if (status != TX_SUCCESS)
  {
    g_ps_hw6_rtos_probe.runtime_package_load_wait_status =
      PS_HW6_RTOS_STATUS_NOT_RUN;
    g_ps_hw6_rtos_probe.runtime_package_load_owner_status =
      PS_HW6_OWNER_SM_STATUS_NOT_RUN;
    return status;
  }

  status = tx_event_flags_get(
    &ps_event_groups[PS_HW6_RTOS_EVENT_DEBUG_INDEX],
    PS_HW6_RTOS_ACK_OWNER(PS_HW6_RTOS_OWNER_STORAGE),
    TX_AND_CLEAR,
    &actual_flags,
    PS_HW6_RTOS_STORAGE_STABILIZE_ACK_WAIT_TICKS);
  g_ps_hw6_rtos_probe.runtime_package_load_wait_status =
    (uint32_t)status;
  g_ps_hw6_rtos_probe.runtime_package_load_owner_status =
    g_ps_hw6_owner_sm_probe.persistent_load_last_status;
  if ((status == TX_SUCCESS) &&
      (g_ps_hw6_owner_sm_probe.persistent_load_last_status !=
       (uint32_t)HAL_OK))
  {
    return TX_NOT_AVAILABLE;
  }
  return status;
}

UINT PS_HW6_RTOS_DebugRequestCommBleShutdown(void)
{
  return PS_HW6_RTOS_SendModeCommand(
    PS_HW6_RTOS_OWNER_COMM,
    PS_HW6_RTOS_COMMAND_COMM_BLE_MODE,
    (uint32_t)PS_HW6_COMM_BLE_MODE_RESET_HELD);
}

UINT PS_HW6_RTOS_DebugRequestCommBleStop(void)
{
  return PS_HW6_RTOS_SendModeCommand(
    PS_HW6_RTOS_OWNER_COMM,
    PS_HW6_RTOS_COMMAND_COMM_BLE_MODE,
    (uint32_t)PS_HW6_COMM_BLE_MODE_SLEEP_SYSTEM_OFF);
}

UINT PS_HW6_RTOS_DebugRequestCommBleSearching(void)
{
  return PS_HW6_RTOS_SendModeCommand(
    PS_HW6_RTOS_OWNER_COMM,
    PS_HW6_RTOS_COMMAND_COMM_BLE_MODE,
    (uint32_t)PS_HW6_COMM_BLE_MODE_SEARCHING);
}

UINT PS_HW6_RTOS_DebugRequestCommBlePairing(void)
{
  return PS_HW6_RTOS_SendModeCommand(
    PS_HW6_RTOS_OWNER_COMM,
    PS_HW6_RTOS_COMMAND_COMM_BLE_MODE,
    (uint32_t)PS_HW6_COMM_BLE_MODE_PAIRING);
}

UINT PS_HW6_RTOS_DebugRequestCommBleConnected(void)
{
  return PS_HW6_RTOS_SendModeCommand(
    PS_HW6_RTOS_OWNER_COMM,
    PS_HW6_RTOS_COMMAND_COMM_BLE_MODE,
    (uint32_t)PS_HW6_COMM_BLE_MODE_CONNECTED);
}

UINT PS_HW6_RTOS_DebugRequestImuOff(void)
{
  return PS_HW6_RTOS_SendModeCommand(
    PS_HW6_RTOS_OWNER_SENSOR,
    PS_HW6_RTOS_COMMAND_SENSOR_IMU_MODE,
    (uint32_t)PS_HW6_IMU_MODE_OFF);
}

UINT PS_HW6_RTOS_DebugRequestImuLowRate(void)
{
  return PS_HW6_RTOS_SendModeCommand(
    PS_HW6_RTOS_OWNER_SENSOR,
    PS_HW6_RTOS_COMMAND_SENSOR_IMU_MODE,
    (uint32_t)PS_HW6_IMU_MODE_LOW_RATE_SAMPLE);
}

UINT PS_HW6_RTOS_DebugRequestImuEventArmed(void)
{
  return PS_HW6_RTOS_SendModeCommand(
    PS_HW6_RTOS_OWNER_SENSOR,
    PS_HW6_RTOS_COMMAND_SENSOR_IMU_MODE,
    (uint32_t)PS_HW6_IMU_MODE_EVENT_ARMED);
}

UINT PS_HW6_RTOS_DebugRequestImuStepCounter(void)
{
  return PS_HW6_RTOS_SendModeCommand(
    PS_HW6_RTOS_OWNER_SENSOR,
    PS_HW6_RTOS_COMMAND_SENSOR_IMU_MODE,
    (uint32_t)PS_HW6_IMU_MODE_STEP_COUNTER);
}

UINT PS_HW6_RTOS_DebugRequestImuStreaming(void)
{
  return PS_HW6_RTOS_SendModeCommand(
    PS_HW6_RTOS_OWNER_SENSOR,
    PS_HW6_RTOS_COMMAND_SENSOR_IMU_MODE,
    (uint32_t)PS_HW6_IMU_MODE_STREAMING);
}

UINT PS_HW6_RTOS_DebugRequestInputDiagnostic(void)
{
  if (g_ps_ui_router_request != 0UL)
  {
    return TX_QUEUE_FULL;
  }
  g_ps_ui_router_request_event = PS_UI_ROUTER_EVENT_NAV_INPUT_DIAGNOSTIC;
  g_ps_ui_router_request = 1UL;
  return TX_SUCCESS;
}

static void PS_HW6_RTOS_PrimeDebugCommandAnchors(void)
{
  ps_debug_usb_export_anchor = PS_HW6_RTOS_DebugRequestUsbExport;
  ps_debug_usb_reclaim_anchor = PS_HW6_RTOS_DebugRequestUsbReclaim;
  ps_debug_storage_flash_init_anchor =
    PS_HW6_RTOS_DebugRequestStorageFlashInit;
  ps_debug_storage_attach_anchor = PS_HW6_RTOS_DebugRequestStorageAttach;
  ps_debug_comm_ble_shutdown_anchor =
    PS_HW6_RTOS_DebugRequestCommBleShutdown;
  ps_debug_comm_ble_stop_anchor = PS_HW6_RTOS_DebugRequestCommBleStop;
  ps_debug_comm_ble_searching_anchor =
    PS_HW6_RTOS_DebugRequestCommBleSearching;
  ps_debug_comm_ble_pairing_anchor =
    PS_HW6_RTOS_DebugRequestCommBlePairing;
  ps_debug_comm_ble_connected_anchor =
    PS_HW6_RTOS_DebugRequestCommBleConnected;
  ps_debug_imu_off_anchor = PS_HW6_RTOS_DebugRequestImuOff;
  ps_debug_imu_low_rate_anchor = PS_HW6_RTOS_DebugRequestImuLowRate;
  ps_debug_imu_event_armed_anchor =
    PS_HW6_RTOS_DebugRequestImuEventArmed;
  ps_debug_imu_step_counter_anchor =
    PS_HW6_RTOS_DebugRequestImuStepCounter;
  ps_debug_imu_streaming_anchor = PS_HW6_RTOS_DebugRequestImuStreaming;
  ps_debug_input_diagnostic_anchor = PS_HW6_RTOS_DebugRequestInputDiagnostic;
}


static UINT PS_HW6_RTOS_SendClockProfileCommand(uint32_t requester_id,
                                                uint32_t profile,
                                                uint32_t capabilities)
{
  ULONG message[PS_HW6_RTOS_MESSAGE_WORDS];

  if ((requester_id >= PS_HW6_CLOCK_REQUESTER_COUNT) ||
      (profile > (uint32_t)PS_HW6_CLOCK_PROFILE_STOP_PREP) ||
      ((capabilities & ~PS_HW6_CLOCK_CAP_ALL) != 0UL))
  {
    return TX_QUEUE_ERROR;
  }

  message[0] = PS_HW6_RTOS_COMMAND_MAGIC;
  message[1] = PS_HW6_RTOS_OWNER_POWER;
  message[2] = PS_HW6_RTOS_COMMAND_CLOCK_PROFILE;
  message[3] = PS_HW6_RTOS_COMMAND_TOKEN ^
               (ULONG)PS_HW6_RTOS_ClockProfilePayload(requester_id,
                                                       profile,
                                                       capabilities);
  return tx_queue_send(&ps_queues[PS_HW6_RTOS_OWNER_POWER],
                       message,
                       TX_NO_WAIT);
}

static UINT PS_HW6_RTOS_RequestPowerClockProfile(uint32_t requester_id,
                                                 uint32_t profile,
                                                 uint32_t capabilities)
{
  ULONG ack_flag = PS_HW6_RTOS_ClockAckFlag(requester_id);
  ULONG actual_flags = 0UL;
  UINT send_status;
  UINT wait_status;

  if (ack_flag == 0UL)
  {
    return TX_QUEUE_ERROR;
  }

  (void)tx_event_flags_get(&ps_event_groups[PS_HW6_RTOS_EVENT_DEBUG_INDEX],
                           ack_flag,
                           TX_AND_CLEAR,
                           &actual_flags,
                           TX_NO_WAIT);

  send_status = PS_HW6_RTOS_SendClockProfileCommand(requester_id,
                                                    profile,
                                                    capabilities);
  wait_status = send_status;
  if (send_status == TX_SUCCESS)
  {
    wait_status = tx_event_flags_get(
      &ps_event_groups[PS_HW6_RTOS_EVENT_DEBUG_INDEX],
      ack_flag,
      TX_AND_CLEAR,
      &actual_flags,
      PS_HW6_RTOS_OWNER_ACK_WAIT_TICKS);
  }

  if ((send_status == TX_SUCCESS) &&
      (wait_status == TX_SUCCESS) &&
      ((actual_flags & ack_flag) != 0UL))
  {
    return (UINT)g_ps_hw6_clock_policy_probe.requester_status[requester_id];
  }

  return wait_status;
}

static void PS_HW6_RTOS_ScheduleClockReleaseStop2Recheck(
  uint32_t requester_id,
  uint32_t capabilities,
  UINT status)
{
  uint32_t now_tick;

  if ((status != TX_SUCCESS) ||
      (capabilities != 0UL) ||
      ((g_ps_hw6_rtos_probe.stop2_auto_blocker_mask &
        PS_HW6_RTOS_STOP2_BLOCK_CLOCK_CAPABILITY) == 0UL))
  {
    return;
  }

  now_tick = (uint32_t)tx_time_get();
  g_ps_hw6_rtos_probe.stop2_auto_clock_release_recheck_count++;
  g_ps_hw6_rtos_probe.stop2_auto_clock_release_recheck_tick = now_tick;
  g_ps_hw6_rtos_probe.stop2_auto_clock_release_recheck_requester =
    requester_id;
  g_ps_hw6_rtos_probe.stop2_auto_next_tick = now_tick;
}

static UINT PS_HW6_RTOS_RequestAudioClockCapabilities(
  uint32_t reason,
  uint32_t capabilities)
{
  UINT status;

  status = PS_HW6_RTOS_RequestPowerClockProfile(
    PS_HW6_RTOS_OWNER_AUDIO,
    (uint32_t)PS_HW6_CLOCK_PROFILE_UNKNOWN,
    capabilities);

  g_ps_hw6_rtos_probe.audio_clock_last_reason = reason;
  g_ps_hw6_rtos_probe.audio_clock_last_capabilities = capabilities;
  g_ps_hw6_rtos_probe.audio_clock_last_status = (uint32_t)status;

  if (capabilities == 0UL)
  {
    g_ps_hw6_rtos_probe.audio_clock_release_count++;
    g_ps_hw6_rtos_probe.audio_clock_release_status = (uint32_t)status;
  }
  else
  {
    g_ps_hw6_rtos_probe.audio_clock_request_count++;
    if (reason == PS_HW6_RTOS_AUDIO_CLOCK_REASON_REACTIVE_SFX)
    {
      g_ps_hw6_rtos_probe.audio_clock_reactive_sfx_status =
        (uint32_t)status;
    }
    else if (reason == PS_HW6_RTOS_AUDIO_CLOCK_REASON_REALTIME_MIXER)
    {
      g_ps_hw6_rtos_probe.audio_clock_realtime_status =
        (uint32_t)status;
    }
  }

  return status;
}

static void PS_HW6_RTOS_RecordStorageClockCapabilities(
  uint32_t reason,
  uint32_t capabilities,
  UINT status)
{
  g_ps_hw6_rtos_probe.storage_clock_last_reason = reason;
  g_ps_hw6_rtos_probe.storage_clock_last_capabilities = capabilities;
  g_ps_hw6_rtos_probe.storage_clock_last_status = (uint32_t)status;

  if (capabilities == 0UL)
  {
    g_ps_hw6_rtos_probe.storage_clock_release_count++;
    g_ps_hw6_rtos_probe.storage_clock_release_status = (uint32_t)status;
  }
  else
  {
    g_ps_hw6_rtos_probe.storage_clock_request_count++;
    if (reason == PS_HW6_RTOS_STORAGE_CLOCK_REASON_MSC_EXPORT)
    {
      g_ps_hw6_rtos_probe.storage_clock_export_status = (uint32_t)status;
    }
    else if (reason == PS_HW6_RTOS_STORAGE_CLOCK_REASON_MSC_RECLAIM)
    {
      g_ps_hw6_rtos_probe.storage_clock_reclaim_status = (uint32_t)status;
    }
    else if (reason == PS_HW6_RTOS_STORAGE_CLOCK_REASON_FLASH_INIT)
    {
      g_ps_hw6_rtos_probe.storage_clock_flash_init_status = (uint32_t)status;
    }
    else if (reason == PS_HW6_RTOS_STORAGE_CLOCK_REASON_ATTACH)
    {
      g_ps_hw6_rtos_probe.storage_clock_attach_status = (uint32_t)status;
    }
    else if (reason == PS_HW6_RTOS_STORAGE_CLOCK_REASON_POST_STOP_RESUME)
    {
      g_ps_hw6_rtos_probe.storage_clock_post_stop_resume_status =
        (uint32_t)status;
    }
  }
}

static UINT PS_HW6_RTOS_RequestStorageClockCapabilities(
  uint32_t reason,
  uint32_t capabilities)
{
  UINT status;

  status = PS_HW6_RTOS_RequestPowerClockProfile(
    PS_HW6_RTOS_OWNER_STORAGE,
    (uint32_t)PS_HW6_CLOCK_PROFILE_UNKNOWN,
    capabilities);

  PS_HW6_RTOS_RecordStorageClockCapabilities(reason, capabilities, status);
  return status;
}

static UINT PS_HW6_RTOS_ApplyStorageClockCapabilitiesFromPower(
  uint32_t reason,
  uint32_t capabilities)
{
  UINT status;

  status = PS_HW6_ClockPolicy_ApplyRequesterProfile(
    PS_HW6_RTOS_OWNER_STORAGE,
    (uint32_t)PS_HW6_CLOCK_PROFILE_UNKNOWN,
    capabilities);
  PS_HW6_RTOS_RecordStorageClockCapabilities(reason, capabilities, status);
  return status;
}

static uint32_t PS_HW6_RTOS_PostStopResumeNeedsStorageClock(void)
{
  return ((g_ps_hw6_owner_sm_probe.current_state[PS_HW6_SM_STORAGE] ==
           (uint32_t)STORAGE_FLASH_READY) &&
          (g_ps_hw6_owner_sm_probe.current_state[PS_HW6_SM_FLASH] ==
           (uint32_t)FLASH_DEEP_POWER_DOWN)) ? 1UL : 0UL;
}

static UINT PS_HW6_RTOS_RequestRuntimeClockCapabilities(
  uint32_t reason,
  uint32_t capabilities)
{
  UINT status;

  status = PS_HW6_RTOS_RequestPowerClockProfile(
    PS_HW6_RTOS_OWNER_RUNTIME,
    (uint32_t)PS_HW6_CLOCK_PROFILE_UNKNOWN,
    capabilities);

  g_ps_hw6_rtos_probe.runtime_clock_last_reason = reason;
  g_ps_hw6_rtos_probe.runtime_clock_last_capabilities = capabilities;
  g_ps_hw6_rtos_probe.runtime_clock_last_status = (uint32_t)status;
  if (status == TX_SUCCESS)
  {
    g_ps_hw6_rtos_probe.runtime_active_capabilities = capabilities;
  }

  if (capabilities == 0UL)
  {
    g_ps_hw6_rtos_probe.runtime_clock_release_count++;
    g_ps_hw6_rtos_probe.runtime_clock_release_status = (uint32_t)status;
  }
  else
  {
    g_ps_hw6_rtos_probe.runtime_clock_request_count++;
    if (reason == PS_HW6_RTOS_RUNTIME_CLOCK_REASON_REACTIVE_TRANSACTION)
    {
      g_ps_hw6_rtos_probe.runtime_clock_reactive_status = (uint32_t)status;
    }
    else if (reason == PS_HW6_RTOS_RUNTIME_CLOCK_REASON_REALTIME_DEADLINE)
    {
      g_ps_hw6_rtos_probe.runtime_clock_realtime_status = (uint32_t)status;
    }
  }

  return status;
}

static UINT PS_HW6_RTOS_RequestUiClockCapabilities(
  uint32_t reason,
  uint32_t capabilities)
{
  UINT status;

  status = PS_HW6_RTOS_RequestPowerClockProfile(
    PS_HW6_RTOS_OWNER_UI,
    (uint32_t)PS_HW6_CLOCK_PROFILE_UNKNOWN,
    capabilities);

  g_ps_hw6_rtos_probe.ui_clock_last_reason = reason;
  g_ps_hw6_rtos_probe.ui_clock_last_capabilities = capabilities;
  g_ps_hw6_rtos_probe.ui_clock_last_status = (uint32_t)status;

  if (capabilities == 0UL)
  {
    g_ps_hw6_rtos_probe.ui_clock_release_count++;
    g_ps_hw6_rtos_probe.ui_clock_release_status = (uint32_t)status;
  }
  else
  {
    g_ps_hw6_rtos_probe.ui_clock_request_count++;
    if (reason == PS_HW6_RTOS_UI_CLOCK_REASON_REACTIVE_TRANSACTION)
    {
      g_ps_hw6_rtos_probe.ui_clock_reactive_status = (uint32_t)status;
    }
  }

  return status;
}

static UINT PS_HW6_RTOS_RequestDisplayClockCapabilities(
  uint32_t reason,
  uint32_t capabilities)
{
  UINT status;

  status = PS_HW6_RTOS_RequestPowerClockProfile(
    PS_HW6_RTOS_OWNER_DISPLAY,
    (uint32_t)PS_HW6_CLOCK_PROFILE_UNKNOWN,
    capabilities);

  g_ps_hw6_rtos_probe.display_clock_last_reason = reason;
  g_ps_hw6_rtos_probe.display_clock_last_capabilities = capabilities;
  g_ps_hw6_rtos_probe.display_clock_last_status = (uint32_t)status;

  if (capabilities == 0UL)
  {
    g_ps_hw6_rtos_probe.display_clock_release_count++;
    g_ps_hw6_rtos_probe.display_clock_release_status = (uint32_t)status;
  }
  else
  {
    g_ps_hw6_rtos_probe.display_clock_request_count++;
    if (reason == PS_HW6_RTOS_DISPLAY_CLOCK_REASON_TRANSFER)
    {
      g_ps_hw6_rtos_probe.display_clock_transfer_status = (uint32_t)status;
    }
  }

  return status;
}

static UINT PS_HW6_RTOS_ApplyDisplayClockCapabilitiesDirect(
  uint32_t reason,
  uint32_t capabilities)
{
  UINT status;

  status = PS_HW6_ClockPolicy_ApplyRequesterProfile(
    PS_HW6_RTOS_OWNER_DISPLAY,
    (uint32_t)PS_HW6_CLOCK_PROFILE_UNKNOWN,
    capabilities);
  PS_HW6_RTOS_ScheduleClockReleaseStop2Recheck(
    PS_HW6_RTOS_OWNER_DISPLAY,
    capabilities,
    status);

  g_ps_hw6_rtos_probe.display_clock_last_reason = reason;
  g_ps_hw6_rtos_probe.display_clock_last_capabilities = capabilities;
  g_ps_hw6_rtos_probe.display_clock_last_status = (uint32_t)status;

  if (capabilities == 0UL)
  {
    g_ps_hw6_rtos_probe.display_clock_release_count++;
    g_ps_hw6_rtos_probe.display_clock_release_status = (uint32_t)status;
  }
  else
  {
    g_ps_hw6_rtos_probe.display_clock_request_count++;
    if (reason == PS_HW6_RTOS_DISPLAY_CLOCK_REASON_TRANSFER)
    {
      g_ps_hw6_rtos_probe.display_clock_transfer_status = (uint32_t)status;
    }
  }

  return status;
}
static uint32_t PS_HW6_RTOS_DisplayLegacyCursorBlinkActive(void)
{
  uint32_t primitive = g_ps_hw6_owner_probe.display_ui_primitive_id;

  return ((primitive == DISPLAY_RENDERER_PRIMITIVE_LIST_FOCUS) ||
          (primitive == DISPLAY_RENDERER_PRIMITIVE_CURSOR_BLINK)) ? 1UL : 0UL;
}

static uint32_t PS_HW6_RTOS_DisplayCursorBlinkEligible(void)
{
  uint32_t page = g_ps_ui_router_probe.current_page;
  uint32_t waiting_sequence_active =
    ((g_ps_hw6_owner_probe.display_waiting_snapshot_status ==
      (uint32_t)HAL_OK) &&
     (g_ps_hw6_owner_probe.display_waiting_sequence_frame_count != 0UL)) ?
      1UL : 0UL;

  if ((page != (uint32_t)PS_UI_ROUTER_PAGE_MENU) &&
      (page != (uint32_t)PS_UI_ROUTER_PAGE_RUNTIME_HANDOFF))
  {
    return 0UL;
  }
  if (g_ps_ui_router_request != 0UL)
  {
    return 0UL;
  }
  if (ps_display_blink_stop2_suppressed != 0UL)
  {
    return 0UL;
  }
  if ((PS_HW6_RTOS_DisplayLegacyCursorBlinkActive() == 0UL) &&
      (waiting_sequence_active == 0UL))
  {
    return 0UL;
  }
  if ((g_ps_hw6_owner_probe.display_complete == 0UL) ||
      (g_ps_hw6_owner_probe.display_success == 0UL) ||
      (g_ps_hw6_owner_probe.display_ui_status != (uint32_t)HAL_OK) ||
      (g_ps_hw6_owner_probe.display_ui_page != page) ||
      (g_ps_hw6_owner_probe.display_lpbam_prearmed != 0UL) ||
      (g_ps_hw6_owner_probe.display_lpbam_active != 0UL))
  {
    return 0UL;
  }

  return 1UL;
}

static uint32_t PS_HW6_RTOS_DisplayCursorBlinkPeriodTicks(void)
{
  uint32_t cadence_ms =
    g_ps_hw6_owner_probe.display_waiting_cadence_ms;

  if ((g_ps_hw6_owner_probe.display_waiting_snapshot_status !=
       (uint32_t)HAL_OK) || (cadence_ms == 0UL))
  {
    cadence_ms = (uint32_t)KNOB_DISPLAY_CURSOR_BLINK_PERIOD_MS;
  }
  return PS_HW6_RTOS_MsToTicks(
    cadence_ms);
}

static uint32_t PS_HW6_RTOS_PowerQuiesceNeedsStorageClock(void)
{
  return ((g_ps_hw6_owner_sm_probe.current_state[PS_HW6_SM_STORAGE] ==
           (uint32_t)STORAGE_FLASH_READY) &&
          (g_ps_hw6_owner_sm_probe.current_state[PS_HW6_SM_FLASH] ==
           (uint32_t)FLASH_READY)) ? 1UL : 0UL;
}

static void PS_HW6_RTOS_ResetDisplayCursorBlink(uint32_t now_tick)
{
  uint32_t sequence_count =
    g_ps_hw6_owner_probe.display_waiting_sequence_frame_count;
  uint32_t settled_frame =
    g_ps_hw6_owner_probe.display_waiting_settled_sequence_frame;

  if ((g_ps_hw6_owner_probe.display_waiting_snapshot_status ==
       (uint32_t)HAL_OK) &&
      (sequence_count != 0UL) &&
      (sequence_count <= PS_HW6_OWNER_LPBAM_SEQUENCE_MAX) &&
      (settled_frame < sequence_count))
  {
    ps_display_waiting_sequence_frame = settled_frame;
    ps_display_waiting_sequence_count = sequence_count;
    ps_display_blink_visible =
      g_ps_hw6_owner_probe.display_waiting_sequence_phase[settled_frame];
  }
  else
  {
    ps_display_blink_visible = 1UL;
    ps_display_waiting_sequence_frame = 0UL;
    ps_display_waiting_sequence_count = 0UL;
  }
  ps_display_blink_next_tick = now_tick +
    PS_HW6_RTOS_DisplayCursorBlinkPeriodTicks();
}

static void PS_HW6_RTOS_RequestStop2AutoCheckNow(uint32_t now_tick)
{
  UINT send_status;

  g_ps_hw6_rtos_probe.stop2_auto_next_tick = now_tick;
  g_ps_hw6_rtos_probe.stop2_lpbam_power_recheck_request_count++;
  g_ps_hw6_rtos_probe.stop2_lpbam_power_recheck_request_tick = now_tick;
  send_status = PS_HW6_RTOS_SendCommand(
    PS_HW6_RTOS_OWNER_POWER,
    PS_HW6_RTOS_COMMAND_POWER_STOP2_RECHECK);
  g_ps_hw6_rtos_probe.stop2_lpbam_power_recheck_send_status =
    (uint32_t)send_status;
}

static uint32_t PS_HW6_RTOS_DisplayLpbamEdgeRequestMatches(void)
{
  if (ps_stop2_lpbam_edge_request_pending == 0UL)
  {
    return 0UL;
  }
  if (ps_stop2_lpbam_edge_page != g_ps_ui_router_probe.current_page)
  {
    return 0UL;
  }
  if (ps_stop2_lpbam_edge_render_count !=
      g_ps_hw6_owner_probe.display_ui_render_count)
  {
    return 0UL;
  }

  return 1UL;
}

static void PS_HW6_RTOS_ClearDisplayLpbamEdgeRequest(uint32_t status)
{
  ps_stop2_lpbam_edge_request_pending = 0UL;
  ps_stop2_lpbam_edge_target_tick = 0UL;
  ps_stop2_lpbam_edge_start_phase = 0UL;
  ps_stop2_lpbam_edge_target_sequence_frame = 0UL;
  ps_stop2_lpbam_edge_render_count = 0UL;
  ps_stop2_lpbam_edge_page = 0UL;
  g_ps_hw6_rtos_probe.stop2_lpbam_edge_state =
    PS_HW6_RTOS_STOP2_LPBAM_EDGE_IDLE;
  if (status != PS_HW6_RTOS_STATUS_NOT_RUN)
  {
    g_ps_hw6_rtos_probe.stop2_lpbam_edge_status = status;
  }
}

static void PS_HW6_RTOS_ResumeDisplayBlinkAfterLpbamStop(uint32_t now_tick)
{
  PS_HW6_RTOS_ClearDisplayLpbamEdgeRequest((uint32_t)HAL_OK);
  ps_stop2_lpbam_edge_rearm_needed = 0UL;
  g_ps_hw6_rtos_probe.stop2_lpbam_edge_rearm_pending = 0UL;
  ps_display_blink_stop2_suppressed = 0UL;
  PS_HW6_RTOS_ResetDisplayCursorBlink(now_tick);
}

static HAL_StatusTypeDef PS_HW6_RTOS_ResumeDisplayTimelineAfterStop2(
  uint32_t now_tick)
{
  uint32_t period_ticks = PS_HW6_RTOS_DisplayCursorBlinkPeriodTicks();
  uint32_t period_counts =
    g_ps_hw6_owner_probe.display_lpbam_wake_lptim_period;
  uint32_t counter =
    g_ps_hw6_owner_probe.display_lpbam_wake_lptim_count;
  uint32_t remaining_counts;
  uint32_t remaining_ticks;

  if ((period_ticks == 0UL) || (period_counts == 0UL) ||
      (g_ps_hw6_owner_probe.display_lpbam_wake_snapshot_status !=
       (uint32_t)HAL_OK) ||
      (g_ps_hw6_owner_probe.display_lpbam_wake_render_status !=
       (uint32_t)HAL_OK) ||
      (g_ps_hw6_owner_probe.display_lpbam_wake_preferred_map_status !=
       (uint32_t)HAL_OK) ||
      (g_ps_hw6_owner_probe.display_lpbam_wake_preferred_sequence_count ==
       0UL) ||
      (g_ps_hw6_owner_probe.display_lpbam_wake_preferred_sequence_frame >=
       g_ps_hw6_owner_probe.display_lpbam_wake_preferred_sequence_count))
  {
    return HAL_ERROR;
  }

  if (counter >= period_counts)
  {
    counter %= period_counts;
  }
  remaining_counts = period_counts - counter;
  remaining_ticks = (uint32_t)(
    (((uint64_t)remaining_counts * (uint64_t)period_ticks) +
     (uint64_t)period_counts - 1ULL) / (uint64_t)period_counts);
  if (remaining_ticks == 0UL)
  {
    remaining_ticks = 1UL;
  }
  if (remaining_ticks > period_ticks)
  {
    remaining_ticks = period_ticks;
  }

  PS_HW6_RTOS_ClearDisplayLpbamEdgeRequest((uint32_t)HAL_OK);
  ps_stop2_lpbam_edge_rearm_needed = 0UL;
  g_ps_hw6_rtos_probe.stop2_lpbam_edge_rearm_pending = 0UL;
  ps_display_blink_stop2_suppressed = 0UL;
  ps_display_blink_visible =
    g_ps_hw6_owner_probe.display_lpbam_wake_phase;
  ps_display_waiting_sequence_frame =
    g_ps_hw6_owner_probe.display_lpbam_wake_preferred_sequence_frame;
  ps_display_waiting_sequence_count =
    g_ps_hw6_owner_probe.display_lpbam_wake_preferred_sequence_count;
  ps_display_blink_next_tick = now_tick + remaining_ticks;
  g_ps_hw6_rtos_probe.stop2_lpbam_wake_resume_count++;
  g_ps_hw6_rtos_probe.stop2_lpbam_wake_resume_phase =
    ps_display_blink_visible;
  g_ps_hw6_rtos_probe.stop2_lpbam_wake_resume_sequence_frame =
    ps_display_waiting_sequence_frame;
  g_ps_hw6_rtos_probe.stop2_lpbam_wake_resume_sequence_count =
    ps_display_waiting_sequence_count;
  g_ps_hw6_rtos_probe.stop2_lpbam_wake_resume_remaining_ticks =
    remaining_ticks;
  g_ps_hw6_rtos_probe.stop2_lpbam_wake_resume_deadline_tick =
    ps_display_blink_next_tick;
  g_ps_hw6_rtos_probe.stop2_lpbam_wake_resume_status =
    (uint32_t)HAL_OK;
  return HAL_OK;
}

static void PS_HW6_RTOS_DeferDisplayLpbamRearm(uint32_t now_tick)
{
  PS_HW6_RTOS_ClearDisplayLpbamEdgeRequest((uint32_t)HAL_ERROR);
  ps_display_blink_stop2_suppressed = 0UL;
  PS_HW6_RTOS_ResetDisplayCursorBlink(now_tick);
  ps_stop2_lpbam_edge_rearm_needed = 1UL;
  g_ps_hw6_rtos_probe.stop2_lpbam_edge_rearm_pending = 1UL;
  g_ps_hw6_rtos_probe.stop2_lpbam_edge_rearm_count++;
  g_ps_hw6_rtos_probe.stop2_lpbam_edge_rearm_tick = now_tick;
}

static uint32_t PS_HW6_RTOS_NextDisplayLpbamHandoffSequenceFrame(void)
{
  uint32_t offset;
  uint32_t sequence_count =
    g_ps_hw6_owner_probe.display_waiting_sequence_frame_count;

  if ((ps_display_waiting_sequence_count == 0UL) ||
      (ps_display_waiting_sequence_count !=
       sequence_count) ||
      (ps_display_waiting_sequence_frame >=
       ps_display_waiting_sequence_count))
  {
    return 0UL;
  }

  for (offset = 1UL;
       offset <= ps_display_waiting_sequence_count;
       ++offset)
  {
    uint32_t candidate =
      (ps_display_waiting_sequence_frame + offset) %
      ps_display_waiting_sequence_count;
    uint32_t previous =
      (candidate + ps_display_waiting_sequence_count - 1UL) %
      ps_display_waiting_sequence_count;

    if ((g_ps_hw6_owner_probe.display_waiting_sequence_phase[previous] ==
         PS_HW6_RTOS_LPBAM_HANDOFF_SOURCE_PHASE) &&
        (g_ps_hw6_owner_probe.display_waiting_sequence_phase[candidate] ==
         PS_HW6_RTOS_LPBAM_HANDOFF_TARGET_PHASE))
    {
      return candidate;
    }
  }

  return 0UL;
}

static void PS_HW6_RTOS_RequestDisplayLpbamPrepareAtBlinkEdge(
  uint32_t now_tick)
{
  uint32_t target_tick = ps_display_blink_next_tick;
  uint32_t period_ticks = PS_HW6_RTOS_DisplayCursorBlinkPeriodTicks();
  UINT wake_status = TX_SUCCESS;

  if (period_ticks == 0UL)
  {
    g_ps_hw6_rtos_probe.stop2_lpbam_edge_state =
      PS_HW6_RTOS_STOP2_LPBAM_EDGE_FAILED;
    g_ps_hw6_rtos_probe.stop2_lpbam_edge_status = (uint32_t)HAL_ERROR;
    return;
  }

  if (PS_HW6_RTOS_DisplayLpbamEdgeRequestMatches() != 0UL)
  {
    return;
  }

  if (target_tick == 0UL)
  {
    target_tick = now_tick + period_ticks;
  }
  else if (PS_HW6_RTOS_TimeReached(now_tick, target_tick) != 0UL)
  {
    target_tick = now_tick;
    g_ps_hw6_rtos_probe.stop2_lpbam_edge_miss_count++;
  }
  if (ps_display_blink_visible !=
      PS_HW6_RTOS_LPBAM_HANDOFF_SOURCE_PHASE)
  {
    target_tick += period_ticks;
  }

  ps_stop2_lpbam_edge_request_pending = 1UL;
  ps_stop2_lpbam_edge_rearm_needed = 0UL;
  g_ps_hw6_rtos_probe.stop2_lpbam_edge_rearm_pending = 0UL;
  ps_stop2_lpbam_edge_target_tick = target_tick;
  ps_stop2_lpbam_edge_start_phase = ps_display_blink_visible;
  ps_stop2_lpbam_edge_target_sequence_frame =
    PS_HW6_RTOS_NextDisplayLpbamHandoffSequenceFrame();
  ps_stop2_lpbam_edge_render_count =
    g_ps_hw6_owner_probe.display_ui_render_count;
  ps_stop2_lpbam_edge_page = g_ps_ui_router_probe.current_page;

  g_ps_hw6_rtos_probe.stop2_lpbam_edge_state =
    PS_HW6_RTOS_STOP2_LPBAM_EDGE_REQUESTED;
  g_ps_hw6_rtos_probe.stop2_lpbam_edge_request_count++;
  g_ps_hw6_rtos_probe.stop2_lpbam_edge_request_tick = now_tick;
  g_ps_hw6_rtos_probe.stop2_lpbam_edge_target_tick = target_tick;
  g_ps_hw6_rtos_probe.stop2_lpbam_edge_start_phase =
    ps_stop2_lpbam_edge_start_phase;
  g_ps_hw6_rtos_probe.stop2_lpbam_edge_start_sequence_frame =
    ps_display_waiting_sequence_frame;
  g_ps_hw6_rtos_probe.stop2_lpbam_edge_target_sequence_frame =
    ps_stop2_lpbam_edge_target_sequence_frame;
  g_ps_hw6_rtos_probe.stop2_lpbam_edge_status =
    PS_HW6_RTOS_STATUS_NOT_RUN;
  g_ps_hw6_rtos_probe.stop2_lpbam_edge_render_phase =
    (ps_stop2_lpbam_edge_start_phase == 0UL) ? 1UL : 0UL;
  g_ps_hw6_rtos_probe.stop2_lpbam_edge_render_status =
    PS_HW6_RTOS_STATUS_NOT_RUN;
  g_ps_hw6_rtos_probe.stop2_lpbam_edge_render_tick = 0UL;
  g_ps_hw6_rtos_probe.stop2_lpbam_edge_wake_send_status =
    PS_HW6_RTOS_STATUS_NOT_RUN;

  wake_status = PS_HW6_RTOS_SendCommand(
    PS_HW6_RTOS_OWNER_DISPLAY,
    PS_HW6_RTOS_COMMAND_DISPLAY_LPBAM_EDGE_WAKE);
  g_ps_hw6_rtos_probe.stop2_lpbam_edge_wake_send_status =
    (uint32_t)wake_status;
  if (wake_status != TX_SUCCESS)
  {
    PS_HW6_RTOS_DeferDisplayLpbamRearm(now_tick);
  }
}

static HAL_StatusTypeDef PS_HW6_RTOS_CompileDisplayLpbamAheadOfEdge(
  uint32_t now_tick)
{
  HAL_StatusTypeDef status;
  uint32_t period_ticks = PS_HW6_RTOS_DisplayCursorBlinkPeriodTicks();

  if ((period_ticks == 0UL) ||
      (PS_HW6_RTOS_DisplayLpbamEdgeRequestMatches() == 0UL))
  {
    return HAL_ERROR;
  }

  g_ps_hw6_rtos_probe.stop2_lpbam_prepare_request_count++;
  g_ps_hw6_rtos_probe.stop2_lpbam_prepare_last_tick = now_tick;
  g_ps_hw6_rtos_probe.stop2_lpbam_prepare_send_status = TX_SUCCESS;
  g_ps_hw6_rtos_probe.stop2_lpbam_prepare_wait_status = TX_SUCCESS;
  g_ps_hw6_rtos_probe.stop2_lpbam_prepare_ack_flags =
    PS_HW6_RTOS_ACK_OWNER(PS_HW6_RTOS_OWNER_DISPLAY);
  g_ps_hw6_rtos_probe.stop2_lpbam_prepare_owner_status =
    PS_HW6_RTOS_STATUS_NOT_RUN;
  g_ps_hw6_rtos_probe.stop2_lpbam_prepare_ready_after = 0UL;
  g_ps_hw6_rtos_probe.stop2_lpbam_prepare_display_clear_count =
    PS_HW6_RTOS_STATUS_NOT_RUN;

  status = PS_HW6_DisplayOwner_CompileLpbamStop2ForAnimationPhase(
    ps_stop2_lpbam_edge_target_sequence_frame,
    ps_stop2_lpbam_edge_target_tick + period_ticks);
  g_ps_hw6_rtos_probe.stop2_lpbam_prepare_owner_status =
    g_ps_hw6_owner_probe.display_lpbam_prepare_status;
  if (status != HAL_OK)
  {
    g_ps_hw6_rtos_probe.stop2_lpbam_edge_state =
      PS_HW6_RTOS_STOP2_LPBAM_EDGE_FAILED;
    g_ps_hw6_rtos_probe.stop2_lpbam_edge_status = (uint32_t)status;
  }
  else
  {
    ps_stop2_lpbam_edge_target_sequence_frame =
      g_ps_hw6_owner_probe.display_lpbam_sequence_start_frame;
    g_ps_hw6_rtos_probe.stop2_lpbam_edge_target_sequence_frame =
      ps_stop2_lpbam_edge_target_sequence_frame;
  }

  return status;
}

static HAL_StatusTypeDef PS_HW6_RTOS_RenderDisplayCursorBlinkPhase(
  uint32_t visible,
  uint32_t now_tick,
  uint32_t period_ticks)
{
  HAL_StatusTypeDef status;

  ps_display_blink_next_tick = now_tick + period_ticks;
  ps_display_blink_visible = visible;
  ps_display_blink_transfer_active = 1UL;
  (void)PS_HW6_RTOS_RequestDisplayClockCapabilities(
    PS_HW6_RTOS_DISPLAY_CLOCK_REASON_TRANSFER,
    PS_HW6_RTOS_DISPLAY_CLOCK_TRANSFER_CAPABILITIES);
  status = PS_HW6_DisplayOwner_RenderCursorBlink(visible);
  if (status != HAL_OK)
  {
    PS_HW6_RTOS_ResetDisplayCursorBlink(now_tick);
  }
  (void)PS_HW6_RTOS_RequestDisplayClockCapabilities(
    PS_HW6_RTOS_DISPLAY_CLOCK_REASON_RELEASE,
    0UL);
  ps_display_blink_transfer_active = 0UL;
  return status;
}

static HAL_StatusTypeDef PS_HW6_RTOS_RenderDisplayWaitingSequenceFrame(
  uint32_t sequence_frame,
  uint32_t now_tick,
  uint32_t period_ticks)
{
  HAL_StatusTypeDef status;
  uint32_t sequence_count =
    g_ps_hw6_owner_probe.display_waiting_sequence_frame_count;

  if ((sequence_count == 0UL) || (sequence_frame >= sequence_count))
  {
    return HAL_ERROR;
  }

  ps_display_blink_next_tick = now_tick + period_ticks;
  ps_display_blink_transfer_active = 1UL;
  (void)PS_HW6_RTOS_RequestDisplayClockCapabilities(
    PS_HW6_RTOS_DISPLAY_CLOCK_REASON_TRANSFER,
    PS_HW6_RTOS_DISPLAY_CLOCK_TRANSFER_CAPABILITIES);
  status = PS_HW6_DisplayOwner_RenderWaitingSequenceFrame(sequence_frame);
  if (status == HAL_OK)
  {
    ps_display_waiting_sequence_frame = sequence_frame;
    ps_display_waiting_sequence_count = sequence_count;
    ps_display_blink_visible =
      g_ps_hw6_owner_probe.display_lpbam_sequence_phase[sequence_frame];
  }
  else
  {
    PS_HW6_RTOS_ResetDisplayCursorBlink(now_tick);
  }
  (void)PS_HW6_RTOS_RequestDisplayClockCapabilities(
    PS_HW6_RTOS_DISPLAY_CLOCK_REASON_RELEASE,
    0UL);
  ps_display_blink_transfer_active = 0UL;
  return status;
}

static void PS_HW6_RTOS_SynchronizeDisplayWaitingTimelineAfterUi(
  uint32_t previous_page,
  uint32_t previous_presentation_id,
  uint32_t previous_sequence_frame,
  uint32_t previous_sequence_count,
  uint32_t previous_deadline_tick,
  uint32_t now_tick)
{
  HAL_StatusTypeDef status;
  uint32_t period_ticks = PS_HW6_RTOS_DisplayCursorBlinkPeriodTicks();
  uint32_t sequence_frame = previous_sequence_frame;
  uint32_t deadline_tick = previous_deadline_tick;

  g_ps_hw6_rtos_probe.display_waiting_preserve_status =
    PS_HW6_RTOS_STATUS_NOT_RUN;
  if ((g_ps_hw6_owner_probe.display_waiting_snapshot_status !=
       (uint32_t)HAL_OK) ||
      (previous_page != g_ps_hw6_owner_probe.display_ui_page) ||
      (previous_presentation_id !=
       g_ps_hw6_owner_probe.display_waiting_presentation_id) ||
      (previous_sequence_count == 0UL) ||
      (previous_sequence_count !=
       g_ps_hw6_owner_probe.display_waiting_sequence_frame_count) ||
      (previous_sequence_frame >= previous_sequence_count) ||
      (previous_deadline_tick == 0UL) ||
      (period_ticks == 0UL))
  {
    g_ps_hw6_rtos_probe.display_waiting_rebase_count++;
    PS_HW6_RTOS_ResetDisplayCursorBlink(now_tick);
    return;
  }

  if (PS_HW6_RTOS_TimeReached(now_tick, deadline_tick) != 0UL)
  {
    uint32_t elapsed_steps =
      1UL + ((now_tick - deadline_tick) / period_ticks);

    sequence_frame = (sequence_frame +
      (elapsed_steps % previous_sequence_count)) % previous_sequence_count;
    deadline_tick += elapsed_steps * period_ticks;
  }

  status = PS_HW6_RTOS_RenderDisplayWaitingSequenceFrame(
    sequence_frame, now_tick, period_ticks);
  g_ps_hw6_rtos_probe.display_waiting_preserve_status = (uint32_t)status;
  if (status != HAL_OK)
  {
    g_ps_hw6_rtos_probe.display_waiting_rebase_count++;
    return;
  }

  ps_display_blink_next_tick = deadline_tick;
  g_ps_hw6_rtos_probe.display_waiting_preserve_count++;
  g_ps_hw6_rtos_probe.display_waiting_preserve_frame = sequence_frame;
  g_ps_hw6_rtos_probe.display_waiting_preserve_deadline_tick = deadline_tick;
}

static HAL_StatusTypeDef PS_HW6_RTOS_RunDisplayLpbamPrepareAtBlinkEdge(
  uint32_t now_tick,
  uint32_t period_ticks)
{
  HAL_StatusTypeDef status;
  uint32_t current_visible = ps_display_blink_visible;
  uint32_t next_visible;

  if (PS_HW6_RTOS_DisplayLpbamEdgeRequestMatches() == 0UL)
  {
    PS_HW6_RTOS_ClearDisplayLpbamEdgeRequest((uint32_t)HAL_ERROR);
    return HAL_ERROR;
  }

  if ((g_ps_hw6_owner_probe.display_lpbam_sequence_frame_count == 0UL) ||
      (ps_stop2_lpbam_edge_target_sequence_frame >=
       g_ps_hw6_owner_probe.display_lpbam_sequence_frame_count))
  {
    PS_HW6_RTOS_ClearDisplayLpbamEdgeRequest((uint32_t)HAL_ERROR);
    return HAL_ERROR;
  }
  next_visible = g_ps_hw6_owner_probe.display_lpbam_sequence_phase[
    ps_stop2_lpbam_edge_target_sequence_frame];

  ps_stop2_lpbam_edge_request_pending = 0UL;
  g_ps_hw6_rtos_probe.stop2_lpbam_edge_run_count++;
  g_ps_hw6_rtos_probe.stop2_lpbam_edge_run_tick = now_tick;
  g_ps_hw6_rtos_probe.stop2_lpbam_edge_run_phase = current_visible;
  g_ps_hw6_rtos_probe.stop2_lpbam_edge_render_phase = next_visible;
  g_ps_hw6_rtos_probe.stop2_lpbam_edge_render_status =
    PS_HW6_RTOS_STATUS_NOT_RUN;
  g_ps_hw6_rtos_probe.stop2_lpbam_edge_render_tick = 0UL;
  g_ps_hw6_rtos_probe.stop2_lpbam_edge_status =
    PS_HW6_RTOS_STATUS_NOT_RUN;

  if ((current_visible != PS_HW6_RTOS_LPBAM_HANDOFF_SOURCE_PHASE) ||
      (next_visible != PS_HW6_RTOS_LPBAM_HANDOFF_TARGET_PHASE))
  {
    g_ps_hw6_rtos_probe.stop2_lpbam_edge_state =
      PS_HW6_RTOS_STOP2_LPBAM_EDGE_FAILED;
    g_ps_hw6_rtos_probe.stop2_lpbam_edge_status = (uint32_t)HAL_ERROR;
    return HAL_ERROR;
  }

  status = PS_HW6_RTOS_RenderDisplayWaitingSequenceFrame(
    ps_stop2_lpbam_edge_target_sequence_frame,
    now_tick,
    period_ticks);
  g_ps_hw6_rtos_probe.stop2_lpbam_edge_render_status = (uint32_t)status;
  g_ps_hw6_rtos_probe.stop2_lpbam_edge_render_tick =
    (uint32_t)tx_time_get();
  if (status != HAL_OK)
  {
    g_ps_hw6_rtos_probe.stop2_lpbam_edge_state =
      PS_HW6_RTOS_STOP2_LPBAM_EDGE_FAILED;
    g_ps_hw6_rtos_probe.stop2_lpbam_edge_status = (uint32_t)HAL_ERROR;
    return HAL_ERROR;
  }

  ps_display_blink_stop2_suppressed = 1UL;
  status = PS_HW6_DisplayOwner_PrearmCompiledLpbamStop2();
  g_ps_hw6_rtos_probe.stop2_lpbam_prepare_owner_status =
    (uint32_t)status;
  g_ps_hw6_rtos_probe.stop2_lpbam_prepare_ready_after =
    PS_HW6_RTOS_Stop2DisplayLpbamReady();
  if (g_ps_hw6_rtos_probe.stop2_lpbam_prepare_ready_after != 0UL)
  {
    g_ps_hw6_rtos_probe.stop2_lpbam_prepare_display_clear_count =
      g_ps_hw6_owner_probe.display_lpbam_clear_count;
  }

  if ((status == HAL_OK) &&
      (g_ps_hw6_rtos_probe.stop2_lpbam_prepare_ready_after != 0UL))
  {
    g_ps_hw6_rtos_probe.stop2_lpbam_edge_state =
      PS_HW6_RTOS_STOP2_LPBAM_EDGE_ARMED;
    g_ps_hw6_rtos_probe.stop2_lpbam_edge_status = (uint32_t)HAL_OK;
    g_ps_hw6_rtos_probe.stop2_lpbam_edge_ready_tick =
      (uint32_t)tx_time_get();
    PS_HW6_RTOS_RequestStop2AutoCheckNow(now_tick);
    return HAL_OK;
  }

  ps_display_blink_stop2_suppressed = 0UL;
  g_ps_hw6_rtos_probe.stop2_lpbam_edge_state =
    PS_HW6_RTOS_STOP2_LPBAM_EDGE_FAILED;
  g_ps_hw6_rtos_probe.stop2_lpbam_edge_status = (uint32_t)HAL_ERROR;
  return HAL_ERROR;
}

static void PS_HW6_RTOS_RunDisplayCursorBlinkPeriodic(uint32_t now_tick)
{
  uint32_t period_ticks = PS_HW6_RTOS_DisplayCursorBlinkPeriodTicks();
  uint32_t next_visible;
  uint32_t next_sequence_frame = 0UL;
  uint32_t sequence_active = 0UL;
  HAL_StatusTypeDef render_status;

  if (period_ticks == 0UL)
  {
    return;
  }
  if (PS_HW6_RTOS_DisplayCursorBlinkEligible() == 0UL)
  {
    if (ps_stop2_lpbam_edge_request_pending != 0UL)
    {
      PS_HW6_RTOS_DeferDisplayLpbamRearm(now_tick);
    }
    PS_HW6_RTOS_ResetDisplayCursorBlink(now_tick);
    return;
  }
  if (ps_display_blink_next_tick == 0UL)
  {
    PS_HW6_RTOS_ResetDisplayCursorBlink(now_tick);
    return;
  }
  if (ps_stop2_lpbam_edge_rearm_needed != 0UL)
  {
    ps_stop2_lpbam_edge_rearm_needed = 0UL;
    g_ps_hw6_rtos_probe.stop2_lpbam_edge_rearm_pending = 0UL;
    PS_HW6_RTOS_RequestStop2AutoCheckNow(now_tick);
  }
  if (PS_HW6_RTOS_TimeReached(now_tick,
                              ps_display_blink_next_tick) == 0UL)
  {
    return;
  }

  if ((ps_display_waiting_sequence_count != 0UL) &&
      (ps_display_waiting_sequence_count ==
       g_ps_hw6_owner_probe.display_waiting_sequence_frame_count) &&
      (ps_display_waiting_sequence_frame <
       ps_display_waiting_sequence_count))
  {
    sequence_active = 1UL;
    next_sequence_frame =
      (ps_display_waiting_sequence_frame + 1UL) %
      ps_display_waiting_sequence_count;
    next_visible = g_ps_hw6_owner_probe.display_waiting_sequence_phase[
      next_sequence_frame];
  }
  else
  {
    next_visible = (ps_display_blink_visible == 0UL) ? 1UL : 0UL;
  }
  if ((ps_stop2_lpbam_edge_request_pending != 0UL) &&
      (PS_HW6_RTOS_TimeReached(now_tick,
                               ps_stop2_lpbam_edge_target_tick) != 0UL))
  {
    if ((ps_display_blink_visible ==
         PS_HW6_RTOS_LPBAM_HANDOFF_SOURCE_PHASE) &&
        (next_visible == PS_HW6_RTOS_LPBAM_HANDOFF_TARGET_PHASE))
    {
      if (PS_HW6_RTOS_RunDisplayLpbamPrepareAtBlinkEdge(now_tick,
                                                         period_ticks) ==
          HAL_OK)
      {
        return;
      }
    }
    else
    {
      render_status = (sequence_active != 0UL) ?
        PS_HW6_RTOS_RenderDisplayWaitingSequenceFrame(next_sequence_frame,
                                                       now_tick,
                                                       period_ticks) :
        PS_HW6_RTOS_RenderDisplayCursorBlinkPhase(next_visible,
                                                   now_tick,
                                                   period_ticks);
      if (render_status == HAL_OK)
      {
        ps_stop2_lpbam_edge_target_tick = ps_display_blink_next_tick;
        g_ps_hw6_rtos_probe.stop2_lpbam_edge_target_tick =
          ps_stop2_lpbam_edge_target_tick;
        g_ps_hw6_rtos_probe.stop2_lpbam_edge_defer_count++;
      }
      return;
    }
  }

  if (sequence_active != 0UL)
  {
    (void)PS_HW6_RTOS_RenderDisplayWaitingSequenceFrame(next_sequence_frame,
                                                        now_tick,
                                                        period_ticks);
  }
  else
  {
    (void)PS_HW6_RTOS_RenderDisplayCursorBlinkPhase(next_visible,
                                                    now_tick,
                                                    period_ticks);
  }
}

static uint32_t PS_HW6_RTOS_StorageClockCapabilitiesActive(
  uint32_t capabilities)
{
  return ((g_ps_hw6_clock_policy_probe.requester_capabilities[
            PS_HW6_RTOS_OWNER_STORAGE] & capabilities) == capabilities) ?
         1UL : 0UL;
}

static uint32_t PS_HW6_RTOS_Stop2PmicStateAllowsStop2(uint32_t pmic_state)
{
  return ((pmic_state == (uint32_t)PMIC_MONITOR) ||
          (pmic_state == (uint32_t)PMIC_CHARGING) ||
          (pmic_state == (uint32_t)PMIC_CHARGE_DONE) ||
          (pmic_state == (uint32_t)PMIC_LOW_BATT)) ? 1UL : 0UL;
}

static uint32_t PS_HW6_RTOS_Stop2BatteryPolicyAllowsStop2(
  uint32_t battery_policy)
{
  return ((battery_policy == (uint32_t)PS_HW6_POWER_BATTERY_POLICY_BOOT_OK) ||
          (battery_policy == (uint32_t)PS_HW6_POWER_BATTERY_POLICY_OK) ||
          (battery_policy == (uint32_t)PS_HW6_POWER_BATTERY_POLICY_WARNING) ||
          (battery_policy ==
           (uint32_t)PS_HW6_POWER_BATTERY_POLICY_BOOT_CHARGE_RECOVERY)) ?
         1UL : 0UL;
}

static uint32_t PS_HW6_RTOS_Stop2DisplayLpbamReady(void)
{
  if ((g_ps_hw6_owner_probe.display_lpbam_ready == 0UL) ||
      (g_ps_hw6_owner_probe.display_lpbam_status != (uint32_t)HAL_OK))
  {
    return 0UL;
  }

  if (g_ps_hw6_owner_probe.display_lpbam_ready_page !=
      g_ps_ui_router_probe.current_page)
  {
    return 0UL;
  }

  return (g_ps_hw6_owner_probe.display_lpbam_ready_render_count ==
          g_ps_hw6_owner_probe.display_ui_render_count) ? 1UL : 0UL;
}

static uint32_t PS_HW6_RTOS_Stop2DisplayHeldFrameReady(void)
{
  if ((g_ps_hw6_owner_probe.display_complete == 0UL) ||
      (g_ps_hw6_owner_probe.display_success == 0UL) ||
      (g_ps_hw6_owner_probe.display_ui_status != (uint32_t)PS_STATUS_OK))
  {
    return 0UL;
  }

  return (g_ps_hw6_owner_probe.display_ui_page ==
          g_ps_ui_router_probe.current_page) ? 1UL : 0UL;
}

static uint32_t PS_HW6_RTOS_Stop2DisplayWaitBackendRequested(void)
{
  uint32_t requested = (uint32_t)KNOB_POWER_STOP2_DISPLAY_WAIT_BACKEND;
  uint32_t override = g_ps_hw6_power_stop2_display_backend_override;

  if (ps_stop2_lpbam_abort_late_test_active != 0UL)
  {
    return PS_HW6_RTOS_STOP2_DISPLAY_BACKEND_LPBAM;
  }

  if ((override == PS_HW6_RTOS_STOP2_DISPLAY_BACKEND_HELD_FRAME) ||
      (override == PS_HW6_RTOS_STOP2_DISPLAY_BACKEND_LPBAM))
  {
    return override;
  }

  if ((requested == PS_HW6_RTOS_STOP2_DISPLAY_BACKEND_HELD_FRAME) ||
      (requested == PS_HW6_RTOS_STOP2_DISPLAY_BACKEND_LPBAM))
  {
    return requested;
  }

  return PS_HW6_RTOS_STOP2_DISPLAY_BACKEND_NONE;
}

static uint32_t PS_HW6_RTOS_Stop2DisplayWaitBackendReady(
  uint32_t backend)
{
  if (backend == PS_HW6_RTOS_STOP2_DISPLAY_BACKEND_HELD_FRAME)
  {
    return PS_HW6_RTOS_Stop2DisplayHeldFrameReady();
  }
  if (backend == PS_HW6_RTOS_STOP2_DISPLAY_BACKEND_LPBAM)
  {
    return PS_HW6_RTOS_Stop2DisplayLpbamReady();
  }

  return 0UL;
}

static uint32_t PS_HW6_RTOS_Stop2DisplayLpbamFallbackEligible(void)
{
  if (g_ps_hw6_owner_probe.display_lpbam_prepare_status ==
      PS_HW6_OWNER_STATUS_UNAVAILABLE)
  {
    return 1UL;
  }
  return ((g_ps_hw6_owner_probe.display_lpbam_prepare_status ==
           (uint32_t)HAL_ERROR) &&
          (g_ps_hw6_owner_probe.display_lpbam_admission_status ==
           (uint32_t)HAL_ERROR) &&
          (g_ps_hw6_owner_probe.display_lpbam_admission_reason !=
           PS_LPBAM_ADMISSION_REASON_NONE)) ? 1UL : 0UL;
}

static uint32_t PS_HW6_RTOS_Stop2ResolveDisplayWaitBackend(void)
{
  uint32_t requested = PS_HW6_RTOS_Stop2DisplayWaitBackendRequested();
  uint32_t selected = requested;
  uint32_t held_ready = PS_HW6_RTOS_Stop2DisplayHeldFrameReady();
  uint32_t ready = PS_HW6_RTOS_Stop2DisplayWaitBackendReady(selected);
  uint32_t status = (uint32_t)HAL_ERROR;

  if ((selected != PS_HW6_RTOS_STOP2_DISPLAY_BACKEND_HELD_FRAME) &&
      (selected != PS_HW6_RTOS_STOP2_DISPLAY_BACKEND_LPBAM))
  {
    selected = PS_HW6_RTOS_STOP2_DISPLAY_BACKEND_NONE;
    ready = 0UL;
  }

  if ((selected == PS_HW6_RTOS_STOP2_DISPLAY_BACKEND_LPBAM) &&
      (ready == 0UL) && (held_ready != 0UL) &&
      (PS_HW6_RTOS_Stop2DisplayLpbamFallbackEligible() != 0UL))
  {
    selected = PS_HW6_RTOS_STOP2_DISPLAY_BACKEND_HELD_FRAME;
    ready = held_ready;
  }

  if (ready != 0UL)
  {
    status = (uint32_t)HAL_OK;
  }

  g_ps_hw6_rtos_probe.stop2_display_wait_backend_requested = requested;
  g_ps_hw6_rtos_probe.stop2_display_wait_backend_selected = selected;
  g_ps_hw6_rtos_probe.stop2_display_wait_backend_status = status;
  g_ps_hw6_rtos_probe.stop2_display_wait_backend_held_ready = held_ready;

  return selected;
}
static HAL_StatusTypeDef PS_HW6_RTOS_RequestDisplayCursorVisibleForStop2(void)
{
  const ULONG expected_ack = PS_HW6_RTOS_ACK_OWNER(
    PS_HW6_RTOS_OWNER_DISPLAY);
  ULONG actual_flags = 0UL;
  UINT clock_status;
  UINT send_status;
  UINT wait_status;

  ps_display_blink_stop2_suppressed = 1UL;
  ps_display_blink_visible = 1UL;
  ps_display_blink_next_tick = 0UL;
  g_ps_hw6_rtos_probe.stop2_blink_handoff_request_count++;
  g_ps_hw6_rtos_probe.stop2_blink_handoff_last_tick =
    (uint32_t)tx_time_get();
  g_ps_hw6_rtos_probe.stop2_blink_handoff_send_status =
    PS_HW6_RTOS_STATUS_NOT_RUN;
  g_ps_hw6_rtos_probe.stop2_blink_handoff_wait_status =
    PS_HW6_RTOS_STATUS_NOT_RUN;
  g_ps_hw6_rtos_probe.stop2_blink_handoff_ack_flags = 0UL;
  g_ps_hw6_rtos_probe.stop2_blink_handoff_owner_status =
    PS_HW6_RTOS_STATUS_NOT_RUN;
  g_ps_hw6_rtos_probe.stop2_blink_handoff_status =
    PS_HW6_RTOS_STATUS_NOT_RUN;

  (void)tx_event_flags_get(&ps_event_groups[PS_HW6_RTOS_EVENT_DEBUG_INDEX],
                           expected_ack,
                           TX_AND_CLEAR,
                           &actual_flags,
                           TX_NO_WAIT);

  clock_status = PS_HW6_RTOS_ApplyDisplayClockCapabilitiesDirect(
    PS_HW6_RTOS_DISPLAY_CLOCK_REASON_TRANSFER,
    PS_HW6_RTOS_DISPLAY_CLOCK_TRANSFER_CAPABILITIES);
  send_status = clock_status;
  wait_status = clock_status;
  if (clock_status == TX_SUCCESS)
  {
    send_status = PS_HW6_RTOS_SendCommand(
      PS_HW6_RTOS_OWNER_DISPLAY,
      PS_HW6_RTOS_COMMAND_DISPLAY_CURSOR_VISIBLE);
    wait_status = send_status;
    if (send_status == TX_SUCCESS)
    {
      wait_status = tx_event_flags_get(
        &ps_event_groups[PS_HW6_RTOS_EVENT_DEBUG_INDEX],
        expected_ack,
        TX_AND_CLEAR,
        &actual_flags,
        PS_HW6_RTOS_OWNER_ACK_WAIT_TICKS);
    }
  }

  (void)PS_HW6_RTOS_ApplyDisplayClockCapabilitiesDirect(
    PS_HW6_RTOS_DISPLAY_CLOCK_REASON_RELEASE,
    0UL);

  g_ps_hw6_rtos_probe.stop2_blink_handoff_send_status =
    (uint32_t)send_status;
  g_ps_hw6_rtos_probe.stop2_blink_handoff_wait_status =
    (uint32_t)wait_status;
  g_ps_hw6_rtos_probe.stop2_blink_handoff_ack_flags =
    (uint32_t)actual_flags;
  g_ps_hw6_rtos_probe.stop2_blink_handoff_owner_status =
    g_ps_hw6_owner_probe.display_blink_status;

  if ((clock_status == TX_SUCCESS) &&
      (send_status == TX_SUCCESS) &&
      (wait_status == TX_SUCCESS) &&
      ((actual_flags & expected_ack) != 0UL) &&
      (g_ps_hw6_owner_probe.display_blink_status == (uint32_t)HAL_OK))
  {
    g_ps_hw6_rtos_probe.stop2_blink_handoff_status = (uint32_t)HAL_OK;
    return HAL_OK;
  }

  ps_display_blink_stop2_suppressed = 0UL;
  g_ps_hw6_rtos_probe.stop2_blink_handoff_status = (uint32_t)HAL_ERROR;
  return HAL_ERROR;
}

static HAL_StatusTypeDef PS_HW6_RTOS_RequestDisplayLpbamPrepare(void)
{
  const ULONG expected_ack = PS_HW6_RTOS_ACK_OWNER(
    PS_HW6_RTOS_OWNER_DISPLAY);
  ULONG actual_flags = 0UL;
  UINT send_status;
  UINT wait_status;

  g_ps_hw6_rtos_probe.stop2_lpbam_prepare_request_count++;
  g_ps_hw6_rtos_probe.stop2_lpbam_prepare_last_tick =
    (uint32_t)tx_time_get();
  g_ps_hw6_rtos_probe.stop2_lpbam_prepare_send_status =
    PS_HW6_RTOS_STATUS_NOT_RUN;
  g_ps_hw6_rtos_probe.stop2_lpbam_prepare_wait_status =
    PS_HW6_RTOS_STATUS_NOT_RUN;
  g_ps_hw6_rtos_probe.stop2_lpbam_prepare_ack_flags = 0UL;
  g_ps_hw6_rtos_probe.stop2_lpbam_prepare_owner_status =
    PS_HW6_RTOS_STATUS_NOT_RUN;
  g_ps_hw6_rtos_probe.stop2_lpbam_prepare_ready_after = 0UL;
  g_ps_hw6_rtos_probe.stop2_lpbam_prepare_display_clear_count =
    PS_HW6_RTOS_STATUS_NOT_RUN;

  (void)tx_event_flags_get(&ps_event_groups[PS_HW6_RTOS_EVENT_DEBUG_INDEX],
                           expected_ack,
                           TX_AND_CLEAR,
                           &actual_flags,
                           TX_NO_WAIT);

  send_status = PS_HW6_RTOS_SendCommand(
    PS_HW6_RTOS_OWNER_DISPLAY,
    PS_HW6_RTOS_COMMAND_DISPLAY_LPBAM_PREPARE);
  wait_status = send_status;
  if (send_status == TX_SUCCESS)
  {
    wait_status = tx_event_flags_get(
      &ps_event_groups[PS_HW6_RTOS_EVENT_DEBUG_INDEX],
      expected_ack,
      TX_AND_CLEAR,
      &actual_flags,
      PS_HW6_RTOS_OWNER_ACK_WAIT_TICKS);
  }

  g_ps_hw6_rtos_probe.stop2_lpbam_prepare_send_status =
    (uint32_t)send_status;
  g_ps_hw6_rtos_probe.stop2_lpbam_prepare_wait_status =
    (uint32_t)wait_status;
  g_ps_hw6_rtos_probe.stop2_lpbam_prepare_ack_flags =
    (uint32_t)actual_flags;
  g_ps_hw6_rtos_probe.stop2_lpbam_prepare_owner_status =
    g_ps_hw6_owner_probe.display_lpbam_prepare_status;
  g_ps_hw6_rtos_probe.stop2_lpbam_prepare_ready_after =
    PS_HW6_RTOS_Stop2DisplayLpbamReady();
  if ((send_status == TX_SUCCESS) &&
      (wait_status == TX_SUCCESS) &&
      ((actual_flags & expected_ack) != 0UL))
  {
    g_ps_hw6_rtos_probe.stop2_lpbam_prepare_display_clear_count =
      g_ps_hw6_owner_probe.display_lpbam_clear_count;
  }

  if ((send_status == TX_SUCCESS) &&
      (wait_status == TX_SUCCESS) &&
      ((actual_flags & expected_ack) != 0UL) &&
      (g_ps_hw6_rtos_probe.stop2_lpbam_prepare_ready_after != 0UL))
  {
    return HAL_OK;
  }

  return HAL_ERROR;
}

static HAL_StatusTypeDef PS_HW6_RTOS_RequestDisplayLpbamAbort(
  uint32_t resume_stop2_timeline)
{
  const ULONG expected_ack = PS_HW6_RTOS_ACK_OWNER(
    PS_HW6_RTOS_OWNER_DISPLAY);
  ULONG actual_flags = 0UL;
  UINT clock_status;
  UINT send_status;
  UINT wait_status;

  g_ps_hw6_rtos_probe.stop2_lpbam_abort_request_count++;
  g_ps_hw6_rtos_probe.stop2_lpbam_abort_last_tick =
    (uint32_t)tx_time_get();
  g_ps_hw6_rtos_probe.stop2_lpbam_abort_send_status =
    PS_HW6_RTOS_STATUS_NOT_RUN;
  g_ps_hw6_rtos_probe.stop2_lpbam_abort_wait_status =
    PS_HW6_RTOS_STATUS_NOT_RUN;
  g_ps_hw6_rtos_probe.stop2_lpbam_abort_ack_flags = 0UL;
  g_ps_hw6_rtos_probe.stop2_lpbam_abort_owner_status =
    PS_HW6_RTOS_STATUS_NOT_RUN;
  g_ps_hw6_rtos_probe.stop2_lpbam_abort_resume_phase = 1UL;
  g_ps_hw6_rtos_probe.stop2_lpbam_abort_resume_status =
    PS_HW6_RTOS_STATUS_NOT_RUN;
  g_ps_hw6_rtos_probe.stop2_lpbam_abort_resume_tick = 0UL;
  if (resume_stop2_timeline != 0UL)
  {
    g_ps_hw6_rtos_probe.stop2_lpbam_wake_resume_status =
      PS_HW6_RTOS_STATUS_NOT_RUN;
  }

  (void)tx_event_flags_get(&ps_event_groups[PS_HW6_RTOS_EVENT_DEBUG_INDEX],
                           expected_ack,
                           TX_AND_CLEAR,
                           &actual_flags,
                           TX_NO_WAIT);

  clock_status = PS_HW6_RTOS_ApplyDisplayClockCapabilitiesDirect(
    PS_HW6_RTOS_DISPLAY_CLOCK_REASON_TRANSFER,
    PS_HW6_RTOS_DISPLAY_CLOCK_TRANSFER_CAPABILITIES);
  send_status = clock_status;
  wait_status = clock_status;
  if (clock_status == TX_SUCCESS)
  {
    send_status = PS_HW6_RTOS_SendCommand(
      PS_HW6_RTOS_OWNER_DISPLAY,
      (resume_stop2_timeline != 0UL) ?
        PS_HW6_RTOS_COMMAND_DISPLAY_LPBAM_WAKE_ABORT :
        PS_HW6_RTOS_COMMAND_DISPLAY_LPBAM_ABORT);
    wait_status = send_status;
    if (send_status == TX_SUCCESS)
    {
      wait_status = tx_event_flags_get(
        &ps_event_groups[PS_HW6_RTOS_EVENT_DEBUG_INDEX],
        expected_ack,
        TX_AND_CLEAR,
        &actual_flags,
        PS_HW6_RTOS_OWNER_ACK_WAIT_TICKS);
    }
  }

  (void)PS_HW6_RTOS_ApplyDisplayClockCapabilitiesDirect(
    PS_HW6_RTOS_DISPLAY_CLOCK_REASON_RELEASE,
    0UL);

  g_ps_hw6_rtos_probe.stop2_lpbam_abort_send_status =
    (uint32_t)send_status;
  g_ps_hw6_rtos_probe.stop2_lpbam_abort_wait_status =
    (uint32_t)wait_status;
  g_ps_hw6_rtos_probe.stop2_lpbam_abort_ack_flags =
    (uint32_t)actual_flags;
  g_ps_hw6_rtos_probe.stop2_lpbam_abort_owner_status =
    g_ps_hw6_owner_probe.display_lpbam_abort_status;

  if ((clock_status == TX_SUCCESS) &&
      (send_status == TX_SUCCESS) &&
      (wait_status == TX_SUCCESS) &&
      ((actual_flags & expected_ack) != 0UL) &&
      (g_ps_hw6_owner_probe.display_lpbam_abort_status ==
       (uint32_t)HAL_OK) &&
      (g_ps_hw6_rtos_probe.stop2_lpbam_abort_resume_status ==
       (uint32_t)HAL_OK))
  {
    if (resume_stop2_timeline != 0UL)
    {
      return PS_HW6_RTOS_ResumeDisplayTimelineAfterStop2(
        (uint32_t)tx_time_get());
    }
    PS_HW6_RTOS_ResumeDisplayBlinkAfterLpbamStop(
      (uint32_t)tx_time_get());
    return HAL_OK;
  }

  return HAL_ERROR;
}

static uint32_t PS_HW6_RTOS_Stop2LpbamPrepareFresh(void)
{
  if (g_ps_hw6_rtos_probe.stop2_lpbam_prepare_display_clear_count ==
      PS_HW6_RTOS_STATUS_NOT_RUN)
  {
    return 0UL;
  }

  return (g_ps_hw6_rtos_probe.stop2_lpbam_prepare_display_clear_count ==
          g_ps_hw6_owner_probe.display_lpbam_clear_count) ? 1UL : 0UL;
}

static HAL_StatusTypeDef PS_HW6_RTOS_RunStop2EligibilityDryRun(void)
{
  uint32_t blocker_mask = 0UL;
  uint32_t pending_mask = 0UL;
  uint32_t power_state;
  uint32_t pmic_state;
  uint32_t battery_policy;
  uint32_t clock_capabilities;
  uint32_t clock_domains;
  uint32_t readback_domains;

  PS_HW6_ClockPolicy_RecordHardwareSnapshot();

  power_state =
    g_ps_hw6_owner_sm_probe.current_state[PS_HW6_SM_POWER];
  pmic_state =
    g_ps_hw6_owner_sm_probe.current_state[PS_HW6_SM_PMIC];
  battery_policy = g_ps_hw6_owner_sm_probe.battery_policy_state;
  clock_capabilities = g_ps_hw6_clock_policy_probe.stop2_blocker_capabilities;
  clock_domains = g_ps_hw6_clock_policy_probe.stop2_blocker_domain_mask;
  readback_domains = g_ps_hw6_clock_policy_probe.readback_domain_mask;
  g_ps_hw6_rtos_probe.stop2_eligibility_idle_peripheral_park_ready =
    PS_HW6_OwnerStateMachines_Stop2IdlePeripheralsReady();

  if ((ps_power_boot_done == 0UL) ||
      (g_ps_hw6_rtos_probe.runtime_complete == 0UL))
  {
    blocker_mask |= PS_HW6_RTOS_STOP2_BLOCK_BOOT_NOT_READY;
  }
  if (power_state != (uint32_t)PWR_ACTIVE_LP)
  {
    blocker_mask |= PS_HW6_RTOS_STOP2_BLOCK_POWER_STATE;
  }
  if (PS_HW6_RTOS_Stop2PmicStateAllowsStop2(pmic_state) == 0UL)
  {
    blocker_mask |= PS_HW6_RTOS_STOP2_BLOCK_PMIC_STATE;
  }
  if (PS_HW6_RTOS_Stop2BatteryPolicyAllowsStop2(battery_policy) == 0UL)
  {
    blocker_mask |= PS_HW6_RTOS_STOP2_BLOCK_BATTERY_POLICY;
  }
  if (clock_capabilities != 0UL)
  {
    blocker_mask |= PS_HW6_RTOS_STOP2_BLOCK_CLOCK_CAPABILITY;
  }
  if (readback_domains != 0UL)
  {
    blocker_mask |= PS_HW6_RTOS_STOP2_BLOCK_CLOCK_READBACK_DOMAIN;
  }
  if (((ps_power_boot_done != 0UL) &&
       (g_ps_hw6_rtos_probe.runtime_complete != 0UL)) &&
      ((ps_power_boot_idle_peripheral_park_done == 0UL) ||
       (g_ps_hw6_rtos_probe.stop2_eligibility_idle_peripheral_park_ready == 0UL)))
  {
    blocker_mask |= PS_HW6_RTOS_STOP2_BLOCK_IDLE_PERIPH_NOT_PARKED;
  }

  if (blocker_mask == 0UL)
  {
    uint32_t display_backend = PS_HW6_RTOS_Stop2ResolveDisplayWaitBackend();

    if (display_backend == PS_HW6_RTOS_STOP2_DISPLAY_BACKEND_LPBAM)
    {
      if (PS_HW6_RTOS_Stop2DisplayWaitBackendReady(display_backend) == 0UL)
      {
        pending_mask |= PS_HW6_RTOS_STOP2_PENDING_LPBAM_VALIDATION;
      }
    }
    else if ((display_backend != PS_HW6_RTOS_STOP2_DISPLAY_BACKEND_HELD_FRAME) ||
             (PS_HW6_RTOS_Stop2DisplayWaitBackendReady(display_backend) == 0UL))
    {
      blocker_mask |= PS_HW6_RTOS_STOP2_BLOCK_DISPLAY_PENDING;
    }
  }

  if (blocker_mask == 0UL)
  {
    pending_mask |= PS_HW6_RTOS_STOP2_PENDING_OWNER_QUIESCE;
  }

  g_ps_hw6_rtos_probe.stop2_eligibility_request_count++;
  g_ps_hw6_rtos_probe.stop2_eligibility_last_tick =
    (uint32_t)tx_time_get();
  g_ps_hw6_rtos_probe.stop2_eligibility_ready =
    (blocker_mask == 0UL) ? 1UL : 0UL;
  g_ps_hw6_rtos_probe.stop2_eligibility_blocker_mask = blocker_mask;
  g_ps_hw6_rtos_probe.stop2_eligibility_pending_mask = pending_mask;
  g_ps_hw6_rtos_probe.stop2_eligibility_clock_capabilities =
    clock_capabilities;
  g_ps_hw6_rtos_probe.stop2_eligibility_clock_domains = clock_domains;
  g_ps_hw6_rtos_probe.stop2_eligibility_readback_domains =
    readback_domains;
  g_ps_hw6_rtos_probe.stop2_eligibility_lpbam_ready =
    PS_HW6_RTOS_Stop2DisplayLpbamReady();
  g_ps_hw6_rtos_probe.stop2_eligibility_power_state = power_state;
  g_ps_hw6_rtos_probe.stop2_eligibility_pmic_state = pmic_state;
  g_ps_hw6_rtos_probe.stop2_eligibility_battery_policy = battery_policy;
  g_ps_hw6_rtos_probe.stop2_eligibility_runtime_class =
    g_ps_hw6_rtos_probe.runtime_current_class;
  g_ps_hw6_rtos_probe.stop2_eligibility_runtime_execution =
    g_ps_hw6_rtos_probe.runtime_execution;
  g_ps_hw6_rtos_probe.stop2_eligibility_runtime_lifecycle =
    g_ps_hw6_rtos_probe.runtime_lifecycle;
  g_ps_hw6_rtos_probe.stop2_eligibility_last_status =
    (blocker_mask == 0UL) ? (uint32_t)HAL_OK : (uint32_t)HAL_ERROR;

  return (blocker_mask == 0UL) ? HAL_OK : HAL_ERROR;
}

static HAL_StatusTypeDef PS_HW6_RTOS_RunStop2ControlledEntry(void)
{
  HAL_StatusTypeDef eligibility_status;
  HAL_StatusTypeDef entry_status;
  uint32_t stop2_count_before;

  g_ps_hw6_rtos_probe.stop2_control_request_count++;
  g_ps_hw6_rtos_probe.stop2_control_last_tick = (uint32_t)tx_time_get();
  g_ps_hw6_rtos_probe.stop2_control_last_status =
    PS_HW6_RTOS_STATUS_NOT_RUN;
  g_ps_hw6_rtos_probe.stop2_control_eligibility_status =
    PS_HW6_RTOS_STATUS_NOT_RUN;
  g_ps_hw6_rtos_probe.stop2_control_entry_status =
    PS_HW6_RTOS_STATUS_NOT_RUN;
  eligibility_status = PS_HW6_RTOS_RunStop2EligibilityDryRun();
  g_ps_hw6_rtos_probe.stop2_control_eligibility_status =
    (uint32_t)eligibility_status;
  g_ps_hw6_rtos_probe.stop2_control_eligibility_blocker_mask =
    g_ps_hw6_rtos_probe.stop2_eligibility_blocker_mask;
  g_ps_hw6_rtos_probe.stop2_control_eligibility_pending_mask =
    g_ps_hw6_rtos_probe.stop2_eligibility_pending_mask;

  stop2_count_before = g_ps_hw6_owner_sm_probe.stop2_request_count;
  g_ps_hw6_rtos_probe.stop2_control_stop2_count_before =
    stop2_count_before;
  g_ps_hw6_rtos_probe.stop2_control_stop2_count_after =
    stop2_count_before;

  if (eligibility_status != HAL_OK)
  {
    g_ps_hw6_rtos_probe.stop2_control_last_status =
      (uint32_t)HAL_ERROR;
    return HAL_ERROR;
  }

  g_ps_hw6_rtos_probe.stop2_control_entry_attempt_count++;
  entry_status = PS_HW6_OwnerStateMachines_RunStop2StartWakeScaffold();
  g_ps_hw6_rtos_probe.stop2_control_entry_status =
    (uint32_t)entry_status;
  g_ps_hw6_rtos_probe.stop2_control_stop2_count_after =
    g_ps_hw6_owner_sm_probe.stop2_request_count;
  g_ps_hw6_rtos_probe.stop2_control_last_status =
    (uint32_t)entry_status;
  return entry_status;
}

static uint32_t PS_HW6_RTOS_Stop2AutoQueuePendingMask(void)
{
  uint32_t owner_id;
  uint32_t pending_mask = 0UL;

  for (owner_id = 0UL; owner_id < PS_HW6_RTOS_QUEUE_COUNT; ++owner_id)
  {
    ULONG enqueued = 0UL;
    UINT status = tx_queue_info_get(&ps_queues[owner_id], TX_NULL,
                                    &enqueued, TX_NULL, TX_NULL,
                                    TX_NULL, TX_NULL);

    if ((status != TX_SUCCESS) || (enqueued != 0UL))
    {
      pending_mask |= PS_HW6_RTOS_ACK_OWNER(owner_id);
    }
  }

  return pending_mask;
}

uint32_t PS_HW6_RTOS_Stop2FinalInputReady(void)
{
  uint32_t owner_id;
  uint32_t primask;
  uint32_t queue_mask = 0UL;
  uint32_t ready;

  primask = __get_PRIMASK();
  __disable_irq();
  g_ps_hw6_rtos_probe.stop2_final_input_check_count++;
  g_ps_hw6_rtos_probe.stop2_final_input_enqueue_count =
    g_ps_hw6_rtos_probe.input_raw_enqueue_count;
  g_ps_hw6_rtos_probe.stop2_final_input_dequeue_count =
    g_ps_hw6_rtos_probe.input_raw_dequeue_count;

  for (owner_id = 0UL; owner_id < PS_HW6_RTOS_QUEUE_COUNT; ++owner_id)
  {
    if (ps_queues[owner_id].tx_queue_enqueued != 0U)
    {
      queue_mask |= PS_HW6_RTOS_ACK_OWNER(owner_id);
    }
  }

  g_ps_hw6_rtos_probe.stop2_final_input_queue_mask = queue_mask;
  g_ps_hw6_rtos_probe.stop2_final_input_gpioa_idr = GPIOA->IDR;
  g_ps_hw6_rtos_probe.stop2_final_input_gpiob_idr = GPIOB->IDR;
  g_ps_hw6_rtos_probe.stop2_final_input_gpioc_idr = GPIOC->IDR;
  g_ps_hw6_rtos_probe.stop2_final_joystick_pending_count =
    ps_joystick_int_pending_count - ps_joystick_int_consumed_count;
  ready = ((g_ps_hw6_rtos_probe.input_raw_enqueue_count ==
            g_ps_hw6_rtos_probe.input_raw_dequeue_count) &&
           (ps_joystick_int_pending_count ==
            ps_joystick_int_consumed_count) &&
           (g_ps_hw6_joystick_cardinal_request == 0UL) &&
           ((KNOB_INPUT_JOYSTICK_STOP2_WAKE_ENABLE == 0) ||
            (PS_HW6_OwnerStateMachines_JoystickStop2WakeAllowed() == 0UL) ||
            ((g_ps_hw6_rtos_probe.stop2_final_input_gpioc_idr &
              (uint32_t)JOY_INT_Pin) != 0UL)) &&
           (queue_mask == 0UL) &&
           (PS_InputButtons_Stop2Ready() != 0UL)) ? 1UL : 0UL;
  g_ps_hw6_rtos_probe.stop2_final_input_last_status =
    (ready != 0UL) ? (uint32_t)HAL_OK : (uint32_t)HAL_ERROR;
  if (ready == 0UL)
  {
    g_ps_hw6_rtos_probe.stop2_final_input_veto_count++;
  }

  if (primask == 0UL)
  {
    __enable_irq();
  }
  return ready;
}

static uint32_t PS_HW6_RTOS_Stop2AutoRuntimeAllowsIdle(void)
{
  uint32_t runtime_class = g_ps_hw6_rtos_probe.runtime_current_class;
  uint32_t runtime_exec = g_ps_hw6_rtos_probe.runtime_execution;
  uint32_t runtime_lifecycle = g_ps_hw6_rtos_probe.runtime_lifecycle;

  if ((runtime_class == (uint32_t)PS_HW6_RUNTIME_CLASS_SHELL) &&
      (runtime_exec == (uint32_t)PS_HW6_RUNTIME_EXEC_REACTIVE) &&
      (runtime_lifecycle == (uint32_t)PS_HW6_RUNTIME_LIFECYCLE_RUNNING))
  {
    return 1UL;
  }

  if ((runtime_class == (uint32_t)PS_HW6_RUNTIME_CLASS_LP_GRAPH) &&
      (runtime_exec == (uint32_t)PS_HW6_RUNTIME_EXEC_REACTIVE) &&
      (runtime_lifecycle == (uint32_t)PS_HW6_RUNTIME_LIFECYCLE_RUNNING) &&
      (g_ps_hw6_rtos_probe.runtime_active_capabilities == 0UL) &&
      (PS_SceneRuntime_StateSceneActive() != 0UL))
  {
    return 1UL;
  }

  if ((runtime_lifecycle == (uint32_t)PS_HW6_RUNTIME_LIFECYCLE_SUSPENDED) &&
      (g_ps_hw6_rtos_probe.runtime_active_capabilities == 0UL))
  {
    return 1UL;
  }

  return 0UL;
}

static uint32_t PS_HW6_RTOS_Stop2AutoUiAllowsIdle(void)
{
  uint32_t page = g_ps_ui_router_probe.current_page;
  uint32_t nav = g_ps_ui_router_probe.nav_state;

  if ((g_ps_ui_router_request != 0UL) ||
      (g_ps_ui_router_probe.pending_action !=
       (uint32_t)PS_UI_ROUTER_ACTION_NONE) ||
      (g_ps_ui_router_probe.modal_state !=
       (uint32_t)PS_UI_ROUTER_MODAL_NONE) ||
      (g_ps_ui_router_probe.shutdown_state !=
       (uint32_t)PS_UI_ROUTER_SHUTDOWN_NONE) ||
      (PS_HW6_RTOS_SystemOverlayActive() != 0UL))
  {
    return 0UL;
  }

  if ((nav != (uint32_t)PS_UI_ROUTER_NAV_IDLE) &&
      (nav != (uint32_t)PS_UI_ROUTER_NAV_FOCUS))
  {
    return 0UL;
  }

  return ((page == (uint32_t)PS_UI_ROUTER_PAGE_HOME) ||
          (page == (uint32_t)PS_UI_ROUTER_PAGE_MENU) ||
          (page == (uint32_t)PS_UI_ROUTER_PAGE_SETTINGS) ||
          (page == (uint32_t)PS_UI_ROUTER_PAGE_PACKAGE_BROWSER) ||
          (page == (uint32_t)PS_UI_ROUTER_PAGE_RUNTIME_HANDOFF)) ?
         1UL : 0UL;
}

static uint32_t PS_HW6_RTOS_Stop2AutoDisplayAllowsIdle(void)
{
  return PS_HW6_RTOS_Stop2DisplayHeldFrameReady();
}

static uint32_t PS_HW6_RTOS_Stop2AutoStorageAllowsIdle(void)
{
  if ((g_ps_hw6_storage_usb_export_request != 0UL) ||
      (g_ps_hw6_storage_usb_reclaim_request != 0UL) ||
      (g_ps_hw6_storage_persistent_install_request != 0UL) ||
      ((g_ps_hw6_owner_sm_probe.usb_host_msc_active != 0UL) ||
       (g_ps_storage_msc_bridge_probe.export_enabled != 0UL)) ||
      (g_ps_ui_router_probe.package_state ==
       (uint32_t)PS_UI_ROUTER_PACKAGE_INSTALLING))
  {
    return 0UL;
  }

  return 1UL;
}

static uint32_t PS_HW6_RTOS_Stop2AutoInputAllowsIdle(void)
{
  uint32_t routed_event_count =
    g_ps_input_buttons_probe.logical_event_count +
    g_ps_hw6_rtos_probe.runtime_interaction_package_start_count;

  if ((g_ps_hw6_rtos_probe.input_raw_enqueue_count !=
       g_ps_hw6_rtos_probe.input_raw_dequeue_count) ||
      (PS_InputButtons_Stop2Ready() == 0UL) ||
      (g_ps_input_buttons_probe.pending_mask != 0UL) ||
      (g_ps_input_buttons_probe.start_active != 0UL) ||
      (g_ps_input_buttons_probe.start_pending_event != 0UL) ||
      (routed_event_count !=
       g_ps_hw6_rtos_probe.input_policy_event_count))
  {
    return 0UL;
  }

  return 1UL;
}

static uint32_t PS_HW6_RTOS_Stop2AutoDynamicBlockerMask(
  uint32_t *queue_pending_mask)
{
  uint32_t blocker_mask = 0UL;
  uint32_t queue_mask = PS_HW6_RTOS_Stop2AutoQueuePendingMask();

  if (PS_HW6_RTOS_Stop2AutoRuntimeAllowsIdle() == 0UL)
  {
    blocker_mask |= PS_HW6_RTOS_STOP2_BLOCK_RUNTIME_BUSY;
  }
  if (PS_HW6_RTOS_Stop2AutoUiAllowsIdle() == 0UL)
  {
    blocker_mask |= PS_HW6_RTOS_STOP2_BLOCK_UI_BUSY;
  }
  if (PS_HW6_RTOS_Stop2AutoDisplayAllowsIdle() == 0UL)
  {
    blocker_mask |= PS_HW6_RTOS_STOP2_BLOCK_DISPLAY_PENDING;
  }
  if (PS_HW6_RTOS_Stop2AutoStorageAllowsIdle() == 0UL)
  {
    blocker_mask |= PS_HW6_RTOS_STOP2_BLOCK_STORAGE_USB_BUSY;
  }
  if (PS_HW6_RTOS_Stop2AutoInputAllowsIdle() == 0UL)
  {
    blocker_mask |= PS_HW6_RTOS_STOP2_BLOCK_INPUT_PENDING;
  }
  if (queue_mask != 0UL)
  {
    blocker_mask |= PS_HW6_RTOS_STOP2_BLOCK_QUEUE_PENDING;
  }
  if (ps_stop2_lpbam_late_blocker_armed != 0UL)
  {
    ps_stop2_lpbam_late_blocker_armed = 0UL;
    g_ps_hw6_rtos_probe.stop2_lpbam_late_blocker_count++;
    blocker_mask |= PS_HW6_RTOS_STOP2_BLOCK_INPUT_PENDING;
  }

  if (queue_pending_mask != 0)
  {
    *queue_pending_mask = queue_mask;
  }

  return blocker_mask;
}

static HAL_StatusTypeDef PS_HW6_RTOS_RunStop2AutoIdleCheck(
  uint32_t now_tick,
  uint32_t allow_entry,
  uint32_t debug_force_enable)
{
  HAL_StatusTypeDef eligibility_status;
  HAL_StatusTypeDef entry_status = HAL_OK;
  uint32_t blocker_mask = 0UL;
  uint32_t pending_mask = 0UL;
  uint32_t queue_pending_mask = 0UL;
  uint32_t required_idle_ticks =
    PS_HW6_RTOS_MsToTicks((uint32_t)KNOB_POWER_AUTO_STOP2_IDLE_MS);
  uint32_t idle_ticks = 0UL;
  uint32_t ready = 0UL;
  uint32_t display_backend;
  uint32_t lpbam_blink_suppressed = 0UL;

  g_ps_hw6_rtos_probe.stop2_auto_enabled =
    (uint32_t)KNOB_POWER_AUTO_STOP2_ENABLE;
  g_ps_hw6_rtos_probe.stop2_auto_check_count++;
  g_ps_hw6_rtos_probe.stop2_auto_last_tick = now_tick;
  g_ps_hw6_rtos_probe.stop2_auto_required_idle_ticks =
    required_idle_ticks;
  g_ps_hw6_rtos_probe.stop2_auto_last_status =
    PS_HW6_RTOS_STATUS_NOT_RUN;
  g_ps_hw6_rtos_probe.stop2_auto_eligibility_status =
    PS_HW6_RTOS_STATUS_NOT_RUN;
  g_ps_hw6_rtos_probe.stop2_auto_entry_status =
    PS_HW6_RTOS_STATUS_NOT_RUN;
  g_ps_hw6_rtos_probe.stop2_auto_debug_force_enable =
    debug_force_enable;
  g_ps_hw6_rtos_probe.stop2_lpbam_awake_hold_enabled =
    (g_ps_hw6_power_stop2_lpbam_awake_hold_enable != 0UL) ? 1UL : 0UL;

  if (ps_power_input_activity_generation != ps_input_activity_generation)
  {
    ps_power_input_activity_generation = ps_input_activity_generation;
    g_ps_hw6_rtos_probe.stop2_auto_idle_start_tick = now_tick;
  }

  if ((g_ps_hw6_rtos_probe.stop2_lpbam_awake_hold_active != 0UL) &&
      ((g_ps_hw6_power_stop2_lpbam_awake_hold_enable == 0UL) ||
       (g_ps_hw6_owner_probe.display_lpbam_active == 0UL) ||
       (PS_HW6_RTOS_Stop2DisplayLpbamReady() == 0UL)))
  {
    g_ps_hw6_rtos_probe.stop2_lpbam_awake_hold_active = 0UL;
  }

  if ((KNOB_POWER_AUTO_STOP2_ENABLE == 0) && (allow_entry != 0UL) &&
      (debug_force_enable == 0UL) &&
      (ps_stop2_lpbam_abort_late_test_active == 0UL))
  {
    blocker_mask |= PS_HW6_RTOS_STOP2_BLOCK_AUTO_DISABLED;
  }

  eligibility_status = PS_HW6_RTOS_RunStop2EligibilityDryRun();
  g_ps_hw6_rtos_probe.stop2_auto_eligibility_status =
    (uint32_t)eligibility_status;
  blocker_mask |= g_ps_hw6_rtos_probe.stop2_eligibility_blocker_mask;
  pending_mask |= g_ps_hw6_rtos_probe.stop2_eligibility_pending_mask;

  blocker_mask |= PS_HW6_RTOS_Stop2AutoDynamicBlockerMask(
    &queue_pending_mask);
  display_backend =
    g_ps_hw6_rtos_probe.stop2_display_wait_backend_selected;

  if ((allow_entry != 0UL) &&
      (display_backend == PS_HW6_RTOS_STOP2_DISPLAY_BACKEND_LPBAM) &&
      (blocker_mask != 0UL) &&
      (PS_HW6_RTOS_Stop2DisplayLpbamReady() != 0UL))
  {
    (void)PS_HW6_RTOS_RequestDisplayLpbamAbort(0UL);
    pending_mask |= PS_HW6_RTOS_STOP2_PENDING_LPBAM_VALIDATION;
  }

  if (blocker_mask == 0UL)
  {
    if (g_ps_hw6_rtos_probe.stop2_auto_idle_start_tick == 0UL)
    {
      g_ps_hw6_rtos_probe.stop2_auto_idle_start_tick = now_tick;
    }
    idle_ticks = now_tick - g_ps_hw6_rtos_probe.stop2_auto_idle_start_tick;
    if (idle_ticks >= required_idle_ticks)
    {
      ready = 1UL;
    }
    else
    {
      pending_mask |= PS_HW6_RTOS_STOP2_PENDING_IDLE_WINDOW;
    }
  }
  else if ((blocker_mask == PS_HW6_RTOS_STOP2_BLOCK_CLOCK_CAPABILITY) &&
           (ps_display_blink_transfer_active != 0UL) &&
           (g_ps_hw6_rtos_probe.stop2_eligibility_clock_capabilities ==
            PS_HW6_CLOCK_CAP_DISPLAY_TRANSFER_ACTIVE))
  {
    if (g_ps_hw6_rtos_probe.stop2_auto_idle_start_tick == 0UL)
    {
      g_ps_hw6_rtos_probe.stop2_auto_idle_start_tick = now_tick;
    }
    idle_ticks = now_tick -
      g_ps_hw6_rtos_probe.stop2_auto_idle_start_tick;
    if (idle_ticks < required_idle_ticks)
    {
      pending_mask |= PS_HW6_RTOS_STOP2_PENDING_IDLE_WINDOW;
    }
    g_ps_hw6_rtos_probe.stop2_auto_clock_idle_preserve_count++;
    g_ps_hw6_rtos_probe.stop2_auto_clock_idle_preserve_tick = now_tick;
  }
  else
  {
    g_ps_hw6_rtos_probe.stop2_auto_idle_start_tick = 0UL;
  }

  g_ps_hw6_rtos_probe.stop2_auto_idle_ticks = idle_ticks;
  g_ps_hw6_rtos_probe.stop2_auto_blocker_mask = blocker_mask;
  g_ps_hw6_rtos_probe.stop2_auto_pending_mask = pending_mask;
  g_ps_hw6_rtos_probe.stop2_auto_queue_pending_mask = queue_pending_mask;

  if (ready == 0UL)
  {
    if (lpbam_blink_suppressed != 0UL)
    {
      ps_display_blink_stop2_suppressed = 0UL;
      PS_HW6_RTOS_ResetDisplayCursorBlink((uint32_t)tx_time_get());
    }
    g_ps_hw6_rtos_probe.stop2_auto_skip_count++;
    g_ps_hw6_rtos_probe.stop2_auto_last_status = (uint32_t)HAL_ERROR;
    return HAL_ERROR;
  }

  if (allow_entry == 0UL)
  {
    g_ps_hw6_rtos_probe.stop2_auto_last_status = (uint32_t)HAL_OK;
    return HAL_OK;
  }

  if ((display_backend == PS_HW6_RTOS_STOP2_DISPLAY_BACKEND_LPBAM) &&
      ((pending_mask & PS_HW6_RTOS_STOP2_PENDING_LPBAM_VALIDATION) != 0UL))
  {
    if ((PS_HW6_RTOS_Stop2DisplayLpbamReady() != 0UL) &&
        (PS_HW6_RTOS_Stop2LpbamPrepareFresh() != 0UL))
    {
      pending_mask &= ~PS_HW6_RTOS_STOP2_PENDING_LPBAM_VALIDATION;
      ps_display_blink_stop2_suppressed = 1UL;
      lpbam_blink_suppressed = 1UL;
      g_ps_hw6_rtos_probe.stop2_display_wait_backend_status =
        (uint32_t)HAL_OK;
      if (ps_stop2_lpbam_abort_late_test_active != 0UL)
      {
        ps_stop2_lpbam_late_blocker_armed = 1UL;
      }
    }
    else
    {
      if (g_ps_hw6_owner_probe.display_lpbam_active != 0UL)
      {
        (void)PS_HW6_RTOS_RequestDisplayLpbamAbort(0UL);
      }
      PS_HW6_RTOS_RequestDisplayLpbamPrepareAtBlinkEdge(now_tick);
      blocker_mask |= PS_HW6_RTOS_STOP2_BLOCK_LPBAM_NOT_READY;
    }
  }

  if ((display_backend == PS_HW6_RTOS_STOP2_DISPLAY_BACKEND_LPBAM) &&
      (g_ps_hw6_owner_probe.display_lpbam_active != 0UL) &&
      (PS_HW6_RTOS_Stop2DisplayLpbamReady() != 0UL))
  {
    ps_display_blink_stop2_suppressed = 1UL;
    lpbam_blink_suppressed = 1UL;
    g_ps_hw6_rtos_probe.stop2_display_wait_backend_status =
      (uint32_t)HAL_OK;
  }

  g_ps_hw6_rtos_probe.stop2_auto_blocker_mask = blocker_mask;
  g_ps_hw6_rtos_probe.stop2_auto_pending_mask = pending_mask;
  g_ps_hw6_rtos_probe.stop2_auto_queue_pending_mask = queue_pending_mask;

  if ((display_backend == PS_HW6_RTOS_STOP2_DISPLAY_BACKEND_HELD_FRAME) &&
      (PS_HW6_RTOS_DisplayLegacyCursorBlinkActive() != 0UL) &&
      (PS_HW6_RTOS_RequestDisplayCursorVisibleForStop2() != HAL_OK))
  {
    blocker_mask |= PS_HW6_RTOS_STOP2_BLOCK_DISPLAY_PENDING;
    g_ps_hw6_rtos_probe.stop2_auto_blocker_mask = blocker_mask;
    g_ps_hw6_rtos_probe.stop2_auto_pending_mask = pending_mask;
    g_ps_hw6_rtos_probe.stop2_auto_queue_pending_mask = queue_pending_mask;
    g_ps_hw6_rtos_probe.stop2_auto_skip_count++;
    g_ps_hw6_rtos_probe.stop2_auto_last_status = (uint32_t)HAL_ERROR;
    return HAL_ERROR;
  }

  blocker_mask |= PS_HW6_RTOS_Stop2AutoDynamicBlockerMask(
    &queue_pending_mask);
  g_ps_hw6_rtos_probe.stop2_auto_blocker_mask = blocker_mask;
  g_ps_hw6_rtos_probe.stop2_auto_pending_mask = pending_mask;
  g_ps_hw6_rtos_probe.stop2_auto_queue_pending_mask = queue_pending_mask;
  if (blocker_mask != 0UL)
  {
    if (PS_HW6_RTOS_Stop2DisplayLpbamReady() != 0UL)
    {
      (void)PS_HW6_RTOS_RequestDisplayLpbamAbort(0UL);
      pending_mask |= PS_HW6_RTOS_STOP2_PENDING_LPBAM_VALIDATION;
    }
    g_ps_hw6_rtos_probe.stop2_auto_blocker_mask = blocker_mask;
    g_ps_hw6_rtos_probe.stop2_auto_pending_mask = pending_mask;
    g_ps_hw6_rtos_probe.stop2_auto_queue_pending_mask = queue_pending_mask;
    g_ps_hw6_rtos_probe.stop2_auto_skip_count++;
    g_ps_hw6_rtos_probe.stop2_auto_last_status = (uint32_t)HAL_ERROR;
    if (lpbam_blink_suppressed != 0UL)
    {
      ps_display_blink_stop2_suppressed = 0UL;
      PS_HW6_RTOS_ResetDisplayCursorBlink((uint32_t)tx_time_get());
    }
    return HAL_ERROR;
  }

  if ((g_ps_hw6_power_stop2_lpbam_awake_hold_enable != 0UL) &&
      (display_backend == PS_HW6_RTOS_STOP2_DISPLAY_BACKEND_LPBAM) &&
      (g_ps_hw6_owner_probe.display_lpbam_active != 0UL) &&
      (PS_HW6_RTOS_Stop2DisplayLpbamReady() != 0UL))
  {
    if (g_ps_hw6_rtos_probe.stop2_lpbam_awake_hold_active == 0UL)
    {
      g_ps_hw6_rtos_probe.stop2_lpbam_awake_hold_active = 1UL;
      g_ps_hw6_rtos_probe.stop2_lpbam_awake_hold_count++;
      g_ps_hw6_rtos_probe.stop2_lpbam_awake_hold_start_tick = now_tick;
    }
    ps_display_blink_stop2_suppressed = 1UL;
    g_ps_hw6_rtos_probe.stop2_auto_entry_status = (uint32_t)HAL_OK;
    g_ps_hw6_rtos_probe.stop2_auto_last_status = (uint32_t)HAL_OK;
    return HAL_OK;
  }

  g_ps_hw6_rtos_probe.stop2_lpbam_wake_resume_status =
    PS_HW6_RTOS_STATUS_NOT_RUN;
  g_ps_hw6_rtos_probe.stop2_auto_entry_count++;
  entry_status = PS_HW6_RTOS_RunStop2ControlledEntry();
  if ((display_backend == PS_HW6_RTOS_STOP2_DISPLAY_BACKEND_LPBAM) &&
      (PS_HW6_RTOS_Stop2DisplayLpbamReady() != 0UL))
  {
    HAL_StatusTypeDef abort_status =
      PS_HW6_RTOS_RequestDisplayLpbamAbort(1UL);

    if ((entry_status == HAL_OK) && (abort_status != HAL_OK))
    {
      entry_status = abort_status;
    }
  }
  if (g_ps_hw6_rtos_probe.stop2_lpbam_wake_resume_status !=
      (uint32_t)HAL_OK)
  {
    ps_display_blink_stop2_suppressed = 0UL;
    PS_HW6_RTOS_ResetDisplayCursorBlink((uint32_t)tx_time_get());
  }
  if (ps_joystick_awake_publish_reset_pending != 0UL)
  {
    (void)PS_HW6_RTOS_SendCommand(
      PS_HW6_RTOS_OWNER_INPUT,
      PS_HW6_RTOS_COMMAND_INPUT_JOYSTICK_WAKE_PUBLISH);
  }
  g_ps_hw6_rtos_probe.stop2_auto_entry_status = (uint32_t)entry_status;
  g_ps_hw6_rtos_probe.stop2_auto_last_status = (uint32_t)entry_status;
  return entry_status;
}

static HAL_StatusTypeDef PS_HW6_RTOS_RunStop2LpbamAbortLateTest(
  uint32_t now_tick)
{
  HAL_StatusTypeDef status;

  g_ps_hw6_rtos_probe.stop2_lpbam_abort_late_test_count++;
  g_ps_hw6_rtos_probe.stop2_lpbam_abort_late_test_tick = now_tick;
  g_ps_hw6_rtos_probe.stop2_lpbam_abort_late_test_status =
    PS_HW6_RTOS_STATUS_NOT_RUN;
  g_ps_hw6_rtos_probe.stop2_lpbam_abort_late_test_blocker_mask = 0UL;
  g_ps_hw6_rtos_probe.stop2_lpbam_prepare_display_clear_count =
    PS_HW6_RTOS_STATUS_NOT_RUN;
  ps_stop2_lpbam_abort_late_test_active = 1UL;
  ps_stop2_lpbam_late_blocker_armed = 0UL;

  PS_HW6_DisplayOwner_DebugForceNextLpbamReady();
  status = PS_HW6_RTOS_RunStop2AutoIdleCheck(now_tick, 1UL, 0UL);

  ps_stop2_lpbam_abort_late_test_active = 0UL;
  ps_stop2_lpbam_late_blocker_armed = 0UL;
  g_ps_hw6_rtos_probe.stop2_lpbam_abort_late_test_status =
    (uint32_t)status;
  g_ps_hw6_rtos_probe.stop2_lpbam_abort_late_test_blocker_mask =
    g_ps_hw6_rtos_probe.stop2_auto_blocker_mask;
  g_ps_hw6_rtos_probe.stop2_auto_next_tick = now_tick +
    PS_HW6_RTOS_MsToTicks(
      (uint32_t)KNOB_POWER_AUTO_STOP2_CHECK_PERIOD_MS);
  return status;
}

static void PS_HW6_RTOS_RunStop2AutoIdlePeriodic(uint32_t now_tick)
{
  uint32_t period_ticks =
    PS_HW6_RTOS_MsToTicks((uint32_t)KNOB_POWER_AUTO_STOP2_CHECK_PERIOD_MS);

  if (KNOB_POWER_AUTO_STOP2_ENABLE == 0)
  {
    return;
  }

  if ((ps_stop2_lpbam_edge_request_pending != 0UL) ||
      (g_ps_hw6_rtos_probe.stop2_lpbam_edge_state ==
       PS_HW6_RTOS_STOP2_LPBAM_EDGE_REQUESTED))
  {
    g_ps_hw6_rtos_probe.stop2_auto_next_tick = now_tick + period_ticks;
    return;
  }

  if (g_ps_hw6_rtos_probe.stop2_auto_next_tick == 0UL)
  {
    g_ps_hw6_rtos_probe.stop2_auto_next_tick = now_tick + period_ticks;
    return;
  }

  if (PS_HW6_RTOS_TimeReached(
        now_tick,
        g_ps_hw6_rtos_probe.stop2_auto_next_tick) == 0UL)
  {
    return;
  }

  g_ps_hw6_rtos_probe.stop2_auto_next_tick = now_tick + period_ticks;
  (void)PS_HW6_RTOS_RunStop2AutoIdleCheck(now_tick, 1UL, 0UL);
}

static HAL_StatusTypeDef PS_HW6_RTOS_RunStop2AutoIdleEntryTest(
  uint32_t now_tick)
{
  uint32_t required_idle_ticks =
    PS_HW6_RTOS_MsToTicks((uint32_t)KNOB_POWER_AUTO_STOP2_IDLE_MS);

  g_ps_hw6_rtos_probe.stop2_auto_debug_force_entry_count++;
  g_ps_hw6_rtos_probe.stop2_auto_debug_force_entry_tick = now_tick;
  g_ps_hw6_rtos_probe.stop2_auto_idle_start_tick =
    now_tick - required_idle_ticks;

  return PS_HW6_RTOS_RunStop2AutoIdleCheck(now_tick, 1UL, 1UL);
}

static UINT PS_HW6_RTOS_SendDisplayUiRenderCommand(
  uint32_t page,
  uint32_t calibration_page,
  uint32_t focus_index,
  uint32_t shutdown_state,
  uint32_t shutdown_countdown_seconds)
{
  ULONG message[PS_HW6_RTOS_MESSAGE_WORDS];

  message[0] = PS_HW6_RTOS_DISPLAY_UI_MAGIC;
  message[1] = PS_HW6_RTOS_OWNER_DISPLAY;
  message[2] = (ULONG)page;
  message[3] = PS_HW6_RTOS_DisplayUiPackedState(
    calibration_page,
    focus_index,
    shutdown_state,
    shutdown_countdown_seconds);
  return tx_queue_send(&ps_queues[PS_HW6_RTOS_OWNER_DISPLAY],
                       message,
                       TX_NO_WAIT);
}

static UINT PS_HW6_RTOS_SendUiLogicalPress(uint32_t source)
{
  ULONG message[PS_HW6_RTOS_MESSAGE_WORDS];
  UINT status;

  message[0] = PS_HW6_RTOS_UI_INPUT_MAGIC;
  message[1] = PS_HW6_RTOS_OWNER_UI;
  message[2] = PS_HW6_RTOS_UI_INPUT_PRESS;
  message[3] = (ULONG)source;
  status = tx_queue_send(&ps_queues[PS_HW6_RTOS_OWNER_UI],
                         message,
                         TX_NO_WAIT);
  PS_HW6_TraceInputButton(source,
                          PS_HW6_RTOS_OWNER_UI,
                          (uint32_t)status,
                          0UL);
  return status;
}

static uint32_t PS_HW6_RTOS_RuntimeInputPackedButton(
  uint32_t button_id,
  uint32_t button_mask)
{
  return ((button_id & PS_HW6_RTOS_RUNTIME_INPUT_BUTTON_ID_MASK) |
          ((button_mask & PS_HW6_RTOS_RUNTIME_INPUT_BUTTON_MASK_MASK) <<
           PS_HW6_RTOS_RUNTIME_INPUT_BUTTON_MASK_SHIFT));
}

static UINT PS_HW6_RTOS_SendRuntimeLogicalEvent(uint32_t event,
                                                uint32_t source,
                                                uint32_t source_mask)
{
  ULONG message[PS_HW6_RTOS_MESSAGE_WORDS];
  UINT status;

  message[0] = PS_HW6_RTOS_RUNTIME_INPUT_MAGIC;
  message[1] = PS_HW6_RTOS_OWNER_RUNTIME;
  message[2] = (ULONG)event;
  message[3] = (ULONG)PS_HW6_RTOS_RuntimeInputPackedButton(source,
                                                           source_mask);
  status = tx_queue_send(&ps_queues[PS_HW6_RTOS_OWNER_RUNTIME],
                         message,
                         TX_NO_WAIT);
  PS_HW6_TraceInputButton(source,
                          PS_HW6_RTOS_OWNER_RUNTIME,
                          (uint32_t)status,
                          source_mask);
  return status;
}

static UINT PS_HW6_RTOS_SendUiLifecycleEvent(uint32_t event)
{
  ULONG message[PS_HW6_RTOS_MESSAGE_WORDS];

  if ((event < (uint32_t)PS_UI_ROUTER_EVENT_MSC_EXPORT) ||
      (event > (uint32_t)PS_UI_ROUTER_EVENT_MSC_RECOVERY))
  {
    return TX_QUEUE_ERROR;
  }

  message[0] = PS_HW6_RTOS_UI_LIFECYCLE_MAGIC;
  message[1] = PS_HW6_RTOS_OWNER_UI;
  message[2] = (ULONG)event;
  message[3] = 0UL;
  return tx_queue_send(&ps_queues[PS_HW6_RTOS_OWNER_UI],
                       message,
                       TX_NO_WAIT);
}

static uint32_t PS_HW6_RTOS_UiInputPackedJoystick(
  uint32_t source,
  uint32_t candidate_direction_mask,
  uint32_t resolved_direction_mask,
  uint32_t dispatch)
{
  return ((source & PS_HW6_RTOS_UI_INPUT_SOURCE_MASK) |
          ((candidate_direction_mask &
            PS_HW6_RTOS_UI_INPUT_CANDIDATE_MASK) <<
           PS_HW6_RTOS_UI_INPUT_CANDIDATE_SHIFT) |
          ((resolved_direction_mask &
            PS_HW6_RTOS_UI_INPUT_RESOLVED_MASK) <<
           PS_HW6_RTOS_UI_INPUT_RESOLVED_SHIFT) |
          (((dispatch != 0UL) ? 1UL : 0UL) <<
           PS_HW6_RTOS_UI_INPUT_DISPATCH_SHIFT));
}

static UINT PS_HW6_RTOS_SendUiJoystickState(
  uint32_t candidate_direction_mask,
  uint32_t resolved_direction_mask,
  uint32_t dispatch)
{
  ULONG message[PS_HW6_RTOS_MESSAGE_WORDS];
  uint32_t source = (dispatch != 0UL) ?
    PS_HW6_RTOS_LogicalSourceForJoystickDirection(resolved_direction_mask) :
    (uint32_t)PS_INPUT_LOGICAL_SOURCE_NONE;
  UINT status;

  message[0] = PS_HW6_RTOS_UI_INPUT_MAGIC;
  message[1] = PS_HW6_RTOS_OWNER_UI;
  message[2] = PS_HW6_RTOS_UI_INPUT_JOYSTICK_STATE;
  message[3] = (ULONG)PS_HW6_RTOS_UiInputPackedJoystick(
    source,
    candidate_direction_mask,
    resolved_direction_mask,
    dispatch);
  status = tx_queue_send(&ps_queues[PS_HW6_RTOS_OWNER_UI],
                         message,
                         TX_NO_WAIT);
  PS_HW6_TraceInputButton(source,
                          PS_HW6_RTOS_OWNER_UI,
                          (uint32_t)status,
                          candidate_direction_mask);
  return status;
}

static UINT PS_HW6_RTOS_SendRuntimeInputEvent(
  const ps_input_button_logical_record_t *record)
{
  return PS_HW6_RTOS_SendRuntimeLogicalEvent((uint32_t)record->event,
                                              (uint32_t)record->button_id,
                                              record->button_mask);
}

static uint32_t PS_HW6_RTOS_InputPolicySystemOverlayActive(void)
{
  return PS_HW6_RTOS_SystemOverlayActive();
}

static uint32_t PS_HW6_RTOS_InputPolicyRuntimeClassOwnsInput(
  uint32_t runtime_class)
{
  if ((runtime_class == (uint32_t)PS_HW6_RUNTIME_CLASS_LP_GRAPH) ||
      (runtime_class == (uint32_t)PS_HW6_RUNTIME_CLASS_LP_MODULE) ||
      (runtime_class == (uint32_t)PS_HW6_RUNTIME_CLASS_RT_SCENE))
  {
    return 1UL;
  }

  return 0UL;
}

static UINT PS_HW6_RTOS_DeliverInputLogicalEvent(
  const ps_input_button_logical_record_t *record)
{
  UINT status = (UINT)PS_HW6_RTOS_INPUT_POLICY_STATUS_SUPPRESSED;
  uint32_t target = PS_HW6_RTOS_INPUT_POLICY_TARGET_NONE;
  uint32_t reason = PS_HW6_RTOS_INPUT_POLICY_REASON_NONE;
  uint32_t event = PS_INPUT_BUTTON_LOGICAL_EVENT_NONE;
  uint32_t button_id = PS_INPUT_BUTTON_ID_NONE;
  uint32_t button_mask = 0UL;
  uint32_t timestamp = 0UL;
  uint32_t runtime_class;
  uint32_t runtime_lifecycle;

  runtime_class = g_ps_hw6_rtos_probe.runtime_current_class;
  runtime_lifecycle = g_ps_hw6_rtos_probe.runtime_lifecycle;

  if (record != 0)
  {
    event = (uint32_t)record->event;
    button_id = (uint32_t)record->button_id;
    button_mask = record->button_mask;
    timestamp = record->timestamp;
  }

  g_ps_hw6_rtos_probe.input_policy_event_count++;
  g_ps_hw6_rtos_probe.input_policy_last_event = event;
  g_ps_hw6_rtos_probe.input_policy_last_button_id = button_id;
  g_ps_hw6_rtos_probe.input_policy_last_mask = button_mask;
  g_ps_hw6_rtos_probe.input_policy_last_timestamp = timestamp;
  g_ps_hw6_rtos_probe.input_policy_last_runtime_class = runtime_class;
  g_ps_hw6_rtos_probe.input_policy_last_runtime_lifecycle =
    runtime_lifecycle;
  g_ps_hw6_rtos_probe.input_policy_last_ui_page =
    g_ps_ui_router_probe.current_page;
  g_ps_hw6_rtos_probe.input_policy_last_package_state =
    g_ps_ui_router_probe.package_state;
  g_ps_hw6_rtos_probe.input_policy_last_shutdown_state =
    g_ps_ui_router_probe.shutdown_state;

  if (record == 0)
  {
    reason = PS_HW6_RTOS_INPUT_POLICY_REASON_INVALID_BUTTON;
  }
  else if ((record->event < PS_INPUT_BUTTON_LOGICAL_EVENT_PRESS) ||
           (record->event > PS_INPUT_BUTTON_LOGICAL_EVENT_REPEAT))
  {
    reason = PS_HW6_RTOS_INPUT_POLICY_REASON_UNSUPPORTED_EVENT;
  }
  else if ((PS_HW6_RTOS_LogicalSourceIsButton(button_id) == 0UL) &&
           (PS_HW6_RTOS_LogicalSourceIsJoystickDirection(button_id) == 0UL))
  {
    reason = PS_HW6_RTOS_INPUT_POLICY_REASON_INVALID_BUTTON;
  }
  else if ((button_id == (uint32_t)PS_INPUT_LOGICAL_SOURCE_START) &&
           (event != (uint32_t)PS_INPUT_BUTTON_LOGICAL_EVENT_PRESS))
  {
    reason = PS_HW6_RTOS_INPUT_POLICY_REASON_UNSUPPORTED_EVENT;
  }
  else if (g_ps_hw6_rtos_probe.input_policy_lock_active != 0UL)
  {
    reason = PS_HW6_RTOS_INPUT_POLICY_REASON_LOCKED;
  }
  else if ((button_id == (uint32_t)PS_INPUT_LOGICAL_SOURCE_BUTTON_B) &&
           (event == (uint32_t)PS_INPUT_BUTTON_LOGICAL_EVENT_PRESS) &&
           (PS_HW6_RTOS_UiMscExportActive() != 0UL))
  {
    target = PS_HW6_RTOS_INPUT_POLICY_TARGET_UI;
    reason = PS_HW6_RTOS_INPUT_POLICY_REASON_SYSTEM_OVERLAY;
    g_ps_hw6_rtos_probe.ui_action_msc_exit_intercept_count++;
    PS_HW6_RTOS_HandleUiRouterAction(
      (uint32_t)PS_UI_ROUTER_ACTION_MSC_EXIT);
    status = (UINT)g_ps_hw6_rtos_probe.ui_action_send_status;
    if (status == TX_SUCCESS)
    {
      g_ps_hw6_rtos_probe.input_policy_deliver_count++;
      g_ps_hw6_rtos_probe.input_policy_ui_deliver_count++;
      g_ps_hw6_rtos_probe.input_policy_overlay_deliver_count++;
    }
    else
    {
      reason = PS_HW6_RTOS_INPUT_POLICY_REASON_SEND_FAILED;
      g_ps_hw6_rtos_probe.input_policy_suppress_count++;
    }
  }
  else if (g_ps_ui_router_probe.current_page ==
           (uint32_t)PS_UI_ROUTER_PAGE_INPUT_DIAGNOSTIC)
  {
    target = PS_HW6_RTOS_INPUT_POLICY_TARGET_UI;
    reason = PS_HW6_RTOS_INPUT_POLICY_REASON_UI_FOCUS;
    PS_HW6_RTOS_SendCurrentUiRenderCommand();
    status = TX_SUCCESS;
    g_ps_hw6_rtos_probe.input_policy_deliver_count++;
    g_ps_hw6_rtos_probe.input_policy_ui_deliver_count++;
  }
  else if ((runtime_class == (uint32_t)PS_HW6_RUNTIME_CLASS_LP_GRAPH) &&
           (runtime_lifecycle ==
            (uint32_t)PS_HW6_RUNTIME_LIFECYCLE_RUNNING) &&
           (ps_runtime_interaction_state ==
            PS_HW6_RUNTIME_INTERACTION_STATE_INACTIVE))
  {
    reason = PS_HW6_RTOS_INPUT_POLICY_REASON_RUNTIME_INACTIVE;
    if (event == (uint32_t)PS_INPUT_BUTTON_LOGICAL_EVENT_PRESS)
    {
      status = PS_HW6_RTOS_RequestRuntimeCommand(
        PS_HW6_RTOS_COMMAND_RUNTIME_INTERACTION_CUE);
      g_ps_hw6_rtos_probe.runtime_interaction_suppressed_input_count++;
    }
  }
  else if (PS_HW6_RTOS_InputPolicySystemOverlayActive() != 0UL)
  {
    reason = PS_HW6_RTOS_INPUT_POLICY_REASON_SYSTEM_OVERLAY;
    if (event == (uint32_t)PS_INPUT_BUTTON_LOGICAL_EVENT_PRESS)
    {
      target = PS_HW6_RTOS_INPUT_POLICY_TARGET_UI;
      status = PS_HW6_RTOS_SendUiLogicalPress(button_id);
      if (status == TX_SUCCESS)
      {
        g_ps_hw6_rtos_probe.input_policy_deliver_count++;
        g_ps_hw6_rtos_probe.input_policy_ui_deliver_count++;
        g_ps_hw6_rtos_probe.input_policy_overlay_deliver_count++;
      }
      else
      {
        reason = PS_HW6_RTOS_INPUT_POLICY_REASON_SEND_FAILED;
        g_ps_hw6_rtos_probe.input_policy_suppress_count++;
      }
    }
  }
  else if ((runtime_class == (uint32_t)PS_HW6_RUNTIME_CLASS_NONE) ||
           (runtime_lifecycle !=
            (uint32_t)PS_HW6_RUNTIME_LIFECYCLE_RUNNING))
  {
    reason = PS_HW6_RTOS_INPUT_POLICY_REASON_RUNTIME_NOT_READY;
  }
  else if ((runtime_class == (uint32_t)PS_HW6_RUNTIME_CLASS_SHELL) ||
           (runtime_class == (uint32_t)PS_HW6_RUNTIME_CLASS_INSTALLER))
  {
    reason = PS_HW6_RTOS_INPUT_POLICY_REASON_UI_FOCUS;
    if (event == (uint32_t)PS_INPUT_BUTTON_LOGICAL_EVENT_PRESS)
    {
      target = PS_HW6_RTOS_INPUT_POLICY_TARGET_UI;
      status = PS_HW6_RTOS_SendUiLogicalPress(button_id);
      if (status == TX_SUCCESS)
      {
        g_ps_hw6_rtos_probe.input_policy_deliver_count++;
        g_ps_hw6_rtos_probe.input_policy_ui_deliver_count++;
      }
      else
      {
        reason = PS_HW6_RTOS_INPUT_POLICY_REASON_SEND_FAILED;
        g_ps_hw6_rtos_probe.input_policy_suppress_count++;
      }
    }
  }
  else if (PS_HW6_RTOS_InputPolicyRuntimeClassOwnsInput(
             runtime_class) != 0UL)
  {
    target = PS_HW6_RTOS_INPUT_POLICY_TARGET_RUNTIME;
    reason = PS_HW6_RTOS_INPUT_POLICY_REASON_RUNTIME_FOCUS;
    status = PS_HW6_RTOS_SendRuntimeInputEvent(record);
    if (status == TX_SUCCESS)
    {
      g_ps_hw6_rtos_probe.input_policy_deliver_count++;
      g_ps_hw6_rtos_probe.input_policy_runtime_deliver_count++;
    }
    else
    {
      reason = PS_HW6_RTOS_INPUT_POLICY_REASON_SEND_FAILED;
      g_ps_hw6_rtos_probe.input_policy_suppress_count++;
    }
  }
  else
  {
    reason = PS_HW6_RTOS_INPUT_POLICY_REASON_UNSUPPORTED_CLASS;
  }

  if (target == PS_HW6_RTOS_INPUT_POLICY_TARGET_NONE)
  {
    g_ps_hw6_rtos_probe.input_policy_suppress_count++;
  }

  g_ps_hw6_rtos_probe.input_policy_last_target = target;
  g_ps_hw6_rtos_probe.input_policy_last_reason = reason;
  g_ps_hw6_rtos_probe.input_policy_last_status = (uint32_t)status;
  return status;
}

static uint32_t PS_HW6_RTOS_LogicalSourceForJoystickDirection(
  uint32_t direction_mask)
{
  if (direction_mask == PS_INPUT_JOYSTICK_DIRECTION_LEFT)
  {
    return (uint32_t)PS_INPUT_LOGICAL_SOURCE_JOY_LEFT;
  }
  if (direction_mask == PS_INPUT_JOYSTICK_DIRECTION_RIGHT)
  {
    return (uint32_t)PS_INPUT_LOGICAL_SOURCE_JOY_RIGHT;
  }
  if (direction_mask == PS_INPUT_JOYSTICK_DIRECTION_UP)
  {
    return (uint32_t)PS_INPUT_LOGICAL_SOURCE_JOY_UP;
  }
  if (direction_mask == PS_INPUT_JOYSTICK_DIRECTION_DOWN)
  {
    return (uint32_t)PS_INPUT_LOGICAL_SOURCE_JOY_DOWN;
  }
  if (direction_mask == (PS_INPUT_JOYSTICK_DIRECTION_UP |
                         PS_INPUT_JOYSTICK_DIRECTION_LEFT))
  {
    return (uint32_t)PS_INPUT_LOGICAL_SOURCE_JOY_UP_LEFT;
  }
  if (direction_mask == (PS_INPUT_JOYSTICK_DIRECTION_UP |
                         PS_INPUT_JOYSTICK_DIRECTION_RIGHT))
  {
    return (uint32_t)PS_INPUT_LOGICAL_SOURCE_JOY_UP_RIGHT;
  }
  if (direction_mask == (PS_INPUT_JOYSTICK_DIRECTION_DOWN |
                         PS_INPUT_JOYSTICK_DIRECTION_LEFT))
  {
    return (uint32_t)PS_INPUT_LOGICAL_SOURCE_JOY_DOWN_LEFT;
  }
  if (direction_mask == (PS_INPUT_JOYSTICK_DIRECTION_DOWN |
                         PS_INPUT_JOYSTICK_DIRECTION_RIGHT))
  {
    return (uint32_t)PS_INPUT_LOGICAL_SOURCE_JOY_DOWN_RIGHT;
  }
  return (uint32_t)PS_INPUT_LOGICAL_SOURCE_NONE;
}

static uint32_t PS_HW6_RTOS_JoystickLogicalPolicy(void)
{
  uint32_t policy = PS_SCENE_RUNTIME_JOYSTICK_FOUR_WAY;

  if ((g_ps_hw6_rtos_probe.runtime_current_class ==
       (uint32_t)PS_HW6_RUNTIME_CLASS_LP_GRAPH) &&
      (g_ps_hw6_rtos_probe.runtime_lifecycle ==
       (uint32_t)PS_HW6_RUNTIME_LIFECYCLE_RUNNING) &&
      (PS_SceneRuntime_StateSceneActive() != 0UL))
  {
    policy = PS_SceneRuntime_JoystickPolicy();
  }
  g_ps_hw6_rtos_probe.joystick_logical_policy = policy;
  return policy;
}

static uint32_t PS_HW6_RTOS_JoystickLogicalDirection(
  uint32_t candidate_direction_mask,
  uint32_t resolved_direction_mask)
{
  return (PS_HW6_RTOS_JoystickLogicalPolicy() ==
          PS_SCENE_RUNTIME_JOYSTICK_EIGHT_WAY) ?
         candidate_direction_mask : resolved_direction_mask;
}

static void PS_HW6_RTOS_ResetJoystickLogicalLifecycle(void)
{
  ps_joystick_logical_active_direction_mask = 0UL;
  ps_joystick_logical_press_tick = 0UL;
  ps_joystick_logical_next_repeat_tick = 0UL;
  ps_joystick_logical_hold_sent = 0UL;
  g_ps_hw6_rtos_probe.joystick_logical_active_direction_mask = 0UL;
  g_ps_hw6_rtos_probe.joystick_logical_active_source =
    (uint32_t)PS_INPUT_LOGICAL_SOURCE_NONE;
  g_ps_hw6_rtos_probe.joystick_logical_press_tick = 0UL;
  g_ps_hw6_rtos_probe.joystick_logical_next_repeat_tick = 0UL;
  g_ps_hw6_rtos_probe.joystick_logical_hold_sent = 0UL;
}

static void PS_HW6_RTOS_StartJoystickLogicalLifecycle(
  uint32_t direction_mask,
  uint32_t timestamp)
{
  uint32_t repeat_start_ticks = PS_HW6_RTOS_MsToTicks(
    (uint32_t)KNOB_INPUT_BTN_REPEAT_START_MS);

  ps_joystick_logical_active_direction_mask = direction_mask;
  ps_joystick_logical_press_tick = timestamp;
  ps_joystick_logical_next_repeat_tick = timestamp + repeat_start_ticks;
  ps_joystick_logical_hold_sent = 0UL;
  g_ps_hw6_rtos_probe.joystick_logical_active_direction_mask = direction_mask;
  g_ps_hw6_rtos_probe.joystick_logical_active_source =
    PS_HW6_RTOS_LogicalSourceForJoystickDirection(direction_mask);
  g_ps_hw6_rtos_probe.joystick_logical_press_tick = timestamp;
  g_ps_hw6_rtos_probe.joystick_logical_next_repeat_tick =
    ps_joystick_logical_next_repeat_tick;
  g_ps_hw6_rtos_probe.joystick_logical_hold_sent = 0UL;
}

static UINT PS_HW6_RTOS_DeliverJoystickLogicalEvent(
  uint32_t event,
  uint32_t logical_direction_mask,
  uint32_t candidate_direction_mask,
  uint32_t resolved_direction_mask,
  uint32_t timestamp)
{
  UINT status = (UINT)PS_HW6_RTOS_INPUT_POLICY_STATUS_SUPPRESSED;
  uint32_t source =
    PS_HW6_RTOS_LogicalSourceForJoystickDirection(logical_direction_mask);
  uint32_t target = PS_HW6_RTOS_INPUT_POLICY_TARGET_NONE;
  uint32_t runtime_class = g_ps_hw6_rtos_probe.runtime_current_class;
  uint32_t runtime_lifecycle = g_ps_hw6_rtos_probe.runtime_lifecycle;

  if ((source == (uint32_t)PS_INPUT_LOGICAL_SOURCE_NONE) ||
      (event < (uint32_t)PS_INPUT_BUTTON_LOGICAL_EVENT_PRESS) ||
      (event > (uint32_t)PS_INPUT_BUTTON_LOGICAL_EVENT_REPEAT))
  {
    g_ps_hw6_rtos_probe.joystick_logical_drop_count++;
  }
  else if (g_ps_hw6_rtos_probe.input_policy_lock_active != 0UL)
  {
    g_ps_hw6_rtos_probe.joystick_logical_drop_count++;
  }
  else if (g_ps_ui_router_probe.current_page ==
           (uint32_t)PS_UI_ROUTER_PAGE_INPUT_DIAGNOSTIC)
  {
    target = PS_HW6_RTOS_INPUT_POLICY_TARGET_UI;
    status = PS_HW6_RTOS_SendUiJoystickState(candidate_direction_mask,
                                             resolved_direction_mask,
                                             0UL);
  }
  else if ((runtime_class == (uint32_t)PS_HW6_RUNTIME_CLASS_LP_GRAPH) &&
           (runtime_lifecycle ==
            (uint32_t)PS_HW6_RUNTIME_LIFECYCLE_RUNNING) &&
           (ps_runtime_interaction_state ==
            PS_HW6_RUNTIME_INTERACTION_STATE_INACTIVE))
  {
    g_ps_hw6_rtos_probe.joystick_logical_drop_count++;
  }
  else if (PS_HW6_RTOS_InputPolicySystemOverlayActive() != 0UL)
  {
    if (event == (uint32_t)PS_INPUT_BUTTON_LOGICAL_EVENT_PRESS)
    {
      target = PS_HW6_RTOS_INPUT_POLICY_TARGET_UI;
      status = PS_HW6_RTOS_SendUiJoystickState(candidate_direction_mask,
                                                resolved_direction_mask,
                                                1UL);
    }
  }
  else if ((runtime_class == (uint32_t)PS_HW6_RUNTIME_CLASS_NONE) ||
           (runtime_lifecycle !=
            (uint32_t)PS_HW6_RUNTIME_LIFECYCLE_RUNNING))
  {
    g_ps_hw6_rtos_probe.joystick_logical_drop_count++;
  }
  else if ((runtime_class == (uint32_t)PS_HW6_RUNTIME_CLASS_SHELL) ||
           (runtime_class == (uint32_t)PS_HW6_RUNTIME_CLASS_INSTALLER))
  {
    if (event == (uint32_t)PS_INPUT_BUTTON_LOGICAL_EVENT_PRESS)
    {
      target = PS_HW6_RTOS_INPUT_POLICY_TARGET_UI;
      status = PS_HW6_RTOS_SendUiJoystickState(candidate_direction_mask,
                                                resolved_direction_mask,
                                                1UL);
    }
  }
  else if (PS_HW6_RTOS_InputPolicyRuntimeClassOwnsInput(runtime_class) != 0UL)
  {
    target = PS_HW6_RTOS_INPUT_POLICY_TARGET_RUNTIME;
    status = PS_HW6_RTOS_SendRuntimeLogicalEvent(event,
                                                  source,
                                                  logical_direction_mask);
  }
  else
  {
    g_ps_hw6_rtos_probe.joystick_logical_drop_count++;
  }

  if (status == TX_SUCCESS)
  {
    if (event == (uint32_t)PS_INPUT_BUTTON_LOGICAL_EVENT_PRESS)
    {
      g_ps_hw6_rtos_probe.joystick_logical_activation_count++;
    }
    if (event != (uint32_t)PS_INPUT_BUTTON_LOGICAL_EVENT_RELEASE)
    {
      ps_input_activity_generation++;
    }
    PS_HW6_RTOS_RequestStop2AutoCheckNow(timestamp);
  }
  else if (target != PS_HW6_RTOS_INPUT_POLICY_TARGET_NONE)
  {
    g_ps_hw6_rtos_probe.joystick_logical_drop_count++;
  }

  g_ps_hw6_rtos_probe.joystick_logical_last_candidate_mask =
    candidate_direction_mask;
  g_ps_hw6_rtos_probe.joystick_logical_last_resolved_mask =
    resolved_direction_mask;
  g_ps_hw6_rtos_probe.joystick_logical_last_direction_mask =
    logical_direction_mask;
  g_ps_hw6_rtos_probe.joystick_logical_last_source = source;
  g_ps_hw6_rtos_probe.joystick_logical_last_event = event;
  g_ps_hw6_rtos_probe.joystick_logical_last_target = target;
  g_ps_hw6_rtos_probe.joystick_logical_last_status = (uint32_t)status;
  g_ps_hw6_rtos_probe.joystick_logical_last_tick = timestamp;
  return status;
}

static uint32_t PS_HW6_RTOS_JoystickAwakePollingAllowed(void)
{
  uint32_t power_state =
    g_ps_hw6_owner_sm_probe.current_state[PS_HW6_SM_POWER];

  if ((power_state != (uint32_t)PWR_ACTIVE_LP) &&
      (power_state != (uint32_t)PWR_ACTIVE_RT))
  {
    return 0UL;
  }
  if ((g_ps_hw6_rtos_probe.runtime_complete == 0UL) ||
      (ps_runtime_interaction_state ==
       PS_HW6_RUNTIME_INTERACTION_STATE_INACTIVE) ||
      (g_ps_hw6_owner_sm_probe
         .joystick_calibration_persistent_load_available == 0UL) ||
      (g_ps_ui_router_probe.current_page ==
       (uint32_t)PS_UI_ROUTER_PAGE_CALIBRATION) ||
      (g_ps_hw6_owner_sm_probe.joystick_calibration_session_active != 0UL) ||
      (PS_HW6_OwnerStateMachines_JoystickCalibrationCaptureActive() != 0UL))
  {
    return 0UL;
  }
  return 1UL;
}

static void PS_HW6_RTOS_RunJoystickAwakeInput(uint32_t now_tick)
{
  HAL_StatusTypeDef status;
  uint32_t period_ticks;
  uint32_t previous_candidate;
  uint32_t previous_direction;
  uint32_t candidate;
  uint32_t direction;
  uint32_t logical_direction;
  uint32_t logical_changed;
  uint32_t long_press_ticks;
  uint32_t repeat_period_ticks;
  uint32_t wake_direction = 0UL;
  uint32_t wake_candidate = 0UL;
  uint32_t wake_logical_direction = 0UL;
  uint32_t wake_direction_available = 0UL;

  if (PS_HW6_RTOS_JoystickAwakePollingAllowed() == 0UL)
  {
    ps_joystick_awake_poll_next_tick = 0UL;
    ps_joystick_awake_published_direction_mask = 0UL;
    ps_joystick_awake_published_candidate_mask = 0UL;
    PS_HW6_RTOS_ResetJoystickLogicalLifecycle();
    g_ps_hw6_rtos_probe.joystick_awake_poll_next_tick = 0UL;
    return;
  }

  if (ps_joystick_awake_publish_reset_pending != 0UL)
  {
    g_ps_hw6_rtos_probe.joystick_logical_stop2_reset_previous_direction =
      ps_joystick_awake_published_direction_mask;
    ps_joystick_awake_published_direction_mask = 0UL;
    ps_joystick_awake_published_candidate_mask = 0UL;
    PS_HW6_RTOS_ResetJoystickLogicalLifecycle();
    ps_joystick_awake_publish_reset_pending = 0UL;
    g_ps_hw6_rtos_probe.joystick_logical_stop2_reset_pending = 0UL;
    g_ps_hw6_rtos_probe.joystick_logical_stop2_reset_consume_count++;
    g_ps_hw6_rtos_probe.joystick_logical_stop2_reset_tick = now_tick;
    g_ps_hw6_rtos_probe.joystick_logical_wake_capture_direction = 0UL;
    g_ps_hw6_rtos_probe.joystick_logical_wake_capture_status =
      PS_HW6_RTOS_STATUS_NOT_RUN;
    g_ps_hw6_rtos_probe.joystick_logical_wake_capture_tick = now_tick;
    wake_direction_available =
      PS_HW6_OwnerStateMachines_TakeJoystickWakeDirection(
        &wake_direction);
    if (wake_direction_available != 0UL)
    {
      wake_candidate =
        g_ps_hw6_owner_sm_probe.joystick_input_candidate_direction_mask;
      if ((PS_HW6_RTOS_JoystickCandidateMaskIsValid(wake_candidate) == 0UL) ||
          (wake_candidate == 0UL))
      {
        wake_candidate = wake_direction;
      }
      g_ps_hw6_rtos_probe.joystick_logical_wake_capture_available_count++;
      g_ps_hw6_rtos_probe.joystick_logical_wake_capture_direction =
        wake_direction;
      g_ps_hw6_rtos_probe.joystick_logical_wake_capture_tick = now_tick;
    }
  }

  if (wake_direction_available != 0UL)
  {
    wake_logical_direction = PS_HW6_RTOS_JoystickLogicalDirection(
      wake_candidate,
      wake_direction);
    g_ps_hw6_rtos_probe.joystick_logical_change_count++;
    g_ps_hw6_rtos_probe.joystick_logical_wake_capture_status =
      (uint32_t)PS_HW6_RTOS_DeliverJoystickLogicalEvent(
        (uint32_t)PS_INPUT_BUTTON_LOGICAL_EVENT_PRESS,
        wake_logical_direction,
        wake_candidate,
        wake_direction,
        now_tick);
    if (g_ps_hw6_rtos_probe.joystick_logical_wake_capture_status ==
        (uint32_t)TX_SUCCESS)
    {
      g_ps_hw6_rtos_probe.joystick_logical_wake_capture_publish_count++;
    }
    ps_joystick_awake_published_direction_mask = wake_direction;
    ps_joystick_awake_published_candidate_mask = wake_candidate;
    PS_HW6_RTOS_StartJoystickLogicalLifecycle(wake_logical_direction,
                                               now_tick);
  }

  if ((ps_joystick_awake_poll_next_tick != 0UL) &&
      (PS_HW6_RTOS_TimeReached(now_tick,
                               ps_joystick_awake_poll_next_tick) == 0UL))
  {
    return;
  }

  period_ticks = PS_HW6_RTOS_MsToTicks(
    (uint32_t)KNOB_INPUT_JOYSTICK_AWAKE_POLL_PERIOD_MS);
  if (period_ticks == 0UL)
  {
    period_ticks = 1UL;
  }
  ps_joystick_awake_poll_next_tick = now_tick + period_ticks;
  g_ps_hw6_rtos_probe.joystick_awake_poll_next_tick =
    ps_joystick_awake_poll_next_tick;
  previous_candidate = ps_joystick_awake_published_candidate_mask;
  previous_direction = ps_joystick_awake_published_direction_mask;
  status = PS_HW6_OwnerStateMachines_RunJoystickCardinalProbe();
  g_ps_hw6_rtos_probe.joystick_awake_poll_count++;
  g_ps_hw6_rtos_probe.joystick_awake_poll_last_status = (uint32_t)status;
  if (status != HAL_OK)
  {
    g_ps_hw6_rtos_probe.joystick_awake_poll_error_count++;
    return;
  }

  candidate =
    g_ps_hw6_owner_sm_probe.joystick_input_candidate_direction_mask;
  direction = g_ps_hw6_owner_sm_probe.joystick_input_direction_mask;
  logical_direction = PS_HW6_RTOS_JoystickLogicalDirection(candidate,
                                                            direction);
  logical_changed = (logical_direction !=
                     ps_joystick_logical_active_direction_mask) ? 1UL : 0UL;
  if (logical_changed != 0UL)
  {
    g_ps_hw6_rtos_probe.joystick_logical_change_count++;
    if ((ps_joystick_logical_active_direction_mask != 0UL) &&
        (logical_direction != 0UL))
    {
      g_ps_hw6_rtos_probe.joystick_logical_switch_count++;
    }
    if (ps_joystick_logical_active_direction_mask != 0UL)
    {
      g_ps_hw6_rtos_probe.joystick_logical_release_count++;
      (void)PS_HW6_RTOS_DeliverJoystickLogicalEvent(
        (uint32_t)PS_INPUT_BUTTON_LOGICAL_EVENT_RELEASE,
        ps_joystick_logical_active_direction_mask,
        candidate,
        direction,
        now_tick);
    }
    if (logical_direction != 0UL)
    {
      g_ps_hw6_rtos_probe.joystick_logical_hold_count++;
      (void)PS_HW6_RTOS_DeliverJoystickLogicalEvent(
        (uint32_t)PS_INPUT_BUTTON_LOGICAL_EVENT_PRESS,
        logical_direction,
        candidate,
        direction,
        now_tick);
      PS_HW6_RTOS_StartJoystickLogicalLifecycle(logical_direction,
                                                 now_tick);
    }
    else
    {
      PS_HW6_RTOS_ResetJoystickLogicalLifecycle();
    }
  }
  else if (logical_direction != 0UL)
  {
    long_press_ticks = PS_HW6_RTOS_MsToTicks(
      (uint32_t)KNOB_INPUT_BTN_LONG_PRESS_MS);
    if ((ps_joystick_logical_hold_sent == 0UL) &&
        (PS_HW6_RTOS_TimeReached(
           now_tick,
           ps_joystick_logical_press_tick + long_press_ticks) != 0UL))
    {
      g_ps_hw6_rtos_probe.joystick_logical_repeat_count++;
      (void)PS_HW6_RTOS_DeliverJoystickLogicalEvent(
        (uint32_t)PS_INPUT_BUTTON_LOGICAL_EVENT_LONG_PRESS,
        logical_direction,
        candidate,
        direction,
        now_tick);
      ps_joystick_logical_hold_sent = 1UL;
      g_ps_hw6_rtos_probe.joystick_logical_hold_sent = 1UL;
    }
    if ((ps_joystick_logical_hold_sent != 0UL) &&
        (PS_HW6_RTOS_TimeReached(
           now_tick,
           ps_joystick_logical_next_repeat_tick) != 0UL))
    {
      (void)PS_HW6_RTOS_DeliverJoystickLogicalEvent(
        (uint32_t)PS_INPUT_BUTTON_LOGICAL_EVENT_REPEAT,
        logical_direction,
        candidate,
        direction,
        now_tick);
      repeat_period_ticks = PS_HW6_RTOS_MsToTicks(
        (uint32_t)KNOB_INPUT_BTN_REPEAT_PERIOD_MS);
      if (repeat_period_ticks == 0UL)
      {
        repeat_period_ticks = 1UL;
      }
      ps_joystick_logical_next_repeat_tick = now_tick + repeat_period_ticks;
      g_ps_hw6_rtos_probe.joystick_logical_next_repeat_tick =
        ps_joystick_logical_next_repeat_tick;
    }
  }

  if ((g_ps_ui_router_probe.current_page ==
       (uint32_t)PS_UI_ROUTER_PAGE_HOME) &&
      ((candidate != previous_candidate) ||
       (direction != previous_direction)) &&
      ((logical_changed == 0UL) || (logical_direction == 0UL)))
  {
    (void)PS_HW6_RTOS_SendUiJoystickState(candidate, direction, 0UL);
  }
  ps_joystick_awake_published_candidate_mask = candidate;
  ps_joystick_awake_published_direction_mask = direction;
}

static uint32_t PS_HW6_RTOS_UiMscExportActive(void)
{
  return ((g_ps_hw6_owner_sm_probe.usb_host_msc_active != 0UL) ||
          (g_ps_storage_msc_bridge_probe.export_enabled != 0UL)) ?
         1UL : 0UL;
}

static void PS_HW6_RTOS_HandleUiRouterAction(uint32_t action)
{
  ps_status_t router_status;
  uint32_t admission_action;
  UINT status;

  if (action == (uint32_t)PS_UI_ROUTER_ACTION_NONE)
  {
    return;
  }

  g_ps_hw6_rtos_probe.ui_action_last = action;
  g_ps_hw6_rtos_probe.ui_action_count++;
  admission_action = PS_HW6_RTOS_AdmissionActionForUiRouterAction(action);
  if (admission_action != PS_HW6_RTOS_ADMISSION_ACTION_NONE)
  {
    status = PS_HW6_RTOS_AdmitSystemAction(admission_action);
    if (status != TX_SUCCESS)
    {
      g_ps_hw6_rtos_probe.ui_action_send_status = (uint32_t)status;
      return;
    }
  }
  if (action == (uint32_t)PS_UI_ROUTER_ACTION_MSC_ENTER)
  {
    g_ps_hw6_rtos_probe.ui_action_msc_enter_count++;
    (void)PS_HW6_RTOS_RequestRuntimeCommand(
      PS_HW6_RTOS_COMMAND_RUNTIME_INSTALLER_ENTER);
    status = PS_HW6_RTOS_RequestUsbMscEnter();
    if (status == TX_SUCCESS)
    {
      PS_HW6_RTOS_SendCurrentUiRenderCommand();
    }
  }
  else if (action == (uint32_t)PS_UI_ROUTER_ACTION_MSC_EXIT)
  {
    g_ps_hw6_rtos_probe.ui_action_msc_exit_count++;
    if (PS_HW6_RTOS_UiMscExportActive() != 0UL)
    {
      status = PS_HW6_RTOS_RequestUsbMscExit();
      if (status == TX_SUCCESS)
      {
        PS_HW6_RTOS_SendCurrentUiRenderCommand();
      }
    }
    else
    {
      status = TX_NO_INSTANCE;
    }
  }
  else if (action == (uint32_t)PS_UI_ROUTER_ACTION_PACKAGE_SCAN)
  {
    g_ps_hw6_rtos_probe.ui_action_package_scan_count++;
    status = PS_HW6_RTOS_RequestPackageScan();
    if (status == TX_SUCCESS)
    {
      PS_HW6_RTOS_SendCurrentUiRenderCommand();
    }
  }
  else if (action ==
           (uint32_t)PS_UI_ROUTER_ACTION_PACKAGE_INSTALL_STUB)
  {
    g_ps_hw6_rtos_probe.ui_action_package_install_stub_count++;
    status = PS_HW6_RTOS_RequestPackageInstallStub();
    if (status == TX_SUCCESS)
    {
      PS_HW6_RTOS_SendCurrentUiRenderCommand();
    }
    else
    {
      (void)PS_HW6_RTOS_RequestRuntimeCommand(
        PS_HW6_RTOS_COMMAND_RUNTIME_INSTALLER_ERROR);
      router_status = PS_UIRouter_Dispatch(
        PS_UI_ROUTER_EVENT_PACKAGE_INSTALL_STUB_ERROR);
      if (router_status == PS_STATUS_OK)
      {
        PS_HW6_RTOS_SendCurrentUiRenderCommand();
      }
    }
  }
  else if (action == (uint32_t)PS_UI_ROUTER_ACTION_PACKAGE_LAUNCH)
  {
    status = PS_HW6_RTOS_RequestRuntimeCommand(
      PS_HW6_RTOS_COMMAND_RUNTIME_PACKAGE_REACTIVE_STUB);
  }
  else
  {
    g_ps_hw6_rtos_probe.ui_action_unsupported_count++;
    status = TX_QUEUE_ERROR;
  }
  g_ps_hw6_rtos_probe.ui_action_send_status = (uint32_t)status;
}

static UINT PS_HW6_RTOS_SendPowerStartEvent(
  ps_input_start_power_event_t event,
  uint32_t hold_ticks)
{
  ULONG message[PS_HW6_RTOS_MESSAGE_WORDS];
  UINT status;

  message[0] = PS_HW6_RTOS_POWER_INPUT_MAGIC;
  message[1] = PS_HW6_RTOS_OWNER_POWER;
  message[2] = (ULONG)event;
  message[3] = (ULONG)hold_ticks;
  status = tx_queue_send(&ps_queues[PS_HW6_RTOS_OWNER_POWER],
                         message,
                         TX_NO_WAIT);
  PS_HW6_TracePowerStart((uint32_t)event,
                         hold_ticks,
                         (uint32_t)status,
                         PS_HW6_RTOS_OWNER_POWER);
  return status;
}

static void PS_HW6_RTOS_SendCurrentUiRenderCommand(void)
{
  uint32_t display_focus = g_ps_ui_router_probe.focus_index;

  if (g_ps_ui_router_probe.current_page ==
      (uint32_t)PS_UI_ROUTER_PAGE_PACKAGE_BROWSER)
  {
    if (g_ps_ui_router_probe.package_state ==
        (uint32_t)PS_UI_ROUTER_PACKAGE_NONE)
    {
      display_focus =
        PS_UI_ROUTER_PACKAGE_MENU_FOCUS_BASE +
        g_ps_ui_router_probe.focus_index;
    }
    else
    {
      display_focus = g_ps_ui_router_probe.package_state;
    }
  }

  (void)PS_HW6_RTOS_SendDisplayUiRenderCommand(
    g_ps_ui_router_probe.current_page,
    g_ps_ui_router_probe.calibration_page,
    display_focus,
    g_ps_ui_router_probe.shutdown_state,
    g_ps_ui_router_probe.shutdown_countdown_seconds);
}

static void PS_HW6_RTOS_SendStorageLifecycleDisplayCue(
  uint32_t shutdown_state)
{
  (void)PS_HW6_RTOS_SendDisplayUiRenderCommand(
    PS_UI_ROUTER_PAGE_SHUTDOWN,
    PS_UI_ROUTER_CAL_NONE,
    0UL,
    shutdown_state,
    0UL);
}

static void PS_HW6_RTOS_SetPowerDebug(GPIO_PinState state)
{
  GPIO_PinState before = HAL_GPIO_ReadPin(PWR_DBG_GPIO_Port, PWR_DBG_Pin);

  HAL_GPIO_WritePin(PWR_DBG_GPIO_Port, PWR_DBG_Pin, state);
  g_ps_hw6_rtos_probe.pwr_dbg_state =
    (state == GPIO_PIN_SET) ? 1UL : 0UL;
  if (before != state)
  {
    g_ps_hw6_rtos_probe.pwr_dbg_toggle_count++;
    g_ps_hw6_rtos_probe.pwr_dbg_last_toggle_tick = (uint32_t)tx_time_get();
  }
}

static void PS_HW6_RTOS_RunCycleOwnerCommand(uint32_t cycle_index,
                                              uint32_t direction,
                                              uint32_t owner_id,
                                              ULONG command)
{
  const ULONG expected_ack = PS_HW6_RTOS_ACK_OWNER(owner_id);
  ULONG actual_flags = 0UL;
  UINT send_status;
  UINT wait_status;

  send_status = PS_HW6_RTOS_SendCycleCommand(
    owner_id, command, cycle_index);
  wait_status = send_status;
  if (send_status == TX_SUCCESS)
  {
    wait_status = tx_event_flags_get(
      &ps_event_groups[PS_HW6_RTOS_EVENT_DEBUG_INDEX],
      expected_ack, TX_AND_CLEAR, &actual_flags,
      PS_HW6_RTOS_OWNER_ACK_WAIT_TICKS);
  }
  PS_HW6_OwnerStateMachines_RecordCycleCommand(
    cycle_index, direction, owner_id,
    send_status, wait_status, (uint32_t)actual_flags);
}

static ULONG PS_HW6_RTOS_StabilizeAckWaitTicks(uint32_t owner_id)
{
  return (owner_id == PS_HW6_RTOS_OWNER_STORAGE) ?
         PS_HW6_RTOS_STORAGE_STABILIZE_ACK_WAIT_TICKS :
         PS_HW6_RTOS_OWNER_ACK_WAIT_TICKS;
}

static void PS_HW6_RTOS_RunBootIdleParkModeCommand(
  uint32_t owner_id,
  ULONG command,
  uint32_t mode,
  uint32_t *send_slot,
  uint32_t *wait_slot,
  uint32_t *ack_slot)
{
  const ULONG expected_ack = PS_HW6_RTOS_ACK_OWNER(owner_id);
  ULONG actual_flags = 0UL;
  UINT send_status;
  UINT wait_status;

  (void)tx_event_flags_get(&ps_event_groups[PS_HW6_RTOS_EVENT_DEBUG_INDEX],
                           expected_ack,
                           TX_AND_CLEAR,
                           &actual_flags,
                           TX_NO_WAIT);
  actual_flags = 0UL;

  send_status = PS_HW6_RTOS_SendModeCommand(owner_id, command, mode);
  wait_status = send_status;
  if (send_status == TX_SUCCESS)
  {
    wait_status = tx_event_flags_get(
      &ps_event_groups[PS_HW6_RTOS_EVENT_DEBUG_INDEX],
      expected_ack,
      TX_AND_CLEAR,
      &actual_flags,
      PS_HW6_RTOS_OWNER_ACK_WAIT_TICKS);
  }

  *send_slot = (uint32_t)send_status;
  *wait_slot = (uint32_t)wait_status;
  *ack_slot = (uint32_t)actual_flags;
}

static HAL_StatusTypeDef PS_HW6_RTOS_RunBootIdlePeripheralPark(void)
{
  HAL_StatusTypeDef status;
  uint32_t ready;
  uint32_t send_status;
  uint32_t wait_status;
  uint32_t ack_flags;

  if (g_ps_hw6_rtos_probe.boot_idle_peripheral_park_request_count != 0UL)
  {
    return (HAL_StatusTypeDef)
      g_ps_hw6_rtos_probe.boot_idle_peripheral_park_last_status;
  }

  g_ps_hw6_rtos_probe.boot_idle_peripheral_park_request_count++;
  g_ps_hw6_rtos_probe.boot_idle_peripheral_park_start_tick =
    (uint32_t)tx_time_get();
  g_ps_hw6_rtos_probe.boot_idle_peripheral_park_last_status =
    PS_HW6_RTOS_STATUS_NOT_RUN;
  g_ps_hw6_rtos_probe.boot_idle_peripheral_park_done = 0UL;
  ps_power_boot_idle_peripheral_park_done = 0UL;

  PS_HW6_RTOS_RunBootIdleParkModeCommand(
    PS_HW6_RTOS_OWNER_COMM,
    PS_HW6_RTOS_COMMAND_COMM_BLE_MODE,
    (uint32_t)PS_HW6_COMM_BLE_MODE_SLEEP_SYSTEM_OFF,
    &send_status,
    &wait_status,
    &ack_flags);
  g_ps_hw6_rtos_probe.boot_idle_peripheral_park_ble_send_status =
    send_status;
  g_ps_hw6_rtos_probe.boot_idle_peripheral_park_ble_wait_status =
    wait_status;
  g_ps_hw6_rtos_probe.boot_idle_peripheral_park_ble_ack_flags =
    ack_flags;

  PS_HW6_RTOS_RunBootIdleParkModeCommand(
    PS_HW6_RTOS_OWNER_SENSOR,
    PS_HW6_RTOS_COMMAND_SENSOR_IMU_MODE,
    (uint32_t)PS_HW6_IMU_MODE_OFF,
    &send_status,
    &wait_status,
    &ack_flags);
  g_ps_hw6_rtos_probe.boot_idle_peripheral_park_imu_send_status =
    send_status;
  g_ps_hw6_rtos_probe.boot_idle_peripheral_park_imu_wait_status =
    wait_status;
  g_ps_hw6_rtos_probe.boot_idle_peripheral_park_imu_ack_flags =
    ack_flags;

  ready = PS_HW6_OwnerStateMachines_Stop2IdlePeripheralsReady();
  status = (ready != 0UL) ? HAL_OK : HAL_ERROR;

  g_ps_hw6_rtos_probe.boot_idle_peripheral_park_done = ready;
  ps_power_boot_idle_peripheral_park_done = ready;
  g_ps_hw6_rtos_probe.boot_idle_peripheral_park_last_status =
    (uint32_t)status;
  g_ps_hw6_rtos_probe.boot_idle_peripheral_park_end_tick =
    (uint32_t)tx_time_get();
  return status;
}

static UINT PS_HW6_RTOS_BootParkStorageUsb(void)
{
  const uint32_t owner_id = PS_HW6_RTOS_OWNER_STORAGE;
  const ULONG expected_ack = PS_HW6_RTOS_ACK_OWNER(owner_id);
  ULONG actual_flags = 0UL;
  UINT send_status;
  UINT wait_status;

  (void)tx_event_flags_get(&ps_event_groups[PS_HW6_RTOS_EVENT_DEBUG_INDEX],
                           expected_ack,
                           TX_AND_CLEAR,
                           &actual_flags,
                           TX_NO_WAIT);
  actual_flags = 0UL;

  send_status = PS_HW6_RTOS_SendCommand(owner_id,
                                        PS_HW6_RTOS_COMMAND_USB_BOOT_PARK);
  wait_status = send_status;
  if (send_status == TX_SUCCESS)
  {
    wait_status = tx_event_flags_get(
      &ps_event_groups[PS_HW6_RTOS_EVENT_DEBUG_INDEX],
      expected_ack,
      TX_AND_CLEAR,
      &actual_flags,
      PS_HW6_RTOS_OWNER_ACK_WAIT_TICKS);
  }

  PS_HW6_OwnerStateMachines_RecordCommand(
    owner_id,
    send_status,
    wait_status,
    (uint32_t)actual_flags);

  return wait_status;
}

static HAL_StatusTypeDef PS_HW6_RTOS_RunPowerQuiesceBarrier(uint32_t reason)
{
  static const uint32_t quiesce_order[] =
  {
    PS_HW6_RTOS_OWNER_AUDIO,
    PS_HW6_RTOS_OWNER_COMM,
    PS_HW6_RTOS_OWNER_SENSOR,
    PS_HW6_RTOS_OWNER_INPUT,
    PS_HW6_RTOS_OWNER_STORAGE,
    PS_HW6_RTOS_OWNER_DISPLAY
  };
  uint32_t index;
  uint32_t storage_clock_required;
  UINT storage_clock_status = TX_SUCCESS;
  UINT storage_clock_release_status = TX_SUCCESS;
  HAL_StatusTypeDef barrier_status;

  storage_clock_required = PS_HW6_RTOS_PowerQuiesceNeedsStorageClock();
  if (storage_clock_required != 0UL)
  {
    storage_clock_status = PS_HW6_RTOS_ApplyStorageClockCapabilitiesFromPower(
      PS_HW6_RTOS_STORAGE_CLOCK_REASON_POWER_QUIESCE,
      PS_HW6_RTOS_STORAGE_CLOCK_FLASH_CAPABILITIES);
  }

  PS_HW6_OwnerStateMachines_BeginPowerQuiesce(reason);
  for (index = 0U;
       index < (sizeof(quiesce_order) / sizeof(quiesce_order[0]));
       ++index)
  {
    const uint32_t owner_id = quiesce_order[index];
    const ULONG expected_ack = PS_HW6_RTOS_ACK_OWNER(owner_id);
    ULONG actual_flags = 0UL;
    UINT send_status;
    UINT wait_status;

    (void)tx_event_flags_get(&ps_event_groups[PS_HW6_RTOS_EVENT_DEBUG_INDEX],
                             expected_ack,
                             TX_AND_CLEAR,
                             &actual_flags,
                             TX_NO_WAIT);
    actual_flags = 0UL;

    if ((owner_id == PS_HW6_RTOS_OWNER_STORAGE) &&
        (storage_clock_required != 0UL) &&
        (storage_clock_status != TX_SUCCESS))
    {
      send_status = storage_clock_status;
      wait_status = storage_clock_status;
    }
    else
    {
      send_status = PS_HW6_RTOS_SendPowerQuiesceCommand(owner_id, reason);
      wait_status = send_status;

      if (send_status == TX_SUCCESS)
      {
        wait_status = tx_event_flags_get(
          &ps_event_groups[PS_HW6_RTOS_EVENT_DEBUG_INDEX],
          expected_ack, TX_AND_CLEAR, &actual_flags,
          PS_HW6_RTOS_OWNER_ACK_WAIT_TICKS);
      }
    }
    PS_HW6_OwnerStateMachines_RecordPowerQuiesceCommand(
      owner_id, send_status, wait_status, (uint32_t)actual_flags);
  }
  barrier_status = PS_HW6_OwnerStateMachines_EndPowerQuiesce();

  if ((storage_clock_required != 0UL) &&
      (storage_clock_status == TX_SUCCESS))
  {
    storage_clock_release_status =
      PS_HW6_RTOS_ApplyStorageClockCapabilitiesFromPower(
        PS_HW6_RTOS_STORAGE_CLOCK_REASON_RELEASE,
        0UL);
    if ((barrier_status == HAL_OK) &&
        (storage_clock_release_status != TX_SUCCESS))
    {
      barrier_status = HAL_ERROR;
    }
  }

  return barrier_status;
}
static HAL_StatusTypeDef PS_HW6_RTOS_RunPostStopResumeBarrier(void)
{
  static const uint32_t resume_order[] =
  {
    PS_HW6_RTOS_OWNER_STORAGE,
    PS_HW6_RTOS_OWNER_INPUT,
    PS_HW6_RTOS_OWNER_SENSOR,
    PS_HW6_RTOS_OWNER_COMM,
    PS_HW6_RTOS_OWNER_DISPLAY,
    PS_HW6_RTOS_OWNER_AUDIO
  };
  uint32_t index;
  uint32_t storage_clock_required;
  UINT storage_clock_status = TX_SUCCESS;
  UINT storage_clock_release_status = TX_SUCCESS;
  HAL_StatusTypeDef barrier_status;

  storage_clock_required = PS_HW6_RTOS_PostStopResumeNeedsStorageClock();
  if (storage_clock_required != 0UL)
  {
    storage_clock_status = PS_HW6_RTOS_ApplyStorageClockCapabilitiesFromPower(
      PS_HW6_RTOS_STORAGE_CLOCK_REASON_POST_STOP_RESUME,
      PS_HW6_RTOS_STORAGE_CLOCK_FLASH_CAPABILITIES);
  }

  PS_HW6_OwnerStateMachines_BeginPostStopResume();
  for (index = 0U;
       index < (sizeof(resume_order) / sizeof(resume_order[0]));
       ++index)
  {
    const uint32_t owner_id = resume_order[index];
    const ULONG expected_ack = PS_HW6_RTOS_ACK_OWNER(owner_id);
    ULONG actual_flags = 0UL;
    UINT send_status = TX_SUCCESS;
    UINT wait_status = TX_SUCCESS;

    if ((owner_id == PS_HW6_RTOS_OWNER_STORAGE) &&
        (storage_clock_required != 0UL) &&
        (storage_clock_status != TX_SUCCESS))
    {
      send_status = storage_clock_status;
      wait_status = storage_clock_status;
    }
    else
    {
      (void)tx_event_flags_get(&ps_event_groups[PS_HW6_RTOS_EVENT_DEBUG_INDEX],
                               expected_ack,
                               TX_AND_CLEAR,
                               &actual_flags,
                               TX_NO_WAIT);
      actual_flags = 0UL;

      send_status = PS_HW6_RTOS_SendPostStopResumeCommand(owner_id);
      wait_status = send_status;
      if (send_status == TX_SUCCESS)
      {
        wait_status = tx_event_flags_get(
          &ps_event_groups[PS_HW6_RTOS_EVENT_DEBUG_INDEX],
          expected_ack, TX_AND_CLEAR, &actual_flags,
          PS_HW6_RTOS_OWNER_ACK_WAIT_TICKS);
      }
    }
    PS_HW6_OwnerStateMachines_RecordPostStopResumeCommand(
      owner_id, send_status, wait_status, (uint32_t)actual_flags);
  }
  barrier_status = PS_HW6_OwnerStateMachines_EndPostStopResume();

  if (storage_clock_required != 0UL)
  {
    storage_clock_release_status =
      PS_HW6_RTOS_ApplyStorageClockCapabilitiesFromPower(
        PS_HW6_RTOS_STORAGE_CLOCK_REASON_RELEASE,
        0UL);
    if ((barrier_status == HAL_OK) &&
        (storage_clock_release_status != TX_SUCCESS))
    {
      barrier_status = HAL_ERROR;
    }
  }

  return barrier_status;
}

static HAL_StatusTypeDef PS_HW6_RTOS_RunStop2ActiveOwnerPrep(void)
{
  static const uint32_t owner_order[] =
  {
    PS_HW6_RTOS_OWNER_DISPLAY,
    PS_HW6_RTOS_OWNER_AUDIO,
    PS_HW6_RTOS_OWNER_INPUT,
    PS_HW6_RTOS_OWNER_SENSOR,
    PS_HW6_RTOS_OWNER_COMM
  };
  static const uint32_t resume_order[] =
  {
    PS_HW6_RTOS_OWNER_INPUT,
    PS_HW6_RTOS_OWNER_SENSOR,
    PS_HW6_RTOS_OWNER_COMM,
    PS_HW6_RTOS_OWNER_DISPLAY,
    PS_HW6_RTOS_OWNER_AUDIO
  };
  static const uint32_t quiesce_order[] =
  {
    PS_HW6_RTOS_OWNER_AUDIO,
    PS_HW6_RTOS_OWNER_DISPLAY,
    PS_HW6_RTOS_OWNER_COMM,
    PS_HW6_RTOS_OWNER_SENSOR,
    PS_HW6_RTOS_OWNER_INPUT
  };
  const uint32_t cycle_index = 0UL;
  uint32_t index;
  uint32_t prep_transport_ok = 1UL;
  HAL_StatusTypeDef prep_status;
  HAL_StatusTypeDef power_status;

  PS_HW6_OwnerStateMachines_BeginWorkflow();
  PS_HW6_OwnerStateMachines_BeginStop2ActivePrep(cycle_index);

  power_status = PS_HW6_OwnerStateMachines_Stabilize(
    PS_HW6_RTOS_OWNER_POWER);
  if (power_status != HAL_OK)
  {
    prep_transport_ok = 0UL;
  }
  PS_HW6_OwnerStateMachines_RecordCommand(
    PS_HW6_RTOS_OWNER_POWER, TX_SUCCESS, TX_SUCCESS,
    PS_HW6_RTOS_ACK_OWNER(PS_HW6_RTOS_OWNER_POWER));

  for (index = 0U; index < (sizeof(owner_order) / sizeof(owner_order[0]));
       ++index)
  {
    const uint32_t owner_id = owner_order[index];
    const ULONG expected_ack = PS_HW6_RTOS_ACK_OWNER(owner_id);
    ULONG actual_flags = 0UL;
    UINT send_status = PS_HW6_RTOS_SendCommand(
      owner_id, PS_HW6_RTOS_COMMAND_STABILIZE);
    UINT wait_status = send_status;

    if (send_status == TX_SUCCESS)
    {
      wait_status = tx_event_flags_get(
        &ps_event_groups[PS_HW6_RTOS_EVENT_DEBUG_INDEX],
        expected_ack, TX_AND_CLEAR, &actual_flags,
        PS_HW6_RTOS_OWNER_ACK_WAIT_TICKS);
    }
    if ((send_status != TX_SUCCESS) ||
        (wait_status != TX_SUCCESS) ||
        ((actual_flags & expected_ack) == 0UL))
    {
      prep_transport_ok = 0UL;
    }
    PS_HW6_OwnerStateMachines_RecordCommand(
      owner_id, send_status, wait_status, (uint32_t)actual_flags);
  }

  PS_HW6_OwnerStateMachines_BeginCycle(cycle_index);
  power_status = PS_HW6_OwnerStateMachines_Resume(
    PS_HW6_RTOS_OWNER_POWER, cycle_index);
  if (power_status != HAL_OK)
  {
    prep_transport_ok = 0UL;
  }
  PS_HW6_OwnerStateMachines_RecordCycleCommand(
    cycle_index, PS_HW6_OWNER_SM_CYCLE_RESUME,
    PS_HW6_RTOS_OWNER_POWER, TX_SUCCESS, TX_SUCCESS,
    PS_HW6_RTOS_ACK_OWNER(PS_HW6_RTOS_OWNER_POWER));

  for (index = 0U; index < (sizeof(resume_order) / sizeof(resume_order[0]));
       ++index)
  {
    PS_HW6_RTOS_RunCycleOwnerCommand(
      cycle_index, PS_HW6_OWNER_SM_CYCLE_RESUME,
      resume_order[index], PS_HW6_RTOS_COMMAND_RESUME);
  }
  PS_HW6_OwnerStateMachines_RecordCycleActiveStates(cycle_index);

  for (index = 0U; index < (sizeof(quiesce_order) / sizeof(quiesce_order[0]));
       ++index)
  {
    PS_HW6_RTOS_RunCycleOwnerCommand(
      cycle_index, PS_HW6_OWNER_SM_CYCLE_QUIESCE,
      quiesce_order[index], PS_HW6_RTOS_COMMAND_QUIESCE);
  }
  power_status = PS_HW6_OwnerStateMachines_Quiesce(
    PS_HW6_RTOS_OWNER_POWER, cycle_index);
  if (power_status != HAL_OK)
  {
    prep_transport_ok = 0UL;
  }
  PS_HW6_OwnerStateMachines_RecordCycleCommand(
    cycle_index, PS_HW6_OWNER_SM_CYCLE_QUIESCE,
    PS_HW6_RTOS_OWNER_POWER, TX_SUCCESS, TX_SUCCESS,
    PS_HW6_RTOS_ACK_OWNER(PS_HW6_RTOS_OWNER_POWER));
  PS_HW6_OwnerStateMachines_EndCycle(cycle_index);

  prep_status = PS_HW6_OwnerStateMachines_EndStop2ActivePrep(cycle_index);
  return ((prep_transport_ok != 0UL) && (prep_status == HAL_OK)) ?
    HAL_OK : HAL_ERROR;
}

static HAL_StatusTypeDef PS_HW6_RTOS_RunStop2ActiveResumeScaffold(void)
{
  HAL_StatusTypeDef prep_status = PS_HW6_RTOS_RunStop2ActiveOwnerPrep();

  if (prep_status != HAL_OK)
  {
    return prep_status;
  }
  return PS_HW6_OwnerStateMachines_RunStop2StartWakeScaffold();
}

static void PS_HW6_RTOS_RunPowerWorkflow(void)
{
  static const uint32_t owner_order[] =
  {
    PS_HW6_RTOS_OWNER_DISPLAY,
    PS_HW6_RTOS_OWNER_AUDIO,
    PS_HW6_RTOS_OWNER_INPUT,
    PS_HW6_RTOS_OWNER_SENSOR,
    PS_HW6_RTOS_OWNER_STORAGE,
    PS_HW6_RTOS_OWNER_COMM
  };
  static const uint32_t resume_order[] =
  {
    PS_HW6_RTOS_OWNER_STORAGE,
    PS_HW6_RTOS_OWNER_INPUT,
    PS_HW6_RTOS_OWNER_SENSOR,
    PS_HW6_RTOS_OWNER_COMM,
    PS_HW6_RTOS_OWNER_DISPLAY,
    PS_HW6_RTOS_OWNER_AUDIO
  };
  static const uint32_t quiesce_order[] =
  {
    PS_HW6_RTOS_OWNER_AUDIO,
    PS_HW6_RTOS_OWNER_DISPLAY,
    PS_HW6_RTOS_OWNER_COMM,
    PS_HW6_RTOS_OWNER_SENSOR,
    PS_HW6_RTOS_OWNER_INPUT,
    PS_HW6_RTOS_OWNER_STORAGE
  };
  uint32_t cycle_index;
  uint32_t index;

  PS_HW6_RTOS_SetPowerDebug(GPIO_PIN_SET);
  g_ps_hw6_owner_probe.workflow_start_tick = (uint32_t)tx_time_get();
  g_ps_hw6_owner_probe.power_command_tick =
    g_ps_hw6_owner_probe.workflow_start_tick;
  PS_HW6_OwnerStateMachines_BeginWorkflow();
  (void)PS_HW6_OwnerStateMachines_Stabilize(PS_HW6_RTOS_OWNER_POWER);
  PS_HW6_OwnerStateMachines_RecordCommand(
    PS_HW6_RTOS_OWNER_POWER,
    g_ps_hw6_owner_probe.power_command_send_status,
    TX_SUCCESS,
    PS_HW6_RTOS_ACK_OWNER(PS_HW6_RTOS_OWNER_POWER));

  for (index = 0U; index < (sizeof(owner_order) / sizeof(owner_order[0]));
       ++index)
  {
    const uint32_t owner_id = owner_order[index];
    const ULONG expected_ack = PS_HW6_RTOS_ACK_OWNER(owner_id);
    ULONG actual_flags = 0UL;
    UINT send_status = PS_HW6_RTOS_SendCommand(
      owner_id, PS_HW6_RTOS_COMMAND_STABILIZE);
    UINT wait_status = send_status;

    if (send_status == TX_SUCCESS)
    {
      wait_status = tx_event_flags_get(
        &ps_event_groups[PS_HW6_RTOS_EVENT_DEBUG_INDEX],
        expected_ack, TX_AND_CLEAR, &actual_flags,
        PS_HW6_RTOS_StabilizeAckWaitTicks(owner_id));
    }
    PS_HW6_OwnerStateMachines_RecordCommand(
      owner_id, send_status, wait_status, (uint32_t)actual_flags);

    if (owner_id == PS_HW6_RTOS_OWNER_DISPLAY)
    {
      g_ps_hw6_owner_probe.display_command_send_status = send_status;
      g_ps_hw6_owner_probe.display_ack_wait_status = wait_status;
      g_ps_hw6_owner_probe.display_ack_flags = (uint32_t)actual_flags;
    }
    else if (owner_id == PS_HW6_RTOS_OWNER_AUDIO)
    {
      g_ps_hw6_owner_probe.audio_command_send_status = send_status;
      g_ps_hw6_owner_probe.audio_ack_wait_status = wait_status;
      g_ps_hw6_owner_probe.audio_ack_flags = (uint32_t)actual_flags;
    }
  }

  for (cycle_index = 0U;
       cycle_index < PS_HW6_OWNER_SM_CYCLE_COUNT;
       ++cycle_index)
  {
    PS_HW6_OwnerStateMachines_BeginCycle(cycle_index);
    (void)PS_HW6_OwnerStateMachines_Resume(
      PS_HW6_RTOS_OWNER_POWER, cycle_index);
    PS_HW6_OwnerStateMachines_RecordCycleCommand(
      cycle_index, PS_HW6_OWNER_SM_CYCLE_RESUME,
      PS_HW6_RTOS_OWNER_POWER, TX_SUCCESS, TX_SUCCESS,
      PS_HW6_RTOS_ACK_OWNER(PS_HW6_RTOS_OWNER_POWER));

    for (index = 0U;
         index < (sizeof(resume_order) / sizeof(resume_order[0]));
         ++index)
    {
      PS_HW6_RTOS_RunCycleOwnerCommand(
        cycle_index, PS_HW6_OWNER_SM_CYCLE_RESUME,
        resume_order[index], PS_HW6_RTOS_COMMAND_RESUME);
    }
    PS_HW6_OwnerStateMachines_RecordCycleActiveStates(cycle_index);

    for (index = 0U;
         index < (sizeof(quiesce_order) / sizeof(quiesce_order[0]));
         ++index)
    {
      PS_HW6_RTOS_RunCycleOwnerCommand(
        cycle_index, PS_HW6_OWNER_SM_CYCLE_QUIESCE,
        quiesce_order[index], PS_HW6_RTOS_COMMAND_QUIESCE);
    }
    (void)PS_HW6_OwnerStateMachines_Quiesce(
      PS_HW6_RTOS_OWNER_POWER, cycle_index);
    PS_HW6_OwnerStateMachines_RecordCycleCommand(
      cycle_index, PS_HW6_OWNER_SM_CYCLE_QUIESCE,
      PS_HW6_RTOS_OWNER_POWER, TX_SUCCESS, TX_SUCCESS,
      PS_HW6_RTOS_ACK_OWNER(PS_HW6_RTOS_OWNER_POWER));
    PS_HW6_OwnerStateMachines_EndCycle(cycle_index);
  }

  PS_HW6_OwnerStateMachines_EndWorkflow();

  g_ps_hw6_owner_probe.workflow_end_tick = (uint32_t)tx_time_get();
  g_ps_hw6_owner_probe.complete = 1UL;
  g_ps_hw6_owner_probe.success =
    ((g_ps_hw6_owner_probe.services_init_status == TX_SUCCESS) &&
     (g_ps_hw6_owner_sm_probe.success != 0UL)) ?
    1UL : 0UL;
  PS_HW6_OwnerServices_MarkComplete();
  PS_HW6_RTOS_SetPowerDebug(GPIO_PIN_RESET);
}

static void PS_HW6_RTOS_RuntimeRecord(uint32_t event,
                                       uint32_t status)
{
  g_ps_hw6_rtos_probe.runtime_event_count++;
  g_ps_hw6_rtos_probe.runtime_last_event = event;
  g_ps_hw6_rtos_probe.runtime_last_status = status;
  g_ps_hw6_rtos_probe.runtime_last_tick = (uint32_t)tx_time_get();
}

static uint32_t PS_HW6_RTOS_RuntimeReturnPage(void)
{
  uint32_t page = g_ps_ui_router_probe.current_page;

  if (page == (uint32_t)PS_UI_ROUTER_PAGE_SHUTDOWN)
  {
    page = g_ps_ui_router_probe.shutdown_return_page;
  }
  if ((page == (uint32_t)PS_UI_ROUTER_PAGE_BOOTSTRAP) ||
      (page == (uint32_t)PS_UI_ROUTER_PAGE_SHUTDOWN) ||
      (page == (uint32_t)PS_UI_ROUTER_PAGE_ERROR))
  {
    page = (uint32_t)PS_UI_ROUTER_PAGE_MENU;
  }
  return page;
}

static uint32_t PS_HW6_RTOS_RuntimeFallbackClass(void)
{
  uint32_t runtime_class = g_ps_hw6_rtos_probe.runtime_current_class;

  if ((runtime_class == (uint32_t)PS_HW6_RUNTIME_CLASS_NONE) ||
      (runtime_class == (uint32_t)PS_HW6_RUNTIME_CLASS_INSTALLER))
  {
    return (uint32_t)PS_HW6_RUNTIME_CLASS_SHELL;
  }
  return runtime_class;
}

static void PS_HW6_RTOS_RuntimeSetState(uint32_t runtime_class,
                                        uint32_t execution,
                                        uint32_t lifecycle)
{
  if (g_ps_hw6_rtos_probe.runtime_current_class != runtime_class)
  {
    g_ps_hw6_rtos_probe.runtime_previous_class =
      g_ps_hw6_rtos_probe.runtime_current_class;
  }

  g_ps_hw6_rtos_probe.runtime_current_class = runtime_class;
  g_ps_hw6_rtos_probe.runtime_execution = execution;
  g_ps_hw6_rtos_probe.runtime_lifecycle = lifecycle;
}

static void PS_HW6_RTOS_RuntimeBootShell(void)
{
  g_ps_hw6_rtos_probe.runtime_boot_shell_count++;
  if (PS_SceneRuntime_StateSceneActive() != 0UL)
  {
    g_ps_hw6_rtos_probe.runtime_boot_shell_reject_count++;
    PS_HW6_RTOS_RuntimeRecord(
      (uint32_t)PS_HW6_RUNTIME_EVENT_BOOT_SHELL,
      (uint32_t)PS_STATUS_INVALID_STATE);
    return;
  }
  g_ps_hw6_rtos_probe.runtime_return_class =
    (uint32_t)PS_HW6_RUNTIME_CLASS_SHELL;
  g_ps_hw6_rtos_probe.runtime_return_page =
    (uint32_t)PS_UI_ROUTER_PAGE_MENU;
  g_ps_hw6_rtos_probe.runtime_active_package_id = 0UL;
  g_ps_hw6_rtos_probe.runtime_active_unit_id = 0UL;
  PS_HW6_RTOS_RuntimeSetState(
    (uint32_t)PS_HW6_RUNTIME_CLASS_SHELL,
    (uint32_t)PS_HW6_RUNTIME_EXEC_REACTIVE,
    (uint32_t)PS_HW6_RUNTIME_LIFECYCLE_RUNNING);
  PS_HW6_RTOS_RuntimeRecord(
    (uint32_t)PS_HW6_RUNTIME_EVENT_BOOT_SHELL,
    (uint32_t)PS_STATUS_OK);
}

static void PS_HW6_RTOS_RuntimeEnterInstaller(void)
{
  g_ps_hw6_rtos_probe.runtime_installer_enter_count++;
  g_ps_hw6_rtos_probe.runtime_return_class =
    PS_HW6_RTOS_RuntimeFallbackClass();
  g_ps_hw6_rtos_probe.runtime_return_page =
    PS_HW6_RTOS_RuntimeReturnPage();
  g_ps_hw6_rtos_probe.runtime_active_package_id = 0UL;
  g_ps_hw6_rtos_probe.runtime_active_unit_id = 0UL;
  PS_HW6_RTOS_RuntimeSetState(
    (uint32_t)PS_HW6_RUNTIME_CLASS_INSTALLER,
    (uint32_t)PS_HW6_RUNTIME_EXEC_REACTIVE,
    (uint32_t)PS_HW6_RUNTIME_LIFECYCLE_RUNNING);
  PS_HW6_RTOS_RuntimeRecord(
    (uint32_t)PS_HW6_RUNTIME_EVENT_INSTALLER_ENTER,
    (uint32_t)PS_STATUS_OK);
}

static void PS_HW6_RTOS_RuntimeCompleteInstaller(void)
{
  uint32_t return_class = g_ps_hw6_rtos_probe.runtime_return_class;

  if (return_class == (uint32_t)PS_HW6_RUNTIME_CLASS_NONE)
  {
    return_class = (uint32_t)PS_HW6_RUNTIME_CLASS_SHELL;
  }

  g_ps_hw6_rtos_probe.runtime_installer_complete_count++;
  g_ps_hw6_rtos_probe.runtime_active_package_id = 0UL;
  g_ps_hw6_rtos_probe.runtime_active_unit_id = 0UL;
  PS_HW6_RTOS_RuntimeSetState(
    return_class,
    (uint32_t)PS_HW6_RUNTIME_EXEC_REACTIVE,
    (uint32_t)PS_HW6_RUNTIME_LIFECYCLE_RUNNING);
  g_ps_hw6_rtos_probe.runtime_return_class = return_class;
  PS_HW6_RTOS_RuntimeRecord(
    (uint32_t)PS_HW6_RUNTIME_EVENT_INSTALLER_COMPLETE,
    (uint32_t)PS_STATUS_OK);
}

static void PS_HW6_RTOS_RuntimeErrorInstaller(void)
{
  g_ps_hw6_rtos_probe.runtime_installer_error_count++;
  PS_HW6_RTOS_RuntimeSetState(
    (uint32_t)PS_HW6_RUNTIME_CLASS_INSTALLER,
    (uint32_t)PS_HW6_RUNTIME_EXEC_REACTIVE,
    (uint32_t)PS_HW6_RUNTIME_LIFECYCLE_ERROR);
  PS_HW6_RTOS_RuntimeRecord(
    (uint32_t)PS_HW6_RUNTIME_EVENT_INSTALLER_ERROR,
    (uint32_t)PS_STATUS_INTERNAL_ERROR);
}

static void PS_HW6_RTOS_RuntimeRecordAdmission(uint32_t runtime_class,
                                               uint32_t execution,
                                               uint32_t capabilities,
                                               UINT clock_status)
{
  g_ps_hw6_rtos_probe.runtime_admission_request_count++;
  g_ps_hw6_rtos_probe.runtime_admission_last_class = runtime_class;
  g_ps_hw6_rtos_probe.runtime_admission_last_execution = execution;
  g_ps_hw6_rtos_probe.runtime_admission_last_capabilities = capabilities;
  g_ps_hw6_rtos_probe.runtime_admission_last_status = (uint32_t)clock_status;

  if (execution == (uint32_t)PS_HW6_RUNTIME_EXEC_REALTIME)
  {
    g_ps_hw6_rtos_probe.runtime_admission_realtime_status =
      (uint32_t)clock_status;
  }
  else
  {
    g_ps_hw6_rtos_probe.runtime_admission_reactive_status =
      (uint32_t)clock_status;
  }
}

static void PS_HW6_RTOS_RequestPackageInstallErrorUi(void)
{
  if ((g_ps_ui_router_probe.package_state ==
       (uint32_t)PS_UI_ROUTER_PACKAGE_INSTALLING) &&
      (g_ps_ui_router_request == 0UL))
  {
    g_ps_ui_router_request_event =
      (uint32_t)PS_UI_ROUTER_EVENT_PACKAGE_INSTALL_STUB_ERROR;
    g_ps_ui_router_request = 1UL;
  }
}

static void PS_HW6_RTOS_RuntimePackageActivateStub(uint32_t runtime_class,
                                                   uint32_t execution,
                                                   uint32_t capabilities,
                                                   UINT clock_status)
{
  UINT admission_status = clock_status;
  uint32_t runtime_status = (clock_status == TX_SUCCESS) ?
    (uint32_t)PS_STATUS_OK : (uint32_t)PS_STATUS_INTERNAL_ERROR;
  uint32_t event =
    (uint32_t)PS_HW6_RUNTIME_EVENT_PACKAGE_REACTIVE_ACTIVATE_STUB;

  g_ps_hw6_rtos_probe.runtime_package_activate_stub_count++;
  if (execution == (uint32_t)PS_HW6_RUNTIME_EXEC_REALTIME)
  {
    g_ps_hw6_rtos_probe.runtime_package_realtime_activate_stub_count++;
    event = (uint32_t)PS_HW6_RUNTIME_EVENT_PACKAGE_REALTIME_ACTIVATE_STUB;
  }
  else
  {
    g_ps_hw6_rtos_probe.runtime_package_reactive_activate_stub_count++;
  }

  if ((runtime_class == (uint32_t)PS_HW6_RUNTIME_CLASS_LP_GRAPH) &&
      (clock_status == TX_SUCCESS) &&
      (g_ps_package_source_override ==
       (uint32_t)PS_PACKAGE_SOURCE_OVERRIDE_DEFAULT) &&
      (g_ps_package_source_probe.staged_available == 0UL) &&
      (PS_HW6_RTOS_RequestStoragePackageLoadAndWait() != TX_SUCCESS))
  {
    PS_HW6_RTOS_RuntimeSetState(
      runtime_class,
      execution,
      (uint32_t)PS_HW6_RUNTIME_LIFECYCLE_ERROR);
    PS_HW6_RTOS_RuntimeRecordAdmission(
      runtime_class,
      execution,
      capabilities,
      TX_NOT_AVAILABLE);
    PS_HW6_RTOS_RuntimeRecord(
      event,
      (uint32_t)PS_STATUS_INTERNAL_ERROR);
    PS_HW6_RTOS_RequestPackageInstallErrorUi();
    return;
  }

  g_ps_hw6_rtos_probe.runtime_return_class =
    PS_HW6_RTOS_RuntimeFallbackClass();
  g_ps_hw6_rtos_probe.runtime_return_page =
    PS_HW6_RTOS_RuntimeReturnPage();
  g_ps_hw6_rtos_probe.runtime_active_package_id =
    g_ps_hw6_rtos_probe.runtime_package_activate_stub_count;
  g_ps_hw6_rtos_probe.runtime_active_unit_id = runtime_class;
  PS_HW6_RTOS_RuntimeSetState(
    runtime_class,
    execution,
    (uint32_t)PS_HW6_RUNTIME_LIFECYCLE_RUNNING);
  if ((runtime_class == (uint32_t)PS_HW6_RUNTIME_CLASS_LP_GRAPH) &&
      (clock_status == TX_SUCCESS))
  {
    if (PS_SceneRuntime_EnterStateScene() == PS_SCENE_RUNTIME_INDEX_INVALID)
    {
      if (g_ps_scene_runtime_probe.activation_status ==
          PS_SCENE_RUNTIME_STATUS_NO_PACKAGE)
      {
        UINT release_status = PS_HW6_RTOS_RequestRuntimeClockCapabilities(
          PS_HW6_RTOS_RUNTIME_CLOCK_REASON_RELEASE,
          0UL);

        admission_status = TX_NO_INSTANCE;
        runtime_status = (uint32_t)PS_STATUS_UNSUPPORTED;
        g_ps_hw6_rtos_probe.runtime_active_package_id = 0UL;
        g_ps_hw6_rtos_probe.runtime_active_unit_id = 0UL;
        if (release_status == TX_SUCCESS)
        {
          PS_HW6_RTOS_RuntimeSetState(
            (uint32_t)PS_HW6_RUNTIME_CLASS_SHELL,
            (uint32_t)PS_HW6_RUNTIME_EXEC_REACTIVE,
            (uint32_t)PS_HW6_RUNTIME_LIFECYCLE_RUNNING);
          if (g_ps_ui_router_probe.package_state ==
              (uint32_t)PS_UI_ROUTER_PACKAGE_INSTALLING)
          {
            PS_HW6_RTOS_RequestPackageInstallErrorUi();
          }
          else
          {
            g_ps_ui_router_request_event =
              (uint32_t)PS_UI_ROUTER_EVENT_RUNTIME_UNAVAILABLE;
            g_ps_ui_router_request = 1UL;
          }
        }
        else
        {
          runtime_status = (uint32_t)PS_STATUS_INTERNAL_ERROR;
          PS_HW6_RTOS_RuntimeSetState(
            runtime_class,
            execution,
            (uint32_t)PS_HW6_RUNTIME_LIFECYCLE_ERROR);
        }
      }
      else
      {
        admission_status = TX_PTR_ERROR;
        runtime_status = (uint32_t)PS_STATUS_INTERNAL_ERROR;
        PS_HW6_RTOS_RuntimeSetState(
          runtime_class,
          execution,
          (uint32_t)PS_HW6_RUNTIME_LIFECYCLE_ERROR);
        PS_HW6_RTOS_RequestPackageInstallErrorUi();
      }
    }
    else
    {
      PS_HW6_RTOS_RuntimeInteractionBegin((uint32_t)tx_time_get());
      g_ps_ui_router_request_event =
        (uint32_t)PS_UI_ROUTER_EVENT_LAUNCH_RUNTIME;
      g_ps_ui_router_request = 1UL;
    }
  }
  PS_HW6_RTOS_RuntimeRecordAdmission(runtime_class,
                                     execution,
                                     capabilities,
                                     admission_status);
  PS_HW6_RTOS_RuntimeRecord(event,
                            runtime_status);
}

static void PS_HW6_RTOS_RuntimePackageReturn(void)
{
  UINT release_status = TX_SUCCESS;

  g_ps_hw6_rtos_probe.runtime_package_return_count++;
  if (g_ps_hw6_rtos_probe.runtime_active_capabilities != 0UL)
  {
    release_status = PS_HW6_RTOS_RequestRuntimeClockCapabilities(
      PS_HW6_RTOS_RUNTIME_CLOCK_REASON_RELEASE,
      0UL);
    g_ps_hw6_rtos_probe.runtime_return_clock_release_status =
      (uint32_t)release_status;
  }
  else
  {
    g_ps_hw6_rtos_probe.runtime_return_clock_release_status =
      PS_HW6_RTOS_STATUS_NOT_RUN;
  }

  if (release_status == TX_SUCCESS)
  {
    PS_HW6_RTOS_RuntimeInteractionEnd();
    PS_SceneRuntime_ExitStateScene();
    g_ps_hw6_rtos_probe.runtime_active_package_id = 0UL;
    g_ps_hw6_rtos_probe.runtime_active_unit_id = 0UL;
    PS_HW6_RTOS_RuntimeSetState(
      (uint32_t)PS_HW6_RUNTIME_CLASS_SHELL,
      (uint32_t)PS_HW6_RUNTIME_EXEC_REACTIVE,
      (uint32_t)PS_HW6_RUNTIME_LIFECYCLE_RUNNING);
    g_ps_ui_router_request_event =
      (uint32_t)PS_UI_ROUTER_EVENT_RUNTIME_RETURNED;
    g_ps_ui_router_request = 1UL;
  }

  PS_HW6_RTOS_RuntimeRecord(
    (uint32_t)PS_HW6_RUNTIME_EVENT_PACKAGE_RETURN,
    (release_status == TX_SUCCESS) ?
    (uint32_t)PS_STATUS_OK : (uint32_t)PS_STATUS_INTERNAL_ERROR);
}

static HAL_StatusTypeDef PS_HW6_RTOS_InteractionRtcReadUnits(
  uint32_t *units)
{
  RTC_TimeTypeDef time;
  RTC_DateTypeDef date;
  HAL_StatusTypeDef status;
  uint32_t seconds;
  uint32_t elapsed_subseconds;
  uint32_t fraction_units;

  if (units == NULL)
  {
    return HAL_ERROR;
  }
  status = HAL_RTC_GetTime(&hrtc, &time, RTC_FORMAT_BIN);
  if (status == HAL_OK)
  {
    status = HAL_RTC_GetDate(&hrtc, &date, RTC_FORMAT_BIN);
  }
  if (status != HAL_OK)
  {
    return status;
  }

  seconds = (((uint32_t)time.Hours * 60UL) +
             (uint32_t)time.Minutes) * 60UL + (uint32_t)time.Seconds;
  elapsed_subseconds = (time.SecondFraction >= time.SubSeconds) ?
    time.SecondFraction - time.SubSeconds : 0UL;
  fraction_units = (uint32_t)
    ((((uint64_t)elapsed_subseconds * PS_HW6_RTOS_RTC_UNITS_PER_SECOND) +
      (uint64_t)time.SecondFraction) /
     ((uint64_t)time.SecondFraction + 1ULL));
  if (fraction_units >= PS_HW6_RTOS_RTC_UNITS_PER_SECOND)
  {
    fraction_units = PS_HW6_RTOS_RTC_UNITS_PER_SECOND - 1UL;
  }
  *units = (seconds * PS_HW6_RTOS_RTC_UNITS_PER_SECOND) + fraction_units;
  return HAL_OK;
}

static void PS_HW6_RTOS_RuntimeInteractionAdvanceEpoch(void)
{
  ps_runtime_interaction_epoch++;
  if (ps_runtime_interaction_epoch == 0UL)
  {
    ps_runtime_interaction_epoch = 1UL;
  }
  ps_runtime_interaction_timeout_forced = 0UL;
  ps_runtime_interaction_timeout_forced_epoch = 0UL;
  ps_runtime_interaction_rtc_command_queued = 0UL;
  g_ps_hw6_rtos_probe.runtime_interaction_epoch =
    ps_runtime_interaction_epoch;
}

void RTC_IRQHandler(void)
{
  UINT status = TX_NOT_AVAILABLE;

  if (__HAL_RTC_WAKEUPTIMER_GET_FLAG(&hrtc, RTC_FLAG_WUTF) != 0UL)
  {
    __HAL_RTC_WAKEUPTIMER_CLEAR_FLAG(&hrtc, RTC_FLAG_WUTF);
    ps_runtime_interaction_rtc_irq_expired = 1UL;
    if (ps_runtime_interaction_rtc_armed != 0UL)
    {
      ps_runtime_interaction_timeout_forced = 1UL;
      ps_runtime_interaction_timeout_forced_epoch =
        ps_runtime_interaction_rtc_armed_epoch;
      if (ps_runtime_interaction_rtc_command_queued == 0UL)
      {
        ps_runtime_interaction_rtc_command_queued = 1UL;
        status = PS_HW6_RTOS_RequestRuntimeCommand(
          PS_HW6_RTOS_COMMAND_RUNTIME_INTERACTION_TIMEOUT);
      }
      g_ps_hw6_rtos_probe.runtime_interaction_rtc_command_status =
        (uint32_t)status;
    }
  }
}

uint32_t PS_HW6_RTOS_InteractionStop2TimeoutPrepare(void)
{
  uint32_t now_tick;
  uint32_t remaining_ticks;
  uint32_t remaining_ms;
  uint32_t wake_clock;
  uint32_t wake_counter;
  uint64_t wake_counts;
  HAL_StatusTypeDef status;

  ps_runtime_interaction_rtc_armed = 0UL;
  ps_runtime_interaction_rtc_irq_expired = 0UL;
  ps_runtime_interaction_rtc_command_queued = 0UL;
  if ((ps_runtime_interaction_mode !=
       PS_SCENE_RUNTIME_INTERACTION_TIMEOUT) ||
      (ps_runtime_interaction_state !=
       PS_HW6_RUNTIME_INTERACTION_STATE_ACTIVE) ||
      (ps_runtime_interaction_deadline_tick == 0UL))
  {
    g_ps_hw6_rtos_probe.runtime_interaction_rtc_arm_status =
      (uint32_t)HAL_OK;
    return (uint32_t)HAL_OK;
  }

  now_tick = (uint32_t)tx_time_get();
  if (PS_HW6_RTOS_TimeReached(
        now_tick, ps_runtime_interaction_deadline_tick) != 0UL)
  {
    ps_runtime_interaction_timeout_forced = 1UL;
    ps_runtime_interaction_timeout_forced_epoch =
      ps_runtime_interaction_epoch;
    g_ps_hw6_rtos_probe.runtime_interaction_rtc_arm_status =
      (uint32_t)HAL_BUSY;
    g_ps_hw6_rtos_probe.runtime_interaction_rtc_command_status =
      (uint32_t)PS_HW6_RTOS_RequestRuntimeCommand(
        PS_HW6_RTOS_COMMAND_RUNTIME_INTERACTION_TIMEOUT);
    return (uint32_t)HAL_BUSY;
  }

  remaining_ticks = ps_runtime_interaction_deadline_tick - now_tick;
  status = PS_HW6_RTOS_InteractionRtcReadUnits(
    &ps_runtime_interaction_rtc_start_units);
  if (status != HAL_OK)
  {
    g_ps_hw6_rtos_probe.runtime_interaction_rtc_arm_status =
      (uint32_t)status;
    return (uint32_t)status;
  }

  remaining_ms = (uint32_t)
    ((((uint64_t)remaining_ticks * 1000ULL) +
      (uint64_t)TX_TIMER_TICKS_PER_SECOND - 1ULL) /
     (uint64_t)TX_TIMER_TICKS_PER_SECOND);
  if (remaining_ms <= PS_HW6_RTOS_RTC_DIV16_MAX_MS)
  {
    wake_clock = RTC_WAKEUPCLOCK_RTCCLK_DIV16;
    wake_counts =
      (((uint64_t)remaining_ms *
        PS_HW6_RTOS_RTC_DIV16_COUNTS_PER_SECOND) + 999ULL) / 1000ULL;
  }
  else
  {
    wake_clock = RTC_WAKEUPCLOCK_CK_SPRE_16BITS;
    wake_counts = ((uint64_t)remaining_ms + 999ULL) / 1000ULL;
  }
  if (wake_counts == 0ULL)
  {
    wake_counts = 1ULL;
  }
  if (wake_counts > 65536ULL)
  {
    wake_counts = 65536ULL;
  }
  wake_counter = (uint32_t)(wake_counts - 1ULL);

  HAL_NVIC_SetPriority(RTC_IRQn,
                       (uint32_t)KNOB_POWER_RTC_WAKE_IRQ_PRIORITY,
                       0U);
  HAL_NVIC_ClearPendingIRQ(RTC_IRQn);
  HAL_NVIC_EnableIRQ(RTC_IRQn);
  status = HAL_RTCEx_SetWakeUpTimer_IT(&hrtc,
                                       wake_counter,
                                       wake_clock,
                                       0UL);
  g_ps_hw6_rtos_probe.runtime_interaction_rtc_arm_count++;
  g_ps_hw6_rtos_probe.runtime_interaction_rtc_arm_status =
    (uint32_t)status;
  g_ps_hw6_rtos_probe.runtime_interaction_rtc_clock = wake_clock;
  g_ps_hw6_rtos_probe.runtime_interaction_rtc_counter = wake_counter;
  g_ps_hw6_rtos_probe.runtime_interaction_rtc_remaining_ticks =
    remaining_ticks;
  if (status != HAL_OK)
  {
    HAL_NVIC_DisableIRQ(RTC_IRQn);
    return (uint32_t)status;
  }

  ps_runtime_interaction_rtc_remaining_ticks = remaining_ticks;
  ps_runtime_interaction_rtc_armed_epoch = ps_runtime_interaction_epoch;
  ps_runtime_interaction_rtc_armed = 1UL;
  return (uint32_t)HAL_OK;
}

void PS_HW6_RTOS_InteractionStop2TimeoutFinish(void)
{
  uint32_t end_units = ps_runtime_interaction_rtc_start_units;
  uint32_t elapsed_units = 0UL;
  uint32_t elapsed_ticks = 0UL;
  uint32_t expired;
  uint32_t now_tick;
  HAL_StatusTypeDef read_status;
  HAL_StatusTypeDef deactivate_status;
  UINT command_status = TX_NOT_AVAILABLE;

  if (ps_runtime_interaction_rtc_armed == 0UL)
  {
    return;
  }

  expired = (ps_runtime_interaction_rtc_irq_expired != 0UL) ? 1UL : 0UL;
  if (__HAL_RTC_WAKEUPTIMER_GET_FLAG(&hrtc, RTC_FLAG_WUTF) != 0UL)
  {
    expired = 1UL;
    ps_runtime_interaction_rtc_irq_expired = 1UL;
    __HAL_RTC_WAKEUPTIMER_CLEAR_FLAG(&hrtc, RTC_FLAG_WUTF);
  }
  read_status = PS_HW6_RTOS_InteractionRtcReadUnits(&end_units);
  deactivate_status = HAL_RTCEx_DeactivateWakeUpTimer(&hrtc);
  HAL_NVIC_ClearPendingIRQ(RTC_IRQn);
  HAL_NVIC_DisableIRQ(RTC_IRQn);

  if (read_status == HAL_OK)
  {
    elapsed_units = (end_units >= ps_runtime_interaction_rtc_start_units) ?
      end_units - ps_runtime_interaction_rtc_start_units :
      (PS_HW6_RTOS_RTC_UNITS_PER_DAY -
       ps_runtime_interaction_rtc_start_units) + end_units;
    elapsed_ticks = (uint32_t)
      ((((uint64_t)elapsed_units *
         (uint64_t)TX_TIMER_TICKS_PER_SECOND) +
        PS_HW6_RTOS_RTC_UNITS_PER_SECOND - 1ULL) /
       PS_HW6_RTOS_RTC_UNITS_PER_SECOND);
  }
  else
  {
    expired = 1UL;
  }
  if (elapsed_ticks >= ps_runtime_interaction_rtc_remaining_ticks)
  {
    expired = 1UL;
  }

  now_tick = (uint32_t)tx_time_get();
  if ((ps_runtime_interaction_rtc_armed_epoch ==
       ps_runtime_interaction_epoch) &&
      (ps_runtime_interaction_state ==
       PS_HW6_RUNTIME_INTERACTION_STATE_ACTIVE) &&
      (ps_runtime_interaction_mode ==
       PS_SCENE_RUNTIME_INTERACTION_TIMEOUT))
  {
    if (expired != 0UL)
    {
      ps_runtime_interaction_timeout_forced = 1UL;
      ps_runtime_interaction_timeout_forced_epoch =
        ps_runtime_interaction_rtc_armed_epoch;
      ps_runtime_interaction_deadline_tick = now_tick;
      g_ps_hw6_rtos_probe.runtime_interaction_rtc_expire_count++;
      if (ps_runtime_interaction_rtc_command_queued == 0UL)
      {
        ps_runtime_interaction_rtc_command_queued = 1UL;
        command_status = PS_HW6_RTOS_RequestRuntimeCommand(
          PS_HW6_RTOS_COMMAND_RUNTIME_INTERACTION_TIMEOUT);
      }
    }
    else
    {
      ps_runtime_interaction_deadline_tick = now_tick +
        (ps_runtime_interaction_rtc_remaining_ticks - elapsed_ticks);
    }
    g_ps_hw6_rtos_probe.runtime_interaction_deadline_tick =
      ps_runtime_interaction_deadline_tick;
  }

  ps_runtime_interaction_rtc_armed = 0UL;
  g_ps_hw6_rtos_probe.runtime_interaction_rtc_elapsed_ticks =
    elapsed_ticks;
  if (command_status != TX_NOT_AVAILABLE)
  {
    g_ps_hw6_rtos_probe.runtime_interaction_rtc_command_status =
      (uint32_t)command_status;
  }
  if ((read_status != HAL_OK) || (deactivate_status != HAL_OK))
  {
    g_ps_hw6_rtos_probe.runtime_interaction_rtc_arm_status =
      (read_status != HAL_OK) ? (uint32_t)read_status :
      (uint32_t)deactivate_status;
  }
}

static uint32_t PS_HW6_RTOS_RuntimeInteractionPeriodTicks(uint32_t ms)
{
  uint32_t ticks = PS_HW6_RTOS_MsToTicks(ms);

  return (ticks != 0UL) ? ticks : 1UL;
}

static void PS_HW6_RTOS_RuntimeInteractionBegin(uint32_t now_tick)
{
  PS_HW6_RTOS_RuntimeInteractionAdvanceEpoch();
  ps_runtime_interaction_mode = PS_SceneRuntime_InteractionMode();
  if (g_ps_hw6_runtime_interaction_test_continuous != 0UL)
  {
    ps_runtime_interaction_mode = PS_SCENE_RUNTIME_INTERACTION_CONTINUOUS;
  }
  ps_runtime_interaction_state = PS_HW6_RUNTIME_INTERACTION_STATE_ACTIVE;
  ps_runtime_interaction_cue_active = 0UL;
  ps_runtime_interaction_cue_deadline_tick = 0UL;
  ps_runtime_interaction_activation_active = 0UL;
  ps_runtime_interaction_activation_frame = 0UL;
  ps_runtime_interaction_activation_deadline_tick = 0UL;
  ps_runtime_interaction_deadline_tick =
    (ps_runtime_interaction_mode ==
     PS_SCENE_RUNTIME_INTERACTION_TIMEOUT) ?
      now_tick + PS_HW6_RTOS_RuntimeInteractionPeriodTicks(
        (uint32_t)KNOB_RUNTIME_INTERACTION_TIMEOUT_MS) : 0UL;
  PS_HW6_OwnerStateMachines_SetJoystickStop2WakeAllowed(1UL);

  g_ps_hw6_rtos_probe.runtime_interaction_mode =
    ps_runtime_interaction_mode;
  g_ps_hw6_rtos_probe.runtime_interaction_state =
    ps_runtime_interaction_state;
  g_ps_hw6_rtos_probe.runtime_interaction_deadline_tick =
    ps_runtime_interaction_deadline_tick;
  g_ps_hw6_rtos_probe.runtime_interaction_cue_active = 0UL;
  g_ps_hw6_rtos_probe.runtime_interaction_cue_deadline_tick = 0UL;
  g_ps_hw6_rtos_probe.runtime_interaction_activation_active = 0UL;
  g_ps_hw6_rtos_probe.runtime_interaction_activation_frame = 0UL;
  g_ps_hw6_rtos_probe.runtime_interaction_activation_deadline_tick = 0UL;
  g_ps_hw6_rtos_probe.runtime_interaction_active_count++;
  g_ps_hw6_rtos_probe.runtime_interaction_active_tick = now_tick;
  g_ps_hw6_rtos_probe.runtime_interaction_joystick_wake_allowed = 1UL;
}

static void PS_HW6_RTOS_RuntimeInteractionEnd(void)
{
  PS_HW6_RTOS_RuntimeInteractionAdvanceEpoch();
  ps_runtime_interaction_mode = 0UL;
  ps_runtime_interaction_state = PS_HW6_RUNTIME_INTERACTION_STATE_NONE;
  ps_runtime_interaction_deadline_tick = 0UL;
  ps_runtime_interaction_cue_active = 0UL;
  ps_runtime_interaction_cue_deadline_tick = 0UL;
  ps_runtime_interaction_activation_active = 0UL;
  ps_runtime_interaction_activation_frame = 0UL;
  ps_runtime_interaction_activation_deadline_tick = 0UL;
  PS_HW6_OwnerStateMachines_SetJoystickStop2WakeAllowed(1UL);

  g_ps_hw6_rtos_probe.runtime_interaction_mode = 0UL;
  g_ps_hw6_rtos_probe.runtime_interaction_state =
    PS_HW6_RUNTIME_INTERACTION_STATE_NONE;
  g_ps_hw6_rtos_probe.runtime_interaction_deadline_tick = 0UL;
  g_ps_hw6_rtos_probe.runtime_interaction_cue_active = 0UL;
  g_ps_hw6_rtos_probe.runtime_interaction_cue_deadline_tick = 0UL;
  g_ps_hw6_rtos_probe.runtime_interaction_activation_active = 0UL;
  g_ps_hw6_rtos_probe.runtime_interaction_activation_frame = 0UL;
  g_ps_hw6_rtos_probe.runtime_interaction_activation_deadline_tick = 0UL;
  g_ps_hw6_rtos_probe.runtime_interaction_joystick_wake_allowed = 1UL;
}

static void PS_HW6_RTOS_RuntimeInteractionRefresh(uint32_t logical_event,
                                                   uint32_t input_id,
                                                   uint32_t now_tick)
{
  if ((ps_runtime_interaction_mode !=
       PS_SCENE_RUNTIME_INTERACTION_TIMEOUT) ||
      (ps_runtime_interaction_state !=
       PS_HW6_RUNTIME_INTERACTION_STATE_ACTIVE) ||
      (PS_SceneRuntime_InputIsMeaningful(logical_event, input_id) == 0UL))
  {
    return;
  }

  PS_HW6_RTOS_RuntimeInteractionAdvanceEpoch();
  ps_runtime_interaction_deadline_tick =
    now_tick + PS_HW6_RTOS_RuntimeInteractionPeriodTicks(
      (uint32_t)KNOB_RUNTIME_INTERACTION_TIMEOUT_MS);
  g_ps_hw6_rtos_probe.runtime_interaction_deadline_tick =
    ps_runtime_interaction_deadline_tick;
  g_ps_hw6_rtos_probe.runtime_interaction_activity_refresh_count++;
}

static void PS_HW6_RTOS_RuntimeInteractionShowCue(uint32_t now_tick)
{
  if ((ps_runtime_interaction_state !=
       PS_HW6_RUNTIME_INTERACTION_STATE_INACTIVE) ||
      (ps_runtime_interaction_cue_active != 0UL))
  {
    return;
  }

  ps_runtime_interaction_cue_active = 1UL;
  ps_runtime_interaction_cue_deadline_tick =
    now_tick + PS_HW6_RTOS_RuntimeInteractionPeriodTicks(
      (uint32_t)KNOB_RUNTIME_INACTIVE_CUE_MS);
  g_ps_hw6_rtos_probe.runtime_interaction_cue_active = 1UL;
  g_ps_hw6_rtos_probe.runtime_interaction_cue_show_count++;
  g_ps_hw6_rtos_probe.runtime_interaction_cue_deadline_tick =
    ps_runtime_interaction_cue_deadline_tick;
  (void)PS_HW6_RTOS_SendDisplayUiRenderCommand(
    (uint32_t)PS_UI_ROUTER_PAGE_INTERACTION_CUE,
    (uint32_t)PS_UI_ROUTER_CAL_NONE,
    PS_SceneRuntime_StateFocusIndex(),
    (uint32_t)PS_UI_ROUTER_SHUTDOWN_NONE,
    0UL);
}

static void PS_HW6_RTOS_RuntimeInteractionActivate(uint32_t now_tick)
{
  UINT status;

  if (ps_runtime_interaction_state !=
      PS_HW6_RUNTIME_INTERACTION_STATE_INACTIVE)
  {
    g_ps_hw6_rtos_probe.runtime_interaction_start_status =
      (uint32_t)TX_NOT_AVAILABLE;
    return;
  }

  PS_HW6_RTOS_RuntimeInteractionAdvanceEpoch();
  ps_runtime_interaction_state = PS_HW6_RUNTIME_INTERACTION_STATE_ACTIVE;
  ps_runtime_interaction_cue_active = 0UL;
  ps_runtime_interaction_cue_deadline_tick = 0UL;
  ps_runtime_interaction_activation_active = 1UL;
  ps_runtime_interaction_activation_frame = 0UL;
  ps_runtime_interaction_activation_deadline_tick =
    now_tick + PS_HW6_RTOS_RuntimeInteractionPeriodTicks(
      (uint32_t)KNOB_RUNTIME_ACTIVATION_EYE_FRAME_MS);
  ps_runtime_interaction_deadline_tick =
    (ps_runtime_interaction_mode ==
     PS_SCENE_RUNTIME_INTERACTION_TIMEOUT) ?
      now_tick + PS_HW6_RTOS_RuntimeInteractionPeriodTicks(
        (uint32_t)KNOB_RUNTIME_INTERACTION_TIMEOUT_MS) : 0UL;
  PS_HW6_OwnerStateMachines_SetJoystickStop2WakeAllowed(1UL);
  status = PS_HW6_RTOS_SendDisplayUiRenderCommand(
    (uint32_t)PS_UI_ROUTER_PAGE_INTERACTION_ACTIVATION,
    (uint32_t)PS_UI_ROUTER_CAL_NONE,
    ps_runtime_interaction_activation_frame,
    (uint32_t)PS_UI_ROUTER_SHUTDOWN_NONE,
    0UL);

  g_ps_hw6_rtos_probe.runtime_interaction_state =
    ps_runtime_interaction_state;
  g_ps_hw6_rtos_probe.runtime_interaction_deadline_tick =
    ps_runtime_interaction_deadline_tick;
  g_ps_hw6_rtos_probe.runtime_interaction_cue_active = 0UL;
  g_ps_hw6_rtos_probe.runtime_interaction_cue_deadline_tick = 0UL;
  g_ps_hw6_rtos_probe.runtime_interaction_activation_active = 1UL;
  g_ps_hw6_rtos_probe.runtime_interaction_activation_frame = 0UL;
  g_ps_hw6_rtos_probe.runtime_interaction_activation_count++;
  g_ps_hw6_rtos_probe.runtime_interaction_activation_deadline_tick =
    ps_runtime_interaction_activation_deadline_tick;
  g_ps_hw6_rtos_probe.runtime_interaction_activation_render_status =
    (uint32_t)status;
  g_ps_hw6_rtos_probe.runtime_interaction_active_count++;
  g_ps_hw6_rtos_probe.runtime_interaction_active_tick = now_tick;
  g_ps_hw6_rtos_probe.runtime_interaction_start_count++;
  g_ps_hw6_rtos_probe.runtime_interaction_start_status = (uint32_t)status;
  g_ps_hw6_rtos_probe.runtime_interaction_joystick_wake_allowed = 1UL;
}

static void PS_HW6_RTOS_RuntimeInteractionEnterInactive(uint32_t now_tick,
                                                        uint32_t manual)
{
  if (ps_runtime_interaction_state !=
      PS_HW6_RUNTIME_INTERACTION_STATE_ACTIVE)
  {
    if (manual != 0UL)
    {
      g_ps_hw6_rtos_probe.runtime_interaction_manual_request_status =
        (uint32_t)TX_NOT_AVAILABLE;
    }
    return;
  }

  ps_runtime_interaction_timeout_forced = 0UL;
  ps_runtime_interaction_timeout_forced_epoch = 0UL;
  ps_runtime_interaction_rtc_command_queued = 0UL;
  ps_runtime_interaction_state = PS_HW6_RUNTIME_INTERACTION_STATE_INACTIVE;
  ps_runtime_interaction_deadline_tick = 0UL;
  PS_HW6_OwnerStateMachines_SetJoystickStop2WakeAllowed(0UL);
  g_ps_hw6_rtos_probe.runtime_interaction_state =
    ps_runtime_interaction_state;
  g_ps_hw6_rtos_probe.runtime_interaction_deadline_tick = 0UL;
  g_ps_hw6_rtos_probe.runtime_interaction_inactive_count++;
  g_ps_hw6_rtos_probe.runtime_interaction_inactive_tick = now_tick;
  g_ps_hw6_rtos_probe.runtime_interaction_joystick_wake_allowed = 0UL;
  if (manual != 0UL)
  {
    g_ps_hw6_rtos_probe.runtime_interaction_manual_inactive_count++;
    g_ps_hw6_rtos_probe.runtime_interaction_manual_request_status =
      (uint32_t)TX_SUCCESS;
    if (g_ps_hw6_runtime_interaction_test_continuous != 0UL)
    {
      PS_HW6_RTOS_RuntimeInteractionShowCue(now_tick);
    }
  }

  if (PS_SceneRuntime_InactiveRoute() ==
      PS_SCENE_RUNTIME_INACTIVE_EXIT_SHELL)
  {
    PS_HW6_RTOS_RuntimePackageReturn();
  }
}

static void PS_HW6_RTOS_RuntimeInteractionService(uint32_t now_tick)
{
  UINT render_status;
  uint32_t forced_timeout =
    ((ps_runtime_interaction_timeout_forced != 0UL) &&
     (ps_runtime_interaction_timeout_forced_epoch ==
      ps_runtime_interaction_epoch)) ? 1UL : 0UL;

  if ((ps_runtime_interaction_activation_active != 0UL) &&
      (PS_HW6_RTOS_TimeReached(
         now_tick,
         ps_runtime_interaction_activation_deadline_tick) != 0UL))
  {
    ps_runtime_interaction_activation_frame++;
    if (ps_runtime_interaction_activation_frame <
        PS_HW6_RTOS_INTERACTION_ACTIVATION_FRAME_COUNT)
    {
      ps_runtime_interaction_activation_deadline_tick =
        now_tick + PS_HW6_RTOS_RuntimeInteractionPeriodTicks(
          (uint32_t)KNOB_RUNTIME_ACTIVATION_EYE_FRAME_MS);
      render_status = PS_HW6_RTOS_SendDisplayUiRenderCommand(
        (uint32_t)PS_UI_ROUTER_PAGE_INTERACTION_ACTIVATION,
        (uint32_t)PS_UI_ROUTER_CAL_NONE,
        ps_runtime_interaction_activation_frame,
        (uint32_t)PS_UI_ROUTER_SHUTDOWN_NONE,
        0UL);
      g_ps_hw6_rtos_probe.runtime_interaction_activation_frame =
        ps_runtime_interaction_activation_frame;
      g_ps_hw6_rtos_probe.runtime_interaction_activation_deadline_tick =
        ps_runtime_interaction_activation_deadline_tick;
      g_ps_hw6_rtos_probe.runtime_interaction_activation_render_status =
        (uint32_t)render_status;
    }
    else
    {
      render_status = PS_HW6_RTOS_SendDisplayUiRenderCommand(
        g_ps_ui_router_probe.current_page,
        g_ps_ui_router_probe.calibration_page,
        PS_SceneRuntime_StateFocusIndex(),
        g_ps_ui_router_probe.shutdown_state,
        g_ps_ui_router_probe.shutdown_countdown_seconds);
      ps_runtime_interaction_activation_active = 0UL;
      ps_runtime_interaction_activation_deadline_tick = 0UL;
      g_ps_hw6_rtos_probe.runtime_interaction_activation_active = 0UL;
      g_ps_hw6_rtos_probe.runtime_interaction_activation_complete_count++;
      g_ps_hw6_rtos_probe.runtime_interaction_activation_deadline_tick = 0UL;
      g_ps_hw6_rtos_probe.runtime_interaction_activation_render_status =
        (uint32_t)render_status;
    }
  }

  if ((ps_runtime_interaction_cue_active != 0UL) &&
      (PS_HW6_RTOS_TimeReached(
         now_tick, ps_runtime_interaction_cue_deadline_tick) != 0UL))
  {
    ps_runtime_interaction_cue_active = 0UL;
    ps_runtime_interaction_cue_deadline_tick = 0UL;
    g_ps_hw6_rtos_probe.runtime_interaction_cue_active = 0UL;
    g_ps_hw6_rtos_probe.runtime_interaction_cue_clear_count++;
    g_ps_hw6_rtos_probe.runtime_interaction_cue_deadline_tick = 0UL;
    PS_HW6_RTOS_SendCurrentUiRenderCommand();
  }

  if ((ps_runtime_interaction_mode !=
       PS_SCENE_RUNTIME_INTERACTION_TIMEOUT) ||
      (ps_runtime_interaction_state !=
       PS_HW6_RUNTIME_INTERACTION_STATE_ACTIVE) ||
      (ps_runtime_interaction_deadline_tick == 0UL) ||
      ((forced_timeout == 0UL) &&
       (PS_HW6_RTOS_TimeReached(
          now_tick, ps_runtime_interaction_deadline_tick) == 0UL)))
  {
    return;
  }

  PS_HW6_RTOS_RuntimeInteractionEnterInactive(now_tick, 0UL);
}

static void PS_HW6_RTOS_RuntimeSuspend(void)
{
  UINT release_status = TX_SUCCESS;
  uint32_t saved_capabilities =
    g_ps_hw6_rtos_probe.runtime_active_capabilities;

  g_ps_hw6_rtos_probe.runtime_suspend_count++;
  g_ps_hw6_rtos_probe.runtime_suspend_saved_class =
    g_ps_hw6_rtos_probe.runtime_current_class;
  g_ps_hw6_rtos_probe.runtime_suspend_saved_execution =
    g_ps_hw6_rtos_probe.runtime_execution;
  g_ps_hw6_rtos_probe.runtime_suspend_saved_lifecycle =
    g_ps_hw6_rtos_probe.runtime_lifecycle;
  g_ps_hw6_rtos_probe.runtime_suspend_saved_capabilities =
    saved_capabilities;
  g_ps_hw6_rtos_probe.runtime_suspend_clock_release_status =
    PS_HW6_RTOS_STATUS_NOT_RUN;

  if (saved_capabilities != 0UL)
  {
    release_status = PS_HW6_RTOS_RequestRuntimeClockCapabilities(
      PS_HW6_RTOS_RUNTIME_CLOCK_REASON_RELEASE,
      0UL);
    g_ps_hw6_rtos_probe.runtime_suspend_clock_release_status =
      (uint32_t)release_status;
  }

  if (release_status == TX_SUCCESS)
  {
    g_ps_hw6_rtos_probe.runtime_lifecycle =
      (uint32_t)PS_HW6_RUNTIME_LIFECYCLE_SUSPENDED;
  }

  PS_HW6_RTOS_RuntimeRecord(
    (uint32_t)PS_HW6_RUNTIME_EVENT_SUSPEND,
    (release_status == TX_SUCCESS) ?
    (uint32_t)PS_STATUS_OK : (uint32_t)PS_STATUS_INTERNAL_ERROR);
}

static void PS_HW6_RTOS_RuntimeResume(void)
{
  UINT request_status = TX_SUCCESS;
  uint32_t capabilities =
    g_ps_hw6_rtos_probe.runtime_suspend_saved_capabilities;
  uint32_t reason = PS_HW6_RTOS_RUNTIME_CLOCK_REASON_REACTIVE_TRANSACTION;

  g_ps_hw6_rtos_probe.runtime_resume_count++;
  g_ps_hw6_rtos_probe.runtime_resume_clock_request_status =
    PS_HW6_RTOS_STATUS_NOT_RUN;
  if (g_ps_hw6_rtos_probe.runtime_current_class ==
      (uint32_t)PS_HW6_RUNTIME_CLASS_NONE)
  {
    PS_HW6_RTOS_RuntimeBootShell();
    return;
  }

  if ((g_ps_hw6_rtos_probe.runtime_lifecycle ==
       (uint32_t)PS_HW6_RUNTIME_LIFECYCLE_SUSPENDED) &&
      (capabilities != 0UL))
  {
    if (g_ps_hw6_rtos_probe.runtime_suspend_saved_execution ==
        (uint32_t)PS_HW6_RUNTIME_EXEC_REALTIME)
    {
      reason = PS_HW6_RTOS_RUNTIME_CLOCK_REASON_REALTIME_DEADLINE;
    }

    request_status = PS_HW6_RTOS_RequestRuntimeClockCapabilities(
      reason,
      capabilities);
    g_ps_hw6_rtos_probe.runtime_resume_clock_request_status =
      (uint32_t)request_status;
  }

  if (request_status == TX_SUCCESS)
  {
    g_ps_hw6_rtos_probe.runtime_lifecycle =
      (uint32_t)PS_HW6_RUNTIME_LIFECYCLE_RUNNING;
  }

  PS_HW6_RTOS_RuntimeRecord(
    (uint32_t)PS_HW6_RUNTIME_EVENT_RESUME,
    (request_status == TX_SUCCESS) ?
    (uint32_t)PS_STATUS_OK : (uint32_t)PS_STATUS_INTERNAL_ERROR);
}

static void PS_HW6_RTOS_HandleRuntimeCommand(ULONG command)
{
  UINT clock_status = TX_SUCCESS;
  uint32_t request_before_command = 1UL;
  uint32_t release_after_command = 1UL;
  uint32_t capabilities = PS_HW6_RTOS_RUNTIME_CLOCK_REACTIVE_CAPABILITIES;
  uint32_t reason = PS_HW6_RTOS_RUNTIME_CLOCK_REASON_REACTIVE_TRANSACTION;

  if (command == PS_HW6_RTOS_COMMAND_RUNTIME_PACKAGE_REALTIME_STUB)
  {
    capabilities = PS_HW6_RTOS_RUNTIME_CLOCK_REALTIME_CAPABILITIES;
    reason = PS_HW6_RTOS_RUNTIME_CLOCK_REASON_REALTIME_DEADLINE;
    release_after_command = 0UL;
  }
  else if ((command == PS_HW6_RTOS_COMMAND_RUNTIME_PACKAGE_RETURN) ||
           (command == PS_HW6_RTOS_COMMAND_RUNTIME_SUSPEND) ||
           (command == PS_HW6_RTOS_COMMAND_RUNTIME_RESUME))
  {
    request_before_command = 0UL;
    release_after_command = 0UL;
  }

  if (request_before_command != 0UL)
  {
    clock_status = PS_HW6_RTOS_RequestRuntimeClockCapabilities(
      reason,
      capabilities);
  }

  if (command == PS_HW6_RTOS_COMMAND_RUNTIME_BOOT_SHELL)
  {
    PS_HW6_RTOS_RuntimeBootShell();
  }
  else if (command == PS_HW6_RTOS_COMMAND_RUNTIME_INSTALLER_ENTER)
  {
    PS_HW6_RTOS_RuntimeEnterInstaller();
  }
  else if (command == PS_HW6_RTOS_COMMAND_RUNTIME_INSTALLER_COMPLETE)
  {
    PS_HW6_RTOS_RuntimeCompleteInstaller();
  }
  else if (command == PS_HW6_RTOS_COMMAND_RUNTIME_INSTALLER_ERROR)
  {
    PS_HW6_RTOS_RuntimeErrorInstaller();
  }
  else if ((command == PS_HW6_RTOS_COMMAND_RUNTIME_PACKAGE_ACTIVATE_STUB) ||
           (command == PS_HW6_RTOS_COMMAND_RUNTIME_PACKAGE_REACTIVE_STUB))
  {
    PS_HW6_RTOS_RuntimePackageActivateStub(
      (uint32_t)PS_HW6_RUNTIME_CLASS_LP_GRAPH,
      (uint32_t)PS_HW6_RUNTIME_EXEC_REACTIVE,
      capabilities,
      clock_status);
  }
  else if (command == PS_HW6_RTOS_COMMAND_RUNTIME_PACKAGE_REALTIME_STUB)
  {
    PS_HW6_RTOS_RuntimePackageActivateStub(
      (uint32_t)PS_HW6_RUNTIME_CLASS_RT_SCENE,
      (uint32_t)PS_HW6_RUNTIME_EXEC_REALTIME,
      capabilities,
      clock_status);
  }
  else if (command == PS_HW6_RTOS_COMMAND_RUNTIME_PACKAGE_RETURN)
  {
    PS_HW6_RTOS_RuntimePackageReturn();
  }
  else if (command == PS_HW6_RTOS_COMMAND_RUNTIME_SUSPEND)
  {
    PS_HW6_RTOS_RuntimeSuspend();
  }
  else if (command == PS_HW6_RTOS_COMMAND_RUNTIME_RESUME)
  {
    PS_HW6_RTOS_RuntimeResume();
  }
  else if (command == PS_HW6_RTOS_COMMAND_RUNTIME_INTERACTION_CUE)
  {
    PS_HW6_RTOS_RuntimeInteractionShowCue((uint32_t)tx_time_get());
  }
  else if (command == PS_HW6_RTOS_COMMAND_RUNTIME_INTERACTION_START)
  {
    PS_HW6_RTOS_RuntimeInteractionActivate((uint32_t)tx_time_get());
  }
  else if (command == PS_HW6_RTOS_COMMAND_RUNTIME_INTERACTION_TIMEOUT)
  {
    ps_runtime_interaction_rtc_command_queued = 0UL;
    PS_HW6_RTOS_RuntimeInteractionService((uint32_t)tx_time_get());
  }
  else if (command == PS_HW6_RTOS_COMMAND_RUNTIME_INTERACTION_INACTIVE)
  {
    PS_HW6_RTOS_RuntimeInteractionEnterInactive((uint32_t)tx_time_get(),
                                                1UL);
  }
  else
  {
    PS_HW6_RTOS_RuntimeRecord(
      (uint32_t)PS_HW6_RUNTIME_EVENT_NONE,
      (uint32_t)PS_STATUS_UNSUPPORTED);
  }

  if (release_after_command != 0UL)
  {
    (void)PS_HW6_RTOS_RequestRuntimeClockCapabilities(
      PS_HW6_RTOS_RUNTIME_CLOCK_REASON_RELEASE,
      0UL);
  }
}

static void PS_HW6_RTOS_RunStorageUsbExportRequest(void)
{
  UINT clock_status;
  HAL_StatusTypeDef export_status = HAL_ERROR;

  PS_HW6_RTOS_SetPowerDebug(GPIO_PIN_SET);
  (void)PS_HW6_RTOS_SendUiLifecycleEvent(
    PS_UI_ROUTER_EVENT_MSC_EXPORT);
  clock_status = PS_HW6_RTOS_RequestStorageClockCapabilities(
    PS_HW6_RTOS_STORAGE_CLOCK_REASON_MSC_EXPORT,
    PS_HW6_RTOS_STORAGE_CLOCK_MSC_CAPABILITIES);
  g_ps_hw6_owner_sm_probe.usb_export_policy_status =
    (uint32_t)clock_status;
  if (clock_status == TX_SUCCESS)
  {
    export_status = PS_HW6_OwnerStateMachines_StartUsbExport();
  }
  if (export_status != HAL_OK)
  {
    (void)PS_HW6_RTOS_RequestRuntimeCommand(
      PS_HW6_RTOS_COMMAND_RUNTIME_INSTALLER_ERROR);
    if (g_ps_hw6_owner_sm_probe.usb_export_fxlx_open_status ==
        (uint32_t)PS_STATUS_RECOVERY_REQUIRED)
    {
      (void)PS_HW6_RTOS_SendUiLifecycleEvent(
        PS_UI_ROUTER_EVENT_MSC_RECOVERY);
    }
    else
    {
      (void)PS_HW6_RTOS_SendUiLifecycleEvent(
        PS_UI_ROUTER_EVENT_MSC_ERROR);
    }
    (void)PS_HW6_RTOS_RequestStorageClockCapabilities(
      PS_HW6_RTOS_STORAGE_CLOCK_REASON_RELEASE,
      0UL);
  }
  else
  {
    (void)PS_HW6_RTOS_SendUiLifecycleEvent(
      PS_UI_ROUTER_EVENT_MSC_ACTIVE);
  }
  PS_HW6_RTOS_SetPowerDebug(GPIO_PIN_RESET);
}

static uint32_t PS_HW6_RTOS_ShouldForceUsbStageRescan(void)
{
  return 0UL;
}

static void PS_HW6_RTOS_RunStorageUsbReclaimRequest(void)
{
  UINT clock_status;
  UINT clock_release_status = TX_SUCCESS;
  HAL_StatusTypeDef reclaim_status = HAL_ERROR;
  uint32_t force_stage_rescan;

  force_stage_rescan = PS_HW6_RTOS_ShouldForceUsbStageRescan();

  PS_HW6_RTOS_SetPowerDebug(GPIO_PIN_SET);
  (void)PS_HW6_RTOS_SendUiLifecycleEvent(
    PS_UI_ROUTER_EVENT_MSC_RECLAIM);
  clock_status = PS_HW6_RTOS_RequestStorageClockCapabilities(
    PS_HW6_RTOS_STORAGE_CLOCK_REASON_MSC_RECLAIM,
    PS_HW6_RTOS_STORAGE_CLOCK_MSC_CAPABILITIES);
  if ((clock_status == TX_SUCCESS) ||
      (PS_HW6_RTOS_StorageClockCapabilitiesActive(
         PS_HW6_RTOS_STORAGE_CLOCK_MSC_CAPABILITIES) != 0UL))
  {
    reclaim_status = PS_HW6_OwnerStateMachines_ReclaimUsbExport(
      force_stage_rescan);
  }
  clock_release_status = PS_HW6_RTOS_RequestStorageClockCapabilities(
    PS_HW6_RTOS_STORAGE_CLOCK_REASON_RELEASE,
    0UL);
  if ((reclaim_status == HAL_OK) && (clock_release_status == TX_SUCCESS))
  {
    reclaim_status =
      PS_HW6_OwnerStateMachines_FinalizeUsbReclaimAfterClockRelease(
        force_stage_rescan);
  }
  else if (reclaim_status == HAL_OK)
  {
    reclaim_status = HAL_ERROR;
  }
  if (reclaim_status == HAL_OK)
  {
    (void)PS_HW6_RTOS_RequestRuntimeCommand(
      PS_HW6_RTOS_COMMAND_RUNTIME_INSTALLER_COMPLETE);
    (void)PS_HW6_RTOS_SendUiLifecycleEvent(
      PS_UI_ROUTER_EVENT_MSC_DONE);
    PS_HW6_RTOS_SendCurrentUiRenderCommand();
  }
  else
  {
    (void)PS_HW6_RTOS_RequestRuntimeCommand(
      PS_HW6_RTOS_COMMAND_RUNTIME_INSTALLER_ERROR);
    (void)PS_HW6_RTOS_SendUiLifecycleEvent(
      PS_UI_ROUTER_EVENT_MSC_ERROR);
  }
  PS_HW6_RTOS_SetPowerDebug(GPIO_PIN_RESET);
}

static void PS_HW6_RTOS_RunStoragePackageScanRequest(void)
{
  UINT clock_status;
  HAL_StatusTypeDef scan_status = HAL_ERROR;

  if (PS_HW6_RTOS_UiMscExportActive() != 0UL)
  {
    if (g_ps_ui_router_request == 0UL)
    {
      g_ps_ui_router_request_event =
        (uint32_t)PS_UI_ROUTER_EVENT_PACKAGE_VALIDATE_ERROR;
      g_ps_ui_router_request = 1UL;
    }
    PS_HW6_RTOS_SendCurrentUiRenderCommand();
    return;
  }

  clock_status = PS_HW6_RTOS_RequestStorageClockCapabilities(
    PS_HW6_RTOS_STORAGE_CLOCK_REASON_PACKAGE_LOAD,
    PS_HW6_RTOS_STORAGE_CLOCK_FLASH_CAPABILITIES);
  if ((clock_status == TX_SUCCESS) ||
      (PS_HW6_RTOS_StorageClockCapabilitiesActive(
         PS_HW6_RTOS_STORAGE_CLOCK_FLASH_CAPABILITIES) != 0UL))
  {
    scan_status = PS_HW6_OwnerStateMachines_ScanUsbStagingPackage();
  }
  (void)PS_HW6_RTOS_RequestStorageClockCapabilities(
    PS_HW6_RTOS_STORAGE_CLOCK_REASON_RELEASE,
    0UL);

  if ((scan_status == HAL_OK) &&
      (g_ps_hw6_owner_sm_probe.package_candidate_pending != 0UL) &&
      (g_ps_ui_router_request == 0UL))
  {
    g_ps_ui_router_request_event =
      (uint32_t)PS_UI_ROUTER_EVENT_PACKAGE_VALID_FOUND;
    g_ps_ui_router_request = 1UL;
  }
  else if ((scan_status == HAL_OK) && (g_ps_ui_router_request == 0UL))
  {
    g_ps_ui_router_request_event =
      (uint32_t)PS_UI_ROUTER_EVENT_PACKAGE_CLEAR;
    g_ps_ui_router_request = 1UL;
  }
  else if (g_ps_ui_router_request == 0UL)
  {
    g_ps_ui_router_request_event =
      (uint32_t)PS_UI_ROUTER_EVENT_PACKAGE_VALIDATE_ERROR;
    g_ps_ui_router_request = 1UL;
  }

  PS_HW6_RTOS_SendCurrentUiRenderCommand();
}

static void PS_HW6_RTOS_RunStoragePackageInstallStubRequest(void)
{
  UINT clock_status;
  UINT complete_send_status = TX_NOT_AVAILABLE;
  UINT launch_send_status = TX_NOT_AVAILABLE;
  HAL_StatusTypeDef install_status = HAL_ERROR;

  g_ps_hw6_rtos_probe.package_install_runtime_complete_send_status =
    PS_HW6_RTOS_STATUS_NOT_RUN;
  g_ps_hw6_rtos_probe.package_install_runtime_launch_send_status =
    PS_HW6_RTOS_STATUS_NOT_RUN;

  clock_status = PS_HW6_RTOS_RequestStorageClockCapabilities(
    PS_HW6_RTOS_STORAGE_CLOCK_REASON_PACKAGE_LOAD,
    PS_HW6_RTOS_STORAGE_CLOCK_FLASH_CAPABILITIES);
  if ((clock_status == TX_SUCCESS) ||
      (PS_HW6_RTOS_StorageClockCapabilitiesActive(
         PS_HW6_RTOS_STORAGE_CLOCK_FLASH_CAPABILITIES) != 0UL))
  {
    install_status = PS_HW6_OwnerStateMachines_RunPackageInstallStub();
  }
  (void)PS_HW6_RTOS_RequestStorageClockCapabilities(
    PS_HW6_RTOS_STORAGE_CLOCK_REASON_RELEASE,
    0UL);

  if (install_status == HAL_OK)
  {
    complete_send_status = PS_HW6_RTOS_RequestRuntimeCommand(
      PS_HW6_RTOS_COMMAND_RUNTIME_INSTALLER_COMPLETE);
    launch_send_status = TX_SUCCESS;
    g_ps_hw6_rtos_probe.package_install_runtime_complete_send_status =
      (uint32_t)complete_send_status;
    g_ps_hw6_rtos_probe.package_install_runtime_launch_send_status =
      (uint32_t)launch_send_status;
    if ((complete_send_status == TX_SUCCESS) &&
        (g_ps_ui_router_request == 0UL))
    {
      g_ps_ui_router_request_event =
        (uint32_t)PS_UI_ROUTER_EVENT_PACKAGE_INSTALL_STUB_DONE;
      g_ps_ui_router_request = 1UL;
      PS_HW6_RTOS_SendCurrentUiRenderCommand();
    }
  }

  if ((install_status != HAL_OK) ||
      (complete_send_status != TX_SUCCESS) ||
      (launch_send_status != TX_SUCCESS))
  {
    (void)PS_HW6_RTOS_RequestRuntimeCommand(
      PS_HW6_RTOS_COMMAND_RUNTIME_INSTALLER_ERROR);
    PS_HW6_RTOS_RequestPackageInstallErrorUi();
  }
}

static void PS_HW6_RTOS_RunStorageFlashInitRequest(void)
{
  UINT clock_status;
  HAL_StatusTypeDef flash_init_status = HAL_ERROR;
  uint32_t display_cue;

  PS_HW6_RTOS_SetPowerDebug(GPIO_PIN_SET);
  PS_HW6_RTOS_SendStorageLifecycleDisplayCue(
    PS_UI_ROUTER_SHUTDOWN_FLASH_INIT);
  clock_status = PS_HW6_RTOS_RequestStorageClockCapabilities(
    PS_HW6_RTOS_STORAGE_CLOCK_REASON_FLASH_INIT,
    PS_HW6_RTOS_STORAGE_CLOCK_FLASH_CAPABILITIES);
  if (clock_status == TX_SUCCESS)
  {
    flash_init_status = PS_HW6_OwnerStateMachines_InitializeFlash();
  }
  (void)PS_HW6_RTOS_RequestStorageClockCapabilities(
    PS_HW6_RTOS_STORAGE_CLOCK_REASON_RELEASE,
    0UL);
  display_cue = (flash_init_status == HAL_OK) ?
    (uint32_t)PS_UI_ROUTER_SHUTDOWN_FLASH_DONE :
    (uint32_t)PS_UI_ROUTER_SHUTDOWN_FLASH_ERROR;
  PS_HW6_RTOS_SendStorageLifecycleDisplayCue(display_cue);
  PS_HW6_RTOS_SetPowerDebug(GPIO_PIN_RESET);
}

static void PS_HW6_RTOS_RunStorageEmbeddedPackageInstallRequest(void)
{
  UINT clock_status;

  clock_status = PS_HW6_RTOS_RequestStorageClockCapabilities(
    PS_HW6_RTOS_STORAGE_CLOCK_REASON_PACKAGE_LOAD,
    PS_HW6_RTOS_STORAGE_CLOCK_FLASH_CAPABILITIES);
  if ((clock_status == TX_SUCCESS) ||
      (PS_HW6_RTOS_StorageClockCapabilitiesActive(
         PS_HW6_RTOS_STORAGE_CLOCK_FLASH_CAPABILITIES) != 0UL))
  {
    (void)PS_HW6_OwnerStateMachines_RunEmbeddedPersistentPackageInstall();
  }
  (void)PS_HW6_RTOS_RequestStorageClockCapabilities(
    PS_HW6_RTOS_STORAGE_CLOCK_REASON_RELEASE,
    0UL);
}

static void PS_HW6_RTOS_RunStorageInstalledPackageLoadRequest(void)
{
  UINT clock_status;

  clock_status = PS_HW6_RTOS_RequestStorageClockCapabilities(
    PS_HW6_RTOS_STORAGE_CLOCK_REASON_PACKAGE_LOAD,
    PS_HW6_RTOS_STORAGE_CLOCK_FLASH_CAPABILITIES);
  if ((clock_status == TX_SUCCESS) ||
      (PS_HW6_RTOS_StorageClockCapabilitiesActive(
         PS_HW6_RTOS_STORAGE_CLOCK_FLASH_CAPABILITIES) != 0UL))
  {
    (void)PS_HW6_OwnerStateMachines_LoadPersistentPackage();
  }
  else
  {
    g_ps_hw6_owner_sm_probe.persistent_load_last_status =
      (uint32_t)HAL_ERROR;
  }
  (void)PS_HW6_RTOS_RequestStorageClockCapabilities(
    PS_HW6_RTOS_STORAGE_CLOCK_REASON_RELEASE,
    0UL);
  (void)tx_event_flags_set(
    &ps_event_groups[PS_HW6_RTOS_EVENT_DEBUG_INDEX],
    PS_HW6_RTOS_ACK_OWNER(PS_HW6_RTOS_OWNER_STORAGE),
    TX_OR);
}

static void PS_HW6_RTOS_RunStorageAttachRequest(void)
{
  UINT clock_status;

  PS_HW6_RTOS_SetPowerDebug(GPIO_PIN_SET);
  clock_status = PS_HW6_RTOS_RequestStorageClockCapabilities(
    PS_HW6_RTOS_STORAGE_CLOCK_REASON_ATTACH,
    PS_HW6_RTOS_STORAGE_CLOCK_FLASH_CAPABILITIES);
  if (clock_status == TX_SUCCESS)
  {
    (void)PS_HW6_OwnerStateMachines_AttachStorage();
  }
  (void)PS_HW6_RTOS_RequestStorageClockCapabilities(
    PS_HW6_RTOS_STORAGE_CLOCK_REASON_RELEASE,
    0UL);
  PS_HW6_RTOS_SetPowerDebug(GPIO_PIN_RESET);
}

static void PS_HW6_RTOS_RunStorageJoystickCalibrationLoadRequest(void)
{
  UINT clock_status;

  clock_status = PS_HW6_RTOS_RequestStorageClockCapabilities(
    PS_HW6_RTOS_STORAGE_CLOCK_REASON_JOYSTICK_CALIBRATION,
    PS_HW6_RTOS_STORAGE_CLOCK_FLASH_CAPABILITIES);
  if ((clock_status == TX_SUCCESS) ||
      (PS_HW6_RTOS_StorageClockCapabilitiesActive(
         PS_HW6_RTOS_STORAGE_CLOCK_FLASH_CAPABILITIES) != 0UL))
  {
    (void)
      PS_HW6_OwnerStateMachines_LoadPersistentJoystickCalibration();
  }
  (void)PS_HW6_RTOS_RequestStorageClockCapabilities(
    PS_HW6_RTOS_STORAGE_CLOCK_REASON_RELEASE,
    0UL);
  ps_joystick_calibration_apply_pending = 1UL;
}

static void PS_HW6_RTOS_RunStorageJoystickCalibrationSaveRequest(void)
{
  UINT clock_status;

  clock_status = PS_HW6_RTOS_RequestStorageClockCapabilities(
    PS_HW6_RTOS_STORAGE_CLOCK_REASON_JOYSTICK_CALIBRATION,
    PS_HW6_RTOS_STORAGE_CLOCK_FLASH_CAPABILITIES);
  if ((clock_status == TX_SUCCESS) ||
      (PS_HW6_RTOS_StorageClockCapabilitiesActive(
         PS_HW6_RTOS_STORAGE_CLOCK_FLASH_CAPABILITIES) != 0UL))
  {
    (void)
      PS_HW6_OwnerStateMachines_SavePersistentJoystickCalibration();
  }
  (void)PS_HW6_RTOS_RequestStorageClockCapabilities(
    PS_HW6_RTOS_STORAGE_CLOCK_REASON_RELEASE,
    0UL);
  ps_joystick_calibration_save_completion_pending = 1UL;
}

static void PS_HW6_RTOS_CaptureAudioSfx(
  volatile PS_HW6_RTOS_AudioSfxCapture *capture,
  HAL_StatusTypeDef owner_status,
  uint32_t clock_apply_count_before,
  uint32_t clock_pll2_on_count_before,
  uint32_t clock_pll2_off_count_before)
{
  capture->valid = 0UL;
  capture->capture_tick = (uint32_t)tx_time_get();
  capture->stop2_entry_count =
    g_ps_hw6_rtos_probe.stop2_auto_entry_count;
  capture->owner_status = (uint32_t)owner_status;
  capture->clock_apply_count_before = clock_apply_count_before;
  capture->clock_apply_count_after =
    g_ps_hw6_clock_policy_probe.apply_count;
  capture->clock_pll2_on_count_before = clock_pll2_on_count_before;
  capture->clock_pll2_on_count_after =
    g_ps_hw6_clock_policy_probe.pll2_domain_on_count;
  capture->clock_pll2_off_count_before = clock_pll2_off_count_before;
  capture->clock_pll2_off_count_after =
    g_ps_hw6_clock_policy_probe.pll2_domain_off_count;
  capture->grant_policy_status =
    g_ps_hw6_rtos_probe.audio_sfx_grant_policy_status;
  capture->grant_policy_stage =
    g_ps_hw6_rtos_probe.audio_sfx_grant_policy_stage;
  capture->grant_required_domains =
    g_ps_hw6_rtos_probe.audio_sfx_grant_required_domains;
  capture->grant_managed_domains =
    g_ps_hw6_rtos_probe.audio_sfx_grant_managed_domains;
  capture->grant_readback_domains =
    g_ps_hw6_rtos_probe.audio_sfx_grant_readback_domains;
  capture->grant_pll2_ready =
    g_ps_hw6_rtos_probe.audio_sfx_grant_pll2_ready;
  capture->grant_pll2_outputs =
    g_ps_hw6_rtos_probe.audio_sfx_grant_pll2_outputs;
  capture->grant_sai_kernel_hz =
    g_ps_hw6_rtos_probe.audio_sfx_grant_sai_kernel_hz;
  capture->final_requester_active_mask =
    g_ps_hw6_clock_policy_probe.requester_active_mask;
  capture->final_aggregated_capabilities =
    g_ps_hw6_clock_policy_probe.aggregated_capabilities;
  capture->final_required_domains =
    g_ps_hw6_clock_policy_probe.required_domain_mask;
  capture->final_managed_domains =
    g_ps_hw6_clock_policy_probe.managed_domain_mask;
  capture->final_readback_domains =
    g_ps_hw6_clock_policy_probe.readback_domain_mask;
  capture->final_pll2_ready = g_ps_hw6_clock_policy_probe.pll2_ready;
  capture->final_pll2_outputs =
    g_ps_hw6_clock_policy_probe.pll2_output_enabled_mask;
  capture->final_sai_kernel_hz =
    g_ps_hw6_clock_policy_probe.sai1_kernel_hz;
  capture->audio_sai_kernel_hz_start =
    g_ps_hw6_owner_probe.audio_sai_kernel_hz;
  capture->audio_sai_kernel_hz_end =
    g_ps_hw6_owner_probe.audio_pre_cleanup_sai_kernel_hz;
  capture->audio_rearm_status =
    g_ps_hw6_owner_probe.audio_rearm_status;
  capture->audio_start_status =
    g_ps_hw6_owner_probe.audio_start_status;
  capture->audio_wait_status =
    g_ps_hw6_owner_probe.audio_completion_wait_status;
  capture->audio_callback_status =
    g_ps_hw6_owner_probe.audio_completion_callback_status;
  capture->audio_stop_status = g_ps_hw6_owner_probe.audio_stop_status;
  capture->dma_irq_delta =
    g_ps_hw6_owner_probe.audio_pre_cleanup_dma_irq_delta;
  capture->tx_callback_delta =
    g_ps_hw6_owner_probe.audio_pre_cleanup_tx_callback_delta;
  capture->error_callback_delta =
    g_ps_hw6_owner_probe.audio_pre_cleanup_error_callback_delta;
  capture->sai_state =
    g_ps_hw6_owner_probe.audio_pre_cleanup_sai_state;
  capture->sai_error =
    g_ps_hw6_owner_probe.audio_pre_cleanup_sai_error;
  capture->sai_cr1 = g_ps_hw6_owner_probe.audio_pre_cleanup_sai_cr1;
  capture->sai_cr2 = g_ps_hw6_owner_probe.audio_pre_cleanup_sai_cr2;
  capture->sai_frcr = g_ps_hw6_owner_probe.audio_pre_cleanup_sai_frcr;
  capture->sai_slotr = g_ps_hw6_owner_probe.audio_pre_cleanup_sai_slotr;
  capture->sai_imr = g_ps_hw6_owner_probe.audio_pre_cleanup_sai_imr;
  capture->sai_sr = g_ps_hw6_owner_probe.audio_pre_cleanup_sai_sr;
  capture->sai_gcr = g_ps_hw6_owner_probe.audio_pre_cleanup_sai_gcr;
  capture->dma_state =
    g_ps_hw6_owner_probe.audio_pre_cleanup_dma_state;
  capture->dma_error =
    g_ps_hw6_owner_probe.audio_pre_cleanup_dma_error;
  capture->dma_ccr = g_ps_hw6_owner_probe.audio_pre_cleanup_dma_ccr;
  capture->dma_csr = g_ps_hw6_owner_probe.audio_pre_cleanup_dma_csr;
  capture->dma_cbr1 = g_ps_hw6_owner_probe.audio_pre_cleanup_dma_cbr1;
  capture->dma_ctr1 = g_ps_hw6_owner_probe.audio_pre_cleanup_dma_ctr1;
  capture->dma_ctr2 = g_ps_hw6_owner_probe.audio_pre_cleanup_dma_ctr2;
  capture->dma_csar = g_ps_hw6_owner_probe.audio_pre_cleanup_dma_csar;
  capture->dma_cdar = g_ps_hw6_owner_probe.audio_pre_cleanup_dma_cdar;
  capture->dma_cllr = g_ps_hw6_owner_probe.audio_pre_cleanup_dma_cllr;
  capture->rcc_ahb1enr = RCC->AHB1ENR;
  capture->rcc_ahb2enr1 = RCC->AHB2ENR1;
  capture->rcc_apb2enr = RCC->APB2ENR;
  capture->rcc_apb2rstr = RCC->APB2RSTR;
  capture->rcc_apb2smenr = RCC->APB2SMENR;
  capture->rcc_ccipr2 = RCC->CCIPR2;
  capture->gpioa_moder = GPIOA->MODER;
  capture->gpioa_afrh = GPIOA->AFR[1];
  capture->gpiob_moder = GPIOB->MODER;
  capture->gpiob_afrh = GPIOB->AFR[1];
  capture->stop2_gpioa_moder_before =
    g_ps_hw6_owner_sm_probe.stop2_gpio_moder_before[0];
  capture->stop2_gpioa_moder_after =
    g_ps_hw6_owner_sm_probe.stop2_gpio_moder_after[0];
  capture->stop2_gpiob_moder_before =
    g_ps_hw6_owner_sm_probe.stop2_gpio_moder_before[1];
  capture->stop2_gpiob_moder_after =
    g_ps_hw6_owner_sm_probe.stop2_gpio_moder_after[1];
  capture->valid = 1UL;
}

static void PS_HW6_RTOS_HandleOwnerCommand(uint32_t owner_id,
                                           ULONG command,
                                           uint32_t cycle_index)
{
  UINT status;

  if ((owner_id == PS_HW6_RTOS_OWNER_POWER) &&
      (command == PS_HW6_RTOS_COMMAND_POWER_WORKFLOW))
  {
    PS_HW6_RTOS_RunPowerWorkflow();
  }
  else if ((owner_id == PS_HW6_RTOS_OWNER_POWER) &&
           (command == PS_HW6_RTOS_COMMAND_POWER_STOP2_RECHECK))
  {
    g_ps_hw6_rtos_probe.stop2_lpbam_power_recheck_consume_count++;
    g_ps_hw6_rtos_probe.stop2_lpbam_power_recheck_consume_tick =
      (uint32_t)tx_time_get();
  }
  else if ((owner_id == PS_HW6_RTOS_OWNER_POWER) &&
           (command == PS_HW6_RTOS_COMMAND_CLOCK_PROFILE))
  {
    uint32_t requester_id = PS_HW6_RTOS_ClockPayloadRequester(cycle_index);
    uint32_t capabilities =
      PS_HW6_RTOS_ClockPayloadCapabilities(cycle_index);
    ULONG ack_flag = PS_HW6_RTOS_ClockAckFlag(requester_id);

    status = PS_HW6_ClockPolicy_ApplyRequesterProfile(
      requester_id,
      PS_HW6_RTOS_ClockPayloadProfile(cycle_index),
      capabilities);
    if ((requester_id == PS_HW6_RTOS_OWNER_AUDIO) &&
        ((capabilities & PS_HW6_RTOS_AUDIO_CLOCK_SAI_CAPABILITIES) != 0UL))
    {
      g_ps_hw6_rtos_probe.audio_sfx_grant_policy_status =
        g_ps_hw6_clock_policy_probe.last_status;
      g_ps_hw6_rtos_probe.audio_sfx_grant_policy_stage =
        g_ps_hw6_clock_policy_probe.last_stage;
      g_ps_hw6_rtos_probe.audio_sfx_grant_required_domains =
        g_ps_hw6_clock_policy_probe.required_domain_mask;
      g_ps_hw6_rtos_probe.audio_sfx_grant_managed_domains =
        g_ps_hw6_clock_policy_probe.managed_domain_mask;
      g_ps_hw6_rtos_probe.audio_sfx_grant_readback_domains =
        g_ps_hw6_clock_policy_probe.readback_domain_mask;
      g_ps_hw6_rtos_probe.audio_sfx_grant_pll2_ready =
        g_ps_hw6_clock_policy_probe.pll2_ready;
      g_ps_hw6_rtos_probe.audio_sfx_grant_pll2_outputs =
        g_ps_hw6_clock_policy_probe.pll2_output_enabled_mask;
      g_ps_hw6_rtos_probe.audio_sfx_grant_sai_kernel_hz =
        g_ps_hw6_clock_policy_probe.sai1_kernel_hz;
    }
    PS_HW6_RTOS_ScheduleClockReleaseStop2Recheck(requester_id,
                                                 capabilities,
                                                 status);
    if (ack_flag != 0UL)
    {
      (void)tx_event_flags_set(
        &ps_event_groups[PS_HW6_RTOS_EVENT_DEBUG_INDEX],
        ack_flag,
        TX_OR);
    }
  }
  else if ((owner_id == PS_HW6_RTOS_OWNER_STORAGE) &&
           (command == PS_HW6_RTOS_COMMAND_USB_EXPORT))
  {
    PS_HW6_RTOS_RunStorageUsbExportRequest();
  }
  else if ((owner_id == PS_HW6_RTOS_OWNER_STORAGE) &&
           (command == PS_HW6_RTOS_COMMAND_USB_RECLAIM))
  {
    PS_HW6_RTOS_RunStorageUsbReclaimRequest();
  }
  else if ((owner_id == PS_HW6_RTOS_OWNER_STORAGE) &&
           (command == PS_HW6_RTOS_COMMAND_STORAGE_FLASH_INIT))
  {
    PS_HW6_RTOS_RunStorageFlashInitRequest();
  }
  else if ((owner_id == PS_HW6_RTOS_OWNER_STORAGE) &&
           (command == PS_HW6_RTOS_COMMAND_STORAGE_ATTACH))
  {
    PS_HW6_RTOS_RunStorageAttachRequest();
  }
  else if ((owner_id == PS_HW6_RTOS_OWNER_STORAGE) &&
           (command ==
            PS_HW6_RTOS_COMMAND_STORAGE_PACKAGE_INSTALL_EMBEDDED))
  {
    PS_HW6_RTOS_RunStorageEmbeddedPackageInstallRequest();
  }
  else if ((owner_id == PS_HW6_RTOS_OWNER_STORAGE) &&
           (command ==
            PS_HW6_RTOS_COMMAND_STORAGE_PACKAGE_LOAD_INSTALLED))
  {
    PS_HW6_RTOS_RunStorageInstalledPackageLoadRequest();
  }
  else if ((owner_id == PS_HW6_RTOS_OWNER_STORAGE) &&
           (command ==
            PS_HW6_RTOS_COMMAND_STORAGE_JOYSTICK_CALIBRATION_SAVE))
  {
    PS_HW6_RTOS_RunStorageJoystickCalibrationSaveRequest();
  }
  else if ((owner_id == PS_HW6_RTOS_OWNER_STORAGE) &&
           (command == PS_HW6_RTOS_COMMAND_PACKAGE_SCAN))
  {
    PS_HW6_RTOS_RunStoragePackageScanRequest();
  }
  else if ((owner_id == PS_HW6_RTOS_OWNER_INPUT) &&
           (command == PS_HW6_RTOS_COMMAND_INPUT_JOYSTICK_WAKE_PUBLISH))
  {
    /* Queue arrival wakes thInput; publication runs after command handling. */
  }
  else if ((owner_id == PS_HW6_RTOS_OWNER_STORAGE) &&
           (command == PS_HW6_RTOS_COMMAND_PACKAGE_INSTALL_STUB))
  {
    PS_HW6_RTOS_RunStoragePackageInstallStubRequest();
  }
  else if ((owner_id == PS_HW6_RTOS_OWNER_STORAGE) &&
           (command == PS_HW6_RTOS_COMMAND_USB_BOOT_PARK))
  {
    (void)PS_HW6_OwnerStateMachines_ParkUsbForBoot();
    (void)tx_event_flags_set(
      &ps_event_groups[PS_HW6_RTOS_EVENT_DEBUG_INDEX],
      PS_HW6_RTOS_ACK_OWNER(owner_id),
      TX_OR);
  }
  else if ((owner_id == PS_HW6_RTOS_OWNER_DISPLAY) &&
           (command == PS_HW6_RTOS_COMMAND_DISPLAY_LPBAM_PREPARE))
  {
    (void)PS_HW6_DisplayOwner_PrepareLpbamStop2();
    (void)tx_event_flags_set(
      &ps_event_groups[PS_HW6_RTOS_EVENT_DEBUG_INDEX],
      PS_HW6_RTOS_ACK_OWNER(owner_id),
      TX_OR);
  }
  else if ((owner_id == PS_HW6_RTOS_OWNER_DISPLAY) &&
           (command == PS_HW6_RTOS_COMMAND_DISPLAY_LPBAM_ABORT))
  {
    HAL_StatusTypeDef abort_status;
    HAL_StatusTypeDef resume_status = HAL_ERROR;

    abort_status = PS_HW6_DisplayOwner_AbortLpbamStop2();
    if (abort_status == HAL_OK)
    {
      resume_status = PS_HW6_DisplayOwner_RenderCursorBlink(1UL);
    }
    g_ps_hw6_rtos_probe.stop2_lpbam_abort_resume_phase = 1UL;
    g_ps_hw6_rtos_probe.stop2_lpbam_abort_resume_status =
      (uint32_t)resume_status;
    g_ps_hw6_rtos_probe.stop2_lpbam_abort_resume_tick =
      (uint32_t)tx_time_get();
    (void)tx_event_flags_set(
      &ps_event_groups[PS_HW6_RTOS_EVENT_DEBUG_INDEX],
      PS_HW6_RTOS_ACK_OWNER(owner_id),
      TX_OR);
  }
  else if ((owner_id == PS_HW6_RTOS_OWNER_DISPLAY) &&
           (command == PS_HW6_RTOS_COMMAND_DISPLAY_LPBAM_WAKE_ABORT))
  {
    HAL_StatusTypeDef resume_status =
      PS_HW6_DisplayOwner_AbortLpbamStop2AndResume();

    g_ps_hw6_rtos_probe.stop2_lpbam_abort_resume_phase =
      g_ps_hw6_owner_probe.display_lpbam_wake_phase;
    g_ps_hw6_rtos_probe.stop2_lpbam_abort_resume_status =
      (uint32_t)resume_status;
    g_ps_hw6_rtos_probe.stop2_lpbam_abort_resume_tick =
      (uint32_t)tx_time_get();
    (void)tx_event_flags_set(
      &ps_event_groups[PS_HW6_RTOS_EVENT_DEBUG_INDEX],
      PS_HW6_RTOS_ACK_OWNER(owner_id),
      TX_OR);
  }
  else if ((owner_id == PS_HW6_RTOS_OWNER_DISPLAY) &&
           (command == PS_HW6_RTOS_COMMAND_DISPLAY_LPBAM_EDGE_WAKE))
  {
    (void)PS_HW6_RTOS_CompileDisplayLpbamAheadOfEdge(
      (uint32_t)tx_time_get());
  }
  else if ((owner_id == PS_HW6_RTOS_OWNER_DISPLAY) &&
           (command == PS_HW6_RTOS_COMMAND_DISPLAY_CURSOR_VISIBLE))
  {
    (void)PS_HW6_DisplayOwner_RenderCursorBlink(1UL);
    (void)tx_event_flags_set(
      &ps_event_groups[PS_HW6_RTOS_EVENT_DEBUG_INDEX],
      PS_HW6_RTOS_ACK_OWNER(owner_id),
      TX_OR);
  }
  else if ((owner_id == PS_HW6_RTOS_OWNER_COMM) &&
           (command == PS_HW6_RTOS_COMMAND_COMM_BLE_MODE))
  {
    (void)PS_HW6_OwnerStateMachines_SetBleMode(cycle_index);
    (void)tx_event_flags_set(
      &ps_event_groups[PS_HW6_RTOS_EVENT_DEBUG_INDEX],
      PS_HW6_RTOS_ACK_OWNER(PS_HW6_RTOS_OWNER_COMM),
      TX_OR);
  }
  else if ((owner_id == PS_HW6_RTOS_OWNER_SENSOR) &&
           (command == PS_HW6_RTOS_COMMAND_SENSOR_IMU_MODE))
  {
    (void)PS_HW6_OwnerStateMachines_SetImuMode(cycle_index);
    (void)tx_event_flags_set(
      &ps_event_groups[PS_HW6_RTOS_EVENT_DEBUG_INDEX],
      PS_HW6_RTOS_ACK_OWNER(PS_HW6_RTOS_OWNER_SENSOR),
      TX_OR);
  }
  else if ((owner_id == PS_HW6_RTOS_OWNER_AUDIO) &&
           (command == PS_HW6_RTOS_COMMAND_AUDIO_PLAY_SFX))
  {
    HAL_StatusTypeDef sfx_status = HAL_ERROR;
    UINT clock_status;
    UINT release_status;
    uint32_t clock_apply_count_before = 0UL;
    uint32_t clock_pll2_on_count_before = 0UL;
    uint32_t clock_pll2_off_count_before = 0UL;

    g_ps_hw6_rtos_probe.audio_sfx_owner_count++;
    g_ps_hw6_rtos_probe.audio_sfx_last_cue_index = cycle_index;
    clock_status = PS_HW6_RTOS_RequestAudioClockCapabilities(
      PS_HW6_RTOS_AUDIO_CLOCK_REASON_REACTIVE_SFX,
      PS_HW6_RTOS_AUDIO_CLOCK_SAI_CAPABILITIES);
    g_ps_hw6_rtos_probe.audio_sfx_clock_request_status =
      (uint32_t)clock_status;
    if (clock_status == TX_SUCCESS)
    {
      sfx_status = PS_HW6_OwnerStateMachines_Stabilize(
        PS_HW6_RTOS_OWNER_AUDIO);
      if (sfx_status == HAL_OK)
      {
        clock_apply_count_before = g_ps_hw6_clock_policy_probe.apply_count;
        clock_pll2_on_count_before =
          g_ps_hw6_clock_policy_probe.pll2_domain_on_count;
        clock_pll2_off_count_before =
          g_ps_hw6_clock_policy_probe.pll2_domain_off_count;
        sfx_status = PS_HW6_OwnerStateMachines_RunAudioSfx(cycle_index);
        if ((sfx_status == HAL_OK) &&
            (g_ps_hw6_rtos_probe.audio_sfx_first_success_capture.valid ==
             0UL))
        {
          PS_HW6_RTOS_CaptureAudioSfx(
            &g_ps_hw6_rtos_probe.audio_sfx_first_success_capture,
            sfx_status,
            clock_apply_count_before,
            clock_pll2_on_count_before,
            clock_pll2_off_count_before);
        }
        else if ((sfx_status != HAL_OK) &&
                 (g_ps_hw6_rtos_probe
                    .audio_sfx_first_post_stop_failure_capture.valid == 0UL) &&
                 (((g_ps_hw6_rtos_probe
                      .audio_sfx_first_success_capture.valid != 0UL) &&
                   (g_ps_hw6_rtos_probe.stop2_auto_entry_count >
                    g_ps_hw6_rtos_probe
                      .audio_sfx_first_success_capture.stop2_entry_count)) ||
                  ((g_ps_hw6_rtos_probe
                      .audio_sfx_first_success_capture.valid == 0UL) &&
                   (g_ps_hw6_rtos_probe.stop2_auto_entry_count != 0UL))))
        {
          PS_HW6_RTOS_CaptureAudioSfx(
            &g_ps_hw6_rtos_probe
               .audio_sfx_first_post_stop_failure_capture,
            sfx_status,
            clock_apply_count_before,
            clock_pll2_on_count_before,
            clock_pll2_off_count_before);
        }
      }
    }
    release_status = PS_HW6_RTOS_RequestAudioClockCapabilities(
      PS_HW6_RTOS_AUDIO_CLOCK_REASON_RELEASE,
      0UL);
    g_ps_hw6_rtos_probe.audio_sfx_clock_release_status =
      (uint32_t)release_status;
    g_ps_hw6_rtos_probe.audio_sfx_owner_status =
      (uint32_t)sfx_status;
    ps_audio_sfx_pending = 0UL;
  }
  else if (owner_id == PS_HW6_RTOS_OWNER_RUNTIME)
  {
    PS_HW6_RTOS_HandleRuntimeCommand(command);
    (void)tx_event_flags_set(
      &ps_event_groups[PS_HW6_RTOS_EVENT_DEBUG_INDEX],
      PS_HW6_RTOS_ACK_OWNER(PS_HW6_RTOS_OWNER_RUNTIME),
      TX_OR);
  }
  else if ((owner_id > PS_HW6_RTOS_OWNER_POWER) &&
           (owner_id <= PS_HW6_RTOS_OWNER_COMM) &&
           (command == PS_HW6_RTOS_COMMAND_STABILIZE))
  {
    if (owner_id == PS_HW6_RTOS_OWNER_DISPLAY)
    {
      g_ps_hw6_owner_probe.display_command_tick = (uint32_t)tx_time_get();
    }
    else if (owner_id == PS_HW6_RTOS_OWNER_AUDIO)
    {
      g_ps_hw6_owner_probe.audio_command_tick = (uint32_t)tx_time_get();
    }

    (void)PS_HW6_OwnerStateMachines_Stabilize(owner_id);
    status = tx_event_flags_set(
      &ps_event_groups[PS_HW6_RTOS_EVENT_DEBUG_INDEX],
      PS_HW6_RTOS_ACK_OWNER(owner_id),
      TX_OR);
    if (owner_id == PS_HW6_RTOS_OWNER_DISPLAY)
    {
      g_ps_hw6_owner_probe.display_ack_set_status = status;
    }
    else if (owner_id == PS_HW6_RTOS_OWNER_AUDIO)
    {
      g_ps_hw6_owner_probe.audio_ack_set_status = status;
    }
  }
  else if ((owner_id > PS_HW6_RTOS_OWNER_POWER) &&
           (owner_id <= PS_HW6_RTOS_OWNER_COMM) &&
           (command == PS_HW6_RTOS_COMMAND_POWER_QUIESCE))
  {
    (void)PS_HW6_OwnerStateMachines_QuiesceForPowerBarrier(owner_id);
    (void)tx_event_flags_set(
      &ps_event_groups[PS_HW6_RTOS_EVENT_DEBUG_INDEX],
      PS_HW6_RTOS_ACK_OWNER(owner_id), TX_OR);
  }
  else if ((owner_id > PS_HW6_RTOS_OWNER_POWER) &&
           (owner_id <= PS_HW6_RTOS_OWNER_COMM) &&
           (command == PS_HW6_RTOS_COMMAND_POST_STOP_RESUME))
  {
    (void)PS_HW6_OwnerStateMachines_ResumeForPostStopBarrier(owner_id);
    (void)tx_event_flags_set(
      &ps_event_groups[PS_HW6_RTOS_EVENT_DEBUG_INDEX],
      PS_HW6_RTOS_ACK_OWNER(owner_id), TX_OR);
  }
  else if ((owner_id > PS_HW6_RTOS_OWNER_POWER) &&
           (owner_id <= PS_HW6_RTOS_OWNER_COMM) &&
           ((command == PS_HW6_RTOS_COMMAND_RESUME) ||
            (command == PS_HW6_RTOS_COMMAND_QUIESCE)) &&
           (cycle_index < PS_HW6_OWNER_SM_CYCLE_COUNT))
  {
    if (command == PS_HW6_RTOS_COMMAND_RESUME)
    {
      (void)PS_HW6_OwnerStateMachines_Resume(owner_id, cycle_index);
    }
    else
    {
      (void)PS_HW6_OwnerStateMachines_Quiesce(owner_id, cycle_index);
    }
    (void)tx_event_flags_set(
      &ps_event_groups[PS_HW6_RTOS_EVENT_DEBUG_INDEX],
      PS_HW6_RTOS_ACK_OWNER(owner_id), TX_OR);
  }
}

static void PS_HW6_RTOS_UpdateRuntimeComplete(void)
{
  if ((g_ps_hw6_rtos_probe.owner_start_mask == PS_HW6_RTOS_OWNER_MASK) &&
      (g_ps_hw6_rtos_probe.queue_selftest_mask == PS_HW6_RTOS_OWNER_MASK) &&
      (g_ps_hw6_rtos_probe.event_selftest_mask == PS_HW6_RTOS_EVENT_MASK))
  {
    if (g_ps_hw6_rtos_probe.runtime_complete == 0UL)
    {
      PS_HW6_TraceSwoLifecycle(PS_HW6_TRACE_SWO_BOOT_DONE);
    }
    g_ps_hw6_rtos_probe.runtime_complete = 1UL;
  }
}

static ULONG PS_HW6_RTOS_OwnerReceiveWaitTicks(uint32_t owner_id,
                                                uint32_t now_tick)
{
  int32_t remaining_ticks;
  ULONG wait_ticks = PS_HW6_RTOS_HEARTBEAT_TICKS;

  if (owner_id == PS_HW6_RTOS_OWNER_INPUT)
  {
    wait_ticks = (ULONG)PS_InputButtons_NextWaitTicks(
      now_tick,
      (uint32_t)PS_HW6_RTOS_HEARTBEAT_TICKS);
    if ((PS_HW6_OwnerStateMachines_JoystickCalibrationCaptureActive() !=
         0UL) &&
        (PS_HW6_OwnerStateMachines_JoystickCalibrationCaptureNextTick() !=
         0UL))
    {
      remaining_ticks = (int32_t)
        (PS_HW6_OwnerStateMachines_JoystickCalibrationCaptureNextTick() -
         now_tick);
      if (remaining_ticks <= 0)
      {
        wait_ticks = TX_NO_WAIT;
      }
      else if ((uint32_t)remaining_ticks < (uint32_t)wait_ticks)
      {
        wait_ticks = (ULONG)remaining_ticks;
      }
    }
    if ((g_ps_ui_router_probe.current_page ==
         PS_UI_ROUTER_PAGE_CALIBRATION) &&
        (g_ps_ui_router_probe.calibration_page ==
         PS_UI_ROUTER_CAL_JOYSTICK_REVIEW) &&
        (g_ps_hw6_owner_sm_probe.joystick_calibration_session_active != 0UL) &&
        (ps_joystick_calibration_review_next_tick != 0UL))
    {
      remaining_ticks = (int32_t)
        (ps_joystick_calibration_review_next_tick - now_tick);
      if (remaining_ticks <= 0)
      {
        wait_ticks = TX_NO_WAIT;
      }
      else if ((uint32_t)remaining_ticks < (uint32_t)wait_ticks)
      {
        wait_ticks = (ULONG)remaining_ticks;
      }
    }
    if (ps_joystick_awake_poll_next_tick != 0UL)
    {
      remaining_ticks = (int32_t)
        (ps_joystick_awake_poll_next_tick - now_tick);
      if (remaining_ticks <= 0)
      {
        wait_ticks = TX_NO_WAIT;
      }
      else if ((uint32_t)remaining_ticks < (uint32_t)wait_ticks)
      {
        wait_ticks = (ULONG)remaining_ticks;
      }
    }
    return wait_ticks;
  }

  if (owner_id == PS_HW6_RTOS_OWNER_RUNTIME)
  {
    if ((ps_runtime_interaction_state ==
         PS_HW6_RUNTIME_INTERACTION_STATE_ACTIVE) &&
        (ps_runtime_interaction_deadline_tick != 0UL))
    {
      remaining_ticks = (int32_t)
        (ps_runtime_interaction_deadline_tick - now_tick);
      if (remaining_ticks <= 0)
      {
        wait_ticks = TX_NO_WAIT;
      }
      else if ((uint32_t)remaining_ticks < (uint32_t)wait_ticks)
      {
        wait_ticks = (ULONG)remaining_ticks;
      }
    }
    if (ps_runtime_interaction_cue_active != 0UL)
    {
      remaining_ticks = (int32_t)
        (ps_runtime_interaction_cue_deadline_tick - now_tick);
      if (remaining_ticks <= 0)
      {
        wait_ticks = TX_NO_WAIT;
      }
      else if ((uint32_t)remaining_ticks < (uint32_t)wait_ticks)
      {
        wait_ticks = (ULONG)remaining_ticks;
      }
    }
    if (ps_runtime_interaction_activation_active != 0UL)
    {
      remaining_ticks = (int32_t)
        (ps_runtime_interaction_activation_deadline_tick - now_tick);
      if (remaining_ticks <= 0)
      {
        wait_ticks = TX_NO_WAIT;
      }
      else if ((uint32_t)remaining_ticks < (uint32_t)wait_ticks)
      {
        wait_ticks = (ULONG)remaining_ticks;
      }
    }
    return wait_ticks;
  }

  if ((owner_id != PS_HW6_RTOS_OWNER_DISPLAY) ||
      (ps_display_blink_next_tick == 0UL) ||
      (ps_display_blink_stop2_suppressed != 0UL))
  {
    return wait_ticks;
  }

  remaining_ticks = (int32_t)(ps_display_blink_next_tick - now_tick);
  g_ps_hw6_rtos_probe.display_deadline_wait_count++;
  if (remaining_ticks <= 0)
  {
    wait_ticks = TX_NO_WAIT;
    g_ps_hw6_rtos_probe.display_deadline_due_count++;
  }
  else if ((uint32_t)remaining_ticks < PS_HW6_RTOS_HEARTBEAT_TICKS)
  {
    wait_ticks = (ULONG)remaining_ticks;
  }

  g_ps_hw6_rtos_probe.display_deadline_wait_last_ticks =
    (uint32_t)wait_ticks;
  return wait_ticks;
}

static void PS_HW6_RTOS_OwnerEntry(ULONG thread_input)
{
  const uint32_t owner_id = (uint32_t)thread_input;
  ULONG message[PS_HW6_RTOS_MESSAGE_WORDS];
  ULONG now;
  ULONG receive_wait_ticks;
  UINT status;
  ps_status_t router_status;
  ps_input_button_logical_record_t button_record;
  ps_input_start_power_event_t start_power_event;
  uint32_t button_drain_count;
  uint32_t start_power_timestamp;
  uint32_t start_power_hold_ticks;
  uint32_t start_power_drain_count;
  uint32_t start_press_timestamp;
  uint32_t start_press_hold_ticks;
  uint32_t start_long_timestamp;
  uint32_t start_long_hold_ticks;
  uint32_t audio_clock_release_after_request;
  uint32_t router_event;
  uint32_t send_ui_render;
  uint32_t boot_gate_clear_count;
  uint32_t word;

  if (owner_id >= PS_HW6_RTOS_OWNER_COUNT)
  {
    for (;;)
    {
      tx_thread_relinquish();
    }
  }

  now = tx_time_get();
  g_ps_hw6_rtos_probe.owner_last_tick[owner_id] = (uint32_t)now;
  g_ps_hw6_rtos_probe.owner_start_mask |= (1UL << owner_id);
  PS_HW6_RTOS_RecordThreadStackProbe(owner_id);
  PS_HW6_RTOS_UpdateRuntimeComplete();

  if ((owner_id == PS_HW6_RTOS_OWNER_DISPLAY) &&
      (ps_display_bootstrap_sent == 0UL))
  {
    ps_display_bootstrap_sent = 1UL;
    g_ps_hw6_rtos_probe.boot_display_bootstrap_sent = 1UL;
    (void)PS_HW6_RTOS_RequestDisplayClockCapabilities(
      PS_HW6_RTOS_DISPLAY_CLOCK_REASON_TRANSFER,
      PS_HW6_RTOS_DISPLAY_CLOCK_TRANSFER_CAPABILITIES);
    (void)PS_HW6_DisplayOwner_ClearBootHold();
    (void)PS_HW6_RTOS_RequestDisplayClockCapabilities(
      PS_HW6_RTOS_DISPLAY_CLOCK_REASON_RELEASE,
      0UL);
  }

  for (;;)
  {
    receive_wait_ticks = PS_HW6_RTOS_OwnerReceiveWaitTicks(
      owner_id,
      (uint32_t)tx_time_get());
    status = tx_queue_receive(&ps_queues[owner_id], message,
                              receive_wait_ticks);
    now = tx_time_get();
    g_ps_hw6_rtos_probe.owner_heartbeat[owner_id]++;
    g_ps_hw6_rtos_probe.owner_last_tick[owner_id] = (uint32_t)now;
    PS_HW6_RTOS_RecordThreadStackProbe(owner_id);

    if (status == TX_SUCCESS)
    {
      g_ps_hw6_rtos_probe.queue_receive_count[owner_id]++;
      for (word = 0U; word < PS_HW6_RTOS_MESSAGE_WORDS; ++word)
      {
        g_ps_hw6_rtos_probe.queue_last_message[owner_id][word] =
          (uint32_t)message[word];
      }

      if (PS_HW6_RTOS_MessageIsValid(owner_id, message) != 0UL)
      {
        g_ps_hw6_rtos_probe.queue_selftest_mask |= (1UL << owner_id);
      }
      else if (PS_HW6_RTOS_InputRawMessageIsValid(owner_id, message) != 0UL)
      {
        uint32_t packed_edge = (uint32_t)message[2];
        ps_input_button_id_t button_id = (ps_input_button_id_t)
          (packed_edge & PS_HW6_RTOS_INPUT_RAW_BUTTON_MASK);
        uint32_t active =
          (packed_edge >> PS_HW6_RTOS_INPUT_RAW_ACTIVE_SHIFT) &
          PS_HW6_RTOS_INPUT_RAW_ACTIVE_MASK;

        g_ps_hw6_rtos_probe.input_raw_dequeue_count++;
        PS_InputButtons_ProcessRawEdge(
          button_id, active, (uint32_t)message[3]);
      }
      else if (PS_HW6_RTOS_InputJoystickMessageIsValid(
                 owner_id, message) != 0UL)
      {
        g_ps_hw6_rtos_probe.joystick_irq_dequeue_count++;
      }
      else if (PS_HW6_RTOS_CommandIsValid(owner_id, message) != 0UL)
      {
        PS_HW6_RTOS_HandleOwnerCommand(
          owner_id, message[2], PS_HW6_RTOS_CommandCycleIndex(message));
      }
      else if (PS_HW6_RTOS_StorageMscCommandIsValid(owner_id, message) != 0UL)
      {
        PS_HW6_OwnerStateMachines_HandleStorageMsc(message[2]);
      }
      else if (PS_HW6_RTOS_PowerInputCommandIsValid(owner_id, message) != 0UL)
      {
        if (PS_HW6_OwnerStateMachines_HandleStartShippingIntent(
              (uint32_t)message[2],
              (uint32_t)message[3]) == HAL_OK)
        {
          router_event =
            PS_HW6_RTOS_RouterEventForStartPower((uint32_t)message[2]);
          if (router_event != 0UL)
          {
            g_ps_ui_router_request_event = router_event;
            g_ps_ui_router_request = 1UL;
          }
          if ((uint32_t)message[2] ==
              (uint32_t)PS_INPUT_START_POWER_EVENT_RELEASED_BEFORE_SHIP)
          {
            (void)PS_HW6_RTOS_ResumePowerSuspendedRuntime(
              PS_HW6_RTOS_ADMISSION_RESUME_REASON_START_CANCEL);
          }
        }
      }
      else if (PS_HW6_RTOS_RuntimeInputCommandIsValid(owner_id, message) != 0UL)
      {
        PS_HW6_RTOS_HandleRuntimeInput(message);
      }
      else if (PS_HW6_RTOS_DisplayUiCommandIsValid(owner_id, message) != 0UL)
      {
        HAL_StatusTypeDef display_status;
        uint32_t previous_page = g_ps_hw6_owner_probe.display_ui_page;
        uint32_t previous_presentation_id =
          g_ps_hw6_owner_probe.display_waiting_presentation_id;
        uint32_t previous_sequence_frame =
          ps_display_waiting_sequence_frame;
        uint32_t previous_sequence_count =
          ps_display_waiting_sequence_count;
        uint32_t previous_deadline_tick = ps_display_blink_next_tick;

        (void)PS_HW6_RTOS_RequestDisplayClockCapabilities(
          PS_HW6_RTOS_DISPLAY_CLOCK_REASON_TRANSFER,
          PS_HW6_RTOS_DISPLAY_CLOCK_TRANSFER_CAPABILITIES);
        display_status = PS_HW6_DisplayOwner_RenderUI(
          (uint32_t)message[2],
          PS_HW6_RTOS_DisplayUiPackedCalibration(message[3]),
          PS_HW6_RTOS_DisplayUiPackedFocus(message[3]),
          PS_HW6_RTOS_DisplayUiPackedShutdown(message[3]),
          PS_HW6_RTOS_DisplayUiPackedCountdown(message[3]));
        (void)PS_HW6_RTOS_RequestDisplayClockCapabilities(
          PS_HW6_RTOS_DISPLAY_CLOCK_REASON_RELEASE,
          0UL);
        if (display_status == HAL_OK)
        {
          PS_HW6_RTOS_SynchronizeDisplayWaitingTimelineAfterUi(
            previous_page,
            previous_presentation_id,
            previous_sequence_frame,
            previous_sequence_count,
            previous_deadline_tick,
            (uint32_t)now);
        }
        else
        {
          PS_HW6_RTOS_ResetDisplayCursorBlink((uint32_t)now);
        }
      }
      else if (PS_HW6_RTOS_UiLifecycleCommandIsValid(owner_id, message) !=
               0UL)
      {
        (void)PS_HW6_RTOS_RequestUiClockCapabilities(
          PS_HW6_RTOS_UI_CLOCK_REASON_REACTIVE_TRANSACTION,
          PS_HW6_RTOS_UI_CLOCK_REACTIVE_CAPABILITIES);
        router_status = PS_UIRouter_Dispatch((uint32_t)message[2]);
        if (router_status == PS_STATUS_OK)
        {
          PS_HW6_RTOS_SendCurrentUiRenderCommand();
        }
        (void)PS_HW6_RTOS_RequestUiClockCapabilities(
          PS_HW6_RTOS_UI_CLOCK_REASON_RELEASE,
          0UL);
      }
      else if (PS_HW6_RTOS_UiInputCommandIsValid(owner_id, message) != 0UL)
      {
        uint32_t command = (uint32_t)message[2];
        uint32_t packed_input = (uint32_t)message[3];
        uint32_t button = PS_HW6_RTOS_UiInputPackedSource(packed_input);
        uint32_t action = (uint32_t)PS_UI_ROUTER_ACTION_NONE;

        (void)PS_HW6_RTOS_RequestUiClockCapabilities(
          PS_HW6_RTOS_UI_CLOCK_REASON_REACTIVE_TRANSACTION,
          PS_HW6_RTOS_UI_CLOCK_REACTIVE_CAPABILITIES);

        if (command == PS_HW6_RTOS_UI_INPUT_JOYSTICK_STATE)
        {
          uint32_t candidate =
            PS_HW6_RTOS_UiInputPackedCandidate(packed_input);
          uint32_t resolved =
            PS_HW6_RTOS_UiInputPackedResolved(packed_input);
          uint32_t dispatch =
            PS_HW6_RTOS_UiInputPackedDispatch(packed_input);

          router_status = PS_UIRouter_RecordJoystickState(candidate,
                                                          resolved);
          if ((router_status == PS_STATUS_OK) && (dispatch != 0UL))
          {
            router_status = PS_UIRouter_Dispatch(
              PS_HW6_RTOS_RouterEventForLogicalSource(button));
          }
          if (router_status == PS_STATUS_OK)
          {
            PS_HW6_RTOS_SendCurrentUiRenderCommand();
          }
        }
        else if ((button == (uint32_t)PS_INPUT_BUTTON_ID_B) &&
                 (PS_HW6_RTOS_UiMscExportActive() != 0UL))
        {
          g_ps_hw6_rtos_probe.ui_action_msc_exit_intercept_count++;
          PS_HW6_RTOS_HandleUiRouterAction(
            (uint32_t)PS_UI_ROUTER_ACTION_MSC_EXIT);
        }
        else if (PS_HW6_RTOS_RequestJoystickCalibrationCapture(button) == 0UL)
        {
          router_status = PS_UIRouter_Dispatch(
            PS_HW6_RTOS_RouterEventForLogicalSource(button));
          if (router_status == PS_STATUS_OK)
          {
            action = PS_UIRouter_TakeAction();
            if (action != (uint32_t)PS_UI_ROUTER_ACTION_NONE)
            {
              PS_HW6_RTOS_HandleUiRouterAction(action);
            }
            else
            {
              if ((button == (uint32_t)PS_INPUT_BUTTON_ID_B) &&
                  (g_ps_hw6_rtos_probe.runtime_current_class ==
                   (uint32_t)PS_HW6_RUNTIME_CLASS_INSTALLER) &&
                  (g_ps_ui_router_probe.package_state ==
                   (uint32_t)PS_UI_ROUTER_PACKAGE_NONE) &&
                  (g_ps_ui_router_probe.current_page !=
                   (uint32_t)PS_UI_ROUTER_PAGE_PACKAGE_BROWSER))
              {
                (void)PS_HW6_RTOS_RequestRuntimeCommand(
                  PS_HW6_RTOS_COMMAND_RUNTIME_INSTALLER_COMPLETE);
              }
              PS_HW6_RTOS_SendCurrentUiRenderCommand();
            }
          }
        }
        (void)PS_HW6_RTOS_RequestUiClockCapabilities(
          PS_HW6_RTOS_UI_CLOCK_REASON_RELEASE,
          0UL);
      }
      else
      {
        g_ps_hw6_rtos_probe.queue_message_error_count[owner_id]++;
      }
    }
    else if (status == TX_QUEUE_EMPTY)
    {
      g_ps_hw6_rtos_probe.queue_timeout_count[owner_id]++;
    }
    else
    {
      g_ps_hw6_rtos_probe.queue_message_error_count[owner_id]++;
    }

    PS_HW6_RTOS_UpdateRuntimeComplete();

    if ((owner_id == PS_HW6_RTOS_OWNER_DISPLAY) &&
        (ps_power_boot_done != 0UL) &&
        (g_ps_hw6_rtos_probe.runtime_complete != 0UL))
    {
      PS_HW6_RTOS_RunDisplayCursorBlinkPeriodic((uint32_t)now);
    }

    if ((owner_id == PS_HW6_RTOS_OWNER_RUNTIME) &&
        (ps_power_boot_done != 0UL) &&
        (g_ps_hw6_rtos_probe.runtime_complete != 0UL))
    {
      PS_HW6_RTOS_RuntimeInteractionService((uint32_t)now);
    }

    if ((owner_id == PS_HW6_RTOS_OWNER_AUDIO) &&
        (ps_power_boot_done != 0UL) &&
        (g_ps_hw6_rtos_probe.runtime_complete != 0UL))
    {
      if (g_ps_hw6_audio_clock_probe_request != 0UL)
      {
        audio_clock_release_after_request =
          g_ps_hw6_audio_clock_probe_release_request;
        g_ps_hw6_audio_clock_probe_request = 0UL;
        (void)PS_HW6_RTOS_RequestAudioClockCapabilities(
          PS_HW6_RTOS_AUDIO_CLOCK_REASON_REACTIVE_SFX,
          PS_HW6_RTOS_AUDIO_CLOCK_SAI_CAPABILITIES);
        if (audio_clock_release_after_request != 0UL)
        {
          g_ps_hw6_audio_clock_probe_release_request = 0UL;
          (void)PS_HW6_RTOS_RequestAudioClockCapabilities(
            PS_HW6_RTOS_AUDIO_CLOCK_REASON_RELEASE,
            0UL);
        }
      }
      if (g_ps_hw6_audio_clock_probe_release_request != 0UL)
      {
        g_ps_hw6_audio_clock_probe_release_request = 0UL;
        (void)PS_HW6_RTOS_RequestAudioClockCapabilities(
          PS_HW6_RTOS_AUDIO_CLOCK_REASON_RELEASE,
          0UL);
      }
    }

    if ((owner_id == PS_HW6_RTOS_OWNER_RUNTIME) &&
        (ps_power_boot_done != 0UL) &&
        (g_ps_hw6_rtos_probe.runtime_complete != 0UL))
    {
      if (g_ps_hw6_runtime_reactive_stub_request != 0UL)
      {
        g_ps_hw6_runtime_reactive_stub_request = 0UL;
        (void)PS_HW6_RTOS_RequestRuntimeCommand(
          PS_HW6_RTOS_COMMAND_RUNTIME_PACKAGE_REACTIVE_STUB);
      }
      if (g_ps_hw6_runtime_realtime_stub_request != 0UL)
      {
        g_ps_hw6_runtime_realtime_stub_request = 0UL;
        (void)PS_HW6_RTOS_RequestRuntimeCommand(
          PS_HW6_RTOS_COMMAND_RUNTIME_PACKAGE_REALTIME_STUB);
      }
      if (g_ps_hw6_runtime_return_request != 0UL)
      {
        g_ps_hw6_runtime_return_request = 0UL;
        (void)PS_HW6_RTOS_RequestRuntimeCommand(
          PS_HW6_RTOS_COMMAND_RUNTIME_PACKAGE_RETURN);
      }
      if (g_ps_hw6_runtime_suspend_request != 0UL)
      {
        g_ps_hw6_runtime_suspend_request = 0UL;
        (void)PS_HW6_RTOS_RequestRuntimeCommand(
          PS_HW6_RTOS_COMMAND_RUNTIME_SUSPEND);
      }
      if (g_ps_hw6_runtime_resume_request != 0UL)
      {
        g_ps_hw6_runtime_resume_request = 0UL;
        (void)PS_HW6_RTOS_RequestRuntimeCommand(
          PS_HW6_RTOS_COMMAND_RUNTIME_RESUME);
      }
    }
    if ((owner_id == PS_HW6_RTOS_OWNER_UI) &&
        (ps_power_boot_done != 0UL) &&
        (g_ps_hw6_rtos_probe.runtime_complete != 0UL))
    {
      if (g_ps_hw6_admission_msc_enter_dry_run_request != 0UL)
      {
        g_ps_hw6_admission_msc_enter_dry_run_request = 0UL;
        (void)PS_HW6_RTOS_AdmitSystemAction(
          PS_HW6_RTOS_ADMISSION_ACTION_UI_MSC_ENTER);
      }
    }

    if ((owner_id == PS_HW6_RTOS_OWNER_POWER) &&
        (ps_power_boot_done != 0UL) &&
        (g_ps_hw6_rtos_probe.runtime_complete != 0UL))
    {
      if (g_ps_hw6_admission_power_shutdown_dry_run_request != 0UL)
      {
        g_ps_hw6_admission_power_shutdown_dry_run_request = 0UL;
        (void)PS_HW6_RTOS_RequestPowerSystemAdmission(
          (uint32_t)PS_HW6_POWER_QUIESCE_REASON_START_SHUTDOWN);
      }
      if (g_ps_hw6_admission_power_battery_dry_run_request != 0UL)
      {
        g_ps_hw6_admission_power_battery_dry_run_request = 0UL;
        (void)PS_HW6_RTOS_RequestPowerSystemAdmission(
          (uint32_t)PS_HW6_POWER_QUIESCE_REASON_BATTERY_CRITICAL);
      }
      if (g_ps_hw6_admission_power_cancel_dry_run_request != 0UL)
      {
        g_ps_hw6_admission_power_cancel_dry_run_request = 0UL;
        (void)PS_HW6_RTOS_ResumePowerSuspendedRuntime(
          PS_HW6_RTOS_ADMISSION_RESUME_REASON_START_CANCEL);
      }
    }

    if ((owner_id == PS_HW6_RTOS_OWNER_POWER) &&
        (ps_power_boot_done == 0UL) &&
        (ps_display_bootstrap_sent != 0UL) &&
        (g_ps_hw6_rtos_probe.runtime_complete != 0UL))
    {
      (void)PS_HW6_RTOS_BootParkStorageUsb();
      (void)PS_HW6_ClockPolicy_ApplyBootIdleDomains();
      (void)PS_HW6_OwnerStateMachines_Stabilize(
        PS_HW6_RTOS_OWNER_POWER);
      ps_power_boot_done = 1UL;
      g_ps_hw6_rtos_probe.boot_power_done = 1UL;
    }
    if ((owner_id == PS_HW6_RTOS_OWNER_POWER) &&
        (ps_power_boot_done != 0UL) &&
        (g_ps_hw6_rtos_probe.runtime_complete != 0UL) &&
        (g_ps_hw6_rtos_probe.boot_idle_peripheral_park_request_count == 0UL))
    {
      (void)PS_HW6_RTOS_RunBootIdlePeripheralPark();
    }
    if ((owner_id == PS_HW6_RTOS_OWNER_POWER) &&
        (g_ps_hw6_owner_sm_start_request != 0UL) &&
        (g_ps_hw6_rtos_probe.runtime_complete != 0UL) &&
        (g_ps_hw6_owner_sm_probe.complete == 0UL))
    {
      g_ps_hw6_owner_sm_start_request = 0UL;
      g_ps_hw6_owner_probe.power_command_send_status = TX_SUCCESS;
      PS_HW6_RTOS_RunPowerWorkflow();
    }
    if ((owner_id == PS_HW6_RTOS_OWNER_POWER) &&
        (g_ps_hw6_pmic_software_ship_request != 0UL) &&
        (g_ps_hw6_rtos_probe.runtime_complete != 0UL))
    {
      g_ps_hw6_pmic_software_ship_request = 0UL;
      (void)PS_HW6_PowerOwner_EnterSoftwareShipmentMode();
    }
    if ((owner_id == PS_HW6_RTOS_OWNER_POWER) &&
        (g_ps_hw6_power_stop2_eligibility_request != 0UL) &&
        (g_ps_hw6_rtos_probe.runtime_complete != 0UL))
    {
      g_ps_hw6_power_stop2_eligibility_request = 0UL;
      (void)PS_HW6_RTOS_RunStop2EligibilityDryRun();
    }
    if ((owner_id == PS_HW6_RTOS_OWNER_POWER) &&
        (g_ps_hw6_power_stop2_controlled_entry_request != 0UL) &&
        (g_ps_hw6_rtos_probe.runtime_complete != 0UL))
    {
      g_ps_hw6_power_stop2_controlled_entry_request = 0UL;
      (void)PS_HW6_RTOS_RunStop2ControlledEntry();
    }
    if ((owner_id == PS_HW6_RTOS_OWNER_POWER) &&
        (g_ps_hw6_power_stop2_auto_idle_dry_run_request != 0UL) &&
        (g_ps_hw6_rtos_probe.runtime_complete != 0UL))
    {
      g_ps_hw6_power_stop2_auto_idle_dry_run_request = 0UL;
      (void)PS_HW6_RTOS_RunStop2AutoIdleCheck((uint32_t)now, 0UL, 0UL);
    }
    if ((owner_id == PS_HW6_RTOS_OWNER_POWER) &&
        (g_ps_hw6_power_stop2_auto_idle_entry_request != 0UL) &&
        (g_ps_hw6_rtos_probe.runtime_complete != 0UL))
    {
      g_ps_hw6_power_stop2_auto_idle_entry_request = 0UL;
      (void)PS_HW6_RTOS_RunStop2AutoIdleEntryTest((uint32_t)now);
    }
    if ((owner_id == PS_HW6_RTOS_OWNER_POWER) &&
        (g_ps_hw6_power_stop2_lpbam_prepare_request != 0UL) &&
        (ps_power_boot_done != 0UL) &&
        (g_ps_hw6_rtos_probe.runtime_complete != 0UL))
    {
      g_ps_hw6_power_stop2_lpbam_prepare_request = 0UL;
      (void)PS_HW6_RTOS_RequestDisplayLpbamPrepare();
    }
    if ((owner_id == PS_HW6_RTOS_OWNER_POWER) &&
        (g_ps_hw6_power_stop2_lpbam_abort_request != 0UL) &&
        (ps_power_boot_done != 0UL) &&
        (g_ps_hw6_rtos_probe.runtime_complete != 0UL))
    {
      g_ps_hw6_power_stop2_lpbam_abort_request = 0UL;
      (void)PS_HW6_RTOS_RequestDisplayLpbamAbort(0UL);
    }
    if ((owner_id == PS_HW6_RTOS_OWNER_POWER) &&
        (g_ps_hw6_power_stop2_lpbam_abort_late_test_request != 0UL) &&
        (ps_power_boot_done != 0UL) &&
        (g_ps_hw6_rtos_probe.runtime_complete != 0UL))
    {
      g_ps_hw6_power_stop2_lpbam_abort_late_test_request = 0UL;
      (void)PS_HW6_RTOS_RunStop2LpbamAbortLateTest((uint32_t)now);
    }
    if ((owner_id == PS_HW6_RTOS_OWNER_POWER) &&
        (ps_power_boot_done != 0UL) &&
        (g_ps_hw6_rtos_probe.runtime_complete != 0UL))
    {
      PS_HW6_RTOS_RunStop2AutoIdlePeriodic((uint32_t)now);
    }
    if ((owner_id == PS_HW6_RTOS_OWNER_POWER) &&
        (g_ps_hw6_power_sleep_prep_request != 0UL) &&
        (ps_power_boot_done != 0UL) &&
        (g_ps_hw6_rtos_probe.runtime_complete != 0UL))
    {
      g_ps_hw6_power_sleep_prep_request = 0UL;
      (void)PS_HW6_OwnerStateMachines_RunSleepPrepScaffold();
    }
    if ((owner_id == PS_HW6_RTOS_OWNER_POWER) &&
        (g_ps_hw6_power_stop2_active_prep_request != 0UL) &&
        (ps_power_boot_done != 0UL) &&
        (g_ps_hw6_rtos_probe.runtime_complete != 0UL))
    {
      g_ps_hw6_power_stop2_active_prep_request = 0UL;
      (void)PS_HW6_RTOS_RunStop2ActiveOwnerPrep();
    }
    if ((owner_id == PS_HW6_RTOS_OWNER_POWER) &&
        (g_ps_hw6_power_stop2_active_enter_request != 0UL) &&
        (ps_power_boot_done != 0UL) &&
        (g_ps_hw6_rtos_probe.runtime_complete != 0UL))
    {
      g_ps_hw6_power_stop2_active_enter_request = 0UL;
      (void)PS_HW6_OwnerStateMachines_RunStop2AfterActivePrepScaffold();
    }
    if ((owner_id == PS_HW6_RTOS_OWNER_POWER) &&
        (g_ps_hw6_power_stop2_active_resume_request != 0UL) &&
        (ps_power_boot_done != 0UL) &&
        (g_ps_hw6_rtos_probe.runtime_complete != 0UL))
    {
      g_ps_hw6_power_stop2_active_resume_request = 0UL;
      (void)PS_HW6_RTOS_RunStop2ActiveResumeScaffold();
    }
    if ((owner_id == PS_HW6_RTOS_OWNER_POWER) &&
        (g_ps_hw6_power_stop2_request != 0UL) &&
        (ps_power_boot_done != 0UL) &&
        (g_ps_hw6_rtos_probe.runtime_complete != 0UL))
    {
      g_ps_hw6_power_stop2_request = 0UL;
      (void)PS_HW6_OwnerStateMachines_RunStop2StartWakeScaffold();
    }
    if ((owner_id == PS_HW6_RTOS_OWNER_POWER) &&
        (ps_power_boot_done != 0UL) &&
        (g_ps_hw6_rtos_probe.runtime_complete != 0UL) &&
        (ps_pmic_int_pending_count != ps_pmic_int_consumed_count))
    {
      uint32_t pending_count = ps_pmic_int_pending_count;
      uint32_t pending_delta = pending_count - ps_pmic_int_consumed_count;
      uint32_t irq_count = ps_pmic_int_irq_count;
      uint32_t last_pin = ps_pmic_int_last_pin;
      uint32_t last_level = ps_pmic_int_last_level;
      uint32_t irq_tick = ps_pmic_int_last_irq_tick;

      ps_pmic_int_consumed_count = pending_count;
      g_ps_hw6_rtos_probe.pmic_int_irq_count = irq_count;
      g_ps_hw6_rtos_probe.pmic_int_pending_count = 0UL;
      g_ps_hw6_rtos_probe.pmic_int_consumed_count =
        ps_pmic_int_consumed_count;
      g_ps_hw6_rtos_probe.pmic_int_last_pin = last_pin;
      g_ps_hw6_rtos_probe.pmic_int_last_level = last_level;
      g_ps_hw6_rtos_probe.pmic_int_last_irq_tick = irq_tick;
      g_ps_hw6_rtos_probe.pmic_int_last_consume_tick = (uint32_t)now;
      (void)PS_HW6_OwnerStateMachines_HandlePmicInterrupt(
        (uint32_t)now,
        pending_delta,
        irq_count,
        last_pin,
        last_level,
        irq_tick);
    }
    if ((owner_id == PS_HW6_RTOS_OWNER_POWER) &&
        (ps_power_boot_done != 0UL) &&
        (g_ps_hw6_rtos_probe.runtime_complete != 0UL))
    {
      (void)PS_HW6_OwnerStateMachines_RunBatteryMonitor((uint32_t)now);
      boot_gate_clear_count =
        g_ps_hw6_owner_sm_probe.battery_policy_boot_restart_gate_clear_count;
      if ((g_ps_hw6_owner_sm_probe.battery_policy_state ==
           PS_HW6_POWER_BATTERY_POLICY_BOOT_RESTART_BLOCKED) &&
          ((g_ps_hw6_rtos_probe.boot_low_battery_ui_sent == 0UL) ||
           (g_ps_ui_router_probe.shutdown_state !=
            PS_UI_ROUTER_SHUTDOWN_LOW_BATTERY_BOOT)) &&
          (g_ps_ui_router_request == 0UL))
      {
        g_ps_ui_router_request_event =
          PS_UI_ROUTER_EVENT_LOW_BATTERY_BOOT_BLOCK;
        g_ps_ui_router_request = 1UL;
      }
      else if ((g_ps_hw6_owner_sm_probe.battery_policy_state ==
                PS_HW6_POWER_BATTERY_POLICY_BOOT_CHARGE_RECOVERY) &&
               ((g_ps_hw6_rtos_probe.boot_low_battery_ui_sent == 0UL) ||
                (g_ps_ui_router_probe.shutdown_state !=
                 PS_UI_ROUTER_SHUTDOWN_LOW_BATTERY_CHARGE)) &&
               (g_ps_ui_router_request == 0UL))
      {
        g_ps_ui_router_request_event =
          PS_UI_ROUTER_EVENT_LOW_BATTERY_CHARGE_RECOVERY;
        g_ps_ui_router_request = 1UL;
      }
      else if ((boot_gate_clear_count != 0UL) &&
               (g_ps_hw6_rtos_probe.boot_low_battery_ui_sent != 0UL) &&
               (g_ps_hw6_rtos_probe.boot_low_battery_recover_ui_sent == 0UL) &&
               (g_ps_ui_router_request == 0UL))
      {
        g_ps_ui_router_request_event = PS_UI_ROUTER_EVENT_RECOVER_OK;
        g_ps_ui_router_request = 1UL;
      }
    }
    if ((owner_id == PS_HW6_RTOS_OWNER_INPUT) &&
        (g_ps_hw6_joystick_xyz_capture_request != 0UL) &&
        (g_ps_hw6_rtos_probe.runtime_complete != 0UL))
    {
      HAL_StatusTypeDef capture_status;
      uint32_t capture_mode = g_ps_hw6_joystick_xyz_capture_mode;
      uint32_t capture_router_event = 0UL;

      if (capture_mode == PS_HW6_JOYSTICK_XYZ_CAPTURE_REST)
      {
        capture_router_event = PS_UI_ROUTER_EVENT_JOYSTICK_XYZ_REST;
      }
      else if (capture_mode == PS_HW6_JOYSTICK_XYZ_CAPTURE_SWEEP)
      {
        capture_router_event = PS_UI_ROUTER_EVENT_JOYSTICK_XYZ_SWEEP;
      }
      else if (capture_mode == PS_HW6_JOYSTICK_XYZ_CAPTURE_SWEEP_Z_HIGH)
      {
        capture_router_event = PS_UI_ROUTER_EVENT_JOYSTICK_XYZ_SWEEP;
      }

      g_ps_hw6_joystick_xyz_capture_request = 0UL;
      if ((capture_router_event != 0UL) &&
          (g_ps_ui_router_request == 0UL))
      {
        g_ps_ui_router_request_event = capture_router_event;
        g_ps_ui_router_request = 1UL;
      }
      capture_status =
        PS_HW6_OwnerStateMachines_RunJoystickXyzCapture(capture_mode);
      if (g_ps_ui_router_request == 0UL)
      {
        g_ps_ui_router_request_event = (capture_status == HAL_OK) ?
          PS_UI_ROUTER_EVENT_JOYSTICK_XYZ_DONE :
          PS_UI_ROUTER_EVENT_JOYSTICK_XYZ_ERROR;
        g_ps_ui_router_request = 1UL;
      }
    }
    if ((owner_id == PS_HW6_RTOS_OWNER_INPUT) &&
        (ps_joystick_calibration_apply_pending != 0UL) &&
        (g_ps_hw6_rtos_probe.runtime_complete != 0UL))
    {
      ps_joystick_calibration_apply_pending = 0UL;
      (void)
        PS_HW6_OwnerStateMachines_ApplyPersistentJoystickCalibration();
    }
    if ((owner_id == PS_HW6_RTOS_OWNER_INPUT) &&
        (ps_joystick_calibration_save_completion_pending != 0UL) &&
        (g_ps_hw6_rtos_probe.runtime_complete != 0UL))
    {
      HAL_StatusTypeDef save_status;

      ps_joystick_calibration_save_completion_pending = 0UL;
      save_status =
        PS_HW6_OwnerStateMachines_CompletePersistentJoystickCalibrationSave();
      g_ps_hw6_rtos_probe.input_policy_lock_active = 0UL;
      if ((save_status == HAL_OK) && (g_ps_ui_router_request == 0UL))
      {
        g_ps_ui_router_request_event =
          PS_HW6_RTOS_RouterEventForCalibrationCapture(
            PS_UI_ROUTER_CAL_JOYSTICK_REVIEW);
        g_ps_ui_router_request = 1UL;
      }
      PS_HW6_RTOS_SendCurrentUiRenderCommand();
    }
    if ((owner_id == PS_HW6_RTOS_OWNER_INPUT) &&
        (g_ps_hw6_joystick_sleep_audit_request != 0UL) &&
        (g_ps_hw6_rtos_probe.runtime_complete != 0UL))
    {
      g_ps_hw6_joystick_sleep_audit_request = 0UL;
      (void)PS_HW6_OwnerStateMachines_RunJoystickSleepAudit();
    }
    if ((owner_id == PS_HW6_RTOS_OWNER_INPUT) &&
        (g_ps_hw6_joystick_sample_request != 0UL) &&
        (g_ps_hw6_rtos_probe.runtime_complete != 0UL))
    {
      g_ps_hw6_joystick_sample_request = 0UL;
      (void)PS_HW6_OwnerStateMachines_RunJoystickSampleProbe();
    }
    if ((owner_id == PS_HW6_RTOS_OWNER_INPUT) &&
        (g_ps_hw6_joystick_live_request != 0UL) &&
        (g_ps_hw6_rtos_probe.runtime_complete != 0UL))
    {
      g_ps_hw6_joystick_live_request = 0UL;
      (void)PS_HW6_OwnerStateMachines_RunJoystickLiveProbe();
    }
    if ((owner_id == PS_HW6_RTOS_OWNER_INPUT) &&
        (g_ps_hw6_joystick_cardinal_request != 0UL) &&
        (g_ps_hw6_rtos_probe.runtime_complete != 0UL))
    {
      HAL_StatusTypeDef cardinal_status;

      g_ps_hw6_joystick_cardinal_request = 0UL;
      ps_joystick_int_consumed_count = ps_joystick_int_pending_count;
      g_ps_hw6_rtos_probe.joystick_irq_pending_count = 0UL;
      cardinal_status =
        PS_HW6_OwnerStateMachines_RunJoystickWakeCardinalProbe();
      if (cardinal_status == HAL_OK)
      {
        (void)
          PS_HW6_OwnerStateMachines_LatchJoystickWakeDirection();
      }
    }
    if ((owner_id == PS_HW6_RTOS_OWNER_INPUT) &&
        (g_ps_hw6_joystick_calibration_capture_request != 0UL) &&
        (g_ps_hw6_rtos_probe.runtime_complete != 0UL))
    {
      uint32_t calibration_page = g_ps_hw6_joystick_calibration_capture_page;
      HAL_StatusTypeDef calibration_status;
      g_ps_hw6_joystick_calibration_capture_request = 0UL;
      calibration_status =
        PS_HW6_OwnerStateMachines_RunJoystickCalibrationCapture(
          calibration_page);
      if ((calibration_status == HAL_OK) &&
          (calibration_page == PS_UI_ROUTER_CAL_JOYSTICK_REVIEW) &&
          (PS_HW6_OwnerStateMachines_JoystickCalibrationSavePending() !=
           0UL))
      {
        UINT save_send_status;

        g_ps_hw6_rtos_probe.input_policy_lock_active = 1UL;
        PS_HW6_RTOS_SendCurrentUiRenderCommand();
        save_send_status = PS_HW6_RTOS_SendCommand(
          PS_HW6_RTOS_OWNER_STORAGE,
          PS_HW6_RTOS_COMMAND_STORAGE_JOYSTICK_CALIBRATION_SAVE);
        if (save_send_status != TX_SUCCESS)
        {
          ps_joystick_calibration_save_completion_pending = 1UL;
        }
      }
      else if ((calibration_status == HAL_OK) &&
          (PS_HW6_OwnerStateMachines_JoystickCalibrationCaptureActive() ==
           0UL) &&
          (g_ps_ui_router_request == 0UL))
      {
        g_ps_ui_router_request_event =
          PS_HW6_RTOS_RouterEventForCalibrationCapture(calibration_page);
        g_ps_ui_router_request = 1UL;
      }
      ps_joystick_calibration_progress_render_next_tick = (uint32_t)now;
      PS_HW6_RTOS_SendCurrentUiRenderCommand();
    }
    if ((owner_id == PS_HW6_RTOS_OWNER_INPUT) &&
        (PS_HW6_OwnerStateMachines_JoystickCalibrationCaptureActive() !=
         0UL) &&
        (g_ps_hw6_rtos_probe.runtime_complete != 0UL))
    {
      HAL_StatusTypeDef calibration_status =
        PS_HW6_OwnerStateMachines_StepJoystickCalibrationCapture();
      uint32_t capture_complete =
        (PS_HW6_OwnerStateMachines_JoystickCalibrationCaptureActive() ==
         0UL) ? 1UL : 0UL;

      if ((capture_complete != 0UL) &&
          (calibration_status == HAL_OK) &&
          (g_ps_ui_router_request == 0UL))
      {
        g_ps_ui_router_request_event =
          PS_HW6_RTOS_RouterEventForCalibrationCapture(
            g_ps_hw6_owner_sm_probe.joystick_calibration_capture_page);
        g_ps_ui_router_request = 1UL;
      }
      if ((capture_complete != 0UL) ||
          ((int32_t)((uint32_t)now -
           ps_joystick_calibration_progress_render_next_tick) >= 0))
      {
        uint32_t render_period_ticks = PS_HW6_RTOS_MsToTicks(
          (uint32_t)KNOB_INPUT_JOYSTICK_CAL_PROGRESS_PERIOD_MS);
        if (render_period_ticks == 0UL)
        {
          render_period_ticks = 1UL;
        }
        ps_joystick_calibration_progress_render_next_tick =
          (uint32_t)now + render_period_ticks;
        PS_HW6_RTOS_SendCurrentUiRenderCommand();
      }
      if (capture_complete != 0UL)
      {
        ps_joystick_calibration_progress_render_next_tick = 0UL;
      }
    }
    if ((owner_id == PS_HW6_RTOS_OWNER_INPUT) &&
        (g_ps_hw6_rtos_probe.runtime_complete != 0UL))
    {
      if ((g_ps_ui_router_probe.current_page ==
           PS_UI_ROUTER_PAGE_CALIBRATION) &&
          (g_ps_ui_router_probe.calibration_page ==
           PS_UI_ROUTER_CAL_JOYSTICK_REVIEW) &&
          (g_ps_hw6_owner_sm_probe.joystick_calibration_session_active !=
           0UL))
      {
        if ((ps_joystick_calibration_review_next_tick == 0UL) ||
            ((int32_t)((uint32_t)now -
             ps_joystick_calibration_review_next_tick) >= 0))
        {
          uint32_t review_period_ticks = PS_HW6_RTOS_MsToTicks(
            (uint32_t)KNOB_INPUT_JOYSTICK_CAL_REVIEW_PERIOD_MS);
          if (review_period_ticks == 0UL)
          {
            review_period_ticks = 1UL;
          }
          (void)
            PS_HW6_OwnerStateMachines_RunJoystickCalibrationReviewSample();
          ps_joystick_calibration_review_next_tick =
            (uint32_t)now + review_period_ticks;
          PS_HW6_RTOS_SendCurrentUiRenderCommand();
        }
      }
      else
      {
        ps_joystick_calibration_review_next_tick = 0UL;
        if ((g_ps_hw6_owner_sm_probe.joystick_calibration_session_active !=
             0UL) &&
            ((g_ps_ui_router_probe.current_page !=
              PS_UI_ROUTER_PAGE_CALIBRATION) ||
             (g_ps_ui_router_probe.calibration_page ==
              PS_UI_ROUTER_CAL_INPUT_ROOT) ||
             (g_ps_ui_router_probe.calibration_page ==
              PS_UI_ROUTER_CAL_NONE)))
        {
          PS_HW6_OwnerStateMachines_CancelJoystickCalibration();
        }
      }
      PS_HW6_RTOS_RunJoystickAwakeInput((uint32_t)now);
      if (ps_queues[PS_HW6_RTOS_OWNER_INPUT].tx_queue_enqueued == 0U)
      {
        PS_InputButtons_ReconcileLiveLevels((uint32_t)now);
      }
      if (PS_InputButtons_StartCheckDue((uint32_t)now) != 0UL)
      {
        PS_InputButtons_PollStart((uint32_t)now);
      }
      if (PS_InputButtons_ButtonsCheckDue((uint32_t)now) != 0UL)
      {
        PS_InputButtons_PollButtons((uint32_t)now);
      }
      for (start_power_drain_count = 0UL;
           start_power_drain_count < 4UL;
           ++start_power_drain_count)
      {
        if (PS_InputButtons_TakeStartPowerEvent(&start_power_event,
                                               &start_power_timestamp,
                                               &start_power_hold_ticks) == 0UL)
        {
          break;
        }
        (void)start_power_timestamp;
        (void)PS_HW6_RTOS_SendPowerStartEvent(start_power_event,
                                             start_power_hold_ticks);
      }
      if (PS_InputButtons_TakeStartLongPress(&start_long_timestamp,
                                             &start_long_hold_ticks) != 0UL)
      {
        (void)start_long_hold_ticks;
        g_ps_hw6_rtos_probe.runtime_interaction_manual_request_count++;
        g_ps_hw6_rtos_probe.runtime_interaction_manual_request_tick =
          start_long_timestamp;
        if (ps_runtime_interaction_state ==
            PS_HW6_RUNTIME_INTERACTION_STATE_ACTIVE)
        {
          g_ps_hw6_rtos_probe.runtime_interaction_manual_request_status =
            (uint32_t)PS_HW6_RTOS_RequestRuntimeCommand(
              PS_HW6_RTOS_COMMAND_RUNTIME_INTERACTION_INACTIVE);
        }
        else
        {
          g_ps_hw6_rtos_probe.runtime_interaction_manual_request_status =
            (uint32_t)TX_NOT_AVAILABLE;
        }
      }
      if (PS_InputButtons_TakeStartPress(&start_press_timestamp,
                                         &start_press_hold_ticks) != 0UL)
      {
        if (ps_runtime_interaction_state ==
            PS_HW6_RUNTIME_INTERACTION_STATE_INACTIVE)
        {
          (void)start_press_timestamp;
          (void)PS_HW6_RTOS_RequestRuntimeCommand(
            PS_HW6_RTOS_COMMAND_RUNTIME_INTERACTION_START);
        }
        else if (ps_runtime_interaction_state ==
                 PS_HW6_RUNTIME_INTERACTION_STATE_ACTIVE)
        {
          button_record.event = PS_INPUT_BUTTON_LOGICAL_EVENT_PRESS;
          button_record.button_id = PS_INPUT_BUTTON_ID_START;
          button_record.button_mask = (1UL << 4U);
          button_record.timestamp = start_press_timestamp;
          button_record.hold_ticks = start_press_hold_ticks;
          g_ps_hw6_rtos_probe.runtime_interaction_package_start_count++;
          g_ps_hw6_rtos_probe.runtime_interaction_package_start_status =
            (uint32_t)PS_HW6_RTOS_DeliverInputLogicalEvent(&button_record);
        }
      }
      for (button_drain_count = 0UL;
           button_drain_count < 16UL;
           ++button_drain_count)
      {
        if (PS_InputButtons_TakeLogicalEvent(&button_record) == 0UL)
        {
          break;
        }
        (void)PS_HW6_RTOS_DeliverInputLogicalEvent(&button_record);
      }
    }
    if ((owner_id == PS_HW6_RTOS_OWNER_UI) &&
        (ps_ui_boot_complete_sent == 0UL) &&
        (ps_power_boot_done != 0UL) &&
        (g_ps_hw6_owner_sm_probe
           .joystick_calibration_persistent_boot_resolved != 0UL) &&
        (g_ps_hw6_rtos_probe.runtime_complete != 0UL))
    {
      (void)PS_HW6_RTOS_RequestUiClockCapabilities(
        PS_HW6_RTOS_UI_CLOCK_REASON_REACTIVE_TRANSACTION,
        PS_HW6_RTOS_UI_CLOCK_REACTIVE_CAPABILITIES);
      if ((g_ps_hw6_owner_sm_probe.battery_policy_state ==
           PS_HW6_POWER_BATTERY_POLICY_UNKNOWN) &&
          (g_ps_hw6_owner_sm_probe.battery_policy_boot_restart_gate_pending !=
           0UL))
      {
        g_ps_hw6_rtos_probe.boot_home_suppressed = 1UL;
      }
      else
      {
        ps_ui_boot_complete_sent = 1UL;
        if (g_ps_hw6_owner_sm_probe.battery_policy_state ==
            PS_HW6_POWER_BATTERY_POLICY_BOOT_RESTART_BLOCKED)
        {
          g_ps_hw6_rtos_probe.boot_home_suppressed = 1UL;
          router_status = PS_UIRouter_Dispatch(
            PS_UI_ROUTER_EVENT_LOW_BATTERY_BOOT_BLOCK);
          if (router_status == PS_STATUS_OK)
          {
            g_ps_hw6_rtos_probe.boot_low_battery_ui_sent = 1UL;
            PS_HW6_RTOS_SendCurrentUiRenderCommand();
          }
        }
        else if (g_ps_hw6_owner_sm_probe.battery_policy_state ==
                 PS_HW6_POWER_BATTERY_POLICY_BOOT_CHARGE_RECOVERY)
        {
          g_ps_hw6_rtos_probe.boot_home_suppressed = 1UL;
          router_status = PS_UIRouter_Dispatch(
            PS_UI_ROUTER_EVENT_LOW_BATTERY_CHARGE_RECOVERY);
          if (router_status == PS_STATUS_OK)
          {
            g_ps_hw6_rtos_probe.boot_low_battery_ui_sent = 1UL;
            PS_HW6_RTOS_SendCurrentUiRenderCommand();
          }
        }
        else
        {
          if (PS_HW6_RTOS_BeginBootHome() != 0UL)
          {
            PS_HW6_RTOS_SendCurrentUiRenderCommand();
          }
        }
      }
      (void)PS_HW6_RTOS_RequestUiClockCapabilities(
        PS_HW6_RTOS_UI_CLOCK_REASON_RELEASE,
        0UL);
    }
    if ((owner_id == PS_HW6_RTOS_OWNER_UI) &&
        (g_ps_ui_router_request != 0UL) &&
        (g_ps_hw6_rtos_probe.runtime_complete != 0UL))
    {
      (void)PS_HW6_RTOS_RequestUiClockCapabilities(
        PS_HW6_RTOS_UI_CLOCK_REASON_REACTIVE_TRANSACTION,
        PS_HW6_RTOS_UI_CLOCK_REACTIVE_CAPABILITIES);
      router_event = g_ps_ui_router_request_event;
      g_ps_ui_router_request = 0UL;
      send_ui_render = 1UL;
      router_status = PS_UIRouter_Dispatch(router_event);
      if (router_status == PS_STATUS_OK)
      {
        if ((router_event == PS_UI_ROUTER_EVENT_LOW_BATTERY_BOOT_BLOCK) ||
            (router_event == PS_UI_ROUTER_EVENT_LOW_BATTERY_CHARGE_RECOVERY))
        {
          g_ps_hw6_rtos_probe.boot_low_battery_ui_sent = 1UL;
        }
        else if (router_event == PS_UI_ROUTER_EVENT_RECOVER_OK)
        {
          g_ps_hw6_rtos_probe.boot_low_battery_recover_ui_sent = 1UL;
          send_ui_render = PS_HW6_RTOS_BeginBootHome();
        }
        else if ((router_event ==
                  PS_UI_ROUTER_EVENT_CAL_JOYSTICK_REVIEW_ACCEPT) &&
                 (ps_boot_package_launch_after_calibration != 0UL))
        {
          ps_boot_package_launch_after_calibration = 0UL;
          if (PS_HW6_RTOS_RequestRuntimeCommand(
                PS_HW6_RTOS_COMMAND_RUNTIME_PACKAGE_REACTIVE_STUB) ==
              TX_SUCCESS)
          {
            send_ui_render = 0UL;
          }
          else
          {
            (void)PS_UIRouter_Dispatch(
              PS_UI_ROUTER_EVENT_RUNTIME_UNAVAILABLE);
          }
        }
        if (send_ui_render != 0UL)
        {
          PS_HW6_RTOS_SendCurrentUiRenderCommand();
        }
      }
      (void)PS_HW6_RTOS_RequestUiClockCapabilities(
        PS_HW6_RTOS_UI_CLOCK_REASON_RELEASE,
        0UL);
    }
    if ((owner_id == PS_HW6_RTOS_OWNER_STORAGE) &&
        (ps_joystick_calibration_boot_load_started == 0UL) &&
        (ps_power_boot_done != 0UL) &&
        (g_ps_hw6_rtos_probe.runtime_complete != 0UL))
    {
      ps_joystick_calibration_boot_load_started = 1UL;
      PS_HW6_RTOS_RunStorageJoystickCalibrationLoadRequest();
    }
    if ((owner_id == PS_HW6_RTOS_OWNER_STORAGE) &&
        (g_ps_hw6_storage_usb_export_request != 0UL) &&
        (g_ps_hw6_rtos_probe.runtime_complete != 0UL))
    {
      g_ps_hw6_storage_usb_export_request = 0UL;
      PS_HW6_RTOS_RunStorageUsbExportRequest();
    }
    if ((owner_id == PS_HW6_RTOS_OWNER_STORAGE) &&
        (g_ps_hw6_storage_usb_reclaim_request != 0UL) &&
        (g_ps_hw6_rtos_probe.runtime_complete != 0UL))
    {
      g_ps_hw6_storage_usb_reclaim_request = 0UL;
      PS_HW6_RTOS_RunStorageUsbReclaimRequest();
    }
    if ((owner_id == PS_HW6_RTOS_OWNER_STORAGE) &&
        (g_ps_hw6_storage_persistent_install_request != 0UL) &&
        (g_ps_hw6_rtos_probe.runtime_complete != 0UL))
    {
      g_ps_hw6_storage_persistent_install_request = 0UL;
      PS_HW6_RTOS_RunStorageEmbeddedPackageInstallRequest();
    }
  }
}

UINT PS_HW6_RTOS_Init(TX_BYTE_POOL *pool)
{
  ULONG message[PS_HW6_RTOS_MESSAGE_WORDS];
  ULONG actual_flags;
  UINT status;
  uint32_t i;

  if (pool == TX_NULL)
  {
    return TX_PTR_ERROR;
  }

  PS_HW6_RTOS_ResetProbe();
  PS_HW6_RTOS_PrimeDebugCommandAnchors();
  HAL_GPIO_WritePin(PWR_DBG_GPIO_Port, PWR_DBG_Pin, GPIO_PIN_RESET);
  (void)PS_HW6_OwnerServices_Init();
  PS_HW6_OwnerStateMachines_Init();
  PS_HW6_OwnerStateMachines_SetPowerQuiesceCallback(
    PS_HW6_RTOS_RunPowerQuiesceBarrier);
  PS_HW6_OwnerStateMachines_SetPowerAdmissionCallback(
    PS_HW6_RTOS_RequestPowerSystemAdmission);
  PS_HW6_OwnerStateMachines_SetPostStopResumeCallback(
    PS_HW6_RTOS_RunPostStopResumeBarrier);
  PS_UIRouter_Init();
  PS_InputButtons_Init();
  PS_Main_PmicIntExtiArm();

  status = PS_HW6_RTOS_SnapshotPool(
    pool,
    (uint32_t *)&g_ps_hw6_rtos_probe.pool_available_before,
    (uint32_t *)&g_ps_hw6_rtos_probe.pool_fragments_before);
  g_ps_hw6_rtos_probe.pool_info_before_status = status;
  PS_HW6_RTOS_RecordFirstError(status, PS_HW6_RTOS_STEP_POOL_INFO, 0U);

  for (i = 0U; i < PS_HW6_RTOS_OWNER_COUNT; ++i)
  {
    status = tx_byte_allocate(pool, &ps_thread_stacks[i],
                              ps_owner_stack_bytes[i], TX_NO_WAIT);
    g_ps_hw6_rtos_probe.stack_alloc_status[i] = status;
    PS_HW6_RTOS_RecordFirstError(status, PS_HW6_RTOS_STEP_STACK_ALLOC, i);
  }

  for (i = 0U; i < PS_HW6_RTOS_QUEUE_COUNT; ++i)
  {
    status = tx_byte_allocate(pool, &ps_queue_storage[i],
                              PS_HW6_RTOS_QUEUE_STORAGE_BYTES, TX_NO_WAIT);
    g_ps_hw6_rtos_probe.queue_alloc_status[i] = status;
    PS_HW6_RTOS_RecordFirstError(status, PS_HW6_RTOS_STEP_QUEUE_ALLOC, i);
  }
  g_ps_hw6_rtos_probe.phase = PS_HW6_RTOS_PHASE_ALLOCATED;

  for (i = 0U; i < PS_HW6_RTOS_QUEUE_COUNT; ++i)
  {
    if (ps_queue_storage[i] != TX_NULL)
    {
      status = tx_queue_create(&ps_queues[i], ps_queue_names[i],
                               PS_HW6_RTOS_MESSAGE_WORDS,
                               ps_queue_storage[i],
                               PS_HW6_RTOS_QUEUE_STORAGE_BYTES);
    }
    else
    {
      status = TX_NO_MEMORY;
    }
    g_ps_hw6_rtos_probe.queue_create_status[i] = status;
    PS_HW6_RTOS_RecordFirstError(status, PS_HW6_RTOS_STEP_QUEUE_CREATE, i);
  }

  status = PS_StorageMscBridge_Init(&ps_queues[PS_HW6_RTOS_OWNER_STORAGE]);
  PS_HW6_RTOS_RecordFirstError(status, PS_HW6_RTOS_STEP_QUEUE_CREATE,
                               PS_HW6_RTOS_OWNER_STORAGE);
  for (i = 0U; i < PS_HW6_RTOS_EVENT_GROUP_COUNT; ++i)
  {
    status = tx_event_flags_create(&ps_event_groups[i], ps_event_names[i]);
    g_ps_hw6_rtos_probe.event_create_status[i] = status;
    PS_HW6_RTOS_RecordFirstError(status, PS_HW6_RTOS_STEP_EVENT_CREATE, i);

    if (status == TX_SUCCESS)
    {
      status = tx_event_flags_set(&ps_event_groups[i], 1UL, TX_OR);
      g_ps_hw6_rtos_probe.event_set_status[i] = status;
      PS_HW6_RTOS_RecordFirstError(status, PS_HW6_RTOS_STEP_EVENT_TEST, i);

      actual_flags = 0UL;
      if (status == TX_SUCCESS)
      {
        status = tx_event_flags_get(&ps_event_groups[i], 1UL,
                                    TX_AND_CLEAR, &actual_flags, TX_NO_WAIT);
      }
      g_ps_hw6_rtos_probe.event_get_status[i] = status;
      PS_HW6_RTOS_RecordFirstError(status, PS_HW6_RTOS_STEP_EVENT_TEST, i);
      if ((status == TX_SUCCESS) && (actual_flags == 1UL))
      {
        g_ps_hw6_rtos_probe.event_selftest_mask |= (1UL << i);
      }
    }
  }

  for (i = 0U; i < PS_HW6_RTOS_QUEUE_COUNT; ++i)
  {
    message[0] = PS_HW6_RTOS_STARTUP_MAGIC;
    message[1] = (ULONG)i;
    message[2] = PS_HW6_RTOS_STARTUP_KIND;
    message[3] = ~((ULONG)i);

    if (g_ps_hw6_rtos_probe.queue_create_status[i] == TX_SUCCESS)
    {
      status = tx_queue_send(&ps_queues[i], message, TX_NO_WAIT);
    }
    else
    {
      status = TX_QUEUE_ERROR;
    }
    g_ps_hw6_rtos_probe.queue_selftest_send_status[i] = status;
    PS_HW6_RTOS_RecordFirstError(status, PS_HW6_RTOS_STEP_QUEUE_TEST, i);
  }
  if (g_ps_hw6_rtos_probe.queue_create_status[PS_HW6_RTOS_OWNER_INPUT] ==
      TX_SUCCESS)
  {
    PS_InputButtons_SetRawEdgeSink(PS_HW6_RTOS_QueueInputRawEdge);
  }
  g_ps_hw6_rtos_probe.phase = PS_HW6_RTOS_PHASE_OBJECTS_CREATED;

  for (i = 0U; i < PS_HW6_RTOS_OWNER_COUNT; ++i)
  {
    if ((ps_thread_stacks[i] != TX_NULL) &&
        (g_ps_hw6_rtos_probe.queue_create_status[i] == TX_SUCCESS))
    {
      status = tx_thread_create(&ps_threads[i], ps_owner_names[i],
                                PS_HW6_RTOS_OwnerEntry, (ULONG)i,
                                ps_thread_stacks[i], ps_owner_stack_bytes[i],
                                ps_owner_priorities[i], ps_owner_priorities[i],
                                TX_NO_TIME_SLICE, TX_AUTO_START);
    }
    else
    {
      status = TX_NO_MEMORY;
    }
    g_ps_hw6_rtos_probe.thread_create_status[i] = status;
    PS_HW6_RTOS_RecordThreadStackProbe(i);
    PS_HW6_RTOS_RecordFirstError(status, PS_HW6_RTOS_STEP_THREAD_CREATE, i);
  }

  status = PS_HW6_RTOS_SnapshotPool(
    pool,
    (uint32_t *)&g_ps_hw6_rtos_probe.pool_available_after,
    (uint32_t *)&g_ps_hw6_rtos_probe.pool_fragments_after);
  g_ps_hw6_rtos_probe.pool_info_after_status = status;
  PS_HW6_RTOS_RecordFirstError(status, PS_HW6_RTOS_STEP_POOL_INFO, 1U);

  if (g_ps_hw6_rtos_probe.init_status == TX_SUCCESS)
  {
    g_ps_hw6_rtos_probe.init_complete = 1UL;
    g_ps_hw6_rtos_probe.phase = PS_HW6_RTOS_PHASE_READY;
  }

  return (UINT)g_ps_hw6_rtos_probe.init_status;
}

void PS_HW6_RTOS_LowPowerTimerSetup(ULONG count)
{
  g_ps_hw6_rtos_probe.low_power_setup_count++;
  g_ps_hw6_rtos_probe.low_power_next_ticks = (uint32_t)count;
}

void PS_HW6_RTOS_LowPowerEnter(void)
{
  g_ps_hw6_rtos_probe.low_power_enter_count++;
  if (g_ps_storage_msc_bridge_probe.export_enabled != 0UL)
  {
    g_ps_hw6_rtos_low_power_usb_skip_count++;
    return;
  }

  __DSB();
  __WFI();
  __ISB();
}

void PS_HW6_RTOS_LowPowerExit(void)
{
  g_ps_hw6_rtos_probe.low_power_exit_count++;
}

ULONG PS_HW6_RTOS_LowPowerTimerAdjust(void)
{
  g_ps_hw6_rtos_probe.low_power_adjust_count++;
  return 0UL;
}
