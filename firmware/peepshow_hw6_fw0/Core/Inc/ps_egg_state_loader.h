#ifndef PS_EGG_STATE_LOADER_H
#define PS_EGG_STATE_LOADER_H

#include <stdint.h>

#include "ps_scene_runtime.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PS_EGG_STATE_LOADER_API_VERSION (12UL)
#define PS_EGG_STATE_LOADER_STATUS_NOT_RUN (0xFFFFFFFFUL)
#define PS_EGG_STATE_LOADER_SPRITE_FRAME_ID_BASE (0x00010000UL)
#define PS_EGG_STATE_LOADER_SCENE_MAX (8U)
#define PS_EGG_STATE_LOADER_AUDIO_ASSET_MAX (32U)
#define PS_EGG_STATE_LOADER_AUDIO_CUE_MAX (64U)
#define PS_EGG_STATE_LOADER_AUDIO_SAMPLE_RATE_HZ (16000UL)
#define PS_EGG_STATE_LOADER_AUDIO_BLOCK_SAMPLES (256UL)
#define PS_EGG_STATE_LOADER_AUDIO_SAMPLE_MAX (32000UL)

typedef struct
{
  const uint8_t *pixels;
  const uint8_t *mask;
  uint16_t width;
  uint16_t height;
  uint16_t row_stride_bytes;
  int16_t pivot_x;
  int16_t pivot_y;
  uint32_t opaque;
} ps_egg_state_loader_sprite_frame_t;

typedef struct
{
  const uint8_t *adpcm;
  uint32_t adpcm_size;
  uint32_t sample_count;
  uint32_t duration_ms;
  uint32_t block_count;
  uint32_t cue_index;
  uint32_t asset_index;
  uint32_t priority;
  uint32_t volume;
} ps_egg_state_loader_audio_cue_t;

typedef enum
{
  PS_EGG_STATE_LOADER_REASON_NONE = 0,
  PS_EGG_STATE_LOADER_REASON_ARGUMENT,
  PS_EGG_STATE_LOADER_REASON_CONTAINER,
  PS_EGG_STATE_LOADER_REASON_HEADER_CRC,
  PS_EGG_STATE_LOADER_REASON_PACKAGE_DIGEST,
  PS_EGG_STATE_LOADER_REASON_CHUNK,
  PS_EGG_STATE_LOADER_REASON_CHUNK_CRC,
  PS_EGG_STATE_LOADER_REASON_STRINGS,
  PS_EGG_STATE_LOADER_REASON_MANIFEST,
  PS_EGG_STATE_LOADER_REASON_SCENE_TABLE,
  PS_EGG_STATE_LOADER_REASON_GRAPH,
  PS_EGG_STATE_LOADER_REASON_RENDER,
  PS_EGG_STATE_LOADER_REASON_WAITING,
  PS_EGG_STATE_LOADER_REASON_CAPACITY,
  PS_EGG_STATE_LOADER_REASON_UNSUPPORTED,
  PS_EGG_STATE_LOADER_REASON_HASH,
  PS_EGG_STATE_LOADER_REASON_ASSET,
  PS_EGG_STATE_LOADER_REASON_AUDIO
} ps_egg_state_loader_reason_t;

typedef struct
{
  uint32_t api_version;
  uint32_t load_count;
  uint32_t last_status;
  uint32_t reason;
  uint32_t package_size;
  uint32_t package_id_hash_low;
  uint32_t package_id_hash_high;
  uint32_t chunk_count;
  uint32_t scene_count;
  uint32_t entry_scene_id;
  uint32_t selected_scene_id;
  uint32_t scene_decode_count;
  uint32_t graph_chunk_index;
  uint32_t render_chunk_index;
  uint32_t waiting_chunk_index;
  uint32_t asset_chunk_index;
  uint32_t sprite_chunk_index;
  uint32_t animation_chunk_index;
  uint32_t audio_asset_chunk_index;
  uint32_t audio_bank_chunk_index;
  uint32_t audio_cue_chunk_index;
  uint32_t sprite_frame_count;
  uint32_t audio_asset_count;
  uint32_t audio_cue_count;
  uint32_t audio_adpcm_bytes;
  uint32_t state_count;
  uint32_t input_count;
  uint32_t route_count;
  uint32_t transition_count;
  uint32_t render_model_count;
  uint32_t render_element_count;
  uint32_t waiting_visual_count;
  uint32_t waiting_element_count;
  uint32_t interaction_mode;
  uint32_t inactive_route;
  uint32_t meaningful_input_mask;
} ps_egg_state_loader_probe_t;

extern volatile ps_egg_state_loader_probe_t g_ps_egg_state_loader_probe;

/* The package blob must remain immutable while the decoded scene is active. */
uint32_t PS_EggStateLoader_Load(
  const uint8_t *blob,
  uint32_t size,
  ps_scene_runtime_state_scene_t *scene);
uint32_t PS_EggStateLoader_LoadScene(
  uint32_t scene_id,
  ps_scene_runtime_state_scene_t *scene);
uint32_t PS_EggStateLoader_SceneCount(void);
uint32_t PS_EggStateLoader_EntrySceneId(void);
uint32_t PS_EggStateLoader_GetSpriteFrame(
  uint32_t frame_id,
  ps_egg_state_loader_sprite_frame_t *frame);
uint32_t PS_EggStateLoader_GetAudioCue(
  uint32_t cue_index,
  ps_egg_state_loader_audio_cue_t *cue);

#ifdef __cplusplus
}
#endif

#endif /* PS_EGG_STATE_LOADER_H */
