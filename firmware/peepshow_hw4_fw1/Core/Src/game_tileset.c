#include "game_tileset.h"

#include <stddef.h>

static uint32_t GameTileset_Crc32MaskedHeaderCrc(const uint8_t *data, uint32_t size)
{
  uint32_t crc = 0xFFFFFFFFUL;
  uint32_t i;
  uint32_t j;
  const uint32_t crc_offset = (uint32_t)offsetof(game_tileset_blob_header_t, crc32);

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

static uint8_t GameTileset_BoundsOk(uint32_t offset, uint32_t bytes, uint32_t total_size)
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

UINT GameTileset_Parse(const void *blob_data, uint32_t blob_size, game_tileset_view_t *view_out)
{
  const uint8_t *bytes = (const uint8_t *)blob_data;
  const game_tileset_blob_header_t *hdr;
  uint32_t min_color_stride;
  uint32_t min_mask_stride;
  uint32_t tile_color_bytes;
  uint32_t tile_mask_bytes;
  uint32_t color_plane_bytes;
  uint32_t mask_plane_bytes;
  uint32_t crc_actual;

  if ((blob_data == (const void *)0) || (view_out == (game_tileset_view_t *)0))
  {
    return TX_PTR_ERROR;
  }

  if ((blob_size < (uint32_t)sizeof(game_tileset_blob_header_t)) ||
      (blob_size > GAME_TILESET_BLOB_MAX_BYTES))
  {
    return TX_SIZE_ERROR;
  }

  hdr = (const game_tileset_blob_header_t *)blob_data;
  if ((hdr->magic != GAME_TILESET_BLOB_MAGIC) ||
      (hdr->version != GAME_TILESET_BLOB_VERSION) ||
      (hdr->header_size != (uint16_t)sizeof(game_tileset_blob_header_t)))
  {
    return TX_NOT_DONE;
  }

  if ((hdr->total_size < (uint32_t)sizeof(game_tileset_blob_header_t)) ||
      (hdr->total_size > blob_size) ||
      (hdr->total_size > GAME_TILESET_BLOB_MAX_BYTES))
  {
    return TX_SIZE_ERROR;
  }

  if ((hdr->tile_width == 0U) || (hdr->tile_height == 0U) || (hdr->tile_count == 0UL))
  {
    return TX_SIZE_ERROR;
  }

  if (((hdr->color_offset & 0x3UL) != 0UL) || ((hdr->mask_offset & 0x3UL) != 0UL))
  {
    return TX_SIZE_ERROR;
  }

  min_color_stride = ((uint32_t)hdr->tile_width + 3UL) >> 2U;
  min_mask_stride = ((uint32_t)hdr->tile_width + 7UL) >> 3U;

  if ((hdr->color_stride < min_color_stride) ||
      ((hdr->mask_stride != 0UL) && (hdr->mask_stride < min_mask_stride)))
  {
    return TX_SIZE_ERROR;
  }

  if (((uint64_t)hdr->color_stride * (uint64_t)hdr->tile_height) > (uint64_t)UINT32_MAX)
  {
    return TX_SIZE_ERROR;
  }
  tile_color_bytes = hdr->color_stride * (uint32_t)hdr->tile_height;
  if (((uint64_t)tile_color_bytes * (uint64_t)hdr->tile_count) > (uint64_t)UINT32_MAX)
  {
    return TX_SIZE_ERROR;
  }
  color_plane_bytes = tile_color_bytes * hdr->tile_count;

  tile_mask_bytes = 0UL;
  mask_plane_bytes = 0UL;
  if (hdr->mask_stride != 0UL)
  {
    if (((uint64_t)hdr->mask_stride * (uint64_t)hdr->tile_height) > (uint64_t)UINT32_MAX)
    {
      return TX_SIZE_ERROR;
    }
    tile_mask_bytes = hdr->mask_stride * (uint32_t)hdr->tile_height;
    if (((uint64_t)tile_mask_bytes * (uint64_t)hdr->tile_count) > (uint64_t)UINT32_MAX)
    {
      return TX_SIZE_ERROR;
    }
    mask_plane_bytes = tile_mask_bytes * hdr->tile_count;
  }

  if (GameTileset_BoundsOk(hdr->color_offset, color_plane_bytes, hdr->total_size) == 0U)
  {
    return TX_SIZE_ERROR;
  }
  if (hdr->mask_stride != 0UL)
  {
    if (GameTileset_BoundsOk(hdr->mask_offset, mask_plane_bytes, hdr->total_size) == 0U)
    {
      return TX_SIZE_ERROR;
    }
  }

  crc_actual = GameTileset_Crc32MaskedHeaderCrc(bytes, hdr->total_size);
  if (crc_actual != hdr->crc32)
  {
    return TX_NOT_DONE;
  }

  view_out->header = hdr;
  view_out->color_plane = bytes + hdr->color_offset;
  view_out->mask_plane = (hdr->mask_stride != 0UL) ? (bytes + hdr->mask_offset) : (const uint8_t *)0;
  view_out->tile_count = hdr->tile_count;
  view_out->tile_color_bytes = tile_color_bytes;
  view_out->tile_mask_bytes = tile_mask_bytes;

  return TX_SUCCESS;
}

uint8_t GameTileset_TryGetTileByGid(const game_tileset_view_t *view,
                                    uint16_t gid,
                                    const uint8_t **color_ptr_out,
                                    const uint8_t **mask_ptr_out)
{
  uint32_t tile_index;

  if ((view == (const game_tileset_view_t *)0) ||
      (view->header == (const game_tileset_blob_header_t *)0) ||
      (view->color_plane == (const uint8_t *)0) ||
      (color_ptr_out == (const uint8_t **)0) ||
      (mask_ptr_out == (const uint8_t **)0))
  {
    return 0U;
  }

  if ((uint32_t)gid < view->header->base_gid)
  {
    return 0U;
  }
  tile_index = (uint32_t)gid - view->header->base_gid;
  if (tile_index >= view->tile_count)
  {
    return 0U;
  }

  *color_ptr_out = view->color_plane + (tile_index * view->tile_color_bytes);
  *mask_ptr_out = (view->mask_plane != (const uint8_t *)0)
                      ? (view->mask_plane + (tile_index * view->tile_mask_bytes))
                      : (const uint8_t *)0;
  return 1U;
}
