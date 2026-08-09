/* Thread entry implementation for App_ThreadX runtime. */

#define APP_GAME_SCENE_TRANSITION_BLOCK_PX        (8U)
#define APP_GAME_SCENE_TRANSITION_STRIPE_H_PX     (8U)
#define APP_GAME_SCENE_TRANSITION_STYLE_COUNT     (20U)
#define APP_GAME_SCENE_TRANSITION_STYLE_CYCLE     (255U)

typedef enum
{
  APP_GAME_SCENE_TRANSITION_PHASE_IDLE = 0U,
  APP_GAME_SCENE_TRANSITION_PHASE_OUT = 1U,
  APP_GAME_SCENE_TRANSITION_PHASE_WAIT_SWAP = 2U,
  APP_GAME_SCENE_TRANSITION_PHASE_IN = 3U
} app_game_scene_transition_phase_t;

typedef enum
{
  APP_GAME_SCENE_TRANSITION_STYLE_DITHER_BLOCKS = 0U,
  APP_GAME_SCENE_TRANSITION_STYLE_VENETIAN = 1U,
  APP_GAME_SCENE_TRANSITION_STYLE_WIPE_LR = 2U,
  APP_GAME_SCENE_TRANSITION_STYLE_WIPE_TB = 3U,
  APP_GAME_SCENE_TRANSITION_STYLE_IRIS = 4U,
  APP_GAME_SCENE_TRANSITION_STYLE_DISSOLVE = 5U,
  APP_GAME_SCENE_TRANSITION_STYLE_DIAMOND_IRIS = 6U,
  APP_GAME_SCENE_TRANSITION_STYLE_CLOCK_SWEEP = 7U,
  APP_GAME_SCENE_TRANSITION_STYLE_CURTAIN_SPLIT = 8U,
  APP_GAME_SCENE_TRANSITION_STYLE_WAVE_WIPE = 9U,
  APP_GAME_SCENE_TRANSITION_STYLE_FOG_DRIFT = 10U,
  APP_GAME_SCENE_TRANSITION_STYLE_FLASHLIGHT = 11U,
  APP_GAME_SCENE_TRANSITION_STYLE_SHOCKWAVE = 12U,
  APP_GAME_SCENE_TRANSITION_STYLE_SCANLINE_GLITCH = 13U,
  APP_GAME_SCENE_TRANSITION_STYLE_VERTICAL_SLICES = 14U,
  APP_GAME_SCENE_TRANSITION_STYLE_STARBURST = 15U,
  APP_GAME_SCENE_TRANSITION_STYLE_TV_SNOW = 16U,
  APP_GAME_SCENE_TRANSITION_STYLE_VHOLD_ROLL = 17U,
  APP_GAME_SCENE_TRANSITION_STYLE_SYNC_TEAR = 18U,
  APP_GAME_SCENE_TRANSITION_STYLE_CRT_CHANNEL_TUNE = 19U
} app_game_scene_transition_style_t;

typedef struct
{
  uint8_t phase;
  uint8_t style;
  ULONG phase_start_tick;
} app_game_scene_transition_state_t;

static app_audio_asset_id_t g_game_rt_music_asset = APP_AUDIO_ASSET_NONE;
static app_audio_asset_id_t g_game_rt_sfx_interact_asset = APP_AUDIO_ASSET_NONE;
static app_audio_asset_id_t g_game_rt_sfx_confirm_asset = APP_AUDIO_ASSET_NONE;
static app_audio_asset_id_t g_game_rt_sfx_error_asset = APP_AUDIO_ASSET_NONE;
static uint8_t g_game_rt_music_started = 0U;
static uint32_t g_game_rt_scene_map_addr = 0UL;
static uint32_t g_game_rt_scene_map_size = 0UL;
static uint32_t g_game_rt_scene_tileset_addr = 0UL;
static uint32_t g_game_rt_scene_tileset_size = 0UL;
static uint32_t g_game_rt_scene_map_id = 0UL;
static uint32_t g_game_rt_scene_tileset_id = 0UL;
static uint8_t g_game_rt_transition_spawn_pending = 0U;
static uint32_t g_game_rt_transition_prev_map_crc32 = 0UL;
static uint32_t g_game_rt_transition_spawn_hash = 0UL;
static uint8_t g_game_rt_restore_pending = 0U;
static uint8_t g_game_rt_restore_applied = 0U;
static uint32_t g_game_rt_restore_scene_map_id = 0UL;
static uint32_t g_game_rt_restore_scene_tileset_id = 0UL;
static game_mode_topdown_basic_snapshot_t g_game_rt_restore_snapshot;
static app_game_scene_transition_state_t g_game_rt_scene_transition;
static uint8_t g_game_rt_scene_transition_overlay_clear_pending = 0U;
static app_storage_scene_load_status_t g_game_rt_scene_swap_load_status_base;
static uint8_t g_game_rt_scene_swap_load_status_base_valid = 0U;
static uint8_t g_game_rt_scene_swap_pending = 0U;
static uint32_t g_game_rt_scene_swap_map_id = 0UL;
static uint32_t g_game_rt_scene_swap_tileset_id = 0UL;
static uint32_t g_game_rt_scene_swap_map_addr = 0UL;
static uint32_t g_game_rt_scene_swap_map_size = 0UL;
static uint32_t g_game_rt_scene_swap_tileset_addr = 0UL;
static uint32_t g_game_rt_scene_swap_tileset_size = 0UL;
static uint32_t g_game_rt_scene_swap_spawn_hash = 0UL;
static uint8_t g_game_rt_scene_transition_style_cycle_idx = 0U;

static uint8_t AppGameRetainedSnapshotModeAllowed(uint32_t mode_id, uint32_t backend_id)
{
  const game_package_mode_desc_t *mode;
  uint32_t lifecycle;
  uint32_t resume_mode_id;

  if ((mode_id == 0UL) ||
      (backend_id != (uint32_t)GAME_RUNTIME_BACKEND_TOPDOWN_BASIC))
  {
    return 0U;
  }

  mode = GamePackage_FindModeById(mode_id);
  if (mode != (const game_package_mode_desc_t *)0)
  {
    lifecycle = mode->runtime_config.scene_lifecycle;
    if (lifecycle == (uint32_t)GAME_PACKAGE_SCENE_LIFECYCLE_RESUMABLE)
    {
      return 1U;
    }
    if (lifecycle == (uint32_t)GAME_PACKAGE_SCENE_LIFECYCLE_TRANSIENT)
    {
      return 0U;
    }
  }

  resume_mode_id = GamePackage_GetPrimaryResumeModeId();
  if (resume_mode_id == 0UL)
  {
    return 0U;
  }

  return (resume_mode_id == mode_id) ? 1U : 0U;
}

static const game_map_registry_entry_t *AppGameFindMapRegistryByHash(uint32_t target_map_hash)
{
  uint32_t i;

  for (i = 0UL; i < (uint32_t)GAME_MAP_REGISTRY_ENTRY_COUNT; i++)
  {
    const game_map_registry_entry_t *entry = &g_game_map_registry_entries[i];
    if ((entry != (const game_map_registry_entry_t *)0) &&
        (entry->target_map_hash == target_map_hash))
    {
      return entry;
    }
  }
  return (const game_map_registry_entry_t *)0;
}

static const game_map_registry_entry_t *AppGameFindMapRegistryBySceneMapId(uint32_t scene_map_id)
{
  uint32_t i;

  for (i = 0UL; i < (uint32_t)GAME_MAP_REGISTRY_ENTRY_COUNT; i++)
  {
    const game_map_registry_entry_t *entry = &g_game_map_registry_entries[i];
    if ((entry != (const game_map_registry_entry_t *)0) &&
        (entry->scene_map_id == scene_map_id))
    {
      return entry;
    }
  }
  return (const game_map_registry_entry_t *)0;
}

static const game_map_registry_entry_t *AppGameFindMapRegistryBySceneTilesetId(uint32_t scene_tileset_id)
{
  uint32_t i;

  for (i = 0UL; i < (uint32_t)GAME_MAP_REGISTRY_ENTRY_COUNT; i++)
  {
    const game_map_registry_entry_t *entry = &g_game_map_registry_entries[i];
    if ((entry != (const game_map_registry_entry_t *)0) &&
        (entry->scene_tileset_id == scene_tileset_id))
    {
      return entry;
    }
  }
  return (const game_map_registry_entry_t *)0;
}

static uint8_t AppGameResolveSceneSlotAddrs(uint32_t slot_index,
                                            uint32_t *map_addr_out,
                                            uint32_t *tileset_addr_out)
{
  uint32_t map_base = (uint32_t)KNOB_GAME_RT_SCENE_MAP_ADDR;
  uint32_t map_alt = (uint32_t)KNOB_GAME_RT_SCENE_MAP_ALT_ADDR;
  uint32_t tileset_base = (uint32_t)KNOB_GAME_RT_SCENE_TILESET_ADDR;
  uint32_t slot_stride = 0x00010000UL;
  uint32_t tileset_offset = 0x00001000UL;
  uint64_t installed_base = (uint64_t)((uint32_t)KNOB_STORAGE_INSTALLED_BASE_ADDR);
  uint64_t installed_size = (uint64_t)((uint32_t)KNOB_STORAGE_INSTALLED_SIZE_BYTES);
  uint64_t installed_end = installed_base + installed_size;
  uint64_t map_addr64;
  uint64_t tileset_addr64;

  if ((map_addr_out == (uint32_t *)0) || (tileset_addr_out == (uint32_t *)0))
  {
    return 0U;
  }

  if (map_alt > map_base)
  {
    slot_stride = map_alt - map_base;
  }
  if ((tileset_base > map_base) && ((tileset_base - map_base) < slot_stride))
  {
    tileset_offset = tileset_base - map_base;
  }

  map_addr64 = (uint64_t)map_base + ((uint64_t)slot_index * (uint64_t)slot_stride);
  tileset_addr64 = map_addr64 + (uint64_t)tileset_offset;
  if ((map_addr64 > 0xFFFFFFFFULL) || (tileset_addr64 > 0xFFFFFFFFULL))
  {
    return 0U;
  }
  if ((map_addr64 < installed_base) ||
      (tileset_addr64 < installed_base) ||
      (map_addr64 >= installed_end) ||
      (tileset_addr64 >= installed_end))
  {
    return 0U;
  }

  *map_addr_out = (uint32_t)map_addr64;
  *tileset_addr_out = (uint32_t)tileset_addr64;
  return 1U;
}

static uint8_t AppGameResolveSceneIdsByTargetMapHash(uint32_t target_map_hash,
                                                     uint32_t *scene_map_id_out,
                                                     uint32_t *scene_tileset_id_out)
{
  const game_map_registry_entry_t *entry;

  if ((scene_map_id_out == (uint32_t *)0) || (scene_tileset_id_out == (uint32_t *)0))
  {
    return 0U;
  }

  entry = AppGameFindMapRegistryByHash(target_map_hash);
  if (entry == (const game_map_registry_entry_t *)0)
  {
    return 0U;
  }

  *scene_map_id_out = entry->scene_map_id;
  *scene_tileset_id_out = entry->scene_tileset_id;
  return 1U;
}

static uint8_t AppGameResolveManifestSceneMapRef(uint32_t scene_map_id,
                                                 uint32_t *map_addr_out,
                                                 uint32_t *map_size_out)
{
  const game_map_registry_entry_t *entry;
  uint32_t map_addr = 0UL;
  uint32_t tileset_addr_unused = 0UL;

  if ((map_addr_out == (uint32_t *)0) || (map_size_out == (uint32_t *)0))
  {
    return 0U;
  }

  entry = AppGameFindMapRegistryBySceneMapId(scene_map_id);
  if (entry == (const game_map_registry_entry_t *)0)
  {
    return 0U;
  }
  if (AppGameResolveSceneSlotAddrs(entry->slot_index, &map_addr, &tileset_addr_unused) == 0U)
  {
    return 0U;
  }

  *map_addr_out = map_addr;
  /* Use header-probed length for ID-based map loads to avoid stale fixed sizes. */
  *map_size_out = 0UL;
  return 1U;
}

static uint8_t AppGameResolveManifestSceneTilesetRef(uint32_t scene_tileset_id,
                                                     uint32_t *tileset_addr_out,
                                                     uint32_t *tileset_size_out)
{
  const game_map_registry_entry_t *entry;
  uint32_t map_addr_unused = 0UL;
  uint32_t tileset_addr = 0UL;

  if ((tileset_addr_out == (uint32_t *)0) || (tileset_size_out == (uint32_t *)0))
  {
    return 0U;
  }

  entry = AppGameFindMapRegistryBySceneTilesetId(scene_tileset_id);
  if (entry == (const game_map_registry_entry_t *)0)
  {
    return 0U;
  }
  if (AppGameResolveSceneSlotAddrs(entry->slot_index, &map_addr_unused, &tileset_addr) == 0U)
  {
    return 0U;
  }

  *tileset_addr_out = tileset_addr;
  /* Use header-probed length for ID-based tileset loads. */
  *tileset_size_out = 0UL;
  return 1U;
}

static VOID AppGameResolveRealtimeSceneBindings(const game_package_runtime_config_t *rt_cfg)
{
  if (rt_cfg == (const game_package_runtime_config_t *)0)
  {
    return;
  }

  g_game_rt_scene_map_addr = rt_cfg->scene_map_addr;
  g_game_rt_scene_map_size = rt_cfg->scene_map_size_bytes;
  g_game_rt_scene_tileset_addr = rt_cfg->scene_tileset_addr;
  g_game_rt_scene_tileset_size = rt_cfg->scene_tileset_size_bytes;
  g_game_rt_scene_map_id = rt_cfg->scene_map_id;
  g_game_rt_scene_tileset_id = rt_cfg->scene_tileset_id;

  if (rt_cfg->scene_map_id != 0UL)
  {
    (void)AppGameResolveManifestSceneMapRef(rt_cfg->scene_map_id,
                                            &g_game_rt_scene_map_addr,
                                            &g_game_rt_scene_map_size);
  }
  if (rt_cfg->scene_tileset_id != 0UL)
  {
    (void)AppGameResolveManifestSceneTilesetRef(rt_cfg->scene_tileset_id,
                                                &g_game_rt_scene_tileset_addr,
                                                &g_game_rt_scene_tileset_size);
  }
}

static uint8_t AppGameApplyRetainedSceneBindings(uint32_t scene_map_id,
                                                 uint32_t scene_tileset_id)
{
  uint32_t map_addr = 0UL;
  uint32_t map_size = 0UL;
  uint32_t tileset_addr = 0UL;
  uint32_t tileset_size = 0UL;

  if ((scene_map_id == 0UL) || (scene_tileset_id == 0UL))
  {
    return 0U;
  }
  if ((AppGameResolveManifestSceneMapRef(scene_map_id, &map_addr, &map_size) == 0U) ||
      (AppGameResolveManifestSceneTilesetRef(scene_tileset_id, &tileset_addr, &tileset_size) == 0U))
  {
    return 0U;
  }
  if ((map_addr == 0UL) || (tileset_addr == 0UL))
  {
    return 0U;
  }

  g_game_rt_scene_map_id = scene_map_id;
  g_game_rt_scene_tileset_id = scene_tileset_id;
  g_game_rt_scene_map_addr = map_addr;
  g_game_rt_scene_map_size = map_size;
  g_game_rt_scene_tileset_addr = tileset_addr;
  g_game_rt_scene_tileset_size = tileset_size;
  return 1U;
}

static VOID AppGameResetRealtimeSceneBindings(void)
{
  g_game_rt_scene_map_addr = 0UL;
  g_game_rt_scene_map_size = 0UL;
  g_game_rt_scene_tileset_addr = 0UL;
  g_game_rt_scene_tileset_size = 0UL;
  g_game_rt_scene_map_id = 0UL;
  g_game_rt_scene_tileset_id = 0UL;
  g_game_rt_transition_spawn_pending = 0U;
  g_game_rt_transition_prev_map_crc32 = 0UL;
  g_game_rt_transition_spawn_hash = 0UL;
  g_game_rt_restore_pending = 0U;
  g_game_rt_restore_applied = 0U;
  g_game_rt_restore_scene_map_id = 0UL;
  g_game_rt_restore_scene_tileset_id = 0UL;
  (void)memset(&g_game_rt_restore_snapshot, 0, sizeof(g_game_rt_restore_snapshot));
  g_game_rt_scene_transition.phase = (uint8_t)APP_GAME_SCENE_TRANSITION_PHASE_IDLE;
  g_game_rt_scene_transition.style = (uint8_t)APP_GAME_SCENE_TRANSITION_STYLE_DITHER_BLOCKS;
  g_game_rt_scene_transition.phase_start_tick = 0UL;
  g_game_rt_scene_transition_overlay_clear_pending = 0U;
  (void)memset(&g_game_rt_scene_swap_load_status_base, 0, sizeof(g_game_rt_scene_swap_load_status_base));
  g_game_rt_scene_swap_load_status_base_valid = 0U;
  g_game_rt_scene_swap_pending = 0U;
  g_game_rt_scene_swap_map_id = 0UL;
  g_game_rt_scene_swap_tileset_id = 0UL;
  g_game_rt_scene_swap_map_addr = 0UL;
  g_game_rt_scene_swap_map_size = 0UL;
  g_game_rt_scene_swap_tileset_addr = 0UL;
  g_game_rt_scene_swap_tileset_size = 0UL;
  g_game_rt_scene_swap_spawn_hash = 0UL;
  g_game_rt_scene_transition_style_cycle_idx = 0U;
}

static ULONG AppGameSceneTransitionMsToTicks(ULONG ms)
{
  ULONG ticks;

  ticks = (ms * (ULONG)TX_TIMER_TICKS_PER_SECOND + 999UL) / 1000UL;
  if (ticks == 0UL)
  {
    ticks = 1UL;
  }
  return ticks;
}

static ULONG AppGameSceneTransitionOutMs(void)
{
  ULONG out_ms = (ULONG)KNOB_GAME_RT_SCENE_TRANSITION_OUT_MS;
  return (out_ms == 0UL) ? 1UL : out_ms;
}

static ULONG AppGameSceneTransitionInMs(void)
{
  ULONG in_ms = (ULONG)KNOB_GAME_RT_SCENE_TRANSITION_IN_MS;
  return (in_ms == 0UL) ? 1UL : in_ms;
}

static ULONG AppGameSceneTransitionWaitTimeoutMs(void)
{
  ULONG wait_ms = (ULONG)KNOB_GAME_RT_SCENE_TRANSITION_WAIT_TIMEOUT_MS;
  return (wait_ms == 0UL) ? 1UL : wait_ms;
}

static app_game_scene_transition_style_t AppGameSceneTransitionStyleFromRaw(uint32_t style_raw)
{
  if (style_raw >= (uint32_t)APP_GAME_SCENE_TRANSITION_STYLE_COUNT)
  {
    return APP_GAME_SCENE_TRANSITION_STYLE_DITHER_BLOCKS;
  }
  return (app_game_scene_transition_style_t)style_raw;
}

static app_game_scene_transition_style_t AppGameSceneTransitionSelectStyle(uint32_t style_knob)
{
  uint32_t style_raw = style_knob;

  if (style_raw == (uint32_t)APP_GAME_SCENE_TRANSITION_STYLE_CYCLE)
  {
    style_raw = (uint32_t)(g_game_rt_scene_transition_style_cycle_idx %
                           (uint8_t)APP_GAME_SCENE_TRANSITION_STYLE_COUNT);
    g_game_rt_scene_transition_style_cycle_idx =
        (uint8_t)((g_game_rt_scene_transition_style_cycle_idx + 1U) %
                  (uint8_t)APP_GAME_SCENE_TRANSITION_STYLE_COUNT);
  }

  return AppGameSceneTransitionStyleFromRaw(style_raw);
}

static uint16_t AppGameSceneTransitionAnglePermille(int32_t dx, int32_t dy)
{
  uint32_t adx = (dx < 0) ? (uint32_t)(-dx) : (uint32_t)dx;
  uint32_t ady = (dy < 0) ? (uint32_t)(-dy) : (uint32_t)dy;

  if ((adx == 0UL) && (ady == 0UL))
  {
    return 0U;
  }

  /* Sweep starts at up (0), advances clockwise to 1000. */
  if ((dy < 0) && (dx >= 0) && (adx <= ady))
  {
    return (uint16_t)((adx * 125UL) / ((ady == 0UL) ? 1UL : ady));
  }
  if ((dx > 0) && (dy < 0) && (adx > ady))
  {
    return (uint16_t)(125UL + ((ady * 125UL) / ((adx == 0UL) ? 1UL : adx)));
  }
  if ((dx > 0) && (dy >= 0) && (adx >= ady))
  {
    return (uint16_t)(250UL + ((ady * 125UL) / ((adx == 0UL) ? 1UL : adx)));
  }
  if ((dy > 0) && (dx >= 0) && (ady > adx))
  {
    return (uint16_t)(375UL + ((adx * 125UL) / ((ady == 0UL) ? 1UL : ady)));
  }
  if ((dy > 0) && (dx <= 0) && (adx <= ady))
  {
    return (uint16_t)(500UL + ((adx * 125UL) / ((ady == 0UL) ? 1UL : ady)));
  }
  if ((dx < 0) && (dy > 0) && (adx > ady))
  {
    return (uint16_t)(625UL + ((ady * 125UL) / ((adx == 0UL) ? 1UL : adx)));
  }
  if ((dx < 0) && (dy <= 0) && (adx >= ady))
  {
    return (uint16_t)(750UL + ((ady * 125UL) / ((adx == 0UL) ? 1UL : adx)));
  }

  return (uint16_t)(875UL + ((adx * 125UL) / ((ady == 0UL) ? 1UL : ady)));
}

static VOID AppGameSceneTransitionBeginIn(app_game_scene_transition_style_t style)
{
  g_game_rt_scene_transition.phase = (uint8_t)APP_GAME_SCENE_TRANSITION_PHASE_IN;
  g_game_rt_scene_transition.style = (uint8_t)style;
  g_game_rt_scene_transition.phase_start_tick = tx_time_get();
}

static VOID AppGameSceneTransitionBeginOut(app_game_scene_transition_style_t style)
{
  g_game_rt_scene_transition.phase = (uint8_t)APP_GAME_SCENE_TRANSITION_PHASE_OUT;
  g_game_rt_scene_transition.style = (uint8_t)style;
  g_game_rt_scene_transition.phase_start_tick = tx_time_get();
  if ((style == APP_GAME_SCENE_TRANSITION_STYLE_CRT_CHANNEL_TUNE) &&
      (g_game_rt_sfx_interact_asset != APP_AUDIO_ASSET_NONE))
  {
    (void)App_AudioReq_PlayAsset(g_game_rt_sfx_interact_asset);
  }
}

static uint8_t AppGameSceneTransitionActive(void)
{
  return (g_game_rt_scene_transition.phase != (uint8_t)APP_GAME_SCENE_TRANSITION_PHASE_IDLE) ? 1U : 0U;
}

static uint16_t AppGameSceneTransitionCoveragePermille(void)
{
  ULONG now_tick;
  ULONG elapsed;
  ULONG duration_ticks;
  uint32_t progress;

  if (g_game_rt_scene_transition.phase == (uint8_t)APP_GAME_SCENE_TRANSITION_PHASE_IDLE)
  {
    return 0U;
  }
  if (g_game_rt_scene_transition.phase == (uint8_t)APP_GAME_SCENE_TRANSITION_PHASE_WAIT_SWAP)
  {
    return 1000U;
  }

  now_tick = tx_time_get();
  elapsed = now_tick - g_game_rt_scene_transition.phase_start_tick;
  duration_ticks = (g_game_rt_scene_transition.phase == (uint8_t)APP_GAME_SCENE_TRANSITION_PHASE_OUT)
                       ? AppGameSceneTransitionMsToTicks(AppGameSceneTransitionOutMs())
                       : AppGameSceneTransitionMsToTicks(AppGameSceneTransitionInMs());
  if (duration_ticks == 0UL)
  {
    duration_ticks = 1UL;
  }
  if (elapsed >= duration_ticks)
  {
    return (g_game_rt_scene_transition.phase == (uint8_t)APP_GAME_SCENE_TRANSITION_PHASE_OUT) ? 1000U : 0U;
  }

  progress = (uint32_t)((elapsed * 1000UL) / duration_ticks);
  if (progress > 1000UL)
  {
    progress = 1000UL;
  }
  if (g_game_rt_scene_transition.phase == (uint8_t)APP_GAME_SCENE_TRANSITION_PHASE_OUT)
  {
    return (uint16_t)progress;
  }
  return (uint16_t)(1000UL - progress);
}

static VOID AppGameSceneTransitionQueueSwap(uint32_t scene_map_id,
                                            uint32_t scene_tileset_id,
                                            uint32_t map_addr,
                                            uint32_t map_size,
                                            uint32_t tileset_addr,
                                            uint32_t tileset_size,
                                            uint32_t spawn_hash)
{
  g_game_rt_scene_swap_map_id = scene_map_id;
  g_game_rt_scene_swap_tileset_id = scene_tileset_id;
  g_game_rt_scene_swap_map_addr = map_addr;
  g_game_rt_scene_swap_map_size = map_size;
  g_game_rt_scene_swap_tileset_addr = tileset_addr;
  g_game_rt_scene_swap_tileset_size = tileset_size;
  g_game_rt_scene_swap_spawn_hash = spawn_hash;
  g_game_rt_scene_swap_pending = 1U;
}

static uint8_t AppGameSceneTransitionApplyQueuedSwap(void)
{
  UINT status_tileset;
  UINT status_map;
  app_storage_scene_load_status_t status_before;

  if (g_game_rt_scene_swap_pending == 0U)
  {
    return 0U;
  }

  if (App_StorageSceneLoadStatusGet(&status_before) == TX_SUCCESS)
  {
    g_game_rt_scene_swap_load_status_base = status_before;
    g_game_rt_scene_swap_load_status_base_valid = 1U;
  }
  else
  {
    (void)memset(&g_game_rt_scene_swap_load_status_base, 0, sizeof(g_game_rt_scene_swap_load_status_base));
    g_game_rt_scene_swap_load_status_base_valid = 0U;
  }

  status_tileset = App_StorageReq_SceneTilesetLoad((ULONG)g_game_rt_scene_swap_tileset_addr,
                                                    (ULONG)g_game_rt_scene_swap_tileset_size);
  status_map = App_StorageReq_SceneMapLoad((ULONG)g_game_rt_scene_swap_map_addr,
                                           (ULONG)g_game_rt_scene_swap_map_size);
  if ((status_tileset != TX_SUCCESS) || (status_map != TX_SUCCESS))
  {
    g_game_rt_scene_swap_load_status_base_valid = 0U;
    (void)memset(&g_game_rt_scene_swap_load_status_base, 0, sizeof(g_game_rt_scene_swap_load_status_base));
    g_game_rt_scene_swap_pending = 0U;
    return 0U;
  }

  g_game_rt_transition_spawn_hash = g_game_rt_scene_swap_spawn_hash;
  g_game_rt_transition_spawn_pending = 1U;
  g_game_rt_scene_map_id = g_game_rt_scene_swap_map_id;
  g_game_rt_scene_tileset_id = g_game_rt_scene_swap_tileset_id;
  g_game_rt_scene_map_addr = g_game_rt_scene_swap_map_addr;
  g_game_rt_scene_map_size = g_game_rt_scene_swap_map_size;
  g_game_rt_scene_tileset_addr = g_game_rt_scene_swap_tileset_addr;
  g_game_rt_scene_tileset_size = g_game_rt_scene_swap_tileset_size;
  g_game_rt_scene_swap_pending = 0U;
  return 1U;
}

static VOID AppGameSceneTransitionPump(void)
{
  ULONG now_tick;
  ULONG elapsed;
  ULONG out_ticks;
  ULONG in_ticks;
  ULONG wait_ticks;

  if (g_game_rt_scene_transition.phase == (uint8_t)APP_GAME_SCENE_TRANSITION_PHASE_IDLE)
  {
    return;
  }

  now_tick = tx_time_get();
  elapsed = now_tick - g_game_rt_scene_transition.phase_start_tick;
  out_ticks = AppGameSceneTransitionMsToTicks(AppGameSceneTransitionOutMs());
  in_ticks = AppGameSceneTransitionMsToTicks(AppGameSceneTransitionInMs());
  wait_ticks = AppGameSceneTransitionMsToTicks(AppGameSceneTransitionWaitTimeoutMs());

  if (g_game_rt_scene_transition.phase == (uint8_t)APP_GAME_SCENE_TRANSITION_PHASE_OUT)
  {
    if (elapsed < out_ticks)
    {
      return;
    }

    if (AppGameSceneTransitionApplyQueuedSwap() == 0U)
    {
      g_game_rt_scene_transition.phase = (uint8_t)APP_GAME_SCENE_TRANSITION_PHASE_IDLE;
      g_game_rt_scene_transition_overlay_clear_pending = 1U;
      return;
    }

    g_game_rt_scene_transition.phase = (uint8_t)APP_GAME_SCENE_TRANSITION_PHASE_WAIT_SWAP;
    g_game_rt_scene_transition.phase_start_tick = now_tick;
    return;
  }

  if (g_game_rt_scene_transition.phase == (uint8_t)APP_GAME_SCENE_TRANSITION_PHASE_WAIT_SWAP)
  {
    if (g_game_rt_transition_spawn_pending == 0U)
    {
      g_game_rt_scene_transition.phase = (uint8_t)APP_GAME_SCENE_TRANSITION_PHASE_IN;
      g_game_rt_scene_transition.phase_start_tick = now_tick;
      return;
    }
    if (elapsed >= wait_ticks)
    {
      /*
       * Keep scene fully obscured until storage confirms completion
       * (success or explicit failure handled by spawn-pending path).
       */
      g_game_rt_scene_transition.phase_start_tick = now_tick;
    }
    return;
  }

  if (g_game_rt_scene_transition.phase == (uint8_t)APP_GAME_SCENE_TRANSITION_PHASE_IN)
  {
    if (elapsed < in_ticks)
    {
      return;
    }
    g_game_rt_scene_transition.phase = (uint8_t)APP_GAME_SCENE_TRANSITION_PHASE_IDLE;
    g_game_rt_scene_transition_overlay_clear_pending = 1U;
  }
}

static VOID AppGameDrawSceneTransitionOverlay(void)
{
  uint16_t coverage = AppGameSceneTransitionCoveragePermille();

  /* Remove prior frame's transition mask before drawing current coverage. */
  renderClearRectTransparent(0U,
                             0U,
                             (uint16_t)RENDER_WIDTH,
                             (uint16_t)RENDER_HEIGHT,
                             RENDER_LAYER_UI);

  if (coverage == 0U)
  {
    return;
  }

  if (g_game_rt_scene_transition.style == (uint8_t)APP_GAME_SCENE_TRANSITION_STYLE_VENETIAN)
  {
    uint16_t stripe_h = (uint16_t)APP_GAME_SCENE_TRANSITION_STRIPE_H_PX;
    uint16_t cover_w = (uint16_t)(((uint32_t)RENDER_WIDTH * (uint32_t)coverage) / 1000UL);
    uint16_t y = 0U;
    uint16_t stripe_idx = 0U;

    if (cover_w == 0U)
    {
      return;
    }
    while (y < (uint16_t)RENDER_HEIGHT)
    {
      uint16_t h = stripe_h;
      uint16_t x = 0U;
      if ((uint16_t)(y + h) > (uint16_t)RENDER_HEIGHT)
      {
        h = (uint16_t)((uint16_t)RENDER_HEIGHT - y);
      }
      if ((stripe_idx & 0x1U) != 0U)
      {
        x = (cover_w >= (uint16_t)RENDER_WIDTH) ? 0U : (uint16_t)((uint16_t)RENDER_WIDTH - cover_w);
      }
      renderFillRect(x, y, cover_w, h, RENDER_LAYER_UI, RENDER_COLOR_BLACK);
      y = (uint16_t)(y + h);
      stripe_idx++;
    }
    return;
  }

  if (g_game_rt_scene_transition.style == (uint8_t)APP_GAME_SCENE_TRANSITION_STYLE_WIPE_LR)
  {
    uint16_t cover_w = (uint16_t)(((uint32_t)RENDER_WIDTH * (uint32_t)coverage) / 1000UL);
    if (cover_w > 0U)
    {
      renderFillRect(0U,
                     0U,
                     cover_w,
                     (uint16_t)RENDER_HEIGHT,
                     RENDER_LAYER_UI,
                     RENDER_COLOR_BLACK);
    }
    return;
  }

  if (g_game_rt_scene_transition.style == (uint8_t)APP_GAME_SCENE_TRANSITION_STYLE_WIPE_TB)
  {
    uint16_t cover_h = (uint16_t)(((uint32_t)RENDER_HEIGHT * (uint32_t)coverage) / 1000UL);
    if (cover_h > 0U)
    {
      renderFillRect(0U,
                     0U,
                     (uint16_t)RENDER_WIDTH,
                     cover_h,
                     RENDER_LAYER_UI,
                     RENDER_COLOR_BLACK);
    }
    return;
  }

  if (g_game_rt_scene_transition.style == (uint8_t)APP_GAME_SCENE_TRANSITION_STYLE_IRIS)
  {
    uint16_t clear_w = (uint16_t)(((uint32_t)RENDER_WIDTH * (uint32_t)(1000U - coverage)) / 1000UL);
    uint16_t clear_h = (uint16_t)(((uint32_t)RENDER_HEIGHT * (uint32_t)(1000U - coverage)) / 1000UL);
    uint16_t clear_x = 0U;
    uint16_t clear_y = 0U;

    renderFillRect(0U,
                   0U,
                   (uint16_t)RENDER_WIDTH,
                   (uint16_t)RENDER_HEIGHT,
                   RENDER_LAYER_UI,
                   RENDER_COLOR_BLACK);

    if ((clear_w == 0U) || (clear_h == 0U))
    {
      return;
    }

    clear_x = (uint16_t)(((uint16_t)RENDER_WIDTH - clear_w) / 2U);
    clear_y = (uint16_t)(((uint16_t)RENDER_HEIGHT - clear_h) / 2U);
    renderClearRectTransparent(clear_x,
                               clear_y,
                               clear_w,
                               clear_h,
                               RENDER_LAYER_UI);
    return;
  }

  if (g_game_rt_scene_transition.style == (uint8_t)APP_GAME_SCENE_TRANSITION_STYLE_DISSOLVE)
  {
    uint16_t block = (uint16_t)APP_GAME_SCENE_TRANSITION_BLOCK_PX;
    uint16_t cols = (uint16_t)(((uint16_t)RENDER_WIDTH + block - 1U) / block);
    uint16_t rows = (uint16_t)(((uint16_t)RENDER_HEIGHT + block - 1U) / block);
    uint16_t by;
    uint16_t bx;

    for (by = 0U; by < rows; by++)
    {
      for (bx = 0U; bx < cols; bx++)
      {
        uint32_t hash = ((uint32_t)bx * 1103515245UL) ^ ((uint32_t)by * 2246822519UL) ^ 0x9E3779B9UL;
        uint16_t gate;
        uint16_t x = (uint16_t)(bx * block);
        uint16_t y = (uint16_t)(by * block);
        uint16_t w = block;
        uint16_t h = block;

        hash ^= (hash >> 16);
        hash *= 2246822519UL;
        hash ^= (hash >> 13);
        gate = (uint16_t)(hash % 1000UL);

        if (gate > coverage)
        {
          continue;
        }
        if ((uint16_t)(x + w) > (uint16_t)RENDER_WIDTH)
        {
          w = (uint16_t)((uint16_t)RENDER_WIDTH - x);
        }
        if ((uint16_t)(y + h) > (uint16_t)RENDER_HEIGHT)
        {
          h = (uint16_t)((uint16_t)RENDER_HEIGHT - y);
        }
        renderFillRect(x, y, w, h, RENDER_LAYER_UI, RENDER_COLOR_BLACK);
      }
    }
    return;
  }

  if (g_game_rt_scene_transition.style == (uint8_t)APP_GAME_SCENE_TRANSITION_STYLE_DIAMOND_IRIS)
  {
    uint16_t block = (uint16_t)APP_GAME_SCENE_TRANSITION_BLOCK_PX;
    uint16_t cols = (uint16_t)(((uint16_t)RENDER_WIDTH + block - 1U) / block);
    uint16_t rows = (uint16_t)(((uint16_t)RENDER_HEIGHT + block - 1U) / block);
    int32_t cx = (int32_t)RENDER_WIDTH / 2;
    int32_t cy = (int32_t)RENDER_HEIGHT / 2;
    uint32_t radius_max = (uint32_t)(cx + cy + 2);
    uint32_t clear_radius = ((uint32_t)(1000U - coverage) * radius_max) / 1000UL;
    uint16_t by;
    uint16_t bx;

    renderFillRect(0U,
                   0U,
                   (uint16_t)RENDER_WIDTH,
                   (uint16_t)RENDER_HEIGHT,
                   RENDER_LAYER_UI,
                   RENDER_COLOR_BLACK);

    if (clear_radius == 0UL)
    {
      return;
    }

    for (by = 0U; by < rows; by++)
    {
      for (bx = 0U; bx < cols; bx++)
      {
        uint16_t x = (uint16_t)(bx * block);
        uint16_t y = (uint16_t)(by * block);
        uint16_t w = block;
        uint16_t h = block;
        int32_t px = (int32_t)x + ((int32_t)w / 2);
        int32_t py = (int32_t)y + ((int32_t)h / 2);
        uint32_t manhattan;
        int32_t dx = px - cx;
        int32_t dy = py - cy;

        if ((uint16_t)(x + w) > (uint16_t)RENDER_WIDTH)
        {
          w = (uint16_t)((uint16_t)RENDER_WIDTH - x);
        }
        if ((uint16_t)(y + h) > (uint16_t)RENDER_HEIGHT)
        {
          h = (uint16_t)((uint16_t)RENDER_HEIGHT - y);
        }
        manhattan = (uint32_t)((dx < 0) ? -dx : dx) + (uint32_t)((dy < 0) ? -dy : dy);
        if (manhattan > clear_radius)
        {
          continue;
        }

        renderClearRectTransparent(x, y, w, h, RENDER_LAYER_UI);
      }
    }
    return;
  }

  if (g_game_rt_scene_transition.style == (uint8_t)APP_GAME_SCENE_TRANSITION_STYLE_CLOCK_SWEEP)
  {
    uint16_t block = (uint16_t)APP_GAME_SCENE_TRANSITION_BLOCK_PX;
    uint16_t cols = (uint16_t)(((uint16_t)RENDER_WIDTH + block - 1U) / block);
    uint16_t rows = (uint16_t)(((uint16_t)RENDER_HEIGHT + block - 1U) / block);
    int32_t cx = (int32_t)RENDER_WIDTH / 2;
    int32_t cy = (int32_t)RENDER_HEIGHT / 2;
    uint16_t by;
    uint16_t bx;

    for (by = 0U; by < rows; by++)
    {
      for (bx = 0U; bx < cols; bx++)
      {
        uint16_t x = (uint16_t)(bx * block);
        uint16_t y = (uint16_t)(by * block);
        uint16_t w = block;
        uint16_t h = block;
        int32_t px = (int32_t)x + ((int32_t)w / 2);
        int32_t py = (int32_t)y + ((int32_t)h / 2);
        uint16_t angle = AppGameSceneTransitionAnglePermille(px - cx, py - cy);

        if (angle > coverage)
        {
          continue;
        }
        if ((uint16_t)(x + w) > (uint16_t)RENDER_WIDTH)
        {
          w = (uint16_t)((uint16_t)RENDER_WIDTH - x);
        }
        if ((uint16_t)(y + h) > (uint16_t)RENDER_HEIGHT)
        {
          h = (uint16_t)((uint16_t)RENDER_HEIGHT - y);
        }
        renderFillRect(x, y, w, h, RENDER_LAYER_UI, RENDER_COLOR_BLACK);
      }
    }
    return;
  }

  if (g_game_rt_scene_transition.style == (uint8_t)APP_GAME_SCENE_TRANSITION_STYLE_CURTAIN_SPLIT)
  {
    uint16_t cover_w = (uint16_t)(((uint32_t)RENDER_WIDTH * (uint32_t)coverage) / 1000UL);
    uint16_t half = (uint16_t)(cover_w / 2U);
    uint16_t right_x = 0U;
    uint16_t right_w = half;

    if (cover_w == 0U)
    {
      return;
    }
    if (coverage >= 1000U)
    {
      renderFillRect(0U,
                     0U,
                     (uint16_t)RENDER_WIDTH,
                     (uint16_t)RENDER_HEIGHT,
                     RENDER_LAYER_UI,
                     RENDER_COLOR_BLACK);
      return;
    }

    renderFillRect(0U,
                   0U,
                   half,
                   (uint16_t)RENDER_HEIGHT,
                   RENDER_LAYER_UI,
                   RENDER_COLOR_BLACK);

    right_x = (uint16_t)((uint16_t)RENDER_WIDTH - half);
    if ((uint16_t)(right_x + right_w) > (uint16_t)RENDER_WIDTH)
    {
      right_w = (uint16_t)((uint16_t)RENDER_WIDTH - right_x);
    }
    renderFillRect(right_x,
                   0U,
                   right_w,
                   (uint16_t)RENDER_HEIGHT,
                   RENDER_LAYER_UI,
                   RENDER_COLOR_BLACK);
    return;
  }

  if (g_game_rt_scene_transition.style == (uint8_t)APP_GAME_SCENE_TRANSITION_STYLE_WAVE_WIPE)
  {
    uint16_t stripe_h = 6U;
    uint16_t y = 0U;
    uint16_t base_cover = (uint16_t)(((uint32_t)RENDER_WIDTH * (uint32_t)coverage) / 1000UL);
    uint16_t amp = (uint16_t)((uint16_t)RENDER_WIDTH / 10U);
    uint16_t stripe_idx = 0U;

    if (base_cover == 0U)
    {
      return;
    }
    if (coverage >= 1000U)
    {
      renderFillRect(0U,
                     0U,
                     (uint16_t)RENDER_WIDTH,
                     (uint16_t)RENDER_HEIGHT,
                     RENDER_LAYER_UI,
                     RENDER_COLOR_BLACK);
      return;
    }

    while (y < (uint16_t)RENDER_HEIGHT)
    {
      uint16_t h = stripe_h;
      int16_t phase = (int16_t)(stripe_idx % 8U);
      int16_t centered = (phase < 4) ? phase : (int16_t)(7 - phase);
      int16_t offset = (int16_t)(((int32_t)centered * (int32_t)amp) / 4L) - (int16_t)(amp / 2U);
      int32_t cover_i = (int32_t)base_cover + (int32_t)offset;
      uint16_t cover_w;

      if ((uint16_t)(y + h) > (uint16_t)RENDER_HEIGHT)
      {
        h = (uint16_t)((uint16_t)RENDER_HEIGHT - y);
      }
      if (cover_i < 0)
      {
        cover_w = 0U;
      }
      else if (cover_i > (int32_t)RENDER_WIDTH)
      {
        cover_w = (uint16_t)RENDER_WIDTH;
      }
      else
      {
        cover_w = (uint16_t)cover_i;
      }

      if (cover_w > 0U)
      {
        renderFillRect(0U, y, cover_w, h, RENDER_LAYER_UI, RENDER_COLOR_BLACK);
      }
      y = (uint16_t)(y + h);
      stripe_idx++;
    }
    return;
  }

  if (g_game_rt_scene_transition.style == (uint8_t)APP_GAME_SCENE_TRANSITION_STYLE_FOG_DRIFT)
  {
    uint16_t block = 4U;
    uint16_t cols = (uint16_t)(((uint16_t)RENDER_WIDTH + block - 1U) / block);
    uint16_t rows = (uint16_t)(((uint16_t)RENDER_HEIGHT + block - 1U) / block);
    uint32_t t = (uint32_t)tx_time_get();
    uint16_t by;
    uint16_t bx;

    if (coverage >= 1000U)
    {
      renderFillRect(0U,
                     0U,
                     (uint16_t)RENDER_WIDTH,
                     (uint16_t)RENDER_HEIGHT,
                     RENDER_LAYER_UI,
                     RENDER_COLOR_BLACK);
      return;
    }

    for (by = 0U; by < rows; by++)
    {
      for (bx = 0U; bx < cols; bx++)
      {
        uint32_t seed0 = ((uint32_t)(bx + (uint16_t)(t & 0x1FU)) * 2654435761UL) ^
                         ((uint32_t)(by + (uint16_t)((t >> 1) & 0x1FU)) * 2246822519UL) ^
                         (t * 40503UL);
        uint32_t seed1 = ((uint32_t)(bx + (uint16_t)((t >> 2) & 0x1FU)) * 3266489917UL) ^
                         ((uint32_t)(by + (uint16_t)((t >> 3) & 0x1FU)) * 668265263UL) ^
                         (t * 95849UL);
        uint16_t gate0;
        uint16_t gate1;
        uint16_t gate;
        uint16_t x = (uint16_t)(bx * block);
        uint16_t y = (uint16_t)(by * block);
        uint16_t w = block;
        uint16_t h = block;
        uint16_t row_bias = (uint16_t)(((uint32_t)by * 180UL) / (uint32_t)((rows == 0U) ? 1U : rows));

        seed0 ^= (seed0 >> 16);
        seed0 *= 2246822519UL;
        seed0 ^= (seed0 >> 13);
        gate0 = (uint16_t)(seed0 % 1000UL);

        seed1 ^= (seed1 >> 16);
        seed1 *= 2654435761UL;
        seed1 ^= (seed1 >> 13);
        gate1 = (uint16_t)(seed1 % 1000UL);

        gate = (gate0 < gate1) ? gate0 : gate1;
        gate = (uint16_t)(gate + row_bias);
        if (gate > 999U)
        {
          gate = 999U;
        }
        if (gate > coverage)
        {
          continue;
        }

        if ((uint16_t)(x + w) > (uint16_t)RENDER_WIDTH)
        {
          w = (uint16_t)((uint16_t)RENDER_WIDTH - x);
        }
        if ((uint16_t)(y + h) > (uint16_t)RENDER_HEIGHT)
        {
          h = (uint16_t)((uint16_t)RENDER_HEIGHT - y);
        }
        renderFillRect(x, y, w, h, RENDER_LAYER_UI, RENDER_COLOR_BLACK);
      }
    }
    return;
  }

  if (g_game_rt_scene_transition.style == (uint8_t)APP_GAME_SCENE_TRANSITION_STYLE_FLASHLIGHT)
  {
    uint16_t block = 4U;
    uint16_t cols = (uint16_t)(((uint16_t)RENDER_WIDTH + block - 1U) / block);
    uint16_t rows = (uint16_t)(((uint16_t)RENDER_HEIGHT + block - 1U) / block);
    int32_t cx = (int32_t)RENDER_WIDTH / 2;
    int32_t cy = (int32_t)RENDER_HEIGHT / 2;
    uint32_t radius_max = (uint32_t)(cx + cy + 2);
    uint32_t clear_radius = ((uint32_t)(1000U - coverage) * radius_max) / 1000UL;
    uint32_t feather = (uint32_t)((radius_max / 12UL) + 2UL);
    uint32_t t = (uint32_t)tx_time_get();
    uint16_t by;
    uint16_t bx;

    renderFillRect(0U,
                   0U,
                   (uint16_t)RENDER_WIDTH,
                   (uint16_t)RENDER_HEIGHT,
                   RENDER_LAYER_UI,
                   RENDER_COLOR_BLACK);

    if (coverage >= 1000U)
    {
      return;
    }

    for (by = 0U; by < rows; by++)
    {
      for (bx = 0U; bx < cols; bx++)
      {
        uint16_t x = (uint16_t)(bx * block);
        uint16_t y = (uint16_t)(by * block);
        uint16_t w = block;
        uint16_t h = block;
        int32_t px = (int32_t)x + ((int32_t)w / 2);
        int32_t py = (int32_t)y + ((int32_t)h / 2);
        int32_t dx = px - cx;
        int32_t dy = py - cy;
        uint32_t dist = (uint32_t)((dx < 0) ? -dx : dx) + (uint32_t)((dy < 0) ? -dy : dy);
        uint32_t noise = (((uint32_t)bx * 1103515245UL) ^ ((uint32_t)by * 2246822519UL) ^
                          (t * 977UL)) % feather;

        if ((dist + noise) > clear_radius)
        {
          continue;
        }

        if ((uint16_t)(x + w) > (uint16_t)RENDER_WIDTH)
        {
          w = (uint16_t)((uint16_t)RENDER_WIDTH - x);
        }
        if ((uint16_t)(y + h) > (uint16_t)RENDER_HEIGHT)
        {
          h = (uint16_t)((uint16_t)RENDER_HEIGHT - y);
        }
        renderClearRectTransparent(x, y, w, h, RENDER_LAYER_UI);
      }
    }
    return;
  }

  if (g_game_rt_scene_transition.style == (uint8_t)APP_GAME_SCENE_TRANSITION_STYLE_SHOCKWAVE)
  {
    uint16_t block = (uint16_t)APP_GAME_SCENE_TRANSITION_BLOCK_PX;
    uint16_t cols = (uint16_t)(((uint16_t)RENDER_WIDTH + block - 1U) / block);
    uint16_t rows = (uint16_t)(((uint16_t)RENDER_HEIGHT + block - 1U) / block);
    int32_t cx = (int32_t)RENDER_WIDTH / 2;
    int32_t cy = (int32_t)RENDER_HEIGHT / 2;
    uint32_t radius_max = (uint32_t)(cx + cy + 2);
    uint32_t radius = ((uint32_t)coverage * radius_max) / 1000UL;
    uint32_t ring_w = (radius_max / 18UL) + 2UL;
    uint32_t t = (uint32_t)tx_time_get();
    uint16_t by;
    uint16_t bx;

    if (coverage >= 1000U)
    {
      renderFillRect(0U,
                     0U,
                     (uint16_t)RENDER_WIDTH,
                     (uint16_t)RENDER_HEIGHT,
                     RENDER_LAYER_UI,
                     RENDER_COLOR_BLACK);
      return;
    }

    for (by = 0U; by < rows; by++)
    {
      for (bx = 0U; bx < cols; bx++)
      {
        uint16_t x = (uint16_t)(bx * block);
        uint16_t y = (uint16_t)(by * block);
        uint16_t w = block;
        uint16_t h = block;
        int32_t px = (int32_t)x + ((int32_t)w / 2);
        int32_t py = (int32_t)y + ((int32_t)h / 2);
        int32_t dx = px - cx;
        int32_t dy = py - cy;
        uint32_t dist = (uint32_t)((dx < 0) ? -dx : dx) + (uint32_t)((dy < 0) ? -dy : dy);
        uint8_t fill = 0U;

        if (dist <= radius)
        {
          fill = 1U;
        }
        else if (dist <= (radius + ring_w))
        {
          uint32_t gate = (((uint32_t)bx * 2654435761UL) ^ ((uint32_t)by * 1597334677UL) ^
                           (t * 257UL)) % 1000UL;
          uint16_t ring_cov = (uint16_t)((coverage > 250U) ? (coverage - 250U) : 0U);
          fill = (gate <= ring_cov) ? 1U : 0U;
        }
        if (fill == 0U)
        {
          continue;
        }

        if ((uint16_t)(x + w) > (uint16_t)RENDER_WIDTH)
        {
          w = (uint16_t)((uint16_t)RENDER_WIDTH - x);
        }
        if ((uint16_t)(y + h) > (uint16_t)RENDER_HEIGHT)
        {
          h = (uint16_t)((uint16_t)RENDER_HEIGHT - y);
        }
        renderFillRect(x, y, w, h, RENDER_LAYER_UI, RENDER_COLOR_BLACK);
      }
    }
    return;
  }

  if (g_game_rt_scene_transition.style == (uint8_t)APP_GAME_SCENE_TRANSITION_STYLE_SCANLINE_GLITCH)
  {
    uint16_t stripe_h = 2U;
    uint16_t y = 0U;
    uint32_t t = (uint32_t)tx_time_get();

    if (coverage >= 1000U)
    {
      renderFillRect(0U,
                     0U,
                     (uint16_t)RENDER_WIDTH,
                     (uint16_t)RENDER_HEIGHT,
                     RENDER_LAYER_UI,
                     RENDER_COLOR_BLACK);
      return;
    }

    while (y < (uint16_t)RENDER_HEIGHT)
    {
      uint16_t h = stripe_h;
      uint32_t y_gate = ((uint32_t)y * 1000UL) / (uint32_t)((uint16_t)RENDER_HEIGHT == 0U ? 1U : (uint16_t)RENDER_HEIGHT);
      uint32_t jitter = ((t * 37UL) + ((uint32_t)y * 83UL)) % 240UL;
      uint32_t gate = y_gate + jitter;
      uint16_t cover_w = (uint16_t)RENDER_WIDTH;

      if ((uint16_t)(y + h) > (uint16_t)RENDER_HEIGHT)
      {
        h = (uint16_t)((uint16_t)RENDER_HEIGHT - y);
      }
      if (gate > 999UL)
      {
        gate = 999UL;
      }
      if (gate > (uint32_t)coverage)
      {
        y = (uint16_t)(y + h);
        continue;
      }

      if ((((y / stripe_h) + (uint16_t)(t & 0x7U)) & 0x3U) == 0U)
      {
        cover_w = (uint16_t)((uint16_t)RENDER_WIDTH - ((uint16_t)RENDER_WIDTH / 6U));
      }
      renderFillRect(0U, y, cover_w, h, RENDER_LAYER_UI, RENDER_COLOR_BLACK);
      y = (uint16_t)(y + h);
    }
    return;
  }

  if (g_game_rt_scene_transition.style == (uint8_t)APP_GAME_SCENE_TRANSITION_STYLE_VERTICAL_SLICES)
  {
    uint16_t slice_w = 4U;
    uint16_t x = 0U;
    uint32_t t = (uint32_t)tx_time_get();

    if (coverage == 0U)
    {
      return;
    }
    if (coverage >= 1000U)
    {
      renderFillRect(0U,
                     0U,
                     (uint16_t)RENDER_WIDTH,
                     (uint16_t)RENDER_HEIGHT,
                     RENDER_LAYER_UI,
                     RENDER_COLOR_BLACK);
      return;
    }

    while (x < (uint16_t)RENDER_WIDTH)
    {
      uint16_t w = slice_w;
      uint32_t hash = ((uint32_t)x * 2654435761UL) ^ (t * 977UL) ^ 0x9E3779B9UL;
      uint16_t bias;
      uint16_t local_cov;
      uint16_t cover_h;

      if ((uint16_t)(x + w) > (uint16_t)RENDER_WIDTH)
      {
        w = (uint16_t)((uint16_t)RENDER_WIDTH - x);
      }
      hash ^= (hash >> 16);
      hash *= 2246822519UL;
      hash ^= (hash >> 13);
      bias = (uint16_t)(hash % 260UL); /* staggered growth per slice */

      if ((uint32_t)coverage + (uint32_t)bias > 999UL)
      {
        local_cov = 999U;
      }
      else
      {
        local_cov = (uint16_t)((uint32_t)coverage + (uint32_t)bias);
      }
      cover_h = (uint16_t)(((uint32_t)RENDER_HEIGHT * (uint32_t)local_cov) / 1000UL);
      if (cover_h > 0U)
      {
        renderFillRect(x, 0U, w, cover_h, RENDER_LAYER_UI, RENDER_COLOR_BLACK);
      }
      x = (uint16_t)(x + w);
    }
    return;
  }

  if (g_game_rt_scene_transition.style == (uint8_t)APP_GAME_SCENE_TRANSITION_STYLE_STARBURST)
  {
    uint16_t block = (uint16_t)APP_GAME_SCENE_TRANSITION_BLOCK_PX;
    uint16_t cols = (uint16_t)(((uint16_t)RENDER_WIDTH + block - 1U) / block);
    uint16_t rows = (uint16_t)(((uint16_t)RENDER_HEIGHT + block - 1U) / block);
    int32_t cx = (int32_t)RENDER_WIDTH / 2;
    int32_t cy = (int32_t)RENDER_HEIGHT / 2;
    uint32_t radius_max = (uint32_t)(cx + cy + 2);
    uint32_t radius_allow = ((uint32_t)coverage * radius_max) / 1000UL;
    uint16_t spoke_allow = (uint16_t)(((uint32_t)coverage * 62UL) / 1000UL);
    uint16_t by;
    uint16_t bx;

    if (coverage >= 1000U)
    {
      renderFillRect(0U,
                     0U,
                     (uint16_t)RENDER_WIDTH,
                     (uint16_t)RENDER_HEIGHT,
                     RENDER_LAYER_UI,
                     RENDER_COLOR_BLACK);
      return;
    }

    for (by = 0U; by < rows; by++)
    {
      for (bx = 0U; bx < cols; bx++)
      {
        uint16_t x = (uint16_t)(bx * block);
        uint16_t y = (uint16_t)(by * block);
        uint16_t w = block;
        uint16_t h = block;
        int32_t px = (int32_t)x + ((int32_t)w / 2);
        int32_t py = (int32_t)y + ((int32_t)h / 2);
        int32_t dx = px - cx;
        int32_t dy = py - cy;
        uint32_t dist = (uint32_t)((dx < 0) ? -dx : dx) + (uint32_t)((dy < 0) ? -dy : dy);
        uint16_t angle = AppGameSceneTransitionAnglePermille(dx, dy);
        uint16_t spoke_phase = (uint16_t)(angle % 125U);
        uint16_t spoke_dist = (spoke_phase <= 62U) ? spoke_phase : (uint16_t)(125U - spoke_phase);

        if (dist > radius_allow)
        {
          continue;
        }
        if (spoke_dist > spoke_allow)
        {
          continue;
        }
        if ((uint16_t)(x + w) > (uint16_t)RENDER_WIDTH)
        {
          w = (uint16_t)((uint16_t)RENDER_WIDTH - x);
        }
        if ((uint16_t)(y + h) > (uint16_t)RENDER_HEIGHT)
        {
          h = (uint16_t)((uint16_t)RENDER_HEIGHT - y);
        }
        renderFillRect(x, y, w, h, RENDER_LAYER_UI, RENDER_COLOR_BLACK);
      }
    }
    return;
  }

  if (g_game_rt_scene_transition.style == (uint8_t)APP_GAME_SCENE_TRANSITION_STYLE_TV_SNOW)
  {
    uint16_t block = 2U;
    uint16_t cols = (uint16_t)(((uint16_t)RENDER_WIDTH + block - 1U) / block);
    uint16_t rows = (uint16_t)(((uint16_t)RENDER_HEIGHT + block - 1U) / block);
    uint32_t t = (uint32_t)tx_time_get();
    uint16_t by;
    uint16_t bx;

    if (coverage == 0U)
    {
      return;
    }
    if (coverage >= 1000U)
    {
      renderFillRect(0U,
                     0U,
                     (uint16_t)RENDER_WIDTH,
                     (uint16_t)RENDER_HEIGHT,
                     RENDER_LAYER_UI,
                     RENDER_COLOR_BLACK);
      return;
    }

    for (by = 0U; by < rows; by++)
    {
      for (bx = 0U; bx < cols; bx++)
      {
        uint32_t hash = ((uint32_t)bx * 1103515245UL) ^
                        ((uint32_t)by * 2246822519UL) ^
                        (t * 97531UL);
        uint16_t gate;
        uint16_t x = (uint16_t)(bx * block);
        uint16_t y = (uint16_t)(by * block);
        uint16_t w = block;
        uint16_t h = block;

        hash ^= (hash >> 16);
        hash *= 2654435761UL;
        hash ^= (hash >> 13);
        gate = (uint16_t)(hash % 1000UL);

        if ((((uint16_t)(by + (uint16_t)(t & 0x0FU))) & 0x7U) == 0U)
        {
          gate = (uint16_t)((gate > 120U) ? (gate - 120U) : 0U);
        }

        if (gate > coverage)
        {
          continue;
        }
        if ((uint16_t)(x + w) > (uint16_t)RENDER_WIDTH)
        {
          w = (uint16_t)((uint16_t)RENDER_WIDTH - x);
        }
        if ((uint16_t)(y + h) > (uint16_t)RENDER_HEIGHT)
        {
          h = (uint16_t)((uint16_t)RENDER_HEIGHT - y);
        }
        renderFillRect(x, y, w, h, RENDER_LAYER_UI, RENDER_COLOR_BLACK);
      }
    }
    return;
  }

  if (g_game_rt_scene_transition.style == (uint8_t)APP_GAME_SCENE_TRANSITION_STYLE_VHOLD_ROLL)
  {
    uint16_t stripe_h = 3U;
    uint16_t y = 0U;
    uint32_t t = (uint32_t)tx_time_get();
    uint16_t roll = (uint16_t)((t * 3UL) % (uint32_t)((uint16_t)RENDER_HEIGHT == 0U ? 1U : (uint16_t)RENDER_HEIGHT));

    if (coverage == 0U)
    {
      return;
    }
    if (coverage >= 1000U)
    {
      renderFillRect(0U,
                     0U,
                     (uint16_t)RENDER_WIDTH,
                     (uint16_t)RENDER_HEIGHT,
                     RENDER_LAYER_UI,
                     RENDER_COLOR_BLACK);
      return;
    }

    while (y < (uint16_t)RENDER_HEIGHT)
    {
      uint16_t h = stripe_h;
      uint16_t pos = (uint16_t)((y + roll) % (uint16_t)RENDER_HEIGHT);
      uint16_t gate = (uint16_t)(((uint32_t)pos * 1000UL) / (uint32_t)RENDER_HEIGHT);
      uint16_t jitter = (uint16_t)(((uint32_t)y * 43UL + t * 19UL) % 180UL);
      uint16_t local_gate;
      uint16_t cover_w;

      if ((uint16_t)(y + h) > (uint16_t)RENDER_HEIGHT)
      {
        h = (uint16_t)((uint16_t)RENDER_HEIGHT - y);
      }
      local_gate = (uint16_t)(gate + jitter);
      if (local_gate > 999U)
      {
        local_gate = 999U;
      }
      if (local_gate > coverage)
      {
        y = (uint16_t)(y + h);
        continue;
      }

      cover_w = (uint16_t)RENDER_WIDTH;
      if ((((uint16_t)(pos + (uint16_t)(t & 0x1FU))) & 0xFU) < 3U)
      {
        uint16_t cut = (uint16_t)((uint16_t)RENDER_WIDTH / 5U);
        cover_w = (uint16_t)((cover_w > cut) ? (cover_w - cut) : cover_w);
      }
      renderFillRect(0U, y, cover_w, h, RENDER_LAYER_UI, RENDER_COLOR_BLACK);
      y = (uint16_t)(y + h);
    }
    return;
  }

  if (g_game_rt_scene_transition.style == (uint8_t)APP_GAME_SCENE_TRANSITION_STYLE_SYNC_TEAR)
  {
    uint16_t stripe_h = 4U;
    uint16_t y = 0U;
    uint32_t t = (uint32_t)tx_time_get();
    uint16_t base_cover = (uint16_t)(((uint32_t)RENDER_WIDTH * (uint32_t)coverage) / 1000UL);

    if (coverage == 0U)
    {
      return;
    }
    if (coverage >= 1000U)
    {
      renderFillRect(0U,
                     0U,
                     (uint16_t)RENDER_WIDTH,
                     (uint16_t)RENDER_HEIGHT,
                     RENDER_LAYER_UI,
                     RENDER_COLOR_BLACK);
      return;
    }

    while (y < (uint16_t)RENDER_HEIGHT)
    {
      uint16_t h = stripe_h;
      uint32_t hash = ((uint32_t)y * 3266489917UL) ^ (t * 40503UL) ^ 0x85EBCA6BUL;
      uint16_t gate;
      uint16_t jitter;
      uint16_t cover_w;

      if ((uint16_t)(y + h) > (uint16_t)RENDER_HEIGHT)
      {
        h = (uint16_t)((uint16_t)RENDER_HEIGHT - y);
      }

      hash ^= (hash >> 16);
      hash *= 2246822519UL;
      hash ^= (hash >> 13);
      gate = (uint16_t)(hash % 1000UL);
      if (gate > coverage)
      {
        y = (uint16_t)(y + h);
        continue;
      }

      jitter = (uint16_t)(hash % (uint32_t)((uint16_t)RENDER_WIDTH / 3U + 1U));
      cover_w = (uint16_t)(base_cover + jitter);
      if (cover_w > (uint16_t)RENDER_WIDTH)
      {
        cover_w = (uint16_t)RENDER_WIDTH;
      }
      renderFillRect(0U, y, cover_w, h, RENDER_LAYER_UI, RENDER_COLOR_BLACK);

      if ((((hash >> 24) & 0x7UL) == 0UL) && (coverage > 260U))
      {
        renderFillRect(0U, y, (uint16_t)RENDER_WIDTH, h, RENDER_LAYER_UI, RENDER_COLOR_BLACK);
      }

      y = (uint16_t)(y + h);
    }
    return;
  }

  if (g_game_rt_scene_transition.style == (uint8_t)APP_GAME_SCENE_TRANSITION_STYLE_CRT_CHANNEL_TUNE)
  {
    uint16_t block = 2U;
    uint16_t cols = (uint16_t)(((uint16_t)RENDER_WIDTH + block - 1U) / block);
    uint16_t rows = (uint16_t)(((uint16_t)RENDER_HEIGHT + block - 1U) / block);
    uint32_t t = (uint32_t)tx_time_get();
    uint16_t tune_strength;
    uint16_t snow_density;
    uint16_t tear_y;
    uint16_t tear_h;
    uint16_t by;
    uint16_t bx;

    if (coverage == 0U)
    {
      return;
    }
    if (coverage >= 1000U)
    {
      renderFillRect(0U,
                     0U,
                     (uint16_t)RENDER_WIDTH,
                     (uint16_t)RENDER_HEIGHT,
                     RENDER_LAYER_UI,
                     RENDER_COLOR_BLACK);
      return;
    }

    if (coverage <= 500U)
    {
      tune_strength = (uint16_t)((uint32_t)coverage * 2UL);
    }
    else
    {
      tune_strength = (uint16_t)(((uint32_t)1000UL - (uint32_t)coverage) * 2UL);
    }

    snow_density = (uint16_t)(120U + (((uint32_t)tune_strength * 760UL) / 1000UL));
    tear_y = (uint16_t)(((t * 5UL) + (((uint32_t)coverage * (uint32_t)RENDER_HEIGHT) / 1000UL)) %
                        (uint32_t)((uint16_t)RENDER_HEIGHT == 0U ? 1U : (uint16_t)RENDER_HEIGHT));
    tear_h = (uint16_t)(4U + (((uint32_t)tune_strength * 10UL) / 1000UL));

    for (by = 0U; by < rows; by++)
    {
      for (bx = 0U; bx < cols; bx++)
      {
        uint16_t x = (uint16_t)(bx * block);
        uint16_t y = (uint16_t)(by * block);
        uint16_t w = block;
        uint16_t h = block;
        uint16_t y_center = (uint16_t)(y + (h / 2U));
        uint16_t band_dist = (y_center >= tear_y) ? (uint16_t)(y_center - tear_y) : (uint16_t)(tear_y - y_center);
        uint32_t hash = ((uint32_t)bx * 3266489917UL) ^
                        ((uint32_t)by * 668265263UL) ^
                        (t * 40503UL) ^
                        0xA53A9E11UL;
        uint16_t gate;
        uint16_t local_density = snow_density;

        hash ^= (hash >> 16);
        hash *= 2246822519UL;
        hash ^= (hash >> 13);
        gate = (uint16_t)(hash % 1000UL);

        if (band_dist < (uint16_t)(tear_h * 2U))
        {
          uint16_t bonus = (uint16_t)(((uint16_t)(tear_h * 2U) - band_dist) * 42U);
          if ((uint16_t)(local_density + bonus) > 999U)
          {
            local_density = 999U;
          }
          else
          {
            local_density = (uint16_t)(local_density + bonus);
          }
        }

        if (gate > local_density)
        {
          continue;
        }
        if ((uint16_t)(x + w) > (uint16_t)RENDER_WIDTH)
        {
          w = (uint16_t)((uint16_t)RENDER_WIDTH - x);
        }
        if ((uint16_t)(y + h) > (uint16_t)RENDER_HEIGHT)
        {
          h = (uint16_t)((uint16_t)RENDER_HEIGHT - y);
        }
        renderFillRect(x, y, w, h, RENDER_LAYER_UI, RENDER_COLOR_BLACK);
      }
    }

    {
      uint16_t glitch_h = 2U;
      uint16_t gy = (tear_y >= tear_h) ? (uint16_t)(tear_y - tear_h) : 0U;
      while (gy < (uint16_t)RENDER_HEIGHT)
      {
        uint16_t seg_w = 8U;
        uint16_t gx = 0U;
        if (gy > (uint16_t)(tear_y + tear_h))
        {
          break;
        }
        while (gx < (uint16_t)RENDER_WIDTH)
        {
          uint32_t hash = ((uint32_t)gx * 1103515245UL) ^
                          ((uint32_t)gy * 2654435761UL) ^
                          (t * 977UL);
          uint16_t gate = (uint16_t)(hash % 1000UL);
          uint16_t w = seg_w;
          if ((uint16_t)(gx + w) > (uint16_t)RENDER_WIDTH)
          {
            w = (uint16_t)((uint16_t)RENDER_WIDTH - gx);
          }
          if (gate < tune_strength)
          {
            renderClearRectTransparent(gx, gy, w, glitch_h, RENDER_LAYER_UI);
          }
          gx = (uint16_t)(gx + w);
        }
        gy = (uint16_t)(gy + glitch_h);
      }
    }
    return;
  }

  {
    uint16_t block = (uint16_t)APP_GAME_SCENE_TRANSITION_BLOCK_PX;
    uint16_t cols = (uint16_t)(((uint16_t)RENDER_WIDTH + block - 1U) / block);
    uint16_t rows = (uint16_t)(((uint16_t)RENDER_HEIGHT + block - 1U) / block);
    uint16_t denom = (uint16_t)((cols + rows) - 1U);
    uint16_t by;
    uint16_t bx;

    if (denom == 0U)
    {
      denom = 1U;
    }

    for (by = 0U; by < rows; by++)
    {
      for (bx = 0U; bx < cols; bx++)
      {
        uint16_t rank = (uint16_t)(bx + by);
        uint16_t gate = (uint16_t)(((uint32_t)(rank + 1U) * 1000UL) / (uint32_t)denom);
        if (gate > coverage)
        {
          continue;
        }

        {
          uint16_t x = (uint16_t)(bx * block);
          uint16_t y = (uint16_t)(by * block);
          uint16_t w = block;
          uint16_t h = block;
          if ((uint16_t)(x + w) > (uint16_t)RENDER_WIDTH)
          {
            w = (uint16_t)((uint16_t)RENDER_WIDTH - x);
          }
          if ((uint16_t)(y + h) > (uint16_t)RENDER_HEIGHT)
          {
            h = (uint16_t)((uint16_t)RENDER_HEIGHT - y);
          }
          renderFillRect(x, y, w, h, RENDER_LAYER_UI, RENDER_COLOR_BLACK);
        }
      }
    }
  }
}

static VOID AppGameTryApplyRetainedRestore(void)
{
  const game_map_view_t *map_view;

  if ((g_game_rt_restore_pending == 0U) || (g_game_rt_restore_applied != 0U))
  {
    return;
  }

  map_view = GameRuntime_GetSceneMap();
  if ((map_view == (const game_map_view_t *)0) ||
      (map_view->header == (const game_map_blob_header_t *)0))
  {
    return;
  }

  if (GameModeTopdownBasic_LoadSnapshot(&g_game_rt_restore_snapshot) != 0U)
  {
    g_game_rt_restore_applied = 1U;
    g_game_rt_restore_pending = 0U;
    return;
  }

  GameModeTopdownBasic_RequestSpawn(0UL, 0U);
  g_game_rt_restore_applied = 1U;
  g_game_rt_restore_pending = 0U;
}

static VOID AppGameTryApplyPendingTransitionSpawn(void)
{
  app_storage_scene_load_status_t status_now;
  const game_map_view_t *map_view;
  uint8_t map_done;
  uint8_t tileset_done;
  uint8_t map_failed;
  uint8_t tileset_failed;

  if (g_game_rt_transition_spawn_pending == 0U)
  {
    return;
  }

  if (g_game_rt_scene_swap_load_status_base_valid == 0U)
  {
    return;
  }
  if (App_StorageSceneLoadStatusGet(&status_now) != TX_SUCCESS)
  {
    return;
  }

  map_done = (status_now.map_ok_count > g_game_rt_scene_swap_load_status_base.map_ok_count) ? 1U : 0U;
  tileset_done = (status_now.tileset_ok_count > g_game_rt_scene_swap_load_status_base.tileset_ok_count) ? 1U : 0U;
  map_failed = (status_now.map_fail_count > g_game_rt_scene_swap_load_status_base.map_fail_count) ? 1U : 0U;
  tileset_failed = (status_now.tileset_fail_count > g_game_rt_scene_swap_load_status_base.tileset_fail_count) ? 1U : 0U;

  if ((map_failed != 0U) || (tileset_failed != 0U))
  {
    g_game_rt_transition_spawn_pending = 0U;
    g_game_rt_transition_spawn_hash = 0UL;
    g_game_rt_scene_swap_load_status_base_valid = 0U;
    (void)memset(&g_game_rt_scene_swap_load_status_base, 0, sizeof(g_game_rt_scene_swap_load_status_base));
    return;
  }
  if ((map_done == 0U) || (tileset_done == 0U))
  {
    return;
  }

  map_view = GameRuntime_GetSceneMap();
  if ((map_view == (const game_map_view_t *)0) ||
      (map_view->header == (const game_map_blob_header_t *)0))
  {
    return;
  }

  GameModeTopdownBasic_RequestSpawn(g_game_rt_transition_spawn_hash,
                                    (g_game_rt_transition_spawn_hash != 0UL) ? 1U : 0U);
  g_game_rt_transition_spawn_pending = 0U;
  g_game_rt_scene_swap_load_status_base_valid = 0U;
  (void)memset(&g_game_rt_scene_swap_load_status_base, 0, sizeof(g_game_rt_scene_swap_load_status_base));
  g_game_rt_transition_prev_map_crc32 = map_view->header->crc32;
  g_game_rt_transition_spawn_hash = 0UL;
}

static VOID AppGameHandlePendingSceneTransition(void)
{
  game_runtime_scene_transition_req_t req;
  uint32_t scene_map_id = 0UL;
  uint32_t scene_tileset_id = 0UL;
  uint32_t map_addr = 0UL;
  uint32_t map_size = 0UL;
  uint32_t tileset_addr = 0UL;
  uint32_t tileset_size = 0UL;

  if (GameRuntime_ConsumeSceneTransition(&req) == 0U)
  {
    return;
  }
  if (req.target_map_hash == 0UL)
  {
    return;
  }
  if (AppGameResolveSceneIdsByTargetMapHash(req.target_map_hash,
                                            &scene_map_id,
                                            &scene_tileset_id) == 0U)
  {
    return;
  }

  if (scene_map_id == g_game_rt_scene_map_id)
  {
    GameModeTopdownBasic_RequestSpawn(req.target_spawn_hash,
                                      (req.target_spawn_hash != 0UL) ? 1U : 0U);
    return;
  }

  if (AppGameSceneTransitionActive() != 0U)
  {
    return;
  }

  if ((AppGameResolveManifestSceneMapRef(scene_map_id, &map_addr, &map_size) == 0U) ||
      (AppGameResolveManifestSceneTilesetRef(scene_tileset_id, &tileset_addr, &tileset_size) == 0U))
  {
    return;
  }
  if ((map_addr == 0UL) || (tileset_addr == 0UL))
  {
    return;
  }

  AppGameSceneTransitionQueueSwap(scene_map_id,
                                  scene_tileset_id,
                                  map_addr,
                                  map_size,
                                  tileset_addr,
                                  tileset_size,
                                  req.target_spawn_hash);
  AppGameSceneTransitionBeginOut(
      AppGameSceneTransitionSelectStyle((uint32_t)KNOB_GAME_RT_SCENE_TRANSITION_CROSS_STYLE));
}

static app_audio_asset_id_t AppGameResolveManifestRefFallbackAssetId(uint32_t manifest_asset_id)
{
  switch (manifest_asset_id)
  {
    case 3001UL: /* music */
      return (app_audio_asset_id_t)KNOB_AUDIO_MUSIC_LOOP_CLIP;

    case 3002UL: /* interact */
      return (app_audio_asset_id_t)KNOB_AUDIO_MAP_GAME_ACTION_CLIP;

    case 3003UL: /* confirm */
      return (app_audio_asset_id_t)KNOB_AUDIO_MAP_RT_PRIMARY_CLIP;

    case 3004UL: /* error */
      return (app_audio_asset_id_t)KNOB_AUDIO_MAP_RT_BACK_CLIP;

    default:
      break;
  }

  return APP_AUDIO_ASSET_NONE;
}

static app_audio_asset_id_t AppGameResolveManifestAssetId(uint32_t manifest_asset_id)
{
  app_audio_asset_id_t asset_id = (app_audio_asset_id_t)manifest_asset_id;
  app_audio_catalog_entry_t entry;

  if ((manifest_asset_id == 0UL) || (asset_id == APP_AUDIO_ASSET_NONE))
  {
    return APP_AUDIO_ASSET_NONE;
  }

  if (AppAudioCatalogResolve(asset_id, &entry) != TX_SUCCESS)
  {
    asset_id = AppGameResolveManifestRefFallbackAssetId(manifest_asset_id);
    if ((asset_id == APP_AUDIO_ASSET_NONE) ||
        (AppAudioCatalogResolve(asset_id, &entry) != TX_SUCCESS))
    {
      return APP_AUDIO_ASSET_NONE;
    }
  }

  return asset_id;
}

static VOID AppGameResetRealtimeAudioBindings(void)
{
  g_game_rt_music_asset = APP_AUDIO_ASSET_NONE;
  g_game_rt_sfx_interact_asset = APP_AUDIO_ASSET_NONE;
  g_game_rt_sfx_confirm_asset = APP_AUDIO_ASSET_NONE;
  g_game_rt_sfx_error_asset = APP_AUDIO_ASSET_NONE;
  g_game_rt_music_started = 0U;
}

static app_audio_asset_id_t AppGameResolveRealtimeCueFallbackAssetId(game_runtime_audio_cue_t cue)
{
  app_audio_asset_id_t asset_id = APP_AUDIO_ASSET_NONE;

  switch (cue)
  {
    case GAME_RT_AUDIO_CUE_MOVE:
      asset_id = (app_audio_asset_id_t)KNOB_AUDIO_MAP_RT_MOVE_CLIP;
      break;

    case GAME_RT_AUDIO_CUE_PRIMARY:
      asset_id = (app_audio_asset_id_t)KNOB_AUDIO_MAP_RT_PRIMARY_CLIP;
      break;

    case GAME_RT_AUDIO_CUE_SECONDARY:
      asset_id = (app_audio_asset_id_t)KNOB_AUDIO_MAP_RT_SECONDARY_CLIP;
      break;

    case GAME_RT_AUDIO_CUE_BACK:
      asset_id = (app_audio_asset_id_t)KNOB_AUDIO_MAP_RT_BACK_CLIP;
      break;

    case GAME_RT_AUDIO_CUE_GAME_ACTION:
      asset_id = (app_audio_asset_id_t)KNOB_AUDIO_MAP_GAME_ACTION_CLIP;
      break;

    default:
      break;
  }

  if ((asset_id != APP_AUDIO_ASSET_NONE) &&
      (AppAudioAssets_IsValidId((uint32_t)asset_id) != 0U))
  {
    return asset_id;
  }

  return APP_AUDIO_ASSET_NONE;
}

static uint8_t AppGameResolveRealtimeCueAssetId(game_runtime_audio_cue_t cue,
                                                app_audio_asset_id_t *asset_id_out)
{
  app_audio_asset_id_t asset_id = APP_AUDIO_ASSET_NONE;

  if (asset_id_out == TX_NULL)
  {
    return 0U;
  }

  switch (cue)
  {
    case GAME_RT_AUDIO_CUE_PRIMARY:
      asset_id = g_game_rt_sfx_confirm_asset;
      break;

    case GAME_RT_AUDIO_CUE_SECONDARY:
      asset_id = g_game_rt_sfx_interact_asset;
      break;

    case GAME_RT_AUDIO_CUE_BACK:
      asset_id = g_game_rt_sfx_error_asset;
      break;

    case GAME_RT_AUDIO_CUE_GAME_ACTION:
      asset_id = g_game_rt_sfx_interact_asset;
      break;

    default:
      break;
  }

  if (asset_id == APP_AUDIO_ASSET_NONE)
  {
    asset_id = AppGameResolveRealtimeCueFallbackAssetId(cue);
  }

  if (asset_id != APP_AUDIO_ASSET_NONE)
  {
    *asset_id_out = asset_id;
    return 1U;
  }

  *asset_id_out = APP_AUDIO_ASSET_NONE;
  return 0U;
}

static VOID AppGameDrainQueuedEvents(void)
{
  app_input_action_evt_t evt;
  UINT status;
  ULONG drained = 0UL;
  ULONG max_drain = (ULONG)KNOB_RTOS_QGAME_EVENTS_DEPTH;

  if (max_drain == 0UL)
  {
    max_drain = 1UL;
  }

  while (drained < max_drain)
  {
    status = tx_queue_receive(&g_q_game_events, &evt, TX_NO_WAIT);
    if (status != TX_SUCCESS)
    {
      break;
    }

    g_game_event_recv_count++;
    g_game_event_last_action = evt.action;
    if (g_game_event_ignored_count < 0xFFFFFFFFUL)
    {
      g_game_event_ignored_count++;
    }
    if (g_game_event_stale_drop_count < 0xFFFFFFFFUL)
    {
      g_game_event_stale_drop_count++;
    }
    drained++;
  }
}

static uint8_t AppGameHandleRealtimeControlEvent(const app_input_action_evt_t *evt)
{
  ULONG mode_flags;
  uint8_t request_exit_to_static = 0U;
  uint8_t handled;
  UINT status;
  app_mode_t exit_mode = APP_MODE_STATIC;
  game_runtime_input_t input;
  game_runtime_audio_cue_t audio_cue = GAME_RT_AUDIO_CUE_NONE;
  app_audio_asset_id_t asset_id = APP_AUDIO_ASSET_NONE;

  if (evt == TX_NULL)
  {
    return 0U;
  }

  /* thGame semantic handling is REALTIME-only by ownership contract. */
  mode_flags = (g_eg_mode.tx_event_flags_group_current & APP_MODE_FLAGS_ALL);
  if ((mode_flags & APP_MODE_FLAG_REALTIME) == 0UL)
  {
    return 0U;
  }

  input.source = evt->source;
  input.event = evt->event;
  input.tick = evt->tick;
  input.pressed_mask = evt->pressed_mask;

  handled = GameRuntime_HandleControl(&input, &request_exit_to_static, &audio_cue);
  if (handled == 0U)
  {
    return 0U;
  }

  if (AppGameResolveRealtimeCueAssetId(audio_cue, &asset_id) != 0U)
  {
    if (asset_id != APP_AUDIO_ASSET_NONE)
    {
      (void)App_AudioReq_PlayAsset(asset_id);
    }
  }

  if (request_exit_to_static != 0U)
  {
    if (GameRuntime_GetActiveBackendId() == (uint32_t)GAME_RUNTIME_BACKEND_RENDER_DEMO_TITLE_ANIM)
    {
      exit_mode = APP_MODE_STOP;
    }

    status = App_SysEvent_ModeSet(exit_mode);
    if (status == TX_SUCCESS)
    {
      g_game_exit_to_static_pending = 0UL;
    }
    else
    {
      g_game_exit_to_static_pending = 1UL;
    }
  }

  return 1U;
}

static VOID AppGameThreadEntry(ULONG thread_input)
{
  UINT status;
  app_input_action_evt_t evt;
  ULONG sim_period_ticks = 1UL;
  ULONG render_period_ticks = 1UL;
  ULONG sim_dt_ms = 1UL;
  ULONG next_sim_tick = 0UL;
  ULONG next_render_tick = 0UL;
  ULONG max_catchup_steps = 1UL;
  ULONG draw_t0;
  ULONG draw_t1;
  uint8_t realtime_active = 0U;
  uint8_t realtime_lis_stream_active = 0U;

  (void)thread_input;

  if ((ULONG)KNOB_GAME_RT_SIM_HZ > 0UL)
  {
    sim_period_ticks = ((ULONG)TX_TIMER_TICKS_PER_SECOND / (ULONG)KNOB_GAME_RT_SIM_HZ);
    if (sim_period_ticks == 0UL)
    {
      sim_period_ticks = 1UL;
    }
  }
  sim_dt_ms = (1000UL * sim_period_ticks) / (ULONG)TX_TIMER_TICKS_PER_SECOND;
  if (sim_dt_ms == 0UL)
  {
    sim_dt_ms = 1UL;
  }

  if ((ULONG)KNOB_GAME_RT_FPS_TARGET > 0UL)
  {
    render_period_ticks = ((ULONG)TX_TIMER_TICKS_PER_SECOND / (ULONG)KNOB_GAME_RT_FPS_TARGET);
    if (render_period_ticks == 0UL)
    {
      render_period_ticks = 1UL;
    }
  }

  max_catchup_steps = (ULONG)KNOB_GAME_RT_MAX_CATCHUP_STEPS;
  if (max_catchup_steps == 0UL)
  {
    max_catchup_steps = 1UL;
  }

  for (;;)
  {
    ULONG mode_flags = (g_eg_mode.tx_event_flags_group_current & APP_MODE_FLAGS_ALL);
    if ((mode_flags & APP_MODE_FLAG_REALTIME) != 0UL)
    {
      ULONG now_tick;
      ULONG due_tick;
      ULONG delta_ticks;
      ULONG sim_steps = 0UL;
      UINT wait_ticks;
      UINT recv_i;
      app_sensor_snapshot_t sensor_snapshot;
      uint8_t have_sensor_snapshot = 0U;

      GameRuntime_ProcessPendingDebugTune();

      if (g_game_exit_to_static_pending != 0UL)
      {
        if (App_SysEvent_ModeSet(APP_MODE_STATIC) == TX_SUCCESS)
        {
          g_game_exit_to_static_pending = 0UL;
        }
      }

      if (realtime_active == 0U)
      {
        const game_package_runtime_config_t *rt_cfg;
        uint32_t backend_id;
        uint32_t mode_id;
        UINT audio_status;

        /* Drop stale events queued before REALTIME ownership became active. */
        AppGameDrainQueuedEvents();
        realtime_active = 1U;
        AppGameResetRealtimeAudioBindings();
        AppGameResetRealtimeSceneBindings();
        if (realtime_lis_stream_active == 0U)
        {
          if (App_SensorReq_LisStreamStart() == TX_SUCCESS)
          {
            realtime_lis_stream_active = 1U;
          }
        }
        GameRuntime_Init();
        backend_id = GameRuntime_GetActiveBackendId();
        mode_id = GameRuntime_GetActiveModeId();
        if ((backend_id == (uint32_t)GAME_RUNTIME_BACKEND_TOPDOWN_BASIC) &&
            (mode_id != 0UL))
        {
          if (AppGameRetainedSnapshotModeAllowed(mode_id, backend_id) != 0U)
          {
            game_mode_topdown_basic_snapshot_t snapshot;
            uint32_t restore_scene_map_id = 0UL;
            uint32_t restore_scene_tileset_id = 0UL;

            if (AppRetainedStateRestoreGameTopdown(mode_id,
                                                   backend_id,
                                                   &snapshot,
                                                   &restore_scene_map_id,
                                                   &restore_scene_tileset_id) == TX_SUCCESS)
            {
              g_game_rt_restore_snapshot = snapshot;
              g_game_rt_restore_pending = 1U;
              g_game_rt_restore_applied = 0U;
              g_game_rt_restore_scene_map_id = restore_scene_map_id;
              g_game_rt_restore_scene_tileset_id = restore_scene_tileset_id;
            }
            else
            {
              GameModeTopdownBasic_RequestSpawn(0UL, 0U);
              g_game_rt_restore_pending = 0U;
              g_game_rt_restore_applied = 1U;
              g_game_rt_restore_scene_map_id = 0UL;
              g_game_rt_restore_scene_tileset_id = 0UL;
            }
          }
          else
          {
            GameModeTopdownBasic_RequestSpawn(0UL, 0U);
            g_game_rt_restore_pending = 0U;
            g_game_rt_restore_applied = 1U;
            g_game_rt_restore_scene_map_id = 0UL;
            g_game_rt_restore_scene_tileset_id = 0UL;
          }
        }
        rt_cfg = GameRuntime_GetActiveModeConfig();
        if (rt_cfg != (const game_package_runtime_config_t *)0)
        {
          AppGameResolveRealtimeSceneBindings(rt_cfg);
          if ((g_game_rt_restore_pending != 0U) &&
              (AppGameApplyRetainedSceneBindings(g_game_rt_restore_scene_map_id,
                                                 g_game_rt_restore_scene_tileset_id) != 0U))
          {
            /* Retained scene bindings now override default mode bindings. */
          }
          if (g_game_rt_scene_tileset_addr != 0UL)
          {
            (void)App_StorageReq_SceneTilesetLoad((ULONG)g_game_rt_scene_tileset_addr,
                                                  (ULONG)g_game_rt_scene_tileset_size);
          }
          if (g_game_rt_scene_map_addr != 0UL)
          {
            (void)App_StorageReq_SceneMapLoad((ULONG)g_game_rt_scene_map_addr,
                                              (ULONG)g_game_rt_scene_map_size);
          }
          g_game_rt_music_asset = AppGameResolveManifestAssetId(rt_cfg->music_asset_id);
          g_game_rt_sfx_interact_asset = AppGameResolveManifestAssetId(rt_cfg->sfx_interact_asset_id);
          g_game_rt_sfx_confirm_asset = AppGameResolveManifestAssetId(rt_cfg->sfx_confirm_asset_id);
          g_game_rt_sfx_error_asset = AppGameResolveManifestAssetId(rt_cfg->sfx_error_asset_id);
        }

        if (g_game_rt_music_asset != APP_AUDIO_ASSET_NONE)
        {
          audio_status = App_AudioReq_PlayAsset(g_game_rt_music_asset);
          if (audio_status == TX_SUCCESS)
          {
            g_game_rt_music_started = 1U;
          }
        }
        now_tick = tx_time_get();
        next_sim_tick = now_tick + sim_period_ticks;
        next_render_tick = now_tick + render_period_ticks;

        if (backend_id == (uint32_t)GAME_RUNTIME_BACKEND_TOPDOWN_BASIC)
        {
          AppGameSceneTransitionBeginIn(
              AppGameSceneTransitionSelectStyle((uint32_t)KNOB_GAME_RT_SCENE_TRANSITION_ENTRY_STYLE));
        }
      }

      now_tick = tx_time_get();
      due_tick = next_sim_tick;
      if ((LONG)(next_render_tick - due_tick) < 0L)
      {
        due_tick = next_render_tick;
      }
      if ((LONG)(now_tick - due_tick) < 0L)
      {
        delta_ticks = (ULONG)(due_tick - now_tick);
        wait_ticks = (UINT)delta_ticks;
        if (wait_ticks == 0U)
        {
          wait_ticks = 1U;
        }
      }
      else
      {
        wait_ticks = TX_NO_WAIT;
      }

      status = tx_queue_receive(&g_q_game_events, &evt, wait_ticks);
      if (status == TX_SUCCESS)
      {
        g_game_event_recv_count++;
        g_game_event_last_action = evt.action;
        if (AppGameHandleRealtimeControlEvent(&evt) != 0U)
        {
          g_game_event_handled_count++;
        }
        else
        {
          g_game_event_ignored_count++;
        }

        for (recv_i = 0U; recv_i < 7U; ++recv_i)
        {
          /* Preserve sim/render cadence under bursty input by yielding when due. */
          now_tick = tx_time_get();
          if ((LONG)(now_tick - due_tick) >= 0L)
          {
            break;
          }

          status = tx_queue_receive(&g_q_game_events, &evt, TX_NO_WAIT);
          if (status != TX_SUCCESS)
          {
            break;
          }

          g_game_event_recv_count++;
          g_game_event_last_action = evt.action;
          if (AppGameHandleRealtimeControlEvent(&evt) != 0U)
          {
            g_game_event_handled_count++;
          }
          else
          {
            g_game_event_ignored_count++;
          }
        }
      }
      else if ((status != TX_QUEUE_EMPTY) && (status != TX_NO_EVENTS))
      {
        g_game_event_queue_error_count++;
      }

      AppGameHandlePendingSceneTransition();
      AppGameTryApplyPendingTransitionSpawn();
      AppGameSceneTransitionPump();
      AppGameTryApplyRetainedRestore();

      now_tick = tx_time_get();
      while (((LONG)(now_tick - next_sim_tick) >= 0L) && (sim_steps < max_catchup_steps))
      {
        if (have_sensor_snapshot == 0U)
        {
          if (App_SensorSnapshot_Get(&sensor_snapshot) == TX_SUCCESS)
          {
            have_sensor_snapshot = 1U;
          }
        }
        if (AppGameSceneTransitionActive() == 0U)
        {
          GameRuntime_Update((have_sensor_snapshot != 0U) ? &sensor_snapshot : (const app_sensor_snapshot_t *)0,
                             (uint32_t)sim_dt_ms);
        }
        else if (GameRuntime_GetActiveBackendId() == (uint32_t)GAME_RUNTIME_BACKEND_TOPDOWN_BASIC)
        {
          /*
           * During scene transitions, keep gameplay motion frozen but still
           * run topdown swap/camera bookkeeping so map cache invalidation and
           * spawn handoff complete while screen is obscured.
           */
          GameRuntime_Update((have_sensor_snapshot != 0U) ? &sensor_snapshot : (const app_sensor_snapshot_t *)0,
                             0UL);
        }
        next_sim_tick += sim_period_ticks;
        sim_steps++;
      }
      if ((LONG)(now_tick - next_sim_tick) >= 0L)
      {
        next_sim_tick = now_tick + sim_period_ticks;
      }

      if ((LONG)(now_tick - next_render_tick) >= 0L)
      {
        if (have_sensor_snapshot == 0U)
        {
          if (App_SensorSnapshot_Get(&sensor_snapshot) == TX_SUCCESS)
          {
            have_sensor_snapshot = 1U;
          }
        }
        draw_t0 = tx_time_get();
        if (AppRendererLock() == TX_SUCCESS)
        {
          if ((GameRuntime_GetActiveBackendId() == (uint32_t)GAME_RUNTIME_BACKEND_TOPDOWN_BASIC) &&
              (g_game_rt_scene_transition_overlay_clear_pending != 0U))
          {
            renderClearRectTransparent(0U,
                                       0U,
                                       (uint16_t)RENDER_WIDTH,
                                       (uint16_t)RENDER_HEIGHT,
                                       RENDER_LAYER_UI);
            g_game_rt_scene_transition_overlay_clear_pending = 0U;
          }

          GameRuntime_DrawFrame((have_sensor_snapshot != 0U) ? &sensor_snapshot : (const app_sensor_snapshot_t *)0);
          if ((GameRuntime_GetActiveBackendId() == (uint32_t)GAME_RUNTIME_BACKEND_TOPDOWN_BASIC) &&
              (AppGameSceneTransitionActive() != 0U))
          {
            AppGameDrawSceneTransitionOverlay();
          }
          AppRendererUnlock();
          draw_t1 = tx_time_get();
          g_power_perf_last_draw_ticks = (ULONG)(draw_t1 - draw_t0);
          (void)App_Display_Present();
        }

        next_render_tick += render_period_ticks;
        if ((LONG)(now_tick - next_render_tick) >= 0L)
        {
          next_render_tick = now_tick + render_period_ticks;
        }
      }
      continue;
    }

    if (realtime_active != 0U)
    {
      uint32_t backend_id = GameRuntime_GetActiveBackendId();
      uint32_t mode_id = GameRuntime_GetActiveModeId();

      if (AppGameRetainedSnapshotModeAllowed(mode_id, backend_id) != 0U)
      {
        game_mode_topdown_basic_snapshot_t snapshot;
        if (GameModeTopdownBasic_SaveSnapshot(&snapshot) != 0U)
        {
          (void)AppRetainedStateSaveGameTopdown(mode_id,
                                                backend_id,
                                                g_game_rt_scene_map_id,
                                                g_game_rt_scene_tileset_id,
                                                &snapshot);
        }
        else
        {
          (void)AppRetainedStateClearGame();
          if (g_retained_state_game_save_fail_count < 0xFFFFFFFFUL)
          {
            g_retained_state_game_save_fail_count++;
          }
        }
      }

      realtime_active = 0U;
      if (g_game_rt_music_started != 0U)
      {
        (void)App_AudioReq_Stop();
      }
      AppGameResetRealtimeAudioBindings();
      AppGameResetRealtimeSceneBindings();
      if (realtime_lis_stream_active != 0U)
      {
        (void)App_SensorReq_LisStreamStop();
        realtime_lis_stream_active = 0U;
      }
      GameRuntime_Shutdown();
      g_game_exit_to_static_pending = 0UL;
      /* Drop stale events after shutdown while game semantics are inactive. */
      AppGameDrainQueuedEvents();
    }

    tx_thread_sleep(KNOB_RTOS_GAME_WAIT_TICKS);
  }
}
