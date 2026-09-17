#define PS_OBJECT_CANDIDATE_MAIN candidate_queue_main
#include "native_object_candidate_queue.c"

static ps_scene_object_graph_t saved_graph;
static ps_egg_context_t saved_catalog;
static ps_scene_objects_snapshot_t saved_snapshot;
static uint32_t saved_activation, saved_state_activation;
static uint32_t installed_test;

static uint32_t enter(uint32_t size)
{
  if (installed_test)
  { return PS_SceneRuntime_EnterStateScene() == PS_SCENE_RUNTIME_INDEX_INVALID ? 1U : 0U; }
  return PS_SceneRuntime_EnterDevelopmentSceneSet(candidate, size);
}

static void installed_preflight(uint32_t size)
{
  uint32_t token;
  ps_package_validation_blob = candidate;
  ps_package_validation_size = size;
  PS_HW6_RTOS_RunPackageValidation(TX_SUCCESS);
  assert(ps_package_validation_status == 0);
  assert(g_ps_object_candidate_probe.scene_count == 2);
  assert(g_ps_object_candidate_probe.requested_scene == 2);
  assert(!PS_SceneRuntime_StateSceneActive());
  inject_missing_scene = 2;
  PS_HW6_RTOS_RunPackageValidation(TX_SUCCESS);
  assert(ps_package_validation_status == 1);
  assert(g_ps_package_workflow_probe.validation_scene == 2);
  assert(g_ps_package_workflow_probe.validation_reason == PS_EGG_STATE_LOADER_REASON_RENDER);
  assert(!ps_candidate_busy);
  inject_missing_scene = 0;
  withhold_scene = 2;
  PS_HW6_RTOS_RunPackageValidation(TX_SUCCESS);
  assert(ps_package_validation_status == 1 && ps_candidate_busy);
  assert(g_ps_package_workflow_probe.validation_scene == 2);
  assert(g_ps_package_workflow_probe.validation_reason == UINT32_MAX);
  token = g_ps_object_candidate_probe.request_id;
  PS_HW6_RTOS_RunPackageValidation(TX_SUCCESS);
  assert(ps_package_validation_status == 1 && g_ps_object_candidate_probe.request_id == token);
  assert(g_ps_package_workflow_probe.validation_scene == 0);
  withhold_scene = 0;
  PS_HW6_RTOS_CandidateDisplay(queued);
  PS_HW6_RTOS_CandidateReap();
  assert(ps_package_validation_status == 1 && !PS_SceneRuntime_StateSceneActive());
  PS_HW6_RTOS_RunPackageValidation(TX_SUCCESS);
  assert(ps_package_validation_status == 0 && !ps_candidate_busy);
}

static void installed_selection(uint32_t size, uint32_t expected_scene, uint32_t count)
{
  uint32_t token = g_ps_object_candidate_probe.request_id;
  memcpy(baseline, candidate, size);
  baseline_size = size;
  ps_package_validation_blob = candidate;
  ps_package_validation_size = size;
  PS_SceneRuntime_SetObjectSceneAdmission(PS_HW6_RTOS_ObjectSceneCheck);
  PS_HW6_RTOS_RunPackageValidation(TX_SUCCESS);
  if (expected_scene == 0)
  {
    assert(ps_package_validation_status == 1);
    assert(PS_SceneRuntime_EnterStateScene() == PS_SCENE_RUNTIME_INDEX_INVALID);
    assert(!PS_SceneRuntime_StateSceneActive() && !PS_SceneRuntime_InstalledObjectsActive());
    assert(!ps_candidate_busy);
    return;
  }
  assert(ps_package_validation_status == 0);
  assert(g_ps_object_candidate_probe.request_id - token == count);
  token = g_ps_object_candidate_probe.request_id;
  assert(PS_SceneRuntime_EnterStateScene() != PS_SCENE_RUNTIME_INDEX_INVALID);
  assert(g_ps_object_candidate_probe.request_id - token == count);
  assert(PS_SceneRuntime_InstalledObjectsActive() && PS_EggStateLoader_SceneCount() == count);
  assert(g_ps_scene_runtime_probe.scene_id == expected_scene);
  assert(g_ps_scene_runtime_probe.package_source == PS_PACKAGE_SOURCE_INSTALLED_RAM);
  assert(s_ps_object_graph.objects.elapsed_ms == 0);
  PS_SceneRuntime_ExitStateScene();
}

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

static void installed_sfx(uint32_t size)
{
  uint32_t cue, timer, epoch, next, full_loads;
  ps_egg_state_loader_audio_cue_t short_cue, long_cue, after;
  memcpy(baseline, candidate, size);
  baseline_size = size;
  PS_SceneRuntime_SetObjectSceneAdmission(PS_HW6_RTOS_ObjectSceneCheck);
  installed_preflight(size);
  assert(PS_SceneRuntime_EnterStateScene() != PS_SCENE_RUNTIME_INDEX_INVALID);
  assert(PS_SceneRuntime_InstalledObjectsActive());
  assert(PS_SceneRuntime_TakeSfxRequest(&cue) == 0);
  assert(PS_SceneRuntime_AdvanceDevelopmentObjects(650) == 0);
  assert(PS_SceneRuntime_ProjectDevelopmentObjects(&model, &next) == 0);
  save_source();
  inject_missing_scene = 1;
  assert(PS_SceneRuntime_HandleStateSceneInput(1, 4) == PS_SCENE_RUNTIME_INPUT_ERROR);
  assert(PS_SceneRuntime_TakeSfxRequest(&cue) == 0);
  /* A rejected local candidate uses the snapshot scratch, not the live bank. */
  assert(PS_SceneRuntime_ProjectDevelopmentObjects(&model, &next) == 0);
  source_unchanged();
  inject_missing_scene = 0;
  assert(PS_SceneRuntime_HandleStateSceneInput(1, 4) == PS_SCENE_RUNTIME_INPUT_APPLIED);
  assert(PS_SceneRuntime_TakeSfxRequest(&cue) == 1);
  assert(PS_EggStateLoader_GetAudioCue(cue, &short_cue) == 1);
  assert(short_cue.sample_count == 1280 && short_cue.duration_ms == 80);
  assert(short_cue.priority == 1 && short_cue.volume == 96);
  assert(short_cue.package_backed == 0 && short_cue.adpcm >= baseline &&
         short_cue.adpcm + short_cue.adpcm_size <= baseline + size);
  assert(PS_SceneRuntime_TakeSfxRequest(&cue) == 0);
  assert(s_ps_object_graph.objects.elapsed_ms == 650);
  assert(PS_SceneRuntime_ProjectDevelopmentObjects(&model, &next) == 0);
  assert(s_ps_object_snapshot.objects[0].step == 1 &&
         s_ps_object_snapshot.objects[0].remaining_ms == 150);
  assert(s_ps_object_snapshot.objects[1].effective.x == 120);
  assert(PS_SceneRuntime_HandleStateSceneInput(1, 4) == PS_SCENE_RUNTIME_INPUT_IGNORED);
  assert(PS_SceneRuntime_TakeSfxRequest(&cue) == 0);
  for (timer = 0; timer < s_ps_scene_runtime_state_scene->event_binding_count; ++timer)
  {
    if (s_ps_scene_runtime_state_scene->event_bindings[timer].event_class ==
        PS_SCENE_RUNTIME_EVENT_CLASS_TIMER) { break; }
  }
  assert(timer < s_ps_scene_runtime_state_scene->event_binding_count);
  epoch = PS_SceneRuntime_StateActivation();
  assert(PS_SceneRuntime_HandleStateSceneEvent(timer) == PS_SCENE_RUNTIME_INPUT_APPLIED);
  assert(PS_SceneRuntime_StateActivation() == epoch);
  assert(PS_SceneRuntime_TakeSfxRequest(&cue) == 1);
  assert(PS_EggStateLoader_GetAudioCue(cue, &long_cue) == 1);
  assert(long_cue.sample_count == 96000 && long_cue.duration_ms == 6000);
  assert(long_cue.priority == 1 && long_cue.volume == 96 && long_cue.package_backed == 0);
  assert(PS_SceneRuntime_TakeSfxRequest(&cue) == 0);
  assert(PS_SceneRuntime_ProjectDevelopmentObjects(&model, &next) == 0);
  assert(s_ps_object_snapshot.objects[2].effective.flags & 1);
  assert(s_ps_object_graph.objects.elapsed_ms == 650);
  save_source();
  PS_HW6_RTOS_RunPackageValidation(TX_SUCCESS);
  assert(ps_package_validation_status == 0);
  source_unchanged();
  full_loads = g_ps_egg_validation_probe.load_count;
  assert(PS_SceneRuntime_HandleStateSceneInput(1, 1) == PS_SCENE_RUNTIME_INPUT_APPLIED);
  assert(g_ps_scene_runtime_probe.scene_id == 2);
  assert(PS_SceneRuntime_TakeSfxRequest(&cue) == 0);
  assert(PS_EggStateLoader_GetAudioCue(long_cue.cue_index, &after) == 1);
  assert(after.adpcm == long_cue.adpcm && after.adpcm_size == long_cue.adpcm_size);
  assert(PS_SceneRuntime_HandleStateSceneInput(1, 4) == PS_SCENE_RUNTIME_INPUT_APPLIED);
  assert(PS_SceneRuntime_TakeSfxRequest(&cue) == 1);
  assert(PS_EggStateLoader_GetAudioCue(cue, &after) == 1);
  assert(after.adpcm == short_cue.adpcm && after.volume == short_cue.volume);
  assert(PS_SceneRuntime_TakeSfxRequest(&cue) == 0);
  assert(PS_SceneRuntime_HandleStateSceneInput(1, 2) == PS_SCENE_RUNTIME_INPUT_APPLIED);
  assert(g_ps_scene_runtime_probe.scene_id == 1 && g_ps_scene_runtime_probe.state_id == 1);
  assert(PS_SceneRuntime_ProjectDevelopmentObjects(&model, &next) == 0);
  assert(s_ps_object_snapshot.objects[0].step == 0 &&
         s_ps_object_snapshot.objects[1].effective.x == 32 &&
         !(s_ps_object_snapshot.objects[2].effective.flags & 1));
  assert(PS_SceneRuntime_TakeSfxRequest(&cue) == 0);
  assert(g_ps_egg_validation_probe.load_count == full_loads);
  PS_SceneRuntime_ExitStateScene();
  puts("installed resident SFX: owner admission, atomic effects and catalog lifetime passed");
}

static void trusted_metadata(uint32_t size)
{
  static ps_scene_runtime_state_scene_t decoded;
  ps_egg_sprite_catalog_t catalog;
  ps_egg_v2_profile_result_t profile;
  uint32_t full_loads, misses, index, next, decodes;
  memcpy(baseline, candidate, size);
  baseline_size = size;
  PS_SceneRuntime_SetObjectSceneAdmission(PS_HW6_RTOS_ObjectSceneCheck);
  assert(PS_EggStateLoader_DecodeActiveV2Scene(candidate, size, 1,
    &decoded, &catalog, &profile) == PS_EGG_STATE_LOADER_NOT_ACTIVE);
  assert(PS_SceneRuntime_EnterStateScene() != PS_SCENE_RUNTIME_INDEX_INVALID);
  assert(PS_SceneRuntime_ProjectDevelopmentObjects(&model, &next) == 0);
  save_source();
  full_loads = g_ps_egg_validation_probe.load_count;
  for (index = 1; index <= 2; ++index)
  {
    assert(PS_EggStateLoader_DecodeActiveV2Scene(candidate, size, index,
      &decoded, &catalog, &profile) == 0);
    assert(profile.scene_id == index && profile.scene_count == 2);
    assert(decoded.object_definition.objects.data >= candidate &&
      decoded.object_definition.objects.data + decoded.object_definition.objects.size <= candidate + size);
    assert(decoded.object_definition.controls.data >= candidate &&
      decoded.object_definition.controls.data + decoded.object_definition.controls.size <= candidate + size);
    assert(decoded.object_definition.animations.data >= candidate &&
      decoded.object_definition.animations.data + decoded.object_definition.animations.size <= candidate + size);
    assert(catalog.records >= candidate && catalog.records < candidate + size);
    assert(catalog.sprite_payload >= candidate &&
      catalog.sprite_payload + catalog.sprite_size <= candidate + size);
    source_unchanged();
  }
  assert(g_ps_egg_validation_probe.load_count == full_loads);
  assert(PS_EggStateLoader_DecodeActiveV2Scene(candidate, size, 3,
    &decoded, &catalog, &profile) == 1);
  assert(decoded.scene_id == 0 && catalog.records == NULL && profile.scene_count == 0);
  source_unchanged();
  assert(PS_EggStateLoader_DecodeActiveV2Scene(candidate, size - 1, 1,
    &decoded, &catalog, &profile) == PS_EGG_STATE_LOADER_NOT_ACTIVE);
  /* Same address/size is insufficient for a candidate copy. Corruption falls
   * back to complete validation and rejects without changing the active scene. */
  candidate[0] ^= 1;
  assert(PS_EggStateLoader_DecodeActiveV2Scene(candidate, size, 1,
    &decoded, &catalog, &profile) == PS_EGG_STATE_LOADER_NOT_ACTIVE);
  assert(PS_HW6_RTOS_ObjectSceneCheck(candidate, size, 1, &s_ps_object_graph.objects, NULL) == 1);
  assert(g_ps_egg_validation_probe.load_count > full_loads);
  source_unchanged();
  candidate[0] ^= 1;
  full_loads = g_ps_egg_validation_probe.load_count;
  misses = ps_candidate_cache_misses;
  decodes = g_ps_egg_validation_probe.scene_decode_count;
  for (index = 0; index < 4; ++index)
  {
    assert(PS_SceneRuntime_HandleStateSceneInput(1, 1) == PS_SCENE_RUNTIME_INPUT_APPLIED);
    assert(g_ps_scene_runtime_probe.scene_id == 2);
    assert(PS_SceneRuntime_HandleStateSceneInput(1, 2) == PS_SCENE_RUNTIME_INPUT_APPLIED);
    assert(g_ps_scene_runtime_probe.scene_id == 1);
  }
  assert(ps_candidate_cache_misses >= misses + 8);
  assert(g_ps_egg_validation_probe.load_count == full_loads);
  assert(g_ps_egg_validation_probe.scene_decode_count == decodes + 8);

  /* Runtime admission consumes the prepared graph, not a second decoded scene.
   * Cache hits must also avoid initializing the unused candidate graph. */
  decoded = *s_ps_object_graph.scene;
  decodes = g_ps_egg_validation_probe.scene_decode_count;
  memset(&ps_candidate_scene, 0, sizeof(ps_candidate_scene));
  assert(PS_HW6_RTOS_ObjectSceneCheck(baseline, size, decoded.scene_id,
    &s_ps_object_graph.objects, &decoded) == 0);
  assert(ps_candidate_cache_runtime == 1);
  assert(g_ps_egg_validation_probe.scene_decode_count == decodes);
  assert(ps_candidate_catalog.records >= ps_candidate_owned_bytes &&
    ps_candidate_catalog.records < ps_candidate_owned_bytes + size);
  assert(PS_EggStateLoader_PrepareActiveV2Display(candidate, size,
    &decoded, &catalog, &profile) == 0);
  assert(catalog.records >= candidate && catalog.records < candidate + size);
  candidate[0] ^= 1;
  assert(PS_EggStateLoader_PrepareActiveV2Display(candidate, size,
    &decoded, &catalog, &profile) == PS_EGG_STATE_LOADER_NOT_ACTIVE);
  assert(catalog.records == NULL && profile.scene_count == 0);
  candidate[0] ^= 1;
  assert(PS_HW6_RTOS_ObjectSceneCheck(candidate, size, decoded.scene_id,
    &s_ps_object_graph.objects, &decoded) == 1);
  decoded.scene_id = 2;
  assert(PS_EggStateLoader_PrepareActiveV2Display(candidate, size,
    &decoded, &catalog, &profile) == 1);
  assert(catalog.records == NULL);
  decoded = *s_ps_object_graph.scene;
  /* Public preflight cannot hit a catalog-only runtime cache. */
  assert(PS_HW6_ObjectCandidate_CheckScene(candidate, size, decoded.scene_id) == 0);
  assert(ps_candidate_cache_runtime == 0);
  assert(g_ps_egg_validation_probe.load_count > full_loads);
  full_loads = g_ps_egg_validation_probe.load_count;
  assert(PS_HW6_RTOS_ObjectSceneCheck(baseline, size, decoded.scene_id,
    &s_ps_object_graph.objects, &decoded) == 0);
  assert(ps_candidate_cache_runtime == 1);
  decodes = g_ps_egg_validation_probe.scene_decode_count;
  assert(PS_HW6_RTOS_ObjectSceneCheck(baseline, size, decoded.scene_id,
    &s_ps_object_graph.objects, &decoded) == 0);
  assert(g_ps_egg_validation_probe.scene_decode_count == decodes);

  /* A timed-out display still owns its private bytes when the source exits and
   * its buffer is reused. Late completion must not depend on that source. */
  withhold_scene = 2;
  assert(PS_SceneRuntime_HandleStateSceneInput(1, 1) == PS_SCENE_RUNTIME_INPUT_ERROR);
  assert(ps_candidate_busy && ps_candidate_catalog.sprite_payload >= ps_candidate_owned_bytes &&
    ps_candidate_catalog.sprite_payload + ps_candidate_catalog.sprite_size <= ps_candidate_owned_bytes + size);
  PS_SceneRuntime_ExitStateScene();
  assert(PS_EggStateLoader_DecodeActiveV2Scene(candidate, size, 1,
    &decoded, &catalog, &profile) == PS_EGG_STATE_LOADER_NOT_ACTIVE);
  memset(baseline, 0, size);
  withhold_scene = 0;
  PS_HW6_RTOS_CandidateDisplay(queued);
  PS_HW6_RTOS_CandidateReap();
  assert(!ps_candidate_busy && g_ps_object_candidate_probe.status == 0);
  assert(!PS_SceneRuntime_StateSceneActive());
  assert(PS_SceneRuntime_EnterStateScene() == PS_SCENE_RUNTIME_INDEX_INVALID);
  assert(PS_EggStateLoader_DecodeActiveV2Scene(candidate, size, 1,
    &decoded, &catalog, &profile) == PS_EGG_STATE_LOADER_NOT_ACTIVE);
  memcpy(baseline, candidate, size);
  assert(PS_SceneRuntime_EnterStateScene() != PS_SCENE_RUNTIME_INDEX_INVALID);
  assert(g_ps_egg_validation_probe.load_count > full_loads);
  assert(PS_EggStateLoader_DecodeActiveV2Scene(candidate, size, 1,
    &decoded, &catalog, &profile) == 0);
  decoded = *s_ps_object_graph.scene;
  assert(PS_HW6_RTOS_ObjectSceneCheck(baseline, size, decoded.scene_id,
    &s_ps_object_graph.objects, &decoded) == 0);
  assert(ps_candidate_cache_valid && ps_candidate_cache_runtime);
  PS_EggStateLoader_ReleaseActiveV2();
  assert(PS_HW6_RTOS_ObjectSceneCheck(baseline, size, decoded.scene_id,
    &s_ps_object_graph.objects, &decoded) == 1);
  PS_SceneRuntime_ExitStateScene();
  assert(PS_SceneRuntime_EnterStateScene() != PS_SCENE_RUNTIME_INDEX_INVALID);
  /* A failed publication attempt revokes trust even at the same address. */
  baseline[0] ^= 1;
  assert(PS_EggStateLoader_LoadDevelopment(baseline, size, &decoded) == 1);
  baseline[0] ^= 1;
  assert(PS_EggStateLoader_DecodeActiveV2Scene(candidate, size, 1,
    &decoded, &catalog, &profile) == PS_EGG_STATE_LOADER_NOT_ACTIVE);
  PS_SceneRuntime_ExitStateScene();
  assert(PS_EggStateLoader_IsActiveV2Source(baseline, size) == 0);
  assert(PS_EggStateLoader_PrepareActiveV2Display(candidate, size,
    &decoded, &catalog, &profile) == PS_EGG_STATE_LOADER_NOT_ACTIVE);
  assert(catalog.records == NULL);
  puts("trusted metadata: one decode per replacement, private spans, rejection and lifetime invalidation passed");
}

int main(int argc, char **argv)
{
  uint32_t size, result, next, trial, token;
  assert(argc == 2 || argc == 3 || argc == 5);
  size = read_blob(argv[1], candidate);
  set_hash(argv[1], candidate, size);
  if (argc == 5)
  {
    installed_selection(size, (uint32_t)strtoul(argv[3], NULL, 10),
      (uint32_t)strtoul(argv[4], NULL, 10));
    return 0;
  }
  if (argc == 3 && strcmp(argv[2], "hardware") == 0)
  {
    assert(size == 3440);
    hardware_fixture();
    return 0;
  }
  if (argc == 3 && strcmp(argv[2], "audio") == 0)
  {
    installed_sfx(size);
    return 0;
  }
  if (argc == 3 && strcmp(argv[2], "trusted") == 0)
  {
    trusted_metadata(size);
    return 0;
  }
  installed_test = argc == 3;
  if (installed_test)
  {
    memcpy(baseline, candidate, size);
    baseline_size = size;
    installed_preflight(size);
  }
  assert(PS_SceneRuntime_EnterDevelopmentObjects(candidate, size) == 1);
  assert(enter(size) == 1);
  PS_SceneRuntime_SetObjectSceneAdmission(PS_HW6_RTOS_ObjectSceneCheck);
  inject_missing_scene = 2;
  assert(enter(size) == 1);
  assert(!PS_SceneRuntime_StateSceneActive());
  inject_missing_scene = 0;
  if (installed_test)
  {
    withhold_scene = 2;
    assert(enter(size) == 1 && ps_candidate_busy);
    assert(!PS_SceneRuntime_StateSceneActive() && !PS_SceneRuntime_InstalledObjectsActive());
    withhold_scene = 0;
    PS_HW6_RTOS_CandidateDisplay(queued);
    PS_HW6_RTOS_CandidateReap();
    assert(!PS_SceneRuntime_StateSceneActive());
  }
  assert(enter(size) == 0);
  assert(PS_SceneRuntime_InstalledObjectsActive() == installed_test);
  if (installed_test)
  {
    assert(g_ps_scene_runtime_probe.package_source == PS_PACKAGE_SOURCE_INSTALLED_RAM);
    assert(s_ps_installed_object_blob == baseline && s_ps_object_scene_blob == baseline);
  }
  assert(PS_SceneRuntime_AdvanceDevelopmentObjects(750) == 0);
  assert(PS_SceneRuntime_ProjectDevelopmentObjects(&model, &next) == 0);
  save_source();
  if (installed_test)
  {
    PS_HW6_RTOS_RunPackageValidation(TX_SUCCESS);
    assert(ps_package_validation_status == 0);
    source_unchanged();
    inject_missing_scene = 2;
    PS_HW6_RTOS_RunPackageValidation(TX_SUCCESS);
    assert(ps_package_validation_status == 1 && g_ps_package_workflow_probe.validation_scene == 2);
    source_unchanged();
    inject_missing_scene = 0;
  }
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
  assert(s_ps_installed_object_blob == NULL && !PS_SceneRuntime_InstalledObjectsActive());
  if (installed_test)
  {
    assert(enter(size) == 0); /* Ordinary installed reload, not development entry. */
    assert(g_ps_scene_runtime_probe.scene_id == 1 && g_ps_scene_runtime_probe.state_id == 1);
    assert(s_ps_object_graph.variables[0] == 10 && s_ps_object_graph.objects.elapsed_ms == 0);
  }
  puts("fresh replacement, exact owner admission, rejection and late completion passed");
  return 0;
}
