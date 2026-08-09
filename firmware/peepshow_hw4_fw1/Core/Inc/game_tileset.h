#ifndef GAME_TILESET_H
#define GAME_TILESET_H

#include <stdint.h>

#include "tx_api.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GAME_TILESET_BLOB_MAGIC      (0x54455354UL) /* "TSET" */
#define GAME_TILESET_BLOB_VERSION    (1U)
#define GAME_TILESET_BLOB_MAX_BYTES  (65536U)

typedef struct __attribute__((packed))
{
  uint32_t magic;
  uint16_t version;
  uint16_t header_size;
  uint32_t total_size;
  uint32_t crc32;
  uint16_t tile_width;
  uint16_t tile_height;
  uint16_t reserved0;
  uint16_t reserved1;
  uint32_t tile_count;
  uint32_t base_gid;
  uint32_t color_stride;
  uint32_t mask_stride;
  uint32_t color_offset;
  uint32_t mask_offset;
} game_tileset_blob_header_t;

typedef struct
{
  const game_tileset_blob_header_t *header;
  const uint8_t *color_plane;
  const uint8_t *mask_plane;
  uint32_t tile_count;
  uint32_t tile_color_bytes;
  uint32_t tile_mask_bytes;
} game_tileset_view_t;

UINT GameTileset_Parse(const void *blob_data, uint32_t blob_size, game_tileset_view_t *view_out);
uint8_t GameTileset_TryGetTileByGid(const game_tileset_view_t *view,
                                    uint16_t gid,
                                    const uint8_t **color_ptr_out,
                                    const uint8_t **mask_ptr_out);

#ifdef __cplusplus
}
#endif

#endif /* GAME_TILESET_H */
