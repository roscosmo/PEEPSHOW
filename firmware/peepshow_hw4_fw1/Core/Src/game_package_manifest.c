#include "game_package_manifest.h"

#define APP_U32_SIZE (4U)

static uint32_t GamePackageManifest_Crc32Masked(const uint8_t *data, uint32_t size, uint32_t crc_offset)
{
  uint32_t crc = 0xFFFFFFFFUL;
  uint32_t i;
  uint32_t j;

  for (i = 0UL; i < size; i++)
  {
    uint8_t byte = data[i];

    if ((i >= crc_offset) && (i < (crc_offset + APP_U32_SIZE)))
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

static uint8_t GamePackageManifest_BoundsOk(uint32_t offset, uint32_t count, uint32_t elem_size, uint32_t total_size)
{
  uint32_t bytes;

  if (elem_size == 0UL)
  {
    return 0U;
  }
  if (count == 0UL)
  {
    return (offset <= total_size) ? 1U : 0U;
  }

  if (offset > total_size)
  {
    return 0U;
  }

  bytes = count * elem_size;
  if ((count != 0UL) && ((bytes / count) != elem_size))
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

UINT GamePackageManifest_Parse(const void *manifest_data,
                               uint32_t manifest_size,
                               game_package_manifest_view_t *view_out)
{
  const uint8_t *bytes = (const uint8_t *)manifest_data;
  const game_package_manifest_header_t *hdr_base;
  const game_package_manifest_header_t *hdr_v12 = (const game_package_manifest_header_t *)0;
  const game_package_manifest_header_v3_t *hdr_v3 = (const game_package_manifest_header_v3_t *)0;
  uint16_t manifest_version;
  uint16_t header_size;
  uint32_t total_size;
  uint32_t mode_count;
  uint32_t pet_route_count;
  uint32_t modes_offset;
  uint32_t pet_routes_offset;
  uint32_t pet_menu_item_count = 0UL;
  uint32_t pet_menu_items_offset = 0UL;
  uint32_t mode_record_size;
  uint32_t crc_actual;
  uint32_t crc_offset = (uint32_t)offsetof(game_package_manifest_header_t, crc32);

  if ((manifest_data == (const void *)0) || (view_out == (game_package_manifest_view_t *)0))
  {
    return TX_PTR_ERROR;
  }

  if ((manifest_size < sizeof(game_package_manifest_header_t)) ||
      (manifest_size > GAME_PACKAGE_MANIFEST_MAX_BYTES))
  {
    return TX_SIZE_ERROR;
  }

  hdr_base = (const game_package_manifest_header_t *)manifest_data;
  manifest_version = hdr_base->version;
  if (hdr_base->magic != GAME_PACKAGE_MANIFEST_MAGIC)
  {
    return TX_NOT_DONE;
  }

  if ((manifest_version != GAME_PACKAGE_MANIFEST_VERSION_V1) &&
      (manifest_version != GAME_PACKAGE_MANIFEST_VERSION_V2) &&
      (manifest_version != GAME_PACKAGE_MANIFEST_VERSION_V3) &&
      (manifest_version != GAME_PACKAGE_MANIFEST_VERSION_V4) &&
      (manifest_version != GAME_PACKAGE_MANIFEST_VERSION_V5))
  {
    return TX_NOT_DONE;
  }

  if ((manifest_version == GAME_PACKAGE_MANIFEST_VERSION_V1) ||
      (manifest_version == GAME_PACKAGE_MANIFEST_VERSION_V2))
  {
    hdr_v12 = (const game_package_manifest_header_t *)manifest_data;
    header_size = hdr_v12->header_size;
    total_size = hdr_v12->total_size;
    mode_count = (uint32_t)hdr_v12->mode_count;
    pet_route_count = (uint32_t)hdr_v12->pet_route_count;
    modes_offset = hdr_v12->modes_offset;
    pet_routes_offset = hdr_v12->pet_routes_offset;
  }
  else
  {
    if (manifest_size < sizeof(game_package_manifest_header_v3_t))
    {
      return TX_SIZE_ERROR;
    }
    hdr_v3 = (const game_package_manifest_header_v3_t *)manifest_data;
    header_size = hdr_v3->header_size;
    total_size = hdr_v3->total_size;
    mode_count = (uint32_t)hdr_v3->mode_count;
    pet_route_count = (uint32_t)hdr_v3->pet_route_count;
    modes_offset = hdr_v3->modes_offset;
    pet_routes_offset = hdr_v3->pet_routes_offset;
    pet_menu_item_count = (uint32_t)hdr_v3->pet_menu_item_count;
    pet_menu_items_offset = hdr_v3->pet_menu_items_offset;
  }

  if ((manifest_version == GAME_PACKAGE_MANIFEST_VERSION_V1) &&
      (header_size != (uint16_t)sizeof(game_package_manifest_header_t)))
  {
    return TX_NOT_DONE;
  }
  if ((manifest_version == GAME_PACKAGE_MANIFEST_VERSION_V2) &&
      (header_size != (uint16_t)sizeof(game_package_manifest_header_t)))
  {
    return TX_NOT_DONE;
  }
  if (((manifest_version == GAME_PACKAGE_MANIFEST_VERSION_V3) ||
       (manifest_version == GAME_PACKAGE_MANIFEST_VERSION_V4) ||
       (manifest_version == GAME_PACKAGE_MANIFEST_VERSION_V5)) &&
      (header_size != (uint16_t)sizeof(game_package_manifest_header_v3_t)))
  {
    return TX_NOT_DONE;
  }

  if (manifest_version == GAME_PACKAGE_MANIFEST_VERSION_V1)
  {
    mode_record_size = (uint32_t)sizeof(game_package_manifest_mode_t);
  }
  else if ((manifest_version == GAME_PACKAGE_MANIFEST_VERSION_V2) ||
           (manifest_version == GAME_PACKAGE_MANIFEST_VERSION_V3))
  {
    mode_record_size = (uint32_t)sizeof(game_package_manifest_mode_v2_t);
  }
  else if (manifest_version == GAME_PACKAGE_MANIFEST_VERSION_V4)
  {
    mode_record_size = (uint32_t)sizeof(game_package_manifest_mode_v4_t);
  }
  else
  {
    mode_record_size = (uint32_t)sizeof(game_package_manifest_mode_v5_t);
  }

  if ((total_size < (uint32_t)header_size) ||
      (total_size > manifest_size) ||
      (total_size > GAME_PACKAGE_MANIFEST_MAX_BYTES))
  {
    return TX_SIZE_ERROR;
  }

  if ((mode_count > GAME_PACKAGE_MANIFEST_MAX_MODE_COUNT) ||
      (pet_route_count > GAME_PACKAGE_MANIFEST_MAX_PET_ROUTE_COUNT))
  {
    return TX_SIZE_ERROR;
  }

  if ((pet_menu_item_count > GAME_PACKAGE_MANIFEST_MAX_PET_MENU_ITEM_COUNT) &&
      ((manifest_version == GAME_PACKAGE_MANIFEST_VERSION_V3) ||
       (manifest_version == GAME_PACKAGE_MANIFEST_VERSION_V4) ||
       (manifest_version == GAME_PACKAGE_MANIFEST_VERSION_V5)))
  {
    return TX_SIZE_ERROR;
  }

  if ((modes_offset & 0x3UL) != 0UL)
  {
    return TX_NOT_DONE;
  }
  if ((pet_routes_offset & 0x3UL) != 0UL)
  {
    return TX_NOT_DONE;
  }
  if (((manifest_version == GAME_PACKAGE_MANIFEST_VERSION_V3) ||
       (manifest_version == GAME_PACKAGE_MANIFEST_VERSION_V4) ||
       (manifest_version == GAME_PACKAGE_MANIFEST_VERSION_V5)) &&
      ((pet_menu_items_offset & 0x3UL) != 0UL))
  {
    return TX_NOT_DONE;
  }

  if (GamePackageManifest_BoundsOk(modes_offset,
                                   mode_count,
                                   mode_record_size,
                                   total_size) == 0U)
  {
    return TX_SIZE_ERROR;
  }
  if (GamePackageManifest_BoundsOk(pet_routes_offset,
                                   pet_route_count,
                                   (uint32_t)sizeof(game_package_manifest_pet_route_t),
                                   total_size) == 0U)
  {
    return TX_SIZE_ERROR;
  }
  if (((manifest_version == GAME_PACKAGE_MANIFEST_VERSION_V3) ||
       (manifest_version == GAME_PACKAGE_MANIFEST_VERSION_V4) ||
       (manifest_version == GAME_PACKAGE_MANIFEST_VERSION_V5)) &&
      (GamePackageManifest_BoundsOk(pet_menu_items_offset,
                                    pet_menu_item_count,
                                    (uint32_t)sizeof(game_package_manifest_pet_menu_item_t),
                                    total_size) == 0U))
  {
    return TX_SIZE_ERROR;
  }

  crc_actual = GamePackageManifest_Crc32Masked(bytes, total_size, crc_offset);
  if (crc_actual != hdr_base->crc32)
  {
    return TX_NOT_DONE;
  }

  view_out->header = hdr_v12;
  view_out->header_v3 = hdr_v3;
  view_out->modes_blob = (const uint8_t *)(bytes + modes_offset);
  view_out->pet_routes = (const game_package_manifest_pet_route_t *)(bytes + pet_routes_offset);
  if (((manifest_version == GAME_PACKAGE_MANIFEST_VERSION_V3) ||
       (manifest_version == GAME_PACKAGE_MANIFEST_VERSION_V4) ||
       (manifest_version == GAME_PACKAGE_MANIFEST_VERSION_V5)) &&
      (pet_menu_item_count > 0UL))
  {
    view_out->pet_menu_items = (const game_package_manifest_pet_menu_item_t *)(bytes + pet_menu_items_offset);
  }
  else
  {
    view_out->pet_menu_items = (const game_package_manifest_pet_menu_item_t *)0;
  }
  view_out->mode_count = mode_count;
  view_out->pet_route_count = pet_route_count;
  view_out->pet_menu_item_count = pet_menu_item_count;
  view_out->manifest_version = manifest_version;
  view_out->mode_record_size = (uint16_t)mode_record_size;
  return TX_SUCCESS;
}
