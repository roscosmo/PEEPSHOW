#define PS_OBJECT_AWAKE_MAIN awake_main
#include "native_object_awake.c"
#define LS013B7DH05_H
#define LCD_DMA_MAX_ROWS_PER_TRANSFER 48U
#include "ps_lpbam_display_buffers.c"
#include "object_display_under_test.inc"
#define DISPLAY_RENDERER_H
#include "ps_scene_object_display_admission.c"

static ps_scene_runtime_state_scene_t candidate_scene;
static ps_egg_sprite_catalog_t candidate_catalog;
static ps_scene_objects_t bank, saved_bank;
static ps_scene_objects_snapshot_t scratch;
static ps_object_waiting_workspace_t waiting_workspace;
static ps_object_waiting_program_t program, saved_program;
static ps_object_display_workspace_t check_workspace;
static ps_object_display_result_t checked;
static uint8_t payload_before[40000], payload_after[40000];
static uint8_t display_before[DISPLAY_RENDERER_BUFFER_SIZE], reference[DISPLAY_RENDERER_BUFFER_SIZE];

static uint32_t payload_snapshot(uint8_t *out)
{
  uint32_t offset = 0;
#define TAKE(value) do { memcpy(out + offset, &(value), sizeof(value)); offset += sizeof(value); } while (0)
  TAKE(ps_lpbam_display_frame_a); TAKE(ps_lpbam_display_frame_b);
  TAKE(ps_lpbam_display_payload_arena); TAKE(ps_lpbam_display_payload_scratch);
  TAKE(ps_lpbam_display_tx); TAKE(ps_lpbam_display_tx_len); TAKE(ps_lpbam_display_tx_payload_slot);
  TAKE(ps_lpbam_display_payload_slot); TAKE(ps_lpbam_display_payload_slot_capacity);
  TAKE(ps_lpbam_display_payload_slot_len); TAKE(ps_lpbam_display_payload_slot_band);
  TAKE(ps_lpbam_display_payload_slot_occupied); TAKE(ps_lpbam_display_sequence);
  TAKE(ps_lpbam_display_admission); TAKE(ps_lpbam_display_active_sequence_count);
  TAKE(ps_lpbam_display_active_chunk_count); TAKE(ps_lpbam_display_payload_wire_bytes);
  TAKE(ps_lpbam_display_sequence_start_frame); TAKE(ps_lpbam_display_queue_start_slot);
  TAKE(ps_lpbam_display_payload_used); TAKE(ps_lpbam_display_experiment_variant);
  TAKE(ps_lpbam_display_candidate_rows); TAKE(ps_lpbam_display_candidate_row_enabled);
  TAKE(ps_lpbam_display_dirty_row_list); TAKE(ps_lpbam_display_expected_sequence_count);
  TAKE(ps_lpbam_display_first_candidate_row);
#undef TAKE
  assert(offset < sizeof(payload_before));
  return offset;
}

typedef struct { uint32_t calls, fail, hold; } compose_test_t;
static uint32_t synthetic_frame(void *opaque, uint32_t step, uint8_t *destination, uint32_t capacity)
{
  compose_test_t *test = opaque;
  static const uint8_t fill[] = {0, 255, 0xAA, 0x55};
  assert(capacity == DISPLAY_RENDERER_BUFFER_SIZE);
  if (++test->calls == test->fail) { return 0; }
  memset(destination, test->hold ? 255 : fill[step % 4], capacity);
  return 1;
}

static void resource_boundaries(void)
{
  ps_lpbam_display_admission_t result;
  compose_test_t test = {0};
  uint32_t before = payload_snapshot(payload_before);
  assert(PS_LpbamDisplay_CheckFullSceneAnimation(3, synthetic_frame, &test,
    &check_workspace.packing, &result) == HAL_OK);
  assert(result.sequence_used == 3 && result.chunk_used == 18 && result.payload_used_bytes == 10512);
  test.calls = 0;
  assert(PS_LpbamDisplay_CheckFullSceneAnimation(4, synthetic_frame, &test,
    &check_workspace.packing, &result) == HAL_ERROR);
  assert(result.reason == PS_LPBAM_ADMISSION_REASON_CHUNKS && result.chunk_used == 18);
  test = (compose_test_t){0, 0, 1};
  assert(PS_LpbamDisplay_CheckFullSceneAnimation(12, synthetic_frame, &test,
    &check_workspace.packing, &result) == HAL_OK);
  assert(test.calls == 13 && result.chunk_used == 12 && result.payload_used_bytes == 584);
  assert(PS_LpbamDisplay_CheckFullSceneAnimation(13, synthetic_frame, &test,
    &check_workspace.packing, &result) == HAL_ERROR);
  assert(result.reason == PS_LPBAM_ADMISSION_REASON_SEQUENCE && test.calls == 13);
  test = (compose_test_t){0, 4, 1};
  assert(PS_LpbamDisplay_CheckFullSceneAnimation(3, synthetic_frame, &test,
    &check_workspace.packing, &result) == HAL_ERROR);
  assert(result.reason == PS_LPBAM_ADMISSION_REASON_BUILD && test.calls == 4);
  assert(check_workspace.packing.frames_composed == 3 && result.sequence_used == 2);
  assert(PS_LpbamDisplay_CheckFullSceneAnimation(1, NULL, NULL, &check_workspace.packing, &result) == HAL_ERROR);
  assert(PS_LpbamDisplay_CheckFullSceneAnimation(1, synthetic_frame, &test, NULL, &result) == HAL_ERROR);
  assert(PS_LpbamDisplay_CheckFullSceneAnimation(0, synthetic_frame, &test, &check_workspace.packing, &result) == HAL_ERROR);
  assert(payload_snapshot(payload_after) == before && memcmp(payload_before, payload_after, before) == 0);
}

int main(int argc, char **argv)
{
  static ps_egg_context_t saved_catalog;
  static ps_egg_state_loader_probe_t saved_loader;
  static ps_scene_runtime_probe_t saved_runtime;
  static ps_scene_object_graph_t saved_graph;
  ps_egg_v2_profile_result_t profile;
  uint16_t rows[DISPLAY_HEIGHT];
  uint32_t size, expected_failure, step, snapshot_size, status;
  HAL_StatusTypeDef packed;
  assert(argc == 4);
  expected_failure = (uint32_t)atoi(argv[3]);
  baseline_size = read_blob(argv[1], baseline);
  set_hash(argv[1], baseline, baseline_size);
  assert(PS_SceneRuntime_EnterDevelopmentObjects(baseline, baseline_size) == 0);
  saved_catalog = s_ps_egg_runtime_context;
  saved_loader = g_ps_egg_state_loader_probe;
  saved_runtime = g_ps_scene_runtime_probe;
  saved_graph = s_ps_object_graph;
  for (step = 0; step < DISPLAY_HEIGHT; ++step) { rows[step] = (uint16_t)(step + 1); }
  memset(ps_lpbam_display_frame_a, 0xFF, sizeof(ps_lpbam_display_frame_a));
  memset(ps_lpbam_display_frame_b, 0, sizeof(ps_lpbam_display_frame_b));
  assert(PS_LpbamDisplay_BeginPreparedAnimation(rows, DISPLAY_HEIGHT, 2, 1) == HAL_OK);
  assert(PS_LpbamDisplay_AppendPreparedTransition(ps_lpbam_display_frame_a, ps_lpbam_display_frame_b) == HAL_OK);
  assert(PS_LpbamDisplay_AppendPreparedTransition(ps_lpbam_display_frame_b, ps_lpbam_display_frame_a) == HAL_OK);
  assert(PS_LpbamDisplay_FinishPreparedAnimation() == HAL_OK);
  resource_boundaries();
  snapshot_size = payload_snapshot(payload_before);
  memset(s_display_framebuffer, 0xA5, sizeof(s_display_framebuffer));
  memcpy(display_before, s_display_framebuffer, sizeof(display_before));
  s_rotate_ccw = 0;
  size = read_blob(argv[2], candidate);
  set_hash(argv[2], candidate, size);
  assert(PS_EggStateLoader_DecodeV2Candidate(candidate, size, &candidate_scene, &candidate_catalog, &profile) == 0);
  assert(profile.reason == 0 && s_ps_egg_validation_context.blob == NULL);
  assert(PS_SceneObjects_Init(&bank, &candidate_scene.object_definition, 10,
    (uint16_t)(candidate_scene.entry_state_id - 1)) == PS_SCENE_OBJECTS_OK);
  assert(PS_SceneObjects_Advance(&bank, 650) == PS_SCENE_OBJECTS_OK);
  saved_bank = bank;
  status = PS_ObjectWaiting_Build(&bank, candidate_scene.scene_id, &program, &waiting_workspace);
  if (expected_failure == 2)
  { assert(status == PS_OBJECT_WAITING_CAPACITY); }
  else
  {
    assert(status == PS_OBJECT_WAITING_OK);
    saved_program = program;
    status = PS_ObjectDisplay_CheckWaiting(&program, &candidate_catalog, &check_workspace, &checked);
    assert(status == (expected_failure != 0));
    assert(memcmp(&saved_program, &program, sizeof(program)) == 0);
    if (status == 0)
    {
      assert(checked.projection_status == 0 && checked.raster_status == 0);
      assert(checked.failed_step == UINT32_MAX && checked.frames_composed == program.step_count + 1);
    }
    else { assert(checked.payload.reason == PS_LPBAM_ADMISSION_REASON_CHUNKS); }
  }
  assert(memcmp(&saved_bank, &bank, sizeof(bank)) == 0);
  assert(payload_snapshot(payload_after) == snapshot_size && memcmp(payload_before, payload_after, snapshot_size) == 0);
  assert(memcmp(display_before, s_display_framebuffer, sizeof(display_before)) == 0);
  assert(s_rotate_ccw == 0 && s_display_candidate_catalog == NULL);
  assert(memcmp(&saved_catalog, &s_ps_egg_runtime_context, sizeof(saved_catalog)) == 0);
  assert(memcmp(&saved_loader, (const void *)&g_ps_egg_state_loader_probe, sizeof(saved_loader)) == 0);
  assert(memcmp(&saved_runtime, (const void *)&g_ps_scene_runtime_probe, sizeof(saved_runtime)) == 0);
  assert(memcmp(&saved_graph, &s_ps_object_graph, sizeof(saved_graph)) == 0);
  if (expected_failure == 2) { puts("schedule rejection isolated"); return 0; }

  /* Only after isolation checks, publish candidate catalogs as the independent
   * existing-packer oracle. IDs deliberately collide with the old package. */
  assert(PS_EggStateLoader_LoadDevelopment(candidate, size, &candidate_scene) == 0);
  assert(PS_ObjectWaiting_Project(&program, 0, &scratch, &model) == PS_OBJECT_WAITING_OK);
  assert(DisplayRenderer_CopySceneModelFrame(&model, reference, sizeof(reference)) == 1);
  if (expected_failure == 0)
  { assert(memcmp(reference, check_workspace.packing.previous, sizeof(reference)) == 0); }
  memcpy(ps_lpbam_display_frame_a, reference, sizeof(reference));
  packed = PS_LpbamDisplay_BeginPreparedAnimation(rows, DISPLAY_HEIGHT, (uint16_t)program.step_count, 0);
  for (step = 0; packed == HAL_OK && step < program.step_count; ++step)
  {
    assert(PS_ObjectWaiting_Project(&program, (step + 1) % program.step_count, &scratch, &model) == PS_OBJECT_WAITING_OK);
    assert(DisplayRenderer_CopySceneModelFrame(&model, &ps_lpbam_display_frame_b[0][0], sizeof(ps_lpbam_display_frame_b)) == 1);
    packed = PS_LpbamDisplay_AppendPreparedTransition(ps_lpbam_display_frame_a, ps_lpbam_display_frame_b);
    memcpy(ps_lpbam_display_frame_a, ps_lpbam_display_frame_b, sizeof(ps_lpbam_display_frame_a));
  }
  if (packed == HAL_OK) { packed = PS_LpbamDisplay_FinishPreparedAnimation(); }
  assert(packed == (expected_failure ? HAL_ERROR : HAL_OK));
  assert(memcmp(&checked.payload, &ps_lpbam_display_admission, sizeof(checked.payload)) == 0);
  printf("exact resources: steps=%u chunks=%u bytes=%u; live data unchanged\n",
    checked.payload.sequence_used, checked.payload.chunk_used, checked.payload.payload_used_bytes);

  /* A missing candidate frame may not be supplied by the now-active catalog. */
  if (program.quantum_ms != 0)
  {
    ps_egg_sprite_catalog_t missing = candidate_catalog;
    missing.frame_count = 0;
    assert(PS_ObjectDisplay_CheckWaiting(&program, &missing, &check_workspace, &checked) == 1);
    assert(checked.raster_status == 1 && checked.payload.reason == PS_LPBAM_ADMISSION_REASON_BUILD);
    assert(s_display_candidate_catalog == NULL && s_rotate_ccw == 0);
  }
  assert(PS_ObjectDisplay_CheckWaiting(NULL, &candidate_catalog, &check_workspace, &checked) == 1);
  assert(PS_ObjectDisplay_CheckWaiting(&program, NULL, &check_workspace, &checked) == 1);
  assert(PS_ObjectDisplay_CheckWaiting(&program, &candidate_catalog, NULL, &checked) == 1);
  assert(PS_ObjectDisplay_CheckWaiting(&program, &candidate_catalog, &check_workspace, NULL) == 1);
  assert(PS_EggStateLoader_DecodeV2Candidate(candidate, 1, &candidate_scene, &candidate_catalog, &profile) == 1);
  assert(candidate_scene.state_count == 0 && candidate_catalog.records == NULL);
  assert(memcmp(display_before, s_display_framebuffer, sizeof(display_before)) == 0);
  return 0;
}
