#define main validation_main
#include "native_package_validation.c"
#undef main

int main(int argc, char **argv)
{
  static ps_egg_context_t saved_context, empty_context;
  static ps_egg_state_loader_probe_t saved_loader;
  static ps_scene_runtime_probe_t saved_runtime;
  static ps_scene_runtime_state_scene_t saved_scene, decoded, empty_scene;
  static ps_egg_sprite_catalog_t catalog, empty_catalog;
  static ps_scene_object_graph_t saved_graph, graph;
  static ps_scene_objects_snapshot_t saved_snapshot, snapshot;
  ps_egg_v2_profile_result_t result;
  int index;
  assert(argc >= 10 && ((argc - 3) % 7) == 0);
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

  assert(PS_EggStateLoader_DecodeV2SceneCandidate(NULL, 0, 0, &decoded, &catalog, NULL) == 1);
  assert(PS_EggStateLoader_DecodeV2SceneCandidate(NULL, 10, 0, &decoded, &catalog, &result) == 1);
  assert(result.reason == PS_EGG_V2_PROFILE_ARGUMENT && result.scene_count == 0);
  assert(PS_EggStateLoader_DecodeV2SceneCandidate(candidate, 0, 0, &decoded, &catalog, &result) == 1);
  assert(result.reason == PS_EGG_V2_PROFILE_ARGUMENT);
  assert(PS_EggStateLoader_DecodeV2SceneCandidate(candidate, 65537, 0, &decoded, &catalog, &result) == 1);
  assert(result.reason == PS_EGG_V2_PROFILE_CAPACITY && result.scene_count == 0);
  assert(PS_EggStateLoader_DecodeV2SceneCandidate(candidate, 10, 0, NULL, &catalog, &result) == 1);
  assert(result.reason == PS_EGG_V2_PROFILE_ARGUMENT);
  assert(PS_EggStateLoader_DecodeV2SceneCandidate(candidate, 10, 0, &decoded, NULL, &result) == 1);
  assert(result.reason == PS_EGG_V2_PROFILE_ARGUMENT);

  for (index = 3; index < argc; index += 7)
  {
    uint32_t size = read_blob(argv[index], candidate);
    uint32_t selected = (uint32_t)strtoul(argv[index + 1], NULL, 10);
    uint32_t reason = (uint32_t)strtoul(argv[index + 2], NULL, 10);
    uint32_t loader = (uint32_t)strtoul(argv[index + 3], NULL, 10);
    uint32_t scene_id = (uint32_t)strtoul(argv[index + 4], NULL, 10);
    uint32_t item = (uint32_t)strtoul(argv[index + 5], NULL, 10);
    uint32_t count = (uint32_t)strtoul(argv[index + 6], NULL, 10);
    uint32_t status;
    set_hash(argv[index], candidate, size);
    memset(&decoded, 0xA5, sizeof(decoded));
    memset(&catalog, 0xA5, sizeof(catalog));
    memset(&result, 0xA5, sizeof(result));
    status = PS_EggStateLoader_DecodeV2SceneCandidate(candidate, size, selected, &decoded, &catalog, &result);
    if (status != (reason != 0) || result.reason != reason || result.loader_reason != loader ||
        result.scene_id != scene_id || result.item_index != item || result.scene_count != count)
    {
      fprintf(stderr, "%s: status/reason/loader/scene/item/count=%lu/%lu/%lu/%lu/%lu/%lu\n", argv[index],
        (unsigned long)status, (unsigned long)result.reason, (unsigned long)result.loader_reason,
        (unsigned long)result.scene_id, (unsigned long)result.item_index, (unsigned long)result.scene_count);
      return 1;
    }
    if (status == 0)
    {
      ps_egg_state_loader_sprite_frame_t frame;
      assert(decoded.scene_id == scene_id);
      assert(PS_SceneObjectGraph_Init(&graph, &decoded, result.scene_count, 17) == 0);
      assert(graph.objects.elapsed_ms == 0 && graph.objects.activation == 17);
      assert(PS_SceneObjects_Snapshot(&graph.objects, &snapshot) == 0);
      assert(snapshot.objects[0].step == 0);
      if (count > 1) { assert(snapshot.objects[1].effective.x == (int32_t)(20 + (scene_id - 1) * 8)); }
      assert(PS_EggStateLoader_GetCatalogSpriteFrame(&catalog, 65537, &frame) == 1);
      assert(frame.pixels >= candidate && frame.pixels < candidate + size);
      /* A private decode must not enable the installed single-scene profile. */
      assert(PS_EggStateLoader_ValidateV2Profile(candidate, size, &result) == (count != 1));
      if (count != 1) { assert(result.reason == PS_EGG_V2_PROFILE_SCENE_COUNT); }
      assert(PS_EggStateLoader_DecodeV2Candidate(candidate, size, &decoded, &catalog, &result) == (count != 1));
      assert(PS_EggStateLoader_ValidatePackage(candidate, size) == 1);
      assert(g_ps_egg_validation_probe.reason == PS_EGG_STATE_LOADER_REASON_CONTAINER);
    }
    else
    {
      assert(memcmp(&empty_scene, &decoded, sizeof(decoded)) == 0);
      assert(memcmp(&empty_catalog, &catalog, sizeof(catalog)) == 0);
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
    saved_loader = g_ps_egg_state_loader_probe;
  }
  puts("all-scene validation, selected decoding and live isolation passed");
  return 0;
}
