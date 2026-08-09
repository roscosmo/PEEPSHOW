#ifndef GAME_MAP_H
#define GAME_MAP_H

#include <stdint.h>

#include "tx_api.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GAME_MAP_BLOB_MAGIC   (0x50414D54UL) /* "TMAP" */
#define GAME_MAP_BLOB_VERSION_V1 (1U)
#define GAME_MAP_BLOB_VERSION_V2 (2U)
#define GAME_MAP_BLOB_MAX_BYTES (32768U)

typedef enum
{
  GAME_MAP_TILE_FLAG_SOLID = (1U << 0),
  GAME_MAP_TILE_FLAG_WATER = (1U << 1),
  GAME_MAP_TILE_FLAG_SLOW = (1U << 2),
  GAME_MAP_TILE_FLAG_OCCLUDER = (1U << 3),
  GAME_MAP_TILE_FLAG_ROOF = (1U << 4),
  GAME_MAP_TILE_FLAG_EMISSIVE = (1U << 5)
} game_map_tile_flag_t;

typedef enum
{
  GAME_MAP_OBJECT_NONE = 0U,
  GAME_MAP_OBJECT_SPAWN = 1U,
  GAME_MAP_OBJECT_INTERACT = 2U,
  GAME_MAP_OBJECT_EXIT = 3U,
  GAME_MAP_OBJECT_CAMERA_ZONE = 4U,
  GAME_MAP_OBJECT_INDOOR_ZONE = 5U,
  GAME_MAP_OBJECT_LIGHT = 6U
} game_map_object_type_t;

typedef struct __attribute__((packed))
{
  uint32_t magic;
  uint16_t version;
  uint16_t header_size;
  uint32_t total_size;
  uint32_t crc32;
  uint16_t map_width;
  uint16_t map_height;
  uint8_t tile_width;
  uint8_t tile_height;
  uint16_t reserved0;
  uint32_t tile_count;
  uint32_t object_count;
  uint32_t tile_flags_offset;
  uint32_t tile_gids_offset;
  uint32_t objects_offset;
  uint16_t object_stride;
  uint16_t reserved1;
} game_map_blob_header_t;

typedef struct __attribute__((packed))
{
  uint16_t type;
  uint16_t flags;
  int16_t x_px;
  int16_t y_px;
  int16_t w_px;
  int16_t h_px;
  uint32_t arg0;
  uint32_t arg1;
} game_map_object_t;

typedef struct
{
  const game_map_blob_header_t *header;
  const uint8_t *tile_flags;
  const uint16_t *tile_gids_layers;
  const game_map_object_t *objects;
  uint32_t tile_count;
  uint32_t object_count;
  uint16_t tile_layer_count;
  uint32_t tile_layer_stride;
} game_map_view_t;

UINT GameMap_Parse(const void *blob_data, uint32_t blob_size, game_map_view_t *view_out);
uint32_t GameMap_TileIndex(const game_map_view_t *view, uint16_t x, uint16_t y);
uint8_t GameMap_GetTileFlags(const game_map_view_t *view, uint16_t x, uint16_t y);
uint16_t GameMap_GetTileLayerCount(const game_map_view_t *view);
uint16_t GameMap_GetTileGidAtLayer(const game_map_view_t *view, uint16_t x, uint16_t y, uint16_t layer_index);
uint16_t GameMap_GetTileGid(const game_map_view_t *view, uint16_t x, uint16_t y);

#ifdef __cplusplus
}
#endif

#endif /* GAME_MAP_H */
