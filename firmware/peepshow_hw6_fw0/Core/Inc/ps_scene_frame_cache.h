#ifndef PS_SCENE_FRAME_CACHE_H
#define PS_SCENE_FRAME_CACHE_H

#include "ps_scene_render_model.h"

/* One private thDisplay raster, never a DMA buffer. The caller must invalidate
 * it whenever the immutable asset content or scene changes. */
#define PS_SCENE_FRAME_CACHE_BYTES \
  (PS_SCENE_RENDER_CANVAS_WIDTH * PS_SCENE_RENDER_CANVAS_HEIGHT / 8U)
typedef struct
{
  ps_scene_render_model_t model;
  uint8_t frame[PS_SCENE_FRAME_CACHE_BYTES];
  uint32_t valid;
  uint32_t full_frames;
  uint32_t reused_frames;
  uint32_t elements_drawn;
} ps_scene_frame_cache_t;

#endif
