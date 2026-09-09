#define main validation_main
#include "native_package_validation.c"
#undef main
#include "ps_scene_object_graph.c"

static uint32_t input_binding(const ps_scene_runtime_state_scene_t *scene, uint32_t source)
{
  uint32_t index;
  for (index = 0; index < scene->event_binding_count; ++index)
  {
    if (scene->event_bindings[index].event_class == PS_SCENE_RUNTIME_EVENT_CLASS_INPUT &&
        scene->event_bindings[index].source == source) { return index; }
  }
  assert(0); return 0;
}

static void graph_checks(const ps_scene_runtime_state_scene_t *scene)
{
  static ps_scene_object_graph_t bank, saved;
  static ps_scene_object_graph_stage_t stage, competing;
  static ps_scene_objects_snapshot_t snapshot;
  static ps_scene_object_effects_t effects, old_effects;
  uint32_t right = input_binding(scene, 4);
  uint32_t left = input_binding(scene, 3);
  uint32_t timer = scene->event_binding_count - 1;
  assert(scene->event_bindings[timer].event_class == PS_SCENE_RUNTIME_EVENT_CLASS_TIMER);
  assert(PS_SceneObjectGraph_Init(&bank, scene, 2, 1) == 0);
  assert(bank.objects.state == 1 && bank.variables[0] == 1);
  assert(PS_SceneObjects_Advance(&bank.objects, 375) == PS_SCENE_OBJECTS_OK);
  saved = bank;
  memset(&effects, 0xA5, sizeof(effects)); old_effects = effects;
  assert(PS_SceneObjectGraph_StageEvent(&bank, right, &stage) == PS_SCENE_RUNTIME_INPUT_APPLIED);
  assert(memcmp(&saved, &bank, sizeof(bank)) == 0);
  assert(stage.variables[0] == 2 && stage.objects.candidate.objects[0].x == 22);
  assert(PS_SceneObjects_Snapshot(&stage.objects.candidate, &snapshot) == PS_SCENE_OBJECTS_OK);
  assert(snapshot.state == 2 && snapshot.objects[0].step == 1 && snapshot.objects[0].remaining_ms == 125);
  assert(snapshot.objects[0].effective.y == 60 && !(snapshot.objects[0].effective.flags & 1));
  /* Simulated display admission failure: abort publishes nothing. */
  PS_SceneObjectGraph_Abort(&stage);
  assert(PS_SceneObjectGraph_Commit(&bank, &stage, &effects) != 0);
  assert(memcmp(&saved, &bank, sizeof(bank)) == 0 && memcmp(&effects, &old_effects, sizeof(effects)) == 0);
  assert(PS_SceneObjectGraph_StageEvent(&bank, right, &stage) == PS_SCENE_RUNTIME_INPUT_APPLIED);
  assert(PS_SceneObjectGraph_StageEvent(&bank, right, &competing) == PS_SCENE_RUNTIME_INPUT_APPLIED);
  assert(PS_SceneObjectGraph_Commit(&bank, &stage, &effects) == 0);
  assert(bank.objects.state == 2 && bank.variables[0] == 2 && bank.objects.objects[0].x == 22);
  assert(effects.count == 1 && effects.actions[0].kind == PS_SCENE_RUNTIME_ACTION_START_TIMER);
  assert(effects.actions[0].target_id == timer && effects.state_entered == 1);
  saved = bank; old_effects = effects;
  assert(PS_SceneObjectGraph_Commit(&bank, &competing, &effects) != 0);
  assert(PS_SceneObjectGraph_Commit(&bank, &stage, &effects) != 0);
  assert(memcmp(&saved, &bank, sizeof(bank)) == 0 && memcmp(&effects, &old_effects, sizeof(effects)) == 0);
  /* Scene timer changes objects/variables without re-entering a state. */
  assert(PS_SceneObjectGraph_StageEvent(&bank, timer, &stage) == PS_SCENE_RUNTIME_INPUT_APPLIED);
  assert(PS_SceneObjectGraph_Commit(&bank, &stage, &effects) == 0);
  assert(bank.variables[0] == 3 && bank.objects.state == 2 && bank.objects.objects[1].y == 18);
  assert(effects.state_entered == 0 && effects.count == 0 && bank.objects.elapsed_ms == 375);
  /* Existing guard reads the underlying variable and fails with value 3. */
  saved = bank;
  assert(PS_SceneObjectGraph_StageEvent(&bank, left, &stage) == PS_SCENE_RUNTIME_INPUT_IGNORED);
  assert(memcmp(&saved, &bank, sizeof(bank)) == 0);
  assert(PS_SceneObjectGraph_Init(&bank, scene, 2, 2) == 0);
  saved = bank;
  /* Overflow after an object write must discard the whole event. */
  assert(PS_SceneObjectGraph_StageEvent(&bank, left, &stage) == PS_SCENE_RUNTIME_INPUT_ERROR);
  assert(PS_SceneObjectGraph_Commit(&bank, &stage, &effects) != 0);
  assert(memcmp(&saved, &bank, sizeof(bank)) == 0);
  assert(PS_SceneObjectGraph_StageEvent(&bank, right, &stage) == PS_SCENE_RUNTIME_INPUT_APPLIED);
  assert(PS_SceneObjects_Advance(&bank.objects, 1) == PS_SCENE_OBJECTS_OK);
  saved = bank;
  assert(PS_SceneObjectGraph_Commit(&bank, &stage, &effects) != 0);
  assert(memcmp(&saved, &bank, sizeof(bank)) == 0);
  assert(PS_SceneObjects_Suspend(&bank.objects, 1) == PS_SCENE_OBJECTS_OK);
  assert(PS_SceneObjectGraph_StageEvent(&bank, right, &stage) == PS_SCENE_RUNTIME_INPUT_ERROR);
  assert(PS_SceneObjects_Suspend(&bank.objects, 0) == PS_SCENE_OBJECTS_OK);
  assert(PS_SceneObjectGraph_StageEvent(&bank, right, &stage) == PS_SCENE_RUNTIME_INPUT_APPLIED);
  bank.variables[0] = 2;
  saved = bank;
  assert(PS_SceneObjectGraph_Commit(&bank, &stage, &effects) != 0);
  assert(memcmp(&saved, &bank, sizeof(bank)) == 0);
  assert(PS_SceneObjectGraph_Init(&bank, scene, 2, 3) == 0);
  assert(PS_SceneObjectGraph_StageEvent(&bank, right, &stage) == PS_SCENE_RUNTIME_INPUT_APPLIED);
  assert(PS_SceneObjectGraph_Init(&bank, scene, 2, 4) == 0);
  saved = bank;
  assert(PS_SceneObjectGraph_Commit(&bank, &stage, &effects) != 0);
  assert(memcmp(&saved, &bank, sizeof(bank)) == 0);
  /* A replacement request is decoded but cannot commit without orchestration. */
  saved = bank;
  assert(PS_SceneObjectGraph_StageEvent(&bank, input_binding(scene, 1), &stage) == PS_SCENE_RUNTIME_INPUT_APPLIED);
  assert(stage.effects.target_scene_id == 2);
  assert(PS_SceneObjectGraph_Commit(&bank, &stage, &effects) != 0);
  assert(memcmp(&saved, &bank, sizeof(bank)) == 0);
}

int main(int argc, char **argv)
{
  static ps_scene_runtime_state_scene_t decoded, sentinel, legacy;
  static ps_egg_context_t saved_context;
  static ps_egg_state_loader_probe_t saved_probe;
  static ps_scene_runtime_probe_t saved_runtime;
  uint32_t size, reason, status;
  assert(argc == 5);
  baseline_size = read_blob(argv[1], baseline);
  set_hash(argv[1], baseline, baseline_size);
  assert(PS_SceneRuntime_EnterStateScene() != PS_SCENE_RUNTIME_INDEX_INVALID);
  saved_context = s_ps_egg_runtime_context;
  saved_probe = g_ps_egg_state_loader_probe;
  saved_runtime = g_ps_scene_runtime_probe;
  size = read_blob(argv[2], candidate);
  set_hash(argv[2], candidate, size);
  reason = (uint32_t)strtoul(argv[3], NULL, 10);
  memset(&decoded, 0xA5, sizeof(decoded)); sentinel = decoded;
  status = PS_EggStateLoader_DecodeDevelopmentScene(candidate, size, 1, &decoded);
  if (status != (reason != 0)) { fprintf(stderr, "status=%u reason=%u expected=%u\n", status, g_ps_egg_validation_probe.reason, reason); }
  assert(status == (reason != 0) && g_ps_egg_validation_probe.reason == reason);
  if (reason != 0) { assert(memcmp(&decoded, &sentinel, sizeof(decoded)) == 0); }
  else
  {
    assert(decoded.execution_model == PS_SCENE_RUNTIME_MODEL_OBJECTS && decoded.visual_binding_count == 0);
    assert(PS_SceneRuntime_ValidateStateScene(&decoded) != 0);
    /* This explicit activation check increments only its validation counter. */
    g_ps_scene_runtime_probe = saved_runtime;
    assert(PS_EggStateLoader_DecodeDevelopmentScene(candidate, size, 2, &legacy) == 0);
    assert(legacy.execution_model == PS_SCENE_RUNTIME_MODEL_LEGACY);
    assert(PS_EggStateLoader_DecodeDevelopmentScene(candidate, size, 0, &legacy) == 0);
    assert(memcmp(&decoded, &legacy, sizeof(decoded)) == 0);
    sentinel = decoded;
    assert(PS_EggStateLoader_DecodeDevelopmentScene(candidate, size, 99, &decoded) != 0);
    assert(memcmp(&decoded, &sentinel, sizeof(decoded)) == 0);
    if (atoi(argv[4])) { graph_checks(&decoded); }
  }
  assert(s_ps_egg_validation_context.blob == NULL && s_ps_egg_validation_context.development == 0);
  assert(s_ps_egg_validation_scene.object_definition.objects.data == NULL);
  assert(memcmp(&saved_context, &s_ps_egg_runtime_context, sizeof(saved_context)) == 0);
  assert(memcmp(&saved_probe, (const void *)&g_ps_egg_state_loader_probe, sizeof(saved_probe)) == 0);
  assert(memcmp(&saved_runtime, (const void *)&g_ps_scene_runtime_probe, sizeof(saved_runtime)) == 0);
  /* An explicit development decode never enables the normal installation path. */
  assert(PS_EggStateLoader_ValidatePackage(candidate, size) != 0);
  assert(g_ps_egg_validation_probe.reason == PS_EGG_STATE_LOADER_REASON_CONTAINER);
  puts("V2 loader and graph transaction checks passed");
  return 0;
}
