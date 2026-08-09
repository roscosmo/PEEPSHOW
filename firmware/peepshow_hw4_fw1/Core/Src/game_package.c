#include "game_package.h"

#include "game_package_manifest.h"
#include "knobs_autogen.h"

#define GAME_PACKAGE_DEFAULT_SCENE_MAP_ADDR       ((uint32_t)KNOB_STORAGE_INSTALLED_BASE_ADDR)
#define GAME_PACKAGE_DEFAULT_SCENE_MAP_SIZE_BYTES (0u)
#define GAME_PACKAGE_DEFAULT_TILESET_ADDR         ((uint32_t)KNOB_STORAGE_INSTALLED_BASE_ADDR + 0x1000u)
#define GAME_PACKAGE_DEFAULT_TILESET_SIZE_BYTES   (0u)
#define GAME_PACKAGE_DEFAULT_TOPDOWN_SCALE        (2u)
#define GAME_PACKAGE_DEFAULT_CONTROLLER_PROFILE   ((uint32_t)GAME_PACKAGE_CONTROLLER_PROFILE_TOPDOWN_ANALOG)
#define GAME_PACKAGE_DEFAULT_CAMERA_PROFILE       ((uint32_t)GAME_PACKAGE_CAMERA_PROFILE_FOLLOW_DEADZONE)
#define GAME_PACKAGE_DEFAULT_INPUT_DEADZONE_PM    (150u)
#define GAME_PACKAGE_DEFAULT_INPUT_FLAGS          ((uint32_t)(GAME_PACKAGE_INPUT_FLAG_NORMALIZE_DIAGONAL | \
                                                               GAME_PACKAGE_INPUT_FLAG_ANALOG_PREFERRED))
#define GAME_PACKAGE_DEFAULT_MOVE_SPEED_PX_S      (72u)
#define GAME_PACKAGE_DEFAULT_MOVE_ACCEL_PX_S2     (480u)
#define GAME_PACKAGE_DEFAULT_MOVE_DECEL_PX_S2     (640u)
#define GAME_PACKAGE_DEFAULT_CAMERA_DZ_W_PX       (24u)
#define GAME_PACKAGE_DEFAULT_CAMERA_DZ_H_PX       (24u)
#define GAME_PACKAGE_DEFAULT_CAMERA_FOLLOW_PM     (280u)
#define GAME_PACKAGE_DEFAULT_CAMERA_MAX_SPEED_PX_S (240u)
#define GAME_PACKAGE_DEFAULT_CAMERA_LOOKAHEAD_X   (16)
#define GAME_PACKAGE_DEFAULT_CAMERA_LOOKAHEAD_Y   (16)
#define GAME_PACKAGE_DEFAULT_SCENE_MAP_ID          (0u)
#define GAME_PACKAGE_DEFAULT_SCENE_TILESET_ID      (0u)
#define GAME_PACKAGE_DEFAULT_MUSIC_ASSET_ID        (0u)
#define GAME_PACKAGE_DEFAULT_SFX_INTERACT_ASSET_ID (0u)
#define GAME_PACKAGE_DEFAULT_SFX_CONFIRM_ASSET_ID  (0u)
#define GAME_PACKAGE_DEFAULT_SFX_ERROR_ASSET_ID    (0u)
#define GAME_PACKAGE_DEFAULT_SCENE_LIFECYCLE       ((uint32_t)GAME_PACKAGE_SCENE_LIFECYCLE_LEGACY)
#define GAME_PACKAGE_DEFAULT_RESUME_DOMAIN_ID      (0u)

static game_package_runtime_config_t GamePackage_RuntimeConfigForBackend(uint32_t backend_id)
{
  game_package_runtime_config_t cfg = {0};

  cfg.scene_lifecycle = GAME_PACKAGE_DEFAULT_SCENE_LIFECYCLE;
  cfg.resume_domain_id = GAME_PACKAGE_DEFAULT_RESUME_DOMAIN_ID;

  if (backend_id == (uint32_t)GAME_RUNTIME_BACKEND_TOPDOWN_BASIC)
  {
    cfg.scene_map_addr = GAME_PACKAGE_DEFAULT_SCENE_MAP_ADDR;
    cfg.scene_map_size_bytes = GAME_PACKAGE_DEFAULT_SCENE_MAP_SIZE_BYTES;
    cfg.scene_tileset_addr = GAME_PACKAGE_DEFAULT_TILESET_ADDR;
    cfg.scene_tileset_size_bytes = GAME_PACKAGE_DEFAULT_TILESET_SIZE_BYTES;
    cfg.topdown_render_scale = GAME_PACKAGE_DEFAULT_TOPDOWN_SCALE;
    cfg.topdown_tile_present_mode = (uint32_t)GAME_PACKAGE_TOPDOWN_PRESENT_AUTO;
    cfg.controller_profile_id = GAME_PACKAGE_DEFAULT_CONTROLLER_PROFILE;
    cfg.camera_profile_id = GAME_PACKAGE_DEFAULT_CAMERA_PROFILE;
    cfg.input_deadzone_permille = GAME_PACKAGE_DEFAULT_INPUT_DEADZONE_PM;
    cfg.input_flags = GAME_PACKAGE_DEFAULT_INPUT_FLAGS;
    cfg.move_speed_px_s = GAME_PACKAGE_DEFAULT_MOVE_SPEED_PX_S;
    cfg.move_accel_px_s2 = GAME_PACKAGE_DEFAULT_MOVE_ACCEL_PX_S2;
    cfg.move_decel_px_s2 = GAME_PACKAGE_DEFAULT_MOVE_DECEL_PX_S2;
    cfg.camera_deadzone_w_px = GAME_PACKAGE_DEFAULT_CAMERA_DZ_W_PX;
    cfg.camera_deadzone_h_px = GAME_PACKAGE_DEFAULT_CAMERA_DZ_H_PX;
    cfg.camera_follow_permille = GAME_PACKAGE_DEFAULT_CAMERA_FOLLOW_PM;
    cfg.camera_max_speed_px_s = GAME_PACKAGE_DEFAULT_CAMERA_MAX_SPEED_PX_S;
    cfg.camera_lookahead_x_px = GAME_PACKAGE_DEFAULT_CAMERA_LOOKAHEAD_X;
    cfg.camera_lookahead_y_px = GAME_PACKAGE_DEFAULT_CAMERA_LOOKAHEAD_Y;
    cfg.scene_map_id = GAME_PACKAGE_DEFAULT_SCENE_MAP_ID;
    cfg.scene_tileset_id = GAME_PACKAGE_DEFAULT_SCENE_TILESET_ID;
    cfg.music_asset_id = GAME_PACKAGE_DEFAULT_MUSIC_ASSET_ID;
    cfg.sfx_interact_asset_id = GAME_PACKAGE_DEFAULT_SFX_INTERACT_ASSET_ID;
    cfg.sfx_confirm_asset_id = GAME_PACKAGE_DEFAULT_SFX_CONFIRM_ASSET_ID;
    cfg.sfx_error_asset_id = GAME_PACKAGE_DEFAULT_SFX_ERROR_ASSET_ID;
  }

  return cfg;
}

static const game_package_mode_desc_t g_pkg_modes[] =
{
  {
    .mode_id = 1u,
    .runtime_kind = (uint32_t)GAME_PACKAGE_RT_KIND_TOPDOWN,
    .backend_id = (uint32_t)GAME_RUNTIME_BACKEND_TOPDOWN_BASIC,
    .name = "TOPDOWN_BASIC",
    .runtime_config =
    {
      .scene_map_addr = GAME_PACKAGE_DEFAULT_SCENE_MAP_ADDR,
      .scene_map_size_bytes = GAME_PACKAGE_DEFAULT_SCENE_MAP_SIZE_BYTES,
      .scene_tileset_addr = GAME_PACKAGE_DEFAULT_TILESET_ADDR,
      .scene_tileset_size_bytes = GAME_PACKAGE_DEFAULT_TILESET_SIZE_BYTES,
      .topdown_render_scale = GAME_PACKAGE_DEFAULT_TOPDOWN_SCALE,
      .topdown_tile_present_mode = (uint32_t)GAME_PACKAGE_TOPDOWN_PRESENT_AUTO,
      .controller_profile_id = GAME_PACKAGE_DEFAULT_CONTROLLER_PROFILE,
      .camera_profile_id = GAME_PACKAGE_DEFAULT_CAMERA_PROFILE,
      .input_deadzone_permille = GAME_PACKAGE_DEFAULT_INPUT_DEADZONE_PM,
      .input_flags = GAME_PACKAGE_DEFAULT_INPUT_FLAGS,
      .move_speed_px_s = GAME_PACKAGE_DEFAULT_MOVE_SPEED_PX_S,
      .move_accel_px_s2 = GAME_PACKAGE_DEFAULT_MOVE_ACCEL_PX_S2,
      .move_decel_px_s2 = GAME_PACKAGE_DEFAULT_MOVE_DECEL_PX_S2,
      .camera_deadzone_w_px = GAME_PACKAGE_DEFAULT_CAMERA_DZ_W_PX,
      .camera_deadzone_h_px = GAME_PACKAGE_DEFAULT_CAMERA_DZ_H_PX,
      .camera_follow_permille = GAME_PACKAGE_DEFAULT_CAMERA_FOLLOW_PM,
      .camera_max_speed_px_s = GAME_PACKAGE_DEFAULT_CAMERA_MAX_SPEED_PX_S,
      .camera_lookahead_x_px = GAME_PACKAGE_DEFAULT_CAMERA_LOOKAHEAD_X,
      .camera_lookahead_y_px = GAME_PACKAGE_DEFAULT_CAMERA_LOOKAHEAD_Y,
      .scene_map_id = GAME_PACKAGE_DEFAULT_SCENE_MAP_ID,
      .scene_tileset_id = GAME_PACKAGE_DEFAULT_SCENE_TILESET_ID,
      .music_asset_id = GAME_PACKAGE_DEFAULT_MUSIC_ASSET_ID,
      .sfx_interact_asset_id = GAME_PACKAGE_DEFAULT_SFX_INTERACT_ASSET_ID,
      .sfx_confirm_asset_id = GAME_PACKAGE_DEFAULT_SFX_CONFIRM_ASSET_ID,
      .sfx_error_asset_id = GAME_PACKAGE_DEFAULT_SFX_ERROR_ASSET_ID,
      .scene_lifecycle = (uint32_t)GAME_PACKAGE_SCENE_LIFECYCLE_RESUMABLE,
      .resume_domain_id = 1UL
    }
  },
  {
    .mode_id = 2u,
    .runtime_kind = (uint32_t)GAME_PACKAGE_RT_KIND_SIDESCROLL,
    .backend_id = (uint32_t)GAME_RUNTIME_BACKEND_RENDER_DEMO_3D_WALK,
    .name = "SIDESCROLL_DEMO",
    .runtime_config =
    {
      .scene_lifecycle = (uint32_t)GAME_PACKAGE_SCENE_LIFECYCLE_TRANSIENT,
      .resume_domain_id = 0UL
    }
  },
  {
    .mode_id = 3u,
    .runtime_kind = (uint32_t)GAME_PACKAGE_RT_KIND_TOPDOWN,
    .backend_id = (uint32_t)GAME_RUNTIME_BACKEND_TOPDOWN_BASIC,
    .name = "TOPDOWN_MARKET",
    .runtime_config =
    {
      .scene_map_addr = GAME_PACKAGE_DEFAULT_SCENE_MAP_ADDR,
      .scene_map_size_bytes = GAME_PACKAGE_DEFAULT_SCENE_MAP_SIZE_BYTES,
      .scene_tileset_addr = GAME_PACKAGE_DEFAULT_TILESET_ADDR,
      .scene_tileset_size_bytes = GAME_PACKAGE_DEFAULT_TILESET_SIZE_BYTES,
      .topdown_render_scale = GAME_PACKAGE_DEFAULT_TOPDOWN_SCALE,
      .topdown_tile_present_mode = (uint32_t)GAME_PACKAGE_TOPDOWN_PRESENT_AUTO,
      .controller_profile_id = GAME_PACKAGE_DEFAULT_CONTROLLER_PROFILE,
      .camera_profile_id = GAME_PACKAGE_DEFAULT_CAMERA_PROFILE,
      .input_deadzone_permille = GAME_PACKAGE_DEFAULT_INPUT_DEADZONE_PM,
      .input_flags = GAME_PACKAGE_DEFAULT_INPUT_FLAGS,
      .move_speed_px_s = GAME_PACKAGE_DEFAULT_MOVE_SPEED_PX_S,
      .move_accel_px_s2 = GAME_PACKAGE_DEFAULT_MOVE_ACCEL_PX_S2,
      .move_decel_px_s2 = GAME_PACKAGE_DEFAULT_MOVE_DECEL_PX_S2,
      .camera_deadzone_w_px = GAME_PACKAGE_DEFAULT_CAMERA_DZ_W_PX,
      .camera_deadzone_h_px = GAME_PACKAGE_DEFAULT_CAMERA_DZ_H_PX,
      .camera_follow_permille = GAME_PACKAGE_DEFAULT_CAMERA_FOLLOW_PM,
      .camera_max_speed_px_s = GAME_PACKAGE_DEFAULT_CAMERA_MAX_SPEED_PX_S,
      .camera_lookahead_x_px = GAME_PACKAGE_DEFAULT_CAMERA_LOOKAHEAD_X,
      .camera_lookahead_y_px = GAME_PACKAGE_DEFAULT_CAMERA_LOOKAHEAD_Y,
      .scene_map_id = 1003UL,
      .scene_tileset_id = 2003UL,
      .music_asset_id = 3001UL,
      .sfx_interact_asset_id = 3002UL,
      .sfx_confirm_asset_id = 3003UL,
      .sfx_error_asset_id = 3004UL,
      .scene_lifecycle = (uint32_t)GAME_PACKAGE_SCENE_LIFECYCLE_TRANSIENT,
      .resume_domain_id = 0UL
    }
  },
  {
    .mode_id = 4u,
    .runtime_kind = (uint32_t)GAME_PACKAGE_RT_KIND_SIDESCROLL,
    .backend_id = (uint32_t)GAME_RUNTIME_BACKEND_RENDER_DEMO,
    .name = "FX_DEMO",
    .runtime_config =
    {
      .scene_lifecycle = (uint32_t)GAME_PACKAGE_SCENE_LIFECYCLE_TRANSIENT,
      .resume_domain_id = 0UL
    }
  },
  {
    .mode_id = 5u,
    .runtime_kind = (uint32_t)GAME_PACKAGE_RT_KIND_SIDESCROLL,
    .backend_id = (uint32_t)GAME_RUNTIME_BACKEND_RENDER_DEMO_TITLE_ANIM,
    .name = "TITLE_ANIM_TEST",
    .runtime_config =
    {
      .scene_lifecycle = (uint32_t)GAME_PACKAGE_SCENE_LIFECYCLE_TRANSIENT,
      .resume_domain_id = 0UL
    }
  }
};

static const game_package_pet_route_t g_pkg_pet_routes[] =
{
  {(uint32_t)GAME_PET_ENTRY_START_GAME, 1u}
};

static const game_package_pet_menu_item_t g_pkg_pet_menu_items[] =
{
  {
    .slot_index = 0U,
    .icon_action_id = (uint8_t)GAME_PET_MENU_ACTION_FEED,
    .select_kind = (uint8_t)GAME_PET_MENU_SELECT_FEED,
    .status_kind = (uint8_t)GAME_PET_MENU_STATUS_NONE,
    .arg0 = 0U,
    .status_source_id = (uint16_t)GAME_PET_MENU_STATUS_SOURCE_NONE
  },
  {
    .slot_index = 1U,
    .icon_action_id = (uint8_t)GAME_PET_MENU_ACTION_PLAY,
    .select_kind = (uint8_t)GAME_PET_MENU_SELECT_PLAY,
    .status_kind = (uint8_t)GAME_PET_MENU_STATUS_NONE,
    .arg0 = 0U,
    .status_source_id = (uint16_t)GAME_PET_MENU_STATUS_SOURCE_NONE
  },
  {
    .slot_index = 2U,
    .icon_action_id = (uint8_t)GAME_PET_MENU_ACTION_SLOT3,
    .select_kind = (uint8_t)GAME_PET_MENU_SELECT_LAUNCH_MODE,
    .status_kind = (uint8_t)GAME_PET_MENU_STATUS_NONE,
    .arg0 = 3U,
    .status_source_id = (uint16_t)GAME_PET_MENU_STATUS_SOURCE_NONE
  },
  {
    .slot_index = 3U,
    .icon_action_id = (uint8_t)GAME_PET_MENU_ACTION_SLOT4,
    .select_kind = (uint8_t)GAME_PET_MENU_SELECT_LAUNCH_MODE,
    .status_kind = (uint8_t)GAME_PET_MENU_STATUS_NONE,
    .arg0 = 2U,
    .status_source_id = (uint16_t)GAME_PET_MENU_STATUS_SOURCE_NONE
  },
  {
    .slot_index = 4U,
    .icon_action_id = (uint8_t)GAME_PET_MENU_ACTION_SLOT5,
    .select_kind = (uint8_t)GAME_PET_MENU_SELECT_LAUNCH_MODE,
    .status_kind = (uint8_t)GAME_PET_MENU_STATUS_NONE,
    .arg0 = 4U,
    .status_source_id = (uint16_t)GAME_PET_MENU_STATUS_SOURCE_NONE
  },
  {
    .slot_index = 5U,
    .icon_action_id = (uint8_t)GAME_PET_MENU_ACTION_SLOT6,
    .select_kind = (uint8_t)GAME_PET_MENU_SELECT_LAUNCH_MODE,
    .status_kind = (uint8_t)GAME_PET_MENU_STATUS_NONE,
    .arg0 = 5U,
    .status_source_id = (uint16_t)GAME_PET_MENU_STATUS_SOURCE_NONE
  },
  {
    .slot_index = 8U,
    .icon_action_id = (uint8_t)GAME_PET_MENU_ACTION_START_GAME,
    .select_kind = (uint8_t)GAME_PET_MENU_SELECT_START_GAME,
    .status_kind = (uint8_t)GAME_PET_MENU_STATUS_NONE,
    .arg0 = 0U,
    .status_source_id = (uint16_t)GAME_PET_MENU_STATUS_SOURCE_NONE
  },
  {
    .slot_index = 9U,
    .icon_action_id = (uint8_t)GAME_PET_MENU_ACTION_OPTIONS,
    .select_kind = (uint8_t)GAME_PET_MENU_SELECT_OPTIONS,
    .status_kind = (uint8_t)GAME_PET_MENU_STATUS_LEVEL4,
    .arg0 = 6U,
    .status_source_id = (uint16_t)GAME_PET_MENU_STATUS_SOURCE_BATTERY
  }
};

static const game_package_desc_t g_active_package =
{
  1u,
  1u,
  g_pkg_modes,
  (uint32_t)(sizeof(g_pkg_modes) / sizeof(g_pkg_modes[0])),
  g_pkg_pet_routes,
  (uint32_t)(sizeof(g_pkg_pet_routes) / sizeof(g_pkg_pet_routes[0])),
  g_pkg_pet_menu_items,
  (uint32_t)(sizeof(g_pkg_pet_menu_items) / sizeof(g_pkg_pet_menu_items[0]))
};

static volatile ULONG g_requested_runtime_mode_id = 0UL;
static game_package_mode_desc_t g_staged_modes[GAME_PACKAGE_MANIFEST_MAX_MODE_COUNT];
static game_package_pet_route_t g_staged_pet_routes[GAME_PACKAGE_MANIFEST_MAX_PET_ROUTE_COUNT];
static game_package_pet_menu_item_t g_staged_pet_menu_items[GAME_PACKAGE_MANIFEST_MAX_PET_MENU_ITEM_COUNT];
static game_package_mode_desc_t g_loaded_modes[GAME_PACKAGE_MANIFEST_MAX_MODE_COUNT];
static game_package_pet_route_t g_loaded_pet_routes[GAME_PACKAGE_MANIFEST_MAX_PET_ROUTE_COUNT];
static game_package_pet_menu_item_t g_loaded_pet_menu_items[GAME_PACKAGE_MANIFEST_MAX_PET_MENU_ITEM_COUNT];
static game_package_desc_t g_loaded_package;
static uint8_t g_loaded_package_valid = 0U;
static const char *const g_loaded_mode_name = "PKG_MODE";

static uint32_t GamePackage_DefaultModeId(void)
{
  ULONG scenario = (ULONG)KNOB_GAME_RUNTIME_DEFAULT_SCENARIO;

  if (scenario == 1UL)
  {
    return 2u;
  }
  return 1u;
}

static uint8_t GamePackage_RuntimeKindValid(uint32_t runtime_kind)
{
  if ((runtime_kind == (uint32_t)GAME_PACKAGE_RT_KIND_TOPDOWN) ||
      (runtime_kind == (uint32_t)GAME_PACKAGE_RT_KIND_SIDESCROLL))
  {
    return 1U;
  }
  return 0U;
}

static uint8_t GamePackage_BackendIdValid(uint32_t backend_id)
{
  if ((backend_id == (uint32_t)GAME_RUNTIME_BACKEND_RENDER_DEMO) ||
      (backend_id == (uint32_t)GAME_RUNTIME_BACKEND_RENDER_DEMO_3D_WALK) ||
      (backend_id == (uint32_t)GAME_RUNTIME_BACKEND_TOPDOWN_BASIC) ||
      (backend_id == (uint32_t)GAME_RUNTIME_BACKEND_RENDER_DEMO_TITLE_ANIM))
  {
    return 1U;
  }
  return 0U;
}

static uint8_t GamePackage_ControllerProfileValid(uint32_t profile_id)
{
  if ((profile_id == (uint32_t)GAME_PACKAGE_CONTROLLER_PROFILE_TOPDOWN_DIGITAL_8DIR) ||
      (profile_id == (uint32_t)GAME_PACKAGE_CONTROLLER_PROFILE_TOPDOWN_ANALOG) ||
      (profile_id == (uint32_t)GAME_PACKAGE_CONTROLLER_PROFILE_SIDESCROLL_PLATFORMER) ||
      (profile_id == (uint32_t)GAME_PACKAGE_CONTROLLER_PROFILE_MINIGAME_CURSOR))
  {
    return 1U;
  }
  return 0U;
}

static uint8_t GamePackage_CameraProfileValid(uint32_t profile_id)
{
  if ((profile_id == (uint32_t)GAME_PACKAGE_CAMERA_PROFILE_LOCKED) ||
      (profile_id == (uint32_t)GAME_PACKAGE_CAMERA_PROFILE_FOLLOW_X) ||
      (profile_id == (uint32_t)GAME_PACKAGE_CAMERA_PROFILE_FOLLOW_XY) ||
      (profile_id == (uint32_t)GAME_PACKAGE_CAMERA_PROFILE_FOLLOW_DEADZONE))
  {
    return 1U;
  }
  return 0U;
}

static uint8_t GamePackage_RuntimeConfigValid(const game_package_runtime_config_t *cfg)
{
  if (cfg == (const game_package_runtime_config_t *)0)
  {
    return 0U;
  }

  if (cfg->topdown_tile_present_mode > (uint32_t)GAME_PACKAGE_TOPDOWN_PRESENT_BAYER)
  {
    return 0U;
  }
  if (cfg->controller_profile_id != 0U)
  {
    if (GamePackage_ControllerProfileValid(cfg->controller_profile_id) == 0U)
    {
      return 0U;
    }
  }
  if (cfg->camera_profile_id != 0U)
  {
    if (GamePackage_CameraProfileValid(cfg->camera_profile_id) == 0U)
    {
      return 0U;
    }
  }
  if (cfg->input_deadzone_permille > 1000U)
  {
    return 0U;
  }
  if (cfg->camera_follow_permille > 1000U)
  {
    return 0U;
  }
  if (cfg->scene_lifecycle > (uint32_t)GAME_PACKAGE_SCENE_LIFECYCLE_TRANSIENT)
  {
    return 0U;
  }
  if ((cfg->scene_lifecycle == (uint32_t)GAME_PACKAGE_SCENE_LIFECYCLE_TRANSIENT) &&
      (cfg->resume_domain_id != 0U))
  {
    return 0U;
  }
  if ((cfg->scene_lifecycle == (uint32_t)GAME_PACKAGE_SCENE_LIFECYCLE_RESUMABLE) &&
      (cfg->resume_domain_id == 0U))
  {
    return 0U;
  }

  return 1U;
}

static uint8_t GamePackage_PetMenuSelectKindValid(uint8_t select_kind)
{
  if ((select_kind == (uint8_t)GAME_PET_MENU_SELECT_NONE) ||
      (select_kind == (uint8_t)GAME_PET_MENU_SELECT_FEED) ||
      (select_kind == (uint8_t)GAME_PET_MENU_SELECT_PLAY) ||
      (select_kind == (uint8_t)GAME_PET_MENU_SELECT_START_GAME) ||
      (select_kind == (uint8_t)GAME_PET_MENU_SELECT_OPTIONS) ||
      (select_kind == (uint8_t)GAME_PET_MENU_SELECT_LAUNCH_MODE) ||
      (select_kind == (uint8_t)GAME_PET_MENU_SELECT_OPEN_PAGE) ||
      (select_kind == (uint8_t)GAME_PET_MENU_SELECT_SAND_FX))
  {
    return 1U;
  }
  return 0U;
}

static uint8_t GamePackage_PetMenuStatusKindValid(uint8_t status_kind)
{
  if ((status_kind == (uint8_t)GAME_PET_MENU_STATUS_NONE) ||
      (status_kind == (uint8_t)GAME_PET_MENU_STATUS_BOOL) ||
      (status_kind == (uint8_t)GAME_PET_MENU_STATUS_LEVEL4))
  {
    return 1U;
  }
  return 0U;
}

static uint8_t GamePackage_PetMenuStatusSourceValid(uint16_t source_id)
{
  if ((source_id == (uint16_t)GAME_PET_MENU_STATUS_SOURCE_NONE) ||
      (source_id == (uint16_t)GAME_PET_MENU_STATUS_SOURCE_BATTERY))
  {
    return 1U;
  }
  return 0U;
}

static uint8_t GamePackage_PetMenuItemValid(const game_package_pet_menu_item_t *item)
{
  if (item == (const game_package_pet_menu_item_t *)0)
  {
    return 0U;
  }
  if ((uint32_t)item->slot_index >= GAME_PET_MENU_SLOT_COUNT)
  {
    return 0U;
  }
  if ((uint32_t)item->icon_action_id >= (uint32_t)GAME_PET_MENU_ACTION_COUNT)
  {
    return 0U;
  }
  if (GamePackage_PetMenuSelectKindValid(item->select_kind) == 0U)
  {
    return 0U;
  }
  if (GamePackage_PetMenuStatusKindValid(item->status_kind) == 0U)
  {
    return 0U;
  }
  if (GamePackage_PetMenuStatusSourceValid(item->status_source_id) == 0U)
  {
    return 0U;
  }
  if (item->status_kind == (uint8_t)GAME_PET_MENU_STATUS_NONE)
  {
    if (item->status_source_id != (uint16_t)GAME_PET_MENU_STATUS_SOURCE_NONE)
    {
      return 0U;
    }
    if ((item->select_kind == (uint8_t)GAME_PET_MENU_SELECT_LAUNCH_MODE) &&
        (item->arg0 == 0U))
    {
      return 0U;
    }
    if ((item->select_kind == (uint8_t)GAME_PET_MENU_SELECT_OPEN_PAGE) &&
        (item->arg0 == 0U))
    {
      return 0U;
    }
  }
  else
  {
    if (item->status_source_id != (uint16_t)GAME_PET_MENU_STATUS_SOURCE_BATTERY)
    {
      return 0U;
    }
    if (item->status_kind == (uint8_t)GAME_PET_MENU_STATUS_BOOL)
    {
      if ((uint32_t)item->arg0 > ((uint32_t)GAME_PET_MENU_ACTION_COUNT - 2U))
      {
        return 0U;
      }
    }
    else if (item->status_kind == (uint8_t)GAME_PET_MENU_STATUS_LEVEL4)
    {
      if ((uint32_t)item->arg0 > ((uint32_t)GAME_PET_MENU_ACTION_COUNT - 4U))
      {
        return 0U;
      }
    }
  }
  return 1U;
}

static game_package_runtime_config_t GamePackage_RuntimeConfigFromManifestV2(
    const game_package_manifest_mode_v2_t *src_mode,
    uint32_t backend_id)
{
  game_package_runtime_config_t cfg = GamePackage_RuntimeConfigForBackend(backend_id);
  if (src_mode == (const game_package_manifest_mode_v2_t *)0)
  {
    return cfg;
  }

  cfg.scene_map_addr = src_mode->scene_map_addr;
  cfg.scene_map_size_bytes = src_mode->scene_map_size_bytes;
  cfg.scene_tileset_addr = src_mode->scene_tileset_addr;
  cfg.scene_tileset_size_bytes = src_mode->scene_tileset_size_bytes;
  cfg.topdown_render_scale = src_mode->topdown_render_scale;
  cfg.topdown_tile_present_mode = src_mode->topdown_tile_present_mode;
  cfg.controller_profile_id = src_mode->controller_profile_id;
  cfg.camera_profile_id = src_mode->camera_profile_id;
  cfg.input_deadzone_permille = src_mode->input_deadzone_permille;
  cfg.input_flags = src_mode->input_flags;
  cfg.move_speed_px_s = src_mode->move_speed_px_s;
  cfg.move_accel_px_s2 = src_mode->move_accel_px_s2;
  cfg.move_decel_px_s2 = src_mode->move_decel_px_s2;
  cfg.camera_deadzone_w_px = src_mode->camera_deadzone_w_px;
  cfg.camera_deadzone_h_px = src_mode->camera_deadzone_h_px;
  cfg.camera_follow_permille = src_mode->camera_follow_permille;
  cfg.camera_max_speed_px_s = src_mode->camera_max_speed_px_s;
  cfg.camera_lookahead_x_px = src_mode->camera_lookahead_x_px;
  cfg.camera_lookahead_y_px = src_mode->camera_lookahead_y_px;

  if (cfg.scene_map_addr == 0U)
  {
    cfg.scene_map_addr = GAME_PACKAGE_DEFAULT_SCENE_MAP_ADDR;
  }
  if (cfg.scene_tileset_addr == 0U)
  {
    cfg.scene_tileset_addr = GAME_PACKAGE_DEFAULT_TILESET_ADDR;
  }
  if (cfg.topdown_render_scale == 0U)
  {
    cfg.topdown_render_scale = GAME_PACKAGE_DEFAULT_TOPDOWN_SCALE;
  }
  if (cfg.controller_profile_id == 0U)
  {
    cfg.controller_profile_id = GAME_PACKAGE_DEFAULT_CONTROLLER_PROFILE;
  }
  if (cfg.camera_profile_id == 0U)
  {
    cfg.camera_profile_id = GAME_PACKAGE_DEFAULT_CAMERA_PROFILE;
  }
  if (cfg.move_speed_px_s == 0U)
  {
    cfg.move_speed_px_s = GAME_PACKAGE_DEFAULT_MOVE_SPEED_PX_S;
  }
  if (cfg.move_accel_px_s2 == 0U)
  {
    cfg.move_accel_px_s2 = GAME_PACKAGE_DEFAULT_MOVE_ACCEL_PX_S2;
  }
  if (cfg.move_decel_px_s2 == 0U)
  {
    cfg.move_decel_px_s2 = GAME_PACKAGE_DEFAULT_MOVE_DECEL_PX_S2;
  }

  return cfg;
}

static game_package_runtime_config_t GamePackage_RuntimeConfigFromManifestV4(
    const game_package_manifest_mode_v4_t *src_mode,
    uint32_t backend_id)
{
  game_package_runtime_config_t cfg = GamePackage_RuntimeConfigForBackend(backend_id);
  if (src_mode == (const game_package_manifest_mode_v4_t *)0)
  {
    return cfg;
  }

  cfg = GamePackage_RuntimeConfigFromManifestV2((const game_package_manifest_mode_v2_t *)src_mode,
                                                backend_id);
  cfg.scene_map_id = src_mode->scene_map_id;
  cfg.scene_tileset_id = src_mode->scene_tileset_id;
  cfg.music_asset_id = src_mode->music_asset_id;
  cfg.sfx_interact_asset_id = src_mode->sfx_interact_asset_id;
  cfg.sfx_confirm_asset_id = src_mode->sfx_confirm_asset_id;
  cfg.sfx_error_asset_id = src_mode->sfx_error_asset_id;

  return cfg;
}

static game_package_runtime_config_t GamePackage_RuntimeConfigFromManifestV5(
    const game_package_manifest_mode_v5_t *src_mode,
    uint32_t backend_id)
{
  game_package_runtime_config_t cfg = GamePackage_RuntimeConfigForBackend(backend_id);
  if (src_mode == (const game_package_manifest_mode_v5_t *)0)
  {
    return cfg;
  }

  cfg = GamePackage_RuntimeConfigFromManifestV4((const game_package_manifest_mode_v4_t *)src_mode,
                                                backend_id);
  cfg.scene_lifecycle = src_mode->scene_lifecycle;
  cfg.resume_domain_id = src_mode->resume_domain_id;
  return cfg;
}

static uint8_t GamePackage_HasModeId(const game_package_desc_t *pkg, uint32_t mode_id)
{
  uint32_t i;
  if ((pkg == (const game_package_desc_t *)0) || (pkg->modes == (const game_package_mode_desc_t *)0))
  {
    return 0U;
  }

  for (i = 0u; i < pkg->mode_count; i++)
  {
    if (pkg->modes[i].mode_id == mode_id)
    {
      return 1U;
    }
  }

  return 0U;
}

const game_package_desc_t *GamePackage_GetActive(void)
{
  if (g_loaded_package_valid != 0U)
  {
    return &g_loaded_package;
  }
  return &g_active_package;
}

const game_package_mode_desc_t *GamePackage_FindModeById(uint32_t mode_id)
{
  const game_package_desc_t *pkg = GamePackage_GetActive();
  uint32_t i;

  if ((pkg == (const game_package_desc_t *)0) || (pkg->modes == (const game_package_mode_desc_t *)0))
  {
    return (const game_package_mode_desc_t *)0;
  }

  for (i = 0u; i < pkg->mode_count; i++)
  {
    if (pkg->modes[i].mode_id == mode_id)
    {
      return &pkg->modes[i];
    }
  }

  return (const game_package_mode_desc_t *)0;
}

const game_package_runtime_config_t *GamePackage_GetRuntimeConfigByModeId(uint32_t mode_id)
{
  const game_package_mode_desc_t *mode = GamePackage_FindModeById(mode_id);
  if (mode == (const game_package_mode_desc_t *)0)
  {
    return (const game_package_runtime_config_t *)0;
  }
  return &mode->runtime_config;
}

UINT GamePackage_RequestRuntimeModeById(uint32_t mode_id)
{
  const game_package_mode_desc_t *mode = GamePackage_FindModeById(mode_id);

  if (mode == (const game_package_mode_desc_t *)0)
  {
    return TX_NOT_DONE;
  }

  g_requested_runtime_mode_id = (ULONG)mode_id;
  return TX_SUCCESS;
}

UINT GamePackage_RequestRuntimeModeByPetEntry(uint32_t pet_entry_id)
{
  const game_package_desc_t *pkg = GamePackage_GetActive();
  uint32_t i;
  uint32_t mode_id = 0u;

  if (pkg == (const game_package_desc_t *)0)
  {
    return TX_NOT_DONE;
  }

  for (i = 0u; i < pkg->pet_route_count; i++)
  {
    if (pkg->pet_routes[i].pet_entry_id == pet_entry_id)
    {
      mode_id = pkg->pet_routes[i].mode_id;
      break;
    }
  }

  if (mode_id == 0u)
  {
    mode_id = GamePackage_DefaultModeId();
  }

  return GamePackage_RequestRuntimeModeById(mode_id);
}

uint32_t GamePackage_GetPrimaryResumeModeId(void)
{
  const game_package_desc_t *pkg = GamePackage_GetActive();
  uint32_t i;
  uint32_t mode_id = 0u;

  if ((pkg == (const game_package_desc_t *)0) ||
      (pkg->modes == (const game_package_mode_desc_t *)0) ||
      (pkg->mode_count == 0U))
  {
    return 0U;
  }

  if (pkg->pet_routes != (const game_package_pet_route_t *)0)
  {
    for (i = 0u; i < pkg->pet_route_count; i++)
    {
      if (pkg->pet_routes[i].pet_entry_id == (uint32_t)GAME_PET_ENTRY_START_GAME)
      {
        mode_id = pkg->pet_routes[i].mode_id;
        break;
      }
    }
  }

  if (mode_id == 0u)
  {
    mode_id = GamePackage_DefaultModeId();
  }
  if ((mode_id == 0u) || (GamePackage_HasModeId(pkg, mode_id) == 0U))
  {
    mode_id = pkg->modes[0].mode_id;
  }

  return mode_id;
}

uint32_t GamePackage_ConsumeRequestedRuntimeModeId(void)
{
  uint32_t mode_id = (uint32_t)g_requested_runtime_mode_id;

  g_requested_runtime_mode_id = 0UL;
  return mode_id;
}

const game_package_pet_menu_item_t *GamePackage_GetPetMenuItemBySlot(uint32_t slot_index)
{
  const game_package_desc_t *pkg = GamePackage_GetActive();
  uint32_t i;

  if ((pkg == (const game_package_desc_t *)0) ||
      (pkg->pet_menu_items == (const game_package_pet_menu_item_t *)0))
  {
    return (const game_package_pet_menu_item_t *)0;
  }

  for (i = 0U; i < pkg->pet_menu_item_count; i++)
  {
    if ((uint32_t)pkg->pet_menu_items[i].slot_index == slot_index)
    {
      return &pkg->pet_menu_items[i];
    }
  }

  return (const game_package_pet_menu_item_t *)0;
}

UINT GamePackage_LoadManifestBlob(const void *manifest_data, uint32_t manifest_size)
{
  game_package_manifest_view_t view;
  game_package_desc_t staged_package = {0};
  uint32_t i;
  uint32_t package_id;
  uint32_t package_version;
  UINT status;

  status = GamePackageManifest_Parse(manifest_data, manifest_size, &view);
  if (status != TX_SUCCESS)
  {
    return status;
  }

  if (view.mode_count == 0u)
  {
    return TX_SIZE_ERROR;
  }

  for (i = 0u; i < view.mode_count; i++)
  {
    const uint8_t *mode_rec = view.modes_blob + ((uint32_t)i * (uint32_t)view.mode_record_size);
    uint32_t mode_id;
    uint32_t runtime_kind;
    uint32_t backend_id;
    game_package_runtime_config_t cfg;

    if (view.manifest_version == GAME_PACKAGE_MANIFEST_VERSION_V5)
    {
      const game_package_manifest_mode_v5_t *src = (const game_package_manifest_mode_v5_t *)mode_rec;
      mode_id = src->mode_id;
      runtime_kind = (uint32_t)src->runtime_kind;
      backend_id = (uint32_t)src->backend_id;
      cfg = GamePackage_RuntimeConfigFromManifestV5(src, backend_id);
    }
    else if (view.manifest_version == GAME_PACKAGE_MANIFEST_VERSION_V4)
    {
      const game_package_manifest_mode_v4_t *src = (const game_package_manifest_mode_v4_t *)mode_rec;
      mode_id = src->mode_id;
      runtime_kind = (uint32_t)src->runtime_kind;
      backend_id = (uint32_t)src->backend_id;
      cfg = GamePackage_RuntimeConfigFromManifestV4(src, backend_id);
    }
    else if ((view.manifest_version == GAME_PACKAGE_MANIFEST_VERSION_V2) ||
             (view.manifest_version == GAME_PACKAGE_MANIFEST_VERSION_V3))
    {
      const game_package_manifest_mode_v2_t *src = (const game_package_manifest_mode_v2_t *)mode_rec;
      mode_id = src->mode_id;
      runtime_kind = (uint32_t)src->runtime_kind;
      backend_id = (uint32_t)src->backend_id;
      cfg = GamePackage_RuntimeConfigFromManifestV2(src, backend_id);
    }
    else
    {
      const game_package_manifest_mode_t *src = (const game_package_manifest_mode_t *)mode_rec;
      mode_id = src->mode_id;
      runtime_kind = (uint32_t)src->runtime_kind;
      backend_id = (uint32_t)src->backend_id;
      cfg = GamePackage_RuntimeConfigForBackend(backend_id);
    }

    if ((mode_id == 0u) ||
        (GamePackage_RuntimeKindValid(runtime_kind) == 0U) ||
        (GamePackage_BackendIdValid(backend_id) == 0U) ||
        (GamePackage_RuntimeConfigValid(&cfg) == 0U))
    {
      return TX_NOT_DONE;
    }

    g_staged_modes[i].mode_id = mode_id;
    g_staged_modes[i].runtime_kind = runtime_kind;
    g_staged_modes[i].backend_id = backend_id;
    g_staged_modes[i].name = g_loaded_mode_name;
    g_staged_modes[i].runtime_config = cfg;
  }

  for (i = 0u; i < view.pet_route_count; i++)
  {
    const game_package_manifest_pet_route_t *src = &view.pet_routes[i];

    if ((src->pet_entry_id == 0u) ||
        (src->mode_id == 0u))
    {
      return TX_NOT_DONE;
    }

    g_staged_pet_routes[i].pet_entry_id = (uint32_t)src->pet_entry_id;
    g_staged_pet_routes[i].mode_id = src->mode_id;
  }

  if (view.header != (const game_package_manifest_header_t *)0)
  {
    package_id = view.header->package_id;
    package_version = view.header->package_version;
  }
  else if (view.header_v3 != (const game_package_manifest_header_v3_t *)0)
  {
    package_id = view.header_v3->package_id;
    package_version = view.header_v3->package_version;
  }
  else
  {
    return TX_NOT_DONE;
  }

  if (view.pet_menu_item_count > 0u)
  {
    uint32_t j;
    for (i = 0u; i < view.pet_menu_item_count; i++)
    {
      const game_package_manifest_pet_menu_item_t *src = &view.pet_menu_items[i];
      game_package_pet_menu_item_t *dst = &g_staged_pet_menu_items[i];

      dst->slot_index = src->slot_index;
      dst->icon_action_id = src->icon_action_id;
      dst->select_kind = src->select_kind;
      dst->status_kind = src->status_kind;
      dst->arg0 = src->arg0;
      dst->status_source_id = src->status_source_id;

      if (GamePackage_PetMenuItemValid(dst) == 0U)
      {
        return TX_NOT_DONE;
      }
      for (j = 0u; j < i; j++)
      {
        if (g_staged_pet_menu_items[j].slot_index == dst->slot_index)
        {
          return TX_NOT_DONE;
        }
      }
    }
  }

  staged_package.package_id = package_id;
  staged_package.package_version = package_version;
  staged_package.modes = g_staged_modes;
  staged_package.mode_count = view.mode_count;
  staged_package.pet_routes = g_staged_pet_routes;
  staged_package.pet_route_count = view.pet_route_count;
  if (view.pet_menu_item_count > 0u)
  {
    staged_package.pet_menu_items = g_staged_pet_menu_items;
    staged_package.pet_menu_item_count = view.pet_menu_item_count;
  }
  else
  {
    staged_package.pet_menu_items = g_pkg_pet_menu_items;
    staged_package.pet_menu_item_count = (uint32_t)(sizeof(g_pkg_pet_menu_items) / sizeof(g_pkg_pet_menu_items[0]));
  }

  for (i = 0u; i < staged_package.pet_route_count; i++)
  {
    if (GamePackage_HasModeId(&staged_package, staged_package.pet_routes[i].mode_id) == 0U)
    {
      return TX_NOT_DONE;
    }
  }
  for (i = 0u; i < staged_package.pet_menu_item_count; i++)
  {
    const game_package_pet_menu_item_t *item = &staged_package.pet_menu_items[i];
    if ((item->select_kind == (uint8_t)GAME_PET_MENU_SELECT_LAUNCH_MODE) &&
        (GamePackage_HasModeId(&staged_package, (uint32_t)item->arg0) == 0U))
    {
      return TX_NOT_DONE;
    }
  }

  /* Commit after full validation to preserve previous active package on failure. */
  for (i = 0u; i < view.mode_count; i++)
  {
    g_loaded_modes[i] = g_staged_modes[i];
  }
  for (i = 0u; i < view.pet_route_count; i++)
  {
    g_loaded_pet_routes[i] = g_staged_pet_routes[i];
  }
  if (view.pet_menu_item_count > 0u)
  {
    for (i = 0u; i < view.pet_menu_item_count; i++)
    {
      g_loaded_pet_menu_items[i] = g_staged_pet_menu_items[i];
    }
  }

  g_loaded_package.package_id = package_id;
  g_loaded_package.package_version = package_version;
  g_loaded_package.modes = g_loaded_modes;
  g_loaded_package.mode_count = view.mode_count;
  g_loaded_package.pet_routes = g_loaded_pet_routes;
  g_loaded_package.pet_route_count = view.pet_route_count;
  if (view.pet_menu_item_count > 0u)
  {
    g_loaded_package.pet_menu_items = g_loaded_pet_menu_items;
    g_loaded_package.pet_menu_item_count = view.pet_menu_item_count;
  }
  else
  {
    g_loaded_package.pet_menu_items = g_pkg_pet_menu_items;
    g_loaded_package.pet_menu_item_count = (uint32_t)(sizeof(g_pkg_pet_menu_items) / sizeof(g_pkg_pet_menu_items[0]));
  }

  g_loaded_package_valid = 1U;
  return TX_SUCCESS;
}

void GamePackage_ClearLoadedManifest(void)
{
  g_loaded_package_valid = 0U;
}
