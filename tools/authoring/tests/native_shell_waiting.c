#include <assert.h>
#include <stdint.h>

typedef uint32_t HAL_StatusTypeDef;
enum { HAL_OK, HAL_ERROR };
enum { PS_UI_ROUTER_PAGE_RUNTIME_HANDOFF = 6,
       PS_HW6_RTOS_DISPLAY_CLOCK_REASON_TRANSFER = 1,
       PS_HW6_RTOS_DISPLAY_CLOCK_REASON_RELEASE = 2,
       PS_HW6_RTOS_DISPLAY_CLOCK_TRANSFER_CAPABILITIES = 1 };
static struct { uint32_t current_page; } g_ps_ui_router_probe;
static struct { uint32_t enabled, deadline_tick; } g_ps_object_lpbam_probe;
static struct {
  uint32_t display_waiting_sequence_frame_count;
  uint32_t display_lpbam_sequence_phase[4];
} g_ps_hw6_owner_probe;
static uint32_t ps_display_blink_next_tick, ps_display_blink_transfer_active;
static uint32_t ps_display_waiting_sequence_frame, ps_display_waiting_sequence_count;
static uint32_t ps_display_blink_visible;
static uint32_t active, resolve_ok, resolve_calls, renders, resets, clocks;
static uint32_t resolved_frame, resolved_period, rendered_frame, render_status;
static uint32_t PS_SceneRuntime_DevelopmentObjectsActive(void) { return active; }
static uint32_t PS_HW6_DisplayOwner_ObjectWaitingPosition(uint32_t now,
  uint32_t *frame, uint32_t *period)
{
  (void)now;
  resolve_calls++;
  *frame = resolved_frame;
  *period = resolved_period;
  return resolve_ok;
}
static void PS_HW6_RTOS_ResetDisplayCursorBlink(uint32_t now)
{ resets++; ps_display_blink_next_tick = now + 50U; }
static uint32_t PS_HW6_RTOS_RequestDisplayClockCapabilities(uint32_t reason, uint32_t caps)
{ (void)reason; (void)caps; clocks++; return 0U; }
static HAL_StatusTypeDef PS_HW6_DisplayOwner_RenderWaitingSequenceFrame(uint32_t frame)
{ renders++; rendered_frame = frame; return render_status; }
#include "waiting.inc"

int main(void)
{
  /* Static suspended package: resolution fails, but shell must never ask it. */
  active = 1U;
  g_ps_object_lpbam_probe.enabled = 1U;
  g_ps_ui_router_probe.current_page = 2U;
  g_ps_hw6_owner_probe.display_waiting_sequence_frame_count = 2U;
  g_ps_hw6_owner_probe.display_lpbam_sequence_phase[1] = 0U;
  ps_display_blink_next_tick = 99U;
  assert(PS_HW6_RTOS_RenderDisplayWaitingSequenceFrame(1U, 100U, 50U) == HAL_OK);
  assert(resolve_calls == 0U && renders == 1U && rendered_frame == 1U);
  assert(ps_display_blink_next_tick == 150U && ps_display_blink_visible == 0U);
  assert(clocks == 2U && ps_display_blink_transfer_active == 0U);

  /* Runtime resolution failure must not retain an expired deadline. */
  g_ps_ui_router_probe.current_page = PS_UI_ROUTER_PAGE_RUNTIME_HANDOFF;
  assert(PS_HW6_RTOS_RenderDisplayWaitingSequenceFrame(1U, 200U, 50U) == HAL_ERROR);
  assert(resolve_calls == 1U && resets == 1U && ps_display_blink_next_tick > 200U);
  assert(renders == 1U && clocks == 2U);

  resolve_ok = 1U;
  resolved_frame = 1U;
  resolved_period = 17U;
  assert(PS_HW6_RTOS_RenderDisplayWaitingSequenceFrame(0U, 300U, 50U) == HAL_OK);
  assert(rendered_frame == 1U && ps_display_blink_visible == 1U);
  assert(ps_display_blink_next_tick == 317U && g_ps_object_lpbam_probe.deadline_tick == 317U);
  resolved_period = 0U;
  assert(PS_HW6_RTOS_RenderDisplayWaitingSequenceFrame(0U, 400U, 50U) == HAL_ERROR);
  assert(ps_display_blink_next_tick > 400U);
  resolved_period = 17U;
  resolved_frame = 2U;
  assert(PS_HW6_RTOS_RenderDisplayWaitingSequenceFrame(0U, 500U, 50U) == HAL_ERROR);
  assert(ps_display_blink_next_tick > 500U);
  g_ps_ui_router_probe.current_page = 2U;
  g_ps_hw6_owner_probe.display_waiting_sequence_frame_count = 0U;
  assert(PS_HW6_RTOS_RenderDisplayWaitingSequenceFrame(0U, 600U, 50U) == HAL_ERROR);
  assert(ps_display_blink_next_tick > 600U);
  g_ps_hw6_owner_probe.display_waiting_sequence_frame_count = 2U;
  render_status = HAL_ERROR;
  assert(PS_HW6_RTOS_RenderDisplayWaitingSequenceFrame(0U, 700U, 50U) == HAL_ERROR);
  assert(ps_display_blink_next_tick > 700U && ps_display_blink_transfer_active == 0U);
  return 0;
}
