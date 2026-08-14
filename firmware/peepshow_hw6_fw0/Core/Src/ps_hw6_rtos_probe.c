#include "ps_hw6_rtos_probe.h"

#include <string.h>

#include "knobs_autogen.h"
#include "main.h"
#include "ps_hw6_clock_policy.h"
#include "ps_hw6_owner_services.h"
#include "ps_hw6_owner_state_machines.h"
#include "ps_hw6_trace.h"
#include "ps_input_buttons.h"
#include "ps_power_state.h"
#include "ps_storage_filex_levelx.h"
#include "ps_storage_msc_bridge.h"
#include "ps_ui_router.h"

#define PS_HW6_RTOS_DEFAULT_STACK_BYTES  ((ULONG)KNOB_RTOS_DEFAULT_STACK_BYTES)
#define PS_HW6_RTOS_POWER_STACK_BYTES    ((ULONG)KNOB_RTOS_POWER_STACK_BYTES)
#define PS_HW6_RTOS_STORAGE_STACK_BYTES  ((ULONG)KNOB_RTOS_STORAGE_STACK_BYTES)
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
#define PS_HW6_RTOS_RUNTIME_INPUT_MAGIC   (0x52494221UL)
#define PS_HW6_RTOS_POWER_INPUT_MAGIC     (0x50574921UL)
#define PS_HW6_RTOS_UI_INPUT_PRESS        (1UL)
#define PS_HW6_RTOS_RUNTIME_INPUT_BUTTON_ID_MASK (0xFFUL)
#define PS_HW6_RTOS_RUNTIME_INPUT_BUTTON_MASK_SHIFT (8U)
#define PS_HW6_RTOS_RUNTIME_INPUT_BUTTON_MASK_MASK (0xFFUL)
#define PS_HW6_RTOS_DISPLAY_UI_FIELD_MASK  (0xFFUL)
#define PS_HW6_RTOS_DISPLAY_UI_CAL_SHIFT   (0U)
#define PS_HW6_RTOS_DISPLAY_UI_FOCUS_SHIFT (8U)
#define PS_HW6_RTOS_DISPLAY_UI_SHUTDOWN_SHIFT (16U)
#define PS_HW6_RTOS_DISPLAY_UI_COUNTDOWN_SHIFT (24U)
#define PS_HW6_RTOS_DISPLAY_UI_SHUTDOWN_MAX \
  ((uint32_t)PS_UI_ROUTER_SHUTDOWN_MSC_RECOVERY)
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

#define PS_HW6_RTOS_INPUT_POLICY_API_VERSION (1UL)
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
#define PS_HW6_RTOS_STOP2_PENDING_OWNER_QUIESCE       (1UL << 0)
#define PS_HW6_RTOS_STOP2_PENDING_LPBAM_VALIDATION    (1UL << 1)
#define PS_HW6_RTOS_STOP2_PENDING_IDLE_WINDOW         (1UL << 2)

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
volatile uint32_t g_ps_hw6_runtime_realtime_stub_request;
volatile uint32_t g_ps_hw6_runtime_return_request;
volatile uint32_t g_ps_hw6_runtime_suspend_request;
volatile uint32_t g_ps_hw6_runtime_resume_request;
volatile uint32_t g_ps_hw6_admission_msc_enter_dry_run_request;
volatile uint32_t g_ps_hw6_admission_power_shutdown_dry_run_request;
volatile uint32_t g_ps_hw6_admission_power_battery_dry_run_request;
volatile uint32_t g_ps_hw6_admission_power_cancel_dry_run_request;
volatile uint32_t g_ps_hw6_power_stop2_auto_idle_dry_run_request;

typedef UINT (*PS_HW6_RTOS_DebugCommandFn)(void);

static TX_THREAD ps_threads[PS_HW6_RTOS_OWNER_COUNT];
static TX_QUEUE ps_queues[PS_HW6_RTOS_QUEUE_COUNT];
static TX_EVENT_FLAGS_GROUP ps_event_groups[PS_HW6_RTOS_EVENT_GROUP_COUNT];
static VOID *ps_thread_stacks[PS_HW6_RTOS_OWNER_COUNT];
static VOID *ps_queue_storage[PS_HW6_RTOS_QUEUE_COUNT];
static volatile PS_HW6_RTOS_DebugCommandFn ps_debug_usb_export_anchor;
static volatile PS_HW6_RTOS_DebugCommandFn ps_debug_usb_reclaim_anchor;
static volatile PS_HW6_RTOS_DebugCommandFn ps_debug_storage_flash_init_anchor;
static uint32_t ps_ui_boot_complete_sent;
static uint32_t ps_power_boot_done;
static uint32_t ps_display_bootstrap_sent;
static volatile uint32_t ps_pmic_int_pending_count;
static volatile uint32_t ps_pmic_int_irq_count;
static volatile uint32_t ps_pmic_int_last_pin;
static volatile uint32_t ps_pmic_int_last_level;
static volatile uint32_t ps_pmic_int_last_irq_tick;
static uint32_t ps_pmic_int_consumed_count;

static void PS_HW6_RTOS_SendCurrentUiRenderCommand(void);

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
  PS_HW6_RTOS_DEFAULT_STACK_BYTES,
  PS_HW6_RTOS_DEFAULT_STACK_BYTES,
  PS_HW6_RTOS_DEFAULT_STACK_BYTES,
  PS_HW6_RTOS_DEFAULT_STACK_BYTES,
  PS_HW6_RTOS_STORAGE_STACK_BYTES,
  PS_HW6_RTOS_DEFAULT_STACK_BYTES,
  PS_HW6_RTOS_DEFAULT_STACK_BYTES,
  PS_HW6_RTOS_DEFAULT_STACK_BYTES
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
  ps_power_boot_done = 0UL;
  ps_display_bootstrap_sent = 0UL;
  ps_pmic_int_pending_count = 0UL;
  ps_pmic_int_irq_count = 0UL;
  ps_pmic_int_last_pin = 0UL;
  ps_pmic_int_last_level = 0UL;
  ps_pmic_int_last_irq_tick = 0UL;
  ps_pmic_int_consumed_count = 0UL;
  g_ps_hw6_audio_clock_probe_request = 0UL;
  g_ps_hw6_audio_clock_probe_release_request = 0UL;
  g_ps_hw6_runtime_reactive_stub_request = 0UL;
  g_ps_hw6_runtime_realtime_stub_request = 0UL;
  g_ps_hw6_runtime_return_request = 0UL;
  g_ps_hw6_runtime_suspend_request = 0UL;
  g_ps_hw6_runtime_resume_request = 0UL;
  g_ps_hw6_admission_msc_enter_dry_run_request = 0UL;
  g_ps_hw6_admission_power_shutdown_dry_run_request = 0UL;
  g_ps_hw6_admission_power_battery_dry_run_request = 0UL;
  g_ps_hw6_admission_power_cancel_dry_run_request = 0UL;
  g_ps_hw6_power_stop2_auto_idle_dry_run_request = 0UL;
  g_ps_hw6_rtos_probe.magic = PS_HW6_RTOS_PROBE_MAGIC;
  g_ps_hw6_rtos_probe.version = PS_HW6_RTOS_PROBE_VERSION;
  g_ps_hw6_rtos_probe.phase = PS_HW6_RTOS_PHASE_INIT;
  g_ps_hw6_rtos_probe.input_policy_api_version =
    PS_HW6_RTOS_INPUT_POLICY_API_VERSION;
  g_ps_hw6_rtos_probe.input_policy_last_status =
    PS_HW6_RTOS_STATUS_NOT_RUN;
  g_ps_hw6_rtos_probe.runtime_input_last_status =
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
  g_ps_hw6_rtos_probe.audio_clock_last_status =
    PS_HW6_RTOS_STATUS_NOT_RUN;
  g_ps_hw6_rtos_probe.audio_clock_reactive_sfx_status =
    PS_HW6_RTOS_STATUS_NOT_RUN;
  g_ps_hw6_rtos_probe.audio_clock_realtime_status =
    PS_HW6_RTOS_STATUS_NOT_RUN;
  g_ps_hw6_rtos_probe.audio_clock_release_status =
    PS_HW6_RTOS_STATUS_NOT_RUN;
  g_ps_hw6_rtos_probe.storage_clock_last_status =
    PS_HW6_RTOS_STATUS_NOT_RUN;
  g_ps_hw6_rtos_probe.storage_clock_export_status =
    PS_HW6_RTOS_STATUS_NOT_RUN;
  g_ps_hw6_rtos_probe.storage_clock_reclaim_status =
    PS_HW6_RTOS_STATUS_NOT_RUN;
  g_ps_hw6_rtos_probe.storage_clock_flash_init_status =
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
       (message[2] == PS_HW6_RTOS_COMMAND_PACKAGE_INSTALL_STUB)) &&
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
       (message[2] == PS_HW6_RTOS_COMMAND_RUNTIME_RESUME)) &&
      (message[3] == PS_HW6_RTOS_COMMAND_TOKEN))
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
                       (uint32_t)PS_UI_ROUTER_PACKAGE_ERROR : 2UL;

  if ((owner_id != PS_HW6_RTOS_OWNER_DISPLAY) ||
      (message[0] != PS_HW6_RTOS_DISPLAY_UI_MAGIC) ||
      (message[1] != PS_HW6_RTOS_OWNER_DISPLAY) ||
      (page > PS_UI_ROUTER_PAGE_SHUTDOWN) ||
      (PS_HW6_RTOS_DisplayUiPackedCalibration(message[3]) >
       PS_UI_ROUTER_CAL_JOYSTICK_REVIEW) ||
      (focus > focus_max) ||
      (PS_HW6_RTOS_DisplayUiPackedShutdown(message[3]) >
       PS_HW6_RTOS_DISPLAY_UI_SHUTDOWN_MAX) ||
      (PS_HW6_RTOS_DisplayUiPackedCountdown(message[3]) > 9UL))
  {
    return 0UL;
  }

  return 1UL;
}

static uint32_t PS_HW6_RTOS_UiInputCommandIsValid(uint32_t owner_id,
                                                  const ULONG *message)
{
  if ((owner_id != PS_HW6_RTOS_OWNER_UI) ||
      (message[0] != PS_HW6_RTOS_UI_INPUT_MAGIC) ||
      (message[1] != PS_HW6_RTOS_OWNER_UI) ||
      (message[2] != PS_HW6_RTOS_UI_INPUT_PRESS) ||
      (message[3] < PS_INPUT_BUTTON_ID_A) ||
      (message[3] > PS_INPUT_BUTTON_ID_R))
  {
    return 0UL;
  }

  return 1UL;
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

  if ((event != (uint32_t)PS_INPUT_BUTTON_LOGICAL_EVENT_PRESS) ||
      (button_id < (uint32_t)PS_INPUT_BUTTON_ID_A) ||
      (button_id > (uint32_t)PS_INPUT_BUTTON_ID_R) ||
      (button_mask == 0UL) ||
      ((button_mask & ~0x0FUL) != 0UL))
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

  event = (uint32_t)message[2];
  packed_button = (uint32_t)message[3];
  button_id = packed_button & PS_HW6_RTOS_RUNTIME_INPUT_BUTTON_ID_MASK;
  button_mask = (packed_button >> PS_HW6_RTOS_RUNTIME_INPUT_BUTTON_MASK_SHIFT) &
                PS_HW6_RTOS_RUNTIME_INPUT_BUTTON_MASK_MASK;

  g_ps_hw6_rtos_probe.runtime_input_event_count++;
  if (event == (uint32_t)PS_INPUT_BUTTON_LOGICAL_EVENT_PRESS)
  {
    g_ps_hw6_rtos_probe.runtime_input_button_count++;
  }
  g_ps_hw6_rtos_probe.runtime_input_last_event = event;
  g_ps_hw6_rtos_probe.runtime_input_last_button_id = button_id;
  g_ps_hw6_rtos_probe.runtime_input_last_mask = button_mask;
  g_ps_hw6_rtos_probe.runtime_input_last_status = TX_SUCCESS;
  g_ps_hw6_rtos_probe.runtime_input_last_tick = (uint32_t)tx_time_get();
  PS_HW6_TraceInputButton(button_id,
                          PS_HW6_RTOS_OWNER_RUNTIME,
                          TX_SUCCESS,
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

static uint32_t PS_HW6_RTOS_RouterEventForButton(uint32_t button_id)
{
  if (button_id == PS_INPUT_BUTTON_ID_A)
  {
    return PS_UI_ROUTER_EVENT_INPUT_BTN_A;
  }
  if (button_id == PS_INPUT_BUTTON_ID_B)
  {
    return PS_UI_ROUTER_EVENT_INPUT_BTN_B;
  }
  if (button_id == PS_INPUT_BUTTON_ID_L)
  {
    return PS_UI_ROUTER_EVENT_INPUT_BTN_L;
  }
  if (button_id == PS_INPUT_BUTTON_ID_R)
  {
    return PS_UI_ROUTER_EVENT_INPUT_BTN_R;
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
  if (calibration_page == PS_UI_ROUTER_CAL_JOYSTICK_RIGHT)
  {
    return PS_UI_ROUTER_EVENT_CAL_JOYSTICK_RIGHT_ACCEPT;
  }
  if (calibration_page == PS_UI_ROUTER_CAL_JOYSTICK_CIRCLE)
  {
    return PS_UI_ROUTER_EVENT_CAL_JOYSTICK_CIRCLE_ACCEPT;
  }
  return 0UL;
}

static uint32_t PS_HW6_RTOS_RequestJoystickCalibrationCapture(
  uint32_t button_id)
{
  uint32_t calibration_page = g_ps_ui_router_probe.calibration_page;

  if ((button_id == PS_INPUT_BUTTON_ID_A) &&
      (g_ps_ui_router_probe.current_page == PS_UI_ROUTER_PAGE_CALIBRATION) &&
      (PS_HW6_RTOS_RouterEventForCalibrationCapture(calibration_page) != 0UL))
  {
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

UINT PS_HW6_RTOS_DebugRequestStorageFlashInit(void)
{
  return PS_HW6_RTOS_SendCommand(
    PS_HW6_RTOS_OWNER_STORAGE,
    PS_HW6_RTOS_COMMAND_STORAGE_FLASH_INIT);
}

static void PS_HW6_RTOS_PrimeDebugCommandAnchors(void)
{
  ps_debug_usb_export_anchor = PS_HW6_RTOS_DebugRequestUsbExport;
  ps_debug_usb_reclaim_anchor = PS_HW6_RTOS_DebugRequestUsbReclaim;
  ps_debug_storage_flash_init_anchor =
    PS_HW6_RTOS_DebugRequestStorageFlashInit;
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

static UINT PS_HW6_RTOS_RequestStorageClockCapabilities(
  uint32_t reason,
  uint32_t capabilities)
{
  UINT status;

  status = PS_HW6_RTOS_RequestPowerClockProfile(
    PS_HW6_RTOS_OWNER_STORAGE,
    (uint32_t)PS_HW6_CLOCK_PROFILE_UNKNOWN,
    capabilities);

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
  }

  return status;
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

  if (blocker_mask == 0UL)
  {
    pending_mask |= PS_HW6_RTOS_STOP2_PENDING_OWNER_QUIESCE;
    if (g_ps_hw6_clock_policy_probe.lpbam_stop2_ready == 0UL)
    {
      pending_mask |= PS_HW6_RTOS_STOP2_PENDING_LPBAM_VALIDATION;
    }
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
    g_ps_hw6_clock_policy_probe.lpbam_stop2_ready;
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
          (page == (uint32_t)PS_UI_ROUTER_PAGE_PACKAGE_BROWSER)) ?
         1UL : 0UL;
}

static uint32_t PS_HW6_RTOS_Stop2AutoDisplayAllowsIdle(void)
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

static uint32_t PS_HW6_RTOS_Stop2AutoStorageAllowsIdle(void)
{
  if ((g_ps_hw6_storage_usb_export_request != 0UL) ||
      (g_ps_hw6_storage_usb_reclaim_request != 0UL) ||
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
  if ((g_ps_input_buttons_probe.pending_mask != 0UL) ||
      (g_ps_input_buttons_probe.start_active != 0UL) ||
      (g_ps_input_buttons_probe.start_pending_event != 0UL) ||
      (g_ps_input_buttons_probe.logical_event_count !=
       g_ps_hw6_rtos_probe.input_policy_event_count))
  {
    return 0UL;
  }

  return 1UL;
}

static HAL_StatusTypeDef PS_HW6_RTOS_RunStop2AutoIdleCheck(
  uint32_t now_tick,
  uint32_t allow_entry)
{
  HAL_StatusTypeDef eligibility_status;
  HAL_StatusTypeDef entry_status = HAL_OK;
  uint32_t blocker_mask = 0UL;
  uint32_t pending_mask = 0UL;
  uint32_t queue_pending_mask;
  uint32_t required_idle_ticks =
    PS_HW6_RTOS_MsToTicks((uint32_t)KNOB_POWER_AUTO_STOP2_IDLE_MS);
  uint32_t idle_ticks = 0UL;
  uint32_t ready = 0UL;

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

  if ((KNOB_POWER_AUTO_STOP2_ENABLE == 0) && (allow_entry != 0UL))
  {
    blocker_mask |= PS_HW6_RTOS_STOP2_BLOCK_AUTO_DISABLED;
  }

  eligibility_status = PS_HW6_RTOS_RunStop2EligibilityDryRun();
  g_ps_hw6_rtos_probe.stop2_auto_eligibility_status =
    (uint32_t)eligibility_status;
  blocker_mask |= g_ps_hw6_rtos_probe.stop2_eligibility_blocker_mask;
  pending_mask |= g_ps_hw6_rtos_probe.stop2_eligibility_pending_mask;

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

  queue_pending_mask = PS_HW6_RTOS_Stop2AutoQueuePendingMask();
  if (queue_pending_mask != 0UL)
  {
    blocker_mask |= PS_HW6_RTOS_STOP2_BLOCK_QUEUE_PENDING;
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
    g_ps_hw6_rtos_probe.stop2_auto_skip_count++;
    g_ps_hw6_rtos_probe.stop2_auto_last_status = (uint32_t)HAL_ERROR;
    return HAL_ERROR;
  }

  if (allow_entry == 0UL)
  {
    g_ps_hw6_rtos_probe.stop2_auto_last_status = (uint32_t)HAL_OK;
    return HAL_OK;
  }

  g_ps_hw6_rtos_probe.stop2_auto_entry_count++;
  entry_status = PS_HW6_RTOS_RunStop2ControlledEntry();
  g_ps_hw6_rtos_probe.stop2_auto_entry_status = (uint32_t)entry_status;
  g_ps_hw6_rtos_probe.stop2_auto_last_status = (uint32_t)entry_status;
  return entry_status;
}

static void PS_HW6_RTOS_RunStop2AutoIdlePeriodic(uint32_t now_tick)
{
  uint32_t period_ticks =
    PS_HW6_RTOS_MsToTicks((uint32_t)KNOB_POWER_AUTO_STOP2_CHECK_PERIOD_MS);

  if (KNOB_POWER_AUTO_STOP2_ENABLE == 0)
  {
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
  (void)PS_HW6_RTOS_RunStop2AutoIdleCheck(now_tick, 1UL);
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

static UINT PS_HW6_RTOS_SendUiButtonPress(ps_input_button_id_t button_id)
{
  ULONG message[PS_HW6_RTOS_MESSAGE_WORDS];
  UINT status;

  message[0] = PS_HW6_RTOS_UI_INPUT_MAGIC;
  message[1] = PS_HW6_RTOS_OWNER_UI;
  message[2] = PS_HW6_RTOS_UI_INPUT_PRESS;
  message[3] = (ULONG)button_id;
  status = tx_queue_send(&ps_queues[PS_HW6_RTOS_OWNER_UI],
                         message,
                         TX_NO_WAIT);
  PS_HW6_TraceInputButton((uint32_t)button_id,
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

static UINT PS_HW6_RTOS_SendRuntimeInputEvent(
  const ps_input_button_logical_record_t *record)
{
  ULONG message[PS_HW6_RTOS_MESSAGE_WORDS];
  UINT status;

  message[0] = PS_HW6_RTOS_RUNTIME_INPUT_MAGIC;
  message[1] = PS_HW6_RTOS_OWNER_RUNTIME;
  message[2] = (ULONG)record->event;
  message[3] = (ULONG)PS_HW6_RTOS_RuntimeInputPackedButton(
    (uint32_t)record->button_id,
    record->button_mask);
  status = tx_queue_send(&ps_queues[PS_HW6_RTOS_OWNER_RUNTIME],
                         message,
                         TX_NO_WAIT);
  PS_HW6_TraceInputButton((uint32_t)record->button_id,
                          PS_HW6_RTOS_OWNER_RUNTIME,
                          (uint32_t)status,
                          record->button_mask);
  return status;
}

static uint32_t PS_HW6_RTOS_InputPolicySystemOverlayActive(void)
{
  return PS_HW6_RTOS_SystemOverlayActive();
}

static uint32_t PS_HW6_RTOS_InputPolicyRuntimeClassOwnsButtons(
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
  else if (record->event != PS_INPUT_BUTTON_LOGICAL_EVENT_PRESS)
  {
    reason = PS_HW6_RTOS_INPUT_POLICY_REASON_UNSUPPORTED_EVENT;
  }
  else if ((record->button_id < PS_INPUT_BUTTON_ID_A) ||
           (record->button_id > PS_INPUT_BUTTON_ID_R))
  {
    reason = PS_HW6_RTOS_INPUT_POLICY_REASON_INVALID_BUTTON;
  }
  else if (g_ps_hw6_rtos_probe.input_policy_lock_active != 0UL)
  {
    reason = PS_HW6_RTOS_INPUT_POLICY_REASON_LOCKED;
  }
  else if (PS_HW6_RTOS_InputPolicySystemOverlayActive() != 0UL)
  {
    target = PS_HW6_RTOS_INPUT_POLICY_TARGET_UI;
    reason = PS_HW6_RTOS_INPUT_POLICY_REASON_SYSTEM_OVERLAY;
    status = PS_HW6_RTOS_SendUiButtonPress(record->button_id);
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
  else if ((runtime_class == (uint32_t)PS_HW6_RUNTIME_CLASS_NONE) ||
           (runtime_lifecycle !=
            (uint32_t)PS_HW6_RUNTIME_LIFECYCLE_RUNNING))
  {
    reason = PS_HW6_RTOS_INPUT_POLICY_REASON_RUNTIME_NOT_READY;
  }
  else if ((runtime_class == (uint32_t)PS_HW6_RUNTIME_CLASS_SHELL) ||
           (runtime_class == (uint32_t)PS_HW6_RUNTIME_CLASS_INSTALLER))
  {
    target = PS_HW6_RTOS_INPUT_POLICY_TARGET_UI;
    reason = PS_HW6_RTOS_INPUT_POLICY_REASON_UI_FOCUS;
    status = PS_HW6_RTOS_SendUiButtonPress(record->button_id);
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
  else if (PS_HW6_RTOS_InputPolicyRuntimeClassOwnsButtons(
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
  }
  else if (action == (uint32_t)PS_UI_ROUTER_ACTION_MSC_EXIT)
  {
    g_ps_hw6_rtos_probe.ui_action_msc_exit_count++;
    status = PS_HW6_RTOS_RequestUsbMscExit();
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
    display_focus = g_ps_ui_router_probe.package_state;
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
    PS_HW6_RTOS_OWNER_DISPLAY,
    PS_HW6_RTOS_OWNER_COMM,
    PS_HW6_RTOS_OWNER_SENSOR,
    PS_HW6_RTOS_OWNER_INPUT,
    PS_HW6_RTOS_OWNER_STORAGE
  };
  uint32_t index;

  PS_HW6_OwnerStateMachines_BeginPowerQuiesce(reason);
  for (index = 0U;
       index < (sizeof(quiesce_order) / sizeof(quiesce_order[0]));
       ++index)
  {
    const uint32_t owner_id = quiesce_order[index];
    const ULONG expected_ack = PS_HW6_RTOS_ACK_OWNER(owner_id);
    ULONG actual_flags = 0UL;
    UINT send_status = PS_HW6_RTOS_SendPowerQuiesceCommand(
      owner_id, reason);
    UINT wait_status = send_status;

    if (send_status == TX_SUCCESS)
    {
      wait_status = tx_event_flags_get(
        &ps_event_groups[PS_HW6_RTOS_EVENT_DEBUG_INDEX],
        expected_ack, TX_AND_CLEAR, &actual_flags,
        PS_HW6_RTOS_OWNER_ACK_WAIT_TICKS);
    }
    PS_HW6_OwnerStateMachines_RecordPowerQuiesceCommand(
      owner_id, send_status, wait_status, (uint32_t)actual_flags);
  }
  return PS_HW6_OwnerStateMachines_EndPowerQuiesce();
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

  PS_HW6_OwnerStateMachines_BeginPostStopResume();
  for (index = 0U;
       index < (sizeof(resume_order) / sizeof(resume_order[0]));
       ++index)
  {
    const uint32_t owner_id = resume_order[index];
    const ULONG expected_ack = PS_HW6_RTOS_ACK_OWNER(owner_id);
    ULONG actual_flags = 0UL;
    UINT send_status = PS_HW6_RTOS_SendPostStopResumeCommand(owner_id);
    UINT wait_status = send_status;

    if (send_status == TX_SUCCESS)
    {
      wait_status = tx_event_flags_get(
        &ps_event_groups[PS_HW6_RTOS_EVENT_DEBUG_INDEX],
        expected_ack, TX_AND_CLEAR, &actual_flags,
        PS_HW6_RTOS_OWNER_ACK_WAIT_TICKS);
    }
    PS_HW6_OwnerStateMachines_RecordPostStopResumeCommand(
      owner_id, send_status, wait_status, (uint32_t)actual_flags);
  }
  return PS_HW6_OwnerStateMachines_EndPostStopResume();
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
    page = (uint32_t)PS_UI_ROUTER_PAGE_HOME;
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
  g_ps_hw6_rtos_probe.runtime_return_class =
    (uint32_t)PS_HW6_RUNTIME_CLASS_SHELL;
  g_ps_hw6_rtos_probe.runtime_return_page =
    (uint32_t)PS_UI_ROUTER_PAGE_HOME;
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

static void PS_HW6_RTOS_RuntimePackageActivateStub(uint32_t runtime_class,
                                                   uint32_t execution,
                                                   uint32_t capabilities,
                                                   UINT clock_status)
{
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
  PS_HW6_RTOS_RuntimeRecordAdmission(runtime_class,
                                     execution,
                                     capabilities,
                                     clock_status);
  PS_HW6_RTOS_RuntimeRecord(event,
    (clock_status == TX_SUCCESS) ?
    (uint32_t)PS_STATUS_OK : (uint32_t)PS_STATUS_INTERNAL_ERROR);
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
    g_ps_hw6_rtos_probe.runtime_active_package_id = 0UL;
    g_ps_hw6_rtos_probe.runtime_active_unit_id = 0UL;
    PS_HW6_RTOS_RuntimeSetState(
      (uint32_t)PS_HW6_RUNTIME_CLASS_SHELL,
      (uint32_t)PS_HW6_RUNTIME_EXEC_REACTIVE,
      (uint32_t)PS_HW6_RUNTIME_LIFECYCLE_RUNNING);
  }

  PS_HW6_RTOS_RuntimeRecord(
    (uint32_t)PS_HW6_RUNTIME_EVENT_PACKAGE_RETURN,
    (release_status == TX_SUCCESS) ?
    (uint32_t)PS_STATUS_OK : (uint32_t)PS_STATUS_INTERNAL_ERROR);
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
      (uint32_t)PS_HW6_RUNTIME_CLASS_LP_MODULE,
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
  PS_HW6_RTOS_SendStorageLifecycleDisplayCue(
    PS_UI_ROUTER_SHUTDOWN_MSC_EXPORT);
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
      PS_HW6_RTOS_SendStorageLifecycleDisplayCue(
        PS_UI_ROUTER_SHUTDOWN_MSC_RECOVERY);
    }
    else
    {
      PS_HW6_RTOS_SendStorageLifecycleDisplayCue(
        PS_UI_ROUTER_SHUTDOWN_MSC_ERROR);
    }
    (void)PS_HW6_RTOS_RequestStorageClockCapabilities(
      PS_HW6_RTOS_STORAGE_CLOCK_REASON_RELEASE,
      0UL);
  }
  else
  {
    PS_HW6_RTOS_SendStorageLifecycleDisplayCue(
      PS_UI_ROUTER_SHUTDOWN_MSC_ACTIVE);
  }
  PS_HW6_RTOS_SetPowerDebug(GPIO_PIN_RESET);
}

static uint32_t PS_HW6_RTOS_ShouldForceUsbStageRescan(void)
{
  uint32_t current_page = g_ps_ui_router_probe.current_page;
  uint32_t return_page = g_ps_ui_router_probe.shutdown_return_page;

  return ((current_page ==
           (uint32_t)PS_UI_ROUTER_PAGE_PACKAGE_BROWSER) ||
          ((current_page == (uint32_t)PS_UI_ROUTER_PAGE_SHUTDOWN) &&
           (return_page ==
            (uint32_t)PS_UI_ROUTER_PAGE_PACKAGE_BROWSER))) ?
         1UL : 0UL;
}

static void PS_HW6_RTOS_RunStorageUsbReclaimRequest(void)
{
  UINT clock_status;
  HAL_StatusTypeDef reclaim_status = HAL_ERROR;
  uint32_t force_stage_rescan;

  force_stage_rescan = PS_HW6_RTOS_ShouldForceUsbStageRescan();

  PS_HW6_RTOS_SetPowerDebug(GPIO_PIN_SET);
  PS_HW6_RTOS_SendStorageLifecycleDisplayCue(
    PS_UI_ROUTER_SHUTDOWN_MSC_RECLAIM);
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
  (void)PS_HW6_RTOS_RequestStorageClockCapabilities(
    PS_HW6_RTOS_STORAGE_CLOCK_REASON_RELEASE,
    0UL);
  if (reclaim_status == HAL_OK)
  {
    if ((g_ps_hw6_owner_sm_probe.package_candidate_pending != 0UL) &&
        (g_ps_ui_router_request == 0UL))
    {
      g_ps_ui_router_request_event =
        PS_UI_ROUTER_EVENT_PACKAGE_VALID_FOUND;
      g_ps_ui_router_request = 1UL;
    }
    else if ((g_ps_hw6_owner_sm_probe.package_validate_status !=
              PS_HW6_OWNER_SM_STATUS_NOT_RUN) &&
             (g_ps_ui_router_request == 0UL))
    {
      (void)PS_HW6_RTOS_RequestRuntimeCommand(
        PS_HW6_RTOS_COMMAND_RUNTIME_INSTALLER_ERROR);
      g_ps_ui_router_request_event =
        PS_UI_ROUTER_EVENT_PACKAGE_VALIDATE_ERROR;
      g_ps_ui_router_request = 1UL;
    }
    else
    {
      (void)PS_HW6_RTOS_RequestRuntimeCommand(
        PS_HW6_RTOS_COMMAND_RUNTIME_INSTALLER_COMPLETE);
      PS_HW6_RTOS_SendCurrentUiRenderCommand();
    }
  }
  else
  {
    (void)PS_HW6_RTOS_RequestRuntimeCommand(
      PS_HW6_RTOS_COMMAND_RUNTIME_INSTALLER_ERROR);
    PS_HW6_RTOS_SendStorageLifecycleDisplayCue(
      PS_UI_ROUTER_SHUTDOWN_MSC_ERROR);
  }
  PS_HW6_RTOS_SetPowerDebug(GPIO_PIN_RESET);
}

static void PS_HW6_RTOS_RunStoragePackageInstallStubRequest(void)
{
  HAL_StatusTypeDef install_status;

  install_status = PS_HW6_OwnerStateMachines_RunPackageInstallStub();
  (void)PS_HW6_RTOS_RequestRuntimeCommand(
    (install_status == HAL_OK) ?
    PS_HW6_RTOS_COMMAND_RUNTIME_INSTALLER_COMPLETE :
    PS_HW6_RTOS_COMMAND_RUNTIME_INSTALLER_ERROR);
  if (g_ps_ui_router_request == 0UL)
  {
    g_ps_ui_router_request_event = (install_status == HAL_OK) ?
      (uint32_t)PS_UI_ROUTER_EVENT_PACKAGE_INSTALL_STUB_DONE :
      (uint32_t)PS_UI_ROUTER_EVENT_PACKAGE_INSTALL_STUB_ERROR;
    g_ps_ui_router_request = 1UL;
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
           (command == PS_HW6_RTOS_COMMAND_CLOCK_PROFILE))
  {
    uint32_t requester_id = PS_HW6_RTOS_ClockPayloadRequester(cycle_index);
    ULONG ack_flag = PS_HW6_RTOS_ClockAckFlag(requester_id);

    status = PS_HW6_ClockPolicy_ApplyRequesterProfile(
      requester_id,
      PS_HW6_RTOS_ClockPayloadProfile(cycle_index),
      PS_HW6_RTOS_ClockPayloadCapabilities(cycle_index));
    (void)status;
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

static void PS_HW6_RTOS_OwnerEntry(ULONG thread_input)
{
  const uint32_t owner_id = (uint32_t)thread_input;
  ULONG message[PS_HW6_RTOS_MESSAGE_WORDS];
  ULONG now;
  UINT status;
  ps_status_t router_status;
  ps_input_button_logical_record_t button_record;
  ps_input_start_power_event_t start_power_event;
  uint32_t button_drain_count;
  uint32_t start_power_timestamp;
  uint32_t start_power_hold_ticks;
  uint32_t start_power_drain_count;
  uint32_t audio_clock_release_after_request;
  uint32_t router_event;
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
    status = tx_queue_receive(&ps_queues[owner_id], message,
                              PS_HW6_RTOS_HEARTBEAT_TICKS);
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
        (void)PS_HW6_RTOS_RequestDisplayClockCapabilities(
          PS_HW6_RTOS_DISPLAY_CLOCK_REASON_TRANSFER,
          PS_HW6_RTOS_DISPLAY_CLOCK_TRANSFER_CAPABILITIES);
        (void)PS_HW6_DisplayOwner_RenderUI(
          (uint32_t)message[2],
          PS_HW6_RTOS_DisplayUiPackedCalibration(message[3]),
          PS_HW6_RTOS_DisplayUiPackedFocus(message[3]),
          PS_HW6_RTOS_DisplayUiPackedShutdown(message[3]),
          PS_HW6_RTOS_DisplayUiPackedCountdown(message[3]));
        (void)PS_HW6_RTOS_RequestDisplayClockCapabilities(
          PS_HW6_RTOS_DISPLAY_CLOCK_REASON_RELEASE,
          0UL);
      }
      else if (PS_HW6_RTOS_UiInputCommandIsValid(owner_id, message) != 0UL)
      {
        uint32_t button = (uint32_t)message[3];
        uint32_t action = (uint32_t)PS_UI_ROUTER_ACTION_NONE;

        (void)PS_HW6_RTOS_RequestUiClockCapabilities(
          PS_HW6_RTOS_UI_CLOCK_REASON_REACTIVE_TRANSACTION,
          PS_HW6_RTOS_UI_CLOCK_REACTIVE_CAPABILITIES);

        if ((button == (uint32_t)PS_INPUT_BUTTON_ID_B) &&
            (PS_HW6_RTOS_UiMscExportActive() != 0UL))
        {
          g_ps_hw6_rtos_probe.ui_action_msc_exit_intercept_count++;
          PS_HW6_RTOS_HandleUiRouterAction(
            (uint32_t)PS_UI_ROUTER_ACTION_MSC_EXIT);
        }
        else if (PS_HW6_RTOS_RequestJoystickCalibrationCapture(button) == 0UL)
        {
          router_status = PS_UIRouter_Dispatch(
            PS_HW6_RTOS_RouterEventForButton(button));
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
      (void)PS_HW6_RTOS_RunStop2AutoIdleCheck((uint32_t)now, 0UL);
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
      g_ps_hw6_joystick_cardinal_request = 0UL;
      (void)PS_HW6_OwnerStateMachines_RunJoystickCardinalProbe();
    }
    if ((owner_id == PS_HW6_RTOS_OWNER_INPUT) &&
        (g_ps_hw6_joystick_calibration_capture_request != 0UL) &&
        (g_ps_hw6_rtos_probe.runtime_complete != 0UL))
    {
      uint32_t calibration_page = g_ps_hw6_joystick_calibration_capture_page;
      g_ps_hw6_joystick_calibration_capture_request = 0UL;
      if (PS_HW6_OwnerStateMachines_RunJoystickCalibrationCapture(
            calibration_page) == HAL_OK)
      {
        g_ps_ui_router_request_event =
          PS_HW6_RTOS_RouterEventForCalibrationCapture(calibration_page);
        g_ps_ui_router_request = 1UL;
      }
    }
    if ((owner_id == PS_HW6_RTOS_OWNER_INPUT) &&
        (g_ps_hw6_rtos_probe.runtime_complete != 0UL))
    {
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
      for (button_drain_count = 0UL;
           button_drain_count < 4UL;
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
          router_status = PS_UIRouter_Dispatch(
            PS_UI_ROUTER_EVENT_BOOT_COMPLETE);
          if (router_status == PS_STATUS_OK)
          {
            (void)PS_HW6_RTOS_RequestRuntimeCommand(
              PS_HW6_RTOS_COMMAND_RUNTIME_BOOT_SHELL);
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
          (void)PS_HW6_RTOS_RequestRuntimeCommand(
            PS_HW6_RTOS_COMMAND_RUNTIME_BOOT_SHELL);
        }
        PS_HW6_RTOS_SendCurrentUiRenderCommand();
      }
      (void)PS_HW6_RTOS_RequestUiClockCapabilities(
        PS_HW6_RTOS_UI_CLOCK_REASON_RELEASE,
        0UL);
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
