#include "ps_input_buttons.h"

#include "knobs_autogen.h"

#include "tx_api.h"

#define PS_INPUT_BUTTON_MASK_A (1UL << 0U)
#define PS_INPUT_BUTTON_MASK_B (1UL << 1U)
#define PS_INPUT_BUTTON_MASK_L (1UL << 2U)
#define PS_INPUT_BUTTON_MASK_R (1UL << 3U)
#define PS_INPUT_BUTTON_COUNT  (4UL)


volatile ps_input_buttons_probe_t g_ps_input_buttons_probe;

static volatile uint32_t ps_input_buttons_pending_mask;
static volatile uint32_t ps_input_buttons_timestamp[PS_INPUT_BUTTON_COUNT];
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
static uint32_t ps_input_start_long_ticks;
static uint32_t ps_input_start_ship_prep_ticks;
static uint32_t ps_input_start_ship_warn_ticks;
static uint32_t ps_input_start_ship_imminent_ticks;

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
  uint32_t was_shipping_intent;

  hold_ticks = ((ps_input_start_active != 0UL) &&
                (ps_input_start_press_tick != 0UL)) ?
    (now_tick - ps_input_start_press_tick) : 0UL;
  was_shipping_intent =
    (g_ps_input_buttons_probe.start_state >=
     (uint32_t)PS_INPUT_START_STATE_SHIP_PREP) ? 1UL : 0UL;

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
  }
}

void PS_InputButtons_RecordExti(uint16_t gpio_pin, GPIO_PinState level)
{
  ps_input_button_id_t button_id;
  uint32_t mask;

  if (PS_InputButtons_RecordStartExti(gpio_pin, level) != 0UL)
  {
    return;
  }

  mask = PS_InputButtons_MaskForPin(gpio_pin, &button_id);

  g_ps_input_buttons_probe.isr_edge_count++;
  g_ps_input_buttons_probe.last_pin = gpio_pin;
  g_ps_input_buttons_probe.last_button_id = button_id;
  g_ps_input_buttons_probe.last_level = (level == GPIO_PIN_SET) ? 1UL : 0UL;
  g_ps_input_buttons_probe.last_tick = HAL_GetTick();

  if ((mask == 0UL) || (level != GPIO_PIN_SET))
  {
    g_ps_input_buttons_probe.ignored_edge_count++;
    return;
  }

  ps_input_buttons_timestamp[(uint32_t)button_id - 1UL] =
    g_ps_input_buttons_probe.last_tick;
  ps_input_buttons_pending_mask |= mask;
  g_ps_input_buttons_probe.pending_mask = ps_input_buttons_pending_mask;
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

uint32_t PS_InputButtons_TakePress(ps_input_button_id_t *button_id,
                                   uint32_t *timestamp)
{
  uint32_t primask;
  uint32_t pending;
  uint32_t mask = 0UL;
  uint32_t index = 0UL;

  if ((button_id == 0) || (timestamp == 0))
  {
    return 0UL;
  }

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
    *button_id = PS_INPUT_BUTTON_ID_A;
  }
  else if ((pending & PS_INPUT_BUTTON_MASK_B) != 0UL)
  {
    mask = PS_INPUT_BUTTON_MASK_B;
    index = 1UL;
    *button_id = PS_INPUT_BUTTON_ID_B;
  }
  else if ((pending & PS_INPUT_BUTTON_MASK_L) != 0UL)
  {
    mask = PS_INPUT_BUTTON_MASK_L;
    index = 2UL;
    *button_id = PS_INPUT_BUTTON_ID_L;
  }
  else
  {
    mask = PS_INPUT_BUTTON_MASK_R;
    index = 3UL;
    *button_id = PS_INPUT_BUTTON_ID_R;
  }

  ps_input_buttons_pending_mask &= ~mask;
  *timestamp = ps_input_buttons_timestamp[index];
  g_ps_input_buttons_probe.pending_mask = ps_input_buttons_pending_mask;
  if (primask == 0UL)
  {
    __enable_irq();
  }

  g_ps_input_buttons_probe.last_button_id = *button_id;
  g_ps_input_buttons_probe.last_event = PS_INPUT_BUTTON_EVENT_PRESS;
  g_ps_input_buttons_probe.press_count++;
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