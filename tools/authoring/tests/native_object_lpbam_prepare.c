#define PS_OBJECT_AWAKE_MAIN awake_main
#include "native_object_awake.c"
#include "ps_hw6_object_development.h"
#include "ps_ui_router.h"
#define LS013B7DH05_H
#define LCD_DMA_MAX_ROWS_PER_TRANSFER 48U
#include "ps_lpbam_display_buffers.c"

static uint32_t ps_hw6_display_lpbam_active, ps_hw6_display_lpbam_prearmed;
static uint32_t ps_hw6_display_lpbam_compiled;
static struct { uint32_t display_success, display_ui_page; } g_ps_hw6_owner_probe;
static ps_scene_render_model_t ps_hw6_development_display_model;
volatile ps_hw6_object_lpbam_prepare_probe_t g_ps_object_lpbam_prepare_probe;
#include "object_lpbam_under_test.inc"

static ps_object_waiting_program_t program, saved_program;
static ps_scene_objects_snapshot_t scratch;
static ps_scene_objects_t saved_bank;
static uint8_t pixels[DISPLAY_RENDERER_BUFFER_SIZE], expected[DISPLAY_RENDERER_BUFFER_SIZE];
static uint8_t saved_frame[DISPLAY_RENDERER_BUFFER_SIZE];

static void replay(uint32_t sequence)
{
  const ps_lpbam_display_sequence_entry_t *entry = &ps_lpbam_display_sequence[sequence];
  uint32_t index;
  for (index = entry->first_chunk; index < entry->first_chunk + entry->chunk_count; ++index)
  {
    const uint8_t *bytes = ps_lpbam_display_tx[index];
    uint32_t offset = 1, size = ps_lpbam_display_tx_len[index];
    assert(size >= 23 && bytes[0] == 1 && bytes[size - 1] == 0 && bytes[size - 2] == 0);
    while (offset < size - 2)
    {
      uint32_t row = bytes[offset++];
      assert(row >= 1 && row <= DISPLAY_HEIGHT && offset + LINE_WIDTH < size - 2);
      memcpy(pixels + (row - 1) * LINE_WIDTH, bytes + offset, LINE_WIDTH);
      offset += LINE_WIDTH;
      assert(bytes[offset++] == 0);
    }
    assert(offset == size - 2);
  }
}

int main(int argc, char **argv)
{
  uint32_t size, mode, next, step;
  HAL_StatusTypeDef status;
  assert(argc == 3);
  mode = (uint32_t)atoi(argv[2]);
  size = read_blob(argv[1], candidate);
  set_hash(argv[1], candidate, size);
  assert(PS_SceneRuntime_EnterDevelopmentObjects(candidate, size) == 0);
  assert(PS_SceneRuntime_AdvanceDevelopmentObjects(650) == 0);
  assert(PS_SceneRuntime_BuildDevelopmentWaiting(&program) == PS_OBJECT_WAITING_OK);
  assert(PS_SceneRuntime_ProjectDevelopmentObjects(&ps_hw6_development_display_model, &next) == 0);
  saved_bank = s_ps_object_graph.objects;
  saved_program = program;
  memset(s_display_framebuffer, 0xA5, sizeof(s_display_framebuffer));
  memcpy(saved_frame, s_display_framebuffer, sizeof(saved_frame));
  g_ps_hw6_owner_probe.display_success = 1;
  g_ps_hw6_owner_probe.display_ui_page = PS_UI_ROUTER_PAGE_RUNTIME_HANDOFF;
  status = PS_HW6_DisplayOwner_PrepareDevelopmentObjectWaiting(&program);
  assert(memcmp(&saved_bank, &s_ps_object_graph.objects, sizeof(saved_bank)) == 0);
  assert(memcmp(&saved_program, &program, sizeof(program)) == 0);
  assert(memcmp(saved_frame, s_display_framebuffer, sizeof(saved_frame)) == 0);
  assert(ps_hw6_display_lpbam_compiled == 0 && ps_hw6_display_lpbam_active == 0 && ps_hw6_display_lpbam_prearmed == 0);
  if (mode == 1)
  {
    assert(status == HAL_ERROR && g_ps_object_lpbam_prepare_probe.reason == PS_LPBAM_ADMISSION_REASON_CHUNKS);
    return 0;
  }
  assert(status == HAL_OK && g_ps_object_lpbam_prepare_probe.reason == 0);
  assert(program.step_count == (mode == 2 ? 1U : 4U));
  assert(program.quantum_ms == (mode == 2 ? 0U : 400U));
  assert(g_ps_object_lpbam_prepare_probe.frames_composed == program.step_count + 1);
  assert(g_ps_object_lpbam_prepare_probe.sequence_used == program.step_count);
  assert(g_ps_object_lpbam_prepare_probe.payload_used_bytes <= g_ps_object_lpbam_prepare_probe.payload_capacity_bytes);
  printf("steps=%u chunks=%u bytes=%u/%u remaining=%u\n", (unsigned)program.step_count,
    (unsigned)g_ps_object_lpbam_prepare_probe.chunk_used,
    (unsigned)g_ps_object_lpbam_prepare_probe.payload_used_bytes,
    (unsigned)g_ps_object_lpbam_prepare_probe.payload_capacity_bytes,
    (unsigned)program.initial_remaining_ms);
  assert(DisplayRenderer_CopySceneModelFrame(&ps_hw6_development_display_model, pixels, sizeof(pixels)) == 1);
  for (step = 0; step < program.step_count * 3; ++step)
  {
    replay(step % program.step_count);
    assert(PS_ObjectWaiting_Project(&program, (step + 1) % program.step_count, &scratch, &model) == PS_OBJECT_WAITING_OK);
    assert(DisplayRenderer_CopySceneModelFrame(&model, expected, sizeof(expected)) == 1);
    assert(memcmp(pixels, expected, sizeof(pixels)) == 0);
    assert(memcmp(saved_frame, s_display_framebuffer, sizeof(saved_frame)) == 0);
  }
  ps_hw6_display_lpbam_active = 1;
  assert(PS_HW6_DisplayOwner_PrepareDevelopmentObjectWaiting(&program) == HAL_ERROR);
  ps_hw6_display_lpbam_active = 0;
  ps_hw6_display_lpbam_prearmed = 1;
  assert(PS_HW6_DisplayOwner_PrepareDevelopmentObjectWaiting(&program) == HAL_ERROR);
  ps_hw6_display_lpbam_prearmed = 0;
  ps_hw6_display_lpbam_compiled = 1;
  assert(PS_HW6_DisplayOwner_PrepareDevelopmentObjectWaiting(&program) == HAL_ERROR);
  ps_hw6_display_lpbam_compiled = 0;
  ps_hw6_development_display_model.content_revision++;
  assert(PS_HW6_DisplayOwner_PrepareDevelopmentObjectWaiting(&program) == HAL_ERROR);
  assert(DisplayRenderer_CopySceneModelFrame(NULL, pixels, sizeof(pixels)) == 0);
  assert(DisplayRenderer_CopySceneModelFrame(&model, s_display_framebuffer, sizeof(pixels)) == 0);
  assert(DisplayRenderer_CopySceneModelFrame(&model, pixels, sizeof(pixels) - 1) == 0);
  assert(memcmp(saved_frame, s_display_framebuffer, sizeof(saved_frame)) == 0);
  return 0;
}
