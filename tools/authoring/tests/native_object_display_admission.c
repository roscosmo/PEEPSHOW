#include <stdint.h>
static void observe_private_raster(uint32_t stage, uint32_t end);
#define NATIVE_OBJECT_RASTER_OBSERVE observe_private_raster
#define PS_OBJECT_AWAKE_MAIN awake_main
#include "native_object_awake.c"
#define LS013B7DH05_H
#define LCD_DMA_MAX_ROWS_PER_TRANSFER 48U
#include "ps_lpbam_display_buffers.c"
#include "object_display_under_test.inc"
#define DISPLAY_RENDERER_H
#include "ps_scene_object_display_admission.c"

static ps_scene_runtime_state_scene_t candidate_scene;
static ps_egg_sprite_catalog_t candidate_catalog;
static ps_scene_objects_t bank, saved_bank;
static ps_scene_objects_snapshot_t scratch;
static ps_object_waiting_workspace_t waiting_workspace;
static ps_object_waiting_program_t program, saved_program;
static ps_object_display_workspace_t check_workspace;
static ps_object_display_result_t checked;
static uint8_t payload_before[40000], payload_after[40000];
static uint8_t display_before[DISPLAY_RENDERER_BUFFER_SIZE], reference[DISPLAY_RENDERER_BUFFER_SIZE];
static uint32_t profile_clock_calls;
static uint32_t profile_clock(void) { return profile_clock_calls++; }
static uint8_t raster_live_before[DISPLAY_RENDERER_BUFFER_SIZE];
static uint32_t raster_observe, raster_copy_groups;

static void observe_private_raster(uint32_t stage, uint32_t end)
{
  if (raster_observe == 0) { return; }
  assert(memcmp(s_display_framebuffer, raster_live_before, sizeof(raster_live_before)) == 0);
  if (stage == PS_TRACE_RASTER_COPY && end == 0) { raster_copy_groups++; }
}

static void cached_frame_matches(ps_scene_frame_cache_t *cache,
  const ps_scene_render_model_t *frame_model, const ps_egg_sprite_catalog_t *catalog)
{
  uint8_t actual[DISPLAY_RENDERER_BUFFER_SIZE], expected[DISPLAY_RENDERER_BUFFER_SIZE];
  uint8_t live[DISPLAY_RENDERER_BUFFER_SIZE];
  uint32_t rotation = s_rotate_ccw;
  memcpy(live, s_display_framebuffer, sizeof(live));
  memcpy(raster_live_before, live, sizeof(live));
  raster_observe = 1;
  assert(DisplayRenderer_CopyCandidateSceneFrame(frame_model, catalog, expected, sizeof(expected)));
  assert(s_display_draw_framebuffer == s_display_framebuffer);
  raster_copy_groups = 0;
  assert(DisplayRenderer_CopyCandidateSceneFrameCached(frame_model, catalog, cache, actual, sizeof(actual)));
  assert(s_display_draw_framebuffer == s_display_framebuffer);
  assert(raster_copy_groups == 1); /* Only the completed frame goes to the packer. */
  assert(s_display_draw_clip == NULL);
  assert(s_rotate_ccw == rotation);
  raster_observe = 0;
  assert(memcmp(actual, expected, sizeof(actual)) == 0);
  assert(memcmp(live, s_display_framebuffer, sizeof(live)) == 0);
  assert(s_display_candidate_catalog == NULL);
  /* A normal draw must still hit the live frame after private composition. */
  memset(s_display_framebuffer, 255, sizeof(s_display_framebuffer));
  s_display_candidate_catalog = catalog;
  s_rotate_ccw = 1;
  (void)DisplayRenderer_DrawSceneModel(frame_model);
  s_rotate_ccw = rotation;
  s_display_candidate_catalog = NULL;
  assert(memcmp(expected, s_display_framebuffer, sizeof(expected)) == 0);
  assert(memcmp(actual, cache->frame, sizeof(actual)) == 0);
  memcpy(s_display_framebuffer, live, sizeof(live));
}

static void clear_rect_matches(uint16_t x, uint16_t y, uint16_t width, uint16_t height)
{
  uint8_t actual[DISPLAY_RENDERER_BUFFER_SIZE + 2], expected[sizeof(actual)];
  for (uint32_t i = 0; i < sizeof(actual); ++i)
  { actual[i] = (uint8_t)(i * 37U + x + y); }
  memcpy(expected, actual, sizeof(actual));
  DisplayRenderer_ClearLogicalRectInBuffer(actual + 1, x, y, width, height);
  for (uint32_t px = x; px < (uint32_t)x + width && px < DISPLAY_RENDERER_WIDTH; ++px)
  {
    for (uint32_t py = y; py < (uint32_t)y + height && py < DISPLAY_RENDERER_HEIGHT; ++py)
    { DisplayRenderer_SetLogicalPixelInBuffer(expected + 1, (uint16_t)px, (uint16_t)py, 0); }
  }
  assert(memcmp(actual, expected, sizeof(actual)) == 0);
}

static void clear_rect_equivalence(void)
{
  /* Every panel-column interval exercises all single-byte and edge masks. */
  for (uint16_t y = 0; y < DISPLAY_RENDERER_HEIGHT; ++y)
  {
    for (uint16_t height = 0; height <= DISPLAY_RENDERER_HEIGHT - y; ++height)
    {
      clear_rect_matches((uint16_t)((y + height) % DISPLAY_RENDERER_WIDTH), y, 3, height);
      clear_rect_matches(0, y, 1, height);
      clear_rect_matches(DISPLAY_RENDERER_WIDTH - 1, y, 8, height);
    }
  }
  clear_rect_matches(0, 0, DISPLAY_RENDERER_WIDTH, DISPLAY_RENDERER_HEIGHT);
  clear_rect_matches(0, 0, UINT16_MAX, UINT16_MAX);
  clear_rect_matches(4, 7, UINT16_MAX, UINT16_MAX);
  clear_rect_matches(DISPLAY_RENDERER_WIDTH, 0, 1, 1);
  clear_rect_matches(0, DISPLAY_RENDERER_HEIGHT, 1, 1);
  clear_rect_matches(UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX);
  clear_rect_matches(0, 0, 0, 8);
  DisplayRenderer_ClearLogicalRectInBuffer(NULL, 0, 0, 8, 8);
  puts("byte clearing matches pixel oracle: masks, rotation, clipping and guards");
}

static void raster_cache_equivalence(void)
{
  static ps_scene_frame_cache_t cache;
  uint8_t record[PS_EGG_ASSET_RECORD_SIZE] = {0}, payload[78];
  ps_egg_sprite_catalog_t catalog = {record, payload, sizeof(payload), 1, NULL, 0};
  ps_scene_render_model_t m = {.api_version = PS_SCENE_RENDER_MODEL_API_VERSION,
    .scene_id = 1, .element_count = 4};
  record[4] = 17; record[6] = 13; record[8] = 3;
  record[20] = 39; record[24] = 39; record[28] = 39;
  for (uint32_t i = 0; i < sizeof(payload); ++i) { payload[i] = (uint8_t)(i * 37U + 19U); }
  m.elements[0] = (ps_scene_render_element_t){.element_id=1, .visible=1,
    .type=PS_SCENE_RENDER_ELEMENT_FILLED_RECT, .x=20, .y=20, .width=35, .height=35};
  m.elements[1] = (ps_scene_render_element_t){.element_id=2, .visible=1, .layer=1,
    .type=PS_SCENE_RENDER_ELEMENT_SPRITE_1BPP, .asset_id=65537, .x=24, .y=24, .width=17, .height=13};
  m.elements[2] = (ps_scene_render_element_t){.element_id=3, .visible=1, .layer=2,
    .type=PS_SCENE_RENDER_ELEMENT_OUTLINE_RECT, .x=30, .y=30, .width=32, .height=32};
  m.elements[3] = (ps_scene_render_element_t){.element_id=4, .visible=1,
    .type=PS_SCENE_RENDER_ELEMENT_FILLED_ELLIPSE, .x=120, .y=100, .width=17, .height=13};
  memset(s_display_framebuffer, 0xA5, sizeof(s_display_framebuffer));
  for (uint32_t opaque = 0; opaque < 2; ++opaque)
  {
    record[32] = opaque ? PS_EGG_ASSET_FLAG_OPAQUE : 0;
    cache.valid = 0; /* Same IDs, changed immutable content must be invalidated. */
    cached_frame_matches(&cache, &m, &catalog);
    for (uint32_t x = 0; x <= 151; ++x)
    {
      m.elements[1].x = (uint16_t)x;
      m.elements[1].y = (uint16_t)(x % 132);
      cached_frame_matches(&cache, &m, &catalog);
    }
    for (uint32_t i = 0; i < 4; ++i)
    {
      m.elements[i].visible = 0; cached_frame_matches(&cache, &m, &catalog);
      m.elements[i].visible = 1; cached_frame_matches(&cache, &m, &catalog);
      m.elements[i].layer = (m.elements[i].layer + 1) % 4;
      m.elements[i].z_order = (uint16_t)(3 - i);
      cached_frame_matches(&cache, &m, &catalog);
    }
    ps_scene_render_element_t swap = m.elements[0];
    m.elements[0] = m.elements[1]; m.elements[1] = swap;
    cached_frame_matches(&cache, &m, &catalog);
    m.element_count = 3; cached_frame_matches(&cache, &m, &catalog);
    m.element_count = 4; cached_frame_matches(&cache, &m, &catalog);
    uint32_t drawn = cache.elements_drawn;
    cached_frame_matches(&cache, &m, &catalog);
    assert(cache.elements_drawn == drawn); /* Identical frame: no drawing. */
    m.scene_id++; cached_frame_matches(&cache, &m, &catalog);
    swap = m.elements[0]; m.elements[0] = m.elements[1]; m.elements[1] = swap;
    cached_frame_matches(&cache, &m, &catalog);
  }
  assert(cache.reused_frames > 300);
  catalog.frame_count = 0;
  assert(!DisplayRenderer_CopyCandidateSceneFrameCached(&m, &catalog, &cache, reference, sizeof(reference)));
  assert(!cache.valid && s_display_candidate_catalog == NULL);
  assert(s_display_draw_framebuffer == s_display_framebuffer);
  catalog.frame_count = 1;
  cached_frame_matches(&cache, &m, &catalog);

  const uint32_t shapes[] = {PS_SCENE_RENDER_ELEMENT_LINE, PS_SCENE_RENDER_ELEMENT_LINE_UP_RIGHT,
    PS_SCENE_RENDER_ELEMENT_HORIZONTAL_LINE, PS_SCENE_RENDER_ELEMENT_OUTLINE_RECT,
    PS_SCENE_RENDER_ELEMENT_FILLED_RECT, PS_SCENE_RENDER_ELEMENT_CIRCLE,
    PS_SCENE_RENDER_ELEMENT_ELLIPSE, PS_SCENE_RENDER_ELEMENT_FILLED_CIRCLE,
    PS_SCENE_RENDER_ELEMENT_FILLED_ELLIPSE};
  m.scene_id++; m.element_count = 1;
  for (uint32_t i = 0; i < sizeof(shapes) / sizeof(shapes[0]); ++i)
  {
    m.elements[0] = (ps_scene_render_element_t){.element_id=1, .visible=1,
      .type=shapes[i], .x=3, .y=5, .width=17, .height=17};
    s_rotate_ccw = i % 2;
    cached_frame_matches(&cache, &m, &catalog);
    m.elements[0].x = 151; m.elements[0].y = 127;
    cached_frame_matches(&cache, &m, &catalog);
  }
  uint8_t private_frame[DISPLAY_RENDERER_BUFFER_SIZE];
  s_display_draw_framebuffer = private_frame;
  assert(!DisplayRenderer_CopySceneModelFrame(&m, reference, sizeof(reference)));
  assert(!DisplayRenderer_CopyCandidateSceneFrameCached(&m, &catalog, &cache, reference, sizeof(reference)));
  assert(s_display_draw_framebuffer == private_frame && s_display_candidate_catalog == NULL);
  s_display_draw_framebuffer = s_display_framebuffer;
  assert(!DisplayRenderer_CopyCandidateSceneFrameCached(&m, &catalog, &cache, reference, 1));
  assert(!DisplayRenderer_CopyCandidateSceneFrameCached(&m, &catalog, &cache, NULL, sizeof(reference)));
  m.elements[0].type = PS_SCENE_RENDER_ELEMENT_FOCUS;
  m.elements[0].animation_binding_id = PS_SCENE_RENDER_ANIMATION_CURSOR;
  assert(!DisplayRenderer_CopyCandidateSceneFrame(&m, &catalog, reference, sizeof(reference)));
  assert(!DisplayRenderer_CopyCandidateSceneFrameCached(&m, &catalog, &cache, reference, sizeof(reference)));
  assert(s_display_draw_framebuffer == s_display_framebuffer && s_display_candidate_catalog == NULL);
  m.elements[0].type = PS_SCENE_RENDER_ELEMENT_FILLED_RECT;
  m.elements[0].animation_binding_id = 0;
  cached_frame_matches(&cache, &m, &catalog);
  s_rotate_ccw = 1;
  puts("cached pixels match full raster: motion, overlap, masks, layers, removal and invalidation");
}

static void clipped_regions_equivalence(void)
{
  static ps_scene_frame_cache_t cache;
  uint8_t record[PS_EGG_ASSET_RECORD_SIZE] = {0}, payload[78];
  ps_egg_sprite_catalog_t catalog = {record, payload, sizeof(payload), 1, NULL, 0};
  ps_scene_render_model_t m = {.api_version=PS_SCENE_RENDER_MODEL_API_VERSION,
    .scene_id=1, .element_count=10};
  record[4] = 17; record[6] = 13; record[8] = 3;
  record[20] = 39; record[24] = 39; record[28] = 39;
  for (uint32_t i = 0; i < sizeof(payload); ++i) { payload[i] = (uint8_t)(i * 37U + 19U); }
  m.elements[0] = (ps_scene_render_element_t){.element_id=1, .visible=1,
    .type=PS_SCENE_RENDER_ELEMENT_OUTLINE_RECT, .x=4, .y=4, .width=160, .height=136};
  m.elements[1] = (ps_scene_render_element_t){.element_id=2, .visible=1, .z_order=1,
    .type=PS_SCENE_RENDER_ELEMENT_FILLED_RECT, .x=72, .y=32, .width=24, .height=24};
  for (uint32_t i = 2; i < m.element_count; ++i)
  {
    m.elements[i] = (ps_scene_render_element_t){.element_id=i+1, .visible=1, .z_order=1,
      .type=PS_SCENE_RENDER_ELEMENT_FILLED_RECT, .x=(uint16_t)(10+16*i),
      .y=100, .width=8, .height=8};
  }
  cached_frame_matches(&cache, &m, &catalog);
  for (uint32_t i = 0; i < 4; ++i)
  {
    m.elements[1].type = (i & 1) ? PS_SCENE_RENDER_ELEMENT_FILLED_RECT :
      PS_SCENE_RENDER_ELEMENT_OUTLINE_RECT;
    cached_frame_matches(&cache, &m, &catalog);
  }
  assert(cache.full_frames == 1 && cache.reused_frames == 4);
  /* Ten initially; only border + digit intersect each subsequent clip. The
   * border's edges are outside the clip and cannot damage unchanged pixels. */
  assert(cache.elements_drawn == 18);

  const uint32_t shapes[] = {PS_SCENE_RENDER_ELEMENT_LINE, PS_SCENE_RENDER_ELEMENT_LINE_UP_RIGHT,
    PS_SCENE_RENDER_ELEMENT_HORIZONTAL_LINE, PS_SCENE_RENDER_ELEMENT_OUTLINE_RECT,
    PS_SCENE_RENDER_ELEMENT_FILLED_RECT, PS_SCENE_RENDER_ELEMENT_CIRCLE,
    PS_SCENE_RENDER_ELEMENT_ELLIPSE, PS_SCENE_RENDER_ELEMENT_FILLED_CIRCLE,
    PS_SCENE_RENDER_ELEMENT_FILLED_ELLIPSE};
  m.element_count = 3;
  m.elements[1] = (ps_scene_render_element_t){.element_id=2, .visible=1, .layer=1,
    .type=PS_SCENE_RENDER_ELEMENT_SPRITE_1BPP, .asset_id=65537, .width=17, .height=13};
  m.elements[2] = (ps_scene_render_element_t){.element_id=3, .visible=1, .layer=2,
    .type=PS_SCENE_RENDER_ELEMENT_LINE_UP_RIGHT, .x=30, .y=30, .width=90, .height=90};
  for (uint32_t shape = 0; shape < sizeof(shapes) / sizeof(shapes[0]); ++shape)
  {
    m.elements[0] = (ps_scene_render_element_t){.element_id=1, .visible=1,
      .type=shapes[shape], .x=50, .y=40, .width=51, .height=51};
    for (uint32_t opaque = 0; opaque < 2; ++opaque)
    {
      record[32] = opaque ? PS_EGG_ASSET_FLAG_OPAQUE : 0;
      cache.valid = 0;
      for (uint32_t pos = 0; pos <= 151; ++pos)
      {
        m.elements[1].x = (uint16_t)pos;
        m.elements[1].y = (uint16_t)(pos % 132);
        cached_frame_matches(&cache, &m, &catalog);
      }
      /* Multiple changed regions, visibility, geometry, ordering and one-pixel damage. */
      m.elements[0].width = 1; m.elements[0].height = 1;
      m.elements[0].type = PS_SCENE_RENDER_ELEMENT_FILLED_RECT;
      m.elements[2].visible ^= 1;
      cached_frame_matches(&cache, &m, &catalog);
      m.elements[0].x++; m.elements[0].y++;
      m.elements[0].layer = 3;
      cached_frame_matches(&cache, &m, &catalog);
      m.elements[0].type = shapes[shape];
      m.elements[0].width = 51; m.elements[0].height = 51;
      m.elements[0].layer = 0;
    }
  }
  puts("clipped regions match full raster: border 18 not 50 draws, partial shapes and masked/opaque sprites");
}

static uint32_t full_frame_oracle(void *context, uint32_t step, uint8_t *destination, uint32_t capacity)
{
  (void)context;
  assert(PS_ObjectWaiting_Project(&program, step, &scratch, &model) == PS_OBJECT_WAITING_OK);
  return DisplayRenderer_CopyCandidateSceneFrame(&model, &candidate_catalog, destination, capacity);
}

static void packing_equivalence(const ps_lpbam_display_check_workspace_t *a,
  const ps_lpbam_display_check_workspace_t *b)
{
  assert(memcmp(a->previous, b->previous, sizeof(a->previous)) == 0);
  assert(memcmp(a->target, b->target, sizeof(a->target)) == 0);
  assert(memcmp(a->length, b->length, sizeof(a->length)) == 0);
  assert(memcmp(a->band, b->band, sizeof(a->band)) == 0);
  assert(a->frames_composed == b->frames_composed);
  for (uint32_t slot = 0; slot < PS_LPBAM_DISPLAY_PAYLOAD_SLOT_COUNT; ++slot)
  {
    assert(a->length[slot] <= sizeof(a->payload[slot]));
    assert(memcmp(a->payload[slot], b->payload[slot], a->length[slot]) == 0);
  }
}

static void timing_equivalence(uint32_t expected_status)
{
  static ps_lpbam_display_check_workspace_t saved_packing;
  ps_object_display_result_t saved_result = checked;
  ps_display_work_profile_t timing = { .clock = profile_clock };
  uint32_t stage;
  saved_packing = check_workspace.packing;
  static ps_lpbam_display_check_workspace_t oracle;
  ps_lpbam_display_admission_t oracle_result;
  assert(PS_LpbamDisplay_CheckFullSceneAnimation(program.step_count, full_frame_oracle, NULL,
    &oracle, &oracle_result) == (expected_status ? HAL_ERROR : HAL_OK));
  packing_equivalence(&oracle, &saved_packing);
  assert(memcmp(&oracle_result, &checked.payload, sizeof(oracle_result)) == 0);
  profile_clock_calls = UINT32_MAX - 3U; /* Exercise elapsed subtraction across wrap. */
  assert(PS_ObjectDisplay_CheckWaitingProfiled(&program, &candidate_catalog,
    &check_workspace, &checked, &timing) == expected_status);
  assert(memcmp(&saved_result, &checked, sizeof(checked)) == 0);
  packing_equivalence(&saved_packing, &check_workspace.packing);
  assert(timing.calls[PS_DISPLAY_WORK_PROJECT] == checked.frames_composed);
  assert(timing.calls[PS_DISPLAY_WORK_RASTER] == checked.frames_composed);
  assert(timing.calls[PS_DISPLAY_WORK_COMPARE] == checked.payload.sequence_used + expected_status);
  assert(timing.calls[PS_DISPLAY_WORK_PAYLOAD] == checked.payload.sequence_used + expected_status);
  assert(timing.calls[PS_DISPLAY_WORK_COPY] == checked.payload.sequence_used + 1U);
  for (stage = 0; stage < PS_DISPLAY_WORK_COUNT; ++stage)
  { assert(timing.ticks[stage] == timing.calls[stage]); }
  memset(&timing, 0, sizeof(timing));
  profile_clock_calls = 0;
  assert(PS_ObjectDisplay_CheckWaitingProfiled(&program, &candidate_catalog,
    &check_workspace, &checked, &timing) == expected_status);
  assert(profile_clock_calls == 0); /* NULL clock is inert, even with a profile. */
  assert(memcmp(&saved_result, &checked, sizeof(checked)) == 0);
  packing_equivalence(&saved_packing, &check_workspace.packing);
  for (stage = 0; stage < PS_DISPLAY_WORK_COUNT; ++stage)
  { assert(timing.ticks[stage] == 0 && timing.calls[stage] == 0); }
  assert(PS_ObjectDisplay_CheckWaitingCached(&program, &candidate_catalog,
    &check_workspace, &checked, NULL, 1) == expected_status);
  packing_equivalence(&saved_packing, &check_workspace.packing);
  if (expected_status == 0)
  {
    assert(check_workspace.raster_cache.full_frames == 0);
    assert(check_workspace.raster_cache.reused_frames == program.step_count + 1);
  }
}

static uint32_t payload_snapshot(uint8_t *out)
{
  uint32_t offset = 0;
#define TAKE(value) do { memcpy(out + offset, &(value), sizeof(value)); offset += sizeof(value); } while (0)
  TAKE(ps_lpbam_display_frame_a); TAKE(ps_lpbam_display_frame_b);
  TAKE(ps_lpbam_display_payload_arena); TAKE(ps_lpbam_display_payload_scratch);
  TAKE(ps_lpbam_display_tx); TAKE(ps_lpbam_display_tx_len); TAKE(ps_lpbam_display_tx_payload_slot);
  TAKE(ps_lpbam_display_payload_slot); TAKE(ps_lpbam_display_payload_slot_capacity);
  TAKE(ps_lpbam_display_payload_slot_len); TAKE(ps_lpbam_display_payload_slot_band);
  TAKE(ps_lpbam_display_payload_slot_occupied); TAKE(ps_lpbam_display_sequence);
  TAKE(ps_lpbam_display_admission); TAKE(ps_lpbam_display_active_sequence_count);
  TAKE(ps_lpbam_display_active_chunk_count); TAKE(ps_lpbam_display_payload_wire_bytes);
  TAKE(ps_lpbam_display_sequence_start_frame); TAKE(ps_lpbam_display_queue_start_slot);
  TAKE(ps_lpbam_display_payload_used); TAKE(ps_lpbam_display_experiment_variant);
  TAKE(ps_lpbam_display_candidate_rows); TAKE(ps_lpbam_display_candidate_row_enabled);
  TAKE(ps_lpbam_display_dirty_row_list); TAKE(ps_lpbam_display_expected_sequence_count);
  TAKE(ps_lpbam_display_first_candidate_row);
#undef TAKE
  assert(offset < sizeof(payload_before));
  return offset;
}

typedef struct { uint32_t calls, fail, hold; } compose_test_t;
static uint32_t byte_change_frame(void *opaque, uint32_t step, uint8_t *destination, uint32_t capacity)
{
  uint32_t offset = *(const uint32_t *)opaque;
  uint32_t band;
  assert(capacity == DISPLAY_RENDERER_BUFFER_SIZE);
  memset(destination, 0xA5, capacity);
  if (step == 0) { return 1; }
  if (offset < capacity) { destination[offset] ^= (uint8_t)(1U << (offset % 8)); }
  else if (offset == capacity + 1U)
  {
    destination[PS_LPBAM_DISPLAY_SPATIAL_ROWS * LINE_WIDTH - 1] ^= 1;
    destination[PS_LPBAM_DISPLAY_SPATIAL_ROWS * LINE_WIDTH] ^= 128;
  }
  else if (offset == capacity + 2U)
  {
    for (band = 0; band < PS_LPBAM_DISPLAY_SPATIAL_CHUNK_COUNT; ++band)
    { destination[(band + 1U) * PS_LPBAM_DISPLAY_SPATIAL_ROWS * LINE_WIDTH - 1U] ^= 8; }
  }
  return 1;
}

static void band_equivalence(void)
{
  uint16_t rows[DISPLAY_HEIGHT];
  uint32_t offset, row, step, slot, other;
  ps_lpbam_display_admission_t result;
  for (row = 0; row < DISPLAY_HEIGHT; ++row) { rows[row] = (uint16_t)(row + 1); }
  /* Every byte in every row, plus unchanged, adjacent-band and all-band cases.
   * Both the changed frame and its wrap back to the original must be packed. */
  for (offset = 0; offset <= DISPLAY_RENDERER_BUFFER_SIZE + 2U; ++offset)
  {
    assert(PS_LpbamDisplay_CheckFullSceneAnimation(2, byte_change_frame, &offset,
      &check_workspace.packing, &result) == HAL_OK);
    assert(check_workspace.packing.frames_composed == 3);
    byte_change_frame(&offset, 0, &ps_lpbam_display_frame_a[0][0], DISPLAY_RENDERER_BUFFER_SIZE);
    assert(PS_LpbamDisplay_BeginPreparedAnimation(rows, DISPLAY_HEIGHT, 2, 0) == HAL_OK);
    for (step = 0; step < 2; ++step)
    {
      byte_change_frame(&offset, (step + 1) % 2, &ps_lpbam_display_frame_b[0][0], DISPLAY_RENDERER_BUFFER_SIZE);
      assert(PS_LpbamDisplay_AppendPreparedTransition(ps_lpbam_display_frame_a, ps_lpbam_display_frame_b) == HAL_OK);
      memcpy(ps_lpbam_display_frame_a, ps_lpbam_display_frame_b, sizeof(ps_lpbam_display_frame_a));
    }
    assert(PS_LpbamDisplay_FinishPreparedAnimation() == HAL_OK);
    assert(memcmp(&result, &ps_lpbam_display_admission, sizeof(result)) == 0);
    for (slot = 0; slot < PS_LPBAM_DISPLAY_PAYLOAD_SLOT_COUNT; ++slot)
    {
      if (check_workspace.packing.length[slot] == 0) { continue; }
      for (other = 0; other < PS_LPBAM_DISPLAY_PAYLOAD_SLOT_COUNT; ++other)
      {
        if (ps_lpbam_display_payload_slot_occupied[other] &&
            check_workspace.packing.band[slot] == ps_lpbam_display_payload_slot_band[other] &&
            check_workspace.packing.length[slot] == ps_lpbam_display_payload_slot_len[other] &&
            memcmp(check_workspace.packing.payload[slot], ps_lpbam_display_payload_slot[other],
              check_workspace.packing.length[slot]) == 0) { break; }
      }
      assert(other < PS_LPBAM_DISPLAY_PAYLOAD_SLOT_COUNT);
    }
    if (offset < DISPLAY_RENDERER_BUFFER_SIZE)
    {
      assert(result.chunk_used == 2 && result.payload_used_bytes == 1168);
      assert(check_workspace.packing.band[0] == offset / (PS_LPBAM_DISPLAY_SPATIAL_ROWS * LINE_WIDTH));
      assert(check_workspace.packing.band[1] == check_workspace.packing.band[0]);
    }
    else if (offset == DISPLAY_RENDERER_BUFFER_SIZE)
    { assert(result.chunk_used == 2 && result.payload_used_bytes == 584); }
  }
  puts("all frame bytes and band boundaries match row-based payloads");
}

static uint32_t synthetic_frame(void *opaque, uint32_t step, uint8_t *destination, uint32_t capacity)
{
  compose_test_t *test = opaque;
  static const uint8_t fill[] = {0, 255, 0xAA, 0x55};
  assert(capacity == DISPLAY_RENDERER_BUFFER_SIZE);
  if (++test->calls == test->fail) { return 0; }
  memset(destination, test->hold ? 255 : fill[step % 4], capacity);
  return 1;
}

static void resource_boundaries(void)
{
  ps_lpbam_display_admission_t result;
  compose_test_t test = {0};
  uint32_t before = payload_snapshot(payload_before);
  assert(PS_LpbamDisplay_CheckFullSceneAnimation(3, synthetic_frame, &test,
    &check_workspace.packing, &result) == HAL_OK);
  assert(result.sequence_used == 3 && result.chunk_used == 18 && result.payload_used_bytes == 10512);
  test.calls = 0;
  assert(PS_LpbamDisplay_CheckFullSceneAnimation(4, synthetic_frame, &test,
    &check_workspace.packing, &result) == HAL_ERROR);
  assert(result.reason == PS_LPBAM_ADMISSION_REASON_CHUNKS && result.chunk_used == 18);
  test = (compose_test_t){0, 0, 1};
  assert(PS_LpbamDisplay_CheckFullSceneAnimation(12, synthetic_frame, &test,
    &check_workspace.packing, &result) == HAL_OK);
  assert(test.calls == 13 && result.chunk_used == 12 && result.payload_used_bytes == 584);
  assert(PS_LpbamDisplay_CheckFullSceneAnimation(13, synthetic_frame, &test,
    &check_workspace.packing, &result) == HAL_ERROR);
  assert(result.reason == PS_LPBAM_ADMISSION_REASON_SEQUENCE && test.calls == 13);
  test = (compose_test_t){0, 4, 1};
  assert(PS_LpbamDisplay_CheckFullSceneAnimation(3, synthetic_frame, &test,
    &check_workspace.packing, &result) == HAL_ERROR);
  assert(result.reason == PS_LPBAM_ADMISSION_REASON_BUILD && test.calls == 4);
  assert(check_workspace.packing.frames_composed == 3 && result.sequence_used == 2);
  assert(PS_LpbamDisplay_CheckFullSceneAnimation(1, NULL, NULL, &check_workspace.packing, &result) == HAL_ERROR);
  assert(PS_LpbamDisplay_CheckFullSceneAnimation(1, synthetic_frame, &test, NULL, &result) == HAL_ERROR);
  assert(PS_LpbamDisplay_CheckFullSceneAnimation(0, synthetic_frame, &test, &check_workspace.packing, &result) == HAL_ERROR);
  assert(payload_snapshot(payload_after) == before && memcmp(payload_before, payload_after, before) == 0);
}

static void poisoned_workspace_equivalence(void)
{
  static ps_lpbam_display_check_workspace_t clean, poisoned;
  static const struct { uint32_t steps, fail, hold; } cases[] = {
    {3, 0, 0}, {4, 0, 0}, {12, 0, 1}, {13, 0, 1},
    {3, 4, 1}, {1, 1, 0}, {1, 2, 0}, {0, 0, 0}, {1, 0, 1}
  };
  uint32_t before = payload_snapshot(payload_before);
  for (uint32_t index = 0; index < sizeof(cases) / sizeof(cases[0]); ++index)
  {
    ps_lpbam_display_admission_t expected, actual;
    compose_test_t test = {0, cases[index].fail, cases[index].hold};
    memset(&clean, 0, sizeof(clean));
    HAL_StatusTypeDef status = PS_LpbamDisplay_CheckFullSceneAnimation(
      cases[index].steps, synthetic_frame, &test, &clean, &expected);
    for (uint32_t pattern = 0; pattern < 2; ++pattern)
    {
      uint8_t poison = pattern ? 0x5A : 0xA5;
      memset(&poisoned, poison, sizeof(poisoned));
      for (uint32_t repeat = 0; repeat < 2; ++repeat)
      {
        test = (compose_test_t){0, cases[index].fail, cases[index].hold};
        assert(PS_LpbamDisplay_CheckFullSceneAnimation(cases[index].steps,
          synthetic_frame, &test, &poisoned, &actual) == status);
        assert(memcmp(&expected, &actual, sizeof(actual)) == 0);
        packing_equivalence(&clean, &poisoned);
        /* Untouched tails remain poison; the comparison must not depend on them. */
        for (uint32_t slot = 0; slot < PS_LPBAM_DISPLAY_PAYLOAD_SLOT_COUNT; ++slot)
        {
          for (uint32_t byte = poisoned.length[slot]; byte < sizeof(poisoned.payload[slot]); ++byte)
          { assert(poisoned.payload[slot][byte] == poison); }
        }
      }
    }
  }
  /* A smaller check after a full workspace must not retain occupied slots. */
  compose_test_t test = {0};
  ps_lpbam_display_admission_t result;
  assert(PS_LpbamDisplay_CheckFullSceneAnimation(3, synthetic_frame, &test, &poisoned, &result) == HAL_OK);
  test = (compose_test_t){0, 0, 1};
  assert(PS_LpbamDisplay_CheckFullSceneAnimation(1, synthetic_frame, &test, &poisoned, &result) == HAL_OK);
  assert(result.chunk_used == 1 && result.payload_used_bytes == 584);
  for (uint32_t slot = 1; slot < PS_LPBAM_DISPLAY_PAYLOAD_SLOT_COUNT; ++slot)
  { assert(poisoned.length[slot] == 0 && poisoned.band[slot] == 0); }
  memset(&poisoned, 0xA5, sizeof(poisoned));
  assert(PS_LpbamDisplay_CheckFullSceneAnimation(1, NULL, NULL, &poisoned, &result) == HAL_ERROR);
  assert(poisoned.frames_composed == 0);
  assert(poisoned.wire[0] == 0xA5 && poisoned.payload[0][0] == 0xA5);
  for (uint32_t slot = 0; slot < PS_LPBAM_DISPLAY_PAYLOAD_SLOT_COUNT; ++slot)
  { assert(poisoned.length[slot] == 0 && poisoned.band[slot] == 0); }
  assert(payload_snapshot(payload_after) == before && memcmp(payload_before, payload_after, before) == 0);
  puts("poisoned and reused workspace preserves admission and valid payload bytes");
}

int main(int argc, char **argv)
{
  static ps_egg_context_t saved_catalog;
  static ps_egg_state_loader_probe_t saved_loader;
  static ps_scene_runtime_probe_t saved_runtime;
  static ps_scene_object_graph_t saved_graph;
  ps_egg_v2_profile_result_t profile;
  uint16_t rows[DISPLAY_HEIGHT];
  uint32_t size, expected_failure, step, snapshot_size, status;
  HAL_StatusTypeDef packed;
  if (argc == 1) { band_equivalence(); return 0; }
  if (argc == 2 && strcmp(argv[1], "poisoned-workspace") == 0) { poisoned_workspace_equivalence(); return 0; }
  if (argc == 2 && strcmp(argv[1], "raster-cache") == 0) { raster_cache_equivalence(); return 0; }
  if (argc == 2 && strcmp(argv[1], "clear-rect") == 0) { clear_rect_equivalence(); return 0; }
  if (argc == 2 && strcmp(argv[1], "clip-regions") == 0) { clipped_regions_equivalence(); return 0; }
  assert(argc == 4);
  expected_failure = (uint32_t)atoi(argv[3]);
  baseline_size = read_blob(argv[1], baseline);
  set_hash(argv[1], baseline, baseline_size);
  assert(PS_SceneRuntime_EnterDevelopmentObjects(baseline, baseline_size) == 0);
  saved_catalog = s_ps_egg_runtime_context;
  saved_loader = g_ps_egg_state_loader_probe;
  saved_runtime = g_ps_scene_runtime_probe;
  saved_graph = s_ps_object_graph;
  for (step = 0; step < DISPLAY_HEIGHT; ++step) { rows[step] = (uint16_t)(step + 1); }
  memset(ps_lpbam_display_frame_a, 0xFF, sizeof(ps_lpbam_display_frame_a));
  memset(ps_lpbam_display_frame_b, 0, sizeof(ps_lpbam_display_frame_b));
  assert(PS_LpbamDisplay_BeginPreparedAnimation(rows, DISPLAY_HEIGHT, 2, 1) == HAL_OK);
  assert(PS_LpbamDisplay_AppendPreparedTransition(ps_lpbam_display_frame_a, ps_lpbam_display_frame_b) == HAL_OK);
  assert(PS_LpbamDisplay_AppendPreparedTransition(ps_lpbam_display_frame_b, ps_lpbam_display_frame_a) == HAL_OK);
  assert(PS_LpbamDisplay_FinishPreparedAnimation() == HAL_OK);
  resource_boundaries();
  snapshot_size = payload_snapshot(payload_before);
  memset(s_display_framebuffer, 0xA5, sizeof(s_display_framebuffer));
  memcpy(display_before, s_display_framebuffer, sizeof(display_before));
  s_rotate_ccw = 0;
  size = read_blob(argv[2], candidate);
  set_hash(argv[2], candidate, size);
  assert(PS_EggStateLoader_DecodeV2Candidate(candidate, size, &candidate_scene, &candidate_catalog, &profile) == 0);
  assert(profile.reason == 0 && s_ps_egg_validation_context.blob == NULL);
  assert(PS_SceneObjects_Init(&bank, &candidate_scene.object_definition, 10,
    (uint16_t)(candidate_scene.entry_state_id - 1)) == PS_SCENE_OBJECTS_OK);
  assert(PS_SceneObjects_Advance(&bank, 650) == PS_SCENE_OBJECTS_OK);
  saved_bank = bank;
  status = PS_ObjectWaiting_Build(&bank, candidate_scene.scene_id, &program, &waiting_workspace);
  if (expected_failure == 2)
  { assert(status == PS_OBJECT_WAITING_CAPACITY); }
  else
  {
    assert(status == PS_OBJECT_WAITING_OK);
    saved_program = program;
    status = PS_ObjectDisplay_CheckWaiting(&program, &candidate_catalog, &check_workspace, &checked);
    assert(status == (expected_failure != 0));
    assert(memcmp(&saved_program, &program, sizeof(program)) == 0);
    if (status == 0)
    {
      assert(checked.projection_status == 0 && checked.raster_status == 0);
      assert(checked.failed_step == UINT32_MAX && checked.frames_composed == program.step_count + 1);
    }
    else { assert(checked.payload.reason == PS_LPBAM_ADMISSION_REASON_CHUNKS); }
    timing_equivalence(status);
  }
  assert(memcmp(&saved_bank, &bank, sizeof(bank)) == 0);
  assert(payload_snapshot(payload_after) == snapshot_size && memcmp(payload_before, payload_after, snapshot_size) == 0);
  assert(memcmp(display_before, s_display_framebuffer, sizeof(display_before)) == 0);
  assert(s_rotate_ccw == 0 && s_display_candidate_catalog == NULL);
  assert(memcmp(&saved_catalog, &s_ps_egg_runtime_context, sizeof(saved_catalog)) == 0);
  assert(memcmp(&saved_loader, (const void *)&g_ps_egg_state_loader_probe, sizeof(saved_loader)) == 0);
  assert(memcmp(&saved_runtime, (const void *)&g_ps_scene_runtime_probe, sizeof(saved_runtime)) == 0);
  assert(memcmp(&saved_graph, &s_ps_object_graph, sizeof(saved_graph)) == 0);
  if (expected_failure == 2) { puts("schedule rejection isolated"); return 0; }

  /* Only after isolation checks, publish candidate catalogs as the independent
   * existing-packer oracle. IDs deliberately collide with the old package. */
  assert(PS_EggStateLoader_LoadDevelopment(candidate, size, &candidate_scene) == 0);
  assert(PS_ObjectWaiting_Project(&program, 0, &scratch, &model) == PS_OBJECT_WAITING_OK);
  assert(DisplayRenderer_CopySceneModelFrame(&model, reference, sizeof(reference)) == 1);
  if (expected_failure == 0)
  { assert(memcmp(reference, check_workspace.packing.previous, sizeof(reference)) == 0); }
  memcpy(ps_lpbam_display_frame_a, reference, sizeof(reference));
  packed = PS_LpbamDisplay_BeginPreparedAnimation(rows, DISPLAY_HEIGHT, (uint16_t)program.step_count, 0);
  for (step = 0; packed == HAL_OK && step < program.step_count; ++step)
  {
    assert(PS_ObjectWaiting_Project(&program, (step + 1) % program.step_count, &scratch, &model) == PS_OBJECT_WAITING_OK);
    assert(DisplayRenderer_CopySceneModelFrame(&model, &ps_lpbam_display_frame_b[0][0], sizeof(ps_lpbam_display_frame_b)) == 1);
    packed = PS_LpbamDisplay_AppendPreparedTransition(ps_lpbam_display_frame_a, ps_lpbam_display_frame_b);
    memcpy(ps_lpbam_display_frame_a, ps_lpbam_display_frame_b, sizeof(ps_lpbam_display_frame_a));
  }
  if (packed == HAL_OK) { packed = PS_LpbamDisplay_FinishPreparedAnimation(); }
  assert(packed == (expected_failure ? HAL_ERROR : HAL_OK));
  assert(memcmp(&checked.payload, &ps_lpbam_display_admission, sizeof(checked.payload)) == 0);
  printf("exact resources: steps=%u chunks=%u bytes=%u; live data unchanged\n",
    checked.payload.sequence_used, checked.payload.chunk_used, checked.payload.payload_used_bytes);

  /* A missing candidate frame may not be supplied by the now-active catalog. */
  if (program.quantum_ms != 0)
  {
    ps_egg_sprite_catalog_t missing = candidate_catalog;
    missing.frame_count = 0;
    assert(PS_ObjectDisplay_CheckWaiting(&program, &missing, &check_workspace, &checked) == 1);
    assert(checked.raster_status == 1 && checked.payload.reason == PS_LPBAM_ADMISSION_REASON_BUILD);
    assert(s_display_candidate_catalog == NULL && s_rotate_ccw == 0);
  }
  assert(PS_ObjectDisplay_CheckWaiting(NULL, &candidate_catalog, &check_workspace, &checked) == 1);
  assert(PS_ObjectDisplay_CheckWaiting(&program, NULL, &check_workspace, &checked) == 1);
  assert(PS_ObjectDisplay_CheckWaiting(&program, &candidate_catalog, NULL, &checked) == 1);
  assert(PS_ObjectDisplay_CheckWaiting(&program, &candidate_catalog, &check_workspace, NULL) == 1);
  assert(PS_EggStateLoader_DecodeV2Candidate(candidate, 1, &candidate_scene, &candidate_catalog, &profile) == 1);
  assert(candidate_scene.state_count == 0 && candidate_catalog.records == NULL);
  assert(memcmp(display_before, s_display_framebuffer, sizeof(display_before)) == 0);
  return 0;
}
