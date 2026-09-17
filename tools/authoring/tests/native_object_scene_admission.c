#define PS_OBJECT_CANDIDATE_MAIN candidate_queue_main
#include "native_object_candidate_queue.c"

static ps_egg_context_t live_catalog;
static ps_egg_state_loader_probe_t live_loader;
static ps_scene_runtime_probe_t live_probe;
static ps_scene_object_graph_t live_graph;
static ps_scene_objects_snapshot_t live_snapshot;
static uint8_t live_framebuffer[DISPLAY_RENDERER_BUFFER_SIZE];
static uint8_t payload_before[40000], payload_after[40000];
static uint32_t payload_size;
#include "candidate_payload_snapshot.inc"

static void unchanged(void)
{
  assert(memcmp(&live_catalog, &s_ps_egg_runtime_context, sizeof(live_catalog)) == 0);
  assert(memcmp(&live_loader, (const void *)&g_ps_egg_state_loader_probe, sizeof(live_loader)) == 0);
  assert(memcmp(&live_probe, (const void *)&g_ps_scene_runtime_probe, sizeof(live_probe)) == 0);
  assert(memcmp(&live_graph, &s_ps_object_graph, sizeof(live_graph)) == 0);
  assert(memcmp(&live_snapshot, &s_ps_object_snapshot, sizeof(live_snapshot)) == 0);
  assert(memcmp(live_framebuffer, s_display_framebuffer, sizeof(live_framebuffer)) == 0);
  assert(payload_snapshot(payload_after) == payload_size);
  assert(memcmp(payload_before, payload_after, payload_size) == 0);
  assert(s_display_candidate_catalog == NULL);
}

int main(int argc, char **argv)
{
  ps_hw6_object_scene_set_result_t result, saved_result, refused_result;
  static ps_object_waiting_program_t frozen;
  static ps_egg_sprite_catalog_t borrowed;
  ULONG stale[4];
  uint16_t rows[DISPLAY_HEIGHT];
  uint32_t size, count, token, index, kind;
  assert(argc == 4);
  kind = (uint32_t)strtoul(argv[3], NULL, 10);
  baseline_size = read_blob(argv[1], baseline);
  set_hash(argv[1], baseline, baseline_size);
  assert(PS_SceneRuntime_EnterDevelopmentObjects(baseline, baseline_size) == 0);
  assert(PS_SceneRuntime_AdvanceDevelopmentObjects(750) == 0);
  live_catalog = s_ps_egg_runtime_context;
  live_loader = g_ps_egg_state_loader_probe;
  live_probe = g_ps_scene_runtime_probe;
  live_graph = s_ps_object_graph;
  live_snapshot = s_ps_object_snapshot;
  memset(s_display_framebuffer, 0xA5, sizeof(s_display_framebuffer));
  memcpy(live_framebuffer, s_display_framebuffer, sizeof(live_framebuffer));
  for (index = 0; index < DISPLAY_HEIGHT; ++index) { rows[index] = (uint16_t)(index + 1); }
  memset(ps_lpbam_display_frame_a, 0xFF, sizeof(ps_lpbam_display_frame_a));
  memset(ps_lpbam_display_frame_b, 0, sizeof(ps_lpbam_display_frame_b));
  assert(PS_LpbamDisplay_BeginPreparedAnimation(rows, DISPLAY_HEIGHT, 2, 1) == HAL_OK);
  assert(PS_LpbamDisplay_AppendPreparedTransition(ps_lpbam_display_frame_a, ps_lpbam_display_frame_b) == HAL_OK);
  assert(PS_LpbamDisplay_AppendPreparedTransition(ps_lpbam_display_frame_b, ps_lpbam_display_frame_a) == HAL_OK);
  assert(PS_LpbamDisplay_FinishPreparedAnimation() == HAL_OK);
  payload_size = payload_snapshot(payload_before);
  size = read_blob(argv[2], candidate);
  set_hash(argv[2], candidate, size);

  if (kind == 4)
  {
    assert(PS_HW6_ObjectCandidate_CheckSceneSet(candidate, size, &result) == 0);
    assert(result.scene_count == 8 && result.checked == 8 && result.failed_scene == 0 && sends == 8);
    withhold_scene = 6;
    assert(PS_HW6_ObjectCandidate_CheckSceneSet(candidate, size, &result) == 1);
    assert(result.checked == 5 && result.failed_scene == 6 && sends == 14);
    assert(ps_candidate_busy);
    PS_HW6_RTOS_CandidateDisplay(queued);
    PS_HW6_RTOS_CandidateReap();
    complete(0);
    assert(result.checked == 5 && sends == 14);
    unchanged();
    puts("eight-scene bound and stopped batch passed");
    return 0;
  }

  if (kind == 1 || kind == 2)
  {
    assert(PS_HW6_ObjectCandidate_CheckSceneSet(candidate, size, &result) == 1);
    assert(result.scene_count == 2 && result.checked == 1 && result.failed_scene == 2);
    assert(g_ps_object_candidate_probe.requested_scene == 2);
    assert(g_ps_object_candidate_probe.profile_status == 0);
    assert(!ps_candidate_busy);
    if (kind == 1)
    {
      complete(1);
      assert(g_ps_object_candidate_probe.schedule_status == 0 && g_ps_object_candidate_probe.raster_status == 0);
      assert(g_ps_object_candidate_probe.payload_status == 1);
      assert(g_ps_object_candidate_probe.payload_reason == PS_LPBAM_ADMISSION_REASON_CHUNKS);
      assert(sends == 2);
    }
    else
    {
      assert(g_ps_object_candidate_probe.schedule_status != 0 && sends == 1);
      assert(g_ps_object_candidate_probe.display_status == UINT32_MAX);
    }
    unchanged();
    puts("later scene resource rejection preserved live scene");
    return 0;
  }

  assert(PS_HW6_ObjectCandidate_CheckScene(candidate, size, 2) == 0);
  complete(0);
  assert(g_ps_object_candidate_probe.mode == 3 && g_ps_object_candidate_probe.api_version == 2);
  assert(g_ps_object_candidate_probe.requested_scene == 2 && g_ps_object_candidate_probe.scene_id == 2);
  assert(g_ps_object_candidate_probe.scene_count == 2 && ps_candidate_program.scene_id == 2);
  assert(ps_candidate_program.base.objects[1].effective.x == 100);
  assert(ps_candidate_program.base.elapsed_ms == 0);
  if (kind == 3)
  {
    assert(g_ps_object_candidate_probe.steps == 1 && g_ps_object_candidate_probe.quantum_ms == 0);
  }
  else
  {
    assert(g_ps_object_candidate_probe.steps == 4 && g_ps_object_candidate_probe.quantum_ms == 400);
    assert(g_ps_object_candidate_probe.frames_composed == 5);
    assert(g_ps_object_candidate_probe.chunks == 8 && g_ps_object_candidate_probe.bytes == 4672);
  }
  unchanged();
  assert(PS_HW6_ObjectCandidate_CheckScene(candidate, size, 0) == 0);
  assert(g_ps_object_candidate_probe.scene_id == 1 && ps_candidate_program.scene_id == 1);
  assert(ps_candidate_program.base.objects[1].effective.x == 32);
  count = sends;
  assert(PS_HW6_ObjectCandidate_CheckSceneSet(candidate, size, &result) == 0);
  assert(result.scene_count == 2 && result.checked == 2 && result.failed_scene == 0);
  assert(result.request_id == g_ps_object_candidate_probe.display_complete && sends == count + 2);
  unchanged();

  /* Reject the later raster even though scene 1 admitted successfully. */
  inject_missing_scene = 2;
  assert(PS_HW6_ObjectCandidate_CheckSceneSet(candidate, size, &result) == 1);
  complete(1);
  assert(result.checked == 1 && result.failed_scene == 2 && result.scene_count == 2);
  assert(g_ps_object_candidate_probe.raster_status == 1 && g_ps_object_candidate_probe.payload_reason == 5);
  inject_missing_scene = 0;
  unchanged();

  /* A batch never continues after a timed-out child, even if it later succeeds. */
  withhold_scene = 2;
  assert(PS_HW6_ObjectCandidate_CheckSceneSet(candidate, size, &result) == 1);
  assert(ps_candidate_busy && result.checked == 1 && result.failed_scene == 2);
  saved_result = result;
  frozen = ps_candidate_program;
  borrowed = ps_candidate_catalog;
  token = g_ps_object_candidate_probe.request_id;
  count = sends;
  memset(candidate, 0x5A, size); /* Caller may release/reuse its transport buffer. */
  assert(PS_HW6_ObjectCandidate_CheckSceneSet(candidate, size, &refused_result) == 1);
  assert(refused_result.checked == 0 && refused_result.scene_count == 0 && refused_result.request_id == 0);
  assert(PS_HW6_ObjectCandidate_CheckScene(candidate, size, 1) == 1);
  assert(PS_HW6_RTOS_InstalledObjectCheck(candidate, size, NULL) == 1);
  assert(g_ps_object_candidate_probe.request_id == token && sends == count);
  assert(memcmp(&frozen, &ps_candidate_program, sizeof(frozen)) == 0);
  assert(memcmp(&borrowed, &ps_candidate_catalog, sizeof(borrowed)) == 0);
  memcpy(stale, queued, sizeof(stale));
  stale[2]--; stale[3] = ~stale[2];
  PS_HW6_RTOS_CandidateDisplay(stale);
  PS_HW6_RTOS_CandidateReap();
  assert(ps_candidate_busy);
  PS_HW6_RTOS_CandidateDisplay(queued);
  PS_HW6_RTOS_CandidateDisplay(queued);
  PS_HW6_RTOS_CandidateReap();
  complete(0);
  assert(g_ps_object_candidate_probe.late_completions == 1 && sends == count);
  assert(memcmp(&saved_result, &result, sizeof(result)) == 0);
  unchanged();
  withhold_scene = 0;
  size = read_blob(argv[2], candidate);
  set_hash(argv[2], candidate, size);
  assert(PS_HW6_ObjectCandidate_CheckSceneSet(candidate, size, &result) == 0);
  assert(result.checked == 2 && result.failed_scene == 0);

  count = sends;
  assert(PS_HW6_ObjectCandidate_CheckScene(candidate, size, 3) == 1);
  assert(!ps_candidate_busy && g_ps_object_candidate_probe.profile_status == 1 && sends == count);
  assert(PS_HW6_RTOS_InstalledObjectCheck(candidate, size, NULL) == 1);
  assert(g_ps_object_candidate_probe.profile_reason == PS_EGG_V2_PROFILE_SCENE_COUNT && sends == count);
  assert(PS_HW6_ObjectCandidate_CheckSceneSet(NULL, size, &result) == 1);
  assert(result.checked == 0 && result.request_id == 0);
  assert(PS_HW6_ObjectCandidate_CheckSceneSet(candidate, size, NULL) == 1);
  assert(PS_HW6_ObjectCandidate_CheckScene(candidate, 65537, 2) == 1);
  unchanged();

  /* Existing display/clock refusal rules apply to the new path too. */
  g_ps_hw6_owner_probe.display_lpbam_prearmed = 1;
  count = display_clock_calls;
  assert(PS_HW6_ObjectCandidate_CheckScene(candidate, size, 2) == 1);
  complete(2);
  assert(display_clock_calls == count && g_ps_object_candidate_probe.frames_composed == 0);
  g_ps_hw6_owner_probe.display_lpbam_prearmed = 0;
  g_ps_hw6_rtos_probe.runtime_active_capabilities = PS_HW6_RTOS_RUNTIME_CLOCK_REACTIVE_CAPABILITIES;
  count = runtime_clock_calls;
  assert(PS_HW6_ObjectCandidate_CheckScene(candidate, size, 2) == 0);
  assert(runtime_clock_calls == count + 3);
  g_ps_hw6_rtos_probe.runtime_active_capabilities = 0;
  unchanged();
  puts("selected and all-scene owner admission, rejection and late-copy lifetime passed");
  return 0;
}
