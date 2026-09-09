#include "ps_scene_objects.h"
#include "ps_scene_runtime.h"

#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint8_t buffers[5][262145];
static ps_egg_object_view_t view;
static ps_scene_objects_t bank;
static ps_scene_objects_stage_t stage;
static ps_scene_objects_snapshot_t snapshot;
static uint32_t activation;

static uint32_t ReadU32(FILE *file)
{
  uint8_t p[4];
  assert(fread(p, 1U, 4U, file) == 4U);
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
    ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void Fresh(void)
{
  assert(PS_SceneObjects_Init(&bank, &view, ++activation, 0U) == PS_SCENE_OBJECTS_OK);
}

static void Snap(uint32_t step, uint32_t remaining)
{
  uint16_t index;
  assert(PS_SceneObjects_Snapshot(&bank, &snapshot) == PS_SCENE_OBJECTS_OK);
  assert(snapshot.count == 2U);
  for (index = 0U; index < 2U; ++index)
  {
    assert(snapshot.objects[index].step == step);
    assert(snapshot.objects[index].remaining_ms == remaining);
  }
}

static void State(uint16_t state)
{
  assert(PS_SceneObjects_Begin(&bank, &stage) == PS_SCENE_OBJECTS_OK);
  assert(PS_SceneObjects_SelectState(&stage, state) == PS_SCENE_OBJECTS_OK);
  assert(PS_SceneObjects_Commit(&bank, &stage) == PS_SCENE_OBJECTS_OK);
}

static void Action(uint16_t action)
{
  assert(PS_SceneObjects_Begin(&bank, &stage) == PS_SCENE_OBJECTS_OK);
  assert(PS_SceneObjects_Apply(&stage, action) == PS_SCENE_OBJECTS_OK);
  assert(PS_SceneObjects_Commit(&bank, &stage) == PS_SCENE_OBJECTS_OK);
}

static void Ownership(void)
{
  uint16_t initial_frame;
  Fresh();
  Snap(0U, 250U);
  initial_frame = snapshot.objects[0].effective.frame;
  assert(snapshot.objects[0].animation_visible);
  assert(snapshot.objects[1].animation_visible); /* Base-hidden, visible in state 0. */
  assert(PS_SceneObjects_Advance(&bank, 375U) == PS_SCENE_OBJECTS_OK);
  State(1U);
  Snap(1U, 125U);
  assert(snapshot.objects[0].effective.x == 100);
  assert(!snapshot.objects[1].animation_visible);
  assert(PS_SceneObjects_Begin(&bank, &stage) == PS_SCENE_OBJECTS_OK);
  assert(PS_SceneObjects_Apply(&stage, 0U) == PS_SCENE_OBJECTS_OK);
  assert(PS_SceneObjects_Apply(&stage, 1U) == PS_SCENE_OBJECTS_OK);
  assert(bank.objects[0].x == 10);
  assert(PS_SceneObjects_Commit(&bank, &stage) == PS_SCENE_OBJECTS_OK);
  assert(bank.objects[0].x == 22);
  Snap(1U, 125U);
  assert(snapshot.objects[0].effective.x == 100);
  Action(2U); /* Y-only absolute edit beneath an X override. */
  Snap(1U, 125U);
  assert(snapshot.objects[0].effective.x == 100 && snapshot.objects[0].effective.y == 40);
  assert(PS_SceneObjects_Advance(&bank, 50U) == PS_SCENE_OBJECTS_OK);
  State(0U);
  Snap(1U, 75U);
  assert(snapshot.objects[0].effective.x == 22 && snapshot.objects[0].effective.y == 40);
  assert(PS_SceneObjects_Advance(&bank, 75U) == PS_SCENE_OBJECTS_OK);
  Snap(2U, 250U);
  Fresh();
  Snap(0U, 250U);
  assert(bank.objects[0].x == 10 && bank.objects[0].y == 20);
  assert(snapshot.objects[0].effective.frame == initial_frame);
}

static void MasksAndSuspension(void)
{
  uint16_t frame_zero;
  uint16_t frame_three;
  Fresh();
  Snap(0U, 250U);
  frame_zero = snapshot.objects[0].effective.frame;
  assert(PS_SceneObjects_Advance(&bank, 375U) == PS_SCENE_OBJECTS_OK);
  Action(3U); /* Hide only P. */
  assert(PS_SceneObjects_Advance(&bank, 500U) == PS_SCENE_OBJECTS_OK);
  Snap(3U, 125U);
  frame_three = snapshot.objects[0].effective.frame;
  assert(!snapshot.objects[0].animation_visible && snapshot.objects[1].animation_visible);
  Action(4U);
  Snap(3U, 125U);
  assert(snapshot.objects[0].animation_visible);
  Action(5U); /* Persistent frame zero masks P, not Q. */
  Snap(3U, 125U);
  assert(snapshot.objects[0].effective.frame == frame_zero);
  assert(snapshot.objects[1].effective.frame == frame_three);
  assert(!snapshot.objects[0].animation_visible);
  State(2U); /* State frame has priority over persistent frame. */
  Snap(3U, 125U);
  assert(snapshot.objects[0].effective.frame != frame_zero);
  assert(snapshot.objects[0].effective.frame != frame_three);
  assert(!(snapshot.objects[0].effective.flags & 1U));
  State(0U);
  Action(6U);
  Snap(3U, 125U);
  assert(snapshot.objects[0].effective.frame == frame_three);
  assert(PS_SceneObjects_Suspend(&bank, 1U) == PS_SCENE_OBJECTS_OK);
  assert(PS_SceneObjects_Advance(&bank, 1000U) == PS_SCENE_OBJECTS_OK);
  assert(PS_SceneObjects_Begin(&bank, &stage) != PS_SCENE_OBJECTS_OK);
  Snap(3U, 125U);
  assert(PS_SceneObjects_Suspend(&bank, 0U) == PS_SCENE_OBJECTS_OK);
  assert(PS_SceneObjects_Advance(&bank, 125U) == PS_SCENE_OBJECTS_OK);
  Snap(0U, 250U);
  assert(bank.elapsed_ms == 1000U);
  /* Reconciled sleep time is ordinary elapsed time, not a suspend operation. */
  assert(PS_SceneObjects_Advance(&bank, UINT64_C(4294967375)) == PS_SCENE_OBJECTS_OK);
  Snap(1U, 125U);
}

static void Transactions(void)
{
  ps_scene_objects_t before;
  ps_scene_objects_stage_t competing;
  uint32_t index;
  Fresh();
  before = bank;
  assert(PS_SceneObjects_Begin(&bank, &stage) == PS_SCENE_OBJECTS_OK);
  assert(PS_SceneObjects_Apply(&stage, 0U) == PS_SCENE_OBJECTS_OK);
  assert(PS_SceneObjects_Apply(&stage, UINT16_MAX) != PS_SCENE_OBJECTS_OK);
  assert(PS_SceneObjects_Commit(&bank, &stage) != PS_SCENE_OBJECTS_OK);
  assert(memcmp(&before, &bank, sizeof(bank)) == 0);
  State(1U);
  assert(PS_SceneObjects_Begin(&bank, &stage) == PS_SCENE_OBJECTS_OK);
  assert(PS_SceneObjects_Apply(&stage, 0U) == PS_SCENE_OBJECTS_OK);
  assert(PS_SceneObjects_SelectState(&stage, 2U) == PS_SCENE_OBJECTS_OK);
  assert(bank.state == 1U && bank.objects[0].x == 10);
  assert(PS_SceneObjects_Snapshot(&stage.candidate, &snapshot) == PS_SCENE_OBJECTS_OK);
  assert(snapshot.state == 2U && snapshot.objects[0].effective.x == 15);
  assert(snapshot.objects[0].effective.y == 60);
  assert(PS_SceneObjects_Commit(&bank, &stage) == PS_SCENE_OBJECTS_OK);
  assert(bank.state == 2U && bank.objects[0].x == 15);
  Fresh();
  before = bank;
  assert(PS_SceneObjects_Begin(&bank, &stage) == PS_SCENE_OBJECTS_OK);
  for (index = 0U; index < PS_SCENE_RUNTIME_ACTION_MAX; ++index)
  {
    assert(PS_SceneObjects_Apply(&stage, 0U) == PS_SCENE_OBJECTS_OK);
  }
  assert(PS_SceneObjects_Apply(&stage, 0U) != PS_SCENE_OBJECTS_OK);
  assert(PS_SceneObjects_Commit(&bank, &stage) != PS_SCENE_OBJECTS_OK);
  assert(memcmp(&before, &bank, sizeof(bank)) == 0);
  assert(PS_SceneObjects_Begin(&bank, &stage) == PS_SCENE_OBJECTS_OK);
  assert(PS_SceneObjects_Apply(&stage, 0U) == PS_SCENE_OBJECTS_OK);
  PS_SceneObjects_Abort(&stage); /* A later variable/display validation failed. */
  assert(PS_SceneObjects_Commit(&bank, &stage) != PS_SCENE_OBJECTS_OK);
  assert(memcmp(&before, &bank, sizeof(bank)) == 0);
  assert(PS_SceneObjects_Begin(&bank, &stage) == PS_SCENE_OBJECTS_OK);
  assert(PS_SceneObjects_SelectState(&stage, UINT16_MAX) != PS_SCENE_OBJECTS_OK);
  assert(PS_SceneObjects_Commit(&bank, &stage) != PS_SCENE_OBJECTS_OK);
  assert(memcmp(&before, &bank, sizeof(bank)) == 0);
  assert(PS_SceneObjects_Begin(&bank, &stage) == PS_SCENE_OBJECTS_OK);
  assert(PS_SceneObjects_Begin(&bank, &competing) == PS_SCENE_OBJECTS_OK);
  assert(PS_SceneObjects_Commit(&bank, &stage) == PS_SCENE_OBJECTS_OK);
  assert(PS_SceneObjects_Commit(&bank, &competing) != PS_SCENE_OBJECTS_OK);
  assert(PS_SceneObjects_Commit(&bank, &stage) != PS_SCENE_OBJECTS_OK);
  assert(PS_SceneObjects_Begin(&bank, &stage) == PS_SCENE_OBJECTS_OK);
  assert(PS_SceneObjects_Advance(&bank, 1U) == PS_SCENE_OBJECTS_OK);
  assert(PS_SceneObjects_Commit(&bank, &stage) != PS_SCENE_OBJECTS_OK);
  assert(PS_SceneObjects_Begin(&bank, &stage) == PS_SCENE_OBJECTS_OK);
  Fresh();
  assert(PS_SceneObjects_Commit(&bank, &stage) != PS_SCENE_OBJECTS_OK);
  Action(7U);
  assert(bank.objects[0].x == 0 && bank.objects[0].y == 0);
  assert(PS_SceneObjects_Begin(&bank, &stage) == PS_SCENE_OBJECTS_OK);
  assert(PS_SceneObjects_Apply(&stage, 8U) == PS_SCENE_OBJECTS_OK);
  assert(PS_SceneObjects_Apply(&stage, 9U) == PS_SCENE_OBJECTS_OK);
  assert(PS_SceneObjects_Commit(&bank, &stage) == PS_SCENE_OBJECTS_OK);
  assert(bank.objects[0].x == 150); /* Clamp 160 first, then subtract 10. */
  Action(10U);
  Action(11U);
  assert(bank.objects[0].y == 128); /* Negating INT32_MIN is safe. */
  Action(12U);
  Action(13U);
  assert(bank.objects[0].x == 30 && bank.objects[0].y == 35);
  before = bank;
  assert(PS_SceneObjects_Advance(&bank, UINT64_MAX) == PS_SCENE_OBJECTS_OK);
  before = bank;
  assert(PS_SceneObjects_Advance(&bank, 1U) != PS_SCENE_OBJECTS_OK);
  assert(memcmp(&before, &bank, sizeof(bank)) == 0);
  PS_SceneObjects_Clear(&bank);
  assert(PS_SceneObjects_Snapshot(&bank, &snapshot) != PS_SCENE_OBJECTS_OK);
}

int main(int argc, char **argv)
{
  FILE *file;
  ps_egg_object_bytes_t spans[5];
  uint32_t index;
  assert(argc == 3);
  file = fopen(argv[1], "rb");
  assert(file != NULL);
  for (index = 0U; index < 5U; ++index)
  {
    spans[index].size = ReadU32(file);
    spans[index].data = buffers[index] + 1U;
    assert(spans[index].size < sizeof(buffers[index]));
    assert(fread(buffers[index] + 1U, 1U, spans[index].size, file) == spans[index].size);
  }
  assert(fclose(file) == 0);
  assert(PS_EggObject_Decode(spans[0], spans[1], spans[2], spans[3], spans[4], &view) == PS_EGG_OBJECT_OK);
  if (strtoul(argv[2], NULL, 10) != 0U)
  {
    ps_scene_objects_t before;
    memset(&bank, 0x5A, sizeof(bank));
    before = bank;
    assert(PS_SceneObjects_Init(&bank, &view, 1U, 0U) == PS_SCENE_OBJECTS_CAPACITY);
    assert(memcmp(&before, &bank, sizeof(bank)) == 0);
  }
  else
  {
    Ownership();
    MasksAndSuspension();
    Transactions();
  }
  printf("object runtime checks passed; bank/stage/snapshot bytes=%zu/%zu/%zu\n",
         sizeof(bank), sizeof(stage), sizeof(snapshot));
  return 0;
}
