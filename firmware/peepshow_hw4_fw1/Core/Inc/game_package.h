#ifndef GAME_PACKAGE_H
#define GAME_PACKAGE_H

#include <stdint.h>

#include "tx_api.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
  GAME_PACKAGE_RT_KIND_TOPDOWN = 1U,
  GAME_PACKAGE_RT_KIND_SIDESCROLL = 2U
} game_package_runtime_kind_t;

typedef enum
{
  GAME_PACKAGE_TOPDOWN_PRESENT_AUTO = 0U,
  GAME_PACKAGE_TOPDOWN_PRESENT_BINARY = 1U,
  GAME_PACKAGE_TOPDOWN_PRESENT_BAYER = 2U
} game_package_topdown_present_mode_t;

typedef enum
{
  GAME_PACKAGE_CONTROLLER_PROFILE_TOPDOWN_DIGITAL_8DIR = 1U,
  GAME_PACKAGE_CONTROLLER_PROFILE_TOPDOWN_ANALOG = 2U,
  GAME_PACKAGE_CONTROLLER_PROFILE_SIDESCROLL_PLATFORMER = 3U,
  GAME_PACKAGE_CONTROLLER_PROFILE_MINIGAME_CURSOR = 4U
} game_package_controller_profile_t;

typedef enum
{
  GAME_PACKAGE_CAMERA_PROFILE_LOCKED = 1U,
  GAME_PACKAGE_CAMERA_PROFILE_FOLLOW_X = 2U,
  GAME_PACKAGE_CAMERA_PROFILE_FOLLOW_XY = 3U,
  GAME_PACKAGE_CAMERA_PROFILE_FOLLOW_DEADZONE = 4U
} game_package_camera_profile_t;

typedef enum
{
  GAME_PACKAGE_INPUT_FLAG_NORMALIZE_DIAGONAL = (1UL << 0),
  GAME_PACKAGE_INPUT_FLAG_ANALOG_PREFERRED = (1UL << 1)
} game_package_input_flag_t;

typedef enum
{
  GAME_PACKAGE_SCENE_LIFECYCLE_LEGACY = 0U,
  GAME_PACKAGE_SCENE_LIFECYCLE_RESUMABLE = 1U,
  GAME_PACKAGE_SCENE_LIFECYCLE_TRANSIENT = 2U
} game_package_scene_lifecycle_t;

typedef enum
{
  GAME_RUNTIME_BACKEND_RENDER_DEMO = 1U,
  GAME_RUNTIME_BACKEND_RENDER_DEMO_3D_WALK = 2U,
  GAME_RUNTIME_BACKEND_TOPDOWN_BASIC = 3U,
  GAME_RUNTIME_BACKEND_RENDER_DEMO_TITLE_ANIM = 4U
} game_runtime_backend_id_t;

typedef enum
{
  GAME_PET_ENTRY_START_GAME = 1U,
  GAME_PET_ENTRY_SLOT6 = 2U,
  GAME_PET_ENTRY_SLOT7 = 3U,
  GAME_PET_ENTRY_SLOT8 = 4U
} game_pet_entry_id_t;

typedef enum
{
  GAME_PET_MENU_ACTION_FEED = 0U,
  GAME_PET_MENU_ACTION_PLAY = 1U,
  GAME_PET_MENU_ACTION_SLOT3 = 2U,
  GAME_PET_MENU_ACTION_SLOT4 = 3U,
  GAME_PET_MENU_ACTION_SLOT5 = 4U,
  GAME_PET_MENU_ACTION_SLOT6 = 5U,
  GAME_PET_MENU_ACTION_SLOT7 = 6U,
  GAME_PET_MENU_ACTION_SLOT8 = 7U,
  GAME_PET_MENU_ACTION_START_GAME = 8U,
  GAME_PET_MENU_ACTION_OPTIONS = 9U,
  GAME_PET_MENU_ACTION_COUNT = 10U
} game_pet_menu_action_id_t;

typedef enum
{
  GAME_PET_MENU_SELECT_NONE = 0U,
  GAME_PET_MENU_SELECT_FEED = 1U,
  GAME_PET_MENU_SELECT_PLAY = 2U,
  GAME_PET_MENU_SELECT_START_GAME = 3U,
  GAME_PET_MENU_SELECT_OPTIONS = 4U,
  GAME_PET_MENU_SELECT_LAUNCH_MODE = 5U,
  GAME_PET_MENU_SELECT_OPEN_PAGE = 6U,
  GAME_PET_MENU_SELECT_SAND_FX = 7U
} game_pet_menu_select_kind_t;

typedef enum
{
  GAME_PET_MENU_STATUS_NONE = 0U,
  GAME_PET_MENU_STATUS_BOOL = 1U,
  GAME_PET_MENU_STATUS_LEVEL4 = 2U
} game_pet_menu_status_kind_t;

typedef enum
{
  GAME_PET_MENU_STATUS_SOURCE_NONE = 0U,
  GAME_PET_MENU_STATUS_SOURCE_BATTERY = 1U
} game_pet_menu_status_source_t;

#define GAME_PET_MENU_SLOT_COUNT ((uint32_t)10U)

typedef struct
{
  uint8_t slot_index;
  uint8_t icon_action_id;
  uint8_t select_kind;
  uint8_t status_kind;
  uint16_t arg0;
  uint16_t status_source_id;
} game_package_pet_menu_item_t;

typedef struct
{
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
} game_package_runtime_config_t;

typedef struct
{
  uint32_t mode_id;
  uint32_t runtime_kind;
  uint32_t backend_id;
  const char *name;
  game_package_runtime_config_t runtime_config;
} game_package_mode_desc_t;

typedef struct
{
  uint32_t pet_entry_id;
  uint32_t mode_id;
} game_package_pet_route_t;

typedef struct
{
  uint32_t package_id;
  uint32_t package_version;
  const game_package_mode_desc_t *modes;
  uint32_t mode_count;
  const game_package_pet_route_t *pet_routes;
  uint32_t pet_route_count;
  const game_package_pet_menu_item_t *pet_menu_items;
  uint32_t pet_menu_item_count;
} game_package_desc_t;

const game_package_desc_t *GamePackage_GetActive(void);
const game_package_mode_desc_t *GamePackage_FindModeById(uint32_t mode_id);
const game_package_runtime_config_t *GamePackage_GetRuntimeConfigByModeId(uint32_t mode_id);
UINT GamePackage_RequestRuntimeModeById(uint32_t mode_id);
UINT GamePackage_RequestRuntimeModeByPetEntry(uint32_t pet_entry_id);
uint32_t GamePackage_GetPrimaryResumeModeId(void);
uint32_t GamePackage_ConsumeRequestedRuntimeModeId(void);
const game_package_pet_menu_item_t *GamePackage_GetPetMenuItemBySlot(uint32_t slot_index);
UINT GamePackage_LoadManifestBlob(const void *manifest_data, uint32_t manifest_size);
void GamePackage_ClearLoadedManifest(void);

#ifdef __cplusplus
}
#endif

#endif /* GAME_PACKAGE_H */
