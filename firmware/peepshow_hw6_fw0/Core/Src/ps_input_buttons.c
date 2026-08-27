#include "ps_input_buttons.h"

#include "knobs_autogen.h"

#include "tx_api.h"

#define PS_INPUT_BUTTON_MASK_A (1UL << 0U)
#define PS_INPUT_BUTTON_MASK_B (1UL << 1U)
#define PS_INPUT_BUTTON_MASK_L (1UL << 2U)
#define PS_INPUT_BUTTON_MASK_R (1UL << 3U)
#define PS_INPUT_BUTTON_COUNT  PS_INPUT_BUTTON_PHYSICAL_COUNT


volatile ps_input_buttons_probe_t g_ps_input_buttons_probe;

static volatile uint32_t ps_input_buttons_pending_mask;
static volatile uint32_t ps_input_button_press_edge_mask;
static volatile uint32_t ps_input_button_release_edge_mask;
static volatile uint32_t ps_input_button_tap_release_mask;
static volatile uint32_t ps_input_buttons_timestamp[PS_INPUT_BUTTON_COUNT];
static volatile uint32_t ps_input_button_state[PS_INPUT_BUTTON_COUNT];
static volatile uint32_t ps_input_button_raw_level[PS_INPUT_BUTTON_COUNT];
static volatile uint32_t ps_input_button_press_tick[PS_INPUT_BUTTON_COUNT];
static volatile uint32_t ps_input_button_release_tick[PS_INPUT_BUTTON_COUNT];
static volatile uint32_t ps_input_button_deadline_tick[PS_INPUT_BUTTON_COUNT];
static volatile uint32_t ps_input_button_return_state[PS_INPUT_BUTTON_COUNT];
static volatile uint32_t ps_input_button_return_deadline_tick[PS_INPUT_BUTTON_COUNT];
static ps_input_buttons_raw_edge_sink_t ps_input_buttons_raw_edge_sink;
static volatile uint32_t ps_input_start_active;
static volatile uint32_t ps_input_start_press_pending;
static volatile uint32_t ps_input_start_release_pending;
static volatile uint32_t ps_input_start_sample_level;
static volatile uint32_t ps_input_start_stable_level;
static volatile uint32_t ps_input_start_stable_count;
static volatile uint32_t ps_input_start_armed;
static volatile uint32_t ps_input_start_press_tick;
static volatile uint32_t ps_input_start_next_check_tick;
static volatile uint32_t ps_input_start_pending_event;
static volatile uint32_t ps_input_start_pending_timestamp;
static volatile uint32_t ps_input_start_pending_hold_ticks;
static volatile uint32_t ps_input_start_press_event_pending;
static volatile uint32_t ps_input_start_press_event_timestamp;
static volatile uint32_t ps_input_start_press_event_hold_ticks;
static volatile uint32_t ps_input_start_long_event_pending;
static volatile uint32_t ps_input_start_long_event_timestamp;
static volatile uint32_t ps_input_start_long_event_hold_ticks;
static uint32_t ps_input_start_long_ticks;
static uint32_t ps_input_start_ship_prep_ticks;
static uint32_t ps_input_start_ship_warn_ticks;
static uint32_t ps_input_start_ship_imminent_ticks;
static uint32_t ps_input_button_debounce_press_ticks;
static uint32_t ps_input_button_debounce_release_ticks;
static uint32_t ps_input_button_long_press_ticks;
static uint32_t ps_input_button_repeat_start_ticks;
static uint32_t ps_input_button_repeat_period_ticks;
static uint32_t ps_input_button_stuck_ticks;
static uint32_t ps_input_chord_window_ticks;

static uint32_t PS_InputButtons_MsToTicks(uint32_t ms)
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

static uint32_t PS_InputButtons_TimeReached(uint32_t now_tick,
                                            uint32_t deadline_tick)
{
  return (((int32_t)(now_tick - deadline_tick)) >= 0) ? 1UL : 0UL;
}

static uint32_t PS_InputButtons_StartLiveLevel(void)
{
  return (HAL_GPIO_ReadPin(BTN_START_GPIO_Port, BTN_START_Pin) ==
          GPIO_PIN_SET) ? 1UL : 0UL;
}

static uint32_t PS_InputButtons_ButtonLiveLevel(uint32_t index)
{
  GPIO_PinState level;

  if (index == 0UL)
  {
    level = HAL_GPIO_ReadPin(BTN_A_GPIO_Port, BTN_A_Pin);
  }
  else if (index == 1UL)
  {
    level = HAL_GPIO_ReadPin(BTN_B_GPIO_Port, BTN_B_Pin);
  }
  else if (index == 2UL)
  {
    level = HAL_GPIO_ReadPin(BTN_L_GPIO_Port, BTN_L_Pin);
  }
  else
  {
    level = HAL_GPIO_ReadPin(BTN_R_GPIO_Port, BTN_R_Pin);
  }

  return (level == GPIO_PIN_SET) ? 1UL : 0UL;
}

static ps_input_button_id_t PS_InputButtons_ButtonForIndex(uint32_t index)
{
  if (index == 0UL)
  {
    return PS_INPUT_BUTTON_ID_A;
  }
  if (index == 1UL)
  {
    return PS_INPUT_BUTTON_ID_B;
  }
  if (index == 2UL)
  {
    return PS_INPUT_BUTTON_ID_L;
  }
  return PS_INPUT_BUTTON_ID_R;
}

static void PS_InputButtons_SetButtonState(uint32_t index,
                                           ps_input_button_state_t state,
                                           uint32_t deadline_tick)
{
  ps_input_button_state[index] = (uint32_t)state;
  ps_input_button_deadline_tick[index] = deadline_tick;
  g_ps_input_buttons_probe.button_state[index] = (uint32_t)state;
  g_ps_input_buttons_probe.button_deadline_tick[index] = deadline_tick;
}

static void PS_InputButtons_RecordLogicalEvent(
  const ps_input_button_logical_record_t *record)
{
  g_ps_input_buttons_probe.logical_event_count++;
  g_ps_input_buttons_probe.logical_last_event = (uint32_t)record->event;
  g_ps_input_buttons_probe.logical_last_button_id =
    (uint32_t)record->button_id;
  g_ps_input_buttons_probe.logical_last_mask = record->button_mask;
  g_ps_input_buttons_probe.logical_last_timestamp = record->timestamp;
  g_ps_input_buttons_probe.logical_last_hold_ticks = record->hold_ticks;

  if (record->event == PS_INPUT_BUTTON_LOGICAL_EVENT_PRESS)
  {
    g_ps_input_buttons_probe.logical_press_count++;
  }
  else if (record->event == PS_INPUT_BUTTON_LOGICAL_EVENT_RELEASE)
  {
    g_ps_input_buttons_probe.logical_release_count++;
  }
  else if (record->event == PS_INPUT_BUTTON_LOGICAL_EVENT_LONG_PRESS)
  {
    g_ps_input_buttons_probe.logical_long_count++;
  }
  else if (record->event == PS_INPUT_BUTTON_LOGICAL_EVENT_REPEAT)
  {
    g_ps_input_buttons_probe.logical_repeat_count++;
  }
  else if (record->event == PS_INPUT_BUTTON_LOGICAL_EVENT_CHORD)
  {
    g_ps_input_buttons_probe.logical_chord_count++;
  }
  else if (record->event == PS_INPUT_BUTTON_LOGICAL_EVENT_STUCK)
  {
    g_ps_input_buttons_probe.logical_stuck_count++;
  }
}

static void PS_InputButtons_QueueButtonPress(uint32_t index,
                                             uint32_t timestamp)
{
  uint32_t mask;

  mask = 1UL << index;
  ps_input_buttons_timestamp[index] = timestamp;
  ps_input_buttons_pending_mask |= mask;
  g_ps_input_buttons_probe.pending_mask = ps_input_buttons_pending_mask;
  g_ps_input_buttons_probe.button_press_accept_count++;
}

static void PS_InputButtons_StartButtonReleaseDebounce(uint32_t index,
                                                       uint32_t now_tick)
{
  ps_input_button_return_state[index] = ps_input_button_state[index];
  ps_input_button_return_deadline_tick[index] =
    ps_input_button_deadline_tick[index];
  PS_InputButtons_SetButtonState(
    index,
    PS_INPUT_BUTTON_STATE_DEBOUNCE_RELEASE,
    now_tick + ps_input_button_debounce_release_ticks);
  g_ps_input_buttons_probe.button_debounce_release_count++;
}

static void PS_InputButtons_UpdateStartSample(uint32_t live_level)
{
  g_ps_input_buttons_probe.start_sample_count++;
  g_ps_input_buttons_probe.start_raw_level = live_level;

  if (live_level == ps_input_start_sample_level)
  {
    if (ps_input_start_stable_count < KNOB_INPUT_START_STABLE_SAMPLES)
    {
      ps_input_start_stable_count++;
    }
  }
  else
  {
    ps_input_start_sample_level = live_level;
    ps_input_start_stable_count = 1UL;
  }

  if (ps_input_start_stable_count >= KNOB_INPUT_START_STABLE_SAMPLES)
  {
    ps_input_start_stable_level = ps_input_start_sample_level;
  }

  g_ps_input_buttons_probe.start_stable_level = ps_input_start_stable_level;
  g_ps_input_buttons_probe.start_stable_count = ps_input_start_stable_count;
}

static void PS_InputButtons_ArmStartCheck(uint32_t deadline_tick)
{
  ps_input_start_armed = 1UL;
  ps_input_start_next_check_tick = deadline_tick;
  g_ps_input_buttons_probe.start_armed = 1UL;
  g_ps_input_buttons_probe.start_next_check_tick = deadline_tick;
}

static void PS_InputButtons_DisarmStartCheck(void)
{
  ps_input_start_armed = 0UL;
  ps_input_start_next_check_tick = 0UL;
  g_ps_input_buttons_probe.start_armed = 0UL;
  g_ps_input_buttons_probe.start_next_check_tick = 0UL;
}

static void PS_InputButtons_PublishStartPowerEvent(
  ps_input_start_power_event_t event,
  uint32_t timestamp,
  uint32_t hold_ticks)
{
  if (ps_input_start_pending_event !=
      (uint32_t)PS_INPUT_START_POWER_EVENT_NONE)
  {
    g_ps_input_buttons_probe.start_pending_drop_count++;
  }

  ps_input_start_pending_event = (uint32_t)event;
  ps_input_start_pending_timestamp = timestamp;
  ps_input_start_pending_hold_ticks = hold_ticks;
  g_ps_input_buttons_probe.start_pending_event = (uint32_t)event;
  g_ps_input_buttons_probe.start_pending_timestamp = timestamp;
  g_ps_input_buttons_probe.start_pending_hold_ticks = hold_ticks;

  if (event == PS_INPUT_START_POWER_EVENT_SHIP_PREP)
  {
    g_ps_input_buttons_probe.start_ship_prep_count++;
  }
  else if (event == PS_INPUT_START_POWER_EVENT_SHIP_WARNING)
  {
    g_ps_input_buttons_probe.start_ship_warning_count++;
  }
  else if (event == PS_INPUT_START_POWER_EVENT_SHIP_IMMINENT)
  {
    g_ps_input_buttons_probe.start_ship_imminent_count++;
  }
  else if (event == PS_INPUT_START_POWER_EVENT_RELEASED_BEFORE_SHIP)
  {
    g_ps_input_buttons_probe.start_release_before_ship_count++;
  }
}

static void PS_InputButtons_AcceptStartPress(uint32_t now_tick,
                                             uint32_t synthetic)
{
  ps_input_start_active = 1UL;
  ps_input_start_press_pending = 0UL;
  ps_input_start_release_pending = 0UL;
  ps_input_start_press_tick = now_tick;

  g_ps_input_buttons_probe.start_active = 1UL;
  g_ps_input_buttons_probe.start_press_pending = 0UL;
  g_ps_input_buttons_probe.start_release_pending = 0UL;
  g_ps_input_buttons_probe.start_live_level = 0UL;
  g_ps_input_buttons_probe.start_press_tick = now_tick;
  g_ps_input_buttons_probe.start_release_tick = 0UL;
  g_ps_input_buttons_probe.start_hold_ticks = 0UL;
  g_ps_input_buttons_probe.start_state = PS_INPUT_START_STATE_NORMAL_PRESS;
  g_ps_input_buttons_probe.last_button_id = PS_INPUT_BUTTON_ID_START;
  g_ps_input_buttons_probe.last_event = PS_INPUT_BUTTON_EVENT_PRESS;
  if (synthetic != 0UL)
  {
    g_ps_input_buttons_probe.start_synth_press_count++;
  }
  PS_InputButtons_ArmStartCheck(now_tick + ps_input_start_long_ticks);
}

static void PS_InputButtons_RecordStartRelease(uint32_t now_tick)
{
  uint32_t hold_ticks;
  uint32_t start_state;
  uint32_t was_shipping_intent;

  hold_ticks = ((ps_input_start_active != 0UL) &&
                (ps_input_start_press_tick != 0UL)) ?
    (now_tick - ps_input_start_press_tick) : 0UL;
  was_shipping_intent =
    (g_ps_input_buttons_probe.start_state >=
     (uint32_t)PS_INPUT_START_STATE_SHIP_PREP) ? 1UL : 0UL;
  start_state = g_ps_input_buttons_probe.start_state;

  if (start_state == (uint32_t)PS_INPUT_START_STATE_NORMAL_PRESS)
  {
    if (ps_input_start_press_event_pending == 0UL)
    {
      ps_input_start_press_event_pending = 1UL;
      ps_input_start_press_event_timestamp = now_tick;
      ps_input_start_press_event_hold_ticks = hold_ticks;
      g_ps_input_buttons_probe.start_short_press_count++;
      g_ps_input_buttons_probe.start_short_press_pending = 1UL;
    }
    else
    {
      g_ps_input_buttons_probe.start_pending_drop_count++;
    }
  }

  ps_input_start_active = 0UL;
  ps_input_start_press_pending = 0UL;
  ps_input_start_release_pending = 0UL;
  PS_InputButtons_DisarmStartCheck();

  g_ps_input_buttons_probe.start_active = 0UL;
  g_ps_input_buttons_probe.start_press_pending = 0UL;
  g_ps_input_buttons_probe.start_release_pending = 0UL;
  g_ps_input_buttons_probe.start_live_level = 1UL;
  g_ps_input_buttons_probe.start_release_tick = now_tick;
  g_ps_input_buttons_probe.start_hold_ticks = hold_ticks;
  g_ps_input_buttons_probe.start_state = PS_INPUT_START_STATE_RELEASED;
  g_ps_input_buttons_probe.last_event = PS_INPUT_BUTTON_EVENT_RELEASE;

  if (was_shipping_intent != 0UL)
  {
    PS_InputButtons_PublishStartPowerEvent(
      PS_INPUT_START_POWER_EVENT_RELEASED_BEFORE_SHIP,
      now_tick,
      hold_ticks);
  }
}

static uint32_t PS_InputButtons_RecordStartExti(uint16_t gpio_pin,
                                                GPIO_PinState level)
{
  uint32_t debug_tick;

  if (gpio_pin != BTN_START_Pin)
  {
    return 0UL;
  }

  debug_tick = HAL_GetTick();
  g_ps_input_buttons_probe.isr_edge_count++;
  g_ps_input_buttons_probe.last_pin = gpio_pin;
  g_ps_input_buttons_probe.last_button_id = PS_INPUT_BUTTON_ID_START;
  g_ps_input_buttons_probe.last_level = (level == GPIO_PIN_SET) ? 1UL : 0UL;
  g_ps_input_buttons_probe.start_live_level =
    (level == GPIO_PIN_SET) ? 1UL : 0UL;
  g_ps_input_buttons_probe.last_tick = debug_tick;

  if (level == GPIO_PIN_RESET)
  {
    if (ps_input_start_active == 0UL)
    {
      ps_input_start_active = 1UL;
      ps_input_start_press_pending = 1UL;
      ps_input_start_release_pending = 0UL;
      ps_input_start_press_tick = 0UL;
      PS_InputButtons_DisarmStartCheck();

      g_ps_input_buttons_probe.start_active = 1UL;
      g_ps_input_buttons_probe.start_press_pending = 1UL;
      g_ps_input_buttons_probe.start_release_pending = 0UL;
      g_ps_input_buttons_probe.start_press_tick = 0UL;
      g_ps_input_buttons_probe.start_release_tick = 0UL;
      g_ps_input_buttons_probe.start_hold_ticks = 0UL;
      g_ps_input_buttons_probe.start_state =
        PS_INPUT_START_STATE_NORMAL_PRESS;
    }
    g_ps_input_buttons_probe.last_event = PS_INPUT_BUTTON_EVENT_PRESS;
    return 1UL;
  }

  if (ps_input_start_active != 0UL)
  {
    ps_input_start_release_pending = 1UL;
    g_ps_input_buttons_probe.start_release_pending = 1UL;
  }
  g_ps_input_buttons_probe.last_event = PS_INPUT_BUTTON_EVENT_RELEASE;
  return 1UL;
}

static uint32_t PS_InputButtons_MaskForPin(uint16_t gpio_pin,
                                           ps_input_button_id_t *button_id)
{
  if (gpio_pin == BTN_A_Pin)
  {
    *button_id = PS_INPUT_BUTTON_ID_A;
    return PS_INPUT_BUTTON_MASK_A;
  }
  if (gpio_pin == BTN_B_Pin)
  {
    *button_id = PS_INPUT_BUTTON_ID_B;
    return PS_INPUT_BUTTON_MASK_B;
  }
  if (gpio_pin == BTN_L_Pin)
  {
    *button_id = PS_INPUT_BUTTON_ID_L;
    return PS_INPUT_BUTTON_MASK_L;
  }
  if (gpio_pin == BTN_R_Pin)
  {
    *button_id = PS_INPUT_BUTTON_ID_R;
    return PS_INPUT_BUTTON_MASK_R;
  }

  *button_id = PS_INPUT_BUTTON_ID_NONE;
  return 0UL;
}

void PS_InputButtons_Init(void)
{
  uint32_t i;

  g_ps_input_buttons_probe.api_version = PS_INPUT_BUTTONS_API_VERSION;
  g_ps_input_buttons_probe.isr_edge_count = 0UL;
  g_ps_input_buttons_probe.press_count = 0UL;
  g_ps_input_buttons_probe.ignored_edge_count = 0UL;
  g_ps_input_buttons_probe.pending_mask = 0UL;
  g_ps_input_buttons_probe.last_pin = 0UL;
  g_ps_input_buttons_probe.last_button_id = PS_INPUT_BUTTON_ID_NONE;
  g_ps_input_buttons_probe.last_event = PS_INPUT_BUTTON_EVENT_NONE;
  g_ps_input_buttons_probe.last_level = 0UL;
  g_ps_input_buttons_probe.last_tick = 0UL;
  g_ps_input_buttons_probe.button_debounce_press_ticks =
    PS_InputButtons_MsToTicks(KNOB_INPUT_BTN_DEBOUNCE_PRESS_MS);
  g_ps_input_buttons_probe.button_debounce_release_ticks =
    PS_InputButtons_MsToTicks(KNOB_INPUT_BTN_DEBOUNCE_RELEASE_MS);
  g_ps_input_buttons_probe.button_long_press_ticks =
    PS_InputButtons_MsToTicks(KNOB_INPUT_BTN_LONG_PRESS_MS);
  g_ps_input_buttons_probe.button_repeat_start_ticks =
    PS_InputButtons_MsToTicks(KNOB_INPUT_BTN_REPEAT_START_MS);
  g_ps_input_buttons_probe.button_repeat_period_ticks =
    PS_InputButtons_MsToTicks(KNOB_INPUT_BTN_REPEAT_PERIOD_MS);
  g_ps_input_buttons_probe.button_stuck_ticks =
    PS_InputButtons_MsToTicks(KNOB_INPUT_BTN_STUCK_MS);
  g_ps_input_buttons_probe.button_chord_window_ticks =
    PS_InputButtons_MsToTicks(KNOB_INPUT_CHORD_WINDOW_MS);
  g_ps_input_buttons_probe.button_debounce_press_count = 0UL;
  g_ps_input_buttons_probe.button_debounce_release_count = 0UL;
  g_ps_input_buttons_probe.button_press_accept_count = 0UL;
  g_ps_input_buttons_probe.button_release_accept_count = 0UL;
  g_ps_input_buttons_probe.button_long_count = 0UL;
  g_ps_input_buttons_probe.button_repeat_count = 0UL;
  g_ps_input_buttons_probe.button_stuck_count = 0UL;
  g_ps_input_buttons_probe.button_bounce_reject_count = 0UL;
  g_ps_input_buttons_probe.raw_edge_send_count = 0UL;
  g_ps_input_buttons_probe.raw_edge_drop_count = 0UL;
  g_ps_input_buttons_probe.raw_edge_process_count = 0UL;
  g_ps_input_buttons_probe.raw_edge_recovery_count = 0UL;
  g_ps_input_buttons_probe.raw_edge_last_status = 0xFFFFFFFFUL;
  g_ps_input_buttons_probe.raw_edge_last_timestamp = 0UL;
  g_ps_input_buttons_probe.start_state = PS_INPUT_START_STATE_IDLE;
  g_ps_input_buttons_probe.start_active = 0UL;
  g_ps_input_buttons_probe.start_press_pending = 0UL;
  g_ps_input_buttons_probe.start_release_pending = 0UL;
  g_ps_input_buttons_probe.start_armed = 0UL;
  g_ps_input_buttons_probe.start_live_level = 1UL;
  g_ps_input_buttons_probe.start_raw_level = 1UL;
  g_ps_input_buttons_probe.start_stable_level = 1UL;
  g_ps_input_buttons_probe.start_stable_count = KNOB_INPUT_START_STABLE_SAMPLES;
  g_ps_input_buttons_probe.start_sample_count = 0UL;
  g_ps_input_buttons_probe.start_synth_press_count = 0UL;
  g_ps_input_buttons_probe.start_next_check_tick = 0UL;
  g_ps_input_buttons_probe.start_checkpoint_count = 0UL;
  g_ps_input_buttons_probe.start_synth_release_count = 0UL;
  g_ps_input_buttons_probe.start_press_tick = 0UL;
  g_ps_input_buttons_probe.start_release_tick = 0UL;
  g_ps_input_buttons_probe.start_hold_ticks = 0UL;
  g_ps_input_buttons_probe.start_short_press_count = 0UL;
  g_ps_input_buttons_probe.start_long_press_count = 0UL;
  g_ps_input_buttons_probe.start_short_press_pending = 0UL;
  g_ps_input_buttons_probe.start_long_press_pending = 0UL;
  g_ps_input_buttons_probe.start_ship_prep_count = 0UL;
  g_ps_input_buttons_probe.start_ship_warning_count = 0UL;
  g_ps_input_buttons_probe.start_ship_imminent_count = 0UL;
  g_ps_input_buttons_probe.start_release_before_ship_count = 0UL;
  g_ps_input_buttons_probe.start_pending_event =
    PS_INPUT_START_POWER_EVENT_NONE;
  g_ps_input_buttons_probe.start_pending_timestamp = 0UL;
  g_ps_input_buttons_probe.start_pending_hold_ticks = 0UL;
  g_ps_input_buttons_probe.start_pending_drop_count = 0UL;

  ps_input_buttons_pending_mask = 0UL;
  ps_input_button_press_edge_mask = 0UL;
  ps_input_button_release_edge_mask = 0UL;
  ps_input_button_tap_release_mask = 0UL;
  ps_input_button_debounce_press_ticks =
    PS_InputButtons_MsToTicks(KNOB_INPUT_BTN_DEBOUNCE_PRESS_MS);
  ps_input_button_debounce_release_ticks =
    PS_InputButtons_MsToTicks(KNOB_INPUT_BTN_DEBOUNCE_RELEASE_MS);
  ps_input_button_long_press_ticks =
    PS_InputButtons_MsToTicks(KNOB_INPUT_BTN_LONG_PRESS_MS);
  ps_input_button_repeat_start_ticks =
    PS_InputButtons_MsToTicks(KNOB_INPUT_BTN_REPEAT_START_MS);
  ps_input_button_repeat_period_ticks =
    PS_InputButtons_MsToTicks(KNOB_INPUT_BTN_REPEAT_PERIOD_MS);
  ps_input_button_stuck_ticks =
    PS_InputButtons_MsToTicks(KNOB_INPUT_BTN_STUCK_MS);
  ps_input_chord_window_ticks =
    PS_InputButtons_MsToTicks(KNOB_INPUT_CHORD_WINDOW_MS);
  ps_input_buttons_raw_edge_sink = NULL;
  ps_input_start_active = 0UL;
  ps_input_start_press_pending = 0UL;
  ps_input_start_release_pending = 0UL;
  ps_input_start_sample_level = 1UL;
  ps_input_start_stable_level = 1UL;
  ps_input_start_stable_count = KNOB_INPUT_START_STABLE_SAMPLES;
  ps_input_start_armed = 0UL;
  ps_input_start_press_tick = 0UL;
  ps_input_start_next_check_tick = 0UL;
  ps_input_start_pending_event = PS_INPUT_START_POWER_EVENT_NONE;
  ps_input_start_pending_timestamp = 0UL;
  ps_input_start_pending_hold_ticks = 0UL;
  ps_input_start_press_event_pending = 0UL;
  ps_input_start_press_event_timestamp = 0UL;
  ps_input_start_press_event_hold_ticks = 0UL;
  ps_input_start_long_event_pending = 0UL;
  ps_input_start_long_event_timestamp = 0UL;
  ps_input_start_long_event_hold_ticks = 0UL;
  ps_input_start_long_ticks =
    PS_InputButtons_MsToTicks(KNOB_INPUT_START_LONG_PRESS_MS);
  ps_input_start_ship_prep_ticks =
    PS_InputButtons_MsToTicks(KNOB_INPUT_START_SHIP_PREP_MS);
  ps_input_start_ship_warn_ticks =
    PS_InputButtons_MsToTicks(KNOB_INPUT_START_SHIP_WARN_MS);
  ps_input_start_ship_imminent_ticks =
    PS_InputButtons_MsToTicks(KNOB_INPUT_START_SHIP_IMMINENT_MS);

  for (i = 0UL; i < PS_INPUT_BUTTON_COUNT; ++i)
  {
    ps_input_buttons_timestamp[i] = 0UL;
    ps_input_button_state[i] = PS_INPUT_BUTTON_STATE_RELEASED;
    ps_input_button_raw_level[i] = 0UL;
    ps_input_button_press_tick[i] = 0UL;
    ps_input_button_release_tick[i] = 0UL;
    ps_input_button_deadline_tick[i] = 0UL;
    ps_input_button_return_state[i] = PS_INPUT_BUTTON_STATE_RELEASED;
    ps_input_button_return_deadline_tick[i] = 0UL;
    g_ps_input_buttons_probe.button_state[i] = PS_INPUT_BUTTON_STATE_RELEASED;
    g_ps_input_buttons_probe.button_raw_level[i] = 0UL;
    g_ps_input_buttons_probe.button_press_tick[i] = 0UL;
    g_ps_input_buttons_probe.button_release_tick[i] = 0UL;
    g_ps_input_buttons_probe.button_deadline_tick[i] = 0UL;
  }
}

void PS_InputButtons_SetRawEdgeSink(
  ps_input_buttons_raw_edge_sink_t sink)
{
  ps_input_buttons_raw_edge_sink = sink;
}

void PS_InputButtons_RecordExti(uint16_t gpio_pin, GPIO_PinState level)
{
  ps_input_button_id_t button_id;
  uint32_t active;
  uint32_t debug_tick;
  uint32_t index;
  uint32_t mask;
  uint32_t status;

  if (PS_InputButtons_RecordStartExti(gpio_pin, level) != 0UL)
  {
    return;
  }

  mask = PS_InputButtons_MaskForPin(gpio_pin, &button_id);
  active = (level == GPIO_PIN_SET) ? 1UL : 0UL;
  debug_tick = (uint32_t)tx_time_get();

  g_ps_input_buttons_probe.isr_edge_count++;
  g_ps_input_buttons_probe.last_pin = gpio_pin;
  g_ps_input_buttons_probe.last_button_id = button_id;
  g_ps_input_buttons_probe.last_level = active;
  g_ps_input_buttons_probe.last_tick = debug_tick;

  if (mask == 0UL)
  {
    g_ps_input_buttons_probe.ignored_edge_count++;
    return;
  }

  if (ps_input_buttons_raw_edge_sink != NULL)
  {
    status = ps_input_buttons_raw_edge_sink(button_id, active, debug_tick);
    g_ps_input_buttons_probe.raw_edge_send_count++;
    g_ps_input_buttons_probe.raw_edge_last_status = status;
    g_ps_input_buttons_probe.raw_edge_last_timestamp = debug_tick;
    if (status == (uint32_t)TX_SUCCESS)
    {
      return;
    }
    g_ps_input_buttons_probe.raw_edge_drop_count++;
  }

  index = (uint32_t)button_id - 1UL;
  ps_input_button_raw_level[index] = active;
  g_ps_input_buttons_probe.button_raw_level[index] = active;

  if (active != 0UL)
  {
    ps_input_button_press_edge_mask |= mask;
    g_ps_input_buttons_probe.last_event = PS_INPUT_BUTTON_EVENT_PRESS;
    return;
  }

  ps_input_button_release_edge_mask |= mask;
  g_ps_input_buttons_probe.last_event = PS_INPUT_BUTTON_EVENT_RELEASE;
}

uint32_t PS_InputButtons_StartCheckDue(uint32_t now_tick)
{
  uint32_t due;
  uint32_t live_level;
  uint32_t primask;

  live_level = PS_InputButtons_StartLiveLevel();

  primask = __get_PRIMASK();
  __disable_irq();
  PS_InputButtons_UpdateStartSample(live_level);
  due = ((ps_input_start_press_pending != 0UL) ||
         (ps_input_start_release_pending != 0UL) ||
         ((ps_input_start_active == 0UL) &&
          (ps_input_start_stable_level == 0UL)) ||
         ((ps_input_start_active != 0UL) &&
          (ps_input_start_stable_level != 0UL)) ||
         ((ps_input_start_armed != 0UL) &&
          (PS_InputButtons_TimeReached(now_tick,
                                       ps_input_start_next_check_tick) !=
           0UL))) ? 1UL : 0UL;
  if (primask == 0UL)
  {
    __enable_irq();
  }
  return due;
}

void PS_InputButtons_PollStart(uint32_t now_tick)
{
  uint32_t hold_ticks;
  uint32_t live_level;
  uint32_t primask;
  uint32_t stable_level;
  uint32_t state;

  live_level = PS_InputButtons_StartLiveLevel();

  primask = __get_PRIMASK();
  __disable_irq();
  stable_level = ps_input_start_stable_level;

  if (ps_input_start_press_pending != 0UL)
  {
    g_ps_input_buttons_probe.start_live_level = live_level;
    if (live_level == 0UL)
    {
      PS_InputButtons_AcceptStartPress(now_tick, 0UL);
      if (primask == 0UL)
      {
        __enable_irq();
      }
      return;
    }
    if ((ps_input_start_release_pending != 0UL) &&
        (stable_level != 0UL))
    {
      PS_InputButtons_RecordStartRelease(now_tick);
      if (primask == 0UL)
      {
        __enable_irq();
      }
      return;
    }
    if (primask == 0UL)
    {
      __enable_irq();
    }
    return;
  }

  if ((ps_input_start_active == 0UL) && (stable_level == 0UL))
  {
    PS_InputButtons_AcceptStartPress(now_tick, 1UL);
    if (primask == 0UL)
    {
      __enable_irq();
    }
    return;
  }

  if ((ps_input_start_release_pending != 0UL) && (stable_level == 0UL))
  {
    ps_input_start_release_pending = 0UL;
    g_ps_input_buttons_probe.start_release_pending = 0UL;
  }

  if ((ps_input_start_active != 0UL) && (stable_level != 0UL))
  {
    if (ps_input_start_release_pending == 0UL)
    {
      g_ps_input_buttons_probe.start_synth_release_count++;
    }
    PS_InputButtons_RecordStartRelease(now_tick);
    if (primask == 0UL)
    {
      __enable_irq();
    }
    return;
  }

  if ((ps_input_start_active == 0UL) ||
      (ps_input_start_armed == 0UL) ||
      (PS_InputButtons_TimeReached(now_tick,
                                   ps_input_start_next_check_tick) == 0UL))
  {
    if (primask == 0UL)
    {
      __enable_irq();
    }
    return;
  }

  g_ps_input_buttons_probe.start_checkpoint_count++;
  g_ps_input_buttons_probe.start_live_level = live_level;
  hold_ticks = now_tick - ps_input_start_press_tick;
  g_ps_input_buttons_probe.start_hold_ticks = hold_ticks;

  if (stable_level != 0UL)
  {
    g_ps_input_buttons_probe.start_synth_release_count++;
    PS_InputButtons_RecordStartRelease(now_tick);
    if (primask == 0UL)
    {
      __enable_irq();
    }
    return;
  }

  state = g_ps_input_buttons_probe.start_state;
  if (state == (uint32_t)PS_INPUT_START_STATE_NORMAL_PRESS)
  {
    g_ps_input_buttons_probe.start_state = PS_INPUT_START_STATE_LONG_PRESS;
    if (ps_input_start_long_event_pending == 0UL)
    {
      ps_input_start_long_event_pending = 1UL;
      ps_input_start_long_event_timestamp = now_tick;
      ps_input_start_long_event_hold_ticks = hold_ticks;
      g_ps_input_buttons_probe.start_long_press_count++;
      g_ps_input_buttons_probe.start_long_press_pending = 1UL;
    }
    else
    {
      g_ps_input_buttons_probe.start_pending_drop_count++;
    }
    PS_InputButtons_ArmStartCheck(
      ps_input_start_press_tick + ps_input_start_ship_prep_ticks);
  }
  else if (state == (uint32_t)PS_INPUT_START_STATE_LONG_PRESS)
  {
    g_ps_input_buttons_probe.start_state = PS_INPUT_START_STATE_SHIP_PREP;
    PS_InputButtons_PublishStartPowerEvent(
      PS_INPUT_START_POWER_EVENT_SHIP_PREP,
      now_tick,
      hold_ticks);
    PS_InputButtons_ArmStartCheck(
      ps_input_start_press_tick + ps_input_start_ship_warn_ticks);
  }
  else if (state == (uint32_t)PS_INPUT_START_STATE_SHIP_PREP)
  {
    g_ps_input_buttons_probe.start_state = PS_INPUT_START_STATE_SHIP_WARNING;
    PS_InputButtons_PublishStartPowerEvent(
      PS_INPUT_START_POWER_EVENT_SHIP_WARNING,
      now_tick,
      hold_ticks);
    PS_InputButtons_ArmStartCheck(
      ps_input_start_press_tick + ps_input_start_ship_imminent_ticks);
  }
  else if (state == (uint32_t)PS_INPUT_START_STATE_SHIP_WARNING)
  {
    g_ps_input_buttons_probe.start_state = PS_INPUT_START_STATE_SHIP_IMMINENT;
    PS_InputButtons_PublishStartPowerEvent(
      PS_INPUT_START_POWER_EVENT_SHIP_IMMINENT,
      now_tick,
      hold_ticks);
    PS_InputButtons_DisarmStartCheck();
  }
  else
  {
    PS_InputButtons_DisarmStartCheck();
  }

  if (primask == 0UL)
  {
    __enable_irq();
  }
}

uint32_t PS_InputButtons_ButtonsCheckDue(uint32_t now_tick)
{
  uint32_t due = 0UL;
  uint32_t i;
  uint32_t primask;
  uint32_t state;

  primask = __get_PRIMASK();
  __disable_irq();
  if ((ps_input_button_press_edge_mask != 0UL) ||
      (ps_input_button_release_edge_mask != 0UL))
  {
    due = 1UL;
  }
  else
  {
    for (i = 0UL; i < PS_INPUT_BUTTON_COUNT; ++i)
    {
      state = ps_input_button_state[i];
      if ((state != (uint32_t)PS_INPUT_BUTTON_STATE_RELEASED) &&
          (state != (uint32_t)PS_INPUT_BUTTON_STATE_STUCK) &&
          (ps_input_button_deadline_tick[i] != 0UL) &&
          (PS_InputButtons_TimeReached(now_tick,
                                       ps_input_button_deadline_tick[i]) !=
           0UL))
      {
        due = 1UL;
        break;
      }
    }
  }
  if (primask == 0UL)
  {
    __enable_irq();
  }

  return due;
}

void PS_InputButtons_PollButtons(uint32_t now_tick)
{
  uint32_t edge_press_mask;
  uint32_t edge_release_mask;
  uint32_t i;
  uint32_t live_level[PS_INPUT_BUTTON_COUNT];
  uint32_t mask;
  uint32_t primask;
  uint32_t release_latched;
  uint32_t state;

  primask = __get_PRIMASK();
  __disable_irq();
  edge_press_mask = ps_input_button_press_edge_mask;
  edge_release_mask = ps_input_button_release_edge_mask;
  ps_input_button_press_edge_mask = 0UL;
  ps_input_button_release_edge_mask = 0UL;
  for (i = 0UL; i < PS_INPUT_BUTTON_COUNT; ++i)
  {
    live_level[i] = ps_input_button_raw_level[i];
    mask = 1UL << i;
    g_ps_input_buttons_probe.button_raw_level[i] = live_level[i];
    state = ps_input_button_state[i];

    if ((edge_press_mask & mask) != 0UL)
    {
      if (state == (uint32_t)PS_INPUT_BUTTON_STATE_RELEASED)
      {
        ps_input_button_tap_release_mask &= ~mask;
        PS_InputButtons_SetButtonState(
          i,
          PS_INPUT_BUTTON_STATE_DEBOUNCE_PRESS,
          now_tick + ps_input_button_debounce_press_ticks);
        g_ps_input_buttons_probe.button_debounce_press_count++;
        state = (uint32_t)PS_INPUT_BUTTON_STATE_DEBOUNCE_PRESS;
      }
      else
      {
        g_ps_input_buttons_probe.ignored_edge_count++;
      }
    }

    if ((edge_release_mask & mask) != 0UL)
    {
      if (state == (uint32_t)PS_INPUT_BUTTON_STATE_DEBOUNCE_PRESS)
      {
        ps_input_button_tap_release_mask |= mask;
      }
      else if ((state == (uint32_t)PS_INPUT_BUTTON_STATE_PRESSED) ||
               (state == (uint32_t)PS_INPUT_BUTTON_STATE_HELD) ||
               (state == (uint32_t)PS_INPUT_BUTTON_STATE_REPEAT) ||
               (state == (uint32_t)PS_INPUT_BUTTON_STATE_STUCK))
      {
        PS_InputButtons_StartButtonReleaseDebounce(i, now_tick);
        state = (uint32_t)PS_INPUT_BUTTON_STATE_DEBOUNCE_RELEASE;
      }
      else
      {
        g_ps_input_buttons_probe.ignored_edge_count++;
      }
    }

    if (state == (uint32_t)PS_INPUT_BUTTON_STATE_DEBOUNCE_PRESS)
    {
      if (PS_InputButtons_TimeReached(now_tick,
                                      ps_input_button_deadline_tick[i]) ==
          0UL)
      {
        continue;
      }

      release_latched =
        ((ps_input_button_tap_release_mask & mask) != 0UL) ? 1UL : 0UL;
      if ((live_level[i] != 0UL) || (release_latched != 0UL))
      {
        ps_input_button_press_tick[i] = now_tick;
        g_ps_input_buttons_probe.button_press_tick[i] = now_tick;
        PS_InputButtons_QueueButtonPress(i, now_tick);
        g_ps_input_buttons_probe.last_button_id =
          PS_InputButtons_ButtonForIndex(i);
        g_ps_input_buttons_probe.last_event = PS_INPUT_BUTTON_EVENT_PRESS;
        if (release_latched != 0UL)
        {
          ps_input_button_tap_release_mask &= ~mask;
          ps_input_button_release_tick[i] = now_tick;
          g_ps_input_buttons_probe.button_release_tick[i] = now_tick;
          g_ps_input_buttons_probe.button_release_accept_count++;
          PS_InputButtons_SetButtonState(
            i,
            PS_INPUT_BUTTON_STATE_RELEASED,
            0UL);
        }
        else
        {
          PS_InputButtons_SetButtonState(
            i,
            PS_INPUT_BUTTON_STATE_PRESSED,
            now_tick + ps_input_button_long_press_ticks);
        }
      }
      else
      {
        g_ps_input_buttons_probe.button_bounce_reject_count++;
        ps_input_button_tap_release_mask &= ~mask;
        PS_InputButtons_SetButtonState(
          i,
          PS_INPUT_BUTTON_STATE_RELEASED,
          0UL);
      }
      continue;
    }

    if (state == (uint32_t)PS_INPUT_BUTTON_STATE_PRESSED)
    {
      if (live_level[i] == 0UL)
      {
        PS_InputButtons_StartButtonReleaseDebounce(i, now_tick);
        continue;
      }

      if (PS_InputButtons_TimeReached(now_tick,
                                      ps_input_button_deadline_tick[i]) !=
          0UL)
      {
        g_ps_input_buttons_probe.button_long_count++;
        PS_InputButtons_SetButtonState(
          i,
          PS_INPUT_BUTTON_STATE_HELD,
          ps_input_button_press_tick[i] +
            ps_input_button_repeat_start_ticks);
      }
      continue;
    }

    if (state == (uint32_t)PS_INPUT_BUTTON_STATE_HELD)
    {
      if (live_level[i] == 0UL)
      {
        PS_InputButtons_StartButtonReleaseDebounce(i, now_tick);
        continue;
      }

      if (PS_InputButtons_TimeReached(
            now_tick,
            ps_input_button_press_tick[i] + ps_input_button_stuck_ticks) !=
          0UL)
      {
        g_ps_input_buttons_probe.button_stuck_count++;
        PS_InputButtons_SetButtonState(
          i,
          PS_INPUT_BUTTON_STATE_STUCK,
          0UL);
        continue;
      }

      if (PS_InputButtons_TimeReached(now_tick,
                                      ps_input_button_deadline_tick[i]) !=
          0UL)
      {
        g_ps_input_buttons_probe.button_repeat_count++;
        PS_InputButtons_SetButtonState(
          i,
          PS_INPUT_BUTTON_STATE_REPEAT,
          now_tick + ps_input_button_repeat_period_ticks);
      }
      continue;
    }

    if (state == (uint32_t)PS_INPUT_BUTTON_STATE_REPEAT)
    {
      if (live_level[i] == 0UL)
      {
        PS_InputButtons_StartButtonReleaseDebounce(i, now_tick);
        continue;
      }

      if (PS_InputButtons_TimeReached(
            now_tick,
            ps_input_button_press_tick[i] + ps_input_button_stuck_ticks) !=
          0UL)
      {
        g_ps_input_buttons_probe.button_stuck_count++;
        PS_InputButtons_SetButtonState(
          i,
          PS_INPUT_BUTTON_STATE_STUCK,
          0UL);
        continue;
      }

      if (PS_InputButtons_TimeReached(now_tick,
                                      ps_input_button_deadline_tick[i]) !=
          0UL)
      {
        g_ps_input_buttons_probe.button_repeat_count++;
        PS_InputButtons_SetButtonState(
          i,
          PS_INPUT_BUTTON_STATE_REPEAT,
          now_tick + ps_input_button_repeat_period_ticks);
      }
      continue;
    }

    if (state == (uint32_t)PS_INPUT_BUTTON_STATE_DEBOUNCE_RELEASE)
    {
      if (PS_InputButtons_TimeReached(now_tick,
                                      ps_input_button_deadline_tick[i]) ==
          0UL)
      {
        continue;
      }

      if (live_level[i] == 0UL)
      {
        ps_input_button_release_tick[i] = now_tick;
        g_ps_input_buttons_probe.button_release_tick[i] = now_tick;
        g_ps_input_buttons_probe.button_release_accept_count++;
        ps_input_button_tap_release_mask &= ~mask;
        PS_InputButtons_SetButtonState(
          i,
          PS_INPUT_BUTTON_STATE_RELEASED,
          0UL);
        g_ps_input_buttons_probe.last_button_id =
          PS_InputButtons_ButtonForIndex(i);
        g_ps_input_buttons_probe.last_event = PS_INPUT_BUTTON_EVENT_RELEASE;
      }
      else
      {
        g_ps_input_buttons_probe.button_bounce_reject_count++;
        if (ps_input_button_return_state[i] ==
            (uint32_t)PS_INPUT_BUTTON_STATE_DEBOUNCE_PRESS)
        {
          PS_InputButtons_SetButtonState(
            i,
            PS_INPUT_BUTTON_STATE_DEBOUNCE_PRESS,
            now_tick + ps_input_button_debounce_press_ticks);
        }
        else
        {
          PS_InputButtons_SetButtonState(
            i,
            (ps_input_button_state_t)ps_input_button_return_state[i],
            ps_input_button_return_deadline_tick[i]);
        }
      }
      continue;
    }

    if ((state == (uint32_t)PS_INPUT_BUTTON_STATE_STUCK) &&
        (live_level[i] == 0UL))
    {
      PS_InputButtons_StartButtonReleaseDebounce(i, now_tick);
    }
  }

  if (primask == 0UL)
  {
    __enable_irq();
  }
}

void PS_InputButtons_ProcessRawEdge(ps_input_button_id_t button_id,
                                    uint32_t active,
                                    uint32_t timestamp)
{
  uint32_t index;
  uint32_t mask;
  uint32_t primask;

  if ((button_id < PS_INPUT_BUTTON_ID_A) ||
      (button_id > PS_INPUT_BUTTON_ID_R))
  {
    return;
  }

  PS_InputButtons_PollButtons(timestamp);

  index = (uint32_t)button_id - 1UL;
  mask = 1UL << index;
  active = (active != 0UL) ? 1UL : 0UL;

  primask = __get_PRIMASK();
  __disable_irq();
  ps_input_button_raw_level[index] = active;
  g_ps_input_buttons_probe.button_raw_level[index] = active;
  if (active != 0UL)
  {
    ps_input_button_press_edge_mask |= mask;
  }
  else
  {
    ps_input_button_release_edge_mask |= mask;
  }
  g_ps_input_buttons_probe.raw_edge_process_count++;
  g_ps_input_buttons_probe.raw_edge_last_timestamp = timestamp;
  if (primask == 0UL)
  {
    __enable_irq();
  }

  PS_InputButtons_PollButtons(timestamp);
}

void PS_InputButtons_ReconcileLiveLevels(uint32_t now_tick)
{
  uint32_t i;

  for (i = 0UL; i < PS_INPUT_BUTTON_COUNT; ++i)
  {
    uint32_t live_level = PS_InputButtons_ButtonLiveLevel(i);

    if (live_level != ps_input_button_raw_level[i])
    {
      g_ps_input_buttons_probe.raw_edge_recovery_count++;
      PS_InputButtons_ProcessRawEdge(
        PS_InputButtons_ButtonForIndex(i), live_level, now_tick);
    }
  }
}

uint32_t PS_InputButtons_NextWaitTicks(uint32_t now_tick,
                                      uint32_t maximum_wait_ticks)
{
  uint32_t i;
  uint32_t primask;
  uint32_t wait_ticks = maximum_wait_ticks;

  primask = __get_PRIMASK();
  __disable_irq();
  if ((ps_input_button_press_edge_mask != 0UL) ||
      (ps_input_button_release_edge_mask != 0UL) ||
      (ps_input_start_press_pending != 0UL) ||
      (ps_input_start_release_pending != 0UL))
  {
    wait_ticks = 0UL;
  }

  for (i = 0UL; (i < PS_INPUT_BUTTON_COUNT) && (wait_ticks != 0UL); ++i)
  {
    uint32_t deadline_tick = ps_input_button_deadline_tick[i];
    uint32_t state = ps_input_button_state[i];
    int32_t remaining_ticks;

    if ((state == (uint32_t)PS_INPUT_BUTTON_STATE_RELEASED) ||
        (state == (uint32_t)PS_INPUT_BUTTON_STATE_STUCK) ||
        (deadline_tick == 0UL))
    {
      continue;
    }

    remaining_ticks = (int32_t)(deadline_tick - now_tick);
    if (remaining_ticks <= 0)
    {
      wait_ticks = 0UL;
    }
    else if ((uint32_t)remaining_ticks < wait_ticks)
    {
      wait_ticks = (uint32_t)remaining_ticks;
    }
  }

  if ((wait_ticks != 0UL) && (ps_input_start_armed != 0UL))
  {
    int32_t remaining_ticks =
      (int32_t)(ps_input_start_next_check_tick - now_tick);

    if (remaining_ticks <= 0)
    {
      wait_ticks = 0UL;
    }
    else if ((uint32_t)remaining_ticks < wait_ticks)
    {
      wait_ticks = (uint32_t)remaining_ticks;
    }
  }

  if (primask == 0UL)
  {
    __enable_irq();
  }
  return wait_ticks;
}

uint32_t PS_InputButtons_Stop2Ready(void)
{
  uint32_t i;
  uint32_t primask;
  uint32_t ready = 1UL;

  primask = __get_PRIMASK();
  __disable_irq();
  if ((ps_input_button_press_edge_mask != 0UL) ||
      (ps_input_button_release_edge_mask != 0UL) ||
      (ps_input_button_tap_release_mask != 0UL) ||
      (ps_input_buttons_pending_mask != 0UL) ||
      (ps_input_start_active != 0UL) ||
      (ps_input_start_press_pending != 0UL) ||
      (ps_input_start_release_pending != 0UL) ||
      (ps_input_start_press_event_pending != 0UL) ||
      (ps_input_start_long_event_pending != 0UL) ||
      (ps_input_start_pending_event !=
       (uint32_t)PS_INPUT_START_POWER_EVENT_NONE))
  {
    ready = 0UL;
  }

  for (i = 0UL; (i < PS_INPUT_BUTTON_COUNT) && (ready != 0UL); ++i)
  {
    if ((ps_input_button_state[i] !=
         (uint32_t)PS_INPUT_BUTTON_STATE_RELEASED) ||
        (ps_input_button_raw_level[i] != 0UL))
    {
      ready = 0UL;
    }
  }

  if ((ready != 0UL) &&
      ((PS_InputButtons_ButtonLiveLevel(0UL) != 0UL) ||
       (PS_InputButtons_ButtonLiveLevel(1UL) != 0UL) ||
       (PS_InputButtons_ButtonLiveLevel(2UL) != 0UL) ||
       (PS_InputButtons_ButtonLiveLevel(3UL) != 0UL) ||
       (PS_InputButtons_StartLiveLevel() == 0UL)))
  {
    ready = 0UL;
  }

  if (primask == 0UL)
  {
    __enable_irq();
  }
  return ready;
}

uint32_t PS_InputButtons_TakeLogicalEvent(
  ps_input_button_logical_record_t *record)
{
  uint32_t primask;
  uint32_t pending;
  uint32_t mask = 0UL;
  uint32_t index = 0UL;

  if (record == 0)
  {
    return 0UL;
  }

  record->event = PS_INPUT_BUTTON_LOGICAL_EVENT_NONE;
  record->button_id = PS_INPUT_BUTTON_ID_NONE;
  record->button_mask = 0UL;
  record->timestamp = 0UL;
  record->hold_ticks = 0UL;

  primask = __get_PRIMASK();
  __disable_irq();
  pending = ps_input_buttons_pending_mask;
  if (pending == 0UL)
  {
    if (primask == 0UL)
    {
      __enable_irq();
    }
    return 0UL;
  }

  if ((pending & PS_INPUT_BUTTON_MASK_A) != 0UL)
  {
    mask = PS_INPUT_BUTTON_MASK_A;
    index = 0UL;
    record->button_id = PS_INPUT_BUTTON_ID_A;
  }
  else if ((pending & PS_INPUT_BUTTON_MASK_B) != 0UL)
  {
    mask = PS_INPUT_BUTTON_MASK_B;
    index = 1UL;
    record->button_id = PS_INPUT_BUTTON_ID_B;
  }
  else if ((pending & PS_INPUT_BUTTON_MASK_L) != 0UL)
  {
    mask = PS_INPUT_BUTTON_MASK_L;
    index = 2UL;
    record->button_id = PS_INPUT_BUTTON_ID_L;
  }
  else
  {
    mask = PS_INPUT_BUTTON_MASK_R;
    index = 3UL;
    record->button_id = PS_INPUT_BUTTON_ID_R;
  }

  ps_input_buttons_pending_mask &= ~mask;
  record->event = PS_INPUT_BUTTON_LOGICAL_EVENT_PRESS;
  record->button_mask = mask;
  record->timestamp = ps_input_buttons_timestamp[index];
  g_ps_input_buttons_probe.pending_mask = ps_input_buttons_pending_mask;
  if (primask == 0UL)
  {
    __enable_irq();
  }

  g_ps_input_buttons_probe.last_button_id = (uint32_t)record->button_id;
  g_ps_input_buttons_probe.last_event = PS_INPUT_BUTTON_EVENT_PRESS;
  g_ps_input_buttons_probe.press_count++;
  PS_InputButtons_RecordLogicalEvent(record);
  return 1UL;
}

uint32_t PS_InputButtons_TakePress(ps_input_button_id_t *button_id,
                                   uint32_t *timestamp)
{
  ps_input_button_logical_record_t record;

  if ((button_id == 0) || (timestamp == 0))
  {
    return 0UL;
  }

  if (PS_InputButtons_TakeLogicalEvent(&record) == 0UL)
  {
    return 0UL;
  }

  if (record.event != PS_INPUT_BUTTON_LOGICAL_EVENT_PRESS)
  {
    return 0UL;
  }

  *button_id = record.button_id;
  *timestamp = record.timestamp;
  return 1UL;
}

uint32_t PS_InputButtons_TakeStartPowerEvent(
  ps_input_start_power_event_t *event,
  uint32_t *timestamp,
  uint32_t *hold_ticks)
{
  uint32_t primask;

  if ((event == 0) || (timestamp == 0) || (hold_ticks == 0))
  {
    return 0UL;
  }

  primask = __get_PRIMASK();
  __disable_irq();
  if (ps_input_start_pending_event ==
      (uint32_t)PS_INPUT_START_POWER_EVENT_NONE)
  {
    if (primask == 0UL)
    {
      __enable_irq();
    }
    return 0UL;
  }

  *event = (ps_input_start_power_event_t)ps_input_start_pending_event;
  *timestamp = ps_input_start_pending_timestamp;
  *hold_ticks = ps_input_start_pending_hold_ticks;
  ps_input_start_pending_event = PS_INPUT_START_POWER_EVENT_NONE;
  ps_input_start_pending_timestamp = 0UL;
  ps_input_start_pending_hold_ticks = 0UL;
  g_ps_input_buttons_probe.start_pending_event =
    PS_INPUT_START_POWER_EVENT_NONE;
  g_ps_input_buttons_probe.start_pending_timestamp = 0UL;
  g_ps_input_buttons_probe.start_pending_hold_ticks = 0UL;
  if (primask == 0UL)
  {
    __enable_irq();
  }
  return 1UL;
}

uint32_t PS_InputButtons_TakeStartPress(uint32_t *timestamp,
                                        uint32_t *hold_ticks)
{
  uint32_t primask;

  if ((timestamp == NULL) || (hold_ticks == NULL))
  {
    return 0UL;
  }
  primask = __get_PRIMASK();
  __disable_irq();
  if (ps_input_start_press_event_pending == 0UL)
  {
    if (primask == 0UL)
    {
      __enable_irq();
    }
    return 0UL;
  }
  *timestamp = ps_input_start_press_event_timestamp;
  *hold_ticks = ps_input_start_press_event_hold_ticks;
  ps_input_start_press_event_pending = 0UL;
  ps_input_start_press_event_timestamp = 0UL;
  ps_input_start_press_event_hold_ticks = 0UL;
  g_ps_input_buttons_probe.start_short_press_pending = 0UL;
  if (primask == 0UL)
  {
    __enable_irq();
  }
  return 1UL;
}

uint32_t PS_InputButtons_TakeStartLongPress(uint32_t *timestamp,
                                            uint32_t *hold_ticks)
{
  uint32_t primask;

  if ((timestamp == NULL) || (hold_ticks == NULL))
  {
    return 0UL;
  }
  primask = __get_PRIMASK();
  __disable_irq();
  if (ps_input_start_long_event_pending == 0UL)
  {
    if (primask == 0UL)
    {
      __enable_irq();
    }
    return 0UL;
  }
  *timestamp = ps_input_start_long_event_timestamp;
  *hold_ticks = ps_input_start_long_event_hold_ticks;
  ps_input_start_long_event_pending = 0UL;
  ps_input_start_long_event_timestamp = 0UL;
  ps_input_start_long_event_hold_ticks = 0UL;
  g_ps_input_buttons_probe.start_long_press_pending = 0UL;
  if (primask == 0UL)
  {
    __enable_irq();
  }
  return 1UL;
}
