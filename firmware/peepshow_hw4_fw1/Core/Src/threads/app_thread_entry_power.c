/* Thread entry implementation for App_ThreadX runtime. */

#define APP_POWER_STOP2_DECISION_NONE            (0UL)
#define APP_POWER_STOP2_DECISION_ARMED           (1UL)
#define APP_POWER_STOP2_DECISION_DEFER_CADENCE   (2UL)
#define APP_POWER_STOP2_DECISION_REENTER_STOP    (3UL)
#define APP_POWER_STOP2_DECISION_RESUME_MODE_EXIT (4UL)
#define APP_POWER_STOP2_DECISION_RESUME_REQ      (5UL)
#define APP_POWER_STOP2_DECISION_ABORT           (6UL)
#define APP_POWER_STOP2_TB_MAGIC                 (0x50535444UL)
#define APP_POWER_STOP2_TB_BKP_MAGIC_REG         RTC_BKP_DR20
#define APP_POWER_STOP2_TB_BKP_SAMPLES_REG       RTC_BKP_DR21
#define APP_POWER_STOP2_TB_BKP_LAST_HAL_DT_REG   RTC_BKP_DR22
#define APP_POWER_STOP2_TB_BKP_LAST_TX_DT_REG    RTC_BKP_DR23
#define APP_POWER_STOP2_TB_BKP_LAST_DIFF_REG     RTC_BKP_DR24
#define APP_POWER_STOP2_TB_BKP_MAX_DIFF_REG      RTC_BKP_DR25
#define APP_POWER_STOP2_TB_BKP_LAST_WAKE_REG     RTC_BKP_DR26

static ULONG g_power_stop2_next_entry_tick;
static ULONG g_power_stop2_decision_last;
static ULONG g_power_stop2_decision_reenter_count;
static ULONG g_power_stop2_decision_resume_count;
static ULONG g_power_stop2_decision_defer_count;
static ULONG g_power_stop2_decision_abort_count;
static ULONG g_power_stop2_pet_active_window;
static ULONG g_power_stop2_pet_active_until_tick;
static ULONG g_power_stop_select_timeout_deadline_wake;
volatile ULONG g_power_stop2_resume_grace_applied_count __attribute__((used));
volatile ULONG g_power_stop2_resume_grace_last_ticks __attribute__((used));

/* Power helpers. */
static UINT AppPowerFlagsUpdate(ULONG set_mask, ULONG clear_mask);
static VOID AppPowerPerfClockApplyFail(ULONG stage, HAL_StatusTypeDef hal_status);
static UINT AppPowerPerfRetuneThreadXSysTick(ULONG fail_stage);
static UINT AppPowerPerfRestorePeriphClocks(void);
static UINT AppPowerPerfApplyNormClock(void);
static UINT AppPowerPerfApplyPllClock(ULONG pll_n, ULONG pll_r, ULONG sysclk_mhz);
static VOID AppPowerPerfSetProfile(ULONG next_profile, ULONG now_tick);
static ULONG AppPowerPerfTopProfileCap(void);
static uint32_t AppPowerPerfFlashLatencyForMHz(ULONG sysclk_mhz);
static VOID AppPowerPerfReset(void);
static VOID AppPowerPerfOnModeChange(app_mode_t mode_token, ULONG now_tick);
static VOID AppPowerPerfHandleHint(ULONG present_ticks, ULONG draw_ticks, ULONG hint_meta, ULONG now_tick);
static VOID AppPowerPerfHintPost(ULONG present_ticks, ULONG draw_ticks, ULONG dirty_rows, ULONG full_flush);
static VOID AppPowerResumeOwners(void);
static VOID AppPowerResumeOwnersWithGrace(UINT apply_grace);
static VOID AppPowerBeginQuiesce(void);
static VOID AppPowerCheckStaticInactivity(void);
static UINT AppPowerStopInputExtendsWindow(ULONG source);
static ULONG AppPowerStopWakeTicks(void);
static ULONG AppPowerDurationMsToThreadxTicks(ULONG duration_ms);
static ULONG AppPowerThreadxTicksToMs(ULONG duration_ticks);
static VOID AppPowerStopSelectTimeoutRestart(void);
static UINT AppPowerStopSelectTimeoutExpired(void);
static ULONG AppPowerStopWakePendingConfirmedMask(void);
static VOID AppPowerStop2TimebaseTelemetryPersist(void);
static VOID AppPowerStop2TimebaseTelemetryClear(void);
static VOID AppPowerStop2TimebaseTelemetryInit(void);
static VOID AppPowerStop2TimebaseTelemetryWakeMark(ULONG hal_tick, ULONG tx_tick);
static VOID AppPowerStop2TimebaseTelemetrySleepMark(ULONG hal_tick, ULONG tx_tick, ULONG wake_count);
static UINT AppPowerRtcWakeArm(ULONG wake_ticks);
static VOID AppPowerPetOnWakeTick(ULONG now_tick);
static VOID AppPowerPetApplyAction(app_pet_action_t action_id);
static VOID AppPowerHandleModeSet(app_mode_t mode_token);
static VOID AppPowerStopResumeForInteraction(ULONG now_tick, UINT refresh_select_timeout);
static VOID AppPowerStopPulseDisplay(ULONG now_tick);
static ULONG AppPowerComputeEventWaitTicks(void);
static UINT AppPowerPrepareUsbStorage(void);

/* Input helpers declared in this translation unit (app_threadx.c). */
static uint8_t AppInputReadPhysicalLevel(ULONG source, ULONG *level_out);
static uint8_t AppInputPhysicalPressedFromLevel(ULONG source, ULONG level, ULONG *pressed_out);

static VOID AppPowerStop2SetDecision(ULONG decision)
{
  g_power_stop2_decision_last = decision;
  if (decision == APP_POWER_STOP2_DECISION_REENTER_STOP)
  {
    if (g_power_stop2_decision_reenter_count < 0xFFFFFFFFUL)
    {
      g_power_stop2_decision_reenter_count++;
    }
  }
  else if ((decision == APP_POWER_STOP2_DECISION_RESUME_MODE_EXIT) ||
           (decision == APP_POWER_STOP2_DECISION_RESUME_REQ))
  {
    if (g_power_stop2_decision_resume_count < 0xFFFFFFFFUL)
    {
      g_power_stop2_decision_resume_count++;
    }
  }
  else if (decision == APP_POWER_STOP2_DECISION_DEFER_CADENCE)
  {
    if (g_power_stop2_decision_defer_count < 0xFFFFFFFFUL)
    {
      g_power_stop2_decision_defer_count++;
    }
  }
  else if (decision == APP_POWER_STOP2_DECISION_ABORT)
  {
    if (g_power_stop2_decision_abort_count < 0xFFFFFFFFUL)
    {
      g_power_stop2_decision_abort_count++;
    }
  }
  else
  {
    /* no counter for NONE/ARMED */
  }
}

static VOID AppPowerStop2TimebaseTelemetryPersist(void)
{
  HAL_RTCEx_BKUPWrite(&hrtc, APP_POWER_STOP2_TB_BKP_MAGIC_REG, (uint32_t)APP_POWER_STOP2_TB_MAGIC);
  HAL_RTCEx_BKUPWrite(&hrtc, APP_POWER_STOP2_TB_BKP_SAMPLES_REG, (uint32_t)g_power_stop2_tb_sample_count);
  HAL_RTCEx_BKUPWrite(&hrtc, APP_POWER_STOP2_TB_BKP_LAST_HAL_DT_REG, (uint32_t)g_power_stop2_tb_last_hal_dt);
  HAL_RTCEx_BKUPWrite(&hrtc, APP_POWER_STOP2_TB_BKP_LAST_TX_DT_REG, (uint32_t)g_power_stop2_tb_last_tx_dt);
  HAL_RTCEx_BKUPWrite(&hrtc, APP_POWER_STOP2_TB_BKP_LAST_DIFF_REG, (uint32_t)g_power_stop2_tb_last_abs_diff);
  HAL_RTCEx_BKUPWrite(&hrtc, APP_POWER_STOP2_TB_BKP_MAX_DIFF_REG, (uint32_t)g_power_stop2_tb_max_abs_diff);
  HAL_RTCEx_BKUPWrite(&hrtc, APP_POWER_STOP2_TB_BKP_LAST_WAKE_REG, (uint32_t)g_power_stop2_tb_last_wake_count);
  g_power_stop2_tb_persist_magic = APP_POWER_STOP2_TB_MAGIC;
}

static VOID AppPowerStop2TimebaseTelemetryClear(void)
{
  g_power_stop2_tb_sample_count = 0UL;
  g_power_stop2_tb_last_hal_dt = 0UL;
  g_power_stop2_tb_last_tx_dt = 0UL;
  g_power_stop2_tb_last_abs_diff = 0UL;
  g_power_stop2_tb_max_abs_diff = 0UL;
  g_power_stop2_tb_last_wake_count = 0UL;
  g_power_stop2_tb_prev_hal_tick = 0UL;
  g_power_stop2_tb_prev_tx_tick = 0UL;
  g_power_stop2_tb_prev_valid = 0UL;
  g_power_stop2_tb_persist_load_ok = 1UL;
  AppPowerStop2TimebaseTelemetryPersist();
}

static VOID AppPowerStop2TimebaseTelemetryInit(void)
{
  uint32_t magic;

  g_power_stop2_tb_prev_hal_tick = 0UL;
  g_power_stop2_tb_prev_tx_tick = 0UL;
  g_power_stop2_tb_prev_valid = 0UL;

  magic = HAL_RTCEx_BKUPRead(&hrtc, APP_POWER_STOP2_TB_BKP_MAGIC_REG);
  g_power_stop2_tb_persist_magic = (ULONG)magic;
  if ((ULONG)magic == APP_POWER_STOP2_TB_MAGIC)
  {
    g_power_stop2_tb_sample_count = (ULONG)HAL_RTCEx_BKUPRead(&hrtc, APP_POWER_STOP2_TB_BKP_SAMPLES_REG);
    g_power_stop2_tb_last_hal_dt = (ULONG)HAL_RTCEx_BKUPRead(&hrtc, APP_POWER_STOP2_TB_BKP_LAST_HAL_DT_REG);
    g_power_stop2_tb_last_tx_dt = (ULONG)HAL_RTCEx_BKUPRead(&hrtc, APP_POWER_STOP2_TB_BKP_LAST_TX_DT_REG);
    g_power_stop2_tb_last_abs_diff = (ULONG)HAL_RTCEx_BKUPRead(&hrtc, APP_POWER_STOP2_TB_BKP_LAST_DIFF_REG);
    g_power_stop2_tb_max_abs_diff = (ULONG)HAL_RTCEx_BKUPRead(&hrtc, APP_POWER_STOP2_TB_BKP_MAX_DIFF_REG);
    g_power_stop2_tb_last_wake_count = (ULONG)HAL_RTCEx_BKUPRead(&hrtc, APP_POWER_STOP2_TB_BKP_LAST_WAKE_REG);
    g_power_stop2_tb_persist_load_ok = 1UL;
    return;
  }

  g_power_stop2_tb_persist_load_ok = 0UL;
  AppPowerStop2TimebaseTelemetryClear();
}

static VOID AppPowerStop2TimebaseTelemetryWakeMark(ULONG hal_tick, ULONG tx_tick)
{
  g_power_stop2_tb_prev_hal_tick = hal_tick;
  g_power_stop2_tb_prev_tx_tick = tx_tick;
  g_power_stop2_tb_prev_valid = 1UL;
}

static VOID AppPowerStop2TimebaseTelemetrySleepMark(ULONG hal_tick, ULONG tx_tick, ULONG wake_count)
{
  if (g_power_stop2_tb_prev_valid != 0UL)
  {
    ULONG hal_dt = (hal_tick - g_power_stop2_tb_prev_hal_tick);
    ULONG tx_dt_ticks = (tx_tick - g_power_stop2_tb_prev_tx_tick);
    ULONG tx_dt_ms = AppPowerThreadxTicksToMs(tx_dt_ticks);
    ULONG abs_diff = (hal_dt >= tx_dt_ms) ? (hal_dt - tx_dt_ms) : (tx_dt_ms - hal_dt);

    g_power_stop2_tb_last_hal_dt = hal_dt;
    g_power_stop2_tb_last_tx_dt = tx_dt_ms;
    g_power_stop2_tb_last_abs_diff = abs_diff;
    g_power_stop2_tb_last_wake_count = wake_count;
    if (g_power_stop2_tb_sample_count < 0xFFFFFFFFUL)
    {
      g_power_stop2_tb_sample_count++;
    }
    if (abs_diff > g_power_stop2_tb_max_abs_diff)
    {
      g_power_stop2_tb_max_abs_diff = abs_diff;
    }
    AppPowerStop2TimebaseTelemetryPersist();
  }
}

static UINT AppPowerStop2EntryDue(ULONG now_tick, ULONG due_tick)
{
  /* Signed-delta compare keeps wraparound-safe tick ordering. */
  return ((LONG)(now_tick - due_tick) >= 0L) ? 1U : 0U;
}

static ULONG AppPowerComputeEventWaitTicks(void)
{
  ULONG wait_ticks = KNOB_RTOS_POWER_WAIT_TICKS;
  ULONG mode_flags;
  ULONG power_flags;
  ULONG now_tick;
  ULONG deadline_tick = 0UL;
  ULONG candidate_tick;
  uint8_t have_deadline = 0U;

  /*
   * Keep fixed wait during quiesce ACK collection because elapsed accounting
   * below assumes KNOB_RTOS_POWER_WAIT_TICKS cadence.
   */
  if ((g_power_quiesce_wait_active != 0UL) && (g_power_pending_ack_mask != 0UL))
  {
    return wait_ticks;
  }

  mode_flags = (g_eg_mode.tx_event_flags_group_current & APP_MODE_FLAGS_ALL);
  if (((mode_flags & APP_MODE_FLAG_STOP) == 0UL) || (g_power_stop2_armed == 0UL))
  {
    return wait_ticks;
  }

  power_flags = (g_eg_power.tx_event_flags_group_current & APP_POWER_FLAGS_ALL);
  now_tick = tx_time_get();

  if ((g_power_stop2_pet_active_window != 0UL) &&
      (g_power_stop2_pet_active_until_tick != 0UL))
  {
    deadline_tick = g_power_stop2_pet_active_until_tick;
    have_deadline = 1U;
  }

  if ((g_power_quiesce_wait_active == 0UL) &&
      (g_power_pending_ack_mask == 0UL) &&
      ((power_flags & APP_POWER_FLAG_QUIESCED) != 0UL) &&
      ((power_flags & APP_POWER_FLAG_RUNNING) == 0UL))
  {
    candidate_tick = g_power_stop2_next_entry_tick;
    if ((have_deadline == 0U) || ((LONG)(candidate_tick - deadline_tick) < 0L))
    {
      deadline_tick = candidate_tick;
      have_deadline = 1U;
    }
  }

  if (have_deadline == 0U)
  {
    return wait_ticks;
  }

  if (AppPowerStop2EntryDue(now_tick, deadline_tick) != 0U)
  {
    /*
     * Avoid zero-wait spin here: it can starve wake/promotion event handling
     * and collapse STOP interaction windows.
     */
    return 1UL;
  }

  candidate_tick = (deadline_tick - now_tick);
  if ((candidate_tick > 0UL) && (candidate_tick < wait_ticks))
  {
    wait_ticks = candidate_tick;
  }
  return wait_ticks;
}

static ULONG AppPowerDurationToThreadxTicks(ULONG duration_ticks)
{
  /*
   * Inactivity knobs are authored in KNOB_RTOS_TICK_HZ domain for UI tuning
   * consistency; convert once to actual ThreadX timer ticks for comparisons.
   */
  const ULONG src_hz = (ULONG)KNOB_RTOS_TICK_HZ;
  const ULONG dst_hz = (ULONG)TX_TIMER_TICKS_PER_SECOND;
  unsigned long long scaled;
  ULONG converted;

  if (duration_ticks == 0UL)
  {
    return 0UL;
  }
  if ((src_hz == 0UL) || (dst_hz == 0UL))
  {
    return duration_ticks;
  }

  scaled = ((unsigned long long)duration_ticks * (unsigned long long)dst_hz) +
           (unsigned long long)(src_hz - 1UL);
  converted = (ULONG)(scaled / (unsigned long long)src_hz);
  if (converted == 0UL)
  {
    converted = 1UL;
  }
  return converted;
}

static ULONG AppPowerDurationMsToThreadxTicks(ULONG duration_ms)
{
  const ULONG dst_hz = (ULONG)TX_TIMER_TICKS_PER_SECOND;
  unsigned long long scaled;
  ULONG converted;

  if (duration_ms == 0UL)
  {
    return 0UL;
  }
  if (dst_hz == 0UL)
  {
    return duration_ms;
  }

  scaled = ((unsigned long long)duration_ms * (unsigned long long)dst_hz) + 999ULL;
  converted = (ULONG)(scaled / 1000ULL);
  if (converted == 0UL)
  {
    converted = 1UL;
  }
  return converted;
}

static ULONG AppPowerThreadxTicksToMs(ULONG duration_ticks)
{
  const ULONG src_hz = (ULONG)TX_TIMER_TICKS_PER_SECOND;
  unsigned long long scaled;
  ULONG converted;

  if (duration_ticks == 0UL)
  {
    return 0UL;
  }
  if (src_hz == 0UL)
  {
    return duration_ticks;
  }

  scaled = ((unsigned long long)duration_ticks * 1000ULL) + ((unsigned long long)src_hz / 2ULL);
  converted = (ULONG)(scaled / (unsigned long long)src_hz);
  return converted;
}

static ULONG AppPowerClampPct(LONG value)
{
  if (value <= 0L)
  {
    return 0UL;
  }
  if (value >= 100L)
  {
    return 100UL;
  }
  return (ULONG)value;
}

static UINT AppPowerStopInputExtendsWindow(ULONG source)
{
  switch (source)
  {
    case APP_INPUT_SOURCE_BTN_A:
    case APP_INPUT_SOURCE_BTN_B:
    case APP_INPUT_SOURCE_BTN_L:
    case APP_INPUT_SOURCE_BTN_R:
      return 1U;
    default:
      return 0U;
  }
}

static VOID AppPowerCheckStaticInactivity(void)
{
  ULONG now_tick;
  ULONG mode_flags;
  ULONG static_timeout_ticks;
  ULONG realtime_timeout_ticks;

  now_tick = tx_time_get();
  mode_flags = (g_eg_mode.tx_event_flags_group_current & APP_MODE_FLAGS_ALL);
  static_timeout_ticks = AppPowerDurationToThreadxTicks((ULONG)KNOB_RTOS_POWER_STATIC_INACTIVITY_TICKS);
  realtime_timeout_ticks = AppPowerDurationToThreadxTicks((ULONG)KNOB_RTOS_POWER_REALTIME_INACTIVITY_TICKS);
  if ((mode_flags & APP_MODE_FLAG_STATIC) != 0UL)
  {
    if ((static_timeout_ticks > 0UL) &&
        (AppPowerStop2EntryDue(now_tick, g_power_last_input_tick + static_timeout_ticks) != 0U))
    {
      AppPowerHandleModeSet(APP_MODE_STOP);
    }
    return;
  }

  if ((mode_flags & APP_MODE_FLAG_REALTIME) != 0UL)
  {
    if ((realtime_timeout_ticks > 0UL) &&
        (AppPowerStop2EntryDue(now_tick, g_power_last_input_tick + realtime_timeout_ticks) != 0U))
    {
      AppPowerHandleModeSet(APP_MODE_STOP);
    }
    return;
  }
}

static ULONG AppPowerStopWakeTicks(void)
{
  ULONG wake_ticks;

  if (g_power_stop_select_active != 0UL)
  {
    wake_ticks = (ULONG)KNOB_RTOS_POWER_STOP_SELECT_RTC_WAKE_TICKS;
  }
  else
  {
    wake_ticks = (ULONG)KNOB_RTOS_POWER_STOP_RTC_WAKE_TICKS;
  }

  if (wake_ticks == 0UL)
  {
    wake_ticks = KNOB_RTOS_POWER_WAIT_TICKS;
  }
  if (wake_ticks == 0UL)
  {
    wake_ticks = 1UL;
  }
  return wake_ticks;
}

static VOID AppPowerStopSelectTimeoutRestart(void)
{
  ULONG timeout_ticks;
  ULONG wake_ticks;
  ULONG wake_beats;

  timeout_ticks = (ULONG)KNOB_RTOS_POWER_STOP_SELECT_TIMEOUT_TICKS;
  if (timeout_ticks == 0UL)
  {
    g_power_stop_select_timeout_deadline_wake = 0UL;
    return;
  }

  wake_ticks = (ULONG)KNOB_RTOS_POWER_STOP_SELECT_RTC_WAKE_TICKS;
  if (wake_ticks == 0UL)
  {
    wake_ticks = (ULONG)KNOB_RTOS_POWER_STOP_RTC_WAKE_TICKS;
    if (wake_ticks == 0UL)
    {
      wake_ticks = 1UL;
    }
  }

  wake_beats = (timeout_ticks + wake_ticks - 1UL) / wake_ticks;
  if (wake_beats == 0UL)
  {
    wake_beats = 1UL;
  }

  g_power_stop_select_timeout_deadline_wake = g_pet_wake_count + wake_beats;
}

static UINT AppPowerStopSelectTimeoutExpired(void)
{
  if ((g_power_stop_select_active == 0UL) ||
      ((ULONG)KNOB_RTOS_POWER_STOP_SELECT_TIMEOUT_TICKS == 0UL) ||
      (g_power_stop_select_timeout_deadline_wake == 0UL))
  {
    return 0U;
  }

  return AppPowerStop2EntryDue(g_pet_wake_count, g_power_stop_select_timeout_deadline_wake);
}

static ULONG AppPowerStopWakePendingConfirmedMask(void)
{
  ULONG pending_mask = g_input_stop_wake_pending_mask;
  ULONG confirmed_mask = 0UL;
  ULONG source;

  if (pending_mask == 0UL)
  {
    return 0UL;
  }

  for (source = APP_INPUT_SOURCE_BTN_A; source <= APP_INPUT_SOURCE_BTN_R; source++)
  {
    ULONG bit = AppInputSourceBit(source);
    ULONG level = 0UL;
    ULONG pressed_now = 0UL;

    if ((bit == 0UL) || ((pending_mask & bit) == 0UL))
    {
      continue;
    }

    if ((AppInputReadPhysicalLevel(source, &level) != 0U) &&
        (AppInputPhysicalPressedFromLevel(source, level, &pressed_now) != 0U) &&
        (pressed_now != 0UL))
    {
      confirmed_mask |= bit;
    }
    else
    {
      g_input_stop_wake_pending_tick[source] = 0UL;
    }
  }

  /* Drop stale/noisy pending edges so they cannot arm select wake cadence. */
  g_input_stop_wake_pending_mask = confirmed_mask;
  return confirmed_mask;
}

static UINT AppPowerRtcWakeArm(ULONG wake_ticks)
{
  HAL_StatusTypeDef hal_status;
  uint32_t rtc_counts;

  rtc_counts = (uint32_t)(((wake_ticks * 2048UL) + ((ULONG)TX_TIMER_TICKS_PER_SECOND - 1UL)) / (ULONG)TX_TIMER_TICKS_PER_SECOND);
  if (rtc_counts == 0UL)
  {
    rtc_counts = 1UL;
  }
  if (rtc_counts > 0xFFFFUL)
  {
    rtc_counts = 0xFFFFUL;
  }

  (void)HAL_RTCEx_DeactivateWakeUpTimer(&hrtc);
  hal_status = HAL_RTCEx_SetWakeUpTimer_IT(&hrtc, rtc_counts, RTC_WAKEUPCLOCK_RTCCLK_DIV16, 0U);
  return (hal_status == HAL_OK) ? TX_SUCCESS : TX_NOT_DONE;
}

typedef enum
{
  APP_PET_FSM_EVT_NONE = 0U,
  APP_PET_FSM_EVT_TICK = 1U,
  APP_PET_FSM_EVT_FEED = 2U,
  APP_PET_FSM_EVT_PLAY = 3U,
  APP_PET_FSM_EVT_REST = 4U
} app_pet_fsm_evt_t;

static ULONG AppPowerPetFsmNextState(ULONG state, app_pet_fsm_evt_t evt)
{
  switch (evt)
  {
    case APP_PET_FSM_EVT_FEED:
      return APP_PET_STATE_FEEDING;
    case APP_PET_FSM_EVT_PLAY:
      return APP_PET_STATE_PLAYING;
    case APP_PET_FSM_EVT_REST:
      return APP_PET_STATE_RESTING;
    case APP_PET_FSM_EVT_TICK:
      if ((state == APP_PET_STATE_FEEDING) ||
          (state == APP_PET_STATE_PLAYING) ||
          (state == APP_PET_STATE_RESTING) ||
          (state == APP_PET_STATE_SLEEP))
      {
        return APP_PET_STATE_IDLE;
      }
      return APP_PET_STATE_IDLE;
    case APP_PET_FSM_EVT_NONE:
    default:
      return state;
  }
}

static app_pet_fsm_evt_t AppPowerPetFsmEventFromAction(app_pet_action_t action_id)
{
  switch (action_id)
  {
    case APP_PET_ACTION_FEED:
      return APP_PET_FSM_EVT_FEED;
    case APP_PET_ACTION_PLAY:
      return APP_PET_FSM_EVT_PLAY;
    case APP_PET_ACTION_REST:
      return APP_PET_FSM_EVT_REST;
    case APP_PET_ACTION_NONE:
    default:
      return APP_PET_FSM_EVT_NONE;
  }
}

static VOID AppPowerPetOnWakeTick(ULONG now_tick)
{
  ULONG state_before = g_pet_state;
  LONG hunger = (LONG)g_pet_hunger_pct;
  LONG energy = (LONG)g_pet_energy_pct;
  LONG mood = (LONG)g_pet_mood_pct;

  if (g_pet_tick_count < 0xFFFFFFFFUL)
  {
    g_pet_tick_count++;
  }
  if (g_pet_wake_count < 0xFFFFFFFFUL)
  {
    g_pet_wake_count++;
  }

  switch (state_before)
  {
    case APP_PET_STATE_FEEDING:
      hunger -= 8L;
      mood += 2L;
      break;

    case APP_PET_STATE_PLAYING:
      hunger += 2L;
      energy -= 6L;
      mood += 4L;
      break;

    case APP_PET_STATE_RESTING:
      energy += 5L;
      mood += 1L;
      break;

    case APP_PET_STATE_SLEEP:
      energy += 2L;
      mood += 1L;
      break;

    case APP_PET_STATE_IDLE:
    default:
      hunger += 1L;
      energy -= 1L;
      if (hunger >= 85L)
      {
        mood -= 1L;
      }
      else if (energy >= 60L)
      {
        mood += 1L;
      }
      break;
  }

  g_pet_state = AppPowerPetFsmNextState(state_before, APP_PET_FSM_EVT_TICK);
  g_pet_hunger_pct = AppPowerClampPct(hunger);
  g_pet_energy_pct = AppPowerClampPct(energy);
  g_pet_mood_pct = AppPowerClampPct(mood);
  g_power_last_input_tick = now_tick;
  (void)AppRetainedStateSavePet();
}

static VOID AppPowerPetApplyAction(app_pet_action_t action_id)
{
  app_pet_fsm_evt_t evt;

  g_pet_last_action = (ULONG)action_id;
  evt = AppPowerPetFsmEventFromAction(action_id);
  if (evt != APP_PET_FSM_EVT_NONE)
  {
    g_pet_state = AppPowerPetFsmNextState(g_pet_state, evt);
  }

  switch (action_id)
  {
    case APP_PET_ACTION_FEED:
    case APP_PET_ACTION_PLAY:
    case APP_PET_ACTION_REST:
      break;
    case APP_PET_ACTION_NONE:
      g_power_stop_select_active = 0UL;
      g_power_stop_select_last_input_tick = tx_time_get();
      g_power_stop_select_timeout_deadline_wake = 0UL;
      break;
    default:
      break;
  }

  (void)AppRetainedStateSavePet();
}

static VOID AppPowerHandleModeSet(app_mode_t mode_token)
{
  ULONG mode_flags_before = (g_eg_mode.tx_event_flags_group_current & APP_MODE_FLAGS_ALL);
  UINT need_resume_grace = ((mode_flags_before & APP_MODE_FLAG_STOP) != 0UL) ? 1U : 0U;

  if (((mode_flags_before & APP_MODE_FLAG_FLASHING) != 0UL) && (mode_token != APP_MODE_FLASHING))
  {
    /*
     * On FLASHING exit, disconnect first and allow a short host-side
     * settle window before hard controller stop to reduce Explorer hangs.
     */
    (void)AppUsbDeviceStopWithGrace((ULONG)KNOB_RTOS_POWER_WAIT_TICKS);
    (void)AppStorageReqPost(APP_STORAGE_REQ_FILEX_UNMOUNT, 0UL);
  }

  if (mode_token != APP_MODE_REALTIME)
  {
    g_power_perf_audio_boost_active = 0UL;
  }

  if (AppSetModeFlag(mode_token) != TX_SUCCESS)
  {
    return;
  }

  AppPowerPerfOnModeChange(mode_token, tx_time_get());
  (void)AppDisplayCmdPost(APP_DISPLAY_CMD_SET_MODE, (ULONG)mode_token);
  if (mode_token != APP_MODE_FLASHING)
  {
    (void)AppDisplayCmdPost(APP_DISPLAY_CMD_PRESENT, 0UL);
  }
  (void)AppSensorReqPost(APP_SENSOR_REQ_MODE_CHANGED, (ULONG)mode_token, 0UL, 0UL);

  if (mode_token == APP_MODE_STOP)
  {
    g_power_stop2_armed = 1UL;
    g_power_stop2_pet_active_window = 0UL;
    g_power_stop2_pet_active_until_tick = 0UL;
    g_power_stop_select_active = 0UL;
    g_power_stop_select_last_input_tick = 0UL;
    g_power_stop_select_timeout_deadline_wake = 0UL;
    g_power_stop2_next_entry_tick = tx_time_get();
    g_power_stop2_last_error = 0L;
    if (g_pet_state == APP_PET_STATE_SLEEP)
    {
      g_pet_state = APP_PET_STATE_IDLE;
      (void)AppRetainedStateSavePet();
    }
    AppPowerStop2SetDecision(APP_POWER_STOP2_DECISION_ARMED);
    AppPowerBeginQuiesce();
    return;
  }

  g_power_stop2_armed = 0UL;
  g_power_stop2_pet_active_window = 0UL;
  g_power_stop2_pet_active_until_tick = 0UL;
  g_power_stop_select_active = 0UL;
  g_power_stop_select_last_input_tick = 0UL;
  g_power_stop_select_timeout_deadline_wake = 0UL;

  if (mode_token == APP_MODE_FLASHING)
  {
    /*
     * FLASHING isolation: close FileX ownership first, then quiesce owners.
     * Ordering is preserved by thStorage queue FIFO.
     */
    (void)AppStorageReqPost(APP_STORAGE_REQ_FILEX_UNMOUNT, 0UL);
    AppPowerBeginQuiesce();
    return;
  }

  if ((g_eg_power.tx_event_flags_group_current & APP_POWER_FLAG_RUNNING) == 0UL)
  {
    AppPowerStop2SetDecision(APP_POWER_STOP2_DECISION_RESUME_MODE_EXIT);
    AppPowerResumeOwnersWithGrace(need_resume_grace);
  }

  if ((mode_token == APP_MODE_STATIC) || (mode_token == APP_MODE_REALTIME))
  {
    g_power_last_input_tick = tx_time_get();
  }
}

static VOID AppPowerStopResumeForInteraction(ULONG now_tick, UINT refresh_select_timeout)
{
  ULONG active_ticks = (ULONG)KNOB_RTOS_POWER_STOP_ACTIVE_TICKS;

  if (refresh_select_timeout != 0U)
  {
    g_power_stop_select_active = 1UL;
    g_power_stop_select_last_input_tick = now_tick;
    AppPowerStopSelectTimeoutRestart();
  }

  AppPowerResumeOwners();
  g_power_stop2_pet_active_window = 1UL;
  g_power_stop2_pet_active_until_tick = now_tick + active_ticks;
  AppPowerStop2SetDecision(APP_POWER_STOP2_DECISION_REENTER_STOP);
}

static VOID AppPowerStopPulseDisplay(ULONG now_tick)
{
  ULONG active_ticks = (ULONG)KNOB_RTOS_POWER_STOP_ACTIVE_TICKS;

  /*
   * Timer-only STOP wake should not bring storage, audio, sensors, and input
   * fully back up. Keep them quiesced and only post the display present pulse.
   * Re-arm STOP2 for the short active window rather than the full RTC cadence.
   */
  (void)AppDisplayCmdPost(APP_DISPLAY_CMD_PRESENT, 0UL);
  g_power_stop2_next_entry_tick = now_tick + active_ticks;
  g_power_stop2_pet_active_window = 1UL;
  g_power_stop2_pet_active_until_tick = now_tick + active_ticks;
  AppPowerStop2SetDecision(APP_POWER_STOP2_DECISION_REENTER_STOP);
}

static UINT AppPowerPrepareUsbStorage(void)
{
  UINT status;
  ULONG media_status = 1UL;
  ULONG block_length = App_StorageReq_UsbMscGetMediaBlockLength();
  uint8_t probe_block[512] = {0U};

  if (block_length > (ULONG)sizeof(probe_block))
  {
    return TX_NOT_DONE;
  }

  status = App_StorageReq_UsbMscStatus(0UL, &media_status);
  if ((status == TX_SUCCESS) && (media_status == 0UL))
  {
    status = App_StorageReq_UsbMscRead(0UL, 1UL, probe_block, &media_status);
    if ((status == TX_SUCCESS) && (media_status == 0UL))
    {
      return TX_SUCCESS;
    }
  }

  if (App_StorageReq_FileXFormat() != TX_SUCCESS)
  {
    return TX_NOT_DONE;
  }

  status = App_StorageReq_UsbMscStatus(0UL, &media_status);
  if ((status == TX_SUCCESS) && (media_status == 0UL))
  {
    status = App_StorageReq_UsbMscRead(0UL, 1UL, probe_block, &media_status);
    if ((status == TX_SUCCESS) && (media_status == 0UL))
    {
      return TX_SUCCESS;
    }
  }

  return TX_NOT_DONE;
}

static VOID AppPowerResumeOwners(void)
{
  g_power_stop2_next_entry_tick = 0UL;
  g_power_stop2_pet_active_window = 0UL;
  g_power_stop2_pet_active_until_tick = 0UL;
  g_power_pending_ack_mask = 0UL;
  g_power_quiesce_wait_active = 0UL;
  g_power_quiesce_wait_elapsed_ticks = 0UL;
  (void)AppPowerFlagsUpdate((APP_POWER_FLAG_RESUME_REQ | APP_POWER_FLAG_RUNNING),
                            (APP_POWER_FLAG_QUIESCE_REQ | APP_POWER_FLAG_QUIESCED | APP_POWER_FLAG_QUIESCE_TIMEOUT));
  (void)AppDisplayCmdPost(APP_DISPLAY_CMD_RESUME, 0UL);
  (void)AppStorageReqPost(APP_STORAGE_REQ_RESUME, 0UL);
  (void)AppInputCmdPost(APP_INPUT_CMD_RESUME, 0UL);
  (void)AppAudioCmdPost(APP_AUDIO_CMD_RESUME, 0UL);
  (void)AppSensorReqPost(APP_SENSOR_REQ_RESUME, 0UL, 0UL, 0UL);
}

static VOID AppPowerResumeOwnersWithGrace(UINT apply_grace)
{
  g_power_stop2_resume_grace_last_ticks = 0UL;
  if ((apply_grace != 0U) && (KNOB_RTOS_POWER_STOP2_RESUME_GRACE_TICKS > 0UL))
  {
    g_power_stop2_resume_grace_last_ticks = KNOB_RTOS_POWER_STOP2_RESUME_GRACE_TICKS;
    if (g_power_stop2_resume_grace_applied_count < 0xFFFFFFFFUL)
    {
      g_power_stop2_resume_grace_applied_count++;
    }
    tx_thread_sleep(KNOB_RTOS_POWER_STOP2_RESUME_GRACE_TICKS);
  }
  AppPowerResumeOwners();
}

static VOID AppPowerAbortStop2(LONG error_code)
{
  g_power_stop2_next_entry_tick = 0UL;
  g_power_stop2_pet_active_window = 0UL;
  g_power_stop2_pet_active_until_tick = 0UL;
  AppPowerStop2SetDecision(APP_POWER_STOP2_DECISION_ABORT);
  g_power_stop2_armed = 0UL;
  g_power_stop2_last_error = error_code;
  if (g_power_stop2_abort_count < 0xFFFFFFFFUL)
  {
    g_power_stop2_abort_count++;
  }
}

static VOID AppPowerBeginQuiesce(void)
{
  ULONG mode_flags = (g_eg_mode.tx_event_flags_group_current & APP_MODE_FLAGS_ALL);
  UINT skip_input_quiesce = ((mode_flags & APP_MODE_FLAG_FLASHING) != 0UL) ? 1U : 0U;
  UINT display_status;
  UINT storage_status;
  UINT input_status;
  UINT audio_status;
  UINT sensor_status;

  g_power_pending_ack_mask = APP_POWER_ACK_MASK_ALL;
  if (skip_input_quiesce != 0U)
  {
    g_power_pending_ack_mask &= ~APP_POWER_ACK_SRC_INPUT;
  }
  g_power_quiesce_wait_active = 1UL;
  g_power_quiesce_wait_elapsed_ticks = 0UL;
  (void)AppPowerFlagsUpdate(APP_POWER_FLAG_QUIESCE_REQ,
                            (APP_POWER_FLAG_QUIESCED | APP_POWER_FLAG_RESUME_REQ | APP_POWER_FLAG_RUNNING | APP_POWER_FLAG_QUIESCE_TIMEOUT));

  display_status = AppDisplayCmdPost(APP_DISPLAY_CMD_QUIESCE, 0UL);
  storage_status = AppStorageReqPost(APP_STORAGE_REQ_QUIESCE, 0UL);
  input_status = TX_SUCCESS;
  if (skip_input_quiesce == 0U)
  {
    input_status = AppInputCmdPost(APP_INPUT_CMD_QUIESCE, 0UL);
  }
  else
  {
    (void)AppInputCmdPost(APP_INPUT_CMD_RESUME, 0UL);
  }
  audio_status = AppAudioCmdPost(APP_AUDIO_CMD_QUIESCE, 0UL);
  sensor_status = AppSensorReqPost(APP_SENSOR_REQ_QUIESCE, 0UL, 0UL, 0UL);
  if ((display_status != TX_SUCCESS) || (storage_status != TX_SUCCESS) || (input_status != TX_SUCCESS) || (audio_status != TX_SUCCESS) || (sensor_status != TX_SUCCESS))
  {
    g_power_quiesce_wait_active = 0UL;
    g_power_pending_ack_mask = 0UL;
    (void)AppPowerFlagsUpdate(APP_POWER_FLAG_QUIESCE_TIMEOUT, APP_POWER_FLAG_QUIESCED);
    if (g_power_stop2_armed != 0UL)
    {
      AppPowerAbortStop2(-101L);
    }
  }
}

static VOID AppPowerRunStop2Cycle(void)
{
  ULONG now_tick;
  ULONG mode_flags;
  ULONG wake_ticks;
  ULONG sample_wake_count;
  ULONG pre_sleep_hal_tick;
  ULONG pre_sleep_tx_tick;

  now_tick = tx_time_get();
  if (AppPowerStop2EntryDue(now_tick, g_power_stop2_next_entry_tick) == 0U)
  {
    AppPowerStop2SetDecision(APP_POWER_STOP2_DECISION_DEFER_CADENCE);
    return;
  }

  wake_ticks = AppPowerStopWakeTicks();
  if (AppPowerRtcWakeArm(wake_ticks) != TX_SUCCESS)
  {
    AppPowerAbortStop2(-105L);
    AppPowerResumeOwners();
    return;
  }

  g_power_stop2_last_error = 0L;
  g_power_stop2_last_wusr = (ULONG)PWR->WUSR;
  g_power_stop2_last_sr = (ULONG)PWR->SR;

  __HAL_PWR_CLEAR_FLAG(PWR_FLAG_STOPF);
  __HAL_PWR_CLEAR_FLAG(PWR_WAKEUP_FLAG1);
  __HAL_PWR_CLEAR_FLAG(PWR_WAKEUP_FLAG2);
  __HAL_PWR_CLEAR_FLAG(PWR_WAKEUP_FLAG3);
  __HAL_PWR_CLEAR_FLAG(PWR_WAKEUP_FLAG4);
  __HAL_PWR_CLEAR_FLAG(PWR_WAKEUP_FLAG5);
  __HAL_PWR_CLEAR_FLAG(PWR_WAKEUP_FLAG6);
  __HAL_PWR_CLEAR_FLAG(PWR_WAKEUP_FLAG7);
  __HAL_PWR_CLEAR_FLAG(PWR_WAKEUP_FLAG8);

  if (g_power_stop2_entry_count < 0xFFFFFFFFUL)
  {
    g_power_stop2_entry_count++;
  }

  pre_sleep_hal_tick = (ULONG)HAL_GetTick();
  pre_sleep_tx_tick = tx_time_get();
  sample_wake_count = g_power_stop2_wake_count;
  AppPowerStop2TimebaseTelemetrySleepMark(pre_sleep_hal_tick, pre_sleep_tx_tick, sample_wake_count);

  HAL_SuspendTick();
  HAL_PWREx_EnterSTOP2Mode(PWR_STOPENTRY_WFI);
  HAL_ResumeTick();

  g_power_stop2_last_wusr = (ULONG)PWR->WUSR;
  g_power_stop2_last_sr = (ULONG)PWR->SR;
  if (g_power_stop2_wake_count < 0xFFFFFFFFUL)
  {
    g_power_stop2_wake_count++;
  }
  g_power_stop2_next_entry_tick = tx_time_get() + wake_ticks;

  if (AppPowerPerfApplyNormClock() != TX_SUCCESS)
  {
    AppPowerAbortStop2(-102L);
    /* Fail-safe: force awake STATIC path so input/display remain reachable. */
    AppPowerHandleModeSet(APP_MODE_STATIC);
    return;
  }

  mode_flags = (g_eg_mode.tx_event_flags_group_current & APP_MODE_FLAGS_ALL);
  if (((mode_flags & APP_MODE_FLAG_STOP) == 0UL) || (g_power_stop2_armed == 0UL))
  {
    AppPowerStop2SetDecision(APP_POWER_STOP2_DECISION_RESUME_MODE_EXIT);
    g_power_stop2_armed = 0UL;
    AppPowerResumeOwnersWithGrace(((mode_flags & APP_MODE_FLAG_STOP) == 0UL) ? 1U : 0U);
  }
  else
  {
    ULONG resume_tick = tx_time_get();
    ULONG resume_hal_tick = (ULONG)HAL_GetTick();

    AppPowerStop2TimebaseTelemetryWakeMark(resume_hal_tick, resume_tick);
    AppPowerPetOnWakeTick(resume_tick);

    if (AppPowerStopWakePendingConfirmedMask() != 0UL)
    {
      /* Wake-by-input while quiesced: enter explicit interaction window. */
      AppPowerStopResumeForInteraction(resume_tick, 1U);
      return;
    }

    /*
     * Timer-only wake in STOP: keep non-display owners quiesced and only pulse
     * the display present path before re-entering STOP2.
     */
    AppPowerStopPulseDisplay(resume_tick);
  }
}

static VOID AppPowerThreadEntry(ULONG thread_input)
{
  UINT status;
  app_sys_event_t msg;
  ULONG mode_flags;
  ULONG wait_ticks;

  (void)thread_input;

  for (;;)
  {
    wait_ticks = AppPowerComputeEventWaitTicks();
    status = tx_queue_receive(&g_q_sys_events, &msg, wait_ticks);
    if (status == TX_SUCCESS)
    {
      switch ((app_sys_event_type_t)msg.type)
      {
        case APP_SYS_EVT_MODE_SET:
          AppPowerHandleModeSet((app_mode_t)msg.arg0);
          break;

        case APP_SYS_EVT_QUIESCE_REQ:
          AppPowerBeginQuiesce();
          break;

        case APP_SYS_EVT_RESUME_REQ:
          AppPowerStop2SetDecision(APP_POWER_STOP2_DECISION_RESUME_REQ);
          g_power_stop2_armed = 0UL;
          g_power_stop2_pet_active_window = 0UL;
          g_power_stop2_pet_active_until_tick = 0UL;
          AppPowerResumeOwnersWithGrace(0U);
          break;

        case APP_SYS_EVT_QUIESCE_ACK:
        {
          ULONG ack_mask = (msg.arg0 & APP_POWER_ACK_MASK_ALL);

          if ((g_power_quiesce_wait_active != 0UL) && (ack_mask != 0UL))
          {
            g_power_pending_ack_mask &= ~ack_mask;
          }

          if ((g_power_quiesce_wait_active != 0UL) && (g_power_pending_ack_mask == 0UL))
          {
            g_power_quiesce_wait_active = 0UL;
            g_power_quiesce_wait_elapsed_ticks = 0UL;
            (void)AppPowerFlagsUpdate(APP_POWER_FLAG_QUIESCED, 0UL);
            mode_flags = (g_eg_mode.tx_event_flags_group_current & APP_MODE_FLAGS_ALL);
            if ((g_power_stop2_armed != 0UL) && ((mode_flags & APP_MODE_FLAG_STOP) != 0UL))
            {
              AppPowerRunStop2Cycle();
            }
            else if ((mode_flags & APP_MODE_FLAG_FLASHING) != 0UL)
            {
              if (AppPowerPrepareUsbStorage() == TX_SUCCESS)
              {
                (void)AppUsbDeviceStart();
              }
              else
              {
                g_usb_device_last_error = -5L;
                if (g_usb_device_start_fail_count < 0xFFFFFFFFUL)
                {
                  g_usb_device_start_fail_count++;
                }
              }
            }
          }
          break;
        }

        case APP_SYS_EVT_INPUT_ACTIVITY:
        {
          ULONG now_tick = tx_time_get();
          ULONG mode_flags = (g_eg_mode.tx_event_flags_group_current & APP_MODE_FLAGS_ALL);
          g_power_input_activity_count++;
          g_power_last_input_tick = now_tick;
          if ((mode_flags & APP_MODE_FLAG_STOP) != 0UL)
          {
            if ((AppPowerStopInputExtendsWindow(msg.arg1) != 0U) &&
                ((ULONG)KNOB_RTOS_POWER_STOP_ACTIVE_TICKS > 0UL))
            {
              g_power_stop2_pet_active_window = 1UL;
              g_power_stop2_pet_active_until_tick = now_tick + (ULONG)KNOB_RTOS_POWER_STOP_ACTIVE_TICKS;
            }
            if (AppPowerStopInputExtendsWindow(msg.arg1) != 0U)
            {
              g_power_stop_select_active = 1UL;
              g_power_stop_select_last_input_tick = now_tick;
              AppPowerStopSelectTimeoutRestart();
            }
          }
          else if ((mode_flags & APP_MODE_FLAG_REALTIME) != 0UL)
          {
            ULONG entry_profile = (ULONG)KNOB_POWER_PERF_REALTIME_ENTRY_PROFILE;
            if (entry_profile > APP_POWER_PERF_PROFILE_MAX)
            {
              entry_profile = APP_POWER_PERF_PROFILE_MAX;
            }

            if (g_power_perf_profile_current < entry_profile)
            {
              /* First input after idle should not wait for dwell+hint ramp. */
              g_power_perf_force_up_no_dwell = 1UL;
              AppPowerPerfSetProfile(entry_profile, now_tick);
            }
          }
          break;
        }

        case APP_SYS_EVT_INPUT_MENU:
          g_power_menu_event_count++;
          if ((g_eg_mode.tx_event_flags_group_current & APP_MODE_FLAG_STOP) != 0UL)
          {
            AppPowerHandleModeSet(APP_MODE_STATIC);
          }
          break;

        case APP_SYS_EVT_PERF_HINT:
          g_power_perf_hint_inflight = 0UL;
          if (g_power_perf_hint_rx_count < 0xFFFFFFFFUL)
          {
            g_power_perf_hint_rx_count++;
          }
          AppPowerPerfHandleHint(msg.arg0, msg.arg1, msg.arg2, tx_time_get());
          break;

        case APP_SYS_EVT_PET_ACTION:
          AppPowerPetApplyAction((app_pet_action_t)msg.arg0);
          if (((g_eg_mode.tx_event_flags_group_current & APP_MODE_FLAG_STOP) != 0UL) &&
              (msg.arg1 > 0UL))
          {
            ULONG hold_ticks = AppPowerDurationMsToThreadxTicks(msg.arg1);
            if (hold_ticks > 0UL)
            {
              ULONG hold_until = tx_time_get() + hold_ticks;
              g_power_stop2_pet_active_window = 1UL;
              g_power_stop2_pet_active_until_tick = hold_until;
            }
          }
          break;

        case APP_SYS_EVT_AUDIO_ACTIVE:
          g_power_perf_audio_boost_active = 1UL;
          if ((g_eg_mode.tx_event_flags_group_current & APP_MODE_FLAG_REALTIME) != 0UL)
          {
            AppPowerPerfSetProfile((ULONG)KNOB_POWER_PERF_REALTIME_ENTRY_PROFILE, tx_time_get());
          }
          break;

        case APP_SYS_EVT_AUDIO_INACTIVE:
          g_power_perf_audio_boost_active = 0UL;
          if ((g_eg_mode.tx_event_flags_group_current & APP_MODE_FLAG_REALTIME) != 0UL)
          {
            AppPowerPerfSetProfile(APP_POWER_PERF_PROFILE_NORM, tx_time_get());
          }
          break;

        case APP_SYS_EVT_USB_VBUS_PRESENT:
        {
          ULONG vbus_present = (msg.arg0 != 0UL) ? 1UL : 0UL;
          ULONG vbus_present_now = (g_sensor_pmic_vbus_present != 0UL) ? 1UL : 0UL;
          ULONG mode_flags_now = (g_eg_mode.tx_event_flags_group_current & APP_MODE_FLAGS_ALL);

          /* Ignore stale queued VBUS events if a newer PMIC sample already superseded them. */
          if (vbus_present != vbus_present_now)
          {
            break;
          }

          if (vbus_present_now != 0UL)
          {
            /*
             * Option-A policy: VBUS only arms a user-consent prompt.
             * USB attach/MSC exposure happens only after explicit UI action.
             */
            if (((mode_flags_now & APP_MODE_FLAG_FLASHING) == 0UL) &&
                (g_usb_flash_prompt_prompted_this_vbus == 0UL))
            {
              g_usb_flash_prompt_pending = 1UL;
              g_usb_flash_prompt_prompted_this_vbus = 1UL;
            }
          }
          else
          {
            g_usb_flash_prompt_pending = 0UL;
            g_usb_flash_prompt_prompted_this_vbus = 0UL;
            if ((mode_flags_now & APP_MODE_FLAG_FLASHING) != 0UL)
            {
              AppPowerHandleModeSet(APP_MODE_STATIC);
            }
          }
          break;
        }

        case APP_SYS_EVT_USB_MSC_RECOVER:
        {
          ULONG mode_flags_now = (g_eg_mode.tx_event_flags_group_current & APP_MODE_FLAGS_ALL);
          ULONG vbus_present_now = (g_sensor_pmic_vbus_present != 0UL) ? 1UL : 0UL;
          UINT stop_status;
          UINT start_status = TX_NOT_DONE;

          if (((mode_flags_now & APP_MODE_FLAG_FLASHING) == 0UL) ||
              (vbus_present_now == 0UL) ||
              (g_usb_msc_recover_pending == 0UL))
          {
            break;
          }

          g_usb_msc_recover_pending = 0UL;
          if (g_usb_msc_recover_attempt_count < 0xFFFFFFFFUL)
          {
            g_usb_msc_recover_attempt_count++;
          }

          stop_status = AppUsbDeviceStop();
          if (KNOB_RTOS_POWER_WAIT_TICKS > 0UL)
          {
            tx_thread_sleep(KNOB_RTOS_POWER_WAIT_TICKS);
          }

          mode_flags_now = (g_eg_mode.tx_event_flags_group_current & APP_MODE_FLAGS_ALL);
          vbus_present_now = (g_sensor_pmic_vbus_present != 0UL) ? 1UL : 0UL;
          if (((mode_flags_now & APP_MODE_FLAG_FLASHING) != 0UL) &&
              (vbus_present_now != 0UL))
          {
            start_status = AppUsbDeviceStart();
          }

          g_usb_msc_recover_last_usb_active_after = g_usb_device_active;
          if ((stop_status == TX_SUCCESS) && (start_status == TX_SUCCESS))
          {
            if (g_usb_msc_recover_ok_count < 0xFFFFFFFFUL)
            {
              g_usb_msc_recover_ok_count++;
            }
          }
          else if (g_usb_msc_recover_fail_count < 0xFFFFFFFFUL)
          {
            g_usb_msc_recover_fail_count++;
          }
          break;
        }

        default:
          /* Unknown event type: ignore for forward compatibility. */
          break;
      }

      AppPowerCheckStaticInactivity();
    }
    else if (status == TX_QUEUE_EMPTY)
    {
      ULONG now_tick = tx_time_get();
      ULONG power_flags = (g_eg_power.tx_event_flags_group_current & APP_POWER_FLAGS_ALL);
      mode_flags = (g_eg_mode.tx_event_flags_group_current & APP_MODE_FLAGS_ALL);

      if (((mode_flags & APP_MODE_FLAG_STOP) != 0UL) &&
          (g_power_stop2_armed == 0UL))
      {
        /* Recover from invalid STOP state: STOP mode must always be STOP2-armed. */
        AppPowerHandleModeSet(APP_MODE_STATIC);
        continue;
      }

      if (((mode_flags & APP_MODE_FLAG_STOP) != 0UL) &&
          (AppPowerStopSelectTimeoutExpired() != 0U))
      {
        g_power_stop_select_active = 0UL;
        g_power_stop_select_timeout_deadline_wake = 0UL;
      }

      if ((g_power_stop2_pet_active_window != 0UL) &&
          (AppPowerStop2EntryDue(now_tick, g_power_stop2_pet_active_until_tick) != 0U))
      {
        g_power_stop2_pet_active_window = 0UL;
        g_power_stop2_pet_active_until_tick = 0UL;
        if ((g_power_stop2_armed != 0UL) &&
            (g_power_quiesce_wait_active == 0UL) &&
            ((power_flags & APP_POWER_FLAG_RUNNING) != 0UL) &&
            ((mode_flags & APP_MODE_FLAG_STOP) != 0UL))
        {
          AppPowerBeginQuiesce();
        }
      }

      AppPowerCheckStaticInactivity();
      mode_flags = (g_eg_mode.tx_event_flags_group_current & APP_MODE_FLAGS_ALL);
      power_flags = (g_eg_power.tx_event_flags_group_current & APP_POWER_FLAGS_ALL);

      if (((mode_flags & APP_MODE_FLAG_FLASHING) != 0UL) &&
          (g_usb_msc_recover_pending != 0UL))
      {
        UINT stop_status;
        UINT start_status = TX_NOT_DONE;
        ULONG mode_flags_now;
        ULONG vbus_present_now;

        g_usb_msc_recover_pending = 0UL;
        if (g_usb_msc_recover_attempt_count < 0xFFFFFFFFUL)
        {
          g_usb_msc_recover_attempt_count++;
        }

        stop_status = AppUsbDeviceStop();
        if (KNOB_RTOS_POWER_WAIT_TICKS > 0UL)
        {
          tx_thread_sleep(KNOB_RTOS_POWER_WAIT_TICKS);
        }

        mode_flags_now = (g_eg_mode.tx_event_flags_group_current & APP_MODE_FLAGS_ALL);
        vbus_present_now = (g_sensor_pmic_vbus_present != 0UL) ? 1UL : 0UL;
        if (((mode_flags_now & APP_MODE_FLAG_FLASHING) != 0UL) &&
            (vbus_present_now != 0UL))
        {
          start_status = AppUsbDeviceStart();
        }

        g_usb_msc_recover_last_usb_active_after = g_usb_device_active;
        if ((stop_status == TX_SUCCESS) && (start_status == TX_SUCCESS))
        {
          if (g_usb_msc_recover_ok_count < 0xFFFFFFFFUL)
          {
            g_usb_msc_recover_ok_count++;
          }
        }
        else if (g_usb_msc_recover_fail_count < 0xFFFFFFFFUL)
        {
          g_usb_msc_recover_fail_count++;
        }

        continue;
      }

      if ((g_power_stop2_armed != 0UL) &&
          (g_power_quiesce_wait_active == 0UL) &&
          (g_power_pending_ack_mask == 0UL) &&
          ((mode_flags & APP_MODE_FLAG_STOP) != 0UL) &&
          ((power_flags & APP_POWER_FLAG_QUIESCED) != 0UL) &&
          ((power_flags & APP_POWER_FLAG_RUNNING) == 0UL))
      {
        if (AppPowerStopWakePendingConfirmedMask() != 0UL)
        {
          ULONG resume_tick = tx_time_get();

          /* Input edge arrived after wake handling; promote to interaction now. */
          AppPowerStopResumeForInteraction(resume_tick, 1U);
          continue;
        }

        AppPowerRunStop2Cycle();
      }

      if ((g_power_quiesce_wait_active != 0UL) && (g_power_pending_ack_mask != 0UL))
      {
        if (g_power_quiesce_wait_elapsed_ticks < KNOB_RTOS_POWER_QUIESCE_TIMEOUT_TICKS)
        {
          ULONG remaining = KNOB_RTOS_POWER_QUIESCE_TIMEOUT_TICKS - g_power_quiesce_wait_elapsed_ticks;
          if (remaining <= KNOB_RTOS_POWER_WAIT_TICKS)
          {
            g_power_quiesce_wait_elapsed_ticks = KNOB_RTOS_POWER_QUIESCE_TIMEOUT_TICKS;
          }
          else
          {
            g_power_quiesce_wait_elapsed_ticks += KNOB_RTOS_POWER_WAIT_TICKS;
          }
        }

        if (g_power_quiesce_wait_elapsed_ticks >= KNOB_RTOS_POWER_QUIESCE_TIMEOUT_TICKS)
        {
          g_power_quiesce_wait_active = 0UL;
          g_power_pending_ack_mask = 0UL;
          (void)AppPowerFlagsUpdate(APP_POWER_FLAG_QUIESCE_TIMEOUT, APP_POWER_FLAG_QUIESCED);
          if (g_power_stop2_armed != 0UL)
          {
            AppPowerAbortStop2(-103L);
          }
        }
      }
    }
    else if (status != TX_QUEUE_EMPTY)
    {
      /*
       * Queue error fallback: recover via normal mode transition so mode/power
       * flags cannot drift into STOP-without-arming dead states.
       */
      g_power_pending_ack_mask = 0UL;
      g_power_quiesce_wait_active = 0UL;
      g_power_quiesce_wait_elapsed_ticks = 0UL;
      g_power_stop2_pet_active_window = 0UL;
      g_power_stop2_pet_active_until_tick = 0UL;
      if (g_power_stop2_armed != 0UL)
      {
        AppPowerAbortStop2(-104L);
      }
      AppPowerHandleModeSet(APP_MODE_STATIC);
    }
  }
}
