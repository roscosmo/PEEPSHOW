#include "ps_egg_object_decoder.h"

#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

/* Host-only transport buffers; deliberately unaligned payloads exercise the decoder. */
static uint8_t buffers[5][262145];

static uint32_t ReadU32(FILE *file)
{
  uint8_t p[4];
  assert(fread(p, 1U, 4U, file) == 4U);
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
    ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

int main(int argc, char **argv)
{
  FILE *file;
  uint32_t cases;
  uint32_t current;
  ps_egg_object_view_t empty = {0};
  ps_egg_object_bytes_t absent = {0};
  assert(argc == 2);
  file = fopen(argv[1], "rb");
  assert(file != NULL);
  assert(PS_EggObject_Decode(absent, absent, absent, absent, absent, NULL) == PS_EGG_OBJECT_ARGUMENT);
  cases = ReadU32(file);
  for (current = 0U; current < cases; ++current)
  {
    ps_egg_object_bytes_t spans[5];
    ps_egg_object_view_t view;
    ps_egg_object_definition_t object;
    ps_egg_object_override_t override;
    ps_egg_object_operation_t operation;
    uint32_t slot;
    uint16_t index;
    uint16_t state;
    for (slot = 0U; slot < 5U; ++slot)
    {
      spans[slot].size = ReadU32(file);
      assert(spans[slot].size < sizeof(buffers[slot]));
      spans[slot].data = buffers[slot] + 1U;
      assert(fread(buffers[slot] + 1U, 1U, spans[slot].size, file) == spans[slot].size);
    }
    memset(&view, 0xA5, sizeof(view));
    if (PS_EggObject_Decode(spans[0], spans[1], spans[2], spans[3], spans[4], &view) != PS_EGG_OBJECT_OK)
    {
      assert(memcmp(&view, &empty, sizeof(view)) == 0);
      assert(!PS_EggObject_GetDefinition(&view, 0U, &object));
      assert(!PS_EggObject_GetOverride(&view, 0U, 0U, &override));
      assert(!PS_EggObject_GetOperation(&view, 0U, &operation));
      puts("ERR");
      continue;
    }
    printf("OK %u %u %u %u", view.object_count, view.state_count, view.override_count, view.operation_count);
    for (index = 0U; index < view.object_count; ++index)
    {
      assert(PS_EggObject_GetDefinition(&view, index, &object));
      printf(" %u %u %u %u %u %u %u %d %d %u %u", object.id, object.kind,
        object.layer, object.z_order, object.flags, object.width, object.height,
        object.x, object.y, object.frame, object.clip);
    }
    for (state = 0U; state < view.state_count; ++state)
    {
      for (index = 0U; PS_EggObject_GetOverride(&view, state, index, &override); ++index)
      {
        printf(" %u %u %" PRId32 " %" PRId32 " %u %u", override.object,
          override.mask, override.x, override.y, override.frame, override.visible);
      }
    }
    for (index = 0U; index < view.operation_count; ++index)
    {
      assert(PS_EggObject_GetOperation(&view, index, &operation));
      printf(" %u %u %u %" PRId32 " %" PRId32 " %u %u", operation.opcode,
        operation.axis_mask, operation.object, operation.x, operation.y,
        operation.frame, operation.visible);
    }
    assert(!PS_EggObject_GetDefinition(&view, view.object_count, &object));
    assert(!PS_EggObject_GetOverride(&view, view.state_count, 0U, &override));
    assert(!PS_EggObject_GetOperation(&view, view.operation_count, &operation));
    assert(!PS_EggObject_GetDefinition(&view, 0U, NULL));
    assert(!PS_EggObject_GetDefinition(NULL, 0U, &object));
    putchar('\n');
  }
  assert(fgetc(file) == EOF);
  assert(fclose(file) == 0);
  return 0;
}
