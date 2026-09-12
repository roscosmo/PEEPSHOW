#define main validation_main
#include "native_package_validation.c"
#undef main

#define DISPLAY_WIDTH 144U
#define DISPLAY_HEIGHT 168U
#define DISPLAY_RENDERER_WIDTH 168U
#define DISPLAY_RENDERER_HEIGHT 144U
#define LINE_WIDTH 18U
#define DISPLAY_RENDERER_BUFFER_SIZE (18U * 168U)
static uint8_t s_display_framebuffer[DISPLAY_RENDERER_BUFFER_SIZE];
static const ps_egg_sprite_catalog_t *s_display_candidate_catalog;
static uint32_t s_rotate_ccw = 1U;
static const char *DisplayRenderer_SceneText(uint32_t id)
{ (void)id; assert(0); return NULL; }
static uint16_t DisplayRenderer_TextWidth(const char *text, uint16_t scale)
{ (void)text; (void)scale; assert(0); return 0; }
static uint32_t DisplayRenderer_DrawText(uint16_t x, uint16_t y, const char *text, uint16_t scale)
{ (void)x; (void)y; (void)text; (void)scale; assert(0); return 0; }
static uint32_t DisplayRenderer_SceneSpritePixel(uint32_t id, uint16_t x, uint16_t y)
{ (void)id; (void)x; (void)y; assert(0); return 0; }
static uint32_t DisplayRenderer_RecordLpbamCursorBounds(const ps_scene_render_element_t *element)
{ (void)element; assert(0); return 0; }
#include "object_raster_under_test.inc"

static void sprite_loop_equivalence(void)
{
  uint8_t record[PS_EGG_ASSET_RECORD_SIZE] = {0}, payload[78];
  uint8_t expected[DISPLAY_RENDERER_BUFFER_SIZE], actual[DISPLAY_RENDERER_BUFFER_SIZE];
  ps_egg_sprite_catalog_t catalog = {record, payload, sizeof(payload), 1};
  ps_scene_waiting_visual_bounds_t bounds = {0, 0, 17, 13};
  const uint16_t xs[] = {0, 1, 7, 8, 79, 150, 151};
  const uint16_t ys[] = {0, 1, 7, 8, 63, 130, 131};
  uint32_t ix, iy, opaque, clear, x, y, count, expected_count;
  record[4] = 17; record[6] = 13; record[8] = 3;
  record[20] = 39; record[24] = 39; record[28] = 39;
  for (x = 0; x < sizeof(payload); ++x) { payload[x] = (uint8_t)(x * 37U + 19U); }
  s_display_candidate_catalog = &catalog;
  for (opaque = 0; opaque < 2; ++opaque)
  for (clear = 0; clear < 2; ++clear)
  for (ix = 0; ix < sizeof(xs) / sizeof(xs[0]); ++ix)
  for (iy = 0; iy < sizeof(ys) / sizeof(ys[0]); ++iy)
  {
    record[32] = opaque ? PS_EGG_ASSET_FLAG_OPAQUE : 0;
    bounds.x = xs[ix]; bounds.y = ys[iy];
    memset(expected, clear ? 0xAA : 0x55, sizeof(expected));
    memcpy(actual, expected, sizeof(actual));
    expected_count = 0;
    for (y = 0; y < bounds.height; ++y)
    for (x = 0; x < bounds.width; ++x)
    {
      uint32_t offset = y * 3 + x / 8;
      uint8_t bit = (uint8_t)(128U >> (x % 8));
      uint32_t owned = opaque || (payload[39 + offset] & bit);
      uint32_t black = (payload[offset] & bit) != 0;
      if (owned || clear)
      { DisplayRenderer_SetLogicalPixelInBuffer(expected, bounds.x + x, bounds.y + y, owned && black); }
      if (owned) { expected_count += black; }
    }
    assert(DisplayRenderer_ApplyPackageSprite(65537, &bounds, actual, sizeof(actual), clear, &count));
    assert(count == expected_count && memcmp(actual, expected, sizeof(actual)) == 0);
  }
  bounds.x = 152;
  memcpy(expected, actual, sizeof(expected));
  assert(!DisplayRenderer_ApplyPackageSprite(65537, &bounds, actual, sizeof(actual), 1, &count));
  assert(memcmp(actual, expected, sizeof(actual)) == 0);
  s_display_candidate_catalog = NULL;
}

static ps_scene_render_model_t model, original;
static void frame(FILE *output)
{
  uint32_t index;
  assert(DisplayRenderer_ValidateSceneModel(&model) == 1U);
  memset(s_display_framebuffer, 255, sizeof(s_display_framebuffer));
  for (index = 0; index < model.element_count; ++index)
  { (void)DisplayRenderer_DrawSceneElement(&model.elements[index]); }
  assert(fwrite(s_display_framebuffer, 1, sizeof(s_display_framebuffer), output) == sizeof(s_display_framebuffer));
}

static void gui_fixture(FILE *output)
{
  uint32_t next, epoch = PS_SceneRuntime_SceneActivation();
  assert(PS_SceneRuntime_ProjectDevelopmentObjects(&model, &next) == 0 && next == 500);
  assert(model.element_count == 2 && model.elements[0].asset_id == 65537);
  assert(model.elements[1].x == 32 && model.elements[1].y == 104);
  frame(output);
  assert(PS_SceneRuntime_AdvanceDevelopmentObjects(650) == 0);
  assert(PS_SceneRuntime_ProjectDevelopmentObjects(&model, &next) == 0 && next == 350);
  assert(model.elements[0].asset_id == 65538);
  frame(output);
  assert(PS_SceneRuntime_HandleStateSceneInput(1, 1) == PS_SCENE_RUNTIME_INPUT_APPLIED);
  assert(PS_SceneRuntime_ProjectDevelopmentObjects(&model, &next) == 0 && next == 350);
  assert(model.state_id == 2 && model.elements[1].x == 120);
  assert(model.elements[0].asset_id == 65538 && model.timeline_revision == epoch);
  assert(s_ps_object_graph.objects.objects[1].x == 32);
  frame(output);
  assert(PS_SceneRuntime_AdvanceDevelopmentObjects(300) == 0);
  assert(PS_SceneRuntime_ProjectDevelopmentObjects(&model, &next) == 0 && next == 50);
  assert(model.elements[0].asset_id == 65538);
  frame(output);
  assert(PS_SceneRuntime_HandleStateSceneInput(1, 2) == PS_SCENE_RUNTIME_INPUT_APPLIED);
  assert(PS_SceneRuntime_TakeShellExitRequest() == 0);
  assert(PS_SceneRuntime_ProjectDevelopmentObjects(&model, &next) == 0 && next == 50);
  assert(model.state_id == 1 && model.elements[1].x == 32);
  assert(model.elements[0].asset_id == 65538 && model.timeline_revision == epoch);
  assert(s_ps_object_graph.objects.objects[1].x == 32);
  frame(output);
  assert(PS_SceneRuntime_AdvanceDevelopmentObjects(100) == 0);
  assert(PS_SceneRuntime_ProjectDevelopmentObjects(&model, &next) == 0 && next == 450);
  assert(model.elements[0].asset_id == 65537);
  frame(output);
  assert(PS_SceneRuntime_AdvanceDevelopmentObjects(500) == 0);
  assert(PS_SceneRuntime_ProjectDevelopmentObjects(&model, &next) == 0 && next == 450);
  assert(model.elements[0].asset_id == 65538);
  frame(output);
}

#ifndef PS_OBJECT_AWAKE_MAIN
#define PS_OBJECT_AWAKE_MAIN main
#endif
int PS_OBJECT_AWAKE_MAIN(int argc, char **argv)
{
  uint32_t size, next, phase, epoch, status;
  FILE *output;
  assert(argc == 4);
  sprite_loop_equivalence();
  size = read_blob(argv[1], candidate);
  set_hash(argv[1], candidate, size);
  status = PS_SceneRuntime_EnterDevelopmentObjects(candidate, size);
  if (atoi(argv[3]) == 1)
  {
    assert(status != 0 && PS_SceneRuntime_StateSceneActive() == 0);
    return 0;
  }
  assert(status == 0 && PS_SceneRuntime_DevelopmentObjectsActive() == 1);
  if (atoi(argv[3]) == 2)
  {
    output = fopen(argv[2], "wb"); assert(output != NULL);
    gui_fixture(output);
    fclose(output);
    PS_SceneRuntime_ExitStateScene();
    assert(PS_EggStateLoader_Load(candidate, size, size, &s_ps_scene_runtime_scene_slots[0]) != 0);
    return 0;
  }
  epoch = PS_SceneRuntime_SceneActivation();
  assert(PS_SceneRuntime_ProjectDevelopmentObjects(&model, &next) == 0 && next == 250);
  assert(model.element_count == 2 && model.elements[0].asset_id == 65537);
  original = model;
  output = fopen(argv[2], "wb"); assert(output != NULL);
  frame(output);
  assert(PS_SceneRuntime_AdvanceDevelopmentObjects(375) == 0);
  assert(PS_SceneRuntime_ProjectDevelopmentObjects(&model, &next) == 0 && next == 125);
  assert(model.elements[0].asset_id == 65538);
  frame(output);
  assert(PS_SceneRuntime_HandleStateSceneInput(1, 1) == PS_SCENE_RUNTIME_INPUT_APPLIED);
  assert(PS_SceneRuntime_ProjectDevelopmentObjects(&model, &next) == 0 && next == 125);
  assert(model.state_id == 2 && model.elements[1].x == 128);
  assert(model.elements[0].asset_id == 65538 && model.timeline_revision == epoch);
  frame(output);
  assert(PS_SceneRuntime_HandleStateSceneInput(2, 1) == PS_SCENE_RUNTIME_INPUT_IGNORED);
  assert(PS_SceneRuntime_AdvanceDevelopmentObjects(125) == 0);
  assert(PS_SceneRuntime_ProjectDevelopmentObjects(&model, &next) == 0 && next == 250);
  assert(model.elements[0].asset_id == 65539);
  frame(output);
  assert(PS_SceneRuntime_HandleStateSceneInput(1, 1) == PS_SCENE_RUNTIME_INPUT_APPLIED);
  assert(PS_SceneRuntime_ProjectDevelopmentObjects(&model, &next) == 0);
  assert(model.elements[1].x == 24 && model.elements[0].asset_id == 65539);
  frame(output);
  assert(PS_SceneRuntime_AdvanceDevelopmentObjects(250) == 0);
  assert(PS_SceneRuntime_ProjectDevelopmentObjects(&model, &next) == 0);
  assert(model.elements[0].asset_id == 65540);
  frame(output);
  assert(PS_SceneRuntime_AdvanceDevelopmentObjects(1250) == 0);
  assert(PS_SceneRuntime_ProjectDevelopmentObjects(&model, &next) == 0);
  assert(model.elements[0].asset_id == original.elements[0].asset_id);
  frame(output);
  fclose(output);
  /* Hidden/static masks suppress deadlines, not the underlying clip clock. */
  for (phase = 0; phase < 2; ++phase)
  {
    s_ps_object_graph.objects.objects[0].visible = phase;
    s_ps_object_graph.objects.objects[0].static_frame = phase ? 1 : PS_EGG_OBJECT_REF_NONE;
    assert(PS_SceneRuntime_ProjectDevelopmentObjects(&model, &next) == 0 && next == 0);
    assert(PS_SceneRuntime_AdvanceDevelopmentObjects(375) == 0);
  }
  s_ps_object_graph.objects.objects[0].static_frame = PS_EGG_OBJECT_REF_NONE;
  assert(PS_SceneRuntime_ProjectDevelopmentObjects(&model, &next) == 0 && next == 250);
  assert(model.elements[0].asset_id == 65540);
  assert(PS_SceneRuntime_HandleStateSceneInput(1, 2) == PS_SCENE_RUNTIME_INPUT_APPLIED);
  assert(PS_SceneRuntime_TakeShellExitRequest() == 1);
  PS_SceneRuntime_ExitStateScene();
  assert(PS_SceneRuntime_DevelopmentObjectsActive() == 0);
  assert(PS_SceneRuntime_ProjectDevelopmentObjects(&model, &next) != 0);
  assert(PS_SceneRuntime_EnterDevelopmentObjects(candidate, size) == 0);
  assert(PS_SceneRuntime_SceneActivation() == epoch + 1);
  assert(PS_SceneRuntime_ProjectDevelopmentObjects(&model, &next) == 0 && next == 250);
  assert(model.elements[0].asset_id == 65537);
  PS_SceneRuntime_ExitStateScene();
  /* An explicit test launch does not weaken ordinary admission. */
  assert(PS_EggStateLoader_Load(candidate, size, size, &s_ps_scene_runtime_scene_slots[0]) != 0);
  return 0;
}
