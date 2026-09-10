#define PS_OBJECT_AWAKE_MAIN awake_fixture_main
#include "native_object_awake.c"
#include "ps_hw6_object_development.h"
#include "object_timer_probe_under_test.inc"

typedef uint32_t UINT;
#define TX_SUCCESS 0U
#define TX_CALLER_ERROR 1U
#define TX_TIMER_TICKS_PER_SECOND 100U
#define PS_HW6_RUNTIME_CLASS_LP_GRAPH 2U
#define PS_HW6_RUNTIME_LIFECYCLE_RUNNING 2U
#define PS_HW6_RTOS_COMMAND_RUNTIME_PACKAGE_RETURN 20U
#define PS_HW6_RTOS_OWNER_AUDIO 1U
#define PS_HW6_RTOS_COMMAND_AUDIO_PLAY_SFX 1U
#define PS_UI_ROUTER_PAGE_RUNTIME_HANDOFF 6U
#define PS_UI_ROUTER_CAL_NONE 0U
#define PS_UI_ROUTER_SHUTDOWN_NONE 0U

volatile ps_hw6_object_development_probe_t g_ps_object_development_probe;
static uint32_t ps_object_last_tick, ps_object_tick_fraction;
static uint32_t now, draws, failures, exits, render_error;
static struct { uint32_t blocked, discarded; } g_ps_audio_package_probe;
static uint32_t sfx_allowed, sfx_pending, sfx_sends, sfx_reject;
static uint32_t sfx_samples[16];
static uint32_t PS_HW6_RTOS_ObjectAdvance(uint32_t tick);
static UINT PS_HW6_RTOS_ObjectPresent(void);
static void PS_HW6_RTOS_RuntimeStateTimersClear(void);
static uint32_t tx_time_get(void) { return now; }
static uint32_t PS_HW6_RTOS_MsToTicks(uint32_t ms) { return (ms + 9U) / 10U; }
static uint32_t PS_HW6_RTOS_TimeReached(uint32_t tick, uint32_t deadline)
{ return (int32_t)(tick - deadline) >= 0; }
static void PS_HW6_RTOS_RuntimePackageReturn(void)
{ exits++; PS_SceneRuntime_ExitStateScene(); PS_HW6_RTOS_RuntimeStateTimersClear(); }
static void PS_HW6_RTOS_RuntimePackageReplacementFail(void)
{ failures++; PS_SceneRuntime_ExitStateScene(); PS_HW6_RTOS_RuntimeStateTimersClear(); }
static UINT PS_HW6_RTOS_RequestRuntimeCommand(uint32_t command)
{ (void)command; assert(0); return 1; }
static void PS_HW6_RTOS_AudioSfxRequestQueued(void) { assert(sfx_allowed); sfx_pending++; }
static void PS_HW6_RTOS_AudioSfxRequestComplete(void) { assert(sfx_pending); sfx_pending--; }
static UINT PS_HW6_RTOS_SendModeCommand(uint32_t owner, uint32_t command, uint32_t cue)
{
  ps_egg_state_loader_audio_cue_t value;
  assert(owner == PS_HW6_RTOS_OWNER_AUDIO && command == PS_HW6_RTOS_COMMAND_AUDIO_PLAY_SFX);
  assert(sfx_allowed && sfx_sends < 16);
  assert(PS_EggStateLoader_GetAudioCue(cue, &value) == 1);
  assert(value.adpcm != NULL && value.package_backed == 0);
  sfx_samples[sfx_sends++] = value.sample_count;
  return sfx_reject;
}
static UINT PS_HW6_RTOS_SendDisplayUiRenderCommand(uint32_t page, uint32_t cal,
  uint32_t focus, uint32_t shutdown, uint32_t countdown)
{ (void)page; (void)cal; (void)focus; (void)shutdown; (void)countdown; assert(0); return 1; }

#include "object_timers_under_test.inc"

static UINT PS_HW6_RTOS_ObjectPresent(void)
{
  uint32_t index, next;
  if (render_error) { return TX_CALLER_ERROR; }
  assert(PS_HW6_RTOS_ObjectAdvance(now) == 0);
  assert(PS_SceneRuntime_ProjectDevelopmentObjects(&model, &next) == 0);
  assert(DisplayRenderer_ValidateSceneModel(&model) == 1);
  memset(s_display_framebuffer, 255, sizeof(s_display_framebuffer));
  for (index = 0; index < model.element_count; ++index)
  { (void)DisplayRenderer_DrawSceneElement(&model.elements[index]); }
  draws++;
  return TX_SUCCESS;
}

static uint32_t binding(uint32_t scope)
{
  uint32_t index;
  for (index = 0; index < s_ps_scene_runtime_state_scene->event_binding_count; ++index)
  {
    const ps_scene_runtime_event_binding_t *event = &s_ps_scene_runtime_state_scene->event_bindings[index];
    if (event->event_class == PS_SCENE_RUNTIME_EVENT_CLASS_TIMER && event->event_kind == scope)
    { return index; }
  }
  assert(0); return 0;
}

static void input(uint32_t button)
{
  uint32_t result;
  assert(PS_HW6_RTOS_ObjectAdvance(now) == 0);
  result = PS_SceneRuntime_HandleStateSceneInput(1, button);
  assert(result == PS_SCENE_RUNTIME_INPUT_APPLIED);
  PS_HW6_RTOS_RuntimeStateTimersSync(now, 0);
  assert(PS_HW6_RTOS_CompleteStateSceneEvent(result) == TX_SUCCESS);
}

static void service(uint32_t tick)
{
  now = tick;
  /* A cached owner-loop tick must not rewind the object's clock. */
  PS_HW6_RTOS_RuntimeStateTimersService(0);
}

int main(int argc, char **argv)
{
  uint32_t size, timer, state_timer, state_epoch, scene_epoch, mode, next;
  FILE *output;
  assert(argc == 4);
  size = read_blob(argv[1], candidate);
  set_hash(argv[1], candidate, size);
  mode = (uint32_t)atoi(argv[3]);
  assert(PS_SceneRuntime_EnterDevelopmentObjects(candidate, size) == 0);
  now = ps_object_last_tick = 100;
  g_ps_hw6_rtos_probe.runtime_current_class = PS_HW6_RUNTIME_CLASS_LP_GRAPH;
  g_ps_hw6_rtos_probe.runtime_lifecycle = PS_HW6_RUNTIME_LIFECYCLE_RUNNING;
  PS_HW6_RTOS_RuntimeStateTimersSync(now, 1);
  assert(PS_HW6_RTOS_ObjectPresent() == 0);
  timer = binding(PS_SCENE_RUNTIME_TIMER_SCENE);
  state_timer = mode >= 11 ? 0U : binding(PS_SCENE_RUNTIME_TIMER_STATE_ENTRY);
  scene_epoch = PS_SceneRuntime_SceneActivation();

  if (mode == 0)
  {
    assert(ps_runtime_state_timers[timer].deadline_tick == 600);
    now = 150; input(1);
    assert(ps_runtime_state_timers[state_timer].deadline_tick == 450);
    now = 200; input(2);
    assert(ps_runtime_state_timers[state_timer].active == 0);
    now = 400; input(1);
    assert(ps_runtime_state_timers[state_timer].deadline_tick == 700);
    assert(ps_runtime_state_timers[timer].deadline_tick == 600);
    state_epoch = PS_SceneRuntime_StateActivation();
    service(625);
    assert(g_ps_hw6_rtos_probe.runtime_state_timer_applied_count == 1);
    assert(PS_SceneRuntime_StateActivation() == state_epoch);
    assert(PS_SceneRuntime_SceneActivation() == scene_epoch);
    assert(model.state_id == 2 && model.elements[2].x == 140);
    assert(s_ps_object_snapshot.elapsed_ms == 5250 && s_ps_object_snapshot.objects[0].remaining_ms == 250);
    assert(ps_runtime_state_timers[state_timer].deadline_tick == 700);
    service(700);
    assert(g_ps_hw6_rtos_probe.runtime_state_timer_applied_count == 2);
    assert(model.state_id == 1 && model.elements[1].x == 32 && model.elements[2].x == 140);
    service(2000);
    assert(g_ps_hw6_rtos_probe.runtime_state_timer_applied_count == 2);
  }
  else if (mode == 1 || mode == 2)
  {
    now = 200; input(3);
    assert(ps_runtime_state_timers[timer].deadline_tick == 700);
    now = 250; input(3);
    assert(ps_runtime_state_timers[timer].deadline_tick == (mode == 1 ? 750U : 700U));
    now = 300; input(4);
    service(1000);
    assert(ps_runtime_state_timers[timer].active == 0);
    assert(g_ps_hw6_rtos_probe.runtime_state_timer_due_count == 0);
    now = 1100; input(3);
    service(1600);
    assert(g_ps_hw6_rtos_probe.runtime_state_timer_applied_count == 1);
    assert(model.elements[2].x == 140);
    service(3000);
    assert(g_ps_hw6_rtos_probe.runtime_state_timer_applied_count == 1);
  }
  else if (mode == 3)
  {
    now = 250;
    PS_HW6_RTOS_RuntimeStateTimersPause(now);
    assert(PS_HW6_RTOS_ObjectAdvance(now) == 0);
    g_ps_hw6_rtos_probe.runtime_lifecycle = 3;
    service(2000);
    assert(g_ps_hw6_rtos_probe.runtime_state_timer_due_count == 0 && draws == 1);
    assert(s_ps_object_graph.objects.elapsed_ms == 1500);
    PS_HW6_RTOS_RuntimeStateTimersResume(now);
    ps_object_last_tick = now;
    g_ps_hw6_rtos_probe.runtime_lifecycle = PS_HW6_RUNTIME_LIFECYCLE_RUNNING;
    service(2349);
    assert(g_ps_hw6_rtos_probe.runtime_state_timer_due_count == 0);
    service(2350);
    assert(g_ps_hw6_rtos_probe.runtime_state_timer_applied_count == 1);
    assert(s_ps_object_snapshot.elapsed_ms == 5000 && model.elements[2].x == 140);
  }
  else if (mode == 4)
  {
    service(600);
    assert(g_ps_hw6_rtos_probe.runtime_state_timer_due_count == 1);
    assert(g_ps_hw6_rtos_probe.runtime_state_timer_applied_count == 1);
    assert(model.elements[1].y == 104);
    service(1200);
    assert(g_ps_hw6_rtos_probe.runtime_state_timer_due_count == 1);
  }
  else if (mode == 5)
  {
    service(600);
    assert(g_ps_hw6_rtos_probe.runtime_state_timer_ignored_count == 1);
    assert(draws == 1 && ps_runtime_state_timers[timer].active == 0);
    now = 700; input(1);
    service(800);
    assert(g_ps_hw6_rtos_probe.runtime_state_timer_due_count == 1);
  }
  else if (mode == 6)
  {
    service(600);
    assert(exits == 1 && PS_SceneRuntime_StateSceneActive() == 0);
    assert(g_ps_hw6_rtos_probe.runtime_state_timer_due_count == 1);
    assert(g_ps_hw6_rtos_probe.runtime_state_timer_active_count == 0);
  }
  else if (mode == 7)
  {
    render_error = 1;
    service(600);
    assert(failures == 1 && PS_SceneRuntime_StateSceneActive() == 0);
    assert(g_ps_hw6_rtos_probe.runtime_state_timer_active_count == 0);
  }
  else if (mode == 8)
  {
    now = 599;
    PS_SceneRuntime_ExitStateScene();
    PS_HW6_RTOS_RuntimeStateTimersSync(now, 0);
    assert(ps_runtime_state_timers[timer].configured == 0);
    assert(PS_SceneRuntime_EnterDevelopmentObjects(candidate, size) == 0);
    ps_object_last_tick = now;
    PS_HW6_RTOS_RuntimeStateTimersSync(now, 0);
    assert(PS_SceneRuntime_SceneActivation() == scene_epoch + 1);
    assert(ps_runtime_state_timers[timer].deadline_tick == 1099);
    service(600);
    assert(g_ps_hw6_rtos_probe.runtime_state_timer_due_count == 0);
    service(1099);
    assert(g_ps_hw6_rtos_probe.runtime_state_timer_applied_count == 1);
  }
  else if (mode == 9)
  {
    now = 150; input(1);
    now = 200; input(4);
    assert(ps_runtime_state_timers[state_timer].deadline_tick == 500);
    service(450);
    assert(g_ps_hw6_rtos_probe.runtime_state_timer_due_count == 0);
    service(500);
    assert(model.state_id == 1 && g_ps_hw6_rtos_probe.runtime_state_timer_applied_count == 1);
  }
  else if (mode == 10)
  {
    now = 650;
    PS_HW6_RTOS_RuntimeStateTimersPause(now);
    assert(PS_HW6_RTOS_ObjectAdvance(now) == 0);
    g_ps_hw6_rtos_probe.runtime_lifecycle = 3;
    service(2000);
    PS_HW6_RTOS_RuntimeStateTimersResume(now);
    ps_object_last_tick = now;
    g_ps_hw6_rtos_probe.runtime_lifecycle = PS_HW6_RUNTIME_LIFECYCLE_RUNNING;
    service(2000);
    assert(g_ps_hw6_rtos_probe.runtime_state_timer_applied_count == 1);
    assert(s_ps_object_snapshot.elapsed_ms == 5500);
    service(2000);
    assert(g_ps_hw6_rtos_probe.runtime_state_timer_applied_count == 1);
  }
  else if (mode == 11)
  {
    assert(model.elements[2].visible == 0);
    assert(ps_runtime_state_timers[timer].deadline_tick == 300);
    now = 165; input(1);
    assert(s_ps_object_snapshot.objects[0].step == 1);
    assert(s_ps_object_snapshot.objects[0].remaining_ms == 350);
    now = 195; input(2);
    assert(s_ps_object_snapshot.objects[0].remaining_ms == 50);
    now = 240; input(1);
    state_epoch = PS_SceneRuntime_StateActivation();
    assert(ps_runtime_state_timers[timer].deadline_tick == 300);
    service(299);
    assert(g_ps_hw6_rtos_probe.runtime_state_timer_due_count == 0);
    assert(PS_HW6_RTOS_ObjectPresent() == 0);
    assert(model.elements[2].visible == 0);
    assert(s_ps_object_snapshot.objects[0].step == 1);
    assert(s_ps_object_snapshot.objects[0].remaining_ms == 10);
    service(305);
    assert(model.elements[2].visible == 1);
    assert(model.elements[2].x == 76 && model.elements[2].y == 80);
    assert(PS_SceneRuntime_StateActivation() == state_epoch);
    assert(PS_SceneRuntime_SceneActivation() == scene_epoch);
    assert(s_ps_object_snapshot.elapsed_ms == 2050);
    assert(s_ps_object_snapshot.objects[0].step == 0);
    assert(s_ps_object_snapshot.objects[0].remaining_ms == 450);
    now = 330; input(2);
    assert(model.elements[2].visible == 1);
    now = 340; input(1);
    assert(model.elements[2].visible == 1);
    service(900);
    assert(PS_HW6_RTOS_ObjectPresent() == 0);
    assert(model.elements[2].visible == 1 && model.elements[1].x == 120);
    assert(g_ps_hw6_rtos_probe.runtime_state_timer_due_count == 1);
    assert(g_ps_hw6_rtos_probe.runtime_state_timer_applied_count == 1);
    assert(g_ps_hw6_rtos_probe.runtime_state_timer_ignored_count == 0);
    assert(ps_runtime_state_timers[timer].active == 0);
    assert(s_ps_object_snapshot.elapsed_ms == 8000);
    assert(s_ps_object_snapshot.objects[0].step == 0);
    assert(s_ps_object_snapshot.objects[0].remaining_ms == 500);
  }
  else if (mode == 12)
  {
    sfx_allowed = 1;
    now = 165; input(1);
    assert(sfx_sends == 1 && sfx_samples[0] == 1280);
    assert(s_ps_object_snapshot.objects[0].step == 2 && s_ps_object_snapshot.objects[0].remaining_ms == 100);
    now = 195; input(2);
    assert(sfx_sends == 2 && sfx_samples[1] == 1280);
    state_epoch = PS_SceneRuntime_StateActivation();
    service(305);
    assert(sfx_sends == 3 && sfx_samples[2] == 96000);
    assert(PS_SceneRuntime_StateActivation() == state_epoch && model.elements[2].visible == 1);
    assert(s_ps_object_snapshot.objects[0].step == 0 && s_ps_object_snapshot.objects[0].remaining_ms == 200);
    now = 330; input(3);
    assert(sfx_sends == 5 && sfx_samples[3] == 1280 && sfx_samples[4] == 96000);
    assert(sfx_pending == 5 && g_ps_scene_runtime_probe.sfx_request_take_count == 5);
    sfx_reject = 1;
    now = 340; input(1);
    assert(sfx_sends == 6 && sfx_pending == 5 && g_ps_hw6_rtos_probe.audio_sfx_send_status == 1);
    g_ps_audio_package_probe.blocked = 1;
    now = 350; input(2);
    assert(sfx_sends == 6 && sfx_pending == 5 && g_ps_audio_package_probe.discarded == 1);
    now = 360; input(4);
    assert(exits == 1 && !PS_SceneRuntime_DevelopmentObjectsActive());
  }
  else if (mode == 13)
  {
    uint32_t cue;
    now = 165;
    assert(PS_HW6_RTOS_ObjectAdvance(now) == 0);
    assert(PS_SceneRuntime_HandleStateSceneInput(1, 1) == PS_SCENE_RUNTIME_INPUT_ERROR);
    assert(PS_SceneRuntime_TakeSfxRequest(&cue) == 0 && sfx_sends == 0);
    assert(g_ps_scene_runtime_probe.state_id == 1);
  }
  else if (mode == 14)
  {
    uint32_t phase;
    assert(ps_runtime_state_timers[timer].deadline_tick == 300);
    output = fopen(argv[2], "wb"); assert(output != NULL);
    for (phase = 0; phase < 4; ++phase)
    {
      now = 100 + 44 * phase;
      if (phase != 0) { input((phase & 1U) ? 1U : 2U); }
      assert(PS_HW6_RTOS_ObjectPresent() == 0);
      assert(s_ps_object_snapshot.objects[0].step == phase);
      assert(s_ps_object_snapshot.objects[0].remaining_ms == 400 - 40 * phase);
      assert(model.elements[0].x == 72 && model.elements[0].y == 40);
      assert(model.elements[2].visible == 0);
      assert(fwrite(s_display_framebuffer, 1, sizeof(s_display_framebuffer), output) == sizeof(s_display_framebuffer));
    }
    fclose(output);
    state_epoch = PS_SceneRuntime_StateActivation();
    service(299);
    assert(g_ps_hw6_rtos_probe.runtime_state_timer_due_count == 0);
    service(300);
    assert(model.elements[2].visible == 1);
    assert(s_ps_object_snapshot.objects[0].step == 1);
    assert(s_ps_object_snapshot.objects[0].remaining_ms == 400);
    assert(PS_SceneRuntime_StateActivation() == state_epoch);
    assert(PS_SceneRuntime_SceneActivation() == scene_epoch);
    now = 330; input(2);
    now = 340; input(1);
    service(625);
    assert(PS_HW6_RTOS_ObjectPresent() == 0);
    assert(s_ps_object_snapshot.objects[0].step == 1);
    assert(s_ps_object_snapshot.objects[0].remaining_ms == 350);
    assert(model.elements[1].x == 120 && model.elements[2].visible == 1);
    assert(g_ps_hw6_rtos_probe.runtime_state_timer_applied_count == 1);
    assert(ps_runtime_state_timers[timer].active == 0 && sfx_sends == 0);
  }
  else { assert(0); }
  assert(g_ps_hw6_rtos_probe.runtime_state_timer_error_count == 0);
  if (PS_SceneRuntime_StateSceneActive())
  { assert(PS_SceneRuntime_ProjectDevelopmentObjects(&model, &next) == 0); }
  output = fopen(argv[2], mode == 14 ? "ab" : "wb"); assert(output != NULL);
  assert(fwrite(s_display_framebuffer, 1, sizeof(s_display_framebuffer), output) == sizeof(s_display_framebuffer));
  fclose(output);
  return 0;
}
