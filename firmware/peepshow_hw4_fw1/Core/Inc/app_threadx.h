/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    app_threadx.h
  * @author  MCD Application Team
  * @brief   ThreadX applicative header file
  ******************************************************************************
    * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __APP_THREADX_H
#define __APP_THREADX_H

#ifdef __cplusplus
 extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "tx_api.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdint.h>

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */
typedef enum
{
  APP_DISPLAY_CMD_QUIESCE = 1U,
  APP_DISPLAY_CMD_RESUME = 2U,
  APP_DISPLAY_CMD_INVALIDATE_ALL = 3U,
  APP_DISPLAY_CMD_PRESENT = 4U,
  APP_DISPLAY_CMD_SET_MODE = 5U
} app_display_cmd_type_t;

typedef struct
{
  ULONG type;
  ULONG arg0;
} app_display_cmd_t;

typedef enum
{
  APP_STORAGE_REQ_QUIESCE = 1U,
  APP_STORAGE_REQ_RESUME = 2U,
  APP_STORAGE_REQ_FLASH_PROBE = 3U,
  APP_STORAGE_REQ_RAW_SMOKE = 4U,
  APP_STORAGE_REQ_FILEX_MOUNT = 5U,
  APP_STORAGE_REQ_FILEX_FORMAT = 6U,
  APP_STORAGE_REQ_FILEX_UNMOUNT = 7U,
  APP_STORAGE_REQ_JOYCFG_LOAD = 8U,
  APP_STORAGE_REQ_JOYCFG_SAVE = 9U,
  APP_STORAGE_REQ_AUDIO_CATALOG_LOAD = 10U,
  APP_STORAGE_REQ_AUDIO_CHUNK_READ = 11U,
  APP_STORAGE_REQ_AUDIO_CATALOG_INSTALL_EMBEDDED = 12U,
  APP_STORAGE_REQ_GAME_PACKAGE_MANIFEST_LOAD = 13U,
  APP_STORAGE_REQ_GAME_PACKAGE_MANIFEST_LOAD_DEFAULT = 14U,
  APP_STORAGE_REQ_GAME_PACKAGE_MANIFEST_ERASE = 15U,
  APP_STORAGE_REQ_RAW_APP_ERASE = 16U,
  APP_STORAGE_REQ_GAME_PACKAGE_MANIFEST_WRITE_TEST = 17U,
  APP_STORAGE_REQ_SCENE_MAP_LOAD = 18U,
  APP_STORAGE_REQ_SCENE_TILESET_LOAD = 19U,
  APP_STORAGE_REQ_AUDIO_CATALOG_INSTALL_MANIFEST_REFS = 20U,
  APP_STORAGE_REQ_USB_MSC_READ = 21U,
  APP_STORAGE_REQ_USB_MSC_WRITE = 22U,
  APP_STORAGE_REQ_USB_MSC_FLUSH = 23U,
  APP_STORAGE_REQ_USB_MSC_STATUS = 24U,
  APP_STORAGE_REQ_GAME_PACKAGE_MANIFEST_IMPORT_FAT = 25U,
  APP_STORAGE_REQ_GAME_PACKAGE_SCENE_IMPORT_FAT = 26U
} app_storage_req_type_t;

typedef struct
{
  ULONG type;
  ULONG arg0;
  ULONG arg1;
  ULONG arg2;
} app_storage_req_t;

typedef enum
{
  APP_INPUT_CMD_QUIESCE = 1U,
  APP_INPUT_CMD_RESUME = 2U
} app_input_cmd_type_t;

typedef struct
{
  ULONG type;
  ULONG arg0;
} app_input_cmd_t;

typedef enum
{
  APP_AUDIO_CMD_QUIESCE = 1U,
  APP_AUDIO_CMD_RESUME = 2U,
  APP_AUDIO_CMD_START_TONE = 3U,
  APP_AUDIO_CMD_STOP = 4U,
  APP_AUDIO_CMD_PLAY_EVENT = 5U,
  APP_AUDIO_CMD_PLAY_CLIP = 6U,
  APP_AUDIO_CMD_SET_USER_GAIN = 7U
} app_audio_cmd_type_t;

typedef struct
{
  ULONG type;
  ULONG arg0;
} app_audio_cmd_t;

typedef enum
{
  APP_AUDIO_EVENT_NONE = 0U,
  APP_AUDIO_EVENT_UI_NAV = 1U,
  APP_AUDIO_EVENT_UI_CONFIRM = 2U,
  APP_AUDIO_EVENT_UI_CANCEL = 3U,
  APP_AUDIO_EVENT_UI_DENIED = 4U,
  APP_AUDIO_EVENT_GAME_ACTION = 5U,
  APP_AUDIO_EVENT_RT_MOVE = 6U,
  APP_AUDIO_EVENT_RT_CONFIRM = 7U,
  APP_AUDIO_EVENT_RT_CANCEL = 8U,
  APP_AUDIO_EVENT_RT_MENU = 9U
} app_audio_event_t;

typedef enum
{
  APP_AUDIO_USER_GAIN_MASTER = 1U,
  APP_AUDIO_USER_GAIN_MUSIC = 2U,
  APP_AUDIO_USER_GAIN_SFX = 3U,
  APP_AUDIO_USER_GAIN_UI = 4U
} app_audio_user_gain_id_t;

typedef uint32_t app_audio_asset_id_t;
#define APP_AUDIO_ASSET_NONE ((app_audio_asset_id_t)0U)

typedef enum
{
  APP_AUDIO_CLIP_NONE = 0U,
  APP_AUDIO_CLIP_UI_MOVE = 1U,
  APP_AUDIO_CLIP_UI_CONFIRM = 2U,
  APP_AUDIO_CLIP_UI_DECLINE = 3U,
  APP_AUDIO_CLIP_UI_DENIED = 4U,
  APP_AUDIO_CLIP_GAME_SFX = 5U,
  APP_AUDIO_CLIP_GAME_MUSIC = 6U
} app_audio_clip_t;

typedef enum
{
  APP_SENSOR_REQ_QUIESCE = 1U,
  APP_SENSOR_REQ_RESUME = 2U,
  APP_SENSOR_REQ_POLL = 3U,
  APP_SENSOR_REQ_CONFIG_DEFAULTS = 4U,
  APP_SENSOR_REQ_HEALTH_SNAPSHOT = 5U,
  APP_SENSOR_REQ_MODE_CHANGED = 6U,
  APP_SENSOR_REQ_JOY_CAL_START = 7U,
  APP_SENSOR_REQ_JOY_CAL_SAVE = 8U,
  APP_SENSOR_REQ_JOY_CAL_CANCEL = 9U,
  APP_SENSOR_REQ_LIS_SET_PROFILE = 10U,
  APP_SENSOR_REQ_LIS_STREAM_START = 11U,
  APP_SENSOR_REQ_LIS_STREAM_STOP = 12U,
  APP_SENSOR_REQ_LIS_STEP_ENABLE = 13U,
  APP_SENSOR_REQ_LIS_STEP_DISABLE = 14U,
  APP_SENSOR_REQ_LIS_STEP_RESET = 15U,
  APP_SENSOR_REQ_JOY_DEADZONE_SET = 16U,
  APP_SENSOR_REQ_SETTINGS_SAVE = 17U,
  APP_SENSOR_REQ_PMIC_IRQ = 18U
} app_sensor_req_type_t;

typedef struct
{
  ULONG type;
  ULONG arg0;
  ULONG arg1;
  ULONG arg2;
} app_sensor_req_t;

typedef enum
{
  APP_SENSOR_TARGET_NONE = 0U,
  APP_SENSOR_TARGET_PMIC = (1U << 0),
  APP_SENSOR_TARGET_TMAG = (1U << 1),
  APP_SENSOR_TARGET_LIS = (1U << 2),
  APP_SENSOR_TARGET_ALL = (APP_SENSOR_TARGET_PMIC | APP_SENSOR_TARGET_TMAG | APP_SENSOR_TARGET_LIS)
} app_sensor_target_t;

typedef enum
{
  APP_SENSOR_LIS_PROFILE_LOW_POWER = 0U,
  APP_SENSOR_LIS_PROFILE_LIVE = 1U
} app_sensor_lis_profile_t;

typedef enum
{
  APP_SYS_EVT_MODE_SET = 1U,
  APP_SYS_EVT_QUIESCE_REQ = 2U,
  APP_SYS_EVT_RESUME_REQ = 3U,
  APP_SYS_EVT_QUIESCE_ACK = 4U,
  APP_SYS_EVT_INPUT_ACTIVITY = 5U,
  APP_SYS_EVT_INPUT_MENU = 6U,
  APP_SYS_EVT_PERF_HINT = 7U,
  APP_SYS_EVT_PET_ACTION = 8U,
  APP_SYS_EVT_AUDIO_ACTIVE = 9U,
  APP_SYS_EVT_AUDIO_INACTIVE = 10U,
  APP_SYS_EVT_USB_VBUS_PRESENT = 11U,
  APP_SYS_EVT_USB_MSC_RECOVER = 12U
} app_sys_event_type_t;

typedef struct
{
  ULONG type;
  ULONG arg0;
  ULONG arg1;
  ULONG arg2;
} app_sys_event_t;

typedef struct
{
  ULONG valid_mask;
  ULONG joy_dir;
  ULONG joy_input_mask;
  float joy_nx;
  float joy_ny;
  float joy_r_abs_mT;
  ULONG joy_deadzone_enabled;
  float joy_deadzone_mT;
  ULONG joy_last_sample_tick;
  LONG lis_x_raw;
  LONG lis_y_raw;
  LONG lis_z_raw;
  ULONG lis_status;
  ULONG lis_sample_count;
  ULONG lis_last_sample_tick;
  LONG lis_last_error;
} app_sensor_snapshot_t;

#define APP_SENSOR_SNAPSHOT_VALID_JOY (1UL << 0)
#define APP_SENSOR_SNAPSHOT_VALID_LIS (1UL << 1)

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */
typedef enum
{
  APP_MODE_STOP = 0U,
  APP_MODE_STATIC = 1U,
  APP_MODE_REALTIME = 2U,
  APP_MODE_FLASHING = 3U
} app_mode_t;

typedef enum
{
  APP_PET_ACTION_NONE = 0U,
  APP_PET_ACTION_FEED = 1U,
  APP_PET_ACTION_PLAY = 2U,
  APP_PET_ACTION_REST = 3U
} app_pet_action_t;

typedef struct
{
  ULONG map_ok_count;
  ULONG map_fail_count;
  ULONG map_last_status;
  ULONG map_loaded;
  ULONG tileset_ok_count;
  ULONG tileset_fail_count;
  ULONG tileset_last_status;
  ULONG tileset_loaded;
} app_storage_scene_load_status_t;

/* USER CODE END EC */

/* Private defines -----------------------------------------------------------*/

/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Main thread defines -------------------------------------------------------*/
/* USER CODE BEGIN MTD */

/* USER CODE END MTD */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
UINT App_ThreadX_Init(VOID *memory_ptr);
void MX_ThreadX_Init(void);
void valueNotSetted(ULONG thread_input);

/* USER CODE BEGIN EFP */
UINT App_SysEvent_ModeSet(app_mode_t mode_token);
UINT App_SysEvent_QuiesceReq(void);
UINT App_SysEvent_ResumeReq(void);
UINT App_SysEvent_QuiesceAck(ULONG ack_mask);
UINT App_SysEvent_PerfHint(ULONG present_ticks, ULONG draw_ticks, ULONG dirty_rows, ULONG full_flush);
UINT App_SysEvent_UsbVbusPresent(ULONG present);
UINT App_UsbVbusPresent_Get(ULONG *present_out);
UINT App_UsbFlashPrompt_GetPending(ULONG *pending_out);
UINT App_UsbFlashPrompt_Clear(void);
UINT App_UsbFlash_RequestEnter(void);
UINT App_PetReq_Action(app_pet_action_t action_id);
UINT App_PetReq_ActionWithHold(app_pet_action_t action_id, ULONG hold_ms);
UINT App_ModeFlags_Get(ULONG *mode_flags_out);
UINT App_PowerFlags_Get(ULONG *power_flags_out);
UINT App_Power_Stop2TimebaseTelemetryClear(void);
UINT App_RetainedStateClear(void);
UINT App_SensorHealthFlags_Get(ULONG *sensor_flags_out);
UINT App_SensorSnapshot_Get(app_sensor_snapshot_t *snapshot_out);
UINT App_SensorReq_Poll(app_sensor_target_t targets);
UINT App_SensorReq_ConfigDefaults(app_sensor_target_t targets);
UINT App_SensorReq_HealthSnapshot(void);
UINT App_SensorReq_LisSetProfile(app_sensor_lis_profile_t profile);
UINT App_SensorReq_LisSetLowPower(void);
UINT App_SensorReq_LisSetLive(void);
UINT App_SensorReq_LisStreamStart(void);
UINT App_SensorReq_LisStreamStop(void);
UINT App_SensorReq_LisStepEnable(void);
UINT App_SensorReq_LisStepDisable(void);
UINT App_SensorReq_LisStepReset(void);
UINT App_Display_InvalidateAll(void);
UINT App_Display_Present(void);
UINT App_StorageReq_FlashProbe(void);
UINT App_StorageReq_RawSmoke(void);
UINT App_StorageReq_FileXMount(void);
UINT App_StorageReq_FileXFormat(void);
UINT App_StorageReq_FileXUnmount(void);
UINT App_StorageReq_JoyCfgLoad(void);
UINT App_StorageReq_JoyCfgSave(void);
UINT App_StorageReq_AudioCatalogLoad(ULONG catalog_addr);
UINT App_StorageReq_AudioChunkRead(ULONG addr, ULONG len, ULONG token);
UINT App_StorageReq_AudioCatalogInstallEmbedded(void);
UINT App_StorageReq_AudioCatalogInstallManifestRefs(void);
UINT App_StorageReq_GamePackageManifestLoad(ULONG manifest_addr, ULONG manifest_size);
UINT App_StorageReq_GamePackageManifestLoadDefault(void);
UINT App_StorageReq_GamePackageManifestErase(void);
UINT App_StorageReq_RawAppErase(void);
UINT App_StorageReq_GamePackageManifestWriteTest(void);
UINT App_StorageReq_SceneMapLoad(ULONG map_addr, ULONG map_size);
UINT App_StorageReq_SceneTilesetLoad(ULONG tileset_addr, ULONG tileset_size);
UINT App_StorageSceneLoadStatusGet(app_storage_scene_load_status_t *status_out);
UINT App_StorageReq_UsbMscRead(ULONG lba, ULONG number_blocks, uint8_t *data_pointer, ULONG *media_status_out);
UINT App_StorageReq_UsbMscWrite(ULONG lba, ULONG number_blocks, uint8_t *data_pointer, ULONG *media_status_out);
UINT App_StorageReq_UsbMscFlush(ULONG lba, ULONG number_blocks, ULONG *media_status_out);
UINT App_StorageReq_UsbMscStatus(ULONG media_id, ULONG *media_status_out);
ULONG App_StorageReq_UsbMscGetMediaLastLba(void);
ULONG App_StorageReq_UsbMscGetMediaBlockLength(void);
UINT App_StorageReq_GamePackageManifestImportFat(void);
UINT App_StorageReq_GamePackageSceneImportFat(void);
UINT App_AudioReq_StartTone(void);
UINT App_AudioReq_Stop(void);
UINT App_AudioReq_PlayEvent(app_audio_event_t event_id);
UINT App_AudioReq_PlayAsset(app_audio_asset_id_t asset_id);
UINT App_AudioReq_PlayClip(app_audio_clip_t clip_id);
UINT App_AudioReq_SetUserGain(app_audio_user_gain_id_t gain_id, ULONG pct);
UINT App_SensorReq_JoyCalStart(void);
UINT App_SensorReq_JoyCalSave(void);
UINT App_SensorReq_JoyCalCancel(void);
UINT App_SensorReq_JoyDeadzoneSet(ULONG deadzone_mT_x10);
UINT App_SensorReq_SettingsSave(void);

/* USER CODE END EFP */

/* USER CODE BEGIN 1 */

/* USER CODE END 1 */

#ifdef __cplusplus
}
#endif
#endif /* __APP_THREADX_H__ */
