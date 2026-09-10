#define PS_OBJECT_AWAKE_MAIN awake_main
#include "native_object_awake.c"

static ps_object_waiting_program_t program;
static ps_object_waiting_workspace_t workspace;
static ps_scene_objects_t before, reference;
static ps_scene_objects_snapshot_t snapshot;
static uint8_t scheduled_pixels[DISPLAY_RENDERER_BUFFER_SIZE];

int main(int argc, char **argv)
{
  static const uint64_t deltas[] = {0, 1, 49, 149, 150, 151, 399, 550, 950, 1750,
                                   5000, 10000000000ULL, UINT64_MAX - 650};
  uint32_t size, mode, index, step, remaining, unused_next;
  FILE *output;
  assert(argc == 4);
  mode = (uint32_t)atoi(argv[3]);
  size = read_blob(argv[1], candidate);
  set_hash(argv[1], candidate, size);
  assert(PS_SceneRuntime_EnterDevelopmentObjects(candidate, size) == 0);
  assert(PS_SceneRuntime_AdvanceDevelopmentObjects(650) == 0);
  before = s_ps_object_graph.objects;
  if (mode == 1)
  {
    program.step_count = 4;
    assert(PS_ObjectWaiting_Build(&before, 1, &program, &workspace) == PS_OBJECT_WAITING_CAPACITY);
    assert(program.step_count == 0);
    assert(memcmp(&before, &s_ps_object_graph.objects, sizeof(before)) == 0);
    return 0;
  }
  assert(PS_ObjectWaiting_Build(&before, 1, &program, &workspace) == PS_OBJECT_WAITING_OK);
  assert(memcmp(&before, &s_ps_object_graph.objects, sizeof(before)) == 0);
  if (mode == 2) { assert(program.step_count == 1 && program.quantum_ms == 0); }
  if (mode == 3) { assert(program.step_count == 12 && program.quantum_ms == 200); }
  if (mode == 0) { assert(program.step_count == 4 && program.quantum_ms == 400 && program.initial_remaining_ms == 150); }
  if (mode == 5) { assert(program.step_count == 8 && program.quantum_ms == 400 && program.initial_remaining_ms == 150); }
  output = fopen(argv[2], "wb"); assert(output != NULL);
  for (index = 0; index < sizeof(deltas) / sizeof(deltas[0]); ++index)
  {
    reference = before;
    assert(PS_SceneObjects_Advance(&reference, deltas[index]) == PS_SCENE_OBJECTS_OK);
    assert(PS_ObjectWaiting_Resolve(&program, reference.elapsed_ms, &step, &remaining) == PS_OBJECT_WAITING_OK);
    if (program.quantum_ms != 0)
    { assert(remaining == program.quantum_ms - reference.elapsed_ms % program.quantum_ms); }
    else { assert(step == 0 && remaining == 0); }
    assert(PS_ObjectWaiting_Project(&program, step, &workspace.snapshot, &model) == PS_OBJECT_WAITING_OK);
    frame(output);
    memcpy(scheduled_pixels, s_display_framebuffer, sizeof(scheduled_pixels));
    assert(PS_SceneObjects_Snapshot(&reference, &snapshot) == PS_SCENE_OBJECTS_OK);
    assert(PS_SceneObjectRender_Project(&snapshot, 1, &model, &unused_next) == 0);
    frame(output);
    assert(memcmp(scheduled_pixels, s_display_framebuffer, sizeof(scheduled_pixels)) == 0);
  }
  fclose(output);
  assert(PS_ObjectWaiting_Resolve(&program, 649, &step, &remaining) == PS_OBJECT_WAITING_TIME);
  assert(PS_ObjectWaiting_Project(&program, program.step_count, &workspace.snapshot, &model) == PS_OBJECT_WAITING_ARGUMENT);
  assert(PS_ObjectWaiting_Project(&program, 0, &program.base, &model) == PS_OBJECT_WAITING_ARGUMENT);
  assert(PS_ObjectWaiting_Build(&before, 0, &program, &workspace) == PS_OBJECT_WAITING_ARGUMENT);
  assert(program.step_count == 0);
  reference = before;
  assert(PS_SceneObjects_Suspend(&reference, 1) == PS_SCENE_OBJECTS_OK);
  assert(PS_SceneObjects_Advance(&reference, 5000) == PS_SCENE_OBJECTS_OK);
  assert(reference.elapsed_ms == 650);
  assert(PS_ObjectWaiting_Build(&reference, 1, &program, &workspace) == PS_OBJECT_WAITING_ARGUMENT);
  assert(program.step_count == 0);
  if (mode == 0 || mode == 5)
  {
    assert(PS_SceneRuntime_HandleStateSceneInput(1, 1) == PS_SCENE_RUNTIME_INPUT_APPLIED);
    assert(PS_ObjectWaiting_Build(&s_ps_object_graph.objects, 1, &program, &workspace) == PS_OBJECT_WAITING_OK);
    assert(program.initial_remaining_ms == 150 && program.base.elapsed_ms == 650);
    assert(program.base.objects[0].step == 1 && program.base.objects[1].effective.x == 120);
    if (mode == 5)
    {
      assert(program.base.objects[8].step == 0 && program.base.objects[8].remaining_ms == 150);
      assert(PS_SceneRuntime_AdvanceDevelopmentObjects(300) == 0);
      assert(PS_SceneRuntime_HandleStateSceneInput(1, 2) == PS_SCENE_RUNTIME_INPUT_APPLIED);
      assert(PS_ObjectWaiting_Build(&s_ps_object_graph.objects, 1, &program, &workspace) == PS_OBJECT_WAITING_OK);
      assert(program.initial_remaining_ms == 250 && program.base.objects[1].effective.x == 32);
      assert(program.base.objects[0].step == 2 && program.base.objects[0].remaining_ms == 250);
      assert(program.base.objects[8].step == 1 && program.base.objects[8].remaining_ms == 650);
    }
    reference = before;
    reference.elapsed_ms = UINT64_MAX - 1;
    assert(PS_ObjectWaiting_Build(&reference, 1, &program, &workspace) == PS_OBJECT_WAITING_TIME);
    assert(program.step_count == 0);
  }
  return 0;
}
