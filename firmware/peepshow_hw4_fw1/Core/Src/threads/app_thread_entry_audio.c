/* Thread entry implementation for App_ThreadX runtime. */

/* Audio helpers. */
static VOID AppAudioVoiceDecodeReset(app_audio_voice_t *voice);
static VOID AppAudioFillFrames(ULONG frame_offset, ULONG frame_count);
static VOID AppAudioVoiceReset(app_audio_voice_t *voice);
static VOID AppAudioVoiceStart(app_audio_voice_t *voice, const app_audio_adpcm_clip_t *clip, uint8_t loop);
static VOID AppAudioVoiceStartExternal(app_audio_voice_t *voice, const app_audio_catalog_entry_t *entry, uint8_t loop);
static UINT AppAudioStorageReadExternalBlock(app_audio_voice_t *voice, ULONG offset, ULONG len);
static UINT AppAudioStoragePrefetchExternalBlock(app_audio_voice_t *voice, ULONG offset, ULONG len);
static uint8_t AppAudioAnyVoiceActive(void);
static ULONG AppAudioSfxActiveCount(void);
static uint8_t AppAudioVoiceFetchSample(app_audio_voice_t *voice, int16_t *sample_out);
static UINT AppAudioAdpcmDecodeNext(app_audio_voice_t *voice, int16_t *sample_out);
static int16_t AppAudioAdpcmDecodeNibble(app_audio_adpcm_decode_t *decode, uint8_t nibble);
static UINT AppAudioStartStream(void);
static UINT AppAudioStartTone(void);
static UINT AppAudioStop(void);
static UINT AppAudioCatalogResolveExternal(app_audio_asset_id_t asset_id, app_audio_catalog_entry_t *entry_out);
static UINT AppAudioCatalogResolve(app_audio_asset_id_t asset_id, app_audio_catalog_entry_t *entry_out);
static UINT AppAudioPlayAsset(app_audio_asset_id_t asset_id);
static VOID AppAudioProcessDmaEvents(void);
static VOID AppAudioProcessAutoStop(void);
static VOID AppAudioPowerBoostUpdate(void);
static app_audio_asset_id_t AppAudioResolveEventAsset(app_audio_event_t event_id);
static app_audio_gain_class_t AppAudioResolveEventGainClass(app_audio_event_t event_id);
static ULONG AppAudioClampUserGainPct(ULONG pct);

static ULONG AppAudioClampUserGainPct(ULONG pct)
{
  if (pct > APP_STORAGE_USER_GAIN_MAX_PCT)
  {
    return APP_STORAGE_USER_GAIN_MAX_PCT;
  }
  return pct;
}

static uint8_t AppAudioPlaybackAllowed(void)
{
  ULONG power_flags = (g_eg_power.tx_event_flags_group_current & APP_POWER_FLAGS_ALL);
  ULONG mode_flags = (g_eg_mode.tx_event_flags_group_current & APP_MODE_FLAGS_ALL);

  if ((mode_flags & APP_MODE_FLAG_STOP) != 0UL)
  {
    return 0U;
  }

  return ((power_flags & APP_POWER_FLAG_RUNNING) != 0UL) ? 1U : 0U;
}

static VOID AppAudioPowerBoostUpdate(void)
{
  ULONG mode_flags = (g_eg_mode.tx_event_flags_group_current & APP_MODE_FLAGS_ALL);
  ULONG desired = 0UL;
  app_sys_event_type_t evt_type;

  if (((mode_flags & APP_MODE_FLAG_REALTIME) != 0UL) &&
      (g_audio_state == APP_AUDIO_STATE_ACTIVE) &&
      (g_audio_source_kind == APP_AUDIO_SOURCE_CLIP))
  {
    desired = 1UL;
  }

  if (desired == g_audio_power_boost_asserted)
  {
    return;
  }

  evt_type = (desired != 0UL) ? APP_SYS_EVT_AUDIO_ACTIVE : APP_SYS_EVT_AUDIO_INACTIVE;
  if (AppSysEventPost(evt_type, 0UL, 0UL, 0UL) == TX_SUCCESS)
  {
    g_audio_power_boost_asserted = desired;
  }
}

static VOID AppAudioHandleCmd(const app_audio_cmd_t *cmd)
{
  if (cmd == (const app_audio_cmd_t *)0)
  {
    return;
  }

  switch ((app_audio_cmd_type_t)cmd->type)
  {
    case APP_AUDIO_CMD_QUIESCE:
      (void)AppAudioStop();
      (void)App_SysEvent_QuiesceAck(APP_POWER_ACK_SRC_AUDIO);
      break;

    case APP_AUDIO_CMD_RESUME:
      /* Resume command intentionally leaves audio stopped until requested. */
      break;

    case APP_AUDIO_CMD_START_TONE:
      if (AppAudioPlaybackAllowed() == 0U)
      {
        break;
      }
      g_audio_sfx_autostop_armed = 0UL;
      (void)AppAudioStartTone();
      break;

    case APP_AUDIO_CMD_STOP:
      (void)AppAudioStop();
      break;

    case APP_AUDIO_CMD_PLAY_EVENT:
    {
      app_audio_event_t event_id = (app_audio_event_t)cmd->arg0;
      app_audio_asset_id_t asset_id;

      if (AppAudioPlaybackAllowed() == 0U)
      {
        break;
      }

      g_audio_last_event = (ULONG)event_id;
      if (g_audio_play_event_count < 0xFFFFFFFFUL)
      {
        g_audio_play_event_count++;
      }
      g_audio_next_sfx_gain_class = (ULONG)AppAudioResolveEventGainClass(event_id);
      asset_id = AppAudioResolveEventAsset(event_id);
      if (asset_id != APP_AUDIO_ASSET_NONE)
      {
        (void)AppAudioPlayAsset(asset_id);
      }
      break;
    }

    case APP_AUDIO_CMD_PLAY_CLIP:
      if (AppAudioPlaybackAllowed() == 0U)
      {
        break;
      }
      g_audio_next_sfx_gain_class = (ULONG)APP_AUDIO_GAIN_CLASS_SFX;
      (void)AppAudioPlayAsset((app_audio_asset_id_t)cmd->arg0);
      break;

    case APP_AUDIO_CMD_SET_USER_GAIN:
    {
      ULONG gain_id = (cmd->arg0 >> 16) & 0xFFFFUL;
      ULONG gain_pct = AppAudioClampUserGainPct(cmd->arg0 & 0xFFFFUL);

      switch ((app_audio_user_gain_id_t)gain_id)
      {
        case APP_AUDIO_USER_GAIN_MASTER:
          g_audio_user_gain_master_pct = gain_pct;
          break;
        case APP_AUDIO_USER_GAIN_MUSIC:
          g_audio_user_gain_music_pct = gain_pct;
          break;
        case APP_AUDIO_USER_GAIN_SFX:
          g_audio_user_gain_sfx_pct = gain_pct;
          break;
        case APP_AUDIO_USER_GAIN_UI:
          g_audio_user_gain_ui_pct = gain_pct;
          break;
        default:
          break;
      }
      break;
    }

    default:
      break;
  }
}

static VOID AppAudioThreadEntry(ULONG thread_input)
{
  UINT status;
  ULONG audio_flags = 0UL;
  app_audio_cmd_t cmd;

  (void)thread_input;
  HAL_GPIO_WritePin(SD_MODE_GPIO_Port, SD_MODE_Pin, GPIO_PIN_RESET);
  (void)AppAudioStop();

  for (;;)
  {
    /* Drain one immediate command if present. */
    status = tx_queue_receive(&g_q_audio_cmd, &cmd, TX_NO_WAIT);
    if (status == TX_SUCCESS)
    {
      AppAudioHandleCmd(&cmd);
      AppAudioProcessDmaEvents();
      AppAudioProcessAutoStop();
      AppAudioPowerBoostUpdate();
      continue;
    }
    if (status != TX_QUEUE_EMPTY)
    {
      (void)AppAudioStop();
      AppAudioPowerBoostUpdate();
      continue;
    }

    AppAudioProcessDmaEvents();
    AppAudioProcessAutoStop();
    AppAudioPowerBoostUpdate();

    if (g_audio_state == APP_AUDIO_STATE_ACTIVE)
    {
      status = tx_event_flags_get(&g_eg_audio_dma,
                                  APP_AUDIO_DMA_EVENT_MASK,
                                  TX_OR_CLEAR,
                                  &audio_flags,
                                  KNOB_RTOS_AUDIO_WAIT_TICKS);
      if ((status != TX_SUCCESS) && (status != TX_NO_EVENTS))
      {
        (void)AppAudioStop();
      }
    }
    else
    {
      /* Idle path: fully event-driven while stopped; no periodic wakeups. */
      status = tx_queue_receive(&g_q_audio_cmd, &cmd, TX_WAIT_FOREVER);
      if (status == TX_SUCCESS)
      {
        AppAudioHandleCmd(&cmd);
        AppAudioPowerBoostUpdate();
      }
      else if (status != TX_QUEUE_EMPTY)
      {
        (void)AppAudioStop();
        AppAudioPowerBoostUpdate();
      }
    }
  }
}
