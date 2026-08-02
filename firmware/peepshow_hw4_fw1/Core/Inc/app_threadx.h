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
  APP_STORAGE_REQ_JOYCFG_SAVE = 9U
} app_storage_req_type_t;

typedef struct
{
  ULONG type;
  ULONG arg0;
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
  APP_AUDIO_CMD_STOP = 4U
} app_audio_cmd_type_t;

typedef struct
{
  ULONG type;
  ULONG arg0;
} app_audio_cmd_t;

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
  APP_SENSOR_REQ_LIS_STEP_RESET = 15U
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
  APP_SYS_EVT_PERF_HINT = 7U
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
UINT App_ModeFlags_Get(ULONG *mode_flags_out);
UINT App_PowerFlags_Get(ULONG *power_flags_out);
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
UINT App_AudioReq_StartTone(void);
UINT App_AudioReq_Stop(void);
UINT App_SensorReq_JoyCalStart(void);
UINT App_SensorReq_JoyCalSave(void);
UINT App_SensorReq_JoyCalCancel(void);

/* USER CODE END EFP */

/* USER CODE BEGIN 1 */

/* USER CODE END 1 */

#ifdef __cplusplus
}
#endif
#endif /* __APP_THREADX_H__ */
