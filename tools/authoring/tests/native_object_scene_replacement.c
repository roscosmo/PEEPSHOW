#define PS_OBJECT_CANDIDATE_MAIN candidate_queue_main
#include "native_object_candidate_queue.c"

static ps_scene_object_graph_t saved_graph;
static ps_egg_context_t saved_catalog;
static ps_scene_objects_snapshot_t saved_snapshot;
static uint32_t saved_activation, saved_state_activation;

static void save_source(void)
{
  saved_graph = s_ps_object_graph;
  saved_catalog = s_ps_egg_runtime_context;
  saved_snapshot = s_ps_object_snapshot;
  saved_activation = PS_SceneRuntime_SceneActivation();
  saved_state_activation = PS_SceneRuntime_StateActivation();
}

static void source_unchanged(void)
{
  assert(memcmp(&saved_graph, &s_ps_object_graph, sizeof(saved_graph)) == 0);
  assert(memcmp(&saved_catalog, &s_ps_egg_runtime_context, sizeof(saved_catalog)) == 0);
  assert(memcmp(&saved_snapshot, &s_ps_object_snapshot, sizeof(saved_snapshot)) == 0);
  assert(PS_SceneRuntime_SceneActivation() == saved_activation);
  assert(PS_SceneRuntime_StateActivation() == saved_state_activation);
  assert(PS_SceneRuntime_StateSceneActive());
  assert(PS_SceneRuntime_TakeShellExitRequest() == 0);
}

static void hardware_fixture(void)
{
  uint32_t next, status;
  PS_SceneRuntime_SetObjectSceneAdmission(PS_HW6_RTOS_ObjectSceneCheck);
  status = PS_SceneRuntime_EnterDevelopmentSceneSet(candidate, 3440);
  if (status != 0)
  {
    fprintf(stderr, "entry=%u loader=%u candidate=%u profile=%u reason=%u graph=%u raster=%u payload=%u\n",
      status, g_ps_egg_validation_probe.reason, g_ps_object_candidate_probe.status,
      g_ps_object_candidate_probe.profile_status, g_ps_object_candidate_probe.profile_reason,
      g_ps_object_candidate_probe.graph_status, g_ps_object_candidate_probe.raster_status,
      g_ps_object_candidate_probe.payload_reason);
  }
  assert(status == 0);
  assert(g_ps_object_candidate_probe.chunks == 8);
  assert(g_ps_object_candidate_probe.bytes == 4672);
  assert(PS_SceneRuntime_AdvanceDevelopmentObjects(650) == 0);
  assert(PS_SceneRuntime_HandleStateSceneInput(1, 4) == PS_SCENE_RUNTIME_INPUT_APPLIED);
  assert(g_ps_object_candidate_probe.requested_scene == 1);
  assert(s_ps_object_graph.variables[0] == 1);
  assert(s_ps_object_graph.objects.elapsed_ms == 650);
  assert(PS_SceneRuntime_HandleStateSceneInput(1, 1) == PS_SCENE_RUNTIME_INPUT_APPLIED);
  assert(g_ps_scene_runtime_probe.scene_id == 2 && g_ps_scene_runtime_probe.state_id == 1);
  assert(s_ps_object_graph.variables[0] == 0 && s_ps_object_graph.objects.elapsed_ms == 0);
  assert(PS_SceneRuntime_AdvanceDevelopmentObjects(900) == 0);
  assert(PS_SceneRuntime_HandleStateSceneInput(1, 4) == PS_SCENE_RUNTIME_INPUT_APPLIED);
  assert(g_ps_object_candidate_probe.requested_scene == 2);
  assert(s_ps_object_graph.objects.elapsed_ms == 900);
  assert(PS_SceneRuntime_HandleStateSceneInput(1, 2) == PS_SCENE_RUNTIME_INPUT_APPLIED);
  assert(g_ps_scene_runtime_probe.scene_id == 1 && g_ps_scene_runtime_probe.state_id == 1);
  assert(s_ps_object_graph.variables[0] == 0 && s_ps_object_graph.objects.elapsed_ms == 0);
  assert(PS_SceneRuntime_ProjectDevelopmentObjects(&model, &next) == 0);
  assert(s_ps_object_snapshot.objects[0].step == 0);
  assert(s_ps_object_snapshot.objects[1].effective.x == 32);
  assert((s_ps_object_snapshot.objects[2].effective.flags & 1) == 0);
  assert(!ps_candidate_busy);
  puts("labelled HOME/AWAY fixture: real owner raster admission and fresh replacement passed");
}

int main(int argc, char **argv)
{
  uint32_t size, result, next, trial, token;
  assert(argc == 2 || argc == 3);
  size = read_blob(argv[1], candidate);
  set_hash(argv[1], candidate, size);
  if (argc == 3)
  {
    assert(size == 3440);
    hardware_fixture();
    return 0;
  }
  assert(PS_SceneRuntime_EnterDevelopmentObjects(candidate, size) == 1);
  assert(PS_SceneRuntime_EnterDevelopmentSceneSet(candidate, size) == 1);
  PS_SceneRuntime_SetObjectSceneAdmission(PS_HW6_RTOS_ObjectSceneCheck);
  inject_missing_scene = 2;
  assert(PS_SceneRuntime_EnterDevelopmentSceneSet(candidate, size) == 1);
  assert(!PS_SceneRuntime_StateSceneActive());
  inject_missing_scene = 0;
  assert(PS_SceneRuntime_EnterDevelopmentSceneSet(candidate, size) == 0);
  assert(!PS_SceneRuntime_InstalledObjectsActive());
  assert(PS_SceneRuntime_AdvanceDevelopmentObjects(750) == 0);
  assert(PS_SceneRuntime_ProjectDevelopmentObjects(&model, &next) == 0);
  save_source();
  for (trial = 0; trial < 5; ++trial)
  {
    inject_missing_scene = trial == 0 ? 2 : 0;
    display_clock_failure = trial == 1 ? 1 : 0;
    send_status = trial == 2 ? 1 : 0;
    display_release_failure = trial == 3 ? 1 : 0;
    withhold_scene = trial == 4 ? 2 : 0;
    result = PS_SceneRuntime_HandleStateSceneInput(1, 1);
    assert(result == PS_SCENE_RUNTIME_INPUT_ERROR);
    assert(PS_SceneRuntime_ObjectReplacementRejected());
    source_unchanged();
  }
  token = g_ps_object_candidate_probe.request_id;
  assert(ps_candidate_busy);
  withhold_scene = 0;
  assert(PS_SceneRuntime_HandleStateSceneInput(1, 1) == PS_SCENE_RUNTIME_INPUT_ERROR);
  assert(g_ps_object_candidate_probe.request_id == token);
  source_unchanged();
  PS_HW6_RTOS_CandidateDisplay(queued);
  PS_HW6_RTOS_CandidateReap();
  complete(0);
  source_unchanged(); /* Late success is not scene replacement. */

  assert(PS_SceneRuntime_HandleStateSceneInput(1, 1) == PS_SCENE_RUNTIME_INPUT_APPLIED);
  assert(!PS_SceneRuntime_ObjectReplacementRejected());
  assert(g_ps_scene_runtime_probe.scene_id == 2 && g_ps_scene_runtime_probe.state_id == 1);
  assert(PS_SceneRuntime_SceneActivation() == saved_activation + 1);
  assert(s_ps_object_graph.scene == s_ps_scene_runtime_state_scene);
  assert(s_ps_object_graph.objects.elapsed_ms == 0 && s_ps_object_graph.variables[0] == 20);
  assert(memcmp(&saved_catalog, &s_ps_egg_runtime_context, sizeof(saved_catalog)) == 0);
  assert(PS_SceneRuntime_ProjectDevelopmentObjects(&model, &next) == 0);
  assert(s_ps_object_snapshot.objects[0].step == 0);
  assert(PS_SceneRuntime_AdvanceDevelopmentObjects(650) == 0);
  /* The same A event is local in scene 2. It was not replayed at entry. */
  assert(PS_SceneRuntime_HandleStateSceneInput(1, 1) == PS_SCENE_RUNTIME_INPUT_APPLIED);
  assert(g_ps_object_candidate_probe.requested_scene == 2);
  assert(g_ps_scene_runtime_probe.state_id == 2 && s_ps_object_graph.variables[0] == 21);
  assert(s_ps_object_graph.objects.elapsed_ms == 650);
  assert(PS_SceneRuntime_HandleStateSceneInput(1, 2) == PS_SCENE_RUNTIME_INPUT_APPLIED);
  assert(g_ps_scene_runtime_probe.scene_id == 1 && g_ps_scene_runtime_probe.state_id == 1);
  assert(s_ps_object_graph.variables[0] == 10 && s_ps_object_graph.objects.elapsed_ms == 0);
  assert(PS_SceneRuntime_HandleStateSceneInput(1, 1) == PS_SCENE_RUNTIME_INPUT_APPLIED);
  assert(s_ps_object_graph.variables[0] == 20 && s_ps_object_graph.objects.elapsed_ms == 0);
  assert(g_ps_scene_runtime_probe.state_id == 1);
  PS_SceneRuntime_ExitStateScene();
  assert(s_ps_object_scene_blob == NULL && s_ps_object_scene_size == 0);
  puts("fresh replacement, exact owner admission, rejection and late completion passed");
  return 0;
}
