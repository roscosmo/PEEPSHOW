#ifndef AUDIO_ASSETS_H
#define AUDIO_ASSETS_H

#include <stdint.h>

typedef struct
{
  const uint8_t *data;
  uint32_t data_size;
  uint32_t sample_rate_hz;
  uint16_t block_align;
  uint16_t samples_per_block;
  uint32_t total_samples;
} app_audio_adpcm_clip_t;

#ifndef APP_AUDIO_ASSET_NONE
#define APP_AUDIO_ASSET_NONE (0U)
#endif
#define APP_AUDIO_ASSET_UI_MOVE (1U)
#define APP_AUDIO_ASSET_UI_CONFIRM (2U)
#define APP_AUDIO_ASSET_UI_DECLINE (3U)
#define APP_AUDIO_ASSET_UI_DENIED (4U)
#define APP_AUDIO_ASSET_UI_MAP_OPEN (5U)
#define APP_AUDIO_ASSET_UI_MAP_CLOSE (6U)
#define APP_AUDIO_ASSET_UI_DOOR (7U)
#define APP_AUDIO_ASSET_MUSIC_WHISPERS_IN_THE_FOG (8U)
#define APP_AUDIO_ASSET_SFX_DOOR (9U)
#define APP_AUDIO_ASSET_SFX_EXPLOSION (10U)
#define APP_AUDIO_ASSET_SFX_GHOST_LAUGH (11U)

uint32_t AppAudioAssets_Count(void);
uint8_t AppAudioAssets_IsValidId(uint32_t asset_id);
const char *AppAudioAssets_Name(uint32_t asset_id);
const app_audio_adpcm_clip_t *AppAudioAssets_GetClip(uint32_t asset_id);

#endif /* AUDIO_ASSETS_H */
