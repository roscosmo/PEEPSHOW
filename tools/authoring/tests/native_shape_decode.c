#define main validation_main
#include "native_package_validation.c"
#undef main

int main(int argc, char **argv)
{
  static const uint32_t expected_types[] = {
    PS_SCENE_RENDER_ELEMENT_LINE,
    PS_SCENE_RENDER_ELEMENT_LINE_UP_RIGHT,
    PS_SCENE_RENDER_ELEMENT_OUTLINE_RECT,
    PS_SCENE_RENDER_ELEMENT_FILLED_RECT,
    PS_SCENE_RENDER_ELEMENT_CIRCLE,
    PS_SCENE_RENDER_ELEMENT_ELLIPSE,
    PS_SCENE_RENDER_ELEMENT_FILLED_CIRCLE,
    PS_SCENE_RENDER_ELEMENT_FILLED_ELLIPSE
  };
  const ps_scene_render_model_t *model;
  uint32_t index, first;
  assert(argc == 2);
  baseline_size = read_blob(argv[1], baseline);
  set_hash(argv[1], baseline, baseline_size);
  assert(PS_SceneRuntime_EnterStateScene() != PS_SCENE_RUNTIME_INDEX_INVALID);
  model = PS_SceneRuntime_ResolveStateSceneRenderModel();
  assert(model != NULL && model->element_count >= 8U);
  first = model->element_count - 8U;
  for (index = 0U; index < 8U; ++index)
  {
    const ps_scene_render_element_t *element = &model->elements[first + index];
    assert(element->type == expected_types[index]);
    assert(element->visible == 1U && element->x == 28U && element->y == 32U);
    assert(PS_SceneRuntime_RenderElementValid(element) == 1U);
  }
  puts("shape decoding checks passed");
  return 0;
}
