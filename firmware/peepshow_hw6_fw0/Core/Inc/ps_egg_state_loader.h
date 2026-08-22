#ifndef PS_EGG_STATE_LOADER_H
#define PS_EGG_STATE_LOADER_H

#include <stdint.h>

#include "ps_scene_runtime.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PS_EGG_STATE_LOADER_API_VERSION (2UL)
#define PS_EGG_STATE_LOADER_STATUS_NOT_RUN (0xFFFFFFFFUL)

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
  PS_EGG_STATE_LOADER_REASON_HASH
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
  uint32_t graph_chunk_index;
  uint32_t render_chunk_index;
  uint32_t waiting_chunk_index;
  uint32_t state_count;
  uint32_t input_count;
  uint32_t route_count;
  uint32_t transition_count;
  uint32_t render_model_count;
  uint32_t render_element_count;
  uint32_t waiting_visual_count;
  uint32_t waiting_element_count;
} ps_egg_state_loader_probe_t;

extern volatile ps_egg_state_loader_probe_t g_ps_egg_state_loader_probe;

uint32_t PS_EggStateLoader_Load(
  const uint8_t *blob,
  uint32_t size,
  ps_scene_runtime_state_scene_t *scene);
uint32_t PS_EggStateLoader_LoadEmbedded(
  ps_scene_runtime_state_scene_t *scene);

#ifdef __cplusplus
}
#endif

#endif /* PS_EGG_STATE_LOADER_H */
