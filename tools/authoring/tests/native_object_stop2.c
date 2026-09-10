#define PS_OBJECT_AWAKE_MAIN awake_main
#include "native_object_awake.c"
#include "ps_hw6_object_development.h"
#include "ps_ui_router.h"
#define LS013B7DH05_H
#define BUFFER_LENGTH (18U * 168U)
#undef DISPLAY_RENDERER_WIDTH
#undef DISPLAY_RENDERER_HEIGHT
#undef DISPLAY_RENDERER_BUFFER_SIZE
#define LCD_DMA_MAX_ROWS_PER_TRANSFER 48U
#include "display_renderer.h"
#include "ps_lpbam_display_buffers.c"
#define HAL_BUSY 2U
#define TX_TIMER_TICKS_PER_SECOND 100U
#define RTC_FORMAT_BIN 0U

static uint32_t ps_hw6_display_lpbam_active, ps_hw6_display_lpbam_prearmed;
static uint32_t ps_hw6_display_lpbam_compiled;
static struct { uint32_t display_success, display_ui_page; } g_ps_hw6_owner_probe;
static ps_scene_render_model_t ps_hw6_development_display_model;
volatile ps_hw6_object_lpbam_prepare_probe_t g_ps_object_lpbam_prepare_probe;
#include "object_lpbam_under_test.inc"

volatile ps_hw6_object_lpbam_probe_t g_ps_object_lpbam_probe;
volatile ps_hw6_object_development_probe_t g_ps_object_development_probe;
static ps_object_waiting_program_t ps_hw6_object_waiting;
static display_renderer_waiting_animation_t ps_hw6_object_animation;
static uint32_t ps_hw6_object_waiting_tick;
static uint64_t ps_hw6_object_waiting_missing_ms;
static display_renderer_waiting_animation_t s_display_full_scene_waiting;
static display_renderer_waiting_animation_t s_display_waiting_guaranteed_animation;
static const display_renderer_waiting_animation_t *s_display_selected_waiting_animation;
static uint32_t s_display_full_scene_waiting_active, s_display_cursor_base_valid;
static uint8_t s_display_cursor_base_framebuffer[DISPLAY_RENDERER_BUFFER_SIZE];
volatile display_renderer_scene_waiting_probe_t g_display_renderer_scene_waiting_probe;
static void PS_HW6_DisplayOwner_SnapshotSceneWaitingTimeline(void) {}
static uint32_t now;
static uint32_t tx_time_get(void) { return now; }
static uint64_t ps_object_missing_consumed, ps_object_rtc_start_ms;
static uint32_t ps_object_rtc_start_tick, ps_object_rtc_valid;
static uint32_t ps_object_last_tick, ps_object_tick_fraction;
typedef struct { uint32_t Hours, Minutes, Seconds, SubSeconds, SecondFraction; } RTC_TimeTypeDef;
typedef struct { uint32_t Year, Month, Date; } RTC_DateTypeDef;
static RTC_TimeTypeDef rtc_time;
static RTC_DateTypeDef rtc_date;
static uint32_t hrtc, rtc_fail;
static HAL_StatusTypeDef HAL_RTC_GetTime(uint32_t *rtc, RTC_TimeTypeDef *time, uint32_t format)
{ (void)rtc; (void)format; *time = rtc_time; return rtc_fail ? HAL_ERROR : HAL_OK; }
static HAL_StatusTypeDef HAL_RTC_GetDate(uint32_t *rtc, RTC_DateTypeDef *date, uint32_t format)
{ (void)rtc; (void)format; *date = rtc_date; return rtc_fail ? HAL_ERROR : HAL_OK; }
typedef struct { uint32_t ARR, CCR1; } test_timer_t;
static test_timer_t timer;
static struct { test_timer_t *Instance; } hlptim1 = { &timer };
static uint32_t register_fail, tick_during_write;
#define LPTIM_FLAG_ARROK 1U
#define LPTIM_FLAG_CMP1OK 2U
#define LPTIM_CHANNEL_1 0U
#define __HAL_LPTIM_CLEAR_FLAG(h,f) ((void)(h), (void)(f))
#define __HAL_LPTIM_AUTORELOAD_SET(h,v) ((h)->Instance->ARR = (v))
#define __HAL_LPTIM_COMPARE_SET(h,c,v) ((void)(c), (h)->Instance->CCR1 = (v))
static HAL_StatusTypeDef PS_HW6_DisplayOwner_WaitLptimFlag(uint32_t flag)
{ (void)flag; now += tick_during_write; return register_fail ? HAL_ERROR : HAL_OK; }
static HAL_StatusTypeDef PS_HW6_DisplayOwner_GetLpbamTimerCounts(uint32_t ms,
  uint32_t *period, uint32_t *compare)
{ *period = ms * 31250U / 1000U; *compare = *period - 1U; return HAL_OK; }
#include "object_stop2_under_test.inc"

int main(int argc, char **argv)
{
  static ps_object_waiting_program_t program;
  static display_renderer_waiting_animation_t animation;
  static uint8_t actual[DISPLAY_RENDERER_BUFFER_SIZE], expected[DISPLAY_RENDERER_BUFFER_SIZE];
  uint32_t size, next, step, remaining;
  uint64_t before, after;
  assert(argc == 3);
  size = read_blob(argv[1], candidate);
  set_hash(argv[1], candidate, size);
  assert(PS_SceneRuntime_EnterDevelopmentObjects(candidate, size) == 0);
  now = 65;
  assert(PS_HW6_RTOS_ObjectAdvance(now) == 0);
  assert(PS_SceneRuntime_BuildDevelopmentWaiting(&program) == 0);
  assert(PS_SceneRuntime_ProjectDevelopmentObjects(&ps_hw6_development_display_model, &next) == 0);
  g_ps_object_lpbam_probe.enabled = 1;
  assert(PS_HW6_DisplayOwner_PublishDevelopmentWaiting(&program, 80) == HAL_OK);
  program.quantum_ms = 401;
  assert(PS_HW6_DisplayOwner_PublishDevelopmentWaiting(&program, 80) == HAL_ERROR);
  program.quantum_ms = 400;
  assert(PS_HW6_DisplayOwner_ObjectWaitingPosition(now, &step, &remaining) == 1);
  assert(step == 0 && remaining == 15);
  assert(DisplayRenderer_ResolveFullSceneWaiting(&animation, 0, 80) == 1);
  assert(DisplayRenderer_GetGuaranteedWaitingAnimation(&animation) == NULL);
  assert(DisplayRenderer_SelectWaitingAnimation(&animation) == 1);
  /* Full-scene playback does not require a bring-up cursor/base buffer. */
  assert(s_display_cursor_base_valid == 0);
  for (step = 0; step < 4; ++step)
  {
    ps_scene_objects_snapshot_t snapshot;
    assert(DisplayRenderer_CopyWaitingAnimationFrame(&animation, step, actual, sizeof(actual)) == 1);
    assert(PS_ObjectWaiting_Project(&program, step, &snapshot, &model) == 0);
    assert(DisplayRenderer_CopySceneModelFrame(&model, expected, sizeof(expected)) == 1);
    assert(memcmp(actual, expected, sizeof(actual)) == 0);
    assert(DisplayRenderer_CopyWaitingAnimationFrame(&animation, step,
      s_display_framebuffer, sizeof(s_display_framebuffer)) == 1);
    assert(memcmp(actual, s_display_framebuffer, sizeof(actual)) == 0);
  }
  assert(PS_HW6_DisplayOwner_CompileWaitingAnimationPayload(&animation) == HAL_OK);
  assert(ps_lpbam_display_active_sequence_count == 4);
  assert(ps_lpbam_display_admission.payload_used_bytes <= 10512);
  assert(PS_HW6_DisplayOwner_SetObjectFirstInterval() == HAL_OK);
  assert(timer.ARR == 12499 && timer.CCR1 == 4687);
  assert(g_ps_object_lpbam_probe.commit_remaining_ticks == 15);
  /* Compare offset gives 150.016ms first, then 400ms between subsequent edges. */
  assert(timer.CCR1 + 1 == 4688 && timer.ARR + 1 == 12500);
  now = 80;
  assert(PS_HW6_DisplayOwner_SetObjectFirstInterval() == HAL_BUSY);
  now = 65; register_fail = 1;
  assert(PS_HW6_DisplayOwner_SetObjectFirstInterval() == HAL_ERROR);
  register_fail = 0; tick_during_write = 1;
  assert(PS_HW6_DisplayOwner_SetObjectFirstInterval() == HAL_BUSY);
  tick_during_write = 0; now = 65;
  animation.next_deadline_tick = 105;
  assert(PS_HW6_DisplayOwner_SetObjectFirstInterval() == HAL_OK && timer.CCR1 == 12498);
  now = UINT32_MAX - 4; animation.next_deadline_tick = 10;
  assert(PS_HW6_DisplayOwner_SetObjectFirstInterval() == HAL_OK);
  now = 65;
  rtc_date = (RTC_DateTypeDef){26,9,10};
  rtc_time = (RTC_TimeTypeDef){23,59,59,0,0};
  assert(PS_HW6_RTOS_ObjectSleepClockBegin() == HAL_OK);
  rtc_date.Date = 11;
  rtc_time = (RTC_TimeTypeDef){0,0,2,0,0};
  PS_HW6_RTOS_ObjectSleepClockFinish();
  assert(g_ps_object_lpbam_probe.sleep_ms == 3000 && g_ps_object_lpbam_probe.sleep_count == 1);
  assert(s_ps_object_graph.objects.elapsed_ms == 650); /* Power never mutates the bank. */
  assert(PS_HW6_DisplayOwner_ObjectWaitingPosition(now, &step, &remaining) == 1);
  assert(step == 0 && remaining == 35);
  assert(PS_HW6_RTOS_ObjectAdvance(now) == 0);
  assert(s_ps_object_graph.objects.elapsed_ms == 3650);
  assert(PS_HW6_RTOS_ObjectAdvance(now) == 0 && s_ps_object_graph.objects.elapsed_ms == 3650);
  assert(PS_SceneRuntime_HandleStateSceneInput(1, 1) == PS_SCENE_RUNTIME_INPUT_APPLIED);
  assert(PS_SceneRuntime_ProjectDevelopmentObjects(&model, &next) == 0);
  assert(model.elements[0].asset_id == 65538 && next == 350 && model.elements[1].x == 120);
  rtc_date = (RTC_DateTypeDef){24,2,28};
  assert(PS_HW6_RTOS_ObjectRtcMilliseconds(&before) == HAL_OK);
  rtc_date.Month = 3; rtc_date.Date = 1;
  assert(PS_HW6_RTOS_ObjectRtcMilliseconds(&after) == HAL_OK && after - before == 172800000ULL);
  assert(PS_HW6_RTOS_ObjectSleepClockBegin() == HAL_OK);
  rtc_time.Seconds++;
  now += 10;
  PS_HW6_RTOS_ObjectSleepClockFinish();
  assert(g_ps_object_lpbam_probe.sleep_ms == 900);
  assert(PS_HW6_RTOS_ObjectAdvance(now) == 0 && s_ps_object_graph.objects.elapsed_ms == 4650);
  assert(PS_HW6_RTOS_ObjectSleepClockBegin() == HAL_OK);
  rtc_fail = 1;
  PS_HW6_RTOS_ObjectSleepClockFinish();
  assert(g_ps_object_lpbam_probe.fault == 1 && g_ps_object_lpbam_probe.sleep_count == 2);
  g_ps_object_lpbam_probe.fault = 0;
  rtc_fail = 1;
  assert(PS_HW6_RTOS_ObjectSleepClockBegin() == HAL_ERROR && g_ps_object_lpbam_probe.fault == 1);
  return 0;
}
