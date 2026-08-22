#include "ps_scene_runtime.h"

#include <string.h>

#include "knobs_autogen.h"

#define PS_SCENE_RUNTIME_PRESENTATION_SHELL_BASE (0x100UL)
#define PS_SCENE_RUNTIME_PRESENTATION_PROOF_BASE (0x200UL)
#define PS_SCENE_RUNTIME_ELEMENT_CURSOR          (1UL)
#define PS_SCENE_RUNTIME_ELEMENT_MARKER          (5UL)

static ps_scene_waiting_visual_t s_ps_scene_runtime_waiting_visual;

volatile uint32_t g_ps_scene_runtime_waiting_demo_enable;
volatile ps_scene_runtime_probe_t g_ps_scene_runtime_probe =
{
  .api_version = PS_SCENE_RUNTIME_API_VERSION,
  .last_status = PS_SCENE_RUNTIME_STATUS_NOT_RUN
};

const ps_scene_waiting_visual_t *PS_SceneRuntime_ResolveShellStateWaitingVisual(
  uint32_t page,
  uint32_t focus_index,
  const ps_scene_waiting_visual_bounds_t *cursor_bounds)
{
  ps_scene_waiting_visual_t *visual = &s_ps_scene_runtime_waiting_visual;
  ps_scene_waiting_visual_element_t *cursor;
  ps_scene_waiting_visual_element_t *marker;
  uint32_t step;
  uint32_t proof_enabled =
    (g_ps_scene_runtime_waiting_demo_enable != 0UL) ? 1UL : 0UL;

  g_ps_scene_runtime_probe.resolve_count++;
  g_ps_scene_runtime_probe.scene_type = PS_SCENE_RUNTIME_SCENE_TYPE_STATE;
  g_ps_scene_runtime_probe.page = page;
  g_ps_scene_runtime_probe.focus_index = focus_index;
  g_ps_scene_runtime_probe.last_status = PS_SCENE_RUNTIME_STATUS_NOT_RUN;

  if ((cursor_bounds == NULL) || (cursor_bounds->width == 0U) ||
      (cursor_bounds->height == 0U))
  {
    g_ps_scene_runtime_probe.reject_count++;
    g_ps_scene_runtime_probe.last_status = 1UL;
    return NULL;
  }

  (void)memset(visual, 0, sizeof(*visual));
  visual->api_version = PS_SCENE_WAITING_VISUAL_API_VERSION;
  visual->presentation_id =
    (proof_enabled != 0UL ? PS_SCENE_RUNTIME_PRESENTATION_PROOF_BASE :
                           PS_SCENE_RUNTIME_PRESENTATION_SHELL_BASE) + page;
  visual->phase_quantum_ms =
    (uint32_t)KNOB_DISPLAY_CURSOR_BLINK_PERIOD_MS;
  visual->sequence_step_count = (proof_enabled != 0UL) ? 6UL : 4UL;
  visual->settled_sequence_step = 1UL;
  visual->cycle_policy = PS_SCENE_WAITING_VISUAL_CYCLE_LOOP;
  visual->rebase_policy = PS_SCENE_WAITING_VISUAL_REBASE_NEW_STATE;
  visual->element_count = (proof_enabled != 0UL) ? 2UL : 1UL;

  cursor = &visual->elements[0];
  cursor->element_id = PS_SCENE_RUNTIME_ELEMENT_CURSOR;
  cursor->visual_source_id = PS_SCENE_WAITING_VISUAL_SOURCE_SHELL_CURSOR;
  cursor->phase_count = 2UL;
  cursor->phase_visual_id[0] = 1UL;
  cursor->phase_visual_id[1] = 2UL;
  cursor->logical_bounds = *cursor_bounds;
  for (step = 0UL; step < visual->sequence_step_count; ++step)
  {
    cursor->sequence_phase[step] = step & 1UL;
  }

  if (visual->element_count == 2UL)
  {
    marker = &visual->elements[1];
    marker->element_id = PS_SCENE_RUNTIME_ELEMENT_MARKER;
    marker->visual_source_id =
      PS_SCENE_WAITING_VISUAL_SOURCE_THREE_PHASE_MARKER;
    marker->phase_count = 3UL;
    marker->phase_visual_id[0] = 10UL;
    marker->phase_visual_id[1] = 11UL;
    marker->phase_visual_id[2] = 12UL;
    marker->logical_bounds.x = 160U;
    marker->logical_bounds.y = 0U;
    marker->logical_bounds.width = 8U;
    marker->logical_bounds.height = 24U;
    for (step = 0UL; step < visual->sequence_step_count; ++step)
    {
      marker->sequence_phase[step] = (step + 2UL) % 3UL;
    }
  }

  g_ps_scene_runtime_probe.presentation_id = visual->presentation_id;
  g_ps_scene_runtime_probe.sequence_step_count =
    visual->sequence_step_count;
  g_ps_scene_runtime_probe.element_count = visual->element_count;
  g_ps_scene_runtime_probe.last_status = 0UL;
  return visual;
}
