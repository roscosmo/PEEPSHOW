#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "ps_egg_state_loader.h"
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

#define DISPLAY_WIDTH 144U
#define DISPLAY_HEIGHT 168U
#define DISPLAY_RENDERER_WIDTH 168U
#define DISPLAY_RENDERER_HEIGHT 144U
#define LINE_WIDTH 18U
static uint8_t s_display_framebuffer[LINE_WIDTH * DISPLAY_HEIGHT];
static uint32_t s_rotate_ccw = 1U;

/* These branches are outside the primitive tests; unexpected use must fail. */
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
static uint32_t DisplayRenderer_ApplyPackageSprite(uint32_t id,
  const ps_scene_waiting_visual_bounds_t *bounds, uint8_t *pixels,
  uint32_t size, uint32_t clear, uint32_t *count)
{ (void)id; (void)bounds; (void)pixels; (void)size; (void)clear; (void)count; assert(0); return 0; }
uint32_t PS_EggStateLoader_GetSpriteFrame(uint32_t id, ps_egg_state_loader_sprite_frame_t *frame)
{ (void)id; (void)frame; assert(0); return 0; }

#include "shape_under_test.inc"

int main(void)
{
  unsigned int type, x, y, width, height, visible, expected_valid;
#ifdef _WIN32
  _setmode(_fileno(stdout), _O_BINARY);
#endif
  while (scanf("%u %u %u %u %u %u %u", &type, &x, &y, &width, &height,
               &visible, &expected_valid) == 7)
  {
    ps_scene_render_model_t model = {0};
    ps_scene_render_element_t *element = &model.elements[0];
    uint32_t count = 0U, actual_count = 0U, index, bit;
    model.api_version = PS_SCENE_RENDER_MODEL_API_VERSION;
    model.element_count = 1U;
    element->element_id = 1U;
    element->type = type;
    element->x = (uint16_t)x;
    element->y = (uint16_t)y;
    element->width = (uint16_t)width;
    element->height = (uint16_t)height;
    element->visible = visible;
    assert(PS_SceneRuntime_RenderElementValid(element) == expected_valid);
    assert(DisplayRenderer_ValidateSceneModel(&model) == expected_valid);
    memset(s_display_framebuffer, 0xff, sizeof(s_display_framebuffer));
    if (expected_valid != 0U)
    {
      count = DisplayRenderer_DrawSceneElement(element);
      assert(DisplayRenderer_DrawSceneElement(element) == 0U);
    }
    for (index = 0; index < sizeof(s_display_framebuffer); ++index)
      for (bit = 0; bit < 8; ++bit)
        actual_count += ((s_display_framebuffer[index] & (1U << bit)) == 0U);
    assert(count == actual_count);
    assert(fwrite(s_display_framebuffer, 1, sizeof(s_display_framebuffer), stdout) == sizeof(s_display_framebuffer));
  }
  return 0;
}
