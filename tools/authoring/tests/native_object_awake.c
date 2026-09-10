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

int main(int argc, char **argv)
{
  uint32_t size, next, phase, epoch, status;
  FILE *output;
  assert(argc == 4);
  size = read_blob(argv[1], candidate);
  set_hash(argv[1], candidate, size);
  status = PS_SceneRuntime_EnterDevelopmentObjects(candidate, size);
  if (atoi(argv[3]) != 0)
  {
    assert(status != 0 && PS_SceneRuntime_StateSceneActive() == 0);
    return 0;
  }
  assert(status == 0 && PS_SceneRuntime_DevelopmentObjectsActive() == 1);
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
