#include <assert.h>
#include <stdint.h>
#include <string.h>
#include "ps_hw6_owner_state_machines.h"
#include "ps_dev_tmag3001.h"
#include "ps_input_joystick.h"
#include "ps_input_state.h"
#include "ps_input_events.h"
#include "knobs_autogen.h"

#define PS_UI_ROUTER_CAL_NONE 0U
#define PS_TRACE_OWNER_JOYSTICK_WAKE 2U
#define PS_TRACE_OWNER_JOYSTICK_READ 3U
#define PS_TRACE_OWNER_JOYSTICK_SUSPEND 4U
typedef struct {uint32_t from, event, to;} PS_HW6_StateTransition;
#include "joystick_table.inc"

volatile PS_HW6_OwnerStateMachineProbe g_ps_hw6_owner_sm_probe;
static ps_dev_tmag3001_t ps_joystick_device;
static ps_input_joystick_state_t ps_joystick_input_state;
static uint32_t wakes, reads, suspends, recoveries, proof_clears;
static uint32_t fail_event, failure_stage, fail_normalize, fail_suspend;
static uint32_t fail_read, fail_wake, tick;
static uint32_t directions[3];

static ULONG tx_time_get(void) {return ++tick;}
static void PS_HW6_SM_UpdateJoystickCalibrationProbe(void) {}
static void PS_HW6_SM_UpdateJoystickDriverProbe(void) {}
static void PS_HW6_SM_UpdateJoystickInputProbe(void) {}
void PS_InputJoystick_InitState(ps_input_joystick_state_t *state)
{memset(state, 0, sizeof(*state));}
static void PS_HW6_SM_ClearJoystickTerminalSleepProof(void) {proof_clears++;}
static uint32_t PS_HW6_TraceObjectOwnerBegin(uint32_t stage) {(void)stage; return 1U;}
static void PS_HW6_TraceObjectOwnerEnd(uint32_t stage, uint32_t sequence, uint32_t status)
{(void)stage; (void)sequence; (void)status;}
static HAL_StatusTypeDef PS_HW6_SM_StatusToHal(ps_status_t status)
{return status == PS_STATUS_OK ? HAL_OK : HAL_ERROR;}

static HAL_StatusTypeDef PS_HW6_SM_Transition(uint32_t owner, uint32_t event, HAL_StatusTypeDef status)
{
  (void)status;
  if (event == fail_event) {return HAL_ERROR;}
  for (uint32_t i = 0U; i < sizeof(ps_joystick_transitions) / sizeof(ps_joystick_transitions[0]); ++i)
  {
    const PS_HW6_StateTransition *t = &ps_joystick_transitions[i];
    if (t->from == g_ps_hw6_owner_sm_probe.current_state[owner] && t->event == event)
    {
      g_ps_hw6_owner_sm_probe.current_state[owner] = t->to;
      return HAL_OK;
    }
  }
  return HAL_ERROR;
}
static HAL_StatusTypeDef PS_HW6_SM_StabilizeJoystick(void)
{
  recoveries++;
  ps_joystick_device.state = PS_DEV_TMAG3001_STATE_SUSPENDED;
  g_ps_hw6_owner_sm_probe.current_state[PS_HW6_SM_JOYSTICK] = JOY_SUSPENDED;
  return HAL_OK;
}
static void PS_HW6_SM_RecordJoystickCardinalFailure(uint32_t stage, ps_status_t status,
                                                   uint32_t hal, uint32_t error)
{(void)status; (void)hal; (void)error; failure_stage = stage;}

ps_status_t ps_dev_tmag3001_wake_continuous(ps_dev_tmag3001_t *device,
                                          ps_dev_tmag3001_wake_result_t *result)
{
  wakes++;
  memset(result, 0, sizeof(*result));
  assert(device->state == PS_DEV_TMAG3001_STATE_SUSPENDED ||
         device->state == PS_DEV_TMAG3001_STATE_WAKE_SLEEP);
  device->state = fail_wake ? PS_DEV_TMAG3001_STATE_FAULT : PS_DEV_TMAG3001_STATE_ACTIVE;
  return fail_wake ? PS_STATUS_INTERNAL_ERROR : PS_STATUS_OK;
}
ps_status_t ps_dev_tmag3001_read_raw_sample(ps_dev_tmag3001_t *device,
                                          ps_dev_tmag3001_raw_sample_t *sample)
{
  assert(device->state == PS_DEV_TMAG3001_STATE_ACTIVE);
  memset(sample, 0, sizeof(*sample));
  sample->x = (int16_t)directions[reads % 3U];
  reads++;
  if (fail_read) {device->state = PS_DEV_TMAG3001_STATE_FAULT; return PS_STATUS_INTERNAL_ERROR;}
  return PS_STATUS_OK;
}
ps_status_t ps_dev_tmag3001_suspend(ps_dev_tmag3001_t *device,
                                   ps_dev_tmag3001_suspend_result_t *result)
{
  assert(device->state == PS_DEV_TMAG3001_STATE_ACTIVE);
  suspends++;
  memset(result, 0, sizeof(*result));
  device->state = fail_suspend ? PS_DEV_TMAG3001_STATE_FAULT : PS_DEV_TMAG3001_STATE_SUSPENDED;
  return fail_suspend ? PS_STATUS_INTERNAL_ERROR : PS_STATUS_OK;
}
static HAL_StatusTypeDef PS_HW6_SM_ClassifyJoystickWakeSample(
  const ps_dev_tmag3001_raw_sample_t *sample, ps_input_joystick_state_t *state)
{state->direction_mask = (uint32_t)sample->x; return HAL_OK;}
static HAL_StatusTypeDef PS_HW6_SM_NormalizeJoystickSample(
  const ps_dev_tmag3001_raw_sample_t *sample, uint32_t policy)
{
  (void)policy;
  ps_joystick_input_state.direction_mask = (uint32_t)sample->x;
  return fail_normalize ? HAL_ERROR : HAL_OK;
}
#include "joystick_under_test.inc"

static void reset(uint32_t owner, uint32_t driver)
{
  memset((void *)&g_ps_hw6_owner_sm_probe, 0, sizeof(g_ps_hw6_owner_sm_probe));
  memset(&ps_joystick_device, 0, sizeof(ps_joystick_device));
  g_ps_hw6_owner_sm_probe.current_state[PS_HW6_SM_JOYSTICK] = owner;
  ps_joystick_device.state = driver;
  wakes = reads = suspends = recoveries = proof_clears = 0U;
  fail_event = failure_stage = fail_normalize = fail_suspend = fail_read = fail_wake = 0U;
  directions[0] = directions[1] = directions[2] = PS_INPUT_JOYSTICK_DIRECTION_RIGHT;
}
static void assert_live(void)
{
  assert(ps_joystick_device.state == PS_DEV_TMAG3001_STATE_ACTIVE);
  assert(g_ps_hw6_owner_sm_probe.current_state[PS_HW6_SM_JOYSTICK] == JOY_SLOW_POLL);
}
int main(void)
{
  reset(JOY_SUSPENDED, PS_DEV_TMAG3001_STATE_SUSPENDED);
  for (uint32_t i = 0U; i < 10U; ++i)
  {
    assert(PS_HW6_SM_RunJoystickCardinalProbe(0U) == HAL_OK);
    assert_live();
  }
  assert(wakes == 1U && reads == 10U && suspends == 0U && recoveries == 0U && proof_clears == 1U);

  /* Diagnostic preparation still explicitly parks a retained active device. */
  assert(PS_HW6_SM_PrepareJoystickInput() == HAL_OK);
  assert(suspends == 1U && ps_joystick_device.state == PS_DEV_TMAG3001_STATE_SUSPENDED);
  assert(g_ps_hw6_owner_sm_probe.current_state[PS_HW6_SM_JOYSTICK] == JOY_SUSPENDED);
  assert(PS_HW6_SM_RunJoystickCardinalProbe(0U) == HAL_OK);
  assert(wakes == 2U);

  /* Model the verified STOP2 terminal configuration, then confirm wake. */
  assert(PS_HW6_SM_Transition(PS_HW6_SM_JOYSTICK, JOY_EV_QUIESCE, HAL_OK) == HAL_OK);
  ps_joystick_device.state = PS_DEV_TMAG3001_STATE_WAKE_SLEEP;
  assert(PS_HW6_SM_RunJoystickCardinalProbe(1U) == HAL_OK);
  assert_live();
  assert(wakes == 3U && proof_clears == 3U);
  assert(g_ps_hw6_owner_sm_probe.joystick_wake_confirm_sample_count ==
         KNOB_INPUT_JOYSTICK_WAKE_CONFIRM_STABLE_SAMPLES);
  assert(PS_HW6_SM_RunJoystickCardinalProbe(0U) == HAL_OK);
  assert(wakes == 3U && suspends == 1U);

  reset(JOY_SUSPENDED, PS_DEV_TMAG3001_STATE_WAKE_SLEEP);
  directions[0] = 1U; directions[1] = 2U; directions[2] = 0U;
  assert(PS_HW6_SM_RunJoystickCardinalProbe(1U) == HAL_OK);
  assert(reads == KNOB_INPUT_JOYSTICK_WAKE_CONFIRM_SAMPLES);
  assert(g_ps_hw6_owner_sm_probe.joystick_wake_confirm_fallback_count == 1U);
  assert(ps_joystick_input_state.direction_mask == 2U);
  assert_live();

  const uint32_t events[] = {JOY_EV_THRESHOLD_ARM_REQUEST, JOY_EV_INTERRUPT,
    JOY_EV_SAMPLE_DONE, JOY_EV_NORMALIZE_DONE, JOY_EV_SLOW_POLL_REQUEST};
  for (uint32_t i = 0U; i < sizeof(events) / sizeof(events[0]); ++i)
  {
    reset(JOY_SLOW_POLL, PS_DEV_TMAG3001_STATE_ACTIVE);
    fail_event = events[i];
    assert(PS_HW6_SM_RunJoystickCardinalProbe(0U) == HAL_ERROR);
    assert(suspends == 1U);
    assert(g_ps_hw6_owner_sm_probe.current_state[PS_HW6_SM_JOYSTICK] == JOY_ERROR);
  }
  for (uint32_t cleanup_fails = 0U; cleanup_fails < 2U; ++cleanup_fails)
  {
    reset(JOY_SLOW_POLL, PS_DEV_TMAG3001_STATE_ACTIVE);
    fail_normalize = 1U; fail_suspend = cleanup_fails;
    assert(PS_HW6_SM_RunJoystickCardinalProbe(0U) == HAL_ERROR);
    assert(suspends == 1U && failure_stage == PS_HW6_JOYSTICK_FAILURE_STAGE_NORMALIZE);
    assert(g_ps_hw6_owner_sm_probe.current_state[PS_HW6_SM_JOYSTICK] == JOY_ERROR);
    fail_normalize = fail_suspend = 0U;
    assert(PS_HW6_SM_RunJoystickCardinalProbe(0U) == HAL_OK);
    assert(recoveries == 1U);
    assert_live();
  }
  reset(JOY_SLOW_POLL, PS_DEV_TMAG3001_STATE_ACTIVE);
  fail_read = 1U;
  assert(PS_HW6_SM_RunJoystickCardinalProbe(0U) == HAL_ERROR);
  assert(suspends == 0U && failure_stage == PS_HW6_JOYSTICK_FAILURE_STAGE_READ);
  reset(JOY_SUSPENDED, PS_DEV_TMAG3001_STATE_SUSPENDED);
  fail_wake = 1U;
  assert(PS_HW6_SM_RunJoystickCardinalProbe(0U) == HAL_ERROR);
  assert(reads == 0U && suspends == 0U && failure_stage == PS_HW6_JOYSTICK_FAILURE_STAGE_WAKE);
  return 0;
}
