#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "ps_hw6_rtos_probe.h"
#include "ps_ui_router.h"
#include "audio_package_constants.inc"

typedef uint32_t HAL_StatusTypeDef;
#define HAL_OK 0U
#define HAL_ERROR 1U
#define HAL_BUSY 2U
#define TX_SUCCESS 0U
#define TX_NOT_DONE 1U
#define TX_NO_WAIT 0U
#define TX_AND_CLEAR 1U
#define TX_OR 2U
#define PS_HW6_RTOS_AUDIO_CLOCK_PACKAGE_SFX_CAPABILITIES 7U

volatile PS_HW6_RTOS_Probe g_ps_hw6_rtos_probe;
volatile PS_HW6_AudioPackageProbe g_ps_audio_package_probe;
volatile uint32_t g_ps_ui_router_request, g_ps_ui_router_request_event;
static struct { uint32_t apply_count, pll2_domain_on_count, pll2_domain_off_count; } g_ps_hw6_clock_policy_probe;
static uint32_t ps_audio_sfx_pending, ps_audio_sfx_clock_held;
static uint32_t ps_audio_sfx_clock_apply_count_before;
static uint32_t ps_audio_sfx_clock_pll2_on_count_before, ps_audio_sfx_clock_pll2_off_count_before;
static uint32_t ps_event_groups[4], ps_object_last_tick;
static uint32_t mode, active, queued, starts, stops, releases, exits, paused, resumed;
static uint32_t sequence, sent, signaled, close_during_grant;
static void play(uint32_t cue);
static void PS_HW6_RTOS_StopPackageSfxOwner(uint32_t request);
void PS_HW6_RTOS_AudioSfxRequestComplete(void)
{ assert(ps_audio_sfx_pending); ps_audio_sfx_pending--; }
static uint32_t PS_HW6_AudioOwner_SfxActive(void) { return active != 0; }
static HAL_StatusTypeDef PS_HW6_OwnerStateMachines_StopAudioSfx(void)
{
  stops++;
  if (mode == 4) { return HAL_ERROR; }
  while (active) { active--; PS_HW6_RTOS_AudioSfxRequestComplete(); }
  return HAL_OK;
}
static HAL_StatusTypeDef PS_HW6_AudioOwner_VerifyIdle(void)
{ assert(!active); return HAL_OK; }
static void PS_HW6_RTOS_FinalizeAudioSfx(HAL_StatusTypeDef status)
{
  (void)status;
  releases++;
  ps_audio_sfx_clock_held = 0;
  g_ps_hw6_rtos_probe.audio_sfx_clock_release_status = mode == 5 ? 1U : 0U;
}
static UINT PS_HW6_RTOS_RequestAudioClockCapabilities(uint32_t reason, uint32_t caps)
{
  assert(reason == PS_HW6_RTOS_AUDIO_CLOCK_REASON_REACTIVE_SFX && caps == 7);
  if (close_during_grant) { g_ps_audio_package_probe.blocked = 1; }
  return TX_SUCCESS;
}
static HAL_StatusTypeDef PS_HW6_OwnerStateMachines_Stabilize(uint32_t owner)
{ assert(owner == PS_HW6_RTOS_OWNER_AUDIO); return HAL_OK; }
static HAL_StatusTypeDef PS_HW6_OwnerStateMachines_RunAudioSfx(uint32_t cue, uint32_t *owned)
{ assert(cue == 0 && !g_ps_audio_package_probe.blocked); *owned = 1; active++; starts++; return HAL_OK; }
static UINT PS_HW6_RTOS_SendModeCommand(uint32_t owner, uint32_t command, uint32_t value)
{
  assert(owner == PS_HW6_RTOS_OWNER_AUDIO && command == PS_HW6_RTOS_COMMAND_AUDIO_STOP_PACKAGE);
  assert(g_ps_audio_package_probe.blocked);
  sequence = value; sent++;
  return mode == 2 ? TX_NOT_DONE : TX_SUCCESS;
}
static UINT tx_event_flags_set(uint32_t *group, ULONG flags, UINT operation)
{ assert(group == &ps_event_groups[3] && flags == PS_HW6_RTOS_AUDIO_STOP_ACK && operation == TX_OR); signaled = 1; return 0; }
static UINT tx_event_flags_get(uint32_t *group, ULONG flags, UINT operation, ULONG *actual, ULONG wait)
{
  assert(group == &ps_event_groups[3] && flags == PS_HW6_RTOS_AUDIO_STOP_ACK && operation == TX_AND_CLEAR);
  if (!wait) { signaled = 0; return TX_NOT_DONE; }
  assert(wait == PS_HW6_RTOS_OWNER_ACK_WAIT_TICKS);
  if (mode == 3) { return TX_NOT_DONE; }
  while (queued) { queued--; play(0); }
  PS_HW6_RTOS_StopPackageSfxOwner(mode == 6 ? sequence - 1 : sequence);
  assert(signaled); *actual = flags;
  return TX_SUCCESS;
}
static uint32_t tx_time_get(void) { return 123; }
static UINT PS_HW6_RTOS_RequestRuntimeClockCapabilities(uint32_t reason, uint32_t caps)
{ (void)reason; (void)caps; assert(!active); return 0; }
static void PS_HW6_RTOS_RuntimeStateTimersPause(uint32_t tick)
{ assert(tick == 123 && (mode == 9 || (!active && !ps_audio_sfx_pending))); paused++; }
static void PS_HW6_RTOS_RuntimeStateTimersResume(uint32_t tick)
{ assert(tick == 123 && !g_ps_audio_package_probe.blocked); resumed++; }
static uint32_t PS_SceneRuntime_DevelopmentObjectsActive(void) { return 1; }
static uint32_t PS_HW6_RTOS_ObjectAdvance(uint32_t tick) { assert(tick == 123); return 0; }
static UINT PS_HW6_RTOS_ObjectPresent(void) { return 0; }
static void PS_HW6_RTOS_RuntimePackageReplacementFail(void) { assert(0); }
static void PS_HW6_RTOS_RuntimeBootShell(void) { assert(0); }
static uint32_t PS_SceneRuntime_StateSceneActive(void) { return 1; }
static uint32_t PS_SceneRuntime_StateFocusIndex(void) { return 0; }
static UINT PS_HW6_RTOS_SendDisplayUiRenderCommand(uint32_t p, uint32_t c, uint32_t f, uint32_t s, uint32_t d)
{ (void)p; (void)c; (void)f; (void)s; (void)d; return 0; }
static void PS_HW6_RTOS_RuntimeInteractionEnd(void) { assert(!active && !ps_audio_sfx_pending); }
static void PS_SceneRuntime_ExitStateScene(void) { assert(!active && !ps_audio_sfx_pending); exits++; }
static void PS_HW6_RTOS_RuntimeSetState(uint32_t cls, uint32_t exec, uint32_t life)
{ g_ps_hw6_rtos_probe.runtime_current_class = cls; g_ps_hw6_rtos_probe.runtime_execution = exec; g_ps_hw6_rtos_probe.runtime_lifecycle = life; }
static void PS_HW6_RTOS_RuntimeRecord(uint32_t event, uint32_t status)
{ g_ps_hw6_rtos_probe.runtime_last_event = event; g_ps_hw6_rtos_probe.runtime_last_status = status; }

#include "audio_package_under_test.inc"

int main(int argc, char **argv)
{
  assert(argc == 2); mode = (uint32_t)atoi(argv[1]);
  g_ps_hw6_rtos_probe.runtime_current_class = PS_HW6_RUNTIME_CLASS_LP_GRAPH;
  g_ps_hw6_rtos_probe.runtime_lifecycle = PS_HW6_RUNTIME_LIFECYCLE_RUNNING;
  if (mode == 0)
  {
    assert(PS_HW6_RTOS_StopPackageSfx() == 0 && !sent && !stops);
    assert(g_ps_audio_package_probe.blocked && PS_HW6_RTOS_OpenPackageSfx() == 0);
  }
  else if (mode == 7)
  {
    ps_audio_sfx_pending = 1; close_during_grant = 1;
    play(0);
    assert(!starts && !active && !ps_audio_sfx_pending && g_ps_audio_package_probe.discarded == 1);
    assert(PS_HW6_RTOS_StopPackageSfx() == 0 && releases == 1);
  }
  else
  {
    active = 1; queued = 2; ps_audio_sfx_pending = 3; ps_audio_sfx_clock_held = 1;
    if (mode == 1)
    {
      PS_HW6_RTOS_RuntimeSuspend(1U);
      assert(paused == 1 && stops == 1 && releases == 1 && !starts && !active && !ps_audio_sfx_pending);
      assert(g_ps_audio_package_probe.discarded == 2 && g_ps_audio_package_probe.blocked);
      assert(g_ps_hw6_rtos_probe.runtime_lifecycle == PS_HW6_RUNTIME_LIFECYCLE_SUSPENDED);
      PS_HW6_RTOS_RuntimeResume();
      assert(resumed == 1 && !starts && !g_ps_audio_package_probe.blocked);
      ps_audio_sfx_pending++; play(0);
      assert(starts == 1 && active == 1);
      PS_HW6_RTOS_RuntimePackageReturn();
      assert(exits == 1 && !active && !ps_audio_sfx_pending && g_ps_audio_package_probe.blocked);
    }
    else if (mode == 9)
    {
      PS_HW6_RTOS_RuntimeSuspend(0U);
      assert(paused == 1 && !stops && !releases && !sent);
      assert(active == 1 && ps_audio_sfx_pending == 3);
      assert(g_ps_hw6_rtos_probe.runtime_lifecycle == PS_HW6_RUNTIME_LIFECYCLE_SUSPENDED);
      /* Power's existing owner-quiesce barrier handles audio next. */
    }
    else if (mode == 8)
    {
      PS_HW6_RTOS_RuntimePackageReturn();
      assert(exits == 1 && !starts && !active && !ps_audio_sfx_pending);
      assert(g_ps_hw6_rtos_probe.runtime_current_class == PS_HW6_RUNTIME_CLASS_SHELL);
    }
    else
    {
      assert(PS_HW6_RTOS_StopPackageSfx() != 0 && g_ps_audio_package_probe.fault);
      if (mode == 3)
      {
        while (queued) { queued--; play(0); }
        PS_HW6_RTOS_StopPackageSfxOwner(sequence);
        assert(g_ps_audio_package_probe.complete == g_ps_audio_package_probe.request);
      }
      assert(PS_HW6_RTOS_OpenPackageSfx() != 0 && g_ps_audio_package_probe.blocked);
      PS_HW6_RTOS_RuntimeResume();
      assert(!resumed);
      PS_HW6_RTOS_RuntimePackageReturn();
      assert(!exits && !starts && sent == 1);
    }
  }
  return 0;
}
