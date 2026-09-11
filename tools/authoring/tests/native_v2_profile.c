#define main validation_main
#include "native_package_validation.c"
#undef main

int main(int argc, char **argv)
{
  static ps_egg_context_t saved_context, empty_context;
  static ps_egg_state_loader_probe_t saved_loader;
  static ps_scene_runtime_probe_t saved_runtime;
  static ps_scene_runtime_state_scene_t saved_scene, decoded, empty_scene;
  static ps_scene_object_graph_t saved_graph;
  static ps_scene_objects_snapshot_t saved_snapshot;
  ps_egg_v2_profile_result_t result;
  int index;
  assert(argc >= 7 && ((argc - 3) % 4) == 0);
  baseline_size = read_blob(argv[1], baseline);
  set_hash(argv[1], baseline, baseline_size);
  if (strcmp(argv[2], "v2") == 0)
  {
    assert(PS_SceneRuntime_EnterDevelopmentObjects(baseline, baseline_size) == 0);
    assert(PS_SceneObjects_Advance(&s_ps_object_graph.objects, 375) == PS_SCENE_OBJECTS_OK);
  }
  else
  {
    assert(PS_SceneRuntime_EnterStateScene() != PS_SCENE_RUNTIME_INDEX_INVALID);
  }
  saved_context = s_ps_egg_runtime_context;
  saved_loader = g_ps_egg_state_loader_probe;
  saved_runtime = g_ps_scene_runtime_probe;
  saved_scene = *s_ps_scene_runtime_state_scene;
  saved_graph = s_ps_object_graph;
  saved_snapshot = s_ps_object_snapshot;
  empty_context.probe = &g_ps_egg_validation_probe;

  assert(PS_EggStateLoader_ValidateV2Profile(NULL, 0, NULL) == 1);
  memset(&result, 0xA5, sizeof(result));
  assert(PS_EggStateLoader_ValidateV2Profile(NULL, 10, &result) == 1);
  assert(result.reason == PS_EGG_V2_PROFILE_ARGUMENT && result.loader_reason == 0);
  assert(result.scene_id == 0 && result.item_index == PS_SCENE_RUNTIME_INDEX_INVALID);
  assert(PS_EggStateLoader_ValidateV2Profile(candidate, 0, &result) == 1);
  assert(result.reason == PS_EGG_V2_PROFILE_ARGUMENT);
  assert(PS_EggStateLoader_ValidateV2Profile(candidate, 65537, &result) == 1);
  assert(result.reason == PS_EGG_V2_PROFILE_CAPACITY && result.loader_reason == 0);
  assert(PS_EggStateLoader_ValidateV2Profile(candidate, 1, &result) == 1);
  assert(result.reason == PS_EGG_V2_PROFILE_PACKAGE);

  for (index = 3; index < argc; index += 4)
  {
    uint32_t size = read_blob(argv[index], candidate);
    uint32_t expected = (uint32_t)strtoul(argv[index + 1], NULL, 10);
    uint32_t loader = (uint32_t)strtoul(argv[index + 2], NULL, 10);
    uint32_t item = (uint32_t)strtoul(argv[index + 3], NULL, 10);
    set_hash(argv[index], candidate, size);
    memset(&result, 0xA5, sizeof(result));
    uint32_t status = PS_EggStateLoader_ValidateV2Profile(candidate, size, &result);
    if (status != (expected != 0) || result.reason != expected ||
        result.loader_reason != loader || result.item_index != item)
    {
      fprintf(stderr, "%s: status/reason/loader/item=%lu/%lu/%lu/%lu\n", argv[index],
        (unsigned long)status, (unsigned long)result.reason,
        (unsigned long)result.loader_reason, (unsigned long)result.item_index);
      return 1;
    }
    if (status == 0)
    {
      assert(result.scene_id == 1);
      /* Profile success must not leak into the production install decision. */
      assert(PS_EggStateLoader_ValidatePackage(candidate, size) == 1);
      assert(g_ps_egg_validation_probe.reason == PS_EGG_STATE_LOADER_REASON_CONTAINER);
    }
    assert(memcmp(&empty_context, &s_ps_egg_validation_context, sizeof(empty_context)) == 0);
    assert(memcmp(&empty_scene, &s_ps_egg_validation_scene, sizeof(empty_scene)) == 0);
    assert(memcmp(&saved_context, &s_ps_egg_runtime_context, sizeof(saved_context)) == 0);
    assert(memcmp(&saved_loader, (const void *)&g_ps_egg_state_loader_probe, sizeof(saved_loader)) == 0);
    assert(memcmp(&saved_runtime, (const void *)&g_ps_scene_runtime_probe, sizeof(saved_runtime)) == 0);
    assert(memcmp(&saved_scene, s_ps_scene_runtime_state_scene, sizeof(saved_scene)) == 0);
    assert(memcmp(&saved_graph, &s_ps_object_graph, sizeof(saved_graph)) == 0);
    assert(memcmp(&saved_snapshot, &s_ps_object_snapshot, sizeof(saved_snapshot)) == 0);
    memset(candidate, 0xA5, size);
    assert(PS_EggStateLoader_LoadScene(PS_EggStateLoader_EntrySceneId(), &decoded) == 0);
    assert(memcmp(&saved_scene, &decoded, sizeof(decoded)) == 0);
    /* Loading an active descriptor legitimately advances its own decode probe. */
    saved_loader = g_ps_egg_state_loader_probe;
  }
  puts("V2 profile isolation and production rejection passed");
  return 0;
}
