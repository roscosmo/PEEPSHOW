#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define PS_INPUT_BUTTONS_H
#define PS_INPUT_BUTTON_LOGICAL_EVENT_PRESS 1U
#define PS_INPUT_BUTTON_LOGICAL_EVENT_REPEAT 4U
#include "ps_egg_state_loader.c"
#include "ps_scene_runtime.c"

static uint8_t baseline[1048576], candidate[1048576];
static uint32_t baseline_size;
static const uint8_t *hash_input;
static uint32_t hash_size;
static uint8_t hash_digest[32];
volatile ps_package_source_probe_t g_ps_package_source_probe;

uint32_t PS_HW6_HASH_Sha256(const uint8_t *bytes, uint32_t size, uint8_t digest[32])
{
  /* Python's hashlib is the oracle; check the exact hardware-HASH input range. */
  assert(bytes == hash_input && size == hash_size);
  memcpy(digest, hash_digest, 32);
  return 0;
}

uint32_t PS_PackageSource_Resolve(ps_package_source_view_t *view)
{
  memset(view, 0, sizeof(*view));
  view->blob = baseline;
  view->size = baseline_size;
  view->resident_size = baseline_size;
  return 0;
}

static uint32_t read_blob(const char *path, uint8_t *bytes)
{
  FILE *file = fopen(path, "rb");
  size_t count;
  assert(file != NULL);
  count = fread(bytes, 1, sizeof(candidate), file);
  assert(count > 104 && count < sizeof(candidate) && !ferror(file));
  fclose(file);
  return (uint32_t)count;
}

static void set_hash(const char *path, const uint8_t *blob, uint32_t size)
{
  char digest_path[2048];
  FILE *file;
  assert(snprintf(digest_path, sizeof(digest_path), "%s.sha256", path) > 0);
  file = fopen(digest_path, "rb");
  assert(file != NULL && fread(hash_digest, 1, 32, file) == 32);
  fclose(file);
  hash_input = blob;
  hash_size = size - 40;
}

int main(int argc, char **argv)
{
  static ps_egg_context_t saved_context;
  static ps_egg_state_loader_probe_t saved_loader;
  static ps_scene_runtime_probe_t saved_runtime;
  static ps_scene_runtime_state_scene_t saved_scene, decoded;
  int index;
  assert(argc >= 4 && (argc % 2) == 0);
  baseline_size = read_blob(argv[1], baseline);
  set_hash(argv[1], baseline, baseline_size);
  assert(PS_SceneRuntime_EnterStateScene() != PS_SCENE_RUNTIME_INDEX_INVALID);
  saved_context = s_ps_egg_runtime_context;
  saved_loader = g_ps_egg_state_loader_probe;
  saved_runtime = g_ps_scene_runtime_probe;
  saved_scene = *s_ps_scene_runtime_state_scene;
  for (index = 2; index < argc; index += 2)
  {
    uint32_t size = read_blob(argv[index], candidate);
    uint32_t expected_reason = (uint32_t)strtoul(argv[index + 1], NULL, 10);
    uint32_t status;
    set_hash(argv[index], candidate, size);
    status = PS_EggStateLoader_ValidatePackage(candidate, size);
    assert(status == ((expected_reason == 0) ? 0U : 1U));
    assert(g_ps_egg_validation_probe.reason == expected_reason);
    assert(memcmp(&saved_context, &s_ps_egg_runtime_context, sizeof(saved_context)) == 0);
    assert(memcmp(&saved_loader, (const void *)&g_ps_egg_state_loader_probe, sizeof(saved_loader)) == 0);
    assert(memcmp(&saved_runtime, (const void *)&g_ps_scene_runtime_probe, sizeof(saved_runtime)) == 0);
    assert(memcmp(&saved_scene, s_ps_scene_runtime_state_scene, sizeof(saved_scene)) == 0);
    assert(s_ps_egg_validation_context.blob == NULL);
  }
  assert(PS_EggStateLoader_LoadScene(PS_EggStateLoader_EntrySceneId(), &decoded) == 0);
  assert(memcmp(&saved_scene, &decoded, sizeof(decoded)) == 0);
  puts("candidate validation and live-context isolation checks passed");
  return 0;
}
