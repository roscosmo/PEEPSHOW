#ifndef GAME_PACKAGE_MANIFEST_H
#define GAME_PACKAGE_MANIFEST_H

#include <stdint.h>
#include <stddef.h>

#include "tx_api.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GAME_PACKAGE_MANIFEST_MAGIC                 (0x4B50474DU) /* "MGPK" */
#define GAME_PACKAGE_MANIFEST_VERSION_V1            (1U)
#define GAME_PACKAGE_MANIFEST_VERSION_V2            (2U)
#define GAME_PACKAGE_MANIFEST_VERSION_V3            (3U)
#define GAME_PACKAGE_MANIFEST_VERSION_V4            (4U)
#define GAME_PACKAGE_MANIFEST_VERSION_V5            (5U)
#define GAME_PACKAGE_MANIFEST_VERSION               (GAME_PACKAGE_MANIFEST_VERSION_V5)
#define GAME_PACKAGE_MANIFEST_MAX_BYTES             (4096U)
#define GAME_PACKAGE_MANIFEST_MAX_MODE_COUNT        (16U)
#define GAME_PACKAGE_MANIFEST_MAX_PET_ROUTE_COUNT   (16U)
#define GAME_PACKAGE_MANIFEST_MAX_PET_MENU_ITEM_COUNT (32U)

typedef struct __attribute__((packed))
{
  uint32_t magic;
  uint16_t version;
  uint16_t header_size;
  uint32_t total_size;
  uint32_t crc32;
  uint32_t package_id;
  uint32_t package_version;
  uint16_t mode_count;
  uint16_t pet_route_count;
  uint32_t modes_offset;
  uint32_t pet_routes_offset;
} game_package_manifest_header_t;

typedef struct __attribute__((packed))
{
  uint32_t magic;
  uint16_t version;
  uint16_t header_size;
  uint32_t total_size;
  uint32_t crc32;
  uint32_t package_id;
  uint32_t package_version;
  uint16_t mode_count;
  uint16_t pet_route_count;
  uint32_t modes_offset;
  uint32_t pet_routes_offset;
  uint16_t pet_menu_item_count;
  uint16_t reserved1;
  uint32_t pet_menu_items_offset;
} game_package_manifest_header_v3_t;

typedef struct __attribute__((packed))
{
  uint32_t mode_id;
  uint16_t runtime_kind;
  uint16_t backend_id;
  uint32_t reserved0;
} game_package_manifest_mode_t;

typedef struct __attribute__((packed))
{
  uint32_t mode_id;
  uint16_t runtime_kind;
  uint16_t backend_id;
  uint32_t scene_map_addr;
  uint32_t scene_map_size_bytes;
  uint32_t scene_tileset_addr;
  uint32_t scene_tileset_size_bytes;
  uint32_t topdown_render_scale;
  uint32_t topdown_tile_present_mode;
  uint32_t controller_profile_id;
  uint32_t camera_profile_id;
  uint32_t input_deadzone_permille;
  uint32_t input_flags;
  uint32_t move_speed_px_s;
  uint32_t move_accel_px_s2;
  uint32_t move_decel_px_s2;
  uint32_t camera_deadzone_w_px;
  uint32_t camera_deadzone_h_px;
  uint32_t camera_follow_permille;
  uint32_t camera_max_speed_px_s;
  int32_t camera_lookahead_x_px;
  int32_t camera_lookahead_y_px;
} game_package_manifest_mode_v2_t;

typedef struct __attribute__((packed))
{
  uint32_t mode_id;
  uint16_t runtime_kind;
  uint16_t backend_id;
  uint32_t scene_map_addr;
  uint32_t scene_map_size_bytes;
  uint32_t scene_tileset_addr;
  uint32_t scene_tileset_size_bytes;
  uint32_t topdown_render_scale;
  uint32_t topdown_tile_present_mode;
  uint32_t controller_profile_id;
  uint32_t camera_profile_id;
  uint32_t input_deadzone_permille;
  uint32_t input_flags;
  uint32_t move_speed_px_s;
  uint32_t move_accel_px_s2;
  uint32_t move_decel_px_s2;
  uint32_t camera_deadzone_w_px;
  uint32_t camera_deadzone_h_px;
  uint32_t camera_follow_permille;
  uint32_t camera_max_speed_px_s;
  int32_t camera_lookahead_x_px;
  int32_t camera_lookahead_y_px;
  uint32_t scene_map_id;
  uint32_t scene_tileset_id;
  uint32_t music_asset_id;
  uint32_t sfx_interact_asset_id;
  uint32_t sfx_confirm_asset_id;
  uint32_t sfx_error_asset_id;
} game_package_manifest_mode_v4_t;

typedef struct __attribute__((packed))
{
  uint32_t mode_id;
  uint16_t runtime_kind;
  uint16_t backend_id;
  uint32_t scene_map_addr;
  uint32_t scene_map_size_bytes;
  uint32_t scene_tileset_addr;
  uint32_t scene_tileset_size_bytes;
  uint32_t topdown_render_scale;
  uint32_t topdown_tile_present_mode;
  uint32_t controller_profile_id;
  uint32_t camera_profile_id;
  uint32_t input_deadzone_permille;
  uint32_t input_flags;
  uint32_t move_speed_px_s;
  uint32_t move_accel_px_s2;
  uint32_t move_decel_px_s2;
  uint32_t camera_deadzone_w_px;
  uint32_t camera_deadzone_h_px;
  uint32_t camera_follow_permille;
  uint32_t camera_max_speed_px_s;
  int32_t camera_lookahead_x_px;
  int32_t camera_lookahead_y_px;
  uint32_t scene_map_id;
  uint32_t scene_tileset_id;
  uint32_t music_asset_id;
  uint32_t sfx_interact_asset_id;
  uint32_t sfx_confirm_asset_id;
  uint32_t sfx_error_asset_id;
  uint32_t scene_lifecycle;
  uint32_t resume_domain_id;
} game_package_manifest_mode_v5_t;

typedef struct __attribute__((packed))
{
  uint16_t pet_entry_id;
  uint16_t reserved0;
  uint32_t mode_id;
} game_package_manifest_pet_route_t;

typedef struct __attribute__((packed))
{
  uint8_t slot_index;
  uint8_t icon_action_id;
  uint8_t select_kind;
  uint8_t status_kind;
  uint16_t arg0;
  uint16_t status_source_id;
} game_package_manifest_pet_menu_item_t;

typedef struct
{
  const game_package_manifest_header_t *header;
  const game_package_manifest_header_v3_t *header_v3;
  const uint8_t *modes_blob;
  const game_package_manifest_pet_route_t *pet_routes;
  const game_package_manifest_pet_menu_item_t *pet_menu_items;
  uint32_t mode_count;
  uint32_t pet_route_count;
  uint32_t pet_menu_item_count;
  uint16_t manifest_version;
  uint16_t mode_record_size;
} game_package_manifest_view_t;

UINT GamePackageManifest_Parse(const void *manifest_data,
                               uint32_t manifest_size,
                               game_package_manifest_view_t *view_out);

#ifdef __cplusplus
}
#endif

#endif /* GAME_PACKAGE_MANIFEST_H */
