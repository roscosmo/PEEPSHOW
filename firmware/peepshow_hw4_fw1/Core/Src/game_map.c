#include "game_map.h"

#include <stddef.h>

static uint32_t GameMap_Crc32MaskedHeaderCrc(const uint8_t *data, uint32_t size)
{
  uint32_t crc = 0xFFFFFFFFUL;
  uint32_t i;
  uint32_t j;
  const uint32_t crc_offset = (uint32_t)offsetof(game_map_blob_header_t, crc32);

  for (i = 0UL; i < size; i++)
  {
    uint8_t byte = data[i];
    if ((i >= crc_offset) && (i < (crc_offset + 4UL)))
    {
      byte = 0U;
    }
    crc ^= (uint32_t)byte;
    for (j = 0UL; j < 8UL; j++)
    {
      if ((crc & 1UL) != 0UL)
      {
        crc = (crc >> 1U) ^ 0xEDB88320UL;
      }
      else
      {
        crc >>= 1U;
      }
    }
  }

  return ~crc;
}

static uint8_t GameMap_BoundsOk(uint32_t offset, uint32_t bytes, uint32_t total_size)
{
  if (offset > total_size)
  {
    return 0U;
  }
  if ((offset + bytes) < offset)
  {
    return 0U;
  }
  if ((offset + bytes) > total_size)
  {
    return 0U;
  }
  return 1U;
}

UINT GameMap_Parse(const void *blob_data, uint32_t blob_size, game_map_view_t *view_out)
{
  const uint8_t *bytes = (const uint8_t *)blob_data;
  const game_map_blob_header_t *hdr;
  uint16_t tile_layer_count;
  uint32_t tile_flags_bytes;
  uint64_t tile_gids_words64;
  uint32_t tile_gids_bytes;
  uint32_t object_bytes;
  uint32_t crc_actual;

  if ((blob_data == (const void *)0) || (view_out == (game_map_view_t *)0))
  {
    return TX_PTR_ERROR;
  }

  if ((blob_size < (uint32_t)sizeof(game_map_blob_header_t)) ||
      (blob_size > GAME_MAP_BLOB_MAX_BYTES))
  {
    return TX_SIZE_ERROR;
  }

  hdr = (const game_map_blob_header_t *)blob_data;
  if ((hdr->magic != GAME_MAP_BLOB_MAGIC) ||
      ((hdr->version != GAME_MAP_BLOB_VERSION_V1) &&
       (hdr->version != GAME_MAP_BLOB_VERSION_V2)) ||
      (hdr->header_size != (uint16_t)sizeof(game_map_blob_header_t)))
  {
    return TX_NOT_DONE;
  }

  if ((hdr->total_size < (uint32_t)sizeof(game_map_blob_header_t)) ||
      (hdr->total_size > blob_size) ||
      (hdr->total_size > GAME_MAP_BLOB_MAX_BYTES))
  {
    return TX_SIZE_ERROR;
  }

  if ((hdr->map_width == 0U) || (hdr->map_height == 0U) ||
      (hdr->tile_width == 0U) || (hdr->tile_height == 0U))
  {
    return TX_SIZE_ERROR;
  }

  if (hdr->tile_count != ((uint32_t)hdr->map_width * (uint32_t)hdr->map_height))
  {
    return TX_SIZE_ERROR;
  }

  tile_layer_count = 1U;
  if (hdr->version == GAME_MAP_BLOB_VERSION_V2)
  {
    tile_layer_count = hdr->reserved0;
    if (tile_layer_count == 0U)
    {
      return TX_SIZE_ERROR;
    }
  }

  if (hdr->object_stride != (uint16_t)sizeof(game_map_object_t))
  {
    return TX_SIZE_ERROR;
  }

  if (((hdr->tile_flags_offset & 0x3UL) != 0UL) ||
      ((hdr->tile_gids_offset & 0x3UL) != 0UL) ||
      ((hdr->objects_offset & 0x3UL) != 0UL))
  {
    return TX_SIZE_ERROR;
  }

  tile_flags_bytes = hdr->tile_count;
  tile_gids_words64 = (uint64_t)hdr->tile_count * (uint64_t)tile_layer_count;
  if (tile_gids_words64 > ((uint64_t)UINT32_MAX / (uint64_t)sizeof(uint16_t)))
  {
    return TX_SIZE_ERROR;
  }
  tile_gids_bytes = (uint32_t)(tile_gids_words64 * (uint64_t)sizeof(uint16_t));

  object_bytes = hdr->object_count * (uint32_t)hdr->object_stride;
  if ((hdr->object_count != 0UL) && ((object_bytes / hdr->object_count) != (uint32_t)hdr->object_stride))
  {
    return TX_SIZE_ERROR;
  }

  if (GameMap_BoundsOk(hdr->tile_flags_offset, tile_flags_bytes, hdr->total_size) == 0U)
  {
    return TX_SIZE_ERROR;
  }
  if (GameMap_BoundsOk(hdr->tile_gids_offset, tile_gids_bytes, hdr->total_size) == 0U)
  {
    return TX_SIZE_ERROR;
  }
  if (GameMap_BoundsOk(hdr->objects_offset, object_bytes, hdr->total_size) == 0U)
  {
    return TX_SIZE_ERROR;
  }

  crc_actual = GameMap_Crc32MaskedHeaderCrc(bytes, hdr->total_size);
  if (crc_actual != hdr->crc32)
  {
    return TX_NOT_DONE;
  }

  view_out->header = hdr;
  view_out->tile_flags = bytes + hdr->tile_flags_offset;
  view_out->tile_gids_layers = (const uint16_t *)(bytes + hdr->tile_gids_offset);
  view_out->objects = (const game_map_object_t *)(bytes + hdr->objects_offset);
  view_out->tile_count = hdr->tile_count;
  view_out->object_count = hdr->object_count;
  view_out->tile_layer_count = tile_layer_count;
  view_out->tile_layer_stride = hdr->tile_count;

  return TX_SUCCESS;
}

uint32_t GameMap_TileIndex(const game_map_view_t *view, uint16_t x, uint16_t y)
{
  uint32_t width;

  if ((view == (const game_map_view_t *)0) || (view->header == (const game_map_blob_header_t *)0))
  {
    return 0xFFFFFFFFUL;
  }

  width = (uint32_t)view->header->map_width;
  if ((x >= view->header->map_width) || (y >= view->header->map_height))
  {
    return 0xFFFFFFFFUL;
  }

  return ((uint32_t)y * width) + (uint32_t)x;
}

uint8_t GameMap_GetTileFlags(const game_map_view_t *view, uint16_t x, uint16_t y)
{
  uint32_t idx = GameMap_TileIndex(view, x, y);

  if ((idx == 0xFFFFFFFFUL) || (view->tile_flags == (const uint8_t *)0))
  {
    return 0U;
  }
  return view->tile_flags[idx];
}

uint16_t GameMap_GetTileLayerCount(const game_map_view_t *view)
{
  if ((view == (const game_map_view_t *)0) ||
      (view->tile_layer_count == 0U))
  {
    return 1U;
  }
  return view->tile_layer_count;
}

uint16_t GameMap_GetTileGidAtLayer(const game_map_view_t *view, uint16_t x, uint16_t y, uint16_t layer_index)
{
  uint32_t idx = GameMap_TileIndex(view, x, y);
  uint32_t offset;

  if ((idx == 0xFFFFFFFFUL) || (view->tile_gids_layers == (const uint16_t *)0))
  {
    return 0U;
  }
  if (layer_index >= GameMap_GetTileLayerCount(view))
  {
    return 0U;
  }

  offset = ((uint32_t)layer_index * view->tile_layer_stride) + idx;
  return view->tile_gids_layers[offset];
}

uint16_t GameMap_GetTileGid(const game_map_view_t *view, uint16_t x, uint16_t y)
{
  uint16_t layer_count;
  int32_t layer;

  layer_count = GameMap_GetTileLayerCount(view);
  for (layer = (int32_t)layer_count - 1; layer >= 0; layer--)
  {
    uint16_t gid = GameMap_GetTileGidAtLayer(view, x, y, (uint16_t)layer);
    if (gid != 0U)
    {
      return gid;
    }
  }
  return 0U;
}
