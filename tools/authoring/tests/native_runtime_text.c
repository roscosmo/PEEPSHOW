#define PS_OBJECT_AWAKE_MAIN awake_main
#include "native_object_awake.c"

int main(int argc, char **argv)
{
  uint32_t size, next;
  FILE *output;
  assert(argc == 3);
  size = read_blob(argv[1], candidate);
  set_hash(argv[1], candidate, size);
  assert(PS_SceneRuntime_EnterDevelopmentObjects(candidate, size) == 0);
  {
    static ps_scene_runtime_state_scene_t decoded;
    ps_egg_sprite_catalog_t catalog;
    ps_egg_v2_profile_result_t profile;
    memcpy(baseline, candidate, size);
    assert(PS_EggStateLoader_DecodeActiveV2Scene(baseline, size, 0,
      &decoded, &catalog, &profile) == 0);
    assert(catalog.strings >= baseline && catalog.strings + catalog.strings_size <= baseline + size);
    assert(PS_EggStateLoader_PrepareActiveV2Display(baseline, size,
      &s_ps_scene_runtime_scene_slots[s_ps_scene_runtime_active_slot], &catalog, &profile) == 0);
    assert(catalog.strings >= baseline && catalog.strings + catalog.strings_size <= baseline + size);
  }
  output = fopen(argv[2], "wb"); assert(output != NULL);
  for (uint32_t i = 0; i < 3; ++i)
  {
    assert(PS_SceneRuntime_ProjectDevelopmentObjects(&model, &next) == 0);
    frame(output);
    assert(PS_SceneRuntime_AdvanceDevelopmentObjects(375) == 0);
    assert(PS_SceneRuntime_HandleStateSceneInput(1, 1) == PS_SCENE_RUNTIME_INPUT_APPLIED);
  }
  fclose(output);
  PS_SceneRuntime_ExitStateScene();
  return 0;
}
