#include "ps_hw6_rtos_probe.h"

#include <string.h>

#include "main.h"
#include "ps_hw6_clock_policy.h"
#include "ps_hw6_owner_services.h"
#include "ps_hw6_owner_state_machines.h"
#include "ps_hw6_trace.h"
#include "ps_input_buttons.h"
#include "ps_storage_msc_bridge.h"
#include "ps_ui_router.h"

#define PS_HW6_RTOS_DEFAULT_STACK_BYTES  (1024UL)
#define PS_HW6_RTOS_STORAGE_STACK_BYTES  (2048UL)
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
#define PS_HW6_RTOS_POWER_INPUT_MAGIC     (0x50574921UL)
#define PS_HW6_RTOS_UI_INPUT_PRESS        (1UL)
#define PS_HW6_RTOS_DISPLAY_UI_FIELD_MASK  (0xFFUL)
#define PS_HW6_RTOS_DISPLAY_UI_CAL_SHIFT   (0U)
#define PS_HW6_RTOS_DISPLAY_UI_FOCUS_SHIFT (8U)
#define PS_HW6_RTOS_DISPLAY_UI_SHUTDOWN_SHIFT (16U)
#define PS_HW6_RTOS_DISPLAY_UI_COUNTDOWN_SHIFT (24U)
#define PS_HW6_RTOS_DISPLAY_UI_SHUTDOWN_MAX \
  ((uint32_t)PS_UI_ROUTER_SHUTDOWN_LOW_BATTERY_CHARGE)
#define PS_HW6_RTOS_COMMAND_POWER_WORKFLOW (1UL)
#define PS_HW6_RTOS_COMMAND_STABILIZE     (2UL)
#define PS_HW6_RTOS_COMMAND_RESUME        (3UL)
#define PS_HW6_RTOS_COMMAND_QUIESCE       (4UL)
#define PS_HW6_RTOS_COMMAND_POWER_QUIESCE (5UL)
#define PS_HW6_RTOS_COMMAND_POST_STOP_RESUME (6UL)
#define PS_HW6_RTOS_COMMAND_CLOCK_PROFILE (7UL)
#define PS_HW6_RTOS_COMMAND_USB_EXPORT (8UL)
#define PS_HW6_RTOS_COMMAND_USB_RECLAIM (9UL)
#define PS_HW6_RTOS_EVENT_DEBUG_INDEX     (3U)
#define PS_HW6_RTOS_ACK_OWNER(owner_id)   (1UL << (owner_id))
#define PS_HW6_RTOS_CLOCK_ACK_FLAG        (1UL << 31)
#define PS_HW6_RTOS_CLOCK_PROFILE_MASK    (0xFFUL)
#define PS_HW6_RTOS_CLOCK_CAP_SHIFT       (8U)
#define PS_HW6_RTOS_OWNER_ACK_WAIT_TICKS  (1000UL)
#define PS_HW6_RTOS_STORAGE_STABILIZE_ACK_WAIT_TICKS (30000UL)
#define PS_HW6_RTOS_STATUS_NOT_RUN        (0xFFFFFFFFUL)

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

typedef UINT (*PS_HW6_RTOS_DebugCommandFn)(void);

static TX_THREAD ps_threads[PS_HW6_RTOS_OWNER_COUNT];
static TX_QUEUE ps_queues[PS_HW6_RTOS_QUEUE_COUNT];
static TX_EVENT_FLAGS_GROUP ps_event_groups[PS_HW6_RTOS_EVENT_GROUP_COUNT];
static VOID *ps_thread_stacks[PS_HW6_RTOS_OWNER_COUNT];
static VOID *ps_queue_storage[PS_HW6_RTOS_QUEUE_COUNT];
static volatile PS_HW6_RTOS_DebugCommandFn ps_debug_usb_export_anchor;
static volatile PS_HW6_RTOS_DebugCommandFn ps_debug_usb_reclaim_anchor;
static uint32_t ps_ui_boot_complete_sent;
static uint32_t ps_power_boot_done;
static uint32_t ps_display_bootstrap_sent;
static volatile uint32_t ps_pmic_int_pending_count;
static volatile uint32_t ps_pmic_int_irq_count;
static volatile uint32_t ps_pmic_int_last_pin;
static volatile uint32_t ps_pmic_int_last_level;
static volatile uint32_t ps_pmic_int_last_irq_tick;
static uint32_t ps_pmic_int_consumed_count;

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
  PS_HW6_RTOS_DEFAULT_STACK_BYTES,
  PS_HW6_RTOS_DEFAULT_STACK_BYTES,
  PS_HW6_RTOS_DEFAULT_STACK_BYTES,
  PS_HW6_RTOS_DEFAULT_STACK_BYTES,
  PS_HW6_RTOS_DEFAULT_STACK_BYTES,
  PS_HW6_RTOS_STORAGE_STACK_BYTES,
  PS_HW6_RTOS_DEFAULT_STACK_BYTES,
  PS_HW6_RTOS_DEFAULT_STACK_BYTES,
  PS_HW6_RTOS_DEFAULT_STACK_BYTES
};

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

static void PS_HW6_RTOS_ResetProbe(void)
{
  uint32_t i;

  (void)memset((void *)&g_ps_hw6_rtos_probe, 0,
               sizeof(g_ps_hw6_rtos_probe));
  g_ps_hw6_rtos_low_power_usb_skip_count = 0UL;
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
  g_ps_hw6_rtos_probe.magic = PS_HW6_RTOS_PROBE_MAGIC;
  g_ps_hw6_rtos_probe.version = PS_HW6_RTOS_PROBE_VERSION;
  g_ps_hw6_rtos_probe.phase = PS_HW6_RTOS_PHASE_INIT;
  g_ps_hw6_rtos_probe.init_status = TX_SUCCESS;
  g_ps_hw6_rtos_probe.init_error_step = PS_HW6_RTOS_STATUS_NOT_RUN;
  g_ps_hw6_rtos_probe.init_error_index = PS_HW6_RTOS_STATUS_NOT_RUN;
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

static uint32_t PS_HW6_RTOS_ClockProfilePayload(uint32_t profile,
                                                uint32_t capabilities)
{
  return (uint32_t)
    ((profile & PS_HW6_RTOS_CLOCK_PROFILE_MASK) |
     ((capabilities & PS_HW6_CLOCK_CAP_ALL) <<
      PS_HW6_RTOS_CLOCK_CAP_SHIFT));
}

static uint32_t PS_HW6_RTOS_ClockPayloadProfile(uint32_t payload)
{
  return payload & PS_HW6_RTOS_CLOCK_PROFILE_MASK;
}

static uint32_t PS_HW6_RTOS_ClockPayloadCapabilities(uint32_t payload)
{
  return (payload >> PS_HW6_RTOS_CLOCK_CAP_SHIFT) & PS_HW6_CLOCK_CAP_ALL;
}

static uint32_t PS_HW6_RTOS_ClockProfileRequestIsValid(uint32_t payload)
{
  uint32_t profile = PS_HW6_RTOS_ClockPayloadProfile(payload);
  uint32_t capabilities = PS_HW6_RTOS_ClockPayloadCapabilities(payload);

  if ((payload & ~(PS_HW6_RTOS_CLOCK_PROFILE_MASK |
                   (PS_HW6_CLOCK_CAP_ALL <<
                    PS_HW6_RTOS_CLOCK_CAP_SHIFT))) != 0UL)
  {
    return 0UL;
  }

  if ((capabilities & ~PS_HW6_CLOCK_CAP_ALL) != 0UL)
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
       (message[2] == PS_HW6_RTOS_COMMAND_USB_RECLAIM)) &&
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
  if ((owner_id != PS_HW6_RTOS_OWNER_DISPLAY) ||
      (message[0] != PS_HW6_RTOS_DISPLAY_UI_MAGIC) ||
      (message[1] != PS_HW6_RTOS_OWNER_DISPLAY) ||
      (message[2] > PS_UI_ROUTER_PAGE_SHUTDOWN) ||
      (PS_HW6_RTOS_DisplayUiPackedCalibration(message[3]) >
       PS_UI_ROUTER_CAL_JOYSTICK_REVIEW) ||
      (PS_HW6_RTOS_DisplayUiPackedFocus(message[3]) > 2UL) ||
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
UINT PS_HW6_RTOS_DebugRequestUsbExport(void)
{
  return PS_HW6_RTOS_SendCommand(
    PS_HW6_RTOS_OWNER_STORAGE,
    PS_HW6_RTOS_COMMAND_USB_EXPORT);
}

UINT PS_HW6_RTOS_DebugRequestUsbReclaim(void)
{
  return PS_HW6_RTOS_SendCommand(
    PS_HW6_RTOS_OWNER_STORAGE,
    PS_HW6_RTOS_COMMAND_USB_RECLAIM);
}

static void PS_HW6_RTOS_PrimeDebugCommandAnchors(void)
{
  ps_debug_usb_export_anchor = PS_HW6_RTOS_DebugRequestUsbExport;
  ps_debug_usb_reclaim_anchor = PS_HW6_RTOS_DebugRequestUsbReclaim;
}


static UINT PS_HW6_RTOS_SendClockProfileCommand(uint32_t profile,
                                                 uint32_t capabilities)
{
  ULONG message[PS_HW6_RTOS_MESSAGE_WORDS];

  if ((profile > (uint32_t)PS_HW6_CLOCK_PROFILE_STOP_PREP) ||
      ((capabilities & ~PS_HW6_CLOCK_CAP_ALL) != 0UL))
  {
    return TX_QUEUE_ERROR;
  }

  message[0] = PS_HW6_RTOS_COMMAND_MAGIC;
  message[1] = PS_HW6_RTOS_OWNER_POWER;
  message[2] = PS_HW6_RTOS_COMMAND_CLOCK_PROFILE;
  message[3] = PS_HW6_RTOS_COMMAND_TOKEN ^
               (ULONG)PS_HW6_RTOS_ClockProfilePayload(profile,
                                                       capabilities);
  return tx_queue_send(&ps_queues[PS_HW6_RTOS_OWNER_POWER],
                       message,
                       TX_NO_WAIT);
}

static UINT PS_HW6_RTOS_RequestPowerClockProfile(uint32_t profile,
                                                  uint32_t capabilities)
{
  ULONG actual_flags = 0UL;
  UINT send_status;
  UINT wait_status;

  (void)tx_event_flags_get(&ps_event_groups[PS_HW6_RTOS_EVENT_DEBUG_INDEX],
                           PS_HW6_RTOS_CLOCK_ACK_FLAG,
                           TX_AND_CLEAR,
                           &actual_flags,
                           TX_NO_WAIT);

  send_status = PS_HW6_RTOS_SendClockProfileCommand(profile, capabilities);
  wait_status = send_status;
  if (send_status == TX_SUCCESS)
  {
    wait_status = tx_event_flags_get(
      &ps_event_groups[PS_HW6_RTOS_EVENT_DEBUG_INDEX],
      PS_HW6_RTOS_CLOCK_ACK_FLAG,
      TX_AND_CLEAR,
      &actual_flags,
      PS_HW6_RTOS_OWNER_ACK_WAIT_TICKS);
  }

  if ((send_status == TX_SUCCESS) &&
      (wait_status == TX_SUCCESS) &&
      ((actual_flags & PS_HW6_RTOS_CLOCK_ACK_FLAG) != 0UL))
  {
    return (UINT)g_ps_hw6_clock_policy_probe.last_status;
  }

  return wait_status;
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
  (void)PS_HW6_RTOS_SendDisplayUiRenderCommand(
    g_ps_ui_router_probe.current_page,
    g_ps_ui_router_probe.calibration_page,
    g_ps_ui_router_probe.focus_index,
    g_ps_ui_router_probe.shutdown_state,
    g_ps_ui_router_probe.shutdown_countdown_seconds);
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
        PS_HW6_RTOS_StabilizeAckWaitTicks(owner_id));
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

static void PS_HW6_RTOS_RunStorageUsbExportRequest(void)
{
  UINT clock_status;
  HAL_StatusTypeDef export_status = HAL_ERROR;

  PS_HW6_RTOS_SetPowerDebug(GPIO_PIN_SET);
  clock_status = PS_HW6_RTOS_RequestPowerClockProfile(
    (uint32_t)PS_HW6_CLOCK_PROFILE_IO_HIGH,
    PS_HW6_CLOCK_CAP_USB_DEVICE_ACTIVE);
  g_ps_hw6_owner_sm_probe.usb_export_policy_status =
    (uint32_t)clock_status;
  if (clock_status == TX_SUCCESS)
  {
    export_status = PS_HW6_OwnerStateMachines_StartUsbExport();
  }
  if (export_status != HAL_OK)
  {
    (void)PS_HW6_RTOS_RequestPowerClockProfile(
      (uint32_t)PS_HW6_CLOCK_PROFILE_REACTIVE_BASE,
      0UL);
  }
  PS_HW6_RTOS_SetPowerDebug(GPIO_PIN_RESET);
}

static void PS_HW6_RTOS_RunStorageUsbReclaimRequest(void)
{
  PS_HW6_RTOS_SetPowerDebug(GPIO_PIN_SET);
  (void)PS_HW6_OwnerStateMachines_ReclaimUsbExport();
  (void)PS_HW6_RTOS_RequestPowerClockProfile(
    (uint32_t)PS_HW6_CLOCK_PROFILE_REACTIVE_BASE,
    0UL);
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
    status = PS_HW6_ClockPolicy_ApplyProfile(
      PS_HW6_RTOS_ClockPayloadProfile(cycle_index),
      PS_HW6_RTOS_ClockPayloadCapabilities(cycle_index));
    (void)status;
    (void)tx_event_flags_set(
      &ps_event_groups[PS_HW6_RTOS_EVENT_DEBUG_INDEX],
      PS_HW6_RTOS_CLOCK_ACK_FLAG,
      TX_OR);
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
  ps_input_button_id_t button_id;
  ps_input_start_power_event_t start_power_event;
  uint32_t button_timestamp;
  uint32_t button_drain_count;
  uint32_t start_power_timestamp;
  uint32_t start_power_hold_ticks;
  uint32_t start_power_drain_count;
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
  PS_HW6_RTOS_UpdateRuntimeComplete();

  if ((owner_id == PS_HW6_RTOS_OWNER_DISPLAY) &&
      (ps_display_bootstrap_sent == 0UL))
  {
    ps_display_bootstrap_sent = 1UL;
    g_ps_hw6_rtos_probe.boot_display_bootstrap_sent = 1UL;
    (void)PS_HW6_DisplayOwner_ClearBootHold();
  }

  for (;;)
  {
    status = tx_queue_receive(&ps_queues[owner_id], message,
                              PS_HW6_RTOS_HEARTBEAT_TICKS);
    now = tx_time_get();
    g_ps_hw6_rtos_probe.owner_heartbeat[owner_id]++;
    g_ps_hw6_rtos_probe.owner_last_tick[owner_id] = (uint32_t)now;

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
        }
      }
      else if (PS_HW6_RTOS_DisplayUiCommandIsValid(owner_id, message) != 0UL)
      {
        (void)PS_HW6_DisplayOwner_RenderUI(
          (uint32_t)message[2],
          PS_HW6_RTOS_DisplayUiPackedCalibration(message[3]),
          PS_HW6_RTOS_DisplayUiPackedFocus(message[3]),
          PS_HW6_RTOS_DisplayUiPackedShutdown(message[3]),
          PS_HW6_RTOS_DisplayUiPackedCountdown(message[3]));
      }
      else if (PS_HW6_RTOS_UiInputCommandIsValid(owner_id, message) != 0UL)
      {
        if (PS_HW6_RTOS_RequestJoystickCalibrationCapture(
              (uint32_t)message[3]) == 0UL)
        {
          router_status = PS_UIRouter_Dispatch(
            PS_HW6_RTOS_RouterEventForButton((uint32_t)message[3]));
          if (router_status == PS_STATUS_OK)
          {
            PS_HW6_RTOS_SendCurrentUiRenderCommand();
          }
        }
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

    if ((owner_id == PS_HW6_RTOS_OWNER_POWER) &&
        (ps_power_boot_done == 0UL) &&
        (ps_display_bootstrap_sent != 0UL) &&
        (g_ps_hw6_rtos_probe.runtime_complete != 0UL))
    {
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
        if (PS_InputButtons_TakePress(&button_id,
                                      &button_timestamp) == 0UL)
        {
          break;
        }
        (void)button_timestamp;
        (void)PS_HW6_RTOS_SendUiButtonPress(button_id);
      }
    }
    if ((owner_id == PS_HW6_RTOS_OWNER_UI) &&
        (ps_ui_boot_complete_sent == 0UL) &&
        (ps_power_boot_done != 0UL) &&
        (g_ps_hw6_rtos_probe.runtime_complete != 0UL))
    {
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
            PS_HW6_RTOS_SendCurrentUiRenderCommand();
          }
        }
      }
    }
    if ((owner_id == PS_HW6_RTOS_OWNER_UI) &&
        (g_ps_ui_router_request != 0UL) &&
        (g_ps_hw6_rtos_probe.runtime_complete != 0UL))
    {
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
        }
        PS_HW6_RTOS_SendCurrentUiRenderCommand();
      }
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
