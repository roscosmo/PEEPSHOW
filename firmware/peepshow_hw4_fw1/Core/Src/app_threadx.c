/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    app_threadx.c
  * @author  MCD Application Team
  * @brief   ThreadX applicative file
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

/* Includes ------------------------------------------------------------------*/
#include "app_threadx.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "main.h"
#include "LS013B7DH05.h"
#include "ADP5360.h"
#include "TMAG5273.h"
#include "TMAG_joy.h"
#include "LIS2DUX12.h"
#include "AT25SL128A.h"
#include "fx_api.h"
#include "fx_stm32_levelx_nor_driver.h"
#include "display_renderer.h"
#include "game_runtime.h"
#include "game_mode_topdown_basic.h"
#include "game_package.h"
#include "game_package_manifest.h"
#include "game_map_registry_autogen.h"
#include "th_mode.h"
#include "audio_assets.h"
#include "ui/ui_runtime_context.h"
#include "ui/ui_router.h"
#include "ui/ui_menu_tree.h"
#include "ui/ui_page_native.h"
#include "knobs_autogen.h"
#include "stm32u5xx_hal_pcd_ex.h"
#include <string.h>
#include <math.h>

extern PCD_HandleTypeDef hpcd_USB_OTG_FS;
extern volatile UINT g_usbx_device_pool_create_status;
extern volatile UINT g_usbx_device_init_status;
extern volatile UINT g_usbx_init_stage;
extern volatile UINT g_usbx_init_error_code;

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
#define APP_AUDIO_EXTERNAL_BLOCK_MAX_BYTES 1024U
#define APP_STORAGE_AUDIO_CHUNK_MAX_BYTES 1024UL

typedef enum
{
  APP_AUDIO_GAIN_CLASS_SFX = 0U,
  APP_AUDIO_GAIN_CLASS_UI = 1U,
  APP_AUDIO_GAIN_CLASS_MUSIC = 2U
} app_audio_gain_class_t;

typedef enum
{
  APP_SENSOR_STATE_OFF = 0U,
  APP_SENSOR_STATE_INITING = 1U,
  APP_SENSOR_STATE_READY = 2U,
  APP_SENSOR_STATE_FAULT = 3U,
  APP_SENSOR_STATE_RECOVERING = 4U,
  APP_SENSOR_STATE_SUSPENDED = 5U
} app_sensor_state_t;

typedef struct
{
  ULONG state;
  ULONG fail_count;
  ULONG recovery_attempts;
  ULONG next_retry_tick;
  ULONG last_success_tick;
  LONG last_error;
} app_sensor_fsm_t;

typedef struct
{
  I2C_HandleTypeDef *hi2c;
  uint16_t addr;
} app_sensor_lis_ctx_t;

typedef struct
{
  int32_t predictor;
  int32_t index;
  uint8_t byte_cache;
  uint8_t have_high_nibble;
  ULONG nibbles_left_in_block;
  ULONG data_offset;
  ULONG block_cursor;
  ULONG block_loaded;
  ULONG emit_block_header;
} app_audio_adpcm_decode_t;

typedef struct
{
  uint8_t active;
  uint8_t loop;
  uint8_t source_kind;
  uint8_t gain_class;
  uint8_t external_req_pending;
  uint8_t external_prefetch_pending;
  uint8_t external_prefetch_ready;
  const app_audio_adpcm_clip_t *clip;
  const uint8_t *embedded_data;
  ULONG data_size_bytes;
  ULONG external_data_addr;
  ULONG external_req_token;
  ULONG external_req_addr;
  ULONG external_req_len;
  ULONG external_prefetch_token;
  ULONG external_prefetch_addr;
  ULONG external_prefetch_offset;
  ULONG external_prefetch_len;
  uint16_t block_align;
  uint16_t samples_per_block;
  uint32_t sample_rate_hz;
  ULONG sample_count;
  ULONG sample_cursor;
  ULONG start_seq;
  uint8_t external_block_buf[APP_AUDIO_EXTERNAL_BLOCK_MAX_BYTES];
  uint8_t external_prefetch_buf[APP_AUDIO_EXTERNAL_BLOCK_MAX_BYTES];
  app_audio_adpcm_decode_t decode;
} app_audio_voice_t;

typedef struct
{
  app_audio_asset_id_t asset_id;
  ULONG source_kind;
  const app_audio_adpcm_clip_t *clip;
  ULONG data_offset;
  ULONG data_size;
  ULONG sample_rate_hz;
  uint16_t block_align;
  uint16_t samples_per_block;
  ULONG total_samples;
} app_audio_catalog_entry_t;

typedef struct
{
  volatile ULONG seq;
  ULONG token;
  ULONG addr;
  ULONG len;
  ULONG status;
  ULONG crc32;
  uint8_t data[APP_STORAGE_AUDIO_CHUNK_MAX_BYTES];
} app_storage_audio_chunk_cache_entry_t;

typedef struct
{
  ULONG addr;
  ULONG whoami;
  ULONG status;
  ULONG sample_count;
  ULONG fail_count;
  ULONG last_sample_tick;
  LONG last_error;
  int16_t x_raw;
  int16_t y_raw;
  int16_t z_raw;
  ULONG step_enabled;
  ULONG step_count;
  ULONG step_detected;
  ULONG tilt_detected;
  ULONG sigmot_detected;
} app_sensor_lis_live_t;

typedef struct
{
  ULONG sample_count;
  ULONG fail_count;
  ULONG last_sample_tick;
  LONG last_error;
  LONG last_transport_error;
  ULONG guard_enabled;
  ULONG cutoff_mv;
  ULONG cutoff_hys_mv;
  ULONG cutoff_confirm_samples;
  ULONG cutoff_low_streak;
  ULONG cutoff_latched;
  ULONG isofet_forced_off;
  ULONG charging_enabled_cfg;
  ULONG charging_active;
  ULONG battery_soc_percent;
  ULONG battery_soc_raw;
  ULONG battery_health_state;
  ULONG battery_health_reason;
  ULONG transport_error_count;
  ULONG fault_event_count;
  ULONG last_fault_mask;
  ULONG status2_raw;
  ULONG fault_raw;
  ULONG pgood_raw;
  ULONG vbus_present;
  ULONG irq_flag1_raw;
  ULONG irq_flag2_raw;
  ULONG charger_state;
  ULONG battery_uv;
  ULONG battery_ov;
  ULONG vbat_mV;
  ULONG vbat_raw;
} app_sensor_pmic_live_t;

typedef struct
{
  ULONG source;
  ULONG edge;
  ULONG tick;
  ULONG level;
} app_input_raw_evt_t;

typedef enum
{
  APP_INPUT_ACTION_NONE = 0U,
  APP_INPUT_ACTION_BTN_A = 1U,
  APP_INPUT_ACTION_BTN_B = 2U,
  APP_INPUT_ACTION_BTN_L = 3U,
  APP_INPUT_ACTION_BTN_R = 4U,
  APP_INPUT_ACTION_BTN_BOOT = 5U,
  APP_INPUT_ACTION_JOY_UP = 6U,
  APP_INPUT_ACTION_JOY_RIGHT = 7U,
  APP_INPUT_ACTION_JOY_DOWN = 8U,
  APP_INPUT_ACTION_JOY_LEFT = 9U
} app_input_action_t;

typedef enum
{
  APP_INPUT_EVENT_PRESS = 1U,
  APP_INPUT_EVENT_RELEASE = 2U,
  APP_INPUT_EVENT_REPEAT = 3U,
  APP_INPUT_EVENT_LONG = 4U
} app_input_event_t;

typedef struct
{
  ULONG action;
  ULONG source;
  ULONG event;
  ULONG tick;
  ULONG pressed_mask;
} app_input_action_evt_t;

typedef struct
{
  ULONG edge_seen;
  ULONG pressed;
  ULONG last_edge_tick;
  ULONG press_tick;
  ULONG long_sent;
  ULONG next_repeat_tick;
} app_input_button_state_t;

typedef enum
{
  APP_UI_PAGE_HOME = 0U,
  APP_UI_PAGE_JOY_CAL = 1U,
  APP_UI_PAGE_PET = 2U
} app_ui_page_t;

typedef enum
{
  APP_JOY_CAL_STAGE_IDLE = 0U,
  APP_JOY_CAL_STAGE_NEUTRAL = 1U,
  APP_JOY_CAL_STAGE_UP = 2U,
  APP_JOY_CAL_STAGE_RIGHT = 3U,
  APP_JOY_CAL_STAGE_DOWN = 4U,
  APP_JOY_CAL_STAGE_LEFT = 5U,
  APP_JOY_CAL_STAGE_SWEEP = 6U,
  APP_JOY_CAL_STAGE_DONE = 7U,
  APP_JOY_CAL_STAGE_ERROR = 8U
} app_joy_cal_stage_t;

typedef struct
{
  ULONG stage;
  float progress;
  LONG last_error;
  ULONG save_pending;
  ULONG save_ok_count;
  ULONG save_fail_count;
  ULONG load_ok_count;
  ULONG load_fail_count;
} app_joy_cal_status_t;

typedef struct
{
  uint8_t active;
  uint32_t t_start_ms;
  uint32_t duration_ms;
  uint32_t sample_every_ms;
  uint32_t settle_ms;
  uint32_t last_sample_ms;
  uint32_t n;
  float peak_r;
  float sum_w;
  float sum_x;
  float sum_y;
} app_joy_cal_capture_t;

typedef struct
{
  ULONG dir;
  ULONG input_mask;
  ULONG deadzone_enabled;
  ULONG invert_x;
  ULONG invert_y;
  float nx;
  float ny;
  float r_abs_mT;
  float center_x_mT;
  float center_y_mT;
  float span_x_mT;
  float span_y_mT;
  float rotation_deg;
  float threshold_x_mT;
  float threshold_y_mT;
  float deadzone_mT;
} app_joy_live_status_t;

typedef struct
{
  ULONG valid;
  ULONG quality_ok;
  float span_ratio;
  float axis_error;
  float dir_norm_min;
  float dir_norm_max;
} app_joy_cal_quality_t;

typedef struct
{
  uint32_t magic;
  uint32_t version;
  uint32_t payload_size;
  uint32_t crc32;
  TMAGJoy_Cal cal;
} app_storage_joycfg_blob_v2_t;

typedef struct
{
  uint32_t magic;
  uint32_t version;
  uint32_t payload_size;
  uint32_t crc32;
  TMAGJoy_Cal cal;
  uint32_t deadzone_enabled;
  float deadzone_mT;
  uint32_t user_gain_master_pct;
  uint32_t user_gain_music_pct;
  uint32_t user_gain_sfx_pct;
  uint32_t user_gain_ui_pct;
} app_storage_joycfg_blob_v3_t;

typedef struct
{
  uint32_t magic;
  uint16_t version;
  uint16_t header_size;
  uint16_t entry_size;
  uint16_t reserved0;
  uint32_t entry_count;
  uint32_t table_crc32;
  uint32_t reserved1;
} app_storage_audio_catalog_header_t;

typedef struct
{
  uint32_t asset_id;
  uint32_t flags;
  uint32_t data_offset;
  uint32_t data_size;
  uint32_t sample_rate_hz;
  uint16_t block_align;
  uint16_t samples_per_block;
  uint32_t total_samples;
} app_storage_audio_catalog_entry_t;

typedef struct
{
  ULONG sysclk_mhz;
  ULONG pll_n;
  ULONG pll_r;
} app_power_perf_profile_t;

typedef struct __attribute__((packed))
{
  uint32_t magic;
  uint16_t version;
  uint16_t header_size;
  uint32_t sequence;
  uint32_t package_id;
  uint32_t package_version;
  uint32_t blob_offset;
  uint32_t blob_size;
  uint32_t blob_crc32;
  uint32_t manifest_addr;
  uint32_t manifest_size;
  uint32_t manifest_crc32;
  uint32_t record_crc32;
} app_storage_install_index_record_t;

#if defined(__GNUC__)
#define APP_RETAIN_ATTR __attribute__((section(".sram4"))) __attribute__((aligned(4)))
#else
#define APP_RETAIN_ATTR
#endif

#define APP_RETAINED_STATE_MAGIC   (0x52535445UL) /* 'RSTE' */
#define APP_RETAINED_STATE_VERSION (2U)
#define APP_RETAINED_VALID_PET     (1UL << 0)
#define APP_RETAINED_VALID_GAME    (1UL << 1)

typedef struct
{
  uint32_t mode_id;
  uint32_t backend_id;
  uint32_t scene_map_id;
  uint32_t scene_tileset_id;
  game_mode_topdown_basic_snapshot_t topdown;
} app_retained_game_state_t;

typedef struct
{
  uint32_t magic;
  uint16_t version;
  uint16_t size;
  uint32_t seq;
  uint32_t valid_mask;
  uint32_t crc32;
  uint32_t pet_state;
  uint32_t pet_tick_count;
  uint32_t pet_wake_count;
  uint32_t pet_last_action;
  uint32_t pet_hunger_pct;
  uint32_t pet_energy_pct;
  uint32_t pet_mood_pct;
  app_retained_game_state_t game;
} app_retained_state_blob_t;

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define APP_MODE_FLAG_STOP      (1UL << 0)
#define APP_MODE_FLAG_STATIC    (1UL << 1)
#define APP_MODE_FLAG_REALTIME  (1UL << 2)
#define APP_MODE_FLAG_FLASHING  (1UL << 3)
#define APP_MODE_FLAGS_ALL      (APP_MODE_FLAG_STOP | APP_MODE_FLAG_STATIC | APP_MODE_FLAG_REALTIME | APP_MODE_FLAG_FLASHING)

#define APP_POWER_FLAG_QUIESCE_REQ  (1UL << 0)
#define APP_POWER_FLAG_QUIESCED     (1UL << 1)
#define APP_POWER_FLAG_RESUME_REQ   (1UL << 2)
#define APP_POWER_FLAG_RUNNING      (1UL << 3)
#define APP_POWER_FLAG_QUIESCE_TIMEOUT (1UL << 4)
#define APP_POWER_FLAGS_ALL         (APP_POWER_FLAG_QUIESCE_REQ | APP_POWER_FLAG_QUIESCED | APP_POWER_FLAG_RESUME_REQ | APP_POWER_FLAG_RUNNING | APP_POWER_FLAG_QUIESCE_TIMEOUT)

#define APP_POWER_ACK_SRC_DISPLAY   (1UL << 0)
#define APP_POWER_ACK_SRC_STORAGE   (1UL << 1)
#define APP_POWER_ACK_SRC_INPUT     (1UL << 2)
#define APP_POWER_ACK_SRC_SENSOR    (1UL << 3)
#define APP_POWER_ACK_SRC_AUDIO     (1UL << 4)
#define APP_POWER_ACK_MASK_ALL      (APP_POWER_ACK_SRC_DISPLAY | APP_POWER_ACK_SRC_STORAGE | APP_POWER_ACK_SRC_INPUT | APP_POWER_ACK_SRC_SENSOR | APP_POWER_ACK_SRC_AUDIO)
#define APP_POWER_PERF_PROFILE_NORM  (0UL)
#define APP_POWER_PERF_PROFILE_ECO   (1UL)
#define APP_POWER_PERF_PROFILE_MID   (2UL)
#define APP_POWER_PERF_PROFILE_BAL   (3UL)
#define APP_POWER_PERF_PROFILE_FAST  (4UL)
#define APP_POWER_PERF_PROFILE_TURBO (5UL)
#define APP_POWER_PERF_PROFILE_MAX   APP_POWER_PERF_PROFILE_TURBO
#define APP_POWER_CLK_STAGE_NONE               (0UL)
#define APP_POWER_CLK_STAGE_NORM_SYSCLK        (21UL)
#define APP_POWER_CLK_STAGE_NORM_VOS           (22UL)
#define APP_POWER_CLK_STAGE_NORM_PLL_OFF       (23UL)
#define APP_POWER_CLK_STAGE_NORM_PERIPH        (24UL)
#define APP_POWER_CLK_STAGE_NORM_SYSTICK       (25UL)
#define APP_POWER_CLK_STAGE_PERF_TARGET        (31UL)
#define APP_POWER_CLK_STAGE_PERF_VOS           (32UL)
#define APP_POWER_CLK_STAGE_PERF_PLL_ON        (33UL)
#define APP_POWER_CLK_STAGE_PERF_SYSCLK        (34UL)
#define APP_POWER_CLK_STAGE_PERF_SYSTICK       (35UL)
#define APP_PMIC_BAT_HEALTH_UNKNOWN  (0UL)
#define APP_PMIC_BAT_HEALTH_OK       (1UL)
#define APP_PMIC_BAT_HEALTH_WARN     (2UL)
#define APP_PMIC_BAT_HEALTH_CRIT     (3UL)
#define APP_PMIC_BAT_REASON_WARN_MV  (1UL << 0)
#define APP_PMIC_BAT_REASON_WARN_SOC (1UL << 1)
#define APP_PMIC_BAT_REASON_CRIT_MV  (1UL << 2)
#define APP_PMIC_BAT_REASON_CRIT_SOC (1UL << 3)

#define APP_QSYS_EVENT_WORDS    ((UINT)(sizeof(app_sys_event_t) / sizeof(ULONG)))
#define APP_DISPLAY_CMD_WORDS   ((UINT)(sizeof(app_display_cmd_t) / sizeof(ULONG)))
#define APP_STORAGE_REQ_WORDS   ((UINT)(sizeof(app_storage_req_t) / sizeof(ULONG)))
#define APP_INPUT_CMD_WORDS     ((UINT)(sizeof(app_input_cmd_t) / sizeof(ULONG)))
#define APP_INPUT_RAW_WORDS     ((UINT)(sizeof(app_input_raw_evt_t) / sizeof(ULONG)))
#define APP_INPUT_ACTION_WORDS  ((UINT)(sizeof(app_input_action_evt_t) / sizeof(ULONG)))
#define APP_AUDIO_CMD_WORDS     ((UINT)(sizeof(app_audio_cmd_t) / sizeof(ULONG)))
#define APP_SENSOR_REQ_WORDS    ((UINT)(sizeof(app_sensor_req_t) / sizeof(ULONG)))

#define APP_INPUT_SOURCE_BTN_A      1UL
#define APP_INPUT_SOURCE_BTN_B      2UL
#define APP_INPUT_SOURCE_BTN_L      3UL
#define APP_INPUT_SOURCE_BTN_R      4UL
#define APP_INPUT_SOURCE_BTN_BOOT   5UL
#define APP_INPUT_SOURCE_JOY_UP     6UL
#define APP_INPUT_SOURCE_JOY_RIGHT  7UL
#define APP_INPUT_SOURCE_JOY_DOWN   8UL
#define APP_INPUT_SOURCE_JOY_LEFT   9UL
#define APP_INPUT_SOURCE_MAX        APP_INPUT_SOURCE_JOY_LEFT
#define APP_INPUT_SOURCE_COUNT      (APP_INPUT_SOURCE_MAX + 1UL)
#define APP_INPUT_EDGE_HIGH         1UL
#define APP_INPUT_EDGE_LOW          2UL
#define APP_INPUT_EDGE_REPEAT       3UL
#define APP_INPUT_SRCBIT(src_)      (1UL << ((src_) - 1UL))
#define APP_INPUT_SOURCES_BTN_MASK  (APP_INPUT_SRCBIT(APP_INPUT_SOURCE_BTN_A) | APP_INPUT_SRCBIT(APP_INPUT_SOURCE_BTN_B) | APP_INPUT_SRCBIT(APP_INPUT_SOURCE_BTN_L) | APP_INPUT_SRCBIT(APP_INPUT_SOURCE_BTN_R) | APP_INPUT_SRCBIT(APP_INPUT_SOURCE_BTN_BOOT))
#define APP_INPUT_SOURCES_JOY_MASK  (APP_INPUT_SRCBIT(APP_INPUT_SOURCE_JOY_UP) | APP_INPUT_SRCBIT(APP_INPUT_SOURCE_JOY_RIGHT) | APP_INPUT_SRCBIT(APP_INPUT_SOURCE_JOY_DOWN) | APP_INPUT_SRCBIT(APP_INPUT_SOURCE_JOY_LEFT))
#define APP_INPUT_SOURCES_ALL_MASK  (APP_INPUT_SOURCES_BTN_MASK | APP_INPUT_SOURCES_JOY_MASK)
#define APP_UI_HOME_ITEM_JOY_CAL    0UL
#define APP_UI_HOME_ITEM_HOME       1UL
#define APP_UI_HOME_ITEM_COUNT      2UL
#define APP_UI_STATIC_ENTRY_AUTO     0UL
#define APP_UI_STATIC_ENTRY_HOME     1UL
#define APP_UI_STATIC_ENTRY_JOY_CAL  2UL
#define APP_PET_STATE_SLEEP          0UL
#define APP_PET_STATE_IDLE           1UL
#define APP_PET_STATE_FEEDING        2UL
#define APP_PET_STATE_PLAYING        3UL
#define APP_PET_STATE_RESTING        4UL

#define APP_DISPLAY_VLT_ACTIVE_LEVEL   (GPIO_PIN_RESET)
#define APP_AUDIO_CHANNEL_COUNT        2UL
#define APP_AUDIO_DMA_SAMPLE_COUNT     (KNOB_AUDIO_DMA_FRAMES * APP_AUDIO_CHANNEL_COUNT)
#define APP_AUDIO_STATE_STOPPED        0UL
#define APP_AUDIO_STATE_ACTIVE         1UL
#define APP_AUDIO_SOURCE_NONE          0UL
#define APP_AUDIO_SOURCE_TONE          1UL
#define APP_AUDIO_SOURCE_CLIP          2UL
#define APP_AUDIO_CATALOG_SOURCE_NONE      0UL
#define APP_AUDIO_CATALOG_SOURCE_EMBEDDED  1UL
#define APP_AUDIO_CATALOG_SOURCE_EXTERNAL  2UL
#define APP_AUDIO_DMA_HALF_FLAG        (1UL << 0)
#define APP_AUDIO_DMA_FULL_FLAG        (1UL << 1)
#define APP_AUDIO_DMA_ERROR_FLAG       (1UL << 2)
#define APP_AUDIO_DMA_EVENT_MASK       (APP_AUDIO_DMA_HALF_FLAG | APP_AUDIO_DMA_FULL_FLAG | APP_AUDIO_DMA_ERROR_FLAG)
#define APP_AUDIO_SFX_UI_NAV_MS        30UL
#define APP_AUDIO_SFX_UI_CONFIRM_MS    60UL
#define APP_AUDIO_SFX_UI_DECLINE_MS    70UL
#define APP_AUDIO_SFX_UI_DENIED_MS     110UL
#define APP_AUDIO_SFX_GAME_ACTION_MS   45UL
#define APP_AUDIO_STORAGE_READ_WAIT_TICKS     4UL
#define APP_USB_MSC_MUTEX_WAIT_TICKS         25UL
#define APP_USB_MSC_QUEUE_WAIT_TICKS         25UL
#define APP_USB_MSC_DONE_WAIT_TICKS         200UL
#define APP_USB_MSC_RECOVER_REASON_QUEUE_FAIL    1UL
#define APP_USB_MSC_RECOVER_REASON_DONE_TIMEOUT  2UL

#define APP_SENSOR_HEALTH_PMIC_READY   (1UL << 0)
#define APP_SENSOR_HEALTH_TMAG_READY   (1UL << 1)
#define APP_SENSOR_HEALTH_LIS_READY    (1UL << 2)
#define APP_SENSOR_HEALTH_PMIC_FAULT   (1UL << 3)
#define APP_SENSOR_HEALTH_TMAG_FAULT   (1UL << 4)
#define APP_SENSOR_HEALTH_LIS_FAULT    (1UL << 5)
#define APP_SENSOR_HEALTH_BUS_FAULT    (1UL << 6)
#define APP_SENSOR_HEALTH_PMIC_SUSPENDED (1UL << 7)
#define APP_SENSOR_HEALTH_TMAG_SUSPENDED (1UL << 8)
#define APP_SENSOR_HEALTH_LIS_SUSPENDED  (1UL << 9)
#define APP_SENSOR_HEALTH_FLAGS_ALL    (APP_SENSOR_HEALTH_PMIC_READY | APP_SENSOR_HEALTH_TMAG_READY | APP_SENSOR_HEALTH_LIS_READY | APP_SENSOR_HEALTH_PMIC_FAULT | APP_SENSOR_HEALTH_TMAG_FAULT | APP_SENSOR_HEALTH_LIS_FAULT | APP_SENSOR_HEALTH_BUS_FAULT | APP_SENSOR_HEALTH_PMIC_SUSPENDED | APP_SENSOR_HEALTH_TMAG_SUSPENDED | APP_SENSOR_HEALTH_LIS_SUSPENDED)
#define APP_SENSOR_TARGET_MASK_ALL     ((ULONG)APP_SENSOR_TARGET_ALL)
#define APP_SENSOR_I2C_GPIO_PORT       GPIOC
#define APP_SENSOR_I2C_SCL_PIN         GPIO_PIN_0
#define APP_SENSOR_I2C_SDA_PIN         GPIO_PIN_1
#define APP_SENSOR_I2C_AF              GPIO_AF4_I2C3
#define APP_STORAGE_FLASH_SIZE_BYTES   (16UL * 1024UL * 1024UL)
#define APP_STORAGE_SMOKE_SECTOR_SIZE  4096UL
#define APP_STORAGE_SMOKE_MAX_LEN      256UL
#define APP_STORAGE_FAT_BLOCK_SIZE     APP_STORAGE_SMOKE_SECTOR_SIZE
#define APP_STORAGE_FAT_BYTES_PER_SECTOR 512U
#define APP_STORAGE_INSTALL_INDEX_MAGIC   0x58444E49UL /* "INDX" */
#define APP_STORAGE_INSTALL_INDEX_VERSION 1U
#define APP_STORAGE_INSTALL_INDEX_SLOT_COUNT 2UL
#define APP_STORAGE_INSTALL_INDEX_SLOT_SIZE APP_STORAGE_SMOKE_SECTOR_SIZE
#define APP_STORAGE_INSTALL_INDEX_RESERVED_BYTES (APP_STORAGE_INSTALL_INDEX_SLOT_COUNT * APP_STORAGE_INSTALL_INDEX_SLOT_SIZE)
#define APP_STORAGE_INSTALLED_DATA_END_ADDR ((uint32_t)((uint64_t)KNOB_STORAGE_INSTALLED_BASE_ADDR + (uint64_t)KNOB_STORAGE_INSTALLED_SIZE_BYTES - (uint64_t)APP_STORAGE_INSTALL_INDEX_RESERVED_BYTES))
#define APP_STORAGE_INSTALL_INDEX_SLOT0_ADDR (APP_STORAGE_INSTALLED_DATA_END_ADDR)
#define APP_STORAGE_INSTALL_INDEX_SLOT1_ADDR ((uint32_t)APP_STORAGE_INSTALL_INDEX_SLOT0_ADDR + (uint32_t)APP_STORAGE_INSTALL_INDEX_SLOT_SIZE)
#define APP_STORAGE_INSTALL_INDEX_RECORD_CRC_SPAN ((uint32_t)offsetof(app_storage_install_index_record_t, record_crc32))
#define APP_STORAGE_INSTALL_INDEX_SLOT_INVALID 0xFFFFFFFFUL
#define APP_STORAGE_FILEX_VOLUME_NAME  "PEEPSHOW   "
#define APP_STORAGE_FILEX_MEDIA_NAME   "PS_FAT"
#define APP_STORAGE_FAT_MANIFEST_PATH_PRIMARY  "INBOX/MANIFEST.BIN"
#define APP_STORAGE_FAT_MANIFEST_PATH_FALLBACK "MANIFEST.BIN"
#define APP_STORAGE_FAT_SCENE_MAP_PATH_PRIMARY      "INBOX/SCENE_MAP.BIN"
#define APP_STORAGE_FAT_SCENE_MAP_PATH_FALLBACK     "SCENE_MAP.BIN"
#define APP_STORAGE_FAT_SCENE_TILESET_PATH_PRIMARY  "INBOX/SCENE_TILESET.BIN"
#define APP_STORAGE_FAT_SCENE_TILESET_PATH_FALLBACK "SCENE_TILESET.BIN"
#define APP_STORAGE_FILEX_NUM_FATS     2U
#define APP_STORAGE_SETTINGS_SECTOR_SIZE 4096UL
#define APP_STORAGE_JOYCFG_MAGIC       0x4A594346UL
#define APP_STORAGE_JOYCFG_VERSION_V2  2UL
#define APP_STORAGE_JOYCFG_VERSION_V3  3UL
#define APP_STORAGE_USER_GAIN_MIN_PCT  0UL
#define APP_STORAGE_USER_GAIN_MAX_PCT  300UL
#define APP_STORAGE_USER_GAIN_DEFAULT_PCT 100UL
#define APP_STORAGE_AUDIO_CATALOG_MAGIC   0x43445541UL /* 'AUDC' */
#define APP_STORAGE_AUDIO_CATALOG_VERSION 1U
#define APP_STORAGE_AUDIO_CHUNK_CACHE_DEPTH 16UL
#define APP_STORAGE_AUDIO_CATALOG_MAX_ENTRIES 128UL
#define APP_JOY_CAL_DIR_COUNT          4U
#define APP_JOY_CAL_DIR_UP_INDEX       0U
#define APP_JOY_CAL_DIR_RIGHT_INDEX    1U
#define APP_JOY_CAL_DIR_DOWN_INDEX     2U
#define APP_JOY_CAL_DIR_LEFT_INDEX     3U
#define APP_JOY_CAL_MIN_DIR_SAMPLES    8U
#define APP_JOY_CAL_QUALITY_SPAN_RATIO_MAX    1.70f
#define APP_JOY_CAL_QUALITY_AXIS_ERROR_MAX    0.50f
#define APP_JOY_CAL_QUALITY_NORM_MIN_MIN      0.65f
#define APP_JOY_CAL_QUALITY_NORM_MAX_MAX      1.45f

#define APP_STORAGE_OP_NONE            0UL
#define APP_STORAGE_OP_FLASH_PROBE     1UL
#define APP_STORAGE_OP_RAW_SMOKE       2UL
#define APP_STORAGE_OP_FILEX_MOUNT     3UL
#define APP_STORAGE_OP_FILEX_FORMAT    4UL
#define APP_STORAGE_OP_FILEX_UNMOUNT   5UL
#define APP_STORAGE_OP_JOYCFG_LOAD     6UL
#define APP_STORAGE_OP_JOYCFG_SAVE     7UL
#define APP_STORAGE_OP_AUDIO_CATALOG_LOAD 8UL
#define APP_STORAGE_OP_AUDIO_CHUNK_READ   9UL
#define APP_STORAGE_OP_AUDIO_CATALOG_INSTALL 10UL
#define APP_STORAGE_OP_GAME_PACKAGE_MANIFEST_LOAD 11UL
#define APP_STORAGE_OP_GAME_PACKAGE_MANIFEST_LOAD_DEFAULT 12UL
#define APP_STORAGE_OP_GAME_PACKAGE_MANIFEST_ERASE 13UL
#define APP_STORAGE_OP_RAW_APP_ERASE 14UL
#define APP_STORAGE_OP_GAME_PACKAGE_MANIFEST_WRITE_TEST 15UL
#define APP_STORAGE_OP_SCENE_MAP_LOAD 16UL
#define APP_STORAGE_OP_SCENE_TILESET_LOAD 17UL
#define APP_STORAGE_OP_INSTALL_INDEX_LOAD 18UL
#define APP_STORAGE_OP_INSTALL_INDEX_WRITE 19UL
#define APP_STORAGE_OP_USB_MSC_READ 20UL
#define APP_STORAGE_OP_USB_MSC_WRITE 21UL
#define APP_STORAGE_OP_USB_MSC_FLUSH 22UL
#define APP_STORAGE_OP_USB_MSC_STATUS 23UL
#define APP_STORAGE_OP_GAME_PACKAGE_MANIFEST_IMPORT 24UL
#define APP_STORAGE_OP_GAME_PACKAGE_SCENE_IMPORT 25UL

#define APP_STORAGE_PKG_SRC_NONE 0UL
#define APP_STORAGE_PKG_SRC_EXPLICIT_RAW 1UL
#define APP_STORAGE_PKG_SRC_DEFAULT_SLOT 2UL
#define APP_STORAGE_PKG_SRC_INSTALL_INDEX 3UL

#define APP_STORAGE_ERR_NONE           0L
#define APP_STORAGE_ERR_BOOTINIT       (-101L)
#define APP_STORAGE_ERR_ALIGN          (-102L)
#define APP_STORAGE_ERR_RANGE          (-103L)
#define APP_STORAGE_ERR_ERASE          (-104L)
#define APP_STORAGE_ERR_ERASE_VERIFY   (-105L)
#define APP_STORAGE_ERR_PROGRAM        (-106L)
#define APP_STORAGE_ERR_PROGRAM_VERIFY (-107L)
#define APP_STORAGE_ERR_FINAL_ERASE    (-108L)
#define APP_STORAGE_ERR_FINAL_VERIFY   (-109L)
#define APP_STORAGE_ERR_FILEX_MOUNT    (-110L)
#define APP_STORAGE_ERR_FILEX_FORMAT   (-111L)
#define APP_STORAGE_ERR_FILEX_UNMOUNT  (-112L)
#define APP_STORAGE_ERR_JOYCFG_READ    (-113L)
#define APP_STORAGE_ERR_JOYCFG_INVALID (-114L)
#define APP_STORAGE_ERR_JOYCFG_ERASE   (-115L)
#define APP_STORAGE_ERR_JOYCFG_PROGRAM (-116L)
#define APP_STORAGE_ERR_JOYCFG_VERIFY  (-117L)
#define APP_STORAGE_ERR_JOYCFG_NODATA  (-118L)
#define APP_STORAGE_ERR_JOYCFG_RANGE   (-119L)
#define APP_STORAGE_ERR_AUDIO_RANGE    (-120L)
#define APP_STORAGE_ERR_AUDIO_READ     (-121L)
#define APP_STORAGE_ERR_AUDIO_CATALOG  (-122L)
#define APP_STORAGE_ERR_AUDIO_CHUNK_SIZE (-123L)
#define APP_STORAGE_ERR_AUDIO_INSTALL  (-124L)
#define APP_STORAGE_ERR_GAME_PACKAGE_MANIFEST (-125L)
#define APP_STORAGE_ERR_GAME_MAP       (-126L)
#define APP_STORAGE_ERR_GAME_TILESET   (-127L)
#define APP_STORAGE_ERR_USB_MSC        (-128L)

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */
static TX_THREAD g_th_display;
static ULONG g_th_display_stack[KNOB_RTOS_DISPLAY_THREAD_STACK_BYTES / sizeof(ULONG)];

static TX_QUEUE g_q_display_cmd;
static ULONG g_q_display_cmd_storage[KNOB_RTOS_QDISPLAY_CMD_DEPTH * ((ULONG)APP_DISPLAY_CMD_WORDS)];
static volatile ULONG g_display_present_pending;
static ULONG g_display_present_post_count;
static ULONG g_display_present_coalesce_count;
static ULONG g_display_present_send_fail_count;
static TX_MUTEX g_mtx_renderer;
static TX_MUTEX g_mtx_retained;
static ULONG g_renderer_lock_error_count;

static TX_THREAD g_th_storage;
static ULONG g_th_storage_stack[KNOB_RTOS_STORAGE_THREAD_STACK_BYTES / sizeof(ULONG)];

static TX_QUEUE g_q_storage_req;
static ULONG g_q_storage_req_storage[KNOB_RTOS_QSTORAGE_REQ_DEPTH * ((ULONG)APP_STORAGE_REQ_WORDS)];
static TX_MUTEX g_mtx_storage_usb_msc;
static TX_SEMAPHORE g_sem_storage_usb_msc_done;
static volatile UINT g_storage_usb_msc_status;
static volatile ULONG g_storage_usb_msc_media_status;
static volatile ULONG g_storage_usb_msc_req_read_count;
static volatile ULONG g_storage_usb_msc_req_write_count;
static volatile ULONG g_storage_usb_msc_req_flush_count;
static volatile ULONG g_storage_usb_msc_req_status_count;
static volatile ULONG g_storage_usb_msc_req_fail_count;
static volatile ULONG g_storage_usb_msc_req_mutex_fail_count;
static volatile ULONG g_storage_usb_msc_req_queue_fail_count;
static volatile ULONG g_storage_usb_msc_req_done_fail_count;
static volatile ULONG g_storage_usb_msc_req_mode_reject_count;
static volatile ULONG g_storage_usb_msc_last_req_type;
static volatile ULONG g_storage_usb_msc_last_lba;
static volatile ULONG g_storage_usb_msc_last_blocks;
static volatile ULONG g_storage_usb_msc_last_req_status;
static volatile ULONG g_storage_usb_msc_last_media_status;

static TX_THREAD g_th_input;
static ULONG g_th_input_stack[KNOB_RTOS_INPUT_THREAD_STACK_BYTES / sizeof(ULONG)];
static TX_THREAD g_th_ui;
static ULONG g_th_ui_stack[KNOB_RTOS_INPUT_THREAD_STACK_BYTES / sizeof(ULONG)];
static TX_THREAD g_th_game;
static ULONG g_th_game_stack[KNOB_RTOS_GAME_THREAD_STACK_BYTES / sizeof(ULONG)];

static TX_QUEUE g_q_input_cmd;
static ULONG g_q_input_cmd_storage[KNOB_RTOS_QINPUT_CMD_DEPTH * ((ULONG)APP_INPUT_CMD_WORDS)];
static TX_QUEUE g_q_input_raw;
static ULONG g_q_input_raw_storage[KNOB_RTOS_QINPUT_RAW_DEPTH * ((ULONG)APP_INPUT_RAW_WORDS)];
static TX_QUEUE g_q_ui_events;
static ULONG g_q_ui_events_storage[KNOB_RTOS_QUI_EVENTS_DEPTH * ((ULONG)APP_INPUT_ACTION_WORDS)];
static TX_QUEUE g_q_game_events;
static ULONG g_q_game_events_storage[KNOB_RTOS_QGAME_EVENTS_DEPTH * ((ULONG)APP_INPUT_ACTION_WORDS)];

static TX_THREAD g_th_audio;
static ULONG g_th_audio_stack[KNOB_RTOS_AUDIO_THREAD_STACK_BYTES / sizeof(ULONG)];

static TX_QUEUE g_q_audio_cmd;
static ULONG g_q_audio_cmd_storage[KNOB_RTOS_QAUDIO_CMD_DEPTH * ((ULONG)APP_AUDIO_CMD_WORDS)];

static TX_THREAD g_th_power;
static ULONG g_th_power_stack[KNOB_RTOS_POWER_THREAD_STACK_BYTES / sizeof(ULONG)];

static TX_QUEUE g_q_sys_events;
static ULONG g_q_sys_events_storage[KNOB_RTOS_QSYS_EVENTS_DEPTH * ((ULONG)APP_QSYS_EVENT_WORDS)];

static TX_THREAD g_th_sensor;
static ULONG g_th_sensor_stack[KNOB_RTOS_SENSOR_THREAD_STACK_BYTES / sizeof(ULONG)];

static TX_QUEUE g_q_sensor_req;
static ULONG g_q_sensor_req_storage[KNOB_RTOS_QSENSOR_REQ_DEPTH * ((ULONG)APP_SENSOR_REQ_WORDS)];

static TX_EVENT_FLAGS_GROUP g_eg_mode;
static TX_EVENT_FLAGS_GROUP g_eg_power;
static TX_EVENT_FLAGS_GROUP g_eg_sensor_health;
static TX_EVENT_FLAGS_GROUP g_eg_audio_dma;
static ULONG g_power_pending_ack_mask;
static ULONG g_power_quiesce_wait_active;
static ULONG g_power_quiesce_wait_elapsed_ticks;
static ULONG g_power_stop2_armed;
static ULONG g_power_stop2_entry_count;
static ULONG g_power_stop2_wake_count;
static ULONG g_power_stop2_abort_count;
static ULONG g_power_stop2_last_wusr;
static ULONG g_power_stop2_last_sr;
static LONG g_power_stop2_last_error;
volatile ULONG g_power_stop2_tb_sample_count __attribute__((used));
volatile ULONG g_power_stop2_tb_last_hal_dt __attribute__((used));
volatile ULONG g_power_stop2_tb_last_tx_dt __attribute__((used));
volatile ULONG g_power_stop2_tb_last_abs_diff __attribute__((used));
volatile ULONG g_power_stop2_tb_max_abs_diff __attribute__((used));
volatile ULONG g_power_stop2_tb_last_wake_count __attribute__((used));
volatile ULONG g_power_stop2_tb_persist_magic __attribute__((used));
volatile ULONG g_power_stop2_tb_persist_load_ok __attribute__((used));
static ULONG g_power_stop2_tb_prev_hal_tick;
static ULONG g_power_stop2_tb_prev_tx_tick;
static ULONG g_power_stop2_tb_prev_valid;
static APP_RETAIN_ATTR app_retained_state_blob_t g_retained_state_blob;
volatile ULONG g_retained_state_magic __attribute__((used));
volatile ULONG g_retained_state_valid_mask __attribute__((used));
volatile ULONG g_retained_state_seq __attribute__((used));
volatile ULONG g_retained_state_crc_ok __attribute__((used));
volatile ULONG g_retained_state_load_ok_count __attribute__((used));
volatile ULONG g_retained_state_load_fail_count __attribute__((used));
volatile ULONG g_retained_state_save_ok_count __attribute__((used));
volatile ULONG g_retained_state_save_fail_count __attribute__((used));
volatile ULONG g_retained_state_game_restore_ok_count __attribute__((used));
volatile ULONG g_retained_state_game_restore_fail_count __attribute__((used));
volatile ULONG g_retained_state_game_save_ok_count __attribute__((used));
volatile ULONG g_retained_state_game_save_fail_count __attribute__((used));
volatile ULONG g_retained_state_game_mode_id __attribute__((used));
volatile ULONG g_retained_state_game_backend_id __attribute__((used));
volatile ULONG g_retained_state_game_topdown_valid __attribute__((used));
static app_sensor_fsm_t g_sensor_pmic;
static app_sensor_fsm_t g_sensor_tmag;
static app_sensor_fsm_t g_sensor_lis;
static app_sensor_pmic_live_t g_sensor_pmic_live;
static app_sensor_lis_live_t g_sensor_lis_live;
static app_sensor_lis_profile_t g_sensor_lis_profile_requested;
static app_sensor_lis_profile_t g_sensor_lis_profile_applied;
static ULONG g_sensor_lis_stream_enabled;
static ULONG g_sensor_lis_step_enabled_requested;
static ULONG g_sensor_bus_fault;
static ULONG g_sensor_mode_token;
static ULONG g_sensor_pmic_vbus_present;
static ULONG g_sensor_pmic_vbus_known;
static ULONG g_usb_flash_prompt_pending;
static ULONG g_usb_flash_prompt_prompted_this_vbus;
static ULONG g_usb_device_active;
static ULONG g_usb_device_start_ok_count;
static ULONG g_usb_device_start_fail_count;
static ULONG g_usb_device_stop_ok_count;
static ULONG g_usb_device_stop_fail_count;
static LONG g_usb_device_last_error;
static volatile ULONG g_usb_msc_recover_pending;
static volatile ULONG g_usb_msc_recover_reason;
static volatile ULONG g_usb_msc_recover_last_req_type;
static volatile ULONG g_usb_msc_recover_last_req_status;
static volatile ULONG g_usb_msc_recover_last_media_status;
static volatile ULONG g_usb_msc_recover_last_mode_flags;
static volatile ULONG g_usb_msc_recover_last_usb_active_before;
static volatile ULONG g_usb_msc_recover_last_usb_active_after;
static volatile ULONG g_usb_msc_recover_trigger_count;
static volatile ULONG g_usb_msc_recover_attempt_count;
static volatile ULONG g_usb_msc_recover_ok_count;
static volatile ULONG g_usb_msc_recover_fail_count;
static ULONG g_storage_flash_ready;
static ULONG g_storage_flash_quiesced;
static ULONG g_storage_flash_in_dpd;
static ULONG g_storage_ospi_clock_enabled;
static ULONG g_storage_last_op;
static LONG g_storage_last_error;
static ULONG g_storage_last_jedec_id;
static ULONG g_storage_smoke_pass_count;
static ULONG g_storage_smoke_fail_count;
static ULONG g_storage_filex_mounted;
static ULONG g_storage_filex_mount_count;
static ULONG g_storage_filex_mount_fail_count;
static ULONG g_storage_filex_format_count;
static ULONG g_storage_filex_unmount_count;
static ULONG g_storage_filex_unmount_fail_count;
static ULONG g_storage_filex_last_status;
static LX_NOR_FLASH g_storage_usb_msc_lx_flash;
static ULONG g_storage_usb_msc_lx_opened;
static ULONG g_storage_joycfg_valid;
static LONG g_storage_joycfg_last_error;
static ULONG g_storage_joycfg_load_ok_count;
static ULONG g_storage_joycfg_load_fail_count;
static ULONG g_storage_joycfg_save_ok_count;
static ULONG g_storage_joycfg_save_fail_count;
static ULONG g_storage_joycfg_load_seq;
static ULONG g_storage_joycfg_save_seq;
static TMAGJoy_Cal g_storage_joycfg_cal;
static ULONG g_storage_joycfg_deadzone_enabled;
static float g_storage_joycfg_deadzone_mT;
static ULONG g_storage_audio_catalog_loaded;
static ULONG g_storage_audio_catalog_load_ok_count;
static ULONG g_storage_audio_catalog_load_fail_count;
static ULONG g_storage_audio_catalog_install_ok_count;
static ULONG g_storage_audio_catalog_install_fail_count;
static ULONG g_storage_audio_catalog_install_last_bytes;
static ULONG g_storage_audio_catalog_addr;
static ULONG g_storage_audio_catalog_entry_count;
static ULONG g_storage_audio_catalog_version;
static ULONG g_storage_audio_catalog_table_crc32;
static ULONG g_storage_audio_chunk_read_ok_count;
static ULONG g_storage_audio_chunk_read_fail_count;
static ULONG g_storage_audio_chunk_last_addr;
static ULONG g_storage_audio_chunk_last_len;
static ULONG g_storage_audio_chunk_last_token;
static ULONG g_storage_audio_chunk_last_crc32;
static ULONG g_storage_audio_chunk_cache_seq;
static ULONG g_storage_game_package_manifest_loaded;
static ULONG g_storage_game_package_manifest_load_ok_count;
static ULONG g_storage_game_package_manifest_load_fail_count;
static ULONG g_storage_game_package_manifest_import_ok_count;
static ULONG g_storage_game_package_manifest_import_fail_count;
static ULONG g_storage_game_package_manifest_import_last_status;
static ULONG g_storage_game_package_manifest_import_last_bytes;
static ULONG g_storage_game_package_scene_import_ok_count;
static ULONG g_storage_game_package_scene_import_fail_count;
static ULONG g_storage_game_package_scene_import_last_status;
static ULONG g_storage_game_package_scene_import_map_bytes;
static ULONG g_storage_game_package_scene_import_tileset_bytes;
static ULONG g_storage_game_package_manifest_addr;
static ULONG g_storage_game_package_manifest_size;
static ULONG g_storage_game_package_manifest_last_status;
static ULONG g_storage_game_package_id;
static ULONG g_storage_game_package_version;
static ULONG g_storage_game_package_mode_count;
static ULONG g_storage_game_package_pet_route_count;
static ULONG g_storage_game_package_source;
static ULONG g_storage_install_index_valid;
static ULONG g_storage_install_index_load_ok_count;
static ULONG g_storage_install_index_load_fail_count;
static ULONG g_storage_install_index_write_ok_count;
static ULONG g_storage_install_index_write_fail_count;
static ULONG g_storage_install_index_active_slot;
static ULONG g_storage_install_index_sequence;
static ULONG g_storage_install_index_package_id;
static ULONG g_storage_install_index_package_version;
static ULONG g_storage_install_index_blob_offset;
static ULONG g_storage_install_index_blob_size;
static ULONG g_storage_install_index_blob_crc32;
static ULONG g_storage_install_index_manifest_addr;
static ULONG g_storage_install_index_manifest_size;
static ULONG g_storage_install_index_manifest_crc32;
static ULONG g_storage_install_index_record_crc32;
static ULONG g_storage_install_index_load_last_status;
static ULONG g_storage_install_index_write_last_status;
static ULONG g_storage_install_index_scan_done;
static ULONG g_storage_game_package_manifest_erase_ok_count;
static ULONG g_storage_game_package_manifest_erase_fail_count;
static ULONG g_storage_raw_app_erase_ok_count;
static ULONG g_storage_raw_app_erase_fail_count;
static ULONG g_storage_scene_map_loaded;
static ULONG g_storage_scene_map_load_ok_count;
static ULONG g_storage_scene_map_load_fail_count;
static ULONG g_storage_scene_map_addr;
static ULONG g_storage_scene_map_size;
static ULONG g_storage_scene_map_last_status;
static ULONG g_storage_scene_map_width;
static ULONG g_storage_scene_map_height;
static ULONG g_storage_scene_map_tile_count;
static ULONG g_storage_scene_map_object_count;
static ULONG g_storage_scene_tileset_loaded;
static ULONG g_storage_scene_tileset_load_ok_count;
static ULONG g_storage_scene_tileset_load_fail_count;
static ULONG g_storage_scene_tileset_addr;
static ULONG g_storage_scene_tileset_size;
static ULONG g_storage_scene_tileset_last_status;
static ULONG g_storage_scene_tileset_tile_width;
static ULONG g_storage_scene_tileset_tile_height;
static ULONG g_storage_scene_tileset_tile_count;
static ULONG g_storage_scene_tileset_base_gid;
static ULONG g_storage_last_erase_addr;
static ULONG g_storage_last_erase_size;
static app_storage_audio_catalog_header_t g_storage_audio_catalog_header;
static app_storage_audio_catalog_entry_t g_storage_audio_catalog_entries[APP_STORAGE_AUDIO_CATALOG_MAX_ENTRIES];
static uint8_t g_storage_audio_chunk_buf[APP_STORAGE_AUDIO_CHUNK_MAX_BYTES];
static uint8_t g_storage_game_package_manifest_buf[GAME_PACKAGE_MANIFEST_MAX_BYTES];
static uint8_t g_storage_scene_map_blob_buf[GAME_MAP_BLOB_MAX_BYTES];
static uint8_t g_storage_scene_tileset_blob_buf[GAME_TILESET_BLOB_MAX_BYTES];
static app_storage_audio_chunk_cache_entry_t g_storage_audio_chunk_cache[APP_STORAGE_AUDIO_CHUNK_CACHE_DEPTH];
const ULONG g_storage_audio_catalog_addr_dbg = (ULONG)KNOB_STORAGE_AUDIO_CATALOG_ADDR;
const ULONG g_storage_audio_catalog_max_bytes_dbg = (ULONG)KNOB_STORAGE_AUDIO_CATALOG_MAX_BYTES;
const ULONG g_storage_settings_addr_dbg = (ULONG)KNOB_STORAGE_SETTINGS_ADDR;
const ULONG g_storage_smoke_addr_dbg = (ULONG)KNOB_STORAGE_SMOKE_ADDR;
const ULONG g_storage_smoke_len_dbg = (ULONG)APP_STORAGE_SMOKE_SECTOR_SIZE;
const ULONG g_storage_game_pkg_manifest_addr_dbg = (ULONG)KNOB_STORAGE_GAME_PKG_MANIFEST_ADDR;
const ULONG g_storage_game_pkg_manifest_max_bytes_dbg = (ULONG)KNOB_STORAGE_GAME_PKG_MANIFEST_MAX_BYTES;
const ULONG g_storage_installed_base_addr_dbg = (ULONG)KNOB_STORAGE_INSTALLED_BASE_ADDR;
const ULONG g_storage_installed_size_bytes_dbg = (ULONG)KNOB_STORAGE_INSTALLED_SIZE_BYTES;
const ULONG g_storage_installed_data_end_addr_dbg = (ULONG)APP_STORAGE_INSTALLED_DATA_END_ADDR;
const ULONG g_storage_install_index_slot0_addr_dbg = (ULONG)APP_STORAGE_INSTALL_INDEX_SLOT0_ADDR;
const ULONG g_storage_install_index_slot1_addr_dbg = (ULONG)APP_STORAGE_INSTALL_INDEX_SLOT1_ADDR;
const ULONG g_storage_install_index_slot_size_bytes_dbg = (ULONG)APP_STORAGE_INSTALL_INDEX_SLOT_SIZE;
const ULONG g_storage_install_index_record_size_dbg = (ULONG)sizeof(app_storage_install_index_record_t);
const ULONG g_storage_install_index_record_crc_offset_dbg = (ULONG)APP_STORAGE_INSTALL_INDEX_RECORD_CRC_SPAN;
const ULONG g_storage_scene_tileset_addr_dbg = (ULONG)KNOB_GAME_RT_SCENE_TILESET_ADDR;
const ULONG g_storage_scene_tileset_size_bytes_dbg = (ULONG)KNOB_GAME_RT_SCENE_TILESET_SIZE_BYTES;
const ULONG g_storage_fat_base_addr_dbg = (ULONG)KNOB_STORAGE_FAT_BASE_ADDR;
const ULONG g_storage_fat_size_bytes_dbg = (ULONG)KNOB_STORAGE_FAT_SIZE_BYTES;
const ULONG g_storage_filex_cache_bytes_dbg = (ULONG)KNOB_STORAGE_FILEX_CACHE_BYTES;
const ULONG g_storage_filex_spc_dbg = (ULONG)KNOB_STORAGE_FILEX_SECTORS_PER_CLUSTER;
const ULONG g_storage_filex_dir_entries_dbg = (ULONG)KNOB_STORAGE_FILEX_DIR_ENTRIES;
static AT25_Debug g_storage_at25_dbg;
static FX_MEDIA g_storage_fx_media;
static UCHAR g_storage_filex_cache[KNOB_STORAGE_FILEX_CACHE_BYTES];
extern SPI_HandleTypeDef hspi3;
extern SAI_HandleTypeDef hsai_BlockA1;
extern OSPI_HandleTypeDef hospi1;
extern RTC_HandleTypeDef hrtc;
static LS013B7DH05 g_display_dev;
static uint8_t g_display_ready;
static uint16_t g_display_dirty_rows[DISPLAY_HEIGHT];
static ULONG g_dbg_display_stack_min_sp;
static ULONG g_dbg_display_stack_sample_count;
static int16_t g_audio_dma_buffer[KNOB_AUDIO_DMA_FRAMES * APP_AUDIO_CHANNEL_COUNT];
static volatile ULONG g_audio_dma_events;
static volatile ULONG g_audio_dma_half_pending;
static volatile ULONG g_audio_dma_full_pending;
static volatile ULONG g_audio_dma_error_pending;
static volatile ULONG g_audio_state;
static ULONG g_audio_start_count;
static ULONG g_audio_stop_count;
static ULONG g_audio_restart_count;
static ULONG g_audio_underflow_count;
static ULONG g_audio_half_irq_count;
static ULONG g_audio_full_irq_count;
static ULONG g_audio_error_irq_count;
static ULONG g_audio_half_missed_count;
static ULONG g_audio_full_missed_count;
static ULONG g_audio_error_missed_count;
static volatile LONG g_audio_last_error;
static uint32_t g_audio_phase_accum;
static uint32_t g_audio_phase_step;
static ULONG g_audio_source_kind;
static app_audio_voice_t g_audio_music_voice;
static app_audio_voice_t g_audio_sfx_voices[KNOB_AUDIO_SFX_VOICE_COUNT];
static ULONG g_audio_voice_seq_counter;
static ULONG g_audio_sfx_voice_steal_count;
static ULONG g_audio_sfx_voice_peak_active;
static ULONG g_audio_play_event_count;
static ULONG g_audio_play_clip_count;
static ULONG g_audio_cmd_post_count;
static ULONG g_audio_cmd_drop_count;
static ULONG g_audio_last_event;
static ULONG g_audio_last_clip;
static ULONG g_audio_next_sfx_gain_class;
static ULONG g_audio_sfx_autostop_armed;
static ULONG g_audio_sfx_autostop_tick;
static ULONG g_audio_power_boost_asserted;
static ULONG g_audio_storage_req_token_seq;
static ULONG g_audio_user_gain_master_pct;
static ULONG g_audio_user_gain_music_pct;
static ULONG g_audio_user_gain_sfx_pct;
static ULONG g_audio_user_gain_ui_pct;
static volatile ULONG g_input_raw_post_count;
static volatile ULONG g_input_raw_drop_count;
static ULONG g_input_raw_recv_count;
static ULONG g_input_raw_suppressed_count;
static ULONG g_input_last_source;
static ULONG g_input_last_edge;
static ULONG g_input_last_tick;
static ULONG g_input_last_level;
static ULONG g_input_quiesced;
static ULONG g_input_action_total_count;
static ULONG g_input_action_ui_route_count;
static ULONG g_input_action_game_route_count;
static ULONG g_input_action_system_route_count;
static ULONG g_input_action_ignored_count;
static ULONG g_input_action_last;
static ULONG g_input_action_last_mode;
static ULONG g_input_action_ui_post_count;
static ULONG g_input_action_ui_drop_count;
static ULONG g_input_action_ui_drop_oldest_count;
static ULONG g_input_action_game_post_count;
static ULONG g_input_action_game_drop_count;
static ULONG g_input_action_game_drop_oldest_count;
static ULONG g_ui_event_recv_count;
static ULONG g_ui_event_last_action;
static ULONG g_ui_event_handled_count;
static ULONG g_ui_event_ignored_count;
static ULONG g_ui_event_queue_error_count;
static ULONG g_game_event_recv_count;
static ULONG g_game_event_last_action;
static ULONG g_game_event_handled_count;
static ULONG g_game_event_ignored_count;
static ULONG g_game_event_stale_drop_count;
static ULONG g_game_event_queue_error_count;
static app_input_button_state_t g_input_button_state[APP_INPUT_SOURCE_COUNT];
static ULONG g_input_debounce_drop_count;
static ULONG g_input_release_pass_count;
static ULONG g_input_release_reconcile_count;
static ULONG g_input_repeat_emit_count;
static ULONG g_input_long_emit_count;
static ULONG g_input_sys_activity_post_count;
static ULONG g_input_sys_activity_drop_count;
static ULONG g_input_sys_activity_last_tick;
static ULONG g_input_sys_menu_post_count;
static ULONG g_input_sys_menu_drop_count;
static ULONG g_input_stop_wake_pending_mask;
static ULONG g_input_stop_wake_pending_tick[APP_INPUT_SOURCE_COUNT];
static ULONG g_input_physical_idle_level[APP_INPUT_SOURCE_COUNT];
static ULONG g_input_physical_idle_valid_mask;
static ULONG g_power_input_activity_count;
static ULONG g_power_last_input_tick;
static ULONG g_power_menu_event_count;
static ULONG g_power_stop_select_active;
static ULONG g_power_stop_select_last_input_tick;
static ULONG g_pet_state;
static ULONG g_pet_tick_count;
static ULONG g_pet_wake_count;
static ULONG g_pet_last_action;
static ULONG g_pet_hunger_pct;
static ULONG g_pet_energy_pct;
static ULONG g_pet_mood_pct;
static ULONG g_game_exit_to_static_pending;
static ULONG g_power_perf_profile_current;
static ULONG g_power_perf_profile_target;
static ULONG g_power_perf_last_switch_tick;
static ULONG g_power_perf_hint_seq;
static ULONG g_power_perf_hint_post_count;
static ULONG g_power_perf_hint_drop_count;
static ULONG g_power_perf_hint_rx_count;
static ULONG g_power_perf_hint_inflight;
static ULONG g_power_perf_force_up_no_dwell;
static ULONG g_power_perf_audio_boost_active;
static ULONG g_power_perf_last_present_ticks;
static volatile ULONG g_power_perf_last_draw_ticks;
static ULONG g_power_perf_last_dirty_rows;
static ULONG g_power_perf_last_full_flush;
static ULONG g_power_perf_miss_streak;
static ULONG g_power_perf_headroom_streak;
static ULONG g_power_perf_up_count;
static ULONG g_power_perf_down_count;
static ULONG g_power_perf_dwell_block_count;
static ULONG g_power_perf_clock_apply_fail_count;
static ULONG g_power_perf_clock_apply_last_stage;
static LONG g_power_perf_clock_apply_last_hal;
static ULONG g_power_perf_base_sysclk_mhz;
static const app_power_perf_profile_t g_power_perf_profiles[] =
{
  /* Profile 0 maps to CubeMX base clock restored by AppPowerPerfApplyNormClock. */
  {24UL, 0UL, 0UL},
  {48UL, 12UL, 4UL},
  {64UL, 8UL, 2UL},
  {80UL, 10UL, 2UL},
  {120UL, 15UL, 2UL},
  {160UL, 10UL, 1UL}
};
static ULONG g_ui_mode_flags_last;
static ULONG g_ui_bootstrap_present_pending;
static ULONG g_ui_boot_ready;
static ui_router_t g_ui_router;
static app_joy_cal_status_t g_sensor_joy_cal_status;
static ULONG g_sensor_joy_cal_active;
static ULONG g_sensor_joy_cal_stage;
static app_joy_cal_capture_t g_sensor_joy_cal_capture;
static float g_sensor_joy_cal_dir_avg_x[APP_JOY_CAL_DIR_COUNT];
static float g_sensor_joy_cal_dir_avg_y[APP_JOY_CAL_DIR_COUNT];
static ULONG g_sensor_joy_cal_wait_confirm;
static TMAGJoy_Cal g_sensor_joy_cal_snapshot;
static ULONG g_sensor_joy_cal_snapshot_valid;
static ULONG g_sensor_joy_input_gate_snapshot_valid;
static ULONG g_sensor_joycfg_seen_load_seq;
static ULONG g_sensor_joycfg_seen_save_seq;
static TMAGJoy *g_sensor_joy;
static ULONG g_sensor_joy_input_mask;
static ULONG g_sensor_joy_input_gate_valid;
static ULONG g_sensor_joy_input_neutral_armed;
static ULONG g_sensor_joy_input_neutral_stable_count;
static ULONG g_sensor_joy_release_stable_count;
static ULONG g_sensor_joy_live_read_fail_streak;
static app_joy_live_status_t g_sensor_joy_live_status;
static app_joy_cal_quality_t g_sensor_joy_cal_quality;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */
void SystemClock_Config(void);
static VOID AppInputThreadEntry(ULONG thread_input);
static VOID AppUiThreadEntry(ULONG thread_input);
static VOID AppGameThreadEntry(ULONG thread_input);
static uint32_t AppRetainedStateCrc32(const uint8_t *data, uint32_t len);
static uint8_t AppRetainedStateBlobHeaderValid(const app_retained_state_blob_t *blob);
static uint8_t AppRetainedStateBlobCrcValid(const app_retained_state_blob_t *blob);
static VOID AppRetainedStateSyncDebugFromBlob(const app_retained_state_blob_t *blob, uint8_t crc_ok);
static VOID AppRetainedStateBlobReset(app_retained_state_blob_t *blob);
static VOID AppRetainedStateBlobFinalize(app_retained_state_blob_t *blob);
static UINT AppRetainedStateRestorePet(void);
static UINT AppRetainedStateSavePet(void);
static UINT AppRetainedStateSaveGameTopdown(uint32_t mode_id,
                                            uint32_t backend_id,
                                            uint32_t scene_map_id,
                                            uint32_t scene_tileset_id,
                                            const game_mode_topdown_basic_snapshot_t *snapshot);
static UINT AppRetainedStateRestoreGameTopdown(uint32_t mode_id,
                                               uint32_t backend_id,
                                               game_mode_topdown_basic_snapshot_t *snapshot_out,
                                               uint32_t *scene_map_id_out,
                                               uint32_t *scene_tileset_id_out);
static UINT AppRetainedStateClearGame(void);
static uint8_t AppSensorJoyCalSane(const TMAGJoy_Cal *cal);
static VOID AppStorageInstallIndexClearActive(void);
static uint8_t AppStorageInstallIndexSeqNewer(uint32_t candidate_seq, uint32_t baseline_seq);
static uint8_t AppStorageInstallIndexRecordValidate(const app_storage_install_index_record_t *record);
static VOID AppStorageInstallIndexPublish(const app_storage_install_index_record_t *record, uint32_t slot_index);
static UINT AppStorageInstallIndexLoad(void);
static UINT AppStorageInstallIndexCommit(uint32_t manifest_addr, uint32_t manifest_size, const uint8_t *manifest_data);
static UINT AppStorageEraseRange4K(uint32_t erase_addr, uint32_t erase_size);
static ULONG AppStorageUserGainClampPct(ULONG pct);
static float AppStorageDeadzoneClampMt(float deadzone_mT);
static VOID AppStorageJoyCfgApplyRuntimeDefaults(void);
static UINT AppModeTokenToFlag(app_mode_t mode_token, ULONG *mode_flag_out);
static UINT AppSetModeFlag(app_mode_t mode_token);
static UINT AppSysEventPost(app_sys_event_type_t event_type, ULONG arg0, ULONG arg1, ULONG arg2);
static UINT AppDisplayCmdPost(app_display_cmd_type_t cmd_type, ULONG arg0);
static UINT AppStorageReqPost(app_storage_req_type_t req_type, ULONG arg0);
static UINT AppInputCmdPost(app_input_cmd_type_t cmd_type, ULONG arg0);
static UINT AppAudioCmdPost(app_audio_cmd_type_t cmd_type, ULONG arg0);
static UINT AppSensorReqPost(app_sensor_req_type_t req_type, ULONG arg0, ULONG arg1, ULONG arg2);
static UINT AppStorageUsbMscRequest(app_storage_req_type_t req_type, ULONG arg0, ULONG arg1, ULONG arg2, ULONG *media_status_out);
static VOID AppStorageUsbMscSignal(UINT status, ULONG media_status);
static VOID AppPowerStopClockPolicyApply(void);
static VOID AppSensorSuspendHardwareForStop(void);
static UINT AppUsbClock48Set(uint32_t hsi48_state);
static VOID AppUsbVddUsbSet(UINT enabled);
static UINT AppUsbDeviceHardwareInit(void);
static UINT AppUsbDeviceHardwareOff(void);
static UINT AppUsbDeviceStart(void);
static UINT AppUsbDeviceStop(void);
static UINT AppUsbDeviceStopWithGrace(ULONG disconnect_grace_ticks);

/* Input helpers referenced before input include is expanded. */
static VOID AppInputRefreshPhysicalIdleLevels(void);
static ULONG AppInputSourceBit(ULONG source);
static app_input_action_t AppInputActionForSource(ULONG source);
static VOID AppInputPostRawEvent(ULONG source, ULONG edge, ULONG level, ULONG tick);

#include "threads/app_thread_entry_power.c"
#include "threads/app_thread_entry_display.c"
#include "threads/app_thread_entry_storage.c"
#include "threads/app_thread_entry_audio.c"
#include "threads/app_thread_entry_sensor.c"

/* USER CODE END PFP */

/**
  * @brief  Application ThreadX Initialization.
  * @param memory_ptr: memory pointer
  * @retval int
  */
UINT App_ThreadX_Init(VOID *memory_ptr)
{
  UINT ret = TX_SUCCESS;

  /* USER CODE BEGIN App_ThreadX_MEM_POOL */
  (void)memory_ptr;

  /* USER CODE END App_ThreadX_MEM_POOL */

  /* USER CODE BEGIN App_ThreadX_Init */
  ret = tx_event_flags_create(&g_eg_mode, (CHAR *)"egMode");
  if (ret != TX_SUCCESS)
  {
    return ret;
  }

  ret = AppSetModeFlag(APP_MODE_STATIC);
  if (ret != TX_SUCCESS)
  {
    return ret;
  }

  ret = tx_event_flags_create(&g_eg_power, (CHAR *)"egPower");
  if (ret != TX_SUCCESS)
  {
    return ret;
  }

  ret = tx_event_flags_create(&g_eg_sensor_health, (CHAR *)"egSensorHealth");
  if (ret != TX_SUCCESS)
  {
    return ret;
  }

  ret = tx_event_flags_create(&g_eg_audio_dma, (CHAR *)"egAudioDma");
  if (ret != TX_SUCCESS)
  {
    return ret;
  }

  ret = AppPowerFlagsUpdate(APP_POWER_FLAG_RUNNING, (APP_POWER_FLAGS_ALL & ~APP_POWER_FLAG_RUNNING));
  if (ret != TX_SUCCESS)
  {
    return ret;
  }
  g_power_pending_ack_mask = 0UL;
  g_power_quiesce_wait_active = 0UL;
  g_power_quiesce_wait_elapsed_ticks = 0UL;
  g_power_stop2_armed = 0UL;
  g_power_stop2_entry_count = 0UL;
  g_power_stop2_wake_count = 0UL;
  g_power_stop2_abort_count = 0UL;
  g_power_stop2_last_wusr = 0UL;
  g_power_stop2_last_sr = 0UL;
  g_power_stop2_last_error = 0L;
  g_power_stop2_tb_sample_count = 0UL;
  g_power_stop2_tb_last_hal_dt = 0UL;
  g_power_stop2_tb_last_tx_dt = 0UL;
  g_power_stop2_tb_last_abs_diff = 0UL;
  g_power_stop2_tb_max_abs_diff = 0UL;
  g_power_stop2_tb_last_wake_count = 0UL;
  g_power_stop2_tb_persist_magic = 0UL;
  g_power_stop2_tb_persist_load_ok = 0UL;
  g_power_stop2_tb_prev_hal_tick = 0UL;
  g_power_stop2_tb_prev_tx_tick = 0UL;
  g_power_stop2_tb_prev_valid = 0UL;
  AppPowerStop2TimebaseTelemetryInit();
  g_retained_state_magic = 0UL;
  g_retained_state_valid_mask = 0UL;
  g_retained_state_seq = 0UL;
  g_retained_state_crc_ok = 0UL;
  g_retained_state_load_ok_count = 0UL;
  g_retained_state_load_fail_count = 0UL;
  g_retained_state_save_ok_count = 0UL;
  g_retained_state_save_fail_count = 0UL;
  g_retained_state_game_restore_ok_count = 0UL;
  g_retained_state_game_restore_fail_count = 0UL;
  g_retained_state_game_save_ok_count = 0UL;
  g_retained_state_game_save_fail_count = 0UL;
  g_retained_state_game_mode_id = 0UL;
  g_retained_state_game_backend_id = 0UL;
  g_retained_state_game_topdown_valid = 0UL;
  g_sensor_bus_fault = 0UL;
  g_sensor_mode_token = (ULONG)APP_MODE_STATIC;
  (void)memset(&g_sensor_pmic_live, 0, sizeof(g_sensor_pmic_live));
  g_sensor_pmic_live.guard_enabled = (KNOB_SENSOR_PMIC_GUARD_ENABLE != 0) ? 1UL : 0UL;
  g_sensor_pmic_live.cutoff_mv = (ULONG)KNOB_SENSOR_PMIC_CUTOFF_MV;
  g_sensor_pmic_live.cutoff_hys_mv = (ULONG)KNOB_SENSOR_PMIC_CUTOFF_HYS_MV;
  g_sensor_pmic_live.cutoff_confirm_samples = (ULONG)KNOB_SENSOR_PMIC_CUTOFF_CONFIRM_SAMPLES;
  g_storage_flash_ready = 0UL;
  g_storage_flash_quiesced = 0UL;
  g_storage_flash_in_dpd = 0UL;
  g_storage_ospi_clock_enabled = 1UL;
  g_storage_last_op = APP_STORAGE_OP_NONE;
  g_storage_last_error = APP_STORAGE_ERR_NONE;
  g_storage_last_jedec_id = 0UL;
  g_storage_smoke_pass_count = 0UL;
  g_storage_smoke_fail_count = 0UL;
  g_storage_filex_mounted = 0UL;
  g_storage_filex_mount_count = 0UL;
  g_storage_filex_mount_fail_count = 0UL;
  g_storage_filex_format_count = 0UL;
  g_storage_filex_unmount_count = 0UL;
  g_storage_filex_unmount_fail_count = 0UL;
  g_storage_filex_last_status = FX_SUCCESS;
  (void)memset(&g_storage_usb_msc_lx_flash, 0, sizeof(g_storage_usb_msc_lx_flash));
  g_storage_usb_msc_lx_opened = 0UL;
  g_storage_joycfg_valid = 0UL;
  g_storage_joycfg_last_error = APP_STORAGE_ERR_NONE;
  g_storage_joycfg_load_ok_count = 0UL;
  g_storage_joycfg_load_fail_count = 0UL;
  g_storage_joycfg_save_ok_count = 0UL;
  g_storage_joycfg_save_fail_count = 0UL;
  g_storage_joycfg_load_seq = 0UL;
  g_storage_joycfg_save_seq = 0UL;
  g_storage_audio_catalog_loaded = 0UL;
  g_storage_audio_catalog_load_ok_count = 0UL;
  g_storage_audio_catalog_load_fail_count = 0UL;
  g_storage_audio_catalog_install_ok_count = 0UL;
  g_storage_audio_catalog_install_fail_count = 0UL;
  g_storage_audio_catalog_install_last_bytes = 0UL;
  g_storage_audio_catalog_addr = 0UL;
  g_storage_audio_catalog_entry_count = 0UL;
  g_storage_audio_catalog_version = 0UL;
  g_storage_audio_catalog_table_crc32 = 0UL;
  g_storage_audio_chunk_read_ok_count = 0UL;
  g_storage_audio_chunk_read_fail_count = 0UL;
  g_storage_audio_chunk_last_addr = 0UL;
  g_storage_audio_chunk_last_len = 0UL;
  g_storage_audio_chunk_last_token = 0UL;
  g_storage_audio_chunk_last_crc32 = 0UL;
  g_storage_audio_chunk_cache_seq = 0UL;
  g_storage_game_package_manifest_loaded = 0UL;
  g_storage_game_package_manifest_load_ok_count = 0UL;
  g_storage_game_package_manifest_load_fail_count = 0UL;
  g_storage_game_package_manifest_import_ok_count = 0UL;
  g_storage_game_package_manifest_import_fail_count = 0UL;
  g_storage_game_package_manifest_import_last_status = (ULONG)TX_NOT_DONE;
  g_storage_game_package_manifest_import_last_bytes = 0UL;
  g_storage_game_package_scene_import_ok_count = 0UL;
  g_storage_game_package_scene_import_fail_count = 0UL;
  g_storage_game_package_scene_import_last_status = (ULONG)TX_NOT_DONE;
  g_storage_game_package_scene_import_map_bytes = 0UL;
  g_storage_game_package_scene_import_tileset_bytes = 0UL;
  g_storage_game_package_manifest_addr = 0UL;
  g_storage_game_package_manifest_size = 0UL;
  g_storage_game_package_manifest_last_status = (ULONG)TX_NOT_DONE;
  g_storage_game_package_id = 0UL;
  g_storage_game_package_version = 0UL;
  g_storage_game_package_mode_count = 0UL;
  g_storage_game_package_pet_route_count = 0UL;
  g_storage_game_package_source = APP_STORAGE_PKG_SRC_NONE;
  g_storage_install_index_valid = 0UL;
  g_storage_install_index_load_ok_count = 0UL;
  g_storage_install_index_load_fail_count = 0UL;
  g_storage_install_index_write_ok_count = 0UL;
  g_storage_install_index_write_fail_count = 0UL;
  g_storage_install_index_active_slot = APP_STORAGE_INSTALL_INDEX_SLOT_INVALID;
  g_storage_install_index_sequence = 0UL;
  g_storage_install_index_package_id = 0UL;
  g_storage_install_index_package_version = 0UL;
  g_storage_install_index_blob_offset = 0UL;
  g_storage_install_index_blob_size = 0UL;
  g_storage_install_index_blob_crc32 = 0UL;
  g_storage_install_index_manifest_addr = 0UL;
  g_storage_install_index_manifest_size = 0UL;
  g_storage_install_index_manifest_crc32 = 0UL;
  g_storage_install_index_record_crc32 = 0UL;
  g_storage_install_index_load_last_status = (ULONG)TX_NOT_DONE;
  g_storage_install_index_write_last_status = (ULONG)TX_NOT_DONE;
  g_storage_install_index_scan_done = 0UL;
  g_storage_game_package_manifest_erase_ok_count = 0UL;
  g_storage_game_package_manifest_erase_fail_count = 0UL;
  g_storage_raw_app_erase_ok_count = 0UL;
  g_storage_raw_app_erase_fail_count = 0UL;
  g_storage_scene_map_loaded = 0UL;
  g_storage_scene_map_load_ok_count = 0UL;
  g_storage_scene_map_load_fail_count = 0UL;
  g_storage_scene_map_addr = 0UL;
  g_storage_scene_map_size = 0UL;
  g_storage_scene_map_last_status = (ULONG)TX_NOT_DONE;
  g_storage_scene_map_width = 0UL;
  g_storage_scene_map_height = 0UL;
  g_storage_scene_map_tile_count = 0UL;
  g_storage_scene_map_object_count = 0UL;
  g_storage_scene_tileset_loaded = 0UL;
  g_storage_scene_tileset_load_ok_count = 0UL;
  g_storage_scene_tileset_load_fail_count = 0UL;
  g_storage_scene_tileset_addr = 0UL;
  g_storage_scene_tileset_size = 0UL;
  g_storage_scene_tileset_last_status = (ULONG)TX_NOT_DONE;
  g_storage_scene_tileset_tile_width = 0UL;
  g_storage_scene_tileset_tile_height = 0UL;
  g_storage_scene_tileset_tile_count = 0UL;
  g_storage_scene_tileset_base_gid = 0UL;
  g_storage_last_erase_addr = 0UL;
  g_storage_last_erase_size = 0UL;
  (void)memset(&g_storage_audio_catalog_header, 0, sizeof(g_storage_audio_catalog_header));
  (void)memset(g_storage_audio_catalog_entries, 0, sizeof(g_storage_audio_catalog_entries));
  (void)memset(g_storage_audio_chunk_buf, 0, sizeof(g_storage_audio_chunk_buf));
  (void)memset(g_storage_game_package_manifest_buf, 0, sizeof(g_storage_game_package_manifest_buf));
  (void)memset(g_storage_scene_map_blob_buf, 0, sizeof(g_storage_scene_map_blob_buf));
  (void)memset(g_storage_scene_tileset_blob_buf, 0, sizeof(g_storage_scene_tileset_blob_buf));
  AppStorageAudioChunkCacheReset();
  g_storage_joycfg_cal.cx = 0.0f;
  g_storage_joycfg_cal.cy = 0.0f;
  g_storage_joycfg_cal.sx = 1.0f;
  g_storage_joycfg_cal.sy = 1.0f;
  g_storage_joycfg_cal.rot_deg = 0.0f;
  g_storage_joycfg_cal.invert_x = 0U;
  g_storage_joycfg_cal.invert_y = 0U;
  g_storage_joycfg_deadzone_enabled = 1UL;
  g_storage_joycfg_deadzone_mT = ((float)KNOB_SENSOR_JOY_NEUTRAL_DEADZONE_MIN_MT_X10) / 10.0f;
  if (g_storage_joycfg_deadzone_mT < 0.1f)
  {
    g_storage_joycfg_deadzone_mT = 0.1f;
  }
  g_storage_at25_dbg.last_op = 0U;
  g_storage_at25_dbg.cmd_status = 0U;
  g_storage_at25_dbg.io_status = 0U;
  g_storage_at25_dbg.reserved0 = 0U;
  g_storage_at25_dbg.addr = 0UL;
  g_storage_at25_dbg.nbytes = 0UL;
  g_storage_at25_dbg.hal_error = 0UL;
  g_storage_at25_dbg.seq = 0UL;
  g_audio_dma_events = 0UL;
  g_audio_dma_half_pending = 0UL;
  g_audio_dma_full_pending = 0UL;
  g_audio_dma_error_pending = 0UL;
  g_audio_state = APP_AUDIO_STATE_STOPPED;
  g_audio_start_count = 0UL;
  g_audio_stop_count = 0UL;
  g_audio_restart_count = 0UL;
  g_audio_underflow_count = 0UL;
  g_audio_half_irq_count = 0UL;
  g_audio_full_irq_count = 0UL;
  g_audio_error_irq_count = 0UL;
  g_audio_half_missed_count = 0UL;
  g_audio_full_missed_count = 0UL;
  g_audio_error_missed_count = 0UL;
  g_audio_last_error = 0L;
  g_audio_phase_accum = 0UL;
  g_audio_phase_step = 0UL;
  g_audio_source_kind = APP_AUDIO_SOURCE_NONE;
  AppAudioVoiceReset(&g_audio_music_voice);
  (void)memset(g_audio_sfx_voices, 0, sizeof(g_audio_sfx_voices));
  g_audio_voice_seq_counter = 0UL;
  g_audio_sfx_voice_steal_count = 0UL;
  g_audio_sfx_voice_peak_active = 0UL;
  g_audio_play_event_count = 0UL;
  g_audio_play_clip_count = 0UL;
  g_audio_cmd_post_count = 0UL;
  g_audio_cmd_drop_count = 0UL;
  g_audio_last_event = (ULONG)APP_AUDIO_EVENT_NONE;
  g_audio_last_clip = (ULONG)APP_AUDIO_ASSET_NONE;
  g_audio_next_sfx_gain_class = (ULONG)APP_AUDIO_GAIN_CLASS_SFX;
  g_audio_sfx_autostop_armed = 0UL;
  g_audio_sfx_autostop_tick = 0UL;
  g_audio_power_boost_asserted = 0UL;
  g_audio_storage_req_token_seq = 0UL;
  g_audio_user_gain_master_pct = APP_STORAGE_USER_GAIN_DEFAULT_PCT;
  g_audio_user_gain_music_pct = APP_STORAGE_USER_GAIN_DEFAULT_PCT;
  g_audio_user_gain_sfx_pct = APP_STORAGE_USER_GAIN_DEFAULT_PCT;
  g_audio_user_gain_ui_pct = APP_STORAGE_USER_GAIN_DEFAULT_PCT;
  g_input_raw_post_count = 0UL;
  g_input_raw_drop_count = 0UL;
  g_input_raw_recv_count = 0UL;
  g_input_raw_suppressed_count = 0UL;
  g_input_last_source = 0UL;
  g_input_last_edge = 0UL;
  g_input_last_tick = 0UL;
  g_input_last_level = 0UL;
  g_input_quiesced = 0UL;
  g_input_action_total_count = 0UL;
  g_input_action_ui_route_count = 0UL;
  g_input_action_game_route_count = 0UL;
  g_input_action_system_route_count = 0UL;
  g_input_action_ignored_count = 0UL;
  g_input_action_last = (ULONG)APP_INPUT_ACTION_NONE;
  g_input_action_last_mode = 0UL;
  g_input_action_ui_post_count = 0UL;
  g_input_action_ui_drop_count = 0UL;
  g_input_action_ui_drop_oldest_count = 0UL;
  g_input_action_game_post_count = 0UL;
  g_input_action_game_drop_count = 0UL;
  g_input_action_game_drop_oldest_count = 0UL;
  g_ui_event_recv_count = 0UL;
  g_ui_event_last_action = (ULONG)APP_INPUT_ACTION_NONE;
  g_ui_event_handled_count = 0UL;
  g_ui_event_ignored_count = 0UL;
  g_ui_event_queue_error_count = 0UL;
  g_game_event_recv_count = 0UL;
  g_game_event_last_action = (ULONG)APP_INPUT_ACTION_NONE;
  g_game_event_handled_count = 0UL;
  g_game_event_ignored_count = 0UL;
  g_game_event_stale_drop_count = 0UL;
  g_game_event_queue_error_count = 0UL;
  (void)memset(g_input_button_state, 0, sizeof(g_input_button_state));
  g_input_debounce_drop_count = 0UL;
  g_input_release_pass_count = 0UL;
  g_input_release_reconcile_count = 0UL;
  g_input_repeat_emit_count = 0UL;
  g_input_long_emit_count = 0UL;
  g_input_sys_activity_post_count = 0UL;
  g_input_sys_activity_drop_count = 0UL;
  g_input_sys_activity_last_tick = 0UL;
  g_input_sys_menu_post_count = 0UL;
  g_input_sys_menu_drop_count = 0UL;
  g_input_stop_wake_pending_mask = 0UL;
  (void)memset(g_input_stop_wake_pending_tick, 0, sizeof(g_input_stop_wake_pending_tick));
  (void)memset(g_input_physical_idle_level, 0, sizeof(g_input_physical_idle_level));
  g_input_physical_idle_valid_mask = 0UL;
  g_power_input_activity_count = 0UL;
  g_power_last_input_tick = 0UL;
  g_power_menu_event_count = 0UL;
  g_power_stop_select_active = 0UL;
  g_power_stop_select_last_input_tick = 0UL;
  g_pet_state = APP_PET_STATE_IDLE;
  g_pet_tick_count = 0UL;
  g_pet_wake_count = 0UL;
  g_pet_last_action = (ULONG)APP_PET_ACTION_NONE;
  g_pet_hunger_pct = 30UL;
  g_pet_energy_pct = 80UL;
  g_pet_mood_pct = 60UL;
  (void)AppRetainedStateRestorePet();
  g_game_exit_to_static_pending = 0UL;
  g_power_perf_profile_current = APP_POWER_PERF_PROFILE_NORM;
  g_power_perf_profile_target = APP_POWER_PERF_PROFILE_NORM;
  g_power_perf_last_switch_tick = 0UL;
  g_power_perf_hint_seq = 0UL;
  g_power_perf_hint_post_count = 0UL;
  g_power_perf_hint_drop_count = 0UL;
  g_power_perf_hint_rx_count = 0UL;
  g_power_perf_hint_inflight = 0UL;
  g_power_perf_force_up_no_dwell = 0UL;
  g_power_perf_audio_boost_active = 0UL;
  g_power_perf_last_present_ticks = 0UL;
  g_power_perf_last_draw_ticks = 0UL;
  g_power_perf_last_dirty_rows = 0UL;
  g_power_perf_last_full_flush = 0UL;
  g_power_perf_miss_streak = 0UL;
  g_power_perf_headroom_streak = 0UL;
  g_power_perf_up_count = 0UL;
  g_power_perf_down_count = 0UL;
  g_power_perf_dwell_block_count = 0UL;
  g_power_perf_clock_apply_fail_count = 0UL;
  g_power_perf_clock_apply_last_stage = APP_POWER_CLK_STAGE_NONE;
  g_power_perf_clock_apply_last_hal = 0L;
  g_power_perf_base_sysclk_mhz = (HAL_RCC_GetSysClockFreq() + 500000UL) / 1000000UL;
  if (g_power_perf_base_sysclk_mhz == 0UL)
  {
    g_power_perf_base_sysclk_mhz = g_power_perf_profiles[APP_POWER_PERF_PROFILE_NORM].sysclk_mhz;
  }
  __HAL_RCC_MSIKSTOP_DISABLE();
  AppPowerStopClockPolicyApply();
  (void)AppUsbDeviceHardwareOff();
  g_ui_mode_flags_last = 0UL;
  g_ui_bootstrap_present_pending = 1UL;
  g_ui_boot_ready = 0UL;
  (void)memset(&g_ui_router, 0, sizeof(g_ui_router));
  g_sensor_joy_cal_active = 0UL;
  g_sensor_joy_cal_stage = (ULONG)APP_JOY_CAL_STAGE_IDLE;
  (void)memset(&g_sensor_joy_cal_capture, 0, sizeof(g_sensor_joy_cal_capture));
  (void)memset(g_sensor_joy_cal_dir_avg_x, 0, sizeof(g_sensor_joy_cal_dir_avg_x));
  (void)memset(g_sensor_joy_cal_dir_avg_y, 0, sizeof(g_sensor_joy_cal_dir_avg_y));
  g_sensor_joy_cal_wait_confirm = 0UL;
  (void)memset(&g_sensor_joy_cal_snapshot, 0, sizeof(g_sensor_joy_cal_snapshot));
  g_sensor_joy_cal_snapshot_valid = 0UL;
  g_sensor_joy_input_gate_snapshot_valid = 0UL;
  g_sensor_joycfg_seen_load_seq = 0UL;
  g_sensor_joycfg_seen_save_seq = 0UL;
  g_sensor_joy = TX_NULL;
  g_sensor_joy_input_mask = 0UL;
  g_sensor_joy_input_gate_valid = 0UL;
  g_sensor_joy_input_neutral_armed = 0UL;
  g_sensor_joy_input_neutral_stable_count = 0UL;
  g_sensor_joy_release_stable_count = 0UL;
  g_sensor_joy_live_read_fail_streak = 0UL;
  (void)memset(&g_sensor_joy_live_status, 0, sizeof(g_sensor_joy_live_status));
  AppSensorJoyCalQualityReset();
  g_sensor_joy_live_status.span_x_mT = 1.0f;
  g_sensor_joy_live_status.span_y_mT = 1.0f;
  AppSensorJoyCalResetStatus();
  AppInputRefreshPhysicalIdleLevels();
  AppSensorMarkAllOff();
  ret = AppSensorHealthFlagsPublish();
  if (ret != TX_SUCCESS)
  {
    return ret;
  }

  ret = tx_queue_create(&g_q_sys_events,
                        (CHAR *)"qSysEvents",
                        APP_QSYS_EVENT_WORDS,
                        g_q_sys_events_storage,
                        sizeof(g_q_sys_events_storage));
  if (ret != TX_SUCCESS)
  {
    return ret;
  }

  ret = tx_queue_create(&g_q_display_cmd,
                        (CHAR *)"qDisplayCmd",
                        APP_DISPLAY_CMD_WORDS,
                        g_q_display_cmd_storage,
                        sizeof(g_q_display_cmd_storage));
  if (ret != TX_SUCCESS)
  {
    return ret;
  }

  ret = tx_queue_create(&g_q_storage_req,
                        (CHAR *)"qStorageReq",
                        APP_STORAGE_REQ_WORDS,
                        g_q_storage_req_storage,
                        sizeof(g_q_storage_req_storage));
  if (ret != TX_SUCCESS)
  {
    return ret;
  }

  ret = tx_queue_create(&g_q_input_cmd,
                        (CHAR *)"qInputCmd",
                        APP_INPUT_CMD_WORDS,
                        g_q_input_cmd_storage,
                        sizeof(g_q_input_cmd_storage));
  if (ret != TX_SUCCESS)
  {
    return ret;
  }

  ret = tx_queue_create(&g_q_input_raw,
                        (CHAR *)"qInputRaw",
                        APP_INPUT_RAW_WORDS,
                        g_q_input_raw_storage,
                        sizeof(g_q_input_raw_storage));
  if (ret != TX_SUCCESS)
  {
    return ret;
  }

  ret = tx_queue_create(&g_q_ui_events,
                        (CHAR *)"qUIEvents",
                        APP_INPUT_ACTION_WORDS,
                        g_q_ui_events_storage,
                        sizeof(g_q_ui_events_storage));
  if (ret != TX_SUCCESS)
  {
    return ret;
  }

  ret = tx_queue_create(&g_q_game_events,
                        (CHAR *)"qGameEvents",
                        APP_INPUT_ACTION_WORDS,
                        g_q_game_events_storage,
                        sizeof(g_q_game_events_storage));
  if (ret != TX_SUCCESS)
  {
    return ret;
  }

  ret = tx_queue_create(&g_q_audio_cmd,
                        (CHAR *)"qAudioCmd",
                        APP_AUDIO_CMD_WORDS,
                        g_q_audio_cmd_storage,
                        sizeof(g_q_audio_cmd_storage));
  if (ret != TX_SUCCESS)
  {
    return ret;
  }

  ret = tx_queue_create(&g_q_sensor_req,
                        (CHAR *)"qSensorReq",
                        APP_SENSOR_REQ_WORDS,
                        g_q_sensor_req_storage,
                        sizeof(g_q_sensor_req_storage));
  if (ret != TX_SUCCESS)
  {
    return ret;
  }

  ret = tx_mutex_create(&g_mtx_renderer, (CHAR *)"mtxRenderer", TX_INHERIT);
  if (ret != TX_SUCCESS)
  {
    return ret;
  }

  ret = tx_mutex_create(&g_mtx_retained, (CHAR *)"mtxRetained", TX_INHERIT);
  if (ret != TX_SUCCESS)
  {
    return ret;
  }

  ret = tx_mutex_create(&g_mtx_storage_usb_msc, (CHAR *)"mtxStorageUsbMsc", TX_INHERIT);
  if (ret != TX_SUCCESS)
  {
    return ret;
  }

  ret = tx_semaphore_create(&g_sem_storage_usb_msc_done, (CHAR *)"semStorageUsbMscDone", 0U);
  if (ret != TX_SUCCESS)
  {
    return ret;
  }

  /* Keep debug-facing API entry points linked for debug.gdb scripted calls. */
  {
    UINT (*volatile keep_mode_set)(app_mode_t) = App_SysEvent_ModeSet;
    UINT (*volatile keep_quiesce)(void) = App_SysEvent_QuiesceReq;
    UINT (*volatile keep_resume)(void) = App_SysEvent_ResumeReq;
    UINT (*volatile keep_quiesce_ack)(ULONG) = App_SysEvent_QuiesceAck;
    UINT (*volatile keep_pet_action)(app_pet_action_t) = App_PetReq_Action;
    UINT (*volatile keep_mode_flags_get)(ULONG *) = App_ModeFlags_Get;
    UINT (*volatile keep_power_flags_get)(ULONG *) = App_PowerFlags_Get;
    UINT (*volatile keep_stop2_tb_clear)(void) = App_Power_Stop2TimebaseTelemetryClear;
    UINT (*volatile keep_retained_state_clear)(void) = App_RetainedStateClear;
    UINT (*volatile keep_sensor_health_flags_get)(ULONG *) = App_SensorHealthFlags_Get;
    UINT (*volatile keep_sensor_poll)(app_sensor_target_t) = App_SensorReq_Poll;
    UINT (*volatile keep_sensor_config_defaults)(app_sensor_target_t) = App_SensorReq_ConfigDefaults;
    UINT (*volatile keep_sensor_health_snapshot)(void) = App_SensorReq_HealthSnapshot;
    UINT (*volatile keep_sensor_lis_set_profile)(app_sensor_lis_profile_t) = App_SensorReq_LisSetProfile;
    UINT (*volatile keep_sensor_lis_set_low_power)(void) = App_SensorReq_LisSetLowPower;
    UINT (*volatile keep_sensor_lis_set_live)(void) = App_SensorReq_LisSetLive;
    UINT (*volatile keep_sensor_lis_stream_start)(void) = App_SensorReq_LisStreamStart;
    UINT (*volatile keep_sensor_lis_stream_stop)(void) = App_SensorReq_LisStreamStop;
    UINT (*volatile keep_sensor_lis_step_enable)(void) = App_SensorReq_LisStepEnable;
    UINT (*volatile keep_sensor_lis_step_disable)(void) = App_SensorReq_LisStepDisable;
    UINT (*volatile keep_sensor_lis_step_reset)(void) = App_SensorReq_LisStepReset;
    UINT (*volatile keep_display_invalidate_all)(void) = App_Display_InvalidateAll;
    UINT (*volatile keep_display_present)(void) = App_Display_Present;
    UINT (*volatile keep_storage_flash_probe)(void) = App_StorageReq_FlashProbe;
    UINT (*volatile keep_storage_raw_smoke)(void) = App_StorageReq_RawSmoke;
    UINT (*volatile keep_storage_filex_mount)(void) = App_StorageReq_FileXMount;
    UINT (*volatile keep_storage_filex_format)(void) = App_StorageReq_FileXFormat;
    UINT (*volatile keep_storage_filex_unmount)(void) = App_StorageReq_FileXUnmount;
    const ULONG *volatile keep_storage_audio_catalog_addr_dbg = &g_storage_audio_catalog_addr_dbg;
    const ULONG *volatile keep_storage_audio_catalog_max_bytes_dbg = &g_storage_audio_catalog_max_bytes_dbg;
    const ULONG *volatile keep_storage_settings_addr_dbg = &g_storage_settings_addr_dbg;
    const ULONG *volatile keep_storage_smoke_addr_dbg = &g_storage_smoke_addr_dbg;
    const ULONG *volatile keep_storage_smoke_len_dbg = &g_storage_smoke_len_dbg;
    const ULONG *volatile keep_storage_game_pkg_manifest_addr_dbg = &g_storage_game_pkg_manifest_addr_dbg;
    const ULONG *volatile keep_storage_game_pkg_manifest_max_bytes_dbg = &g_storage_game_pkg_manifest_max_bytes_dbg;
    const ULONG *volatile keep_storage_installed_base_addr_dbg = &g_storage_installed_base_addr_dbg;
    const ULONG *volatile keep_storage_installed_size_bytes_dbg = &g_storage_installed_size_bytes_dbg;
    const ULONG *volatile keep_storage_installed_data_end_addr_dbg = &g_storage_installed_data_end_addr_dbg;
    const ULONG *volatile keep_storage_install_index_slot0_addr_dbg = &g_storage_install_index_slot0_addr_dbg;
    const ULONG *volatile keep_storage_install_index_slot1_addr_dbg = &g_storage_install_index_slot1_addr_dbg;
    const ULONG *volatile keep_storage_install_index_slot_size_bytes_dbg = &g_storage_install_index_slot_size_bytes_dbg;
    const ULONG *volatile keep_storage_install_index_record_size_dbg = &g_storage_install_index_record_size_dbg;
    const ULONG *volatile keep_storage_install_index_record_crc_offset_dbg = &g_storage_install_index_record_crc_offset_dbg;
    const ULONG *volatile keep_storage_scene_tileset_addr_dbg = &g_storage_scene_tileset_addr_dbg;
    const ULONG *volatile keep_storage_scene_tileset_size_bytes_dbg = &g_storage_scene_tileset_size_bytes_dbg;
    const ULONG *volatile keep_storage_fat_base_addr_dbg = &g_storage_fat_base_addr_dbg;
    const ULONG *volatile keep_storage_fat_size_bytes_dbg = &g_storage_fat_size_bytes_dbg;
    const ULONG *volatile keep_storage_filex_cache_bytes_dbg = &g_storage_filex_cache_bytes_dbg;
    const ULONG *volatile keep_storage_filex_spc_dbg = &g_storage_filex_spc_dbg;
    const ULONG *volatile keep_storage_filex_dir_entries_dbg = &g_storage_filex_dir_entries_dbg;
    UINT (*volatile keep_storage_joycfg_load)(void) = App_StorageReq_JoyCfgLoad;
    UINT (*volatile keep_storage_joycfg_save)(void) = App_StorageReq_JoyCfgSave;
    UINT (*volatile keep_storage_audio_catalog_load)(ULONG) = App_StorageReq_AudioCatalogLoad;
    UINT (*volatile keep_storage_audio_chunk_read)(ULONG, ULONG, ULONG) = App_StorageReq_AudioChunkRead;
    UINT (*volatile keep_storage_audio_catalog_install_embedded)(void) = App_StorageReq_AudioCatalogInstallEmbedded;
    UINT (*volatile keep_storage_audio_catalog_install_manifest_refs)(void) = App_StorageReq_AudioCatalogInstallManifestRefs;
    UINT (*volatile keep_storage_game_pkg_manifest_load)(ULONG, ULONG) = App_StorageReq_GamePackageManifestLoad;
    UINT (*volatile keep_storage_game_pkg_manifest_load_default)(void) = App_StorageReq_GamePackageManifestLoadDefault;
    UINT (*volatile keep_storage_game_pkg_manifest_erase)(void) = App_StorageReq_GamePackageManifestErase;
    UINT (*volatile keep_storage_game_pkg_manifest_import_fat)(void) = App_StorageReq_GamePackageManifestImportFat;
    UINT (*volatile keep_storage_game_pkg_scene_import_fat)(void) = App_StorageReq_GamePackageSceneImportFat;
    UINT (*volatile keep_storage_raw_app_erase)(void) = App_StorageReq_RawAppErase;
    UINT (*volatile keep_storage_game_pkg_manifest_write_test)(void) = App_StorageReq_GamePackageManifestWriteTest;
    UINT (*volatile keep_storage_scene_map_load)(ULONG, ULONG) = App_StorageReq_SceneMapLoad;
    UINT (*volatile keep_storage_scene_tileset_load)(ULONG, ULONG) = App_StorageReq_SceneTilesetLoad;
    UINT (*volatile keep_audio_start_tone)(void) = App_AudioReq_StartTone;
    UINT (*volatile keep_audio_stop)(void) = App_AudioReq_Stop;
    const char *(*volatile keep_audio_assets_name)(uint32_t) = AppAudioAssets_Name;
    UINT (*volatile keep_sensor_joycal_start)(void) = App_SensorReq_JoyCalStart;
    UINT (*volatile keep_sensor_joycal_save)(void) = App_SensorReq_JoyCalSave;
    UINT (*volatile keep_sensor_joycal_cancel)(void) = App_SensorReq_JoyCalCancel;
    (void)keep_mode_set;
    (void)keep_quiesce;
    (void)keep_resume;
    (void)keep_quiesce_ack;
    (void)keep_pet_action;
    (void)keep_mode_flags_get;
    (void)keep_power_flags_get;
    (void)keep_stop2_tb_clear;
    (void)keep_retained_state_clear;
    (void)keep_sensor_health_flags_get;
    (void)keep_sensor_poll;
    (void)keep_sensor_config_defaults;
    (void)keep_sensor_health_snapshot;
    (void)keep_sensor_lis_set_profile;
    (void)keep_sensor_lis_set_low_power;
    (void)keep_sensor_lis_set_live;
    (void)keep_sensor_lis_stream_start;
    (void)keep_sensor_lis_stream_stop;
    (void)keep_sensor_lis_step_enable;
    (void)keep_sensor_lis_step_disable;
    (void)keep_sensor_lis_step_reset;
    (void)keep_display_invalidate_all;
    (void)keep_display_present;
    (void)keep_storage_flash_probe;
    (void)keep_storage_raw_smoke;
    (void)keep_storage_filex_mount;
    (void)keep_storage_filex_format;
    (void)keep_storage_filex_unmount;
    (void)keep_storage_audio_catalog_addr_dbg;
    (void)keep_storage_audio_catalog_max_bytes_dbg;
    (void)keep_storage_settings_addr_dbg;
    (void)keep_storage_smoke_addr_dbg;
    (void)keep_storage_smoke_len_dbg;
    (void)keep_storage_game_pkg_manifest_addr_dbg;
    (void)keep_storage_game_pkg_manifest_max_bytes_dbg;
    (void)keep_storage_installed_base_addr_dbg;
    (void)keep_storage_installed_size_bytes_dbg;
    (void)keep_storage_installed_data_end_addr_dbg;
    (void)keep_storage_install_index_slot0_addr_dbg;
    (void)keep_storage_install_index_slot1_addr_dbg;
    (void)keep_storage_install_index_slot_size_bytes_dbg;
    (void)keep_storage_install_index_record_size_dbg;
    (void)keep_storage_install_index_record_crc_offset_dbg;
    (void)keep_storage_scene_tileset_addr_dbg;
    (void)keep_storage_scene_tileset_size_bytes_dbg;
    (void)keep_storage_fat_base_addr_dbg;
    (void)keep_storage_fat_size_bytes_dbg;
    (void)keep_storage_filex_cache_bytes_dbg;
    (void)keep_storage_filex_spc_dbg;
    (void)keep_storage_filex_dir_entries_dbg;
    (void)keep_storage_joycfg_load;
    (void)keep_storage_joycfg_save;
    (void)keep_storage_audio_catalog_load;
    (void)keep_storage_audio_chunk_read;
    (void)keep_storage_audio_catalog_install_embedded;
    (void)keep_storage_audio_catalog_install_manifest_refs;
    (void)keep_storage_game_pkg_manifest_load;
    (void)keep_storage_game_pkg_manifest_load_default;
    (void)keep_storage_game_pkg_manifest_erase;
    (void)keep_storage_game_pkg_manifest_import_fat;
    (void)keep_storage_game_pkg_scene_import_fat;
    (void)keep_storage_raw_app_erase;
    (void)keep_storage_game_pkg_manifest_write_test;
    (void)keep_storage_scene_map_load;
    (void)keep_storage_scene_tileset_load;
    (void)keep_audio_start_tone;
    (void)keep_audio_stop;
    (void)keep_audio_assets_name;
    (void)keep_sensor_joycal_start;
    (void)keep_sensor_joycal_save;
    (void)keep_sensor_joycal_cancel;
  }

  ret = tx_thread_create(&g_th_display,
                         (CHAR *)"thDisplay",
                         AppDisplayThreadEntry,
                         0UL,
                         g_th_display_stack,
                         sizeof(g_th_display_stack),
                         KNOB_RTOS_DISPLAY_THREAD_PRIORITY,
                         KNOB_RTOS_DISPLAY_THREAD_PREEMPTION_THRESHOLD,
                         KNOB_RTOS_DISPLAY_THREAD_TIME_SLICE,
                         TX_AUTO_START);
  if (ret != TX_SUCCESS)
  {
    return ret;
  }

  ret = tx_thread_create(&g_th_storage,
                         (CHAR *)"thStorage",
                         AppStorageThreadEntry,
                         0UL,
                         g_th_storage_stack,
                         sizeof(g_th_storage_stack),
                         KNOB_RTOS_STORAGE_THREAD_PRIORITY,
                         KNOB_RTOS_STORAGE_THREAD_PREEMPTION_THRESHOLD,
                         KNOB_RTOS_STORAGE_THREAD_TIME_SLICE,
                         TX_AUTO_START);
  if (ret != TX_SUCCESS)
  {
    return ret;
  }

  ret = tx_thread_create(&g_th_input,
                         (CHAR *)"thInput",
                         AppInputThreadEntry,
                         0UL,
                         g_th_input_stack,
                         sizeof(g_th_input_stack),
                         KNOB_RTOS_INPUT_THREAD_PRIORITY,
                         KNOB_RTOS_INPUT_THREAD_PREEMPTION_THRESHOLD,
                         KNOB_RTOS_INPUT_THREAD_TIME_SLICE,
                         TX_AUTO_START);
  if (ret != TX_SUCCESS)
  {
    return ret;
  }

  ret = tx_thread_create(&g_th_ui,
                         (CHAR *)"thUI",
                         AppUiThreadEntry,
                         0UL,
                         g_th_ui_stack,
                         sizeof(g_th_ui_stack),
                         KNOB_RTOS_INPUT_THREAD_PRIORITY,
                         KNOB_RTOS_INPUT_THREAD_PREEMPTION_THRESHOLD,
                         KNOB_RTOS_INPUT_THREAD_TIME_SLICE,
                         TX_AUTO_START);
  if (ret != TX_SUCCESS)
  {
    return ret;
  }

  ret = tx_thread_create(&g_th_game,
                         (CHAR *)"thGame",
                         AppGameThreadEntry,
                         0UL,
                         g_th_game_stack,
                         sizeof(g_th_game_stack),
                         KNOB_RTOS_GAME_THREAD_PRIORITY,
                         KNOB_RTOS_GAME_THREAD_PREEMPTION_THRESHOLD,
                         KNOB_RTOS_GAME_THREAD_TIME_SLICE,
                         TX_AUTO_START);
  if (ret != TX_SUCCESS)
  {
    return ret;
  }

  ret = tx_thread_create(&g_th_audio,
                         (CHAR *)"thAudio",
                         AppAudioThreadEntry,
                         0UL,
                         g_th_audio_stack,
                         sizeof(g_th_audio_stack),
                         KNOB_RTOS_AUDIO_THREAD_PRIORITY,
                         KNOB_RTOS_AUDIO_THREAD_PREEMPTION_THRESHOLD,
                         KNOB_RTOS_AUDIO_THREAD_TIME_SLICE,
                         TX_AUTO_START);
  if (ret != TX_SUCCESS)
  {
    return ret;
  }

  ret = tx_thread_create(&g_th_power,
                         (CHAR *)"thPower",
                         AppPowerThreadEntry,
                         0UL,
                         g_th_power_stack,
                         sizeof(g_th_power_stack),
                         KNOB_RTOS_POWER_THREAD_PRIORITY,
                         KNOB_RTOS_POWER_THREAD_PREEMPTION_THRESHOLD,
                         KNOB_RTOS_POWER_THREAD_TIME_SLICE,
                         TX_AUTO_START);
  if (ret != TX_SUCCESS)
  {
    return ret;
  }

  ret = tx_thread_create(&g_th_sensor,
                         (CHAR *)"thSensor",
                         AppSensorThreadEntry,
                         0UL,
                         g_th_sensor_stack,
                         sizeof(g_th_sensor_stack),
                         KNOB_RTOS_SENSOR_THREAD_PRIORITY,
                         KNOB_RTOS_SENSOR_THREAD_PREEMPTION_THRESHOLD,
                         KNOB_RTOS_SENSOR_THREAD_TIME_SLICE,
                         TX_AUTO_START);
  /* USER CODE END App_ThreadX_Init */

  return ret;
}

  /**
  * @brief  Function that implements the kernel's initialization.
  * @param  None
  * @retval None
  */
void MX_ThreadX_Init(void)
{
  /* USER CODE BEGIN  Before_Kernel_Start */

  /* USER CODE END  Before_Kernel_Start */

  tx_kernel_enter();

  /* USER CODE BEGIN  Kernel_Start_Error */

  /* USER CODE END  Kernel_Start_Error */
}

/**
  * @brief  App_ThreadX_LowPower_Timer_Setup
  * @param  count : TX timer count
  * @retval None
  */
void App_ThreadX_LowPower_Timer_Setup(ULONG count)
{
  /* USER CODE BEGIN  App_ThreadX_LowPower_Timer_Setup */
  (void)count;

  /* USER CODE END  App_ThreadX_LowPower_Timer_Setup */
}

/**
  * @brief  App_ThreadX_LowPower_Enter
  * @param  None
  * @retval None
  */
void App_ThreadX_LowPower_Enter(void)
{
  /* USER CODE BEGIN  App_ThreadX_LowPower_Enter */

  /* USER CODE END  App_ThreadX_LowPower_Enter */
}

/**
  * @brief  App_ThreadX_LowPower_Exit
  * @param  None
  * @retval None
  */
void App_ThreadX_LowPower_Exit(void)
{
  /* USER CODE BEGIN  App_ThreadX_LowPower_Exit */

  /* USER CODE END  App_ThreadX_LowPower_Exit */
}

/**
  * @brief  App_ThreadX_LowPower_Timer_Adjust
  * @param  None
  * @retval Amount of time (in ticks)
  */
ULONG App_ThreadX_LowPower_Timer_Adjust(void)
{
  /* USER CODE BEGIN  App_ThreadX_LowPower_Timer_Adjust */
  return 0;
  /* USER CODE END  App_ThreadX_LowPower_Timer_Adjust */
}

/* USER CODE BEGIN 1 */
#if defined(TX_PORT_USE_BASEPRI) && ((TX_PORT_BASEPRI) == 0)
#error "TX_PORT_BASEPRI must be non-zero when TX_PORT_USE_BASEPRI is enabled."
#endif

_Static_assert((KNOB_RTOS_POWER_THREAD_STACK_BYTES % sizeof(ULONG)) == 0U, "Power thread stack must align to ULONG");
_Static_assert((KNOB_RTOS_DISPLAY_THREAD_STACK_BYTES % sizeof(ULONG)) == 0U, "Display thread stack must align to ULONG");
_Static_assert((KNOB_RTOS_STORAGE_THREAD_STACK_BYTES % sizeof(ULONG)) == 0U, "Storage thread stack must align to ULONG");
_Static_assert((KNOB_RTOS_INPUT_THREAD_STACK_BYTES % sizeof(ULONG)) == 0U, "Input thread stack must align to ULONG");
_Static_assert((KNOB_RTOS_GAME_THREAD_STACK_BYTES % sizeof(ULONG)) == 0U, "Game thread stack must align to ULONG");
_Static_assert((sizeof(g_th_ui_stack) % sizeof(ULONG)) == 0U, "UI thread stack must align to ULONG");
_Static_assert((sizeof(g_th_game_stack) % sizeof(ULONG)) == 0U, "Game thread stack must align to ULONG");
_Static_assert((KNOB_RTOS_AUDIO_THREAD_STACK_BYTES % sizeof(ULONG)) == 0U, "Audio thread stack must align to ULONG");
_Static_assert((KNOB_RTOS_SENSOR_THREAD_STACK_BYTES % sizeof(ULONG)) == 0U, "Sensor thread stack must align to ULONG");
_Static_assert((KNOB_RTOS_QSYS_EVENTS_DEPTH > 0), "qSysEvents depth must be > 0");
_Static_assert((KNOB_RTOS_QDISPLAY_CMD_DEPTH > 0), "qDisplayCmd depth must be > 0");
_Static_assert((KNOB_RTOS_QSTORAGE_REQ_DEPTH > 0), "qStorageReq depth must be > 0");
_Static_assert((KNOB_RTOS_QINPUT_CMD_DEPTH > 0), "qInputCmd depth must be > 0");
_Static_assert((KNOB_RTOS_QINPUT_RAW_DEPTH > 0), "qInputRaw depth must be > 0");
_Static_assert((KNOB_RTOS_QUI_EVENTS_DEPTH > 0), "qUIEvents depth must be > 0");
_Static_assert((KNOB_RTOS_QGAME_EVENTS_DEPTH > 0), "qGameEvents depth must be > 0");
_Static_assert((KNOB_RTOS_QAUDIO_CMD_DEPTH > 0), "qAudioCmd depth must be > 0");
_Static_assert((KNOB_RTOS_QSENSOR_REQ_DEPTH > 0), "qSensorReq depth must be > 0");
_Static_assert((KNOB_RTOS_POWER_WAIT_TICKS > 0), "Power thread wait ticks must be > 0");
_Static_assert((KNOB_RTOS_DISPLAY_WAIT_TICKS > 0), "Display thread wait ticks must be > 0");
_Static_assert((KNOB_RTOS_STORAGE_WAIT_TICKS > 0), "Storage thread wait ticks must be > 0");
_Static_assert((KNOB_RTOS_INPUT_WAIT_TICKS > 0), "Input thread wait ticks must be > 0");
_Static_assert((KNOB_RTOS_GAME_WAIT_TICKS > 0), "Game thread wait ticks must be > 0");
_Static_assert((KNOB_RTOS_AUDIO_WAIT_TICKS > 0), "Audio thread wait ticks must be > 0");
_Static_assert((KNOB_RTOS_SENSOR_WAIT_TICKS > 0), "Sensor thread wait ticks must be > 0");
_Static_assert((KNOB_RTOS_POWER_QUIESCE_TIMEOUT_TICKS > 0), "Power quiesce timeout must be > 0");
_Static_assert((KNOB_INPUT_REPEAT_PERIOD_STAGE1_TICKS > 0), "Input repeat stage1 period must be > 0");
_Static_assert((KNOB_INPUT_REPEAT_PERIOD_STAGE2_TICKS > 0), "Input repeat stage2 period must be > 0");
_Static_assert((KNOB_INPUT_JOY_REPEAT_PERIOD_STAGE1_TICKS > 0), "Input joy repeat stage1 period must be > 0");
_Static_assert((KNOB_INPUT_JOY_REPEAT_PERIOD_STAGE2_TICKS > 0), "Input joy repeat stage2 period must be > 0");
_Static_assert((KNOB_INPUT_LONG_PRESS_TICKS > 0), "Input long press ticks must be > 0");
_Static_assert((KNOB_INPUT_REPEAT_ACCEL_STAGE2_AFTER_TICKS >= KNOB_INPUT_REPEAT_ACCEL_STAGE1_AFTER_TICKS), "Input repeat stage2 threshold must be >= stage1 threshold");
_Static_assert((KNOB_INPUT_JOY_REPEAT_ACCEL_STAGE2_AFTER_TICKS >= KNOB_INPUT_JOY_REPEAT_ACCEL_STAGE1_AFTER_TICKS), "Input joy repeat stage2 threshold must be >= stage1 threshold");
_Static_assert((KNOB_INPUT_REALTIME_ACTIVITY_MIN_TICKS <= 5000), "Input realtime activity minimum ticks must be <= 5000");
_Static_assert((KNOB_INPUT_REPEAT_ENABLE_MASK <= APP_INPUT_SOURCES_ALL_MASK), "Input repeat enable mask out of range");
_Static_assert((KNOB_INPUT_LONG_ENABLE_MASK <= APP_INPUT_SOURCES_ALL_MASK), "Input long enable mask out of range");
_Static_assert((KNOB_SENSOR_JOY_RELEASE_STABLE_SAMPLES > 0), "Joystick release stable samples must be > 0");
_Static_assert((KNOB_SENSOR_JOY_DIGITAL_RELEASE_PERCENT > 0), "Joystick digital release percent must be > 0");
_Static_assert((KNOB_SENSOR_JOY_DIGITAL_RELEASE_PERCENT < 100), "Joystick digital release percent must be < 100");
_Static_assert((KNOB_SENSOR_JOY_NEUTRAL_DEADZONE_SCALE_PERMILLE > 0), "Joystick neutral deadzone scale must be > 0");
_Static_assert((KNOB_SENSOR_JOY_NEUTRAL_DEADZONE_MIN_MT_X10 > 0), "Joystick neutral deadzone min must be > 0");
_Static_assert((KNOB_SENSOR_JOY_NEUTRAL_DEADZONE_MAX_MT_X10 >= KNOB_SENSOR_JOY_NEUTRAL_DEADZONE_MIN_MT_X10), "Joystick neutral deadzone max must be >= min");
_Static_assert((KNOB_AUDIO_DMA_FRAMES >= 32), "Audio DMA frame count must be >= 32");
_Static_assert((KNOB_AUDIO_DMA_FRAMES % 2) == 0U, "Audio DMA frame count must be even");
_Static_assert((APP_AUDIO_DMA_SAMPLE_COUNT <= 65535UL), "Audio DMA sample count must fit uint16_t");
_Static_assert((KNOB_AUDIO_TEST_TONE_HZ > 0), "Audio test-tone frequency must be > 0");
_Static_assert((KNOB_AUDIO_TEST_TONE_HZ < (KNOB_AUDIO_SAMPLE_RATE / 2)), "Audio test-tone frequency must be < Nyquist");
_Static_assert((KNOB_AUDIO_TEST_TONE_AMPLITUDE > 0), "Audio test-tone amplitude must be > 0");
_Static_assert((KNOB_AUDIO_TEST_TONE_AMPLITUDE <= 32767), "Audio test-tone amplitude must fit int16");
_Static_assert((KNOB_AUDIO_SFX_VOICE_COUNT > 0), "Audio SFX voice count must be > 0");
_Static_assert((KNOB_AUDIO_SFX_VOICE_COUNT <= 8), "Audio SFX voice count must be <= 8");
_Static_assert((KNOB_AUDIO_GAIN_MASTER_PCT >= 0), "Audio master gain percent must be >= 0");
_Static_assert((KNOB_AUDIO_GAIN_MUSIC_PCT >= 0), "Audio music gain percent must be >= 0");
_Static_assert((KNOB_AUDIO_GAIN_SFX_PCT >= 0), "Audio SFX gain percent must be >= 0");
_Static_assert((KNOB_AUDIO_GAIN_MASTER_PCT <= 300), "Audio master gain percent must be <= 300");
_Static_assert((KNOB_AUDIO_GAIN_MUSIC_PCT <= 300), "Audio music gain percent must be <= 300");
_Static_assert((KNOB_AUDIO_GAIN_SFX_PCT <= 300), "Audio SFX gain percent must be <= 300");
_Static_assert((KNOB_SENSOR_RECOVERY_MAX_ATTEMPTS > 0), "Sensor recovery max attempts must be > 0");
_Static_assert((KNOB_SENSOR_RECOVERY_BACKOFF_TICKS > 0), "Sensor recovery backoff ticks must be > 0");
_Static_assert((KNOB_SENSOR_FAULT_RETRY_TICKS > 0), "Sensor fault retry ticks must be > 0");
_Static_assert((KNOB_SENSOR_BUS_RECOVERY_SCL_PULSES >= 9), "Sensor bus recovery pulses must be >= 9");
_Static_assert((KNOB_SENSOR_BUS_RECOVERY_SCL_PULSES <= 16), "Sensor bus recovery pulses must be <= 16");
_Static_assert((KNOB_SENSOR_PMIC_POLL_PERIOD_MS > 0), "PMIC poll period must be > 0 ms");
_Static_assert((KNOB_SENSOR_PMIC_CUTOFF_MV >= 2400), "PMIC cutoff must be >= 2400 mV");
_Static_assert((KNOB_SENSOR_PMIC_CUTOFF_MV <= 3600), "PMIC cutoff must be <= 3600 mV");
_Static_assert((KNOB_SENSOR_PMIC_CUTOFF_HYS_MV <= 400), "PMIC cutoff hysteresis must be <= 400 mV");
_Static_assert((KNOB_SENSOR_PMIC_CUTOFF_CONFIRM_SAMPLES > 0), "PMIC cutoff confirm samples must be > 0");
_Static_assert((KNOB_SENSOR_PMIC_WARN_MV >= KNOB_SENSOR_PMIC_CUTOFF_MV), "PMIC warn mv must be >= cutoff mv");
_Static_assert((KNOB_SENSOR_PMIC_WARN_MV <= 4300), "PMIC warn mv must be <= 4300 mV");
_Static_assert((KNOB_SENSOR_PMIC_WARN_HYS_MV <= 500), "PMIC warn hysteresis must be <= 500 mV");
_Static_assert((KNOB_SENSOR_PMIC_WARN_SOC_PCT <= 100), "PMIC warn soc must be <= 100%");
_Static_assert((KNOB_SENSOR_PMIC_WARN_SOC_HYS_PCT <= 50), "PMIC warn soc hysteresis must be <= 50%");
_Static_assert((KNOB_SENSOR_PMIC_CRIT_SOC_PCT <= 100), "PMIC crit soc must be <= 100%");
_Static_assert((KNOB_SENSOR_PMIC_CRIT_SOC_HYS_PCT <= 50), "PMIC crit soc hysteresis must be <= 50%");
_Static_assert((KNOB_SENSOR_PMIC_CRIT_SOC_PCT <= KNOB_SENSOR_PMIC_WARN_SOC_PCT), "PMIC crit soc must be <= warn soc");
_Static_assert((KNOB_SENSOR_LIS_FS <= LIS2DUX12_16g), "LIS FS knob must map to 2g/4g/8g/16g");
_Static_assert((KNOB_SENSOR_LIS_LOW_POWER_BW <= LIS2DUX12_ODR_div_16), "LIS low-power BW knob out of range");
_Static_assert((KNOB_SENSOR_LIS_LOW_POWER_STEP_BW <= LIS2DUX12_ODR_div_16), "LIS low-power step BW knob out of range");
_Static_assert((KNOB_SENSOR_LIS_LIVE_BW <= LIS2DUX12_ODR_div_16), "LIS live BW knob out of range");
_Static_assert((KNOB_STORAGE_SETTINGS_ADDR < APP_STORAGE_FLASH_SIZE_BYTES), "Storage settings address must be within flash range");
_Static_assert((KNOB_STORAGE_SETTINGS_ADDR % APP_STORAGE_SETTINGS_SECTOR_SIZE) == 0U, "Storage settings address must be 4KiB aligned");
_Static_assert((((uint64_t)KNOB_STORAGE_SETTINGS_ADDR + (uint64_t)APP_STORAGE_SETTINGS_SECTOR_SIZE) <= (uint64_t)APP_STORAGE_FLASH_SIZE_BYTES), "Storage settings region must fit in flash");

_Static_assert((KNOB_STORAGE_SMOKE_ADDR < APP_STORAGE_FLASH_SIZE_BYTES), "Storage smoke address must be within flash range");
_Static_assert((KNOB_STORAGE_SMOKE_ADDR % APP_STORAGE_SMOKE_SECTOR_SIZE) == 0U, "Storage smoke address must be 4KiB aligned");
_Static_assert((KNOB_STORAGE_SMOKE_LEN > 0), "Storage smoke length must be > 0");
_Static_assert((KNOB_STORAGE_SMOKE_LEN <= APP_STORAGE_SMOKE_MAX_LEN), "Storage smoke length must be <= 256 bytes");
_Static_assert((((uint64_t)KNOB_STORAGE_SMOKE_ADDR + (uint64_t)APP_STORAGE_SMOKE_SECTOR_SIZE) <= (uint64_t)APP_STORAGE_FLASH_SIZE_BYTES), "Storage smoke reserved sector must fit in flash");

_Static_assert((KNOB_STORAGE_GAME_PKG_MANIFEST_ADDR < APP_STORAGE_FLASH_SIZE_BYTES), "Storage game package manifest address must be within flash range");
_Static_assert((KNOB_STORAGE_GAME_PKG_MANIFEST_ADDR % APP_STORAGE_SMOKE_SECTOR_SIZE) == 0U, "Storage game package manifest address must be 4KiB aligned");
_Static_assert((KNOB_STORAGE_GAME_PKG_MANIFEST_MAX_BYTES > 0U), "Storage game package manifest size must be > 0");
_Static_assert((KNOB_STORAGE_GAME_PKG_MANIFEST_MAX_BYTES % APP_STORAGE_SMOKE_SECTOR_SIZE) == 0U, "Storage game package manifest size must be 4KiB aligned");
_Static_assert((KNOB_STORAGE_GAME_PKG_MANIFEST_MAX_BYTES <= GAME_PACKAGE_MANIFEST_MAX_BYTES), "Storage game package manifest size must be <= manifest parser max");
_Static_assert((((uint64_t)KNOB_STORAGE_GAME_PKG_MANIFEST_ADDR + (uint64_t)KNOB_STORAGE_GAME_PKG_MANIFEST_MAX_BYTES) <= (uint64_t)APP_STORAGE_FLASH_SIZE_BYTES), "Storage game package manifest region must fit in flash");

_Static_assert((KNOB_STORAGE_AUDIO_CATALOG_ADDR < APP_STORAGE_FLASH_SIZE_BYTES), "Storage audio catalog address must be within flash range");
_Static_assert((KNOB_STORAGE_AUDIO_CATALOG_ADDR % APP_STORAGE_SMOKE_SECTOR_SIZE) == 0U, "Storage audio catalog address must be 4KiB aligned");
_Static_assert((KNOB_STORAGE_AUDIO_CATALOG_MAX_BYTES > 0U), "Storage audio catalog size must be > 0");
_Static_assert((KNOB_STORAGE_AUDIO_CATALOG_MAX_BYTES % APP_STORAGE_SMOKE_SECTOR_SIZE) == 0U, "Storage audio catalog size must be 4KiB aligned");
_Static_assert((((uint64_t)KNOB_STORAGE_AUDIO_CATALOG_ADDR + (uint64_t)KNOB_STORAGE_AUDIO_CATALOG_MAX_BYTES) <= (uint64_t)APP_STORAGE_FLASH_SIZE_BYTES), "Storage audio catalog region must fit in flash");

_Static_assert((KNOB_STORAGE_INSTALLED_BASE_ADDR < APP_STORAGE_FLASH_SIZE_BYTES), "Storage installed base address must be within flash range");
_Static_assert((KNOB_STORAGE_INSTALLED_BASE_ADDR % APP_STORAGE_SMOKE_SECTOR_SIZE) == 0U, "Storage installed base address must be 4KiB aligned");
_Static_assert((KNOB_STORAGE_INSTALLED_SIZE_BYTES > 0U), "Storage installed size must be > 0");
_Static_assert((KNOB_STORAGE_INSTALLED_SIZE_BYTES % APP_STORAGE_SMOKE_SECTOR_SIZE) == 0U, "Storage installed size must be 4KiB aligned");
_Static_assert((KNOB_STORAGE_INSTALLED_SIZE_BYTES >= APP_STORAGE_INSTALL_INDEX_RESERVED_BYTES), "Storage installed size must reserve install-index slots");
_Static_assert((((uint64_t)KNOB_STORAGE_INSTALLED_BASE_ADDR + (uint64_t)KNOB_STORAGE_INSTALLED_SIZE_BYTES) <= (uint64_t)APP_STORAGE_FLASH_SIZE_BYTES), "Storage installed region must fit in flash");
_Static_assert((APP_STORAGE_INSTALL_INDEX_SLOT0_ADDR % APP_STORAGE_SMOKE_SECTOR_SIZE) == 0U, "Install-index slot0 must be 4KiB aligned");
_Static_assert((APP_STORAGE_INSTALL_INDEX_SLOT1_ADDR % APP_STORAGE_SMOKE_SECTOR_SIZE) == 0U, "Install-index slot1 must be 4KiB aligned");
_Static_assert((((uint64_t)APP_STORAGE_INSTALL_INDEX_SLOT0_ADDR + (uint64_t)APP_STORAGE_INSTALL_INDEX_SLOT_SIZE) <= ((uint64_t)KNOB_STORAGE_INSTALLED_BASE_ADDR + (uint64_t)KNOB_STORAGE_INSTALLED_SIZE_BYTES)), "Install-index slot0 must fit in installed region");
_Static_assert((((uint64_t)APP_STORAGE_INSTALL_INDEX_SLOT1_ADDR + (uint64_t)APP_STORAGE_INSTALL_INDEX_SLOT_SIZE) <= ((uint64_t)KNOB_STORAGE_INSTALLED_BASE_ADDR + (uint64_t)KNOB_STORAGE_INSTALLED_SIZE_BYTES)), "Install-index slot1 must fit in installed region");
_Static_assert((sizeof(app_storage_install_index_record_t) <= APP_STORAGE_INSTALL_INDEX_SLOT_SIZE), "Install-index record must fit one slot");
_Static_assert((APP_STORAGE_INSTALL_INDEX_RECORD_CRC_SPAN < sizeof(app_storage_install_index_record_t)), "Install-index CRC span must be valid");
_Static_assert((KNOB_GAME_RT_SCENE_MAP_SIZE_BYTES <= GAME_MAP_BLOB_MAX_BYTES), "Realtime scene map size must be <= parser max");
_Static_assert((KNOB_GAME_RT_SCENE_MAP_ADDR % APP_STORAGE_SMOKE_SECTOR_SIZE) == 0U, "Realtime scene map slot address must be 4KiB aligned");
_Static_assert((((uint64_t)KNOB_GAME_RT_SCENE_MAP_ADDR + (uint64_t)KNOB_GAME_RT_SCENE_MAP_SIZE_BYTES) <= (uint64_t)APP_STORAGE_FLASH_SIZE_BYTES), "Realtime scene map slot must fit in flash");
_Static_assert(((KNOB_GAME_RT_SCENE_MAP_SIZE_BYTES == 0U) ||
               (((uint64_t)KNOB_GAME_RT_SCENE_MAP_ADDR >= (uint64_t)KNOB_STORAGE_INSTALLED_BASE_ADDR) &&
                (((uint64_t)KNOB_GAME_RT_SCENE_MAP_ADDR + (uint64_t)KNOB_GAME_RT_SCENE_MAP_SIZE_BYTES) <=
                 (uint64_t)APP_STORAGE_INSTALLED_DATA_END_ADDR))),
               "Realtime scene map slot must be inside installed raw region or disabled");
_Static_assert((KNOB_GAME_RT_SCENE_TILESET_SIZE_BYTES <= GAME_TILESET_BLOB_MAX_BYTES), "Realtime scene tileset size must be <= parser max");
_Static_assert((KNOB_GAME_RT_SCENE_TILESET_ADDR % APP_STORAGE_SMOKE_SECTOR_SIZE) == 0U, "Realtime scene tileset slot address must be 4KiB aligned");
_Static_assert((((uint64_t)KNOB_GAME_RT_SCENE_TILESET_ADDR + (uint64_t)KNOB_GAME_RT_SCENE_TILESET_SIZE_BYTES) <= (uint64_t)APP_STORAGE_FLASH_SIZE_BYTES), "Realtime scene tileset slot must fit in flash");
_Static_assert(((KNOB_GAME_RT_SCENE_TILESET_SIZE_BYTES == 0U) ||
               (((uint64_t)KNOB_GAME_RT_SCENE_TILESET_ADDR >= (uint64_t)KNOB_STORAGE_INSTALLED_BASE_ADDR) &&
                (((uint64_t)KNOB_GAME_RT_SCENE_TILESET_ADDR + (uint64_t)KNOB_GAME_RT_SCENE_TILESET_SIZE_BYTES) <=
                 (uint64_t)APP_STORAGE_INSTALLED_DATA_END_ADDR))),
               "Realtime scene tileset slot must be inside installed raw region or disabled");

_Static_assert((KNOB_STORAGE_FAT_BASE_ADDR < APP_STORAGE_FLASH_SIZE_BYTES), "Storage FAT base must be within flash range");
_Static_assert((KNOB_STORAGE_FAT_BASE_ADDR % APP_STORAGE_FAT_BLOCK_SIZE) == 0U, "Storage FAT base must be 4KiB aligned");
_Static_assert((KNOB_STORAGE_FAT_SIZE_BYTES > 0), "Storage FAT size must be > 0");
_Static_assert((KNOB_STORAGE_FAT_SIZE_BYTES % APP_STORAGE_FAT_BLOCK_SIZE) == 0U, "Storage FAT size must be 4KiB aligned");
_Static_assert((KNOB_STORAGE_FAT_SIZE_BYTES % APP_STORAGE_FAT_BYTES_PER_SECTOR) == 0U, "Storage FAT size must be sector aligned");
_Static_assert((((uint64_t)KNOB_STORAGE_FAT_BASE_ADDR + (uint64_t)KNOB_STORAGE_FAT_SIZE_BYTES) <= (uint64_t)APP_STORAGE_FLASH_SIZE_BYTES), "Storage FAT region must fit in flash");
_Static_assert((KNOB_STORAGE_FILEX_CACHE_BYTES >= APP_STORAGE_FAT_BYTES_PER_SECTOR), "Storage FileX cache must be >= sector size");
_Static_assert((KNOB_STORAGE_FILEX_CACHE_BYTES % sizeof(ULONG)) == 0U, "Storage FileX cache must align to ULONG");
_Static_assert((KNOB_STORAGE_FILEX_SECTORS_PER_CLUSTER > 0), "Storage FileX sectors/cluster must be > 0");
_Static_assert((KNOB_STORAGE_FILEX_DIR_ENTRIES > 0), "Storage FileX dir entries must be > 0");

/* Explicit raw partition map non-overlap checks (half-open ranges [addr, addr+size)). */
_Static_assert((((uint64_t)KNOB_STORAGE_SETTINGS_ADDR + (uint64_t)APP_STORAGE_SETTINGS_SECTOR_SIZE) <= (uint64_t)KNOB_STORAGE_SMOKE_ADDR) || (((uint64_t)KNOB_STORAGE_SMOKE_ADDR + (uint64_t)APP_STORAGE_SMOKE_SECTOR_SIZE) <= (uint64_t)KNOB_STORAGE_SETTINGS_ADDR), "Storage settings/smoke regions must not overlap");
_Static_assert((((uint64_t)KNOB_STORAGE_SETTINGS_ADDR + (uint64_t)APP_STORAGE_SETTINGS_SECTOR_SIZE) <= (uint64_t)KNOB_STORAGE_GAME_PKG_MANIFEST_ADDR) || (((uint64_t)KNOB_STORAGE_GAME_PKG_MANIFEST_ADDR + (uint64_t)KNOB_STORAGE_GAME_PKG_MANIFEST_MAX_BYTES) <= (uint64_t)KNOB_STORAGE_SETTINGS_ADDR), "Storage settings/manifest regions must not overlap");
_Static_assert((((uint64_t)KNOB_STORAGE_SETTINGS_ADDR + (uint64_t)APP_STORAGE_SETTINGS_SECTOR_SIZE) <= (uint64_t)KNOB_STORAGE_AUDIO_CATALOG_ADDR) || (((uint64_t)KNOB_STORAGE_AUDIO_CATALOG_ADDR + (uint64_t)KNOB_STORAGE_AUDIO_CATALOG_MAX_BYTES) <= (uint64_t)KNOB_STORAGE_SETTINGS_ADDR), "Storage settings/audio regions must not overlap");
_Static_assert((((uint64_t)KNOB_STORAGE_SETTINGS_ADDR + (uint64_t)APP_STORAGE_SETTINGS_SECTOR_SIZE) <= (uint64_t)KNOB_STORAGE_INSTALLED_BASE_ADDR) || (((uint64_t)KNOB_STORAGE_INSTALLED_BASE_ADDR + (uint64_t)KNOB_STORAGE_INSTALLED_SIZE_BYTES) <= (uint64_t)KNOB_STORAGE_SETTINGS_ADDR), "Storage settings/installed regions must not overlap");
_Static_assert((((uint64_t)KNOB_STORAGE_SMOKE_ADDR + (uint64_t)APP_STORAGE_SMOKE_SECTOR_SIZE) <= (uint64_t)KNOB_STORAGE_GAME_PKG_MANIFEST_ADDR) || (((uint64_t)KNOB_STORAGE_GAME_PKG_MANIFEST_ADDR + (uint64_t)KNOB_STORAGE_GAME_PKG_MANIFEST_MAX_BYTES) <= (uint64_t)KNOB_STORAGE_SMOKE_ADDR), "Storage smoke/manifest regions must not overlap");
_Static_assert((((uint64_t)KNOB_STORAGE_SMOKE_ADDR + (uint64_t)APP_STORAGE_SMOKE_SECTOR_SIZE) <= (uint64_t)KNOB_STORAGE_AUDIO_CATALOG_ADDR) || (((uint64_t)KNOB_STORAGE_AUDIO_CATALOG_ADDR + (uint64_t)KNOB_STORAGE_AUDIO_CATALOG_MAX_BYTES) <= (uint64_t)KNOB_STORAGE_SMOKE_ADDR), "Storage smoke/audio regions must not overlap");
_Static_assert((((uint64_t)KNOB_STORAGE_SMOKE_ADDR + (uint64_t)APP_STORAGE_SMOKE_SECTOR_SIZE) <= (uint64_t)KNOB_STORAGE_INSTALLED_BASE_ADDR) || (((uint64_t)KNOB_STORAGE_INSTALLED_BASE_ADDR + (uint64_t)KNOB_STORAGE_INSTALLED_SIZE_BYTES) <= (uint64_t)KNOB_STORAGE_SMOKE_ADDR), "Storage smoke/installed regions must not overlap");
_Static_assert((((uint64_t)KNOB_STORAGE_GAME_PKG_MANIFEST_ADDR + (uint64_t)KNOB_STORAGE_GAME_PKG_MANIFEST_MAX_BYTES) <= (uint64_t)KNOB_STORAGE_AUDIO_CATALOG_ADDR) || (((uint64_t)KNOB_STORAGE_AUDIO_CATALOG_ADDR + (uint64_t)KNOB_STORAGE_AUDIO_CATALOG_MAX_BYTES) <= (uint64_t)KNOB_STORAGE_GAME_PKG_MANIFEST_ADDR), "Storage manifest/audio regions must not overlap");
_Static_assert((((uint64_t)KNOB_STORAGE_GAME_PKG_MANIFEST_ADDR + (uint64_t)KNOB_STORAGE_GAME_PKG_MANIFEST_MAX_BYTES) <= (uint64_t)KNOB_STORAGE_INSTALLED_BASE_ADDR) || (((uint64_t)KNOB_STORAGE_INSTALLED_BASE_ADDR + (uint64_t)KNOB_STORAGE_INSTALLED_SIZE_BYTES) <= (uint64_t)KNOB_STORAGE_GAME_PKG_MANIFEST_ADDR), "Storage manifest/installed regions must not overlap");
_Static_assert((((uint64_t)KNOB_STORAGE_AUDIO_CATALOG_ADDR + (uint64_t)KNOB_STORAGE_AUDIO_CATALOG_MAX_BYTES) <= (uint64_t)KNOB_STORAGE_INSTALLED_BASE_ADDR) || (((uint64_t)KNOB_STORAGE_INSTALLED_BASE_ADDR + (uint64_t)KNOB_STORAGE_INSTALLED_SIZE_BYTES) <= (uint64_t)KNOB_STORAGE_AUDIO_CATALOG_ADDR), "Storage audio/installed regions must not overlap");
_Static_assert((((uint64_t)KNOB_STORAGE_AUDIO_CATALOG_ADDR + (uint64_t)KNOB_STORAGE_AUDIO_CATALOG_MAX_BYTES) <= (uint64_t)KNOB_STORAGE_FAT_BASE_ADDR) || (((uint64_t)KNOB_STORAGE_FAT_BASE_ADDR + (uint64_t)KNOB_STORAGE_FAT_SIZE_BYTES) <= (uint64_t)KNOB_STORAGE_AUDIO_CATALOG_ADDR), "Storage audio/FAT regions must not overlap");
_Static_assert((((uint64_t)KNOB_STORAGE_INSTALLED_BASE_ADDR + (uint64_t)KNOB_STORAGE_INSTALLED_SIZE_BYTES) <= (uint64_t)KNOB_STORAGE_FAT_BASE_ADDR) || (((uint64_t)KNOB_STORAGE_FAT_BASE_ADDR + (uint64_t)KNOB_STORAGE_FAT_SIZE_BYTES) <= (uint64_t)KNOB_STORAGE_INSTALLED_BASE_ADDR), "Storage installed/FAT regions must not overlap");
_Static_assert((((uint64_t)KNOB_STORAGE_GAME_PKG_MANIFEST_ADDR + (uint64_t)KNOB_STORAGE_GAME_PKG_MANIFEST_MAX_BYTES) <= (uint64_t)KNOB_STORAGE_FAT_BASE_ADDR) || (((uint64_t)KNOB_STORAGE_FAT_BASE_ADDR + (uint64_t)KNOB_STORAGE_FAT_SIZE_BYTES) <= (uint64_t)KNOB_STORAGE_GAME_PKG_MANIFEST_ADDR), "Storage manifest/FAT regions must not overlap");
_Static_assert((((uint64_t)KNOB_STORAGE_SMOKE_ADDR + (uint64_t)APP_STORAGE_SMOKE_SECTOR_SIZE) <= (uint64_t)KNOB_STORAGE_FAT_BASE_ADDR) || (((uint64_t)KNOB_STORAGE_FAT_BASE_ADDR + (uint64_t)KNOB_STORAGE_FAT_SIZE_BYTES) <= (uint64_t)KNOB_STORAGE_SMOKE_ADDR), "Storage smoke/FAT regions must not overlap");
_Static_assert((((uint64_t)KNOB_STORAGE_SETTINGS_ADDR + (uint64_t)APP_STORAGE_SETTINGS_SECTOR_SIZE) <= (uint64_t)KNOB_STORAGE_FAT_BASE_ADDR) || (((uint64_t)KNOB_STORAGE_FAT_BASE_ADDR + (uint64_t)KNOB_STORAGE_FAT_SIZE_BYTES) <= (uint64_t)KNOB_STORAGE_SETTINGS_ADDR), "Storage settings/FAT regions must not overlap");
_Static_assert((sizeof(app_storage_audio_catalog_header_t) % sizeof(uint32_t)) == 0U, "Audio catalog header must align to uint32_t");
_Static_assert((sizeof(app_storage_audio_catalog_entry_t) % sizeof(uint32_t)) == 0U, "Audio catalog entry must align to uint32_t");
_Static_assert((APP_STORAGE_AUDIO_CATALOG_MAX_ENTRIES > 0UL), "Audio catalog max entries must be > 0");
_Static_assert((APP_STORAGE_AUDIO_CHUNK_MAX_BYTES > 0UL), "Audio chunk max bytes must be > 0");
_Static_assert((APP_STORAGE_AUDIO_CHUNK_MAX_BYTES <= APP_STORAGE_SMOKE_SECTOR_SIZE), "Audio chunk max bytes must be <= storage sector size");
_Static_assert((APP_STORAGE_AUDIO_CHUNK_CACHE_DEPTH > 0UL), "Audio chunk cache depth must be > 0");
_Static_assert((APP_AUDIO_EXTERNAL_BLOCK_MAX_BYTES > 0U), "Audio external block max bytes must be > 0");
_Static_assert((APP_AUDIO_EXTERNAL_BLOCK_MAX_BYTES <= APP_STORAGE_AUDIO_CHUNK_MAX_BYTES), "Audio external block max bytes must be <= storage chunk max bytes");
_Static_assert((KNOB_SENSOR_JOY_CAL_NEUTRAL_WINDOW_MS > 0U), "Joy neutral calibration window must be > 0");
_Static_assert((KNOB_SENSOR_JOY_CAL_NEUTRAL_STEP_MS > 0U), "Joy neutral calibration step must be > 0");
_Static_assert((KNOB_SENSOR_JOY_CAL_DIRECTION_WINDOW_MS > 0U), "Joy direction calibration window must be > 0");
_Static_assert((KNOB_SENSOR_JOY_CAL_DIRECTION_STEP_MS > 0U), "Joy direction calibration step must be > 0");
_Static_assert((KNOB_SENSOR_JOY_CAL_SWEEP_WINDOW_MS > 0U), "Joy sweep calibration window must be > 0");
_Static_assert((KNOB_SENSOR_JOY_CAL_SWEEP_STEP_MS > 0U), "Joy sweep calibration step must be > 0");
_Static_assert((KNOB_UI_STATIC_ENTRY_POINT <= APP_UI_STATIC_ENTRY_JOY_CAL), "UI static entry point knob out of range");
_Static_assert((sizeof(app_sys_event_t) % sizeof(ULONG)) == 0U, "qSysEvents payload must align to ULONG words");
_Static_assert((APP_QSYS_EVENT_WORDS == 4U), "qSysEvents payload contract requires 4 ULONG words");
_Static_assert((sizeof(app_display_cmd_t) % sizeof(ULONG)) == 0U, "qDisplayCmd payload must align to ULONG words");
_Static_assert((sizeof(app_storage_req_t) % sizeof(ULONG)) == 0U, "qStorageReq payload must align to ULONG words");
_Static_assert((sizeof(app_input_cmd_t) % sizeof(ULONG)) == 0U, "qInputCmd payload must align to ULONG words");
_Static_assert((sizeof(app_input_raw_evt_t) % sizeof(ULONG)) == 0U, "qInputRaw payload must align to ULONG words");
_Static_assert((sizeof(app_input_action_evt_t) % sizeof(ULONG)) == 0U, "qUIEvents/qGameEvents payload must align to ULONG words");
_Static_assert((sizeof(app_audio_cmd_t) % sizeof(ULONG)) == 0U, "qAudioCmd payload must align to ULONG words");
_Static_assert((sizeof(app_sensor_req_t) % sizeof(ULONG)) == 0U, "qSensorReq payload must align to ULONG words");
_Static_assert((APP_AUDIO_CMD_WORDS == 2U), "qAudioCmd payload contract requires 2 ULONG words");
_Static_assert((APP_INPUT_RAW_WORDS == 4U), "qInputRaw payload contract requires 4 ULONG words");
_Static_assert((APP_INPUT_ACTION_WORDS == 5U), "qUIEvents/qGameEvents payload contract requires 5 ULONG words");
_Static_assert((APP_SENSOR_REQ_WORDS == 4U), "qSensorReq payload contract requires 4 ULONG words");

static void AppDisplaySetTranslator(uint8_t enabled)
{
  (void)enabled;

  /* Display translator must remain enabled so EXTCOM reaches the panel. */
  HAL_GPIO_WritePin(VLT_LCD_GPIO_Port, VLT_LCD_Pin, APP_DISPLAY_VLT_ACTIVE_LEVEL);
}

static void AppDisplaySetCs(uint8_t active)
{
  GPIO_PinState state = (active != 0U) ? GPIO_PIN_SET : GPIO_PIN_RESET;

  HAL_GPIO_WritePin(SPI3_CS_GPIO_Port, SPI3_CS_Pin, state);
}

static HAL_StatusTypeDef AppDisplayEnsureReady(void)
{
  HAL_StatusTypeDef hal_status;

  AppDisplaySetTranslator(1U);

  if (g_display_ready != 0U)
  {
    return HAL_OK;
  }

  AppDisplaySetCs(0U);
  hal_status = LCD_Init(&g_display_dev, &hspi3, SPI3_CS_GPIO_Port, SPI3_CS_Pin);
  if (hal_status == HAL_OK)
  {
    g_display_ready = 1U;
  }

  return hal_status;
}

static uint8_t AppModeToThMode(app_mode_t mode_token, th_mode_t *mode_out)
{
  if (mode_out == NULL)
  {
    return 0U;
  }

  switch (mode_token)
  {
    case APP_MODE_STOP:
      *mode_out = TH_MODE_STOP;
      break;

    case APP_MODE_STATIC:
      *mode_out = TH_MODE_STATIC;
      break;

    case APP_MODE_REALTIME:
      *mode_out = TH_MODE_REALTIME;
      break;

    case APP_MODE_FLASHING:
      *mode_out = TH_MODE_FLASHING;
      break;

    default:
      return 0U;
  }

  return 1U;
}

static void AppDisplayPrepareBootstrapFrame(void)
{
  Render_Init();
  renderClear(RENDER_COLOR_WHITE);
  Render_SetModeIndicator(TH_MODE_STATIC);
}

static void AppDebugDisplayStackSample(void)
{
  ULONG sp = (ULONG)g_th_display.tx_thread_stack_ptr;

  if (sp == 0UL)
  {
    return;
  }

  if ((g_dbg_display_stack_min_sp == 0UL) || (sp < g_dbg_display_stack_min_sp))
  {
    g_dbg_display_stack_min_sp = sp;
  }

  if (g_dbg_display_stack_sample_count < 0xFFFFFFFFUL)
  {
    g_dbg_display_stack_sample_count++;
  }
}

static UINT AppRendererLock(void)
{
  UINT status = tx_mutex_get(&g_mtx_renderer, TX_WAIT_FOREVER);
  if ((status != TX_SUCCESS) && (g_renderer_lock_error_count < 0xFFFFFFFFUL))
  {
    g_renderer_lock_error_count++;
  }
  return status;
}

static VOID AppRendererUnlock(void)
{
  (void)tx_mutex_put(&g_mtx_renderer);
}

static HAL_StatusTypeDef AppDisplayPresent(uint16_t *dirty_rows_out, uint8_t *full_flush_out)
{
  uint16_t dirty_count = 0U;
  bool full_flush = false;
  const uint8_t *framebuffer = NULL;
  HAL_StatusTypeDef hal_status = AppDisplayEnsureReady();

  AppDebugDisplayStackSample();

  if (dirty_rows_out != NULL)
  {
    *dirty_rows_out = 0U;
  }
  if (full_flush_out != NULL)
  {
    *full_flush_out = 0U;
  }

  if (hal_status != HAL_OK)
  {
    return HAL_ERROR;
  }

  if (AppRendererLock() != TX_SUCCESS)
  {
    return HAL_ERROR;
  }

  framebuffer = Render_GetBuffer();
  if (framebuffer == NULL)
  {
    AppRendererUnlock();
    return HAL_ERROR;
  }

  if (Render_TakeDirtyRows(g_display_dirty_rows, DISPLAY_HEIGHT, &dirty_count, &full_flush) == false)
  {
    AppRendererUnlock();
    return HAL_OK;
  }
  AppRendererUnlock();

  if (dirty_rows_out != NULL)
  {
    *dirty_rows_out = dirty_count;
  }
  if (full_flush_out != NULL)
  {
    *full_flush_out = (full_flush != false) ? 1U : 0U;
  }

  if (full_flush != false)
  {
    hal_status = LCD_FlushAll(&g_display_dev, framebuffer);
    AppDebugDisplayStackSample();
    return hal_status;
  }

  hal_status = LCD_FlushRows(&g_display_dev, framebuffer, g_display_dirty_rows, dirty_count);
  AppDebugDisplayStackSample();
  return hal_status;
}

static VOID AppAudioVoiceDecodeReset(app_audio_voice_t *voice)
{
  if (voice == (app_audio_voice_t *)0)
  {
    return;
  }

  voice->sample_cursor = 0UL;
  voice->decode.predictor = 0L;
  voice->decode.index = 0L;
  voice->decode.byte_cache = 0U;
  voice->decode.have_high_nibble = 0U;
  voice->decode.nibbles_left_in_block = 0UL;
  voice->decode.data_offset = 0UL;
  voice->decode.block_cursor = 0UL;
  voice->decode.block_loaded = 0UL;
  voice->decode.emit_block_header = 0UL;
  voice->external_req_pending = 0U;
  voice->external_prefetch_pending = 0U;
  voice->external_prefetch_ready = 0U;
  voice->external_req_token = 0UL;
  voice->external_req_addr = 0UL;
  voice->external_req_len = 0UL;
  voice->external_prefetch_token = 0UL;
  voice->external_prefetch_addr = 0UL;
  voice->external_prefetch_offset = 0UL;
  voice->external_prefetch_len = 0UL;
}

static VOID AppAudioVoiceReset(app_audio_voice_t *voice)
{
  if (voice == (app_audio_voice_t *)0)
  {
    return;
  }

  voice->active = 0U;
  voice->loop = 0U;
  voice->source_kind = APP_AUDIO_CATALOG_SOURCE_NONE;
  voice->gain_class = (uint8_t)APP_AUDIO_GAIN_CLASS_SFX;
  voice->external_req_pending = 0U;
  voice->external_prefetch_pending = 0U;
  voice->external_prefetch_ready = 0U;
  voice->clip = (const app_audio_adpcm_clip_t *)0;
  voice->embedded_data = (const uint8_t *)0;
  voice->data_size_bytes = 0UL;
  voice->external_data_addr = 0UL;
  voice->external_req_token = 0UL;
  voice->external_req_addr = 0UL;
  voice->external_req_len = 0UL;
  voice->external_prefetch_token = 0UL;
  voice->external_prefetch_addr = 0UL;
  voice->external_prefetch_offset = 0UL;
  voice->external_prefetch_len = 0UL;
  voice->block_align = 0U;
  voice->samples_per_block = 0U;
  voice->sample_rate_hz = 0UL;
  voice->sample_count = 0UL;
  voice->sample_cursor = 0UL;
  voice->start_seq = 0UL;
  (void)memset(voice->external_block_buf, 0, sizeof(voice->external_block_buf));
  (void)memset(voice->external_prefetch_buf, 0, sizeof(voice->external_prefetch_buf));
  (void)memset(&voice->decode, 0, sizeof(voice->decode));
}

static VOID AppAudioVoiceStart(app_audio_voice_t *voice, const app_audio_adpcm_clip_t *clip, uint8_t loop)
{
  if (voice == (app_audio_voice_t *)0)
  {
    return;
  }

  voice->active = 1U;
  voice->loop = (loop != 0U) ? 1U : 0U;
  voice->source_kind = APP_AUDIO_CATALOG_SOURCE_EMBEDDED;
  voice->external_req_pending = 0U;
  voice->external_prefetch_pending = 0U;
  voice->external_prefetch_ready = 0U;
  voice->clip = clip;
  voice->embedded_data = (clip != (const app_audio_adpcm_clip_t *)0) ? clip->data : (const uint8_t *)0;
  voice->data_size_bytes = (clip != (const app_audio_adpcm_clip_t *)0) ? (ULONG)clip->data_size : 0UL;
  voice->external_data_addr = 0UL;
  voice->external_req_token = 0UL;
  voice->external_req_addr = 0UL;
  voice->external_req_len = 0UL;
  voice->external_prefetch_token = 0UL;
  voice->external_prefetch_addr = 0UL;
  voice->external_prefetch_offset = 0UL;
  voice->external_prefetch_len = 0UL;
  voice->block_align = (clip != (const app_audio_adpcm_clip_t *)0) ? clip->block_align : 0U;
  voice->samples_per_block = (clip != (const app_audio_adpcm_clip_t *)0) ? clip->samples_per_block : 0U;
  voice->sample_rate_hz = (clip != (const app_audio_adpcm_clip_t *)0) ? clip->sample_rate_hz : 0UL;
  voice->sample_count = (clip != (const app_audio_adpcm_clip_t *)0) ? (ULONG)clip->total_samples : 0UL;
  voice->start_seq = ++g_audio_voice_seq_counter;
  AppAudioVoiceDecodeReset(voice);
}

static VOID AppAudioVoiceStartExternal(app_audio_voice_t *voice, const app_audio_catalog_entry_t *entry, uint8_t loop)
{
  if ((voice == (app_audio_voice_t *)0) || (entry == (const app_audio_catalog_entry_t *)0))
  {
    return;
  }

  voice->active = 1U;
  voice->loop = (loop != 0U) ? 1U : 0U;
  voice->source_kind = APP_AUDIO_CATALOG_SOURCE_EXTERNAL;
  voice->external_req_pending = 0U;
  voice->external_prefetch_pending = 0U;
  voice->external_prefetch_ready = 0U;
  voice->clip = (const app_audio_adpcm_clip_t *)0;
  voice->embedded_data = (const uint8_t *)0;
  voice->data_size_bytes = entry->data_size;
  voice->external_data_addr = entry->data_offset;
  voice->external_req_token = 0UL;
  voice->external_req_addr = 0UL;
  voice->external_req_len = 0UL;
  voice->external_prefetch_token = 0UL;
  voice->external_prefetch_addr = 0UL;
  voice->external_prefetch_offset = 0UL;
  voice->external_prefetch_len = 0UL;
  voice->block_align = entry->block_align;
  voice->samples_per_block = entry->samples_per_block;
  voice->sample_rate_hz = (uint32_t)entry->sample_rate_hz;
  voice->sample_count = entry->total_samples;
  voice->start_seq = ++g_audio_voice_seq_counter;
  (void)memset(voice->external_block_buf, 0, sizeof(voice->external_block_buf));
  (void)memset(voice->external_prefetch_buf, 0, sizeof(voice->external_prefetch_buf));
  AppAudioVoiceDecodeReset(voice);
}

static uint8_t AppAudioAnyVoiceActive(void)
{
  ULONG i;

  if (g_audio_music_voice.active != 0U)
  {
    return 1U;
  }

  for (i = 0UL; i < (ULONG)KNOB_AUDIO_SFX_VOICE_COUNT; i++)
  {
    if (g_audio_sfx_voices[i].active != 0U)
    {
      return 1U;
    }
  }

  return 0U;
}

static ULONG AppAudioSfxActiveCount(void)
{
  ULONG i;
  ULONG count = 0UL;

  for (i = 0UL; i < (ULONG)KNOB_AUDIO_SFX_VOICE_COUNT; i++)
  {
    if (g_audio_sfx_voices[i].active != 0U)
    {
      count++;
    }
  }

  return count;
}

static VOID AppStorageAudioChunkCacheReset(void)
{
  g_storage_audio_chunk_cache_seq = 0UL;
  (void)memset(g_storage_audio_chunk_cache, 0, sizeof(g_storage_audio_chunk_cache));
}

static VOID AppStorageAudioChunkCachePublish(ULONG token,
                                             ULONG addr,
                                             ULONG len,
                                             ULONG status,
                                             ULONG crc32,
                                             const uint8_t *data_ptr)
{
  ULONG next_seq = g_storage_audio_chunk_cache_seq + 1UL;
  ULONG slot_ix;
  app_storage_audio_chunk_cache_entry_t *slot;

  if (next_seq == 0UL)
  {
    next_seq = 1UL;
  }

  slot_ix = next_seq % APP_STORAGE_AUDIO_CHUNK_CACHE_DEPTH;
  slot = &g_storage_audio_chunk_cache[slot_ix];
  slot->seq = 0UL;
  slot->token = token;
  slot->addr = addr;
  slot->len = len;
  slot->status = status;
  slot->crc32 = crc32;
  if ((status == (ULONG)TX_SUCCESS) &&
      (data_ptr != (const uint8_t *)0) &&
      (len <= APP_STORAGE_AUDIO_CHUNK_MAX_BYTES))
  {
    (void)memcpy(slot->data, data_ptr, len);
  }
  slot->seq = next_seq;
  g_storage_audio_chunk_cache_seq = next_seq;
}

static UINT AppStorageAudioChunkCacheConsume(ULONG token,
                                             ULONG addr,
                                             ULONG len,
                                             uint8_t *dst_ptr)
{
  ULONG depth = APP_STORAGE_AUDIO_CHUNK_CACHE_DEPTH;
  ULONG base_seq = g_storage_audio_chunk_cache_seq;
  ULONG scan;

  if ((len == 0UL) || (len > APP_STORAGE_AUDIO_CHUNK_MAX_BYTES))
  {
    return TX_SIZE_ERROR;
  }

  for (scan = 0UL; scan < depth; scan++)
  {
    ULONG seq_candidate = base_seq - scan;
    ULONG slot_ix;
    app_storage_audio_chunk_cache_entry_t *slot;
    ULONG seq_before;
    ULONG seq_after;

    if (seq_candidate == 0UL)
    {
      break;
    }

    slot_ix = seq_candidate % depth;
    slot = &g_storage_audio_chunk_cache[slot_ix];
    seq_before = slot->seq;
    if (seq_before == 0UL)
    {
      continue;
    }
    if ((slot->token != token) ||
        (slot->addr != addr) ||
        (slot->len != len))
    {
      continue;
    }

    if (slot->status == (ULONG)TX_SUCCESS)
    {
      if (dst_ptr == (uint8_t *)0)
      {
        return TX_PTR_ERROR;
      }
      (void)memcpy(dst_ptr, slot->data, len);
    }

    seq_after = slot->seq;
    if ((seq_after == seq_before) && (seq_after != 0UL))
    {
      if (slot->status == (ULONG)TX_SUCCESS)
      {
        return TX_SUCCESS;
      }
      return TX_NOT_DONE;
    }
  }

  return TX_NO_EVENTS;
}

static UINT AppAudioStorageReadExternalBlock(app_audio_voice_t *voice, ULONG offset, ULONG len)
{
  ULONG token;
  ULONG target_addr;
  UINT post_status;

  if (voice == (app_audio_voice_t *)0)
  {
    return TX_PTR_ERROR;
  }

  if ((len == 0UL) || (len > APP_AUDIO_EXTERNAL_BLOCK_MAX_BYTES))
  {
    return TX_SIZE_ERROR;
  }

  if ((offset >= voice->data_size_bytes) || ((offset + len) > voice->data_size_bytes))
  {
    return TX_SIZE_ERROR;
  }

  target_addr = voice->external_data_addr + offset;
  if ((voice->external_req_pending == 0U) ||
      (voice->external_req_addr != target_addr) ||
      (voice->external_req_len != len))
  {
    token = ++g_audio_storage_req_token_seq;
    if (token == 0UL)
    {
      token = ++g_audio_storage_req_token_seq;
    }

    voice->external_req_token = token;
    voice->external_req_addr = target_addr;
    voice->external_req_len = len;

    post_status = App_StorageReq_AudioChunkRead(target_addr, len, token);
    if (post_status != TX_SUCCESS)
    {
      if (post_status == TX_QUEUE_FULL)
      {
        return TX_NO_EVENTS;
      }
      return post_status;
    }

    voice->external_req_pending = 1U;
    return TX_NO_EVENTS;
  }

  {
    UINT consume_status = AppStorageAudioChunkCacheConsume(voice->external_req_token,
                                                           voice->external_req_addr,
                                                           voice->external_req_len,
                                                           voice->external_block_buf);
    if (consume_status == TX_SUCCESS)
    {
      voice->external_req_pending = 0U;
      return TX_SUCCESS;
    }
    if (consume_status == TX_NOT_DONE)
    {
      voice->external_req_pending = 0U;
      return TX_NOT_DONE;
    }
    if ((consume_status != TX_NO_EVENTS) &&
        (consume_status != TX_SIZE_ERROR))
    {
      return consume_status;
    }
  }

  return TX_NO_EVENTS;
}

static UINT AppAudioStoragePrefetchExternalBlock(app_audio_voice_t *voice, ULONG offset, ULONG len)
{
  ULONG token;
  ULONG target_addr;
  UINT post_status;

  if (voice == (app_audio_voice_t *)0)
  {
    return TX_PTR_ERROR;
  }

  if ((len == 0UL) || (len > APP_AUDIO_EXTERNAL_BLOCK_MAX_BYTES))
  {
    return TX_SIZE_ERROR;
  }

  if ((offset >= voice->data_size_bytes) || ((offset + len) > voice->data_size_bytes))
  {
    return TX_SIZE_ERROR;
  }

  target_addr = voice->external_data_addr + offset;

  if ((voice->external_prefetch_ready != 0U) &&
      (voice->external_prefetch_offset == offset) &&
      (voice->external_prefetch_len == len))
  {
    return TX_SUCCESS;
  }

  if (voice->external_prefetch_pending != 0U)
  {
    UINT consume_status = AppStorageAudioChunkCacheConsume(voice->external_prefetch_token,
                                                           voice->external_prefetch_addr,
                                                           voice->external_prefetch_len,
                                                           voice->external_prefetch_buf);
    if (consume_status == TX_SUCCESS)
    {
      voice->external_prefetch_pending = 0U;
      voice->external_prefetch_ready = 1U;
      return TX_SUCCESS;
    }
    if (consume_status == TX_NOT_DONE)
    {
      voice->external_prefetch_pending = 0U;
      voice->external_prefetch_ready = 0U;
      return TX_NOT_DONE;
    }
    if ((consume_status != TX_NO_EVENTS) &&
        (consume_status != TX_SIZE_ERROR))
    {
      return consume_status;
    }

    return TX_NO_EVENTS;
  }

  token = ++g_audio_storage_req_token_seq;
  if (token == 0UL)
  {
    token = ++g_audio_storage_req_token_seq;
  }

  voice->external_prefetch_token = token;
  voice->external_prefetch_addr = target_addr;
  voice->external_prefetch_offset = offset;
  voice->external_prefetch_len = len;
  voice->external_prefetch_ready = 0U;

  post_status = App_StorageReq_AudioChunkRead(target_addr, len, token);
  if (post_status != TX_SUCCESS)
  {
    if (post_status == TX_QUEUE_FULL)
    {
      return TX_NO_EVENTS;
    }
    return post_status;
  }

  voice->external_prefetch_pending = 1U;
  return TX_NO_EVENTS;
}

static VOID AppAudioFillFrames(ULONG frame_offset, ULONG frame_count)
{
  ULONG frame_index;
  ULONG sample_index = (frame_offset * APP_AUDIO_CHANNEL_COUNT);
  const int32_t base_master_q15 = ((int32_t)KNOB_AUDIO_GAIN_MASTER_PCT * 32768 + 50) / 100;
  const int32_t base_music_q15 = ((int32_t)KNOB_AUDIO_GAIN_MUSIC_PCT * 32768 + 50) / 100;
  const int32_t base_sfx_q15 = ((int32_t)KNOB_AUDIO_GAIN_SFX_PCT * 32768 + 50) / 100;
  ULONG user_master_pct = g_audio_user_gain_master_pct;
  ULONG user_music_pct = g_audio_user_gain_music_pct;
  ULONG user_sfx_pct = g_audio_user_gain_sfx_pct;
  ULONG user_ui_pct = g_audio_user_gain_ui_pct;
  int32_t user_master_q15;
  int32_t user_music_q15;
  int32_t user_sfx_q15;
  int32_t user_ui_q15;
  int32_t master_mul_q15;
  int32_t music_mul_q15;
  int32_t sfx_mul_q15;
  int32_t ui_mul_q15;

  if (user_master_pct > APP_STORAGE_USER_GAIN_MAX_PCT)
  {
    user_master_pct = APP_STORAGE_USER_GAIN_MAX_PCT;
  }
  if (user_music_pct > APP_STORAGE_USER_GAIN_MAX_PCT)
  {
    user_music_pct = APP_STORAGE_USER_GAIN_MAX_PCT;
  }
  if (user_sfx_pct > APP_STORAGE_USER_GAIN_MAX_PCT)
  {
    user_sfx_pct = APP_STORAGE_USER_GAIN_MAX_PCT;
  }
  if (user_ui_pct > APP_STORAGE_USER_GAIN_MAX_PCT)
  {
    user_ui_pct = APP_STORAGE_USER_GAIN_MAX_PCT;
  }

  user_master_q15 = ((int32_t)user_master_pct * 32768 + 50) / 100;
  user_music_q15 = ((int32_t)user_music_pct * 32768 + 50) / 100;
  user_sfx_q15 = ((int32_t)user_sfx_pct * 32768 + 50) / 100;
  user_ui_q15 = ((int32_t)user_ui_pct * 32768 + 50) / 100;
  master_mul_q15 = (int32_t)(((int64_t)base_master_q15 * (int64_t)user_master_q15 + 16384LL) >> 15);
  music_mul_q15 = (int32_t)(((int64_t)base_music_q15 * (int64_t)user_music_q15 + 16384LL) >> 15);
  sfx_mul_q15 = (int32_t)(((int64_t)base_sfx_q15 * (int64_t)user_sfx_q15 + 16384LL) >> 15);
  ui_mul_q15 = (int32_t)(((int64_t)base_sfx_q15 * (int64_t)user_ui_q15 + 16384LL) >> 15);
  music_mul_q15 = (int32_t)(((int64_t)music_mul_q15 * (int64_t)master_mul_q15 + 16384LL) >> 15);
  sfx_mul_q15 = (int32_t)(((int64_t)sfx_mul_q15 * (int64_t)master_mul_q15 + 16384LL) >> 15);
  ui_mul_q15 = (int32_t)(((int64_t)ui_mul_q15 * (int64_t)master_mul_q15 + 16384LL) >> 15);

  if (g_audio_source_kind == APP_AUDIO_SOURCE_TONE)
  {
    int16_t high = (int16_t)KNOB_AUDIO_TEST_TONE_AMPLITUDE;
    int16_t low = (int16_t)(-(int32_t)KNOB_AUDIO_TEST_TONE_AMPLITUDE);

    for (frame_index = 0UL; frame_index < frame_count; frame_index++)
    {
      int16_t sample = ((g_audio_phase_accum & 0x80000000UL) != 0UL) ? high : low;
      int32_t mixed = (int32_t)(((int32_t)sample * sfx_mul_q15) >> 15);
      int16_t out;

      g_audio_phase_accum += g_audio_phase_step;
      if (mixed > 32767)
      {
        out = 32767;
      }
      else if (mixed < -32768)
      {
        out = -32768;
      }
      else
      {
        out = (int16_t)mixed;
      }
      g_audio_dma_buffer[sample_index] = out;
      g_audio_dma_buffer[sample_index + 1UL] = out;
      sample_index += APP_AUDIO_CHANNEL_COUNT;
    }
    return;
  }

  for (frame_index = 0UL; frame_index < frame_count; frame_index++)
  {
    int16_t sample;
    int32_t accum = 0;
    int16_t out;
    ULONG i;

    if (AppAudioVoiceFetchSample(&g_audio_music_voice, &sample) != 0U)
    {
      accum += (int32_t)(((int32_t)sample * music_mul_q15) >> 15);
    }

    for (i = 0UL; i < (ULONG)KNOB_AUDIO_SFX_VOICE_COUNT; i++)
    {
      if (AppAudioVoiceFetchSample(&g_audio_sfx_voices[i], &sample) != 0U)
      {
        int32_t voice_mul_q15 = (g_audio_sfx_voices[i].gain_class == (uint8_t)APP_AUDIO_GAIN_CLASS_UI)
                                    ? ui_mul_q15
                                    : sfx_mul_q15;
        accum += (int32_t)(((int32_t)sample * voice_mul_q15) >> 15);
      }
    }

    if (accum > 32767)
    {
      out = 32767;
    }
    else if (accum < -32768)
    {
      out = -32768;
    }
    else
    {
      out = (int16_t)accum;
    }

    g_audio_dma_buffer[sample_index] = out;
    g_audio_dma_buffer[sample_index + 1UL] = out;
    sample_index += APP_AUDIO_CHANNEL_COUNT;
  }
}

static int16_t AppAudioAdpcmDecodeNibble(app_audio_adpcm_decode_t *decode, uint8_t nibble)
{
  static const int8_t kIndexTable[16] =
  {
    -1, -1, -1, -1, 2, 4, 6, 8,
    -1, -1, -1, -1, 2, 4, 6, 8
  };
  static const int16_t kStepTable[89] =
  {
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17,
    19, 21, 23, 25, 28, 31, 34, 37, 41, 45,
    50, 55, 60, 66, 73, 80, 88, 97, 107, 118,
    130, 143, 157, 173, 190, 209, 230, 253, 279, 307,
    337, 371, 408, 449, 494, 544, 598, 658, 724, 796,
    876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066,
    2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358,
    5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899,
    15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767
  };
  int32_t step;
  int32_t diff;
  int32_t predictor;
  int32_t index;

  if (decode == (app_audio_adpcm_decode_t *)0)
  {
    return 0;
  }

  index = decode->index;
  if (index < 0L)
  {
    index = 0L;
  }
  else if (index > 88L)
  {
    index = 88L;
  }

  step = (int32_t)kStepTable[index];
  diff = step >> 3;
  if ((nibble & 0x01U) != 0U)
  {
    diff += (step >> 2);
  }
  if ((nibble & 0x02U) != 0U)
  {
    diff += (step >> 1);
  }
  if ((nibble & 0x04U) != 0U)
  {
    diff += step;
  }

  predictor = decode->predictor;
  if ((nibble & 0x08U) != 0U)
  {
    predictor -= diff;
  }
  else
  {
    predictor += diff;
  }

  if (predictor < -32768L)
  {
    predictor = -32768L;
  }
  else if (predictor > 32767L)
  {
    predictor = 32767L;
  }
  decode->predictor = predictor;

  index += (int32_t)kIndexTable[nibble & 0x0FU];
  if (index < 0L)
  {
    index = 0L;
  }
  else if (index > 88L)
  {
    index = 88L;
  }
  decode->index = index;

  return (int16_t)predictor;
}

static UINT AppAudioAdpcmDecodeNext(app_audio_voice_t *voice, int16_t *sample_out)
{
  app_audio_adpcm_decode_t *decode;
  const uint8_t *data = (const uint8_t *)0;
  ULONG data_size = 0UL;
  ULONG source_kind;
  ULONG block_align;
  ULONG block_payload_bytes;
  uint8_t nibble = 0U;

  if ((voice == (app_audio_voice_t *)0) || (sample_out == (int16_t *)0))
  {
    return TX_PTR_ERROR;
  }

  decode = &voice->decode;
  source_kind = voice->source_kind;
  if ((source_kind != APP_AUDIO_CATALOG_SOURCE_EMBEDDED) &&
      (source_kind != APP_AUDIO_CATALOG_SOURCE_EXTERNAL))
  {
    return TX_PTR_ERROR;
  }

  if ((voice->data_size_bytes == 0UL) || (voice->block_align <= 4U))
  {
    return TX_NOT_DONE;
  }

  if (source_kind == APP_AUDIO_CATALOG_SOURCE_EMBEDDED)
  {
    if (voice->embedded_data == (const uint8_t *)0)
    {
      return TX_NOT_DONE;
    }
    data = voice->embedded_data;
    data_size = voice->data_size_bytes;
  }

  block_align = (ULONG)voice->block_align;
  block_payload_bytes = block_align - 4UL;
  if (block_align > APP_AUDIO_EXTERNAL_BLOCK_MAX_BYTES)
  {
    return TX_NOT_DONE;
  }

  if ((source_kind == APP_AUDIO_CATALOG_SOURCE_EXTERNAL) &&
      (voice->external_prefetch_pending != 0U))
  {
    UINT prefetch_status = AppAudioStoragePrefetchExternalBlock(voice,
                                                                voice->external_prefetch_offset,
                                                                voice->external_prefetch_len);
    if ((prefetch_status != TX_SUCCESS) &&
        (prefetch_status != TX_NO_EVENTS) &&
        (prefetch_status != TX_NOT_DONE))
    {
      return prefetch_status;
    }
  }

  if (decode->block_loaded == 0UL)
  {
    ULONG block_header_ix = decode->data_offset;

    if ((decode->data_offset + block_align) > voice->data_size_bytes)
    {
      return TX_NOT_DONE;
    }

    if (source_kind == APP_AUDIO_CATALOG_SOURCE_EXTERNAL)
    {
      if ((voice->external_prefetch_ready != 0U) &&
          (voice->external_prefetch_offset == decode->data_offset) &&
          (voice->external_prefetch_len == block_align))
      {
        (void)memcpy(voice->external_block_buf, voice->external_prefetch_buf, block_align);
        voice->external_prefetch_ready = 0U;
      }
      else
      {
        UINT st = AppAudioStorageReadExternalBlock(voice, decode->data_offset, block_align);
        if (st != TX_SUCCESS)
        {
          return st;
        }
      }
      data = voice->external_block_buf;
      data_size = block_align;
      decode->block_cursor = 4UL;
      decode->data_offset += block_align;
      block_header_ix = 0UL;
    }
    else
    {
      decode->block_cursor = decode->data_offset + 4UL;
      decode->data_offset += block_align;
      block_header_ix = decode->block_cursor - 4UL;
    }

    decode->predictor =
      (int32_t)(int16_t)((uint16_t)data[block_header_ix] |
                         ((uint16_t)data[block_header_ix + 1UL] << 8));
    decode->index = (int32_t)data[block_header_ix + 2UL];
    if (decode->index < 0L)
    {
      decode->index = 0L;
    }
    else if (decode->index > 88L)
    {
      decode->index = 88L;
    }
    decode->nibbles_left_in_block = block_payload_bytes * 2UL;
    decode->have_high_nibble = 0U;
    decode->block_loaded = 1UL;
    decode->emit_block_header = 1UL;

    if ((source_kind == APP_AUDIO_CATALOG_SOURCE_EXTERNAL) &&
        ((decode->data_offset + block_align) <= voice->data_size_bytes))
    {
      UINT prefetch_status = AppAudioStoragePrefetchExternalBlock(voice, decode->data_offset, block_align);
      if ((prefetch_status != TX_SUCCESS) &&
          (prefetch_status != TX_NO_EVENTS) &&
          (prefetch_status != TX_NOT_DONE))
      {
        return prefetch_status;
      }
    }
  }

  if (decode->emit_block_header != 0UL)
  {
    decode->emit_block_header = 0UL;
    *sample_out = (int16_t)decode->predictor;
    voice->sample_cursor++;
    return TX_SUCCESS;
  }

  if (decode->nibbles_left_in_block == 0UL)
  {
    decode->block_loaded = 0UL;
    return AppAudioAdpcmDecodeNext(voice, sample_out);
  }

  if (decode->have_high_nibble != 0U)
  {
    nibble = (uint8_t)((decode->byte_cache >> 4) & 0x0FU);
    decode->have_high_nibble = 0U;
  }
  else
  {
    if (source_kind == APP_AUDIO_CATALOG_SOURCE_EXTERNAL)
    {
      data = voice->external_block_buf;
      data_size = block_align;
    }

    if ((decode->block_cursor >= data_size) ||
        (decode->block_cursor < 4UL))
    {
      return TX_NOT_DONE;
    }
    decode->byte_cache = data[decode->block_cursor++];
    nibble = (uint8_t)(decode->byte_cache & 0x0FU);
    decode->have_high_nibble = 1U;
  }

  decode->nibbles_left_in_block--;
  *sample_out = AppAudioAdpcmDecodeNibble(decode, nibble);
  voice->sample_cursor++;
  return TX_SUCCESS;
}

static uint8_t AppAudioVoiceFetchSample(app_audio_voice_t *voice, int16_t *sample_out)
{
  UINT decode_status;

  if ((voice == (app_audio_voice_t *)0) || (sample_out == (int16_t *)0))
  {
    return 0U;
  }

  *sample_out = 0;
  if ((voice->active == 0U) ||
      (voice->sample_count == 0UL) ||
      (voice->source_kind == APP_AUDIO_CATALOG_SOURCE_NONE))
  {
    voice->active = 0U;
    return 0U;
  }

  if (voice->sample_cursor >= voice->sample_count)
  {
    if (voice->loop != 0U)
    {
      AppAudioVoiceDecodeReset(voice);
    }
    else
    {
      voice->active = 0U;
      return 0U;
    }
  }

  decode_status = AppAudioAdpcmDecodeNext(voice, sample_out);
  if (decode_status != TX_SUCCESS)
  {
    if ((voice->source_kind == APP_AUDIO_CATALOG_SOURCE_EXTERNAL) &&
        (decode_status == TX_NO_EVENTS))
    {
      /*
       * External chunk reads are asynchronous through the storage thread.
       * Keep the voice alive on transient misses and output silence until
       * the next block becomes available.
       */
      *sample_out = 0;
      return 1U;
    }

    if (voice->loop != 0U)
    {
      AppAudioVoiceDecodeReset(voice);
      if (AppAudioAdpcmDecodeNext(voice, sample_out) == TX_SUCCESS)
      {
        return 1U;
      }
    }

    voice->active = 0U;
    *sample_out = 0;
    return 0U;
  }

  return 1U;
}

static UINT AppAudioStartStream(void)
{
  HAL_StatusTypeDef hal_status;
  ULONG ignored_flags = 0UL;

  if (g_audio_state == APP_AUDIO_STATE_ACTIVE)
  {
    return TX_SUCCESS;
  }

  g_audio_dma_events = 0UL;
  g_audio_dma_half_pending = 0UL;
  g_audio_dma_full_pending = 0UL;
  g_audio_dma_error_pending = 0UL;
  (void)tx_event_flags_get(&g_eg_audio_dma, APP_AUDIO_DMA_EVENT_MASK, TX_OR_CLEAR, &ignored_flags, TX_NO_WAIT);
  AppAudioFillFrames(0UL, KNOB_AUDIO_DMA_FRAMES);
  /* Demand-power SAI path: enable peripheral clock only while audio is active. */
  __HAL_RCC_SAI1_CLK_ENABLE();
  __HAL_SAI_ENABLE(&hsai_BlockA1);
  HAL_GPIO_WritePin(SD_MODE_GPIO_Port, SD_MODE_Pin, GPIO_PIN_RESET);
  (void)HAL_SAI_DMAStop(&hsai_BlockA1);

  hal_status = HAL_SAI_Transmit_DMA(&hsai_BlockA1,
                                    (uint8_t *)g_audio_dma_buffer,
                                    (uint16_t)APP_AUDIO_DMA_SAMPLE_COUNT);
  g_audio_last_error = (LONG)hal_status;
  if (hal_status != HAL_OK)
  {
    g_audio_underflow_count++;
    g_audio_state = APP_AUDIO_STATE_STOPPED;
    HAL_GPIO_WritePin(SD_MODE_GPIO_Port, SD_MODE_Pin, GPIO_PIN_RESET);
    __HAL_SAI_DISABLE(&hsai_BlockA1);
    __HAL_RCC_SAI1_CLK_DISABLE();
    return TX_NOT_DONE;
  }

  HAL_GPIO_WritePin(SD_MODE_GPIO_Port, SD_MODE_Pin, GPIO_PIN_SET);
  g_audio_state = APP_AUDIO_STATE_ACTIVE;
  g_audio_start_count++;
  return TX_SUCCESS;
}

static UINT AppAudioStartTone(void)
{
  if ((g_audio_state == APP_AUDIO_STATE_ACTIVE) && (g_audio_source_kind == APP_AUDIO_SOURCE_TONE))
  {
    return TX_SUCCESS;
  }

  (void)AppAudioStop();
  g_audio_phase_accum = 0UL;
  g_audio_phase_step = (uint32_t)(((uint64_t)KNOB_AUDIO_TEST_TONE_HZ << 32) / (uint64_t)KNOB_AUDIO_SAMPLE_RATE);
  g_audio_source_kind = APP_AUDIO_SOURCE_TONE;
  g_audio_sfx_autostop_armed = 0UL;
  return AppAudioStartStream();
}

static UINT AppAudioStop(void)
{
  HAL_StatusTypeDef hal_status;
  ULONG ignored_flags = 0UL;

  /* Gate ISR callbacks before stopping DMA to prevent re-arm races. */
  g_audio_state = APP_AUDIO_STATE_STOPPED;
  g_audio_dma_events = 0UL;
  g_audio_dma_half_pending = 0UL;
  g_audio_dma_full_pending = 0UL;
  g_audio_dma_error_pending = 0UL;
  (void)tx_event_flags_get(&g_eg_audio_dma, APP_AUDIO_DMA_EVENT_MASK, TX_OR_CLEAR, &ignored_flags, TX_NO_WAIT);
  g_audio_sfx_autostop_armed = 0UL;
  g_audio_source_kind = APP_AUDIO_SOURCE_NONE;
  AppAudioVoiceReset(&g_audio_music_voice);
  (void)memset(g_audio_sfx_voices, 0, sizeof(g_audio_sfx_voices));
  /* Mute amp first to avoid audible tail/chirp while stopping DMA. */
  HAL_GPIO_WritePin(SD_MODE_GPIO_Port, SD_MODE_Pin, GPIO_PIN_RESET);
  hal_status = HAL_SAI_DMAStop(&hsai_BlockA1);
  if (hal_status == HAL_BUSY)
  {
    hal_status = HAL_OK;
  }
  __HAL_SAI_DISABLE(&hsai_BlockA1);
  __HAL_RCC_SAI1_CLK_DISABLE();
  g_audio_last_error = (LONG)hal_status;
  g_audio_stop_count++;
  return (hal_status == HAL_OK) ? TX_SUCCESS : TX_NOT_DONE;
}

static UINT AppAudioCatalogResolveExternal(app_audio_asset_id_t asset_id, app_audio_catalog_entry_t *entry_out)
{
  ULONG i;

  if (entry_out == (app_audio_catalog_entry_t *)0)
  {
    return TX_NOT_DONE;
  }

  if ((g_storage_audio_catalog_loaded == 0UL) ||
      (g_storage_audio_catalog_entry_count == 0UL))
  {
    return TX_NOT_DONE;
  }

  for (i = 0UL; i < g_storage_audio_catalog_entry_count; i++)
  {
    const app_storage_audio_catalog_entry_t *staged = &g_storage_audio_catalog_entries[i];
    if ((app_audio_asset_id_t)staged->asset_id != asset_id)
    {
      continue;
    }

    /* Metadata resolve only at this stage; chunk-backed decode is added later. */
    if ((staged->data_size == 0UL) ||
        (staged->sample_rate_hz == 0UL) ||
        (staged->block_align <= 4U) ||
        (staged->samples_per_block == 0U) ||
        (staged->total_samples == 0UL))
    {
      return TX_NOT_DONE;
    }

    entry_out->asset_id = asset_id;
    entry_out->source_kind = APP_AUDIO_CATALOG_SOURCE_EXTERNAL;
    entry_out->clip = (const app_audio_adpcm_clip_t *)0;
    entry_out->data_offset = staged->data_offset;
    entry_out->data_size = staged->data_size;
    entry_out->sample_rate_hz = staged->sample_rate_hz;
    entry_out->block_align = staged->block_align;
    entry_out->samples_per_block = staged->samples_per_block;
    entry_out->total_samples = staged->total_samples;
    return TX_SUCCESS;
  }

  entry_out->asset_id = APP_AUDIO_ASSET_NONE;
  entry_out->source_kind = APP_AUDIO_CATALOG_SOURCE_NONE;
  entry_out->clip = (const app_audio_adpcm_clip_t *)0;
  entry_out->data_offset = 0UL;
  entry_out->data_size = 0UL;
  entry_out->sample_rate_hz = 0UL;
  entry_out->block_align = 0U;
  entry_out->samples_per_block = 0U;
  entry_out->total_samples = 0UL;
  return TX_NOT_DONE;
}

static UINT AppAudioCatalogResolve(app_audio_asset_id_t asset_id, app_audio_catalog_entry_t *entry_out)
{
  if (entry_out == (app_audio_catalog_entry_t *)0)
  {
    return TX_NOT_DONE;
  }

  entry_out->asset_id = APP_AUDIO_ASSET_NONE;
  entry_out->source_kind = APP_AUDIO_CATALOG_SOURCE_NONE;
  entry_out->clip = (const app_audio_adpcm_clip_t *)0;
  entry_out->data_offset = 0UL;
  entry_out->data_size = 0UL;
  entry_out->sample_rate_hz = 0UL;
  entry_out->block_align = 0U;
  entry_out->samples_per_block = 0U;
  entry_out->total_samples = 0UL;

  if (asset_id == APP_AUDIO_ASSET_NONE)
  {
    return TX_NOT_DONE;
  }

  if (AppAudioCatalogResolveExternal(asset_id, entry_out) == TX_SUCCESS)
  {
    return TX_SUCCESS;
  }

  if (AppAudioAssets_IsValidId((uint32_t)asset_id) != 0U)
  {
    const app_audio_adpcm_clip_t *clip = AppAudioAssets_GetClip((uint32_t)asset_id);
    if (clip != (const app_audio_adpcm_clip_t *)0)
    {
      entry_out->asset_id = asset_id;
      entry_out->source_kind = APP_AUDIO_CATALOG_SOURCE_EMBEDDED;
      entry_out->clip = clip;
      entry_out->data_offset = 0UL;
      entry_out->data_size = clip->data_size;
      entry_out->sample_rate_hz = clip->sample_rate_hz;
      entry_out->block_align = clip->block_align;
      entry_out->samples_per_block = clip->samples_per_block;
      entry_out->total_samples = clip->total_samples;
      return TX_SUCCESS;
    }
  }
  return TX_NOT_DONE;
}

static app_audio_asset_id_t AppAudioResolveEventAsset(app_audio_event_t event_id)
{
  app_audio_asset_id_t asset_id = APP_AUDIO_ASSET_NONE;
  app_audio_catalog_entry_t entry;

  switch (event_id)
  {
    case APP_AUDIO_EVENT_UI_NAV:
      asset_id = (app_audio_asset_id_t)KNOB_AUDIO_MAP_UI_NAV_CLIP;
      break;

    case APP_AUDIO_EVENT_UI_CONFIRM:
      asset_id = (app_audio_asset_id_t)KNOB_AUDIO_MAP_UI_CONFIRM_CLIP;
      break;

    case APP_AUDIO_EVENT_UI_CANCEL:
      asset_id = (app_audio_asset_id_t)KNOB_AUDIO_MAP_UI_CANCEL_CLIP;
      break;

    case APP_AUDIO_EVENT_UI_DENIED:
      asset_id = (app_audio_asset_id_t)KNOB_AUDIO_MAP_UI_DENIED_CLIP;
      break;

    case APP_AUDIO_EVENT_GAME_ACTION:
      asset_id = (app_audio_asset_id_t)KNOB_AUDIO_MAP_GAME_ACTION_CLIP;
      break;

    case APP_AUDIO_EVENT_RT_MOVE:
      asset_id = (app_audio_asset_id_t)KNOB_AUDIO_MAP_RT_MOVE_CLIP;
      break;

    case APP_AUDIO_EVENT_RT_CONFIRM:
      asset_id = (app_audio_asset_id_t)KNOB_AUDIO_MAP_RT_PRIMARY_CLIP;
      break;

    case APP_AUDIO_EVENT_RT_CANCEL:
      asset_id = (app_audio_asset_id_t)KNOB_AUDIO_MAP_RT_SECONDARY_CLIP;
      break;

    case APP_AUDIO_EVENT_RT_MENU:
      asset_id = (app_audio_asset_id_t)KNOB_AUDIO_MAP_RT_BACK_CLIP;
      break;

    case APP_AUDIO_EVENT_NONE:
    default:
      asset_id = APP_AUDIO_ASSET_NONE;
      break;
  }

  if ((asset_id != APP_AUDIO_ASSET_NONE) &&
      (AppAudioCatalogResolve(asset_id, &entry) != TX_SUCCESS))
  {
    asset_id = APP_AUDIO_ASSET_NONE;
  }
  return asset_id;
}

static app_audio_gain_class_t AppAudioResolveEventGainClass(app_audio_event_t event_id)
{
  switch (event_id)
  {
    case APP_AUDIO_EVENT_UI_NAV:
    case APP_AUDIO_EVENT_UI_CONFIRM:
    case APP_AUDIO_EVENT_UI_CANCEL:
    case APP_AUDIO_EVENT_UI_DENIED:
      return APP_AUDIO_GAIN_CLASS_UI;

    case APP_AUDIO_EVENT_GAME_ACTION:
    case APP_AUDIO_EVENT_RT_MOVE:
    case APP_AUDIO_EVENT_RT_CONFIRM:
    case APP_AUDIO_EVENT_RT_CANCEL:
    case APP_AUDIO_EVENT_RT_MENU:
    case APP_AUDIO_EVENT_NONE:
    default:
      return APP_AUDIO_GAIN_CLASS_SFX;
  }
}

static UINT AppAudioPlayAsset(app_audio_asset_id_t asset_id)
{
  const app_audio_adpcm_clip_t *clip;
  app_audio_catalog_entry_t entry;
  ULONG mode_flags;
  ULONG i;
  app_audio_voice_t *voice_slot = (app_audio_voice_t *)0;
  UINT status;

  g_audio_last_clip = (ULONG)asset_id;
  if (g_audio_play_clip_count < 0xFFFFFFFFUL)
  {
    g_audio_play_clip_count++;
  }

  if (asset_id == APP_AUDIO_ASSET_NONE)
  {
    g_audio_sfx_autostop_armed = 0UL;
    return AppAudioStop();
  }

  if (AppAudioCatalogResolve(asset_id, &entry) != TX_SUCCESS)
  {
    g_audio_last_error = -401L;
    return TX_NOT_DONE;
  }

  if ((entry.data_size == 0UL) ||
      (entry.block_align <= 4U) ||
      (entry.samples_per_block == 0U) ||
      (entry.total_samples == 0UL))
  {
    g_audio_last_error = -401L;
    return TX_NOT_DONE;
  }

  if (entry.source_kind == APP_AUDIO_CATALOG_SOURCE_EMBEDDED)
  {
    clip = entry.clip;
    if ((clip == (const app_audio_adpcm_clip_t *)0) ||
        (clip->data == (const uint8_t *)0))
    {
      g_audio_last_error = -401L;
      return TX_NOT_DONE;
    }
  }
  else if (entry.source_kind == APP_AUDIO_CATALOG_SOURCE_EXTERNAL)
  {
    clip = (const app_audio_adpcm_clip_t *)0;
    if ((entry.block_align > APP_AUDIO_EXTERNAL_BLOCK_MAX_BYTES) ||
        ((entry.data_offset + entry.data_size) > APP_STORAGE_FLASH_SIZE_BYTES))
    {
      g_audio_last_error = -404L;
      return TX_NOT_DONE;
    }
  }
  else
  {
    g_audio_last_error = -403L;
    return TX_NOT_DONE;
  }

  if (entry.sample_rate_hz != (uint32_t)KNOB_AUDIO_SAMPLE_RATE)
  {
    g_audio_last_error = -402L;
    return TX_NOT_DONE;
  }

  if ((g_audio_state == APP_AUDIO_STATE_ACTIVE) && (g_audio_source_kind == APP_AUDIO_SOURCE_TONE))
  {
    (void)AppAudioStop();
  }
  else if ((g_audio_state == APP_AUDIO_STATE_ACTIVE) &&
           (hsai_BlockA1.State != HAL_SAI_STATE_BUSY_TX) &&
           (hsai_BlockA1.State != HAL_SAI_STATE_BUSY))
  {
    /* Recover stale active state after STOP wake by forcing a clean restart. */
    (void)AppAudioStop();
  }

  mode_flags = (g_eg_mode.tx_event_flags_group_current & APP_MODE_FLAGS_ALL);
  g_audio_source_kind = APP_AUDIO_SOURCE_CLIP;
  if (asset_id == (app_audio_asset_id_t)KNOB_AUDIO_MUSIC_LOOP_CLIP)
  {
    g_audio_music_voice.gain_class = (uint8_t)APP_AUDIO_GAIN_CLASS_MUSIC;
    if (entry.source_kind == APP_AUDIO_CATALOG_SOURCE_EXTERNAL)
    {
      AppAudioVoiceStartExternal(&g_audio_music_voice, &entry, 1U);
    }
    else
    {
      AppAudioVoiceStart(&g_audio_music_voice, clip, 1U);
    }
  }
  else
  {
    ULONG oldest_seq = 0xFFFFFFFFUL;
    app_audio_voice_t *oldest_slot = (app_audio_voice_t *)0;

    if ((mode_flags & APP_MODE_FLAG_REALTIME) == 0UL)
    {
      /* Keep UI/system audio crisp: replace SFX in non-REALTIME modes. */
      (void)memset(g_audio_sfx_voices, 0, sizeof(g_audio_sfx_voices));
    }

    for (i = 0UL; i < (ULONG)KNOB_AUDIO_SFX_VOICE_COUNT; i++)
    {
      if (g_audio_sfx_voices[i].active == 0U)
      {
        voice_slot = &g_audio_sfx_voices[i];
        break;
      }

      if (g_audio_sfx_voices[i].start_seq < oldest_seq)
      {
        oldest_seq = g_audio_sfx_voices[i].start_seq;
        oldest_slot = &g_audio_sfx_voices[i];
      }
    }

    if (voice_slot == (app_audio_voice_t *)0)
    {
      voice_slot = oldest_slot;
      if (g_audio_sfx_voice_steal_count < 0xFFFFFFFFUL)
      {
        g_audio_sfx_voice_steal_count++;
      }
    }

    if (voice_slot != (app_audio_voice_t *)0)
    {
      ULONG gain_class = g_audio_next_sfx_gain_class;
      if (gain_class > (ULONG)APP_AUDIO_GAIN_CLASS_UI)
      {
        gain_class = (ULONG)APP_AUDIO_GAIN_CLASS_SFX;
      }
      voice_slot->gain_class = (uint8_t)gain_class;
      if (entry.source_kind == APP_AUDIO_CATALOG_SOURCE_EXTERNAL)
      {
        AppAudioVoiceStartExternal(voice_slot, &entry, 0U);
      }
      else
      {
        AppAudioVoiceStart(voice_slot, clip, 0U);
      }
    }
  }

  {
    ULONG active_sfx = AppAudioSfxActiveCount();
    if (active_sfx > g_audio_sfx_voice_peak_active)
    {
      g_audio_sfx_voice_peak_active = active_sfx;
    }
  }

  g_audio_sfx_autostop_armed = 0UL;
  status = AppAudioStartStream();
  g_audio_next_sfx_gain_class = (ULONG)APP_AUDIO_GAIN_CLASS_SFX;

  return status;
}

static VOID AppAudioProcessAutoStop(void)
{
  ULONG now_tick;

  if ((g_audio_state != APP_AUDIO_STATE_ACTIVE) || (g_audio_sfx_autostop_armed == 0UL))
  {
    return;
  }

  now_tick = tx_time_get();
  if ((LONG)(now_tick - g_audio_sfx_autostop_tick) >= 0L)
  {
    g_audio_sfx_autostop_armed = 0UL;
    (void)AppAudioStop();
  }
}

static VOID AppAudioProcessDmaEvents(void)
{
  ULONG half_pending;
  ULONG full_pending;
  ULONG error_pending;
  const ULONG half_frames = (ULONG)(KNOB_AUDIO_DMA_FRAMES / 2U);

  if (g_audio_state != APP_AUDIO_STATE_ACTIVE)
  {
    g_audio_dma_events = 0UL;
    g_audio_dma_half_pending = 0UL;
    g_audio_dma_full_pending = 0UL;
    g_audio_dma_error_pending = 0UL;
    return;
  }

  __disable_irq();
  half_pending = g_audio_dma_half_pending;
  full_pending = g_audio_dma_full_pending;
  error_pending = g_audio_dma_error_pending;
  g_audio_dma_half_pending = 0UL;
  g_audio_dma_full_pending = 0UL;
  g_audio_dma_error_pending = 0UL;
  g_audio_dma_events &= ~APP_AUDIO_DMA_EVENT_MASK;
  __enable_irq();

  if (error_pending > 1UL)
  {
    g_audio_error_missed_count += (error_pending - 1UL);
  }
  if (half_pending > 1UL)
  {
    g_audio_half_missed_count += (half_pending - 1UL);
  }
  if (full_pending > 1UL)
  {
    g_audio_full_missed_count += (full_pending - 1UL);
  }

  if (error_pending != 0UL)
  {
    g_audio_underflow_count++;
    (void)AppAudioStop();
    return;
  }

  if (half_pending != 0UL)
  {
    AppAudioFillFrames(0UL, half_frames);
  }

  if (full_pending != 0UL)
  {
    AppAudioFillFrames(half_frames, half_frames);
  }

  if ((g_audio_source_kind == APP_AUDIO_SOURCE_CLIP) &&
      (AppAudioAnyVoiceActive() == 0U))
  {
    if (g_audio_sfx_autostop_armed == 0UL)
    {
      g_audio_sfx_autostop_armed = 1UL;
      g_audio_sfx_autostop_tick = tx_time_get() + 2UL;
    }
  }
}

static UINT AppSensorHealthFlagsWrite(ULONG set_mask, ULONG clear_mask)
{
  UINT status;

  if (clear_mask != 0UL)
  {
    status = tx_event_flags_set(&g_eg_sensor_health, ~clear_mask, TX_AND);
    if (status != TX_SUCCESS)
    {
      return status;
    }
  }

  if (set_mask != 0UL)
  {
    return tx_event_flags_set(&g_eg_sensor_health, set_mask, TX_OR);
  }

  return TX_SUCCESS;
}

static UINT AppSensorHealthFlagsPublish(void)
{
  ULONG set_mask = 0UL;

  if (g_sensor_pmic.state == (ULONG)APP_SENSOR_STATE_READY)
  {
    set_mask |= APP_SENSOR_HEALTH_PMIC_READY;
  }
  else if (g_sensor_pmic.state == (ULONG)APP_SENSOR_STATE_FAULT)
  {
    set_mask |= APP_SENSOR_HEALTH_PMIC_FAULT;
  }
  else if (g_sensor_pmic.state == (ULONG)APP_SENSOR_STATE_SUSPENDED)
  {
    set_mask |= APP_SENSOR_HEALTH_PMIC_SUSPENDED;
  }

  if (g_sensor_tmag.state == (ULONG)APP_SENSOR_STATE_READY)
  {
    set_mask |= APP_SENSOR_HEALTH_TMAG_READY;
  }
  else if (g_sensor_tmag.state == (ULONG)APP_SENSOR_STATE_FAULT)
  {
    set_mask |= APP_SENSOR_HEALTH_TMAG_FAULT;
  }
  else if (g_sensor_tmag.state == (ULONG)APP_SENSOR_STATE_SUSPENDED)
  {
    set_mask |= APP_SENSOR_HEALTH_TMAG_SUSPENDED;
  }

  if (g_sensor_lis.state == (ULONG)APP_SENSOR_STATE_READY)
  {
    set_mask |= APP_SENSOR_HEALTH_LIS_READY;
  }
  else if (g_sensor_lis.state == (ULONG)APP_SENSOR_STATE_FAULT)
  {
    set_mask |= APP_SENSOR_HEALTH_LIS_FAULT;
  }
  else if (g_sensor_lis.state == (ULONG)APP_SENSOR_STATE_SUSPENDED)
  {
    set_mask |= APP_SENSOR_HEALTH_LIS_SUSPENDED;
  }

  if (g_sensor_bus_fault != 0UL)
  {
    set_mask |= APP_SENSOR_HEALTH_BUS_FAULT;
  }

  return AppSensorHealthFlagsWrite(set_mask, APP_SENSOR_HEALTH_FLAGS_ALL);
}

static VOID AppSensorMarkAllOff(void)
{
  (void)memset(&g_sensor_pmic_live, 0, sizeof(g_sensor_pmic_live));
  g_sensor_pmic_live.guard_enabled = (KNOB_SENSOR_PMIC_GUARD_ENABLE != 0) ? 1UL : 0UL;
  g_sensor_pmic_live.cutoff_mv = (ULONG)KNOB_SENSOR_PMIC_CUTOFF_MV;
  g_sensor_pmic_live.cutoff_hys_mv = (ULONG)KNOB_SENSOR_PMIC_CUTOFF_HYS_MV;
  g_sensor_pmic_live.cutoff_confirm_samples = (ULONG)KNOB_SENSOR_PMIC_CUTOFF_CONFIRM_SAMPLES;
  (void)memset(&g_sensor_lis_live, 0, sizeof(g_sensor_lis_live));
  g_sensor_lis_profile_requested = APP_SENSOR_LIS_PROFILE_LOW_POWER;
  g_sensor_lis_profile_applied = APP_SENSOR_LIS_PROFILE_LOW_POWER;
  g_sensor_lis_stream_enabled = 0UL;
  g_sensor_lis_step_enabled_requested = 0UL;

  g_sensor_pmic.state = (ULONG)APP_SENSOR_STATE_OFF;
  g_sensor_pmic.fail_count = 0UL;
  g_sensor_pmic.recovery_attempts = 0UL;
  g_sensor_pmic.next_retry_tick = 0UL;
  g_sensor_pmic.last_success_tick = 0UL;
  g_sensor_pmic.last_error = 0L;

  g_sensor_tmag.state = (ULONG)APP_SENSOR_STATE_OFF;
  g_sensor_tmag.fail_count = 0UL;
  g_sensor_tmag.recovery_attempts = 0UL;
  g_sensor_tmag.next_retry_tick = 0UL;
  g_sensor_tmag.last_success_tick = 0UL;
  g_sensor_tmag.last_error = 0L;

  g_sensor_lis.state = (ULONG)APP_SENSOR_STATE_OFF;
  g_sensor_lis.fail_count = 0UL;
  g_sensor_lis.recovery_attempts = 0UL;
  g_sensor_lis.next_retry_tick = 0UL;
  g_sensor_lis.last_success_tick = 0UL;
  g_sensor_lis.last_error = 0L;
}

static VOID AppSensorMarkAllSuspended(uint8_t keep_pmic_active)
{
  g_sensor_lis_stream_enabled = 0UL;

  if ((keep_pmic_active == 0U) && (g_sensor_pmic.state != (ULONG)APP_SENSOR_STATE_FAULT))
  {
    g_sensor_pmic.state = (ULONG)APP_SENSOR_STATE_SUSPENDED;
    g_sensor_pmic.recovery_attempts = 0UL;
    g_sensor_pmic.next_retry_tick = 0UL;
  }

  if (g_sensor_tmag.state != (ULONG)APP_SENSOR_STATE_FAULT)
  {
    g_sensor_tmag.state = (ULONG)APP_SENSOR_STATE_SUSPENDED;
    g_sensor_tmag.recovery_attempts = 0UL;
    g_sensor_tmag.next_retry_tick = 0UL;
  }

  if (g_sensor_lis.state != (ULONG)APP_SENSOR_STATE_FAULT)
  {
    g_sensor_lis.state = (ULONG)APP_SENSOR_STATE_SUSPENDED;
    g_sensor_lis.recovery_attempts = 0UL;
    g_sensor_lis.next_retry_tick = 0UL;
  }
}

static VOID AppSensorResetRecoveryState(app_sensor_fsm_t *dev)
{
  if (dev == TX_NULL)
  {
    return;
  }

  dev->recovery_attempts = 0UL;
  dev->next_retry_tick = 0UL;
}

static uint8_t AppSensorRetryDue(ULONG now_ticks, ULONG deadline_ticks)
{
  LONG delta = (LONG)(now_ticks - deadline_ticks);
  return (delta >= 0L) ? 1U : 0U;
}

static VOID AppSensorBusRecoverPulseDelay(void)
{
  __NOP();
  __NOP();
  __NOP();
  __NOP();
  __NOP();
  __NOP();
  __NOP();
  __NOP();
}

static uint8_t AppSensorBusRecover(LONG *error_out)
{
  GPIO_InitTypeDef gpio_init = {0};
  HAL_StatusTypeDef status;
  ULONG pulse_index;

  (void)HAL_I2C_DeInit(&hi2c3);
  __HAL_RCC_GPIOC_CLK_ENABLE();

  gpio_init.Pin = APP_SENSOR_I2C_SCL_PIN | APP_SENSOR_I2C_SDA_PIN;
  gpio_init.Mode = GPIO_MODE_OUTPUT_OD;
  gpio_init.Pull = GPIO_NOPULL;
  gpio_init.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(APP_SENSOR_I2C_GPIO_PORT, &gpio_init);

  HAL_GPIO_WritePin(APP_SENSOR_I2C_GPIO_PORT, APP_SENSOR_I2C_SCL_PIN, GPIO_PIN_SET);
  HAL_GPIO_WritePin(APP_SENSOR_I2C_GPIO_PORT, APP_SENSOR_I2C_SDA_PIN, GPIO_PIN_SET);
  AppSensorBusRecoverPulseDelay();

  if (HAL_GPIO_ReadPin(APP_SENSOR_I2C_GPIO_PORT, APP_SENSOR_I2C_SDA_PIN) == GPIO_PIN_RESET)
  {
    for (pulse_index = 0UL; pulse_index < KNOB_SENSOR_BUS_RECOVERY_SCL_PULSES; pulse_index++)
    {
      HAL_GPIO_WritePin(APP_SENSOR_I2C_GPIO_PORT, APP_SENSOR_I2C_SCL_PIN, GPIO_PIN_RESET);
      AppSensorBusRecoverPulseDelay();
      HAL_GPIO_WritePin(APP_SENSOR_I2C_GPIO_PORT, APP_SENSOR_I2C_SCL_PIN, GPIO_PIN_SET);
      AppSensorBusRecoverPulseDelay();

      if (HAL_GPIO_ReadPin(APP_SENSOR_I2C_GPIO_PORT, APP_SENSOR_I2C_SDA_PIN) == GPIO_PIN_SET)
      {
        break;
      }
    }
  }

  HAL_GPIO_WritePin(APP_SENSOR_I2C_GPIO_PORT, APP_SENSOR_I2C_SDA_PIN, GPIO_PIN_RESET);
  AppSensorBusRecoverPulseDelay();
  HAL_GPIO_WritePin(APP_SENSOR_I2C_GPIO_PORT, APP_SENSOR_I2C_SCL_PIN, GPIO_PIN_SET);
  AppSensorBusRecoverPulseDelay();
  HAL_GPIO_WritePin(APP_SENSOR_I2C_GPIO_PORT, APP_SENSOR_I2C_SDA_PIN, GPIO_PIN_SET);
  AppSensorBusRecoverPulseDelay();

  gpio_init.Mode = GPIO_MODE_AF_OD;
  gpio_init.Alternate = APP_SENSOR_I2C_AF;
  HAL_GPIO_Init(APP_SENSOR_I2C_GPIO_PORT, &gpio_init);

  status = HAL_I2C_Init(&hi2c3);
  if (status != HAL_OK)
  {
    if (error_out != TX_NULL)
    {
      *error_out = -401L;
    }
    return 0U;
  }

  status = HAL_I2CEx_ConfigAnalogFilter(&hi2c3, I2C_ANALOGFILTER_ENABLE);
  if (status != HAL_OK)
  {
    if (error_out != TX_NULL)
    {
      *error_out = -402L;
    }
    return 0U;
  }

  status = HAL_I2CEx_ConfigDigitalFilter(&hi2c3, 0U);
  if (status != HAL_OK)
  {
    if (error_out != TX_NULL)
    {
      *error_out = -403L;
    }
    return 0U;
  }

  return 1U;
}

static uint8_t AppSensorProbeWithRecovery(uint8_t (*probe_fn)(LONG *), LONG *error_out)
{
  LONG probe_error = 0L;
  LONG recover_error = 0L;

  if (probe_fn == 0)
  {
    if (error_out != TX_NULL)
    {
      *error_out = -404L;
    }
    return 0U;
  }

  if (probe_fn(&probe_error) != 0U)
  {
    if (error_out != TX_NULL)
    {
      *error_out = 0L;
    }
    return 1U;
  }

  if (AppSensorBusRecover(&recover_error) == 0U)
  {
    g_sensor_bus_fault = 1UL;
    if (error_out != TX_NULL)
    {
      *error_out = (recover_error != 0L) ? recover_error : probe_error;
    }
    return 0U;
  }

  if (probe_fn(&probe_error) != 0U)
  {
    if (error_out != TX_NULL)
    {
      *error_out = 0L;
    }
    return 1U;
  }

  if (error_out != TX_NULL)
  {
    *error_out = probe_error;
  }
  return 0U;
}

static uint8_t AppSensorBusSanityCheck(LONG *error_out)
{
  if (HAL_I2C_GetState(&hi2c3) == HAL_I2C_STATE_RESET)
  {
    if (error_out != TX_NULL)
    {
      *error_out = -100L;
    }
    return 0U;
  }

  if (HAL_I2C_IsDeviceReady(&hi2c3, ADP5360_I2C_ADDR_W, 1U, ADP5360_I2C_TIMEOUT_MS) == HAL_OK)
  {
    return 1U;
  }

  if (HAL_I2C_IsDeviceReady(&hi2c3, TMAG5273_I2C_ADDR_8B, 1U, ADP5360_I2C_TIMEOUT_MS) == HAL_OK)
  {
    return 1U;
  }

  if (HAL_I2C_IsDeviceReady(&hi2c3, LIS2DUX12_I2C_ADD_H, 1U, ADP5360_I2C_TIMEOUT_MS) == HAL_OK)
  {
    return 1U;
  }

  if (HAL_I2C_IsDeviceReady(&hi2c3, LIS2DUX12_I2C_ADD_L, 1U, ADP5360_I2C_TIMEOUT_MS) == HAL_OK)
  {
    return 1U;
  }

  if (error_out != TX_NULL)
  {
    *error_out = (LONG)hi2c3.ErrorCode;
  }

  return 0U;
}

static VOID AppSensorPmicPolicyRefresh(void)
{
  g_sensor_pmic_live.guard_enabled = (KNOB_SENSOR_PMIC_GUARD_ENABLE != 0) ? 1UL : 0UL;
  g_sensor_pmic_live.cutoff_mv = (ULONG)KNOB_SENSOR_PMIC_CUTOFF_MV;
  g_sensor_pmic_live.cutoff_hys_mv = (ULONG)KNOB_SENSOR_PMIC_CUTOFF_HYS_MV;
  g_sensor_pmic_live.cutoff_confirm_samples = (ULONG)KNOB_SENSOR_PMIC_CUTOFF_CONFIRM_SAMPLES;
}

static VOID AppSensorPmicUpdateBatteryHealth(ULONG vbat_mV, ULONG soc_pct)
{
  ULONG reason = 0UL;
  ULONG state = APP_PMIC_BAT_HEALTH_OK;
  ULONG prev_reason = g_sensor_pmic_live.battery_health_reason;
  ULONG crit_mv_threshold = g_sensor_pmic_live.cutoff_mv;
  ULONG crit_mv_hys = g_sensor_pmic_live.cutoff_hys_mv;

  if (prev_reason & APP_PMIC_BAT_REASON_CRIT_MV)
  {
    if (vbat_mV < (crit_mv_threshold + crit_mv_hys))
    {
      reason |= APP_PMIC_BAT_REASON_CRIT_MV;
    }
  }
  else if (vbat_mV <= crit_mv_threshold)
  {
    reason |= APP_PMIC_BAT_REASON_CRIT_MV;
  }

  if (prev_reason & APP_PMIC_BAT_REASON_WARN_MV)
  {
    if (vbat_mV < ((ULONG)KNOB_SENSOR_PMIC_WARN_MV + (ULONG)KNOB_SENSOR_PMIC_WARN_HYS_MV))
    {
      reason |= APP_PMIC_BAT_REASON_WARN_MV;
    }
  }
  else if (vbat_mV <= (ULONG)KNOB_SENSOR_PMIC_WARN_MV)
  {
    reason |= APP_PMIC_BAT_REASON_WARN_MV;
  }

  if (prev_reason & APP_PMIC_BAT_REASON_WARN_SOC)
  {
    if (soc_pct < ((ULONG)KNOB_SENSOR_PMIC_WARN_SOC_PCT + (ULONG)KNOB_SENSOR_PMIC_WARN_SOC_HYS_PCT))
    {
      reason |= APP_PMIC_BAT_REASON_WARN_SOC;
    }
  }
  else if (soc_pct <= (ULONG)KNOB_SENSOR_PMIC_WARN_SOC_PCT)
  {
    reason |= APP_PMIC_BAT_REASON_WARN_SOC;
  }

  if (prev_reason & APP_PMIC_BAT_REASON_CRIT_SOC)
  {
    if (soc_pct < ((ULONG)KNOB_SENSOR_PMIC_CRIT_SOC_PCT + (ULONG)KNOB_SENSOR_PMIC_CRIT_SOC_HYS_PCT))
    {
      reason |= APP_PMIC_BAT_REASON_CRIT_SOC;
    }
  }
  else if (soc_pct <= (ULONG)KNOB_SENSOR_PMIC_CRIT_SOC_PCT)
  {
    reason |= APP_PMIC_BAT_REASON_CRIT_SOC;
  }

  if ((reason & (APP_PMIC_BAT_REASON_CRIT_MV | APP_PMIC_BAT_REASON_CRIT_SOC)) != 0UL)
  {
    state = APP_PMIC_BAT_HEALTH_CRIT;
  }
  else if ((reason & (APP_PMIC_BAT_REASON_WARN_MV | APP_PMIC_BAT_REASON_WARN_SOC)) != 0UL)
  {
    state = APP_PMIC_BAT_HEALTH_WARN;
  }

  g_sensor_pmic_live.battery_health_state = state;
  g_sensor_pmic_live.battery_health_reason = reason;
}

static VOID AppSensorPmicRuntimeReset(void)
{
  g_sensor_pmic_live.sample_count = 0UL;
  g_sensor_pmic_live.fail_count = 0UL;
  g_sensor_pmic_live.last_sample_tick = 0UL;
  g_sensor_pmic_live.last_error = 0L;
  g_sensor_pmic_live.last_transport_error = 0L;
  g_sensor_pmic_live.cutoff_low_streak = 0UL;
  g_sensor_pmic_live.cutoff_latched = 0UL;
  g_sensor_pmic_live.isofet_forced_off = 0UL;
  g_sensor_pmic_live.charging_enabled_cfg = 0UL;
  g_sensor_pmic_live.charging_active = 0UL;
  g_sensor_pmic_live.battery_soc_percent = 0UL;
  g_sensor_pmic_live.battery_soc_raw = 0UL;
  g_sensor_pmic_live.battery_health_state = APP_PMIC_BAT_HEALTH_UNKNOWN;
  g_sensor_pmic_live.battery_health_reason = 0UL;
  g_sensor_pmic_live.transport_error_count = 0UL;
  g_sensor_pmic_live.fault_event_count = 0UL;
  g_sensor_pmic_live.last_fault_mask = 0UL;
  g_sensor_pmic_live.status2_raw = 0UL;
  g_sensor_pmic_live.fault_raw = 0UL;
  g_sensor_pmic_live.pgood_raw = 0UL;
  g_sensor_pmic_live.vbus_present = 0UL;
  g_sensor_pmic_live.irq_flag1_raw = 0UL;
  g_sensor_pmic_live.irq_flag2_raw = 0UL;
  g_sensor_pmic_live.charger_state = 0UL;
  g_sensor_pmic_live.battery_uv = 0UL;
  g_sensor_pmic_live.battery_ov = 0UL;
  g_sensor_pmic_live.vbat_mV = 0UL;
  g_sensor_pmic_live.vbat_raw = 0UL;
  g_sensor_pmic_vbus_present = 0UL;
  g_sensor_pmic_vbus_known = 0UL;
  AppSensorPmicPolicyRefresh();
}

static VOID AppSensorPmicRecordTransportError(LONG error_code)
{
  g_sensor_pmic_live.transport_error_count++;
  g_sensor_pmic_live.fail_count++;
  g_sensor_pmic_live.last_transport_error = error_code;
  g_sensor_pmic_live.last_error = error_code;
}

static VOID AppSensorPmicRecordFaultMask(uint8_t fault_mask)
{
  g_sensor_pmic_live.fault_raw = (ULONG)fault_mask;
  g_sensor_pmic_live.last_fault_mask = (ULONG)fault_mask;
  if (fault_mask != 0U)
  {
    g_sensor_pmic_live.fault_event_count++;
  }
}

static VOID AppSensorPmicUpdateVbusPresence(uint8_t vbus_present, uint8_t force_post)
{
  ULONG present = (vbus_present != 0U) ? 1UL : 0UL;
  uint8_t should_post = 0U;

  g_sensor_pmic_live.vbus_present = present;
  if ((g_sensor_pmic_vbus_known == 0UL) ||
      (g_sensor_pmic_vbus_present != present) ||
      (force_post != 0U))
  {
    should_post = 1U;
  }

  g_sensor_pmic_vbus_present = present;
  g_sensor_pmic_vbus_known = 1UL;

  if (should_post != 0U)
  {
    (void)App_SysEvent_UsbVbusPresent(present);
  }
}

static VOID AppSensorPmicHandleIrq(void)
{
  uint8_t irq_flag1 = 0U;
  uint8_t irq_flag2 = 0U;
  uint8_t pgood_raw = 0U;
  HAL_StatusTypeDef status;
  uint8_t force_post = 0U;

  status = ADP5360_read_irq_flags(&irq_flag1, &irq_flag2);
  if (status != HAL_OK)
  {
    AppSensorPmicRecordTransportError(-124L);
    return;
  }

  g_sensor_pmic_live.irq_flag1_raw = (ULONG)irq_flag1;
  g_sensor_pmic_live.irq_flag2_raw = (ULONG)irq_flag2;

  status = ADP5360_get_pgood(TX_NULL, &pgood_raw);
  if (status != HAL_OK)
  {
    AppSensorPmicRecordTransportError(-125L);
    return;
  }

  g_sensor_pmic_live.pgood_raw = (ULONG)pgood_raw;
  force_post = ((irq_flag1 & ADP5360_IF_VBUS_INT) != 0U) ? 1U : 0U;
  AppSensorPmicUpdateVbusPresence(((pgood_raw & ADP5360_PG_VBUSOK) != 0U) ? 1U : 0U, force_post);
}

static uint8_t AppSensorPmicForceIsofetOff(LONG *error_out)
{
  ADP5360_func_t func = {0U};
  HAL_StatusTypeDef status;

  status = ADP5360_get_chg_function(&func);
  if (status != HAL_OK)
  {
    if (error_out != TX_NULL)
    {
      *error_out = -112L;
    }
    return 0U;
  }

  if (func.off_isofet == 0U)
  {
    func.off_isofet = 1U;
    status = ADP5360_set_chg_function(&func);
    if (status != HAL_OK)
    {
      if (error_out != TX_NULL)
      {
        *error_out = -113L;
      }
      return 0U;
    }

    status = ADP5360_get_chg_function(&func);
    if ((status != HAL_OK) || (func.off_isofet == 0U))
    {
      if (error_out != TX_NULL)
      {
        *error_out = -114L;
      }
      return 0U;
    }
  }

  g_sensor_pmic_live.isofet_forced_off = 1UL;
  g_sensor_pmic_live.cutoff_latched = 1UL;
  return 1U;
}

static uint8_t AppSensorPmicGuardApply(ULONG vbat_mV, LONG *error_out)
{
  ULONG cutoff_mv;
  ULONG cutoff_hys_mv;
  ULONG cutoff_confirm_samples;
  ULONG bat_status;

  AppSensorPmicPolicyRefresh();
  if (g_sensor_pmic_live.guard_enabled == 0UL)
  {
    g_sensor_pmic_live.cutoff_low_streak = 0UL;
    g_sensor_pmic_live.cutoff_latched = 0UL;
    return 1U;
  }

  cutoff_mv = g_sensor_pmic_live.cutoff_mv;
  cutoff_hys_mv = g_sensor_pmic_live.cutoff_hys_mv;
  cutoff_confirm_samples = g_sensor_pmic_live.cutoff_confirm_samples;
  if (cutoff_confirm_samples == 0UL)
  {
    cutoff_confirm_samples = 1UL;
  }

  /*
   * Guard startup hardening:
   * - wait until PMIC stream has at least confirm-window samples
   * - ignore known transitional charger/battery-detect states
   * - ignore obviously invalid VBAT reads
   *
   * This prevents false ISOFET-off latching from early boot transients.
   */
  bat_status = (g_sensor_pmic_live.status2_raw & 0x07UL);
  if ((g_sensor_pmic_live.sample_count < cutoff_confirm_samples) ||
      (g_sensor_pmic_live.charger_state == (ULONG)ADP5360_CHG_BATT_DETECT) ||
      (bat_status == (ULONG)ADP5360_BATSTAT_NO_BATT) ||
      (vbat_mV < 1800UL))
  {
    g_sensor_pmic_live.cutoff_low_streak = 0UL;
    return 1U;
  }

  if (vbat_mV <= cutoff_mv)
  {
    if (g_sensor_pmic_live.cutoff_low_streak < cutoff_confirm_samples)
    {
      g_sensor_pmic_live.cutoff_low_streak++;
    }
  }
  else if (vbat_mV >= (cutoff_mv + cutoff_hys_mv))
  {
    g_sensor_pmic_live.cutoff_low_streak = 0UL;
  }

  if ((g_sensor_pmic_live.cutoff_latched == 0UL) &&
      (g_sensor_pmic_live.cutoff_low_streak >= cutoff_confirm_samples))
  {
    if (AppSensorPmicForceIsofetOff(error_out) == 0U)
    {
      return 0U;
    }
  }
  else if ((g_sensor_pmic_live.cutoff_latched != 0UL) &&
           (g_sensor_pmic_live.isofet_forced_off == 0UL))
  {
    if (AppSensorPmicForceIsofetOff(error_out) == 0U)
    {
      return 0U;
    }
  }

  return 1U;
}

static uint8_t AppSensorProbePmic(LONG *error_out)
{
  ADP5360_id_t id = {0U};
  ADP5360_status1_t status1 = {0U};
  ADP5360_func_t func = {0U};
  ADP5360_irq_enable_t irq_enable = {0U};
  uint16_t vbat_mV = 0U;
  uint16_t vbat_raw = 0U;
  uint8_t pgood_raw = 0U;
  uint8_t soc_percent = 0U;
  uint8_t soc_raw = 0U;
  uint8_t irq_flag1 = 0U;
  uint8_t irq_flag2 = 0U;
  HAL_StatusTypeDef status;

  AppSensorPmicPolicyRefresh();

  status = ADP5360_init();
  if (status != HAL_OK)
  {
    AppSensorPmicRecordTransportError(-101L);
    if (error_out != TX_NULL)
    {
      *error_out = -101L;
    }
    return 0U;
  }

  status = ADP5360_get_id(&id);
  if (status != HAL_OK)
  {
    AppSensorPmicRecordTransportError(-102L);
    if (error_out != TX_NULL)
    {
      *error_out = -102L;
    }
    return 0U;
  }

  if ((id.manuf == 0U) && (id.model == 0U))
  {
    AppSensorPmicRecordTransportError(-101L);
    if (error_out != TX_NULL)
    {
      *error_out = -101L;
    }
    return 0U;
  }

  status = ADP5360_get_chg_function(&func);
  if (status != HAL_OK)
  {
    AppSensorPmicRecordTransportError(-103L);
    if (error_out != TX_NULL)
    {
      *error_out = -103L;
    }
    return 0U;
  }

  status = ADP5360_get_status1(&status1);
  if (status != HAL_OK)
  {
    AppSensorPmicRecordTransportError(-120L);
    if (error_out != TX_NULL)
    {
      *error_out = -120L;
    }
    return 0U;
  }

  status = ADP5360_get_irq_enable(&irq_enable, TX_NULL, TX_NULL);
  if (status != HAL_OK)
  {
    AppSensorPmicRecordTransportError(-121L);
    if (error_out != TX_NULL)
    {
      *error_out = -121L;
    }
    return 0U;
  }

  if (irq_enable.vbus == 0U)
  {
    irq_enable.vbus = 1U;
    status = ADP5360_set_irq_enable(&irq_enable);
    if (status != HAL_OK)
    {
      AppSensorPmicRecordTransportError(-122L);
      if (error_out != TX_NULL)
      {
        *error_out = -122L;
      }
      return 0U;
    }
  }

  status = ADP5360_read_irq_flags(&irq_flag1, &irq_flag2);
  if (status != HAL_OK)
  {
    AppSensorPmicRecordTransportError(-124L);
    if (error_out != TX_NULL)
    {
      *error_out = -124L;
    }
    return 0U;
  }

  g_sensor_pmic_live.irq_flag1_raw = (ULONG)irq_flag1;
  g_sensor_pmic_live.irq_flag2_raw = (ULONG)irq_flag2;

  status = ADP5360_get_vbat(&vbat_mV, &vbat_raw);
  if (status != HAL_OK)
  {
    AppSensorPmicRecordTransportError(-104L);
    if (error_out != TX_NULL)
    {
      *error_out = -104L;
    }
    return 0U;
  }

  status = ADP5360_get_pgood(TX_NULL, &pgood_raw);
  if (status != HAL_OK)
  {
    AppSensorPmicRecordTransportError(-125L);
    if (error_out != TX_NULL)
    {
      *error_out = -125L;
    }
    return 0U;
  }

  g_sensor_pmic_live.vbat_mV = (ULONG)vbat_mV;
  g_sensor_pmic_live.vbat_raw = (ULONG)vbat_raw;
  g_sensor_pmic_live.pgood_raw = (ULONG)pgood_raw;
  g_sensor_pmic_live.charger_state = (ULONG)status1.state;
  g_sensor_pmic_live.isofet_forced_off = (func.off_isofet != 0U) ? 1UL : 0UL;
  g_sensor_pmic_live.cutoff_latched = (g_sensor_pmic_live.isofet_forced_off != 0UL) ? 1UL : 0UL;
  g_sensor_pmic_live.charging_enabled_cfg = (func.en_chg != 0U) ? 1UL : 0UL;

  status = ADP5360_get_soc(&soc_percent, &soc_raw);
  if (status != HAL_OK)
  {
    AppSensorPmicRecordTransportError(-118L);
    if (error_out != TX_NULL)
    {
      *error_out = -118L;
    }
    return 0U;
  }
  g_sensor_pmic_live.battery_soc_percent = (ULONG)soc_percent;
  g_sensor_pmic_live.battery_soc_raw = (ULONG)soc_raw;
  g_sensor_pmic_live.charging_active = ((status1.state == ADP5360_CHG_TRICKLE) ||
                                        (status1.state == ADP5360_CHG_FAST_CC) ||
                                        (status1.state == ADP5360_CHG_FAST_CV)) ? 1UL : 0UL;
  AppSensorPmicUpdateVbusPresence(((pgood_raw & ADP5360_PG_VBUSOK) != 0U) ? 1U : 0U, 1U);
  AppSensorPmicUpdateBatteryHealth((ULONG)vbat_mV, (ULONG)soc_percent);

  g_sensor_pmic_live.last_error = 0L;

  return 1U;
}

static uint8_t AppSensorProbeTmag(LONG *error_out)
{
  uint8_t device_id = 0U;
  uint8_t sensor_config_1 = 0U;
  uint8_t device_config_2 = 0U;

  if (TMAG5273_read_reg(TMAG5273_REG_DEVICE_ID, &device_id) != 0)
  {
    if (error_out != TX_NULL)
    {
      *error_out = -201L;
    }
    return 0U;
  }

  if (device_id == 0U)
  {
    if (error_out != TX_NULL)
    {
      *error_out = -202L;
    }
    return 0U;
  }

  if (TMAG5273_set_magnetic_channels(TMAG5273_CH_XYZ) != 0)
  {
    if (error_out != TX_NULL)
    {
      *error_out = -203L;
    }
    return 0U;
  }

  if (TMAG5273_set_operating_mode(TMAG5273_MODE_CONTINUOUS) != 0)
  {
    if (error_out != TX_NULL)
    {
      *error_out = -204L;
    }
    return 0U;
  }

  if (TMAG5273_read_reg(TMAG5273_REG_SENSOR_CONFIG_1, &sensor_config_1) != 0)
  {
    if (error_out != TX_NULL)
    {
      *error_out = -205L;
    }
    return 0U;
  }

  if (TMAG5273_read_reg(TMAG5273_REG_DEVICE_CONFIG_2, &device_config_2) != 0)
  {
    if (error_out != TX_NULL)
    {
      *error_out = -206L;
    }
    return 0U;
  }

  if ((((sensor_config_1 >> TMAG5273_SENSOR_CONFIG_1_MAG_CH_EN_Pos) & 0x07U) != TMAG5273_CH_XYZ) ||
      ((device_config_2 & 0x03U) != (uint8_t)TMAG5273_MODE_CONTINUOUS))
  {
    if (error_out != TX_NULL)
    {
      *error_out = -207L;
    }
    return 0U;
  }

  return 1U;
}

static uint8_t AppSensorPollPmic(LONG *error_out)
{
  ADP5360_status1_t status1 = {0U};
  ADP5360_status2_t status2 = {0U};
  ADP5360_func_t func = {0U};
  uint16_t vbat_mV = 0U;
  uint16_t vbat_raw = 0U;
  uint8_t fault_raw = 0U;
  uint8_t pgood_raw = 0U;
  uint8_t soc_percent = 0U;
  uint8_t soc_raw = 0U;
  HAL_StatusTypeDef status;
  LONG guard_error = 0L;

  AppSensorPmicPolicyRefresh();

  status = ADP5360_get_vbat(&vbat_mV, &vbat_raw);
  if (status != HAL_OK)
  {
    AppSensorPmicRecordTransportError(-105L);
    if (error_out != TX_NULL)
    {
      *error_out = -105L;
    }
    return 0U;
  }

  status = ADP5360_get_status1(&status1);
  if (status != HAL_OK)
  {
    AppSensorPmicRecordTransportError(-106L);
    if (error_out != TX_NULL)
    {
      *error_out = -106L;
    }
    return 0U;
  }

  status = ADP5360_get_status2(&status2);
  if (status != HAL_OK)
  {
    AppSensorPmicRecordTransportError(-107L);
    if (error_out != TX_NULL)
    {
      *error_out = -107L;
    }
    return 0U;
  }

  status = ADP5360_get_fault(&fault_raw);
  if (status != HAL_OK)
  {
    AppSensorPmicRecordTransportError(-108L);
    if (error_out != TX_NULL)
    {
      *error_out = -108L;
    }
    return 0U;
  }

  status = ADP5360_get_pgood(TX_NULL, &pgood_raw);
  if (status != HAL_OK)
  {
    AppSensorPmicRecordTransportError(-109L);
    if (error_out != TX_NULL)
    {
      *error_out = -109L;
    }
    return 0U;
  }

  status = ADP5360_get_chg_function(&func);
  if (status != HAL_OK)
  {
    AppSensorPmicRecordTransportError(-110L);
    if (error_out != TX_NULL)
    {
      *error_out = -110L;
    }
    return 0U;
  }

  status = ADP5360_get_soc(&soc_percent, &soc_raw);
  if (status != HAL_OK)
  {
    AppSensorPmicRecordTransportError(-119L);
    if (error_out != TX_NULL)
    {
      *error_out = -119L;
    }
    return 0U;
  }

  g_sensor_pmic_live.sample_count++;
  g_sensor_pmic_live.last_sample_tick = (ULONG)HAL_GetTick();
  g_sensor_pmic_live.vbat_mV = (ULONG)vbat_mV;
  g_sensor_pmic_live.vbat_raw = (ULONG)vbat_raw;
  g_sensor_pmic_live.charger_state = (ULONG)status1.state;
  g_sensor_pmic_live.status2_raw = (((ULONG)status2.thr & 0x07UL) << 5) |
                                   (((ULONG)status2.bat_ov & 0x01UL) << 4) |
                                   (((ULONG)status2.bat_uv & 0x01UL) << 3) |
                                   ((ULONG)status2.bat_status & 0x07UL);
  AppSensorPmicRecordFaultMask(fault_raw);
  g_sensor_pmic_live.pgood_raw = (ULONG)pgood_raw;
  g_sensor_pmic_live.battery_uv = (ULONG)status2.bat_uv;
  g_sensor_pmic_live.battery_ov = (ULONG)status2.bat_ov;
  g_sensor_pmic_live.isofet_forced_off = (func.off_isofet != 0U) ? 1UL : 0UL;
  g_sensor_pmic_live.charging_enabled_cfg = (func.en_chg != 0U) ? 1UL : 0UL;
  g_sensor_pmic_live.charging_active = ((status1.state == ADP5360_CHG_TRICKLE) ||
                                        (status1.state == ADP5360_CHG_FAST_CC) ||
                                        (status1.state == ADP5360_CHG_FAST_CV)) ? 1UL : 0UL;
  AppSensorPmicUpdateVbusPresence(((pgood_raw & ADP5360_PG_VBUSOK) != 0U) ? 1U : 0U, 0U);
  g_sensor_pmic_live.battery_soc_percent = (ULONG)soc_percent;
  g_sensor_pmic_live.battery_soc_raw = (ULONG)soc_raw;
  AppSensorPmicUpdateBatteryHealth((ULONG)vbat_mV, (ULONG)soc_percent);
  g_sensor_pmic_live.last_error = 0L;

  if (AppSensorPmicGuardApply((ULONG)vbat_mV, &guard_error) == 0U)
  {
    AppSensorPmicRecordTransportError(guard_error);
    if (error_out != TX_NULL)
    {
      *error_out = guard_error;
    }
    return 0U;
  }

  return 1U;
}

static uint8_t AppSensorPollTmag(LONG *error_out)
{
  uint8_t device_status = 0U;
  float x_sample = 0.0f;
  float y_sample = 0.0f;

  if (TMAG5273_read_reg(TMAG5273_REG_DEVICE_STATUS, &device_status) != 0)
  {
    if (error_out != TX_NULL)
    {
      *error_out = -208L;
    }
    return 0U;
  }

  if (TMAG5273_read_mT(&x_sample, &y_sample, TX_NULL) != 0)
  {
    if (error_out != TX_NULL)
    {
      *error_out = -209L;
    }
    return 0U;
  }

  return 1U;
}

static VOID AppSensorTmagMarkRuntimeFault(LONG error_code)
{
  ULONG now_ticks = tx_time_get();

  if (g_sensor_tmag.state != (ULONG)APP_SENSOR_STATE_READY)
  {
    return;
  }

  g_sensor_tmag.fail_count++;
  g_sensor_tmag.last_error = error_code;
  g_sensor_tmag.recovery_attempts = 1UL;
  if (g_sensor_tmag.recovery_attempts >= KNOB_SENSOR_RECOVERY_MAX_ATTEMPTS)
  {
    g_sensor_tmag.state = (ULONG)APP_SENSOR_STATE_FAULT;
    g_sensor_tmag.next_retry_tick = now_ticks + KNOB_SENSOR_FAULT_RETRY_TICKS;
  }
  else
  {
    g_sensor_tmag.state = (ULONG)APP_SENSOR_STATE_RECOVERING;
    g_sensor_tmag.next_retry_tick = now_ticks + KNOB_SENSOR_RECOVERY_BACKOFF_TICKS;
  }
  g_sensor_bus_fault = 1UL;
  (void)AppSensorHealthFlagsPublish();
}

static int32_t AppSensorLisRead(void *ctx, uint8_t reg, uint8_t *data, uint16_t len)
{
  app_sensor_lis_ctx_t *lis_ctx = (app_sensor_lis_ctx_t *)ctx;

  if ((lis_ctx == TX_NULL) || (lis_ctx->hi2c == TX_NULL) || (data == TX_NULL) || (len == 0U))
  {
    return -1;
  }

  return (HAL_I2C_Mem_Read(lis_ctx->hi2c, lis_ctx->addr, reg, I2C_MEMADD_SIZE_8BIT, data, len, ADP5360_I2C_TIMEOUT_MS) == HAL_OK) ? 0 : -1;
}

static int32_t AppSensorLisWrite(void *ctx, uint8_t reg, const uint8_t *data, uint16_t len)
{
  app_sensor_lis_ctx_t *lis_ctx = (app_sensor_lis_ctx_t *)ctx;

  if ((lis_ctx == TX_NULL) || (lis_ctx->hi2c == TX_NULL) || (data == TX_NULL) || (len == 0U))
  {
    return -1;
  }

  return (HAL_I2C_Mem_Write(lis_ctx->hi2c, lis_ctx->addr, reg, I2C_MEMADD_SIZE_8BIT, (uint8_t *)data, len, ADP5360_I2C_TIMEOUT_MS) == HAL_OK) ? 0 : -1;
}

static uint8_t AppSensorLisResolveDevice(stmdev_ctx_t *driver_ctx, app_sensor_lis_ctx_t *lis_ctx, uint8_t *whoami_out)
{
  uint8_t whoami = 0U;

  if ((driver_ctx == TX_NULL) || (lis_ctx == TX_NULL) || (whoami_out == TX_NULL))
  {
    return 0U;
  }

  lis_ctx->addr = LIS2DUX12_I2C_ADD_H;
  if ((lis2dux12_device_id_get(driver_ctx, &whoami) == 0) && (whoami == LIS2DUX12_ID))
  {
    *whoami_out = whoami;
    return 1U;
  }

  lis_ctx->addr = LIS2DUX12_I2C_ADD_L;
  if ((lis2dux12_device_id_get(driver_ctx, &whoami) == 0) && (whoami == LIS2DUX12_ID))
  {
    *whoami_out = whoami;
    return 1U;
  }

  return 0U;
}

static uint8_t AppSensorLisOdrIsValid(uint8_t odr)
{
  switch (odr)
  {
    case LIS2DUX12_OFF:
    case LIS2DUX12_1Hz6_ULP:
    case LIS2DUX12_3Hz_ULP:
    case LIS2DUX12_25Hz_ULP:
    case LIS2DUX12_6Hz_LP:
    case LIS2DUX12_12Hz5_LP:
    case LIS2DUX12_25Hz_LP:
    case LIS2DUX12_50Hz_LP:
    case LIS2DUX12_100Hz_LP:
    case LIS2DUX12_200Hz_LP:
    case LIS2DUX12_400Hz_LP:
    case LIS2DUX12_800Hz_LP:
    case LIS2DUX12_6Hz_HP:
    case LIS2DUX12_12Hz5_HP:
    case LIS2DUX12_25Hz_HP:
    case LIS2DUX12_50Hz_HP:
    case LIS2DUX12_100Hz_HP:
    case LIS2DUX12_200Hz_HP:
    case LIS2DUX12_400Hz_HP:
    case LIS2DUX12_800Hz_HP:
    case LIS2DUX12_TRIG_PIN:
    case LIS2DUX12_TRIG_SW:
      return 1U;

    default:
      return 0U;
  }
}

static uint8_t AppSensorLisOdrSupportsBw(uint8_t odr)
{
  switch (odr)
  {
    case LIS2DUX12_6Hz_LP:
    case LIS2DUX12_12Hz5_LP:
    case LIS2DUX12_25Hz_LP:
    case LIS2DUX12_50Hz_LP:
    case LIS2DUX12_100Hz_LP:
    case LIS2DUX12_200Hz_LP:
    case LIS2DUX12_400Hz_LP:
    case LIS2DUX12_800Hz_LP:
    case LIS2DUX12_6Hz_HP:
    case LIS2DUX12_12Hz5_HP:
    case LIS2DUX12_25Hz_HP:
    case LIS2DUX12_50Hz_HP:
    case LIS2DUX12_100Hz_HP:
    case LIS2DUX12_200Hz_HP:
    case LIS2DUX12_400Hz_HP:
    case LIS2DUX12_800Hz_HP:
      return 1U;

    default:
      return 0U;
  }
}

static uint8_t AppSensorLisResolveOdrKnob(ULONG knob_value, uint8_t fallback_odr)
{
  uint8_t odr = (uint8_t)knob_value;

  return (AppSensorLisOdrIsValid(odr) != 0U) ? odr : fallback_odr;
}

static uint8_t AppSensorLisResolveBwKnob(ULONG knob_value, uint8_t fallback_bw)
{
  uint8_t bw = (uint8_t)knob_value;

  switch (bw)
  {
    case LIS2DUX12_ODR_div_2:
    case LIS2DUX12_ODR_div_4:
    case LIS2DUX12_ODR_div_8:
    case LIS2DUX12_ODR_div_16:
      return bw;

    default:
      return fallback_bw;
  }
}

static uint8_t AppSensorLisResolveFsKnob(ULONG knob_value, uint8_t fallback_fs)
{
  uint8_t fs = (uint8_t)knob_value;

  switch (fs)
  {
    case LIS2DUX12_2g:
    case LIS2DUX12_4g:
    case LIS2DUX12_8g:
    case LIS2DUX12_16g:
      return fs;

    default:
      return fallback_fs;
  }
}

static uint8_t AppSensorLisApplyProfile(const stmdev_ctx_t *driver_ctx, app_sensor_lis_profile_t profile, LONG *error_out)
{
  lis2dux12_md_t md = {0};
  lis2dux12_md_t verify = {0};
  uint8_t step_path_active = 0U;
  uint8_t fs_cfg = AppSensorLisResolveFsKnob((ULONG)KNOB_SENSOR_LIS_FS, LIS2DUX12_2g);
  uint8_t low_power_odr = AppSensorLisResolveOdrKnob((ULONG)KNOB_SENSOR_LIS_LOW_POWER_ODR, LIS2DUX12_25Hz_ULP);
  uint8_t low_power_bw = AppSensorLisResolveBwKnob((ULONG)KNOB_SENSOR_LIS_LOW_POWER_BW, LIS2DUX12_ODR_div_2);
  uint8_t low_power_step_odr = AppSensorLisResolveOdrKnob((ULONG)KNOB_SENSOR_LIS_LOW_POWER_STEP_ODR, LIS2DUX12_25Hz_LP);
  uint8_t low_power_step_bw = AppSensorLisResolveBwKnob((ULONG)KNOB_SENSOR_LIS_LOW_POWER_STEP_BW, LIS2DUX12_ODR_div_4);
  uint8_t live_odr = AppSensorLisResolveOdrKnob((ULONG)KNOB_SENSOR_LIS_LIVE_ODR, LIS2DUX12_100Hz_LP);
  uint8_t live_bw = AppSensorLisResolveBwKnob((ULONG)KNOB_SENSOR_LIS_LIVE_BW, LIS2DUX12_ODR_div_4);

  if (driver_ctx == TX_NULL)
  {
    if (error_out != TX_NULL)
    {
      *error_out = -308L;
    }
    return 0U;
  }

  switch (profile)
  {
    case APP_SENSOR_LIS_PROFILE_LOW_POWER:
      step_path_active = ((g_sensor_lis_step_enabled_requested != 0UL) ||
                          (g_sensor_lis_live.step_enabled != 0UL)) ? 1U : 0U;
      md.odr = (lis2dux12_odr_t)((step_path_active != 0U) ? low_power_step_odr : low_power_odr);
      md.fs = (lis2dux12_fs_t)fs_cfg;
      md.bw = (lis2dux12_bw_t)((step_path_active != 0U) ? low_power_step_bw : low_power_bw);
      break;

    case APP_SENSOR_LIS_PROFILE_LIVE:
      md.odr = (lis2dux12_odr_t)live_odr;
      md.fs = (lis2dux12_fs_t)fs_cfg;
      md.bw = (lis2dux12_bw_t)live_bw;
      break;

    default:
      if (error_out != TX_NULL)
      {
        *error_out = -311L;
      }
      return 0U;
  }

  if (lis2dux12_mode_set(driver_ctx, &md) != 0)
  {
    if (error_out != TX_NULL)
    {
      *error_out = -308L;
    }
    return 0U;
  }

  if (lis2dux12_mode_get(driver_ctx, &verify) != 0)
  {
    if (error_out != TX_NULL)
    {
      *error_out = -309L;
    }
    return 0U;
  }

  if ((verify.odr != md.odr) || (verify.fs != md.fs))
  {
    if (error_out != TX_NULL)
    {
      *error_out = -310L;
    }
    return 0U;
  }

  if ((AppSensorLisOdrSupportsBw((uint8_t)md.odr) != 0U) && (verify.bw != md.bw))
  {
    if (error_out != TX_NULL)
    {
      *error_out = -310L;
    }
    return 0U;
  }

  g_sensor_lis_profile_applied = profile;
  return 1U;
}

static uint8_t AppSensorLisApplyStepConfig(const stmdev_ctx_t *driver_ctx, ULONG step_enable, LONG *error_out)
{
  lis2dux12_stpcnt_mode_t mode = {0};
  lis2dux12_stpcnt_mode_t verify = {0};
  uint8_t want_enable = (step_enable != 0UL) ? 1U : 0U;

  if (driver_ctx == TX_NULL)
  {
    if (error_out != TX_NULL)
    {
      *error_out = -318L;
    }
    return 0U;
  }

  if (want_enable != 0U)
  {
    if (lis2dux12_embedded_state_set(driver_ctx, PROPERTY_ENABLE) != 0)
    {
      if (error_out != TX_NULL)
      {
        *error_out = -319L;
      }
      return 0U;
    }
  }

  mode.false_step_rej = (KNOB_SENSOR_LIS_STEP_FALSE_REJ_ENABLE != 0) ? PROPERTY_ENABLE : PROPERTY_DISABLE;
  mode.step_counter_enable = (want_enable != 0U) ? PROPERTY_ENABLE : PROPERTY_DISABLE;
  mode.step_counter_in_fifo = (KNOB_SENSOR_LIS_STEP_IN_FIFO_ENABLE != 0) ? PROPERTY_ENABLE : PROPERTY_DISABLE;
  if (lis2dux12_stpcnt_mode_set(driver_ctx, mode) != 0)
  {
    if (error_out != TX_NULL)
    {
      *error_out = -320L;
    }
    return 0U;
  }

  if (lis2dux12_stpcnt_mode_get(driver_ctx, &verify) != 0)
  {
    if (error_out != TX_NULL)
    {
      *error_out = -321L;
    }
    return 0U;
  }

  if (((verify.step_counter_enable != 0U) ? 1U : 0U) != want_enable)
  {
    if (error_out != TX_NULL)
    {
      *error_out = -322L;
    }
    return 0U;
  }

  g_sensor_lis_live.step_enabled = (want_enable != 0U) ? 1UL : 0UL;
  return 1U;
}

static VOID AppSensorLisMarkRuntimeFault(LONG error_code)
{
  ULONG now_ticks = tx_time_get();

  if (g_sensor_lis.state != (ULONG)APP_SENSOR_STATE_READY)
  {
    return;
  }

  g_sensor_lis.fail_count++;
  g_sensor_lis.last_error = error_code;
  g_sensor_lis.recovery_attempts = 1UL;
  if (g_sensor_lis.recovery_attempts >= KNOB_SENSOR_RECOVERY_MAX_ATTEMPTS)
  {
    g_sensor_lis.state = (ULONG)APP_SENSOR_STATE_FAULT;
    g_sensor_lis.next_retry_tick = now_ticks + KNOB_SENSOR_FAULT_RETRY_TICKS;
  }
  else
  {
    g_sensor_lis.state = (ULONG)APP_SENSOR_STATE_RECOVERING;
    g_sensor_lis.next_retry_tick = now_ticks + KNOB_SENSOR_RECOVERY_BACKOFF_TICKS;
  }
  g_sensor_bus_fault = 1UL;
  (void)AppSensorHealthFlagsPublish();
}

static VOID AppSensorSuspendHardwareForStop(void)
{
  app_sensor_lis_ctx_t lis_ctx = {0};
  stmdev_ctx_t driver_ctx = {0};
  lis2dux12_priv_t lis_priv = {0};
  lis2dux12_md_t md = {0};
  lis2dux12_md_t verify = {0};
  uint8_t whoami = 0U;

  if (HAL_I2C_GetState(&hi2c3) == HAL_I2C_STATE_RESET)
  {
    return;
  }

  if (g_sensor_tmag.state == (ULONG)APP_SENSOR_STATE_READY)
  {
    if (TMAG5273_set_operating_mode(TMAG5273_MODE_WAKE_SLEEP) != 0)
    {
      g_sensor_tmag.last_error = -210L;
    }
    else
    {
      g_sensor_tmag.last_error = 0L;
    }
  }

  if (g_sensor_lis.state != (ULONG)APP_SENSOR_STATE_READY)
  {
    return;
  }

  lis_ctx.hi2c = &hi2c3;
  lis_ctx.addr = LIS2DUX12_I2C_ADD_H;
  driver_ctx.read_reg = AppSensorLisRead;
  driver_ctx.write_reg = AppSensorLisWrite;
  driver_ctx.handle = &lis_ctx;
  driver_ctx.mdelay = 0;
  driver_ctx.priv_data = &lis_priv;

  if (AppSensorLisResolveDevice(&driver_ctx, &lis_ctx, &whoami) == 0U)
  {
    g_sensor_lis.last_error = -328L;
    return;
  }

  md.odr = LIS2DUX12_OFF;
  md.fs = (lis2dux12_fs_t)AppSensorLisResolveFsKnob((ULONG)KNOB_SENSOR_LIS_FS, LIS2DUX12_2g);
  md.bw = LIS2DUX12_ODR_div_2;

  if ((lis2dux12_mode_set(&driver_ctx, &md) != 0) ||
      (lis2dux12_mode_get(&driver_ctx, &verify) != 0) ||
      (verify.odr != LIS2DUX12_OFF))
  {
    g_sensor_lis.last_error = -329L;
    return;
  }

  g_sensor_lis_live.addr = (ULONG)lis_ctx.addr;
  g_sensor_lis_live.whoami = (ULONG)whoami;
  g_sensor_lis.last_error = 0L;
}

static VOID AppSensorLisApplyRequestedProfileNow(void)
{
  LONG error_code = 0L;
  app_sensor_lis_ctx_t lis_ctx = {0};
  stmdev_ctx_t driver_ctx = {0};
  lis2dux12_priv_t lis_priv = {0};
  uint8_t whoami = 0U;
  uint8_t step_cfg_needed = 0U;

  if (g_sensor_lis.state != (ULONG)APP_SENSOR_STATE_READY)
  {
    return;
  }

  if (AppSensorModeHasAny(APP_MODE_FLAG_FLASHING) != 0U)
  {
    return;
  }

  lis_ctx.hi2c = &hi2c3;
  lis_ctx.addr = LIS2DUX12_I2C_ADD_H;
  driver_ctx.read_reg = AppSensorLisRead;
  driver_ctx.write_reg = AppSensorLisWrite;
  driver_ctx.handle = &lis_ctx;
  driver_ctx.mdelay = 0;
  driver_ctx.priv_data = &lis_priv;

  if (AppSensorLisResolveDevice(&driver_ctx, &lis_ctx, &whoami) == 0U)
  {
    AppSensorLisMarkRuntimeFault(-313L);
    return;
  }

  if (AppSensorLisApplyProfile(&driver_ctx, g_sensor_lis_profile_requested, &error_code) == 0U)
  {
    AppSensorLisMarkRuntimeFault(error_code);
    return;
  }

  step_cfg_needed = ((g_sensor_lis_step_enabled_requested != 0UL) || (g_sensor_lis_live.step_enabled != 0UL)) ? 1U : 0U;
  if (step_cfg_needed != 0U)
  {
    if (AppSensorLisApplyStepConfig(&driver_ctx, g_sensor_lis_step_enabled_requested, &error_code) == 0U)
    {
      AppSensorLisMarkRuntimeFault(error_code);
      return;
    }
  }
  else
  {
    g_sensor_lis_live.step_enabled = 0UL;
  }

  if ((step_cfg_needed != 0U) &&
      (g_sensor_lis_step_enabled_requested == 0UL) &&
      (g_sensor_lis_profile_requested == APP_SENSOR_LIS_PROFILE_LOW_POWER))
  {
    if (AppSensorLisApplyProfile(&driver_ctx, APP_SENSOR_LIS_PROFILE_LOW_POWER, &error_code) == 0U)
    {
      AppSensorLisMarkRuntimeFault(error_code);
      return;
    }
  }

  g_sensor_lis_live.addr = (ULONG)lis_ctx.addr;
  g_sensor_lis_live.whoami = (ULONG)whoami;
  g_sensor_lis.last_error = 0L;
  g_sensor_bus_fault = 0UL;
  (void)AppSensorHealthFlagsPublish();
}

static VOID AppSensorLisResetStepCounterNow(void)
{
  LONG error_code = 0L;
  app_sensor_lis_ctx_t lis_ctx = {0};
  stmdev_ctx_t driver_ctx = {0};
  lis2dux12_priv_t lis_priv = {0};
  uint8_t whoami = 0U;
  uint16_t steps = 0U;

  if (g_sensor_lis.state != (ULONG)APP_SENSOR_STATE_READY)
  {
    return;
  }

  if (AppSensorModeHasAny(APP_MODE_FLAG_FLASHING) != 0U)
  {
    return;
  }

  if ((g_sensor_lis_step_enabled_requested == 0UL) && (g_sensor_lis_live.step_enabled == 0UL))
  {
    return;
  }

  lis_ctx.hi2c = &hi2c3;
  lis_ctx.addr = LIS2DUX12_I2C_ADD_H;
  driver_ctx.read_reg = AppSensorLisRead;
  driver_ctx.write_reg = AppSensorLisWrite;
  driver_ctx.handle = &lis_ctx;
  driver_ctx.mdelay = 0;
  driver_ctx.priv_data = &lis_priv;

  if (AppSensorLisResolveDevice(&driver_ctx, &lis_ctx, &whoami) == 0U)
  {
    AppSensorLisMarkRuntimeFault(-323L);
    return;
  }

  if (lis2dux12_stpcnt_rst_step_set(&driver_ctx) != 0)
  {
    AppSensorLisMarkRuntimeFault(-324L);
    return;
  }

  if (lis2dux12_stpcnt_steps_get(&driver_ctx, &steps) == 0)
  {
    g_sensor_lis_live.step_count = (ULONG)steps;
  }
  else
  {
    error_code = -325L;
  }

  g_sensor_lis_live.addr = (ULONG)lis_ctx.addr;
  g_sensor_lis_live.whoami = (ULONG)whoami;
  g_sensor_lis.last_error = error_code;
  g_sensor_bus_fault = 0UL;
  (void)AppSensorHealthFlagsPublish();
}

static uint8_t AppSensorProbeLis(LONG *error_out)
{
  app_sensor_lis_ctx_t lis_ctx = {0};
  stmdev_ctx_t driver_ctx = {0};
  lis2dux12_priv_t lis_priv = {0};
  uint8_t whoami = 0U;
  lis2dux12_ctrl1_t ctrl1 = {0};
  lis2dux12_ctrl4_t ctrl4 = {0};

  lis_ctx.hi2c = &hi2c3;
  lis_ctx.addr = LIS2DUX12_I2C_ADD_H;
  driver_ctx.read_reg = AppSensorLisRead;
  driver_ctx.write_reg = AppSensorLisWrite;
  driver_ctx.handle = &lis_ctx;
  driver_ctx.mdelay = 0;
  driver_ctx.priv_data = &lis_priv;

  if (AppSensorLisResolveDevice(&driver_ctx, &lis_ctx, &whoami) == 0U)
  {
    if (error_out != TX_NULL)
    {
      *error_out = -301L;
    }
    return 0U;
  }

  if (lis2dux12_init_set(&driver_ctx) != 0)
  {
    if (error_out != TX_NULL)
    {
      *error_out = -302L;
    }
    return 0U;
  }

  if ((lis2dux12_read_reg(&driver_ctx, LIS2DUX12_CTRL1, (uint8_t *)&ctrl1, 1U) != 0) ||
      (lis2dux12_read_reg(&driver_ctx, LIS2DUX12_CTRL4, (uint8_t *)&ctrl4, 1U) != 0))
  {
    if (error_out != TX_NULL)
    {
      *error_out = -303L;
    }
    return 0U;
  }

  if ((ctrl1.if_add_inc != PROPERTY_ENABLE) || (ctrl4.bdu != PROPERTY_ENABLE))
  {
    if (error_out != TX_NULL)
    {
      *error_out = -304L;
    }
    return 0U;
  }

  if (AppSensorLisApplyProfile(&driver_ctx, g_sensor_lis_profile_requested, error_out) == 0U)
  {
    return 0U;
  }

  if (g_sensor_lis_step_enabled_requested != 0UL)
  {
    if (AppSensorLisApplyStepConfig(&driver_ctx, g_sensor_lis_step_enabled_requested, error_out) == 0U)
    {
      return 0U;
    }
  }
  else
  {
    g_sensor_lis_live.step_enabled = 0UL;
  }

  g_sensor_lis_live.addr = (ULONG)lis_ctx.addr;
  g_sensor_lis_live.whoami = (ULONG)whoami;

  return 1U;
}

static uint8_t AppSensorPollLis(LONG *error_out)
{
  app_sensor_lis_ctx_t lis_ctx = {0};
  stmdev_ctx_t driver_ctx = {0};
  lis2dux12_priv_t lis_priv = {0};
  uint8_t whoami = 0U;
  uint8_t status_reg = 0U;
  uint8_t raw_xyz[6] = {0U};
  int16_t x_raw;
  int16_t y_raw;
  int16_t z_raw;

  lis_ctx.hi2c = &hi2c3;
  lis_ctx.addr = LIS2DUX12_I2C_ADD_H;
  driver_ctx.read_reg = AppSensorLisRead;
  driver_ctx.write_reg = AppSensorLisWrite;
  driver_ctx.handle = &lis_ctx;
  driver_ctx.mdelay = 0;
  driver_ctx.priv_data = &lis_priv;

  if (AppSensorLisResolveDevice(&driver_ctx, &lis_ctx, &whoami) == 0U)
  {
    g_sensor_lis_live.fail_count++;
    g_sensor_lis_live.last_error = -305L;
    if (error_out != TX_NULL)
    {
      *error_out = -305L;
    }
    return 0U;
  }

  if (lis2dux12_read_reg(&driver_ctx, LIS2DUX12_STATUS, &status_reg, 1U) != 0)
  {
    g_sensor_lis_live.fail_count++;
    g_sensor_lis_live.last_error = -306L;
    if (error_out != TX_NULL)
    {
      *error_out = -306L;
    }
    return 0U;
  }

  if (lis2dux12_read_reg(&driver_ctx, LIS2DUX12_OUT_X_L, raw_xyz, sizeof(raw_xyz)) != 0)
  {
    g_sensor_lis_live.fail_count++;
    g_sensor_lis_live.last_error = -307L;
    if (error_out != TX_NULL)
    {
      *error_out = -307L;
    }
    return 0U;
  }

  x_raw = (int16_t)((uint16_t)raw_xyz[0] | ((uint16_t)raw_xyz[1] << 8));
  y_raw = (int16_t)((uint16_t)raw_xyz[2] | ((uint16_t)raw_xyz[3] << 8));
  z_raw = (int16_t)((uint16_t)raw_xyz[4] | ((uint16_t)raw_xyz[5] << 8));

  g_sensor_lis_live.addr = (ULONG)lis_ctx.addr;
  g_sensor_lis_live.whoami = (ULONG)whoami;
  g_sensor_lis_live.status = (ULONG)status_reg;
  g_sensor_lis_live.x_raw = x_raw;
  g_sensor_lis_live.y_raw = y_raw;
  g_sensor_lis_live.z_raw = z_raw;
  g_sensor_lis_live.last_sample_tick = (ULONG)HAL_GetTick();
  g_sensor_lis_live.sample_count++;
  g_sensor_lis_live.last_error = 0L;
  {
    lis2dux12_stpcnt_mode_t step_mode = {0};
    lis2dux12_embedded_status_t emb_status = {0};
    uint16_t steps = 0U;

    if (lis2dux12_stpcnt_mode_get(&driver_ctx, &step_mode) == 0)
    {
      g_sensor_lis_live.step_enabled = (step_mode.step_counter_enable != 0U) ? 1UL : 0UL;
    }

    if (lis2dux12_stpcnt_steps_get(&driver_ctx, &steps) == 0)
    {
      g_sensor_lis_live.step_count = (ULONG)steps;
    }

    if (lis2dux12_embedded_status_get(&driver_ctx, &emb_status) == 0)
    {
      g_sensor_lis_live.step_detected = (emb_status.is_step_det != 0U) ? 1UL : 0UL;
      g_sensor_lis_live.tilt_detected = (emb_status.is_tilt != 0U) ? 1UL : 0UL;
      g_sensor_lis_live.sigmot_detected = (emb_status.is_sigmot != 0U) ? 1UL : 0UL;
    }
  }

  return 1U;
}

static uint8_t AppSensorPollLisStreamFast(LONG *error_out)
{
  app_sensor_lis_ctx_t lis_ctx = {0};
  stmdev_ctx_t driver_ctx = {0};
  lis2dux12_priv_t lis_priv = {0};
  uint8_t whoami = LIS2DUX12_ID;
  uint8_t status_reg = 0U;
  uint8_t raw_xyz[6] = {0U};
  int16_t x_raw = 0;
  int16_t y_raw = 0;
  int16_t z_raw = 0;
  uint8_t retried = 0U;

  if (g_sensor_lis.state != (ULONG)APP_SENSOR_STATE_READY)
  {
    if (error_out != TX_NULL)
    {
      *error_out = -308L;
    }
    return 0U;
  }

  lis_ctx.hi2c = &hi2c3;
  lis_ctx.addr = (uint16_t)g_sensor_lis_live.addr;
  if ((lis_ctx.addr != LIS2DUX12_I2C_ADD_H) && (lis_ctx.addr != LIS2DUX12_I2C_ADD_L))
  {
    lis_ctx.addr = LIS2DUX12_I2C_ADD_H;
  }

  if ((uint8_t)g_sensor_lis_live.whoami == LIS2DUX12_ID)
  {
    whoami = (uint8_t)g_sensor_lis_live.whoami;
  }

  driver_ctx.read_reg = AppSensorLisRead;
  driver_ctx.write_reg = AppSensorLisWrite;
  driver_ctx.handle = &lis_ctx;
  driver_ctx.mdelay = 0;
  driver_ctx.priv_data = &lis_priv;

  for (;;)
  {
    if ((lis2dux12_read_reg(&driver_ctx, LIS2DUX12_STATUS, &status_reg, 1U) == 0) &&
        (lis2dux12_read_reg(&driver_ctx, LIS2DUX12_OUT_X_L, raw_xyz, sizeof(raw_xyz)) == 0))
    {
      break;
    }

    if (retried != 0U)
    {
      g_sensor_lis_live.fail_count++;
      g_sensor_lis_live.last_error = -309L;
      if (error_out != TX_NULL)
      {
        *error_out = -309L;
      }
      return 0U;
    }

    if (AppSensorLisResolveDevice(&driver_ctx, &lis_ctx, &whoami) == 0U)
    {
      g_sensor_lis_live.fail_count++;
      g_sensor_lis_live.last_error = -310L;
      if (error_out != TX_NULL)
      {
        *error_out = -310L;
      }
      return 0U;
    }

    retried = 1U;
  }

  x_raw = (int16_t)((uint16_t)raw_xyz[0] | ((uint16_t)raw_xyz[1] << 8));
  y_raw = (int16_t)((uint16_t)raw_xyz[2] | ((uint16_t)raw_xyz[3] << 8));
  z_raw = (int16_t)((uint16_t)raw_xyz[4] | ((uint16_t)raw_xyz[5] << 8));

  g_sensor_lis_live.addr = (ULONG)lis_ctx.addr;
  g_sensor_lis_live.whoami = (ULONG)whoami;
  g_sensor_lis_live.status = (ULONG)status_reg;
  g_sensor_lis_live.x_raw = x_raw;
  g_sensor_lis_live.y_raw = y_raw;
  g_sensor_lis_live.z_raw = z_raw;
  g_sensor_lis_live.last_sample_tick = (ULONG)HAL_GetTick();
  g_sensor_lis_live.sample_count++;
  g_sensor_lis_live.last_error = 0L;

  return 1U;
}

static VOID AppSensorLisRefreshStepStatusNow(void)
{
  app_sensor_lis_ctx_t lis_ctx = {0};
  stmdev_ctx_t driver_ctx = {0};
  lis2dux12_priv_t lis_priv = {0};
  uint8_t whoami = LIS2DUX12_ID;
  lis2dux12_stpcnt_mode_t step_mode = {0};
  lis2dux12_embedded_status_t emb_status = {0};
  uint16_t steps = 0U;
  LONG error_code = 0L;

  if (g_sensor_lis.state != (ULONG)APP_SENSOR_STATE_READY)
  {
    return;
  }

  if (AppSensorModeHasAny(APP_MODE_FLAG_FLASHING) != 0U)
  {
    return;
  }

  if ((g_sensor_lis_step_enabled_requested == 0UL) &&
      (g_sensor_lis_live.step_enabled == 0UL))
  {
    g_sensor_lis_live.step_detected = 0UL;
    g_sensor_lis_live.tilt_detected = 0UL;
    g_sensor_lis_live.sigmot_detected = 0UL;
    return;
  }

  lis_ctx.hi2c = &hi2c3;
  lis_ctx.addr = (uint16_t)g_sensor_lis_live.addr;
  if ((lis_ctx.addr != LIS2DUX12_I2C_ADD_H) && (lis_ctx.addr != LIS2DUX12_I2C_ADD_L))
  {
    lis_ctx.addr = LIS2DUX12_I2C_ADD_H;
  }

  if ((uint8_t)g_sensor_lis_live.whoami == LIS2DUX12_ID)
  {
    whoami = (uint8_t)g_sensor_lis_live.whoami;
  }

  driver_ctx.read_reg = AppSensorLisRead;
  driver_ctx.write_reg = AppSensorLisWrite;
  driver_ctx.handle = &lis_ctx;
  driver_ctx.mdelay = 0;
  driver_ctx.priv_data = &lis_priv;

  if (lis2dux12_stpcnt_mode_get(&driver_ctx, &step_mode) != 0)
  {
    if (AppSensorLisResolveDevice(&driver_ctx, &lis_ctx, &whoami) == 0U)
    {
      AppSensorLisMarkRuntimeFault(-326L);
      return;
    }

    if (lis2dux12_stpcnt_mode_get(&driver_ctx, &step_mode) != 0)
    {
      AppSensorLisMarkRuntimeFault(-327L);
      return;
    }
  }

  g_sensor_lis_live.step_enabled = (step_mode.step_counter_enable != 0U) ? 1UL : 0UL;

  if (lis2dux12_stpcnt_steps_get(&driver_ctx, &steps) == 0)
  {
    g_sensor_lis_live.step_count = (ULONG)steps;
  }
  else
  {
    error_code = -328L;
  }

  if (lis2dux12_embedded_status_get(&driver_ctx, &emb_status) == 0)
  {
    g_sensor_lis_live.step_detected = (emb_status.is_step_det != 0U) ? 1UL : 0UL;
    g_sensor_lis_live.tilt_detected = (emb_status.is_tilt != 0U) ? 1UL : 0UL;
    g_sensor_lis_live.sigmot_detected = (emb_status.is_sigmot != 0U) ? 1UL : 0UL;
  }
  else if (error_code == 0L)
  {
    error_code = -329L;
  }

  g_sensor_lis_live.addr = (ULONG)lis_ctx.addr;
  g_sensor_lis_live.whoami = (ULONG)whoami;

  if (error_code != 0L)
  {
    AppSensorLisMarkRuntimeFault(error_code);
    return;
  }

  g_sensor_lis.last_error = 0L;
  g_sensor_bus_fault = 0UL;
}

static VOID AppSensorDeviceInit(app_sensor_fsm_t *dev, uint8_t (*probe_fn)(LONG *))
{
  LONG error_code = 0L;
  ULONG now_ticks = tx_time_get();

  if ((dev == TX_NULL) || (probe_fn == 0))
  {
    return;
  }

  if (((dev->state == (ULONG)APP_SENSOR_STATE_RECOVERING) ||
       (dev->state == (ULONG)APP_SENSOR_STATE_FAULT)) &&
      (AppSensorRetryDue(now_ticks, dev->next_retry_tick) == 0U))
  {
    return;
  }

  if (dev->state == (ULONG)APP_SENSOR_STATE_FAULT)
  {
    dev->recovery_attempts = 0UL;
  }

  dev->state = (ULONG)APP_SENSOR_STATE_INITING;

  if (AppSensorProbeWithRecovery(probe_fn, &error_code) != 0U)
  {
    dev->state = (ULONG)APP_SENSOR_STATE_READY;
    dev->recovery_attempts = 0UL;
    dev->next_retry_tick = 0UL;
    dev->last_success_tick = now_ticks;
    dev->last_error = 0L;
    return;
  }

  dev->fail_count++;
  dev->last_error = error_code;

  dev->recovery_attempts++;
  if (dev->recovery_attempts >= KNOB_SENSOR_RECOVERY_MAX_ATTEMPTS)
  {
    dev->state = (ULONG)APP_SENSOR_STATE_FAULT;
    dev->next_retry_tick = now_ticks + KNOB_SENSOR_FAULT_RETRY_TICKS;
    return;
  }

  dev->state = (ULONG)APP_SENSOR_STATE_RECOVERING;
  dev->next_retry_tick = now_ticks + KNOB_SENSOR_RECOVERY_BACKOFF_TICKS;
}

static VOID AppSensorDevicePoll(app_sensor_fsm_t *dev, uint8_t (*poll_fn)(LONG *), uint8_t (*probe_fn)(LONG *))
{
  LONG error_code = 0L;
  ULONG now_ticks = tx_time_get();

  if ((dev == TX_NULL) || (poll_fn == 0) || (probe_fn == 0))
  {
    return;
  }

  if (dev->state == (ULONG)APP_SENSOR_STATE_SUSPENDED)
  {
    return;
  }

  if ((dev->state == (ULONG)APP_SENSOR_STATE_READY) && (poll_fn(&error_code) == 0U))
  {
    dev->fail_count++;
    dev->last_error = error_code;

    dev->recovery_attempts = 1UL;
    if (dev->recovery_attempts >= KNOB_SENSOR_RECOVERY_MAX_ATTEMPTS)
    {
      dev->state = (ULONG)APP_SENSOR_STATE_FAULT;
      dev->next_retry_tick = now_ticks + KNOB_SENSOR_FAULT_RETRY_TICKS;
    }
    else
    {
      dev->state = (ULONG)APP_SENSOR_STATE_RECOVERING;
      dev->next_retry_tick = now_ticks + KNOB_SENSOR_RECOVERY_BACKOFF_TICKS;
    }
    return;
  }

  if (dev->state == (ULONG)APP_SENSOR_STATE_READY)
  {
    dev->last_success_tick = now_ticks;
    dev->last_error = 0L;
    return;
  }

  if (dev->state != (ULONG)APP_SENSOR_STATE_READY)
  {
    AppSensorDeviceInit(dev, probe_fn);
  }
}

static VOID AppSensorApplyDefaults(ULONG targets_mask)
{
  ULONG selected = targets_mask & APP_SENSOR_TARGET_MASK_ALL;
  LONG bus_error = 0L;

  if (selected == 0UL)
  {
    selected = APP_SENSOR_TARGET_MASK_ALL;
  }

  g_sensor_bus_fault = 0UL;
  if (AppSensorBusSanityCheck(&bus_error) == 0U)
  {
    g_sensor_bus_fault = 1UL;
  }

  if ((selected & APP_SENSOR_TARGET_PMIC) != 0UL)
  {
    AppSensorResetRecoveryState(&g_sensor_pmic);
    AppSensorDeviceInit(&g_sensor_pmic, AppSensorProbePmic);
  }

  if ((selected & APP_SENSOR_TARGET_TMAG) != 0UL)
  {
    AppSensorResetRecoveryState(&g_sensor_tmag);
    AppSensorDeviceInit(&g_sensor_tmag, AppSensorProbeTmag);
  }

  if ((selected & APP_SENSOR_TARGET_LIS) != 0UL)
  {
    AppSensorResetRecoveryState(&g_sensor_lis);
    AppSensorDeviceInit(&g_sensor_lis, AppSensorProbeLis);
  }

  if (((selected & APP_SENSOR_TARGET_PMIC) != 0UL) && (g_sensor_pmic.state == (ULONG)APP_SENSOR_STATE_READY))
  {
    g_sensor_bus_fault = 0UL;
  }

  if (((selected & APP_SENSOR_TARGET_TMAG) != 0UL) && (g_sensor_tmag.state == (ULONG)APP_SENSOR_STATE_READY))
  {
    g_sensor_bus_fault = 0UL;
  }

  if (((selected & APP_SENSOR_TARGET_LIS) != 0UL) && (g_sensor_lis.state == (ULONG)APP_SENSOR_STATE_READY))
  {
    g_sensor_bus_fault = 0UL;
  }

  (void)AppSensorHealthFlagsPublish();
}

static VOID AppSensorRunPollSequence(ULONG targets_mask)
{
  ULONG selected = targets_mask & APP_SENSOR_TARGET_MASK_ALL;
  LONG bus_error = 0L;

  if (selected == 0UL)
  {
    selected = APP_SENSOR_TARGET_MASK_ALL;
  }

  if (AppSensorBusSanityCheck(&bus_error) == 0U)
  {
    g_sensor_bus_fault = 1UL;
  }

  if ((selected & APP_SENSOR_TARGET_PMIC) != 0UL)
  {
    AppSensorDevicePoll(&g_sensor_pmic, AppSensorPollPmic, AppSensorProbePmic);
  }

  if ((selected & APP_SENSOR_TARGET_TMAG) != 0UL)
  {
    AppSensorDevicePoll(&g_sensor_tmag, AppSensorPollTmag, AppSensorProbeTmag);
  }

  if ((selected & APP_SENSOR_TARGET_LIS) != 0UL)
  {
    AppSensorDevicePoll(&g_sensor_lis, AppSensorPollLis, AppSensorProbeLis);
  }

  if (((selected & APP_SENSOR_TARGET_PMIC) != 0UL) && (g_sensor_pmic.state == (ULONG)APP_SENSOR_STATE_READY))
  {
    g_sensor_bus_fault = 0UL;
  }

  if (((selected & APP_SENSOR_TARGET_TMAG) != 0UL) && (g_sensor_tmag.state == (ULONG)APP_SENSOR_STATE_READY))
  {
    g_sensor_bus_fault = 0UL;
  }

  if (((selected & APP_SENSOR_TARGET_LIS) != 0UL) && (g_sensor_lis.state == (ULONG)APP_SENSOR_STATE_READY))
  {
    g_sensor_bus_fault = 0UL;
  }

  (void)AppSensorHealthFlagsPublish();
}

static VOID AppSensorRunResumeSequence(void)
{
  LONG bus_error = 0L;

  g_sensor_bus_fault = 0UL;
  AppSensorPmicRuntimeReset();
  g_sensor_lis_profile_requested = APP_SENSOR_LIS_PROFILE_LOW_POWER;
  g_sensor_lis_profile_applied = APP_SENSOR_LIS_PROFILE_LOW_POWER;
  g_sensor_lis_stream_enabled = 0UL;

  if (AppSensorBusSanityCheck(&bus_error) == 0U)
  {
    g_sensor_bus_fault = 1UL;
    g_sensor_pmic.last_error = bus_error;
    g_sensor_tmag.last_error = bus_error;
    g_sensor_lis.last_error = bus_error;
  }

  AppSensorResetRecoveryState(&g_sensor_pmic);
  AppSensorResetRecoveryState(&g_sensor_tmag);
  AppSensorResetRecoveryState(&g_sensor_lis);
  AppSensorDeviceInit(&g_sensor_pmic, AppSensorProbePmic);
  AppSensorDeviceInit(&g_sensor_tmag, AppSensorProbeTmag);
  AppSensorDeviceInit(&g_sensor_lis, AppSensorProbeLis);

  if ((g_sensor_pmic.state == (ULONG)APP_SENSOR_STATE_READY) ||
      (g_sensor_tmag.state == (ULONG)APP_SENSOR_STATE_READY) ||
      (g_sensor_lis.state == (ULONG)APP_SENSOR_STATE_READY))
  {
    g_sensor_bus_fault = 0UL;
  }

  if (g_sensor_tmag.state == (ULONG)APP_SENSOR_STATE_READY)
  {
    TMAGJoy_Cal active_cal = {0};

    TMAGJoy_InitOnce();
    g_sensor_joy = UI_GetJoy();
    g_sensor_joy_input_gate_valid = 0UL;
    g_sensor_joy_input_neutral_armed = 0UL;
    g_sensor_joy_input_neutral_stable_count = 0UL;
    g_sensor_joy_live_read_fail_streak = 0UL;
    AppSensorJoyCalApplyLoadedIfReady();

    /*
     * Resume can occur without a new joycfg load/save sequence tick.
     * If active runtime calibration is already sane, re-arm input gate
     * immediately so wake does not strand joystick input at gate_valid=0.
     */
    if (g_sensor_joy != TX_NULL)
    {
      TMAGJoy_GetCal(g_sensor_joy, &active_cal);
      if (AppSensorJoyCalSane(&active_cal) != 0U)
      {
        g_sensor_joy_input_gate_valid = 1UL;
        AppSensorJoySeedNeutralArm();
      }
    }
  }
  else
  {
    g_sensor_joy = TX_NULL;
    g_sensor_joy_input_gate_valid = 0UL;
    g_sensor_joy_input_neutral_armed = 0UL;
    g_sensor_joy_input_neutral_stable_count = 0UL;
    g_sensor_joy_live_read_fail_streak = 0UL;
  }

  (void)AppSensorHealthFlagsPublish();
}

static void AppSensorJoyCalResetStatus(void)
{
  g_sensor_joy_cal_status.stage = (ULONG)APP_JOY_CAL_STAGE_IDLE;
  g_sensor_joy_cal_status.progress = 0.0f;
  g_sensor_joy_cal_status.last_error = 0L;
  g_sensor_joy_cal_status.save_pending = 0UL;
  g_sensor_joy_cal_status.save_ok_count = 0UL;
  g_sensor_joy_cal_status.save_fail_count = 0UL;
  g_sensor_joy_cal_status.load_ok_count = 0UL;
  g_sensor_joy_cal_status.load_fail_count = 0UL;
  AppSensorJoyCalQualityReset();
}

static void AppSensorJoyApplyRuntimeDeadzone(void)
{
  if (g_sensor_joy != TX_NULL)
  {
    uint8_t deadzone_en = 0U;
    float deadzone_live_mT = 0.0f;

    TMAGJoy_SetAbsDeadzone(g_sensor_joy,
                           (uint8_t)((g_storage_joycfg_deadzone_enabled != 0UL) ? 1U : 0U),
                           AppStorageDeadzoneClampMt(g_storage_joycfg_deadzone_mT));
    TMAGJoy_GetAbsDeadzone(g_sensor_joy, &deadzone_en, &deadzone_live_mT);
    g_sensor_joy_live_status.deadzone_enabled = (ULONG)deadzone_en;
    g_sensor_joy_live_status.deadzone_mT = deadzone_live_mT;
  }
}

static void AppSensorJoyCalApplyLoadedIfReady(void)
{
  if (g_storage_joycfg_load_seq != g_sensor_joycfg_seen_load_seq)
  {
    g_sensor_joycfg_seen_load_seq = g_storage_joycfg_load_seq;
    g_sensor_joy_cal_status.load_ok_count = g_storage_joycfg_load_ok_count;
    g_sensor_joy_cal_status.load_fail_count = g_storage_joycfg_load_fail_count;
    AppSensorJoyApplyRuntimeDeadzone();

    if ((g_storage_joycfg_valid != 0UL) && (g_sensor_joy != TX_NULL))
    {
      if (AppSensorJoyCalSane(&g_storage_joycfg_cal) != 0U)
      {
        TMAGJoy_SetCenter(g_sensor_joy, g_storage_joycfg_cal.cx, g_storage_joycfg_cal.cy);
        TMAGJoy_SetSpan(g_sensor_joy, g_storage_joycfg_cal.sx, g_storage_joycfg_cal.sy);
        TMAGJoy_SetRotationDeg(g_sensor_joy, g_storage_joycfg_cal.rot_deg);
        TMAGJoy_SetInvert(g_sensor_joy, g_storage_joycfg_cal.invert_x, g_storage_joycfg_cal.invert_y);
        g_sensor_joy_input_gate_valid = 1UL;
        AppSensorJoySeedNeutralArm();
      }
      else
      {
        g_sensor_joy_cal_status.last_error = APP_STORAGE_ERR_JOYCFG_INVALID;
        g_sensor_joy_input_gate_valid = 0UL;
        g_sensor_joy_input_neutral_armed = 0UL;
        g_sensor_joy_input_neutral_stable_count = 0UL;
      }
    }
    else if (g_storage_joycfg_last_error != APP_STORAGE_ERR_NONE)
    {
      g_sensor_joy_cal_status.last_error = g_storage_joycfg_last_error;
      if (g_sensor_joy != TX_NULL)
      {
        TMAGJoy_Cal active_cal = {0};
        TMAGJoy_GetCal(g_sensor_joy, &active_cal);
        if (AppSensorJoyCalSane(&active_cal) != 0U)
        {
          g_sensor_joy_input_gate_valid = 1UL;
          AppSensorJoySeedNeutralArm();
        }
        else
        {
          g_sensor_joy_input_gate_valid = 0UL;
          g_sensor_joy_input_neutral_armed = 0UL;
          g_sensor_joy_input_neutral_stable_count = 0UL;
        }
      }
      else
      {
        g_sensor_joy_input_gate_valid = 0UL;
        g_sensor_joy_input_neutral_armed = 0UL;
        g_sensor_joy_input_neutral_stable_count = 0UL;
      }
    }
    else
    {
      g_sensor_joy_input_gate_valid = 0UL;
      g_sensor_joy_input_neutral_armed = 0UL;
      g_sensor_joy_input_neutral_stable_count = 0UL;
    }
  }

  if (g_storage_joycfg_save_seq != g_sensor_joycfg_seen_save_seq)
  {
    g_sensor_joycfg_seen_save_seq = g_storage_joycfg_save_seq;
    g_sensor_joy_cal_status.save_ok_count = g_storage_joycfg_save_ok_count;
    g_sensor_joy_cal_status.save_fail_count = g_storage_joycfg_save_fail_count;
    g_sensor_joy_cal_status.save_pending = 0UL;
    if (g_storage_joycfg_last_error != APP_STORAGE_ERR_NONE)
    {
      g_sensor_joy_cal_status.last_error = g_storage_joycfg_last_error;
      if (g_sensor_joy != TX_NULL)
      {
        TMAGJoy_Cal active_cal = {0};
        TMAGJoy_GetCal(g_sensor_joy, &active_cal);
        if (AppSensorJoyCalSane(&active_cal) != 0U)
        {
          g_sensor_joy_input_gate_valid = 1UL;
          AppSensorJoySeedNeutralArm();
        }
        else
        {
          g_sensor_joy_input_gate_valid = 0UL;
          g_sensor_joy_input_neutral_armed = 0UL;
          g_sensor_joy_input_neutral_stable_count = 0UL;
        }
      }
      else
      {
        g_sensor_joy_input_gate_valid = 0UL;
        g_sensor_joy_input_neutral_armed = 0UL;
        g_sensor_joy_input_neutral_stable_count = 0UL;
      }
    }
    else
    {
      /* Save completion should not revoke input on transient non-neutral state. */
      if ((g_storage_joycfg_valid != 0UL) && (g_sensor_joy != TX_NULL))
      {
        TMAGJoy_Cal active_cal = {0};
        TMAGJoy_GetCal(g_sensor_joy, &active_cal);
        if (AppSensorJoyCalSane(&active_cal) != 0U)
        {
          g_sensor_joy_input_gate_valid = 1UL;
          AppSensorJoySeedNeutralArm();
          g_sensor_joy_cal_snapshot_valid = 0UL;
        }
        else
        {
          g_sensor_joy_cal_status.last_error = APP_STORAGE_ERR_JOYCFG_INVALID;
          g_sensor_joy_input_gate_valid = 0UL;
          g_sensor_joy_input_neutral_armed = 0UL;
          g_sensor_joy_input_neutral_stable_count = 0UL;
        }
      }
      else
      {
        g_sensor_joy_input_gate_valid = 0UL;
        g_sensor_joy_input_neutral_armed = 0UL;
        g_sensor_joy_input_neutral_stable_count = 0UL;
      }
    }
  }
}

static void AppSensorJoyCalSnapshot(void)
{
  if (g_sensor_joy == TX_NULL)
  {
    g_sensor_joy_cal_snapshot_valid = 0UL;
    g_sensor_joy_input_gate_snapshot_valid = 0UL;
    return;
  }

  TMAGJoy_GetCal(g_sensor_joy, &g_sensor_joy_cal_snapshot);
  g_sensor_joy_cal_snapshot_valid = 1UL;
  g_sensor_joy_input_gate_snapshot_valid = g_sensor_joy_input_gate_valid;
}

static void AppSensorJoyCalRestoreSnapshot(void)
{
  if ((g_sensor_joy != TX_NULL) &&
      (g_sensor_joy_cal_snapshot_valid != 0UL) &&
      (AppSensorJoyCalSane(&g_sensor_joy_cal_snapshot) != 0U))
  {
    TMAGJoy_SetCenter(g_sensor_joy, g_sensor_joy_cal_snapshot.cx, g_sensor_joy_cal_snapshot.cy);
    TMAGJoy_SetSpan(g_sensor_joy, g_sensor_joy_cal_snapshot.sx, g_sensor_joy_cal_snapshot.sy);
    TMAGJoy_SetRotationDeg(g_sensor_joy, g_sensor_joy_cal_snapshot.rot_deg);
    TMAGJoy_SetInvert(g_sensor_joy, g_sensor_joy_cal_snapshot.invert_x, g_sensor_joy_cal_snapshot.invert_y);
    g_sensor_joy_input_gate_valid = g_sensor_joy_input_gate_snapshot_valid;
  }
  else
  {
    g_sensor_joy_input_gate_valid = 0UL;
    g_sensor_joy_cal_snapshot_valid = 0UL;
  }

  g_sensor_joy_input_neutral_armed = 0UL;
  g_sensor_joy_input_neutral_stable_count = 0UL;
}

static void AppSensorJoyCalCancel(void)
{
  if (g_sensor_joy_cal_snapshot_valid != 0UL)
  {
    AppSensorJoyCalRestoreSnapshot();
  }

  g_sensor_joy_cal_active = 0UL;
  g_sensor_joy_cal_capture.active = 0U;
  g_sensor_joy_cal_stage = (ULONG)APP_JOY_CAL_STAGE_IDLE;
  g_sensor_joy_cal_wait_confirm = 0UL;
  g_sensor_joy_cal_status.stage = (ULONG)APP_JOY_CAL_STAGE_IDLE;
  g_sensor_joy_cal_status.progress = 0.0f;
  g_sensor_joy_cal_status.save_pending = 0UL;
  g_sensor_joy_cal_status.last_error = 0L;
  g_sensor_joy_cal_snapshot_valid = 0UL;
  g_sensor_joy_input_gate_snapshot_valid = 0UL;
  AppSensorJoyCalQualityReset();

  if ((g_sensor_joy_input_gate_valid != 0UL) && (g_sensor_joy != TX_NULL))
  {
    AppSensorJoySeedNeutralArm();
  }
}

static uint8_t AppSensorJoyCalDirectionMatchesStage(ULONG stage, float avg_x, float avg_y)
{
  float mag = sqrtf((avg_x * avg_x) + (avg_y * avg_y));
  (void)stage;
  return (mag >= 0.20f) ? 1U : 0U;
}

static void AppSensorJoyCalQualityReset(void)
{
  (void)memset(&g_sensor_joy_cal_quality, 0, sizeof(g_sensor_joy_cal_quality));
}

static void AppSensorJoyCalComputeQuality(const TMAGJoy_Cal *final_cal)
{
  static const float target_x[APP_JOY_CAL_DIR_COUNT] = {0.0f, 1.0f, 0.0f, -1.0f};
  static const float target_y[APP_JOY_CAL_DIR_COUNT] = {1.0f, 0.0f, -1.0f, 0.0f};
  float sx;
  float sy;
  float span_ratio;
  float axis_error = 0.0f;
  float norm_min = 1000.0f;
  float norm_max = 0.0f;
  float rot_rad;
  float rot_c;
  float rot_s;
  uint32_t i;
  ULONG wrong_dir = 0UL;

  AppSensorJoyCalQualityReset();
  if (final_cal == TX_NULL)
  {
    return;
  }

  sx = fabsf(final_cal->sx);
  sy = fabsf(final_cal->sy);
  if ((sx < 1.0e-6f) || (sy < 1.0e-6f))
  {
    g_sensor_joy_cal_quality.valid = 1UL;
    return;
  }

  span_ratio = (sx >= sy) ? (sx / sy) : (sy / sx);
  rot_rad = final_cal->rot_deg * (3.14159265359f / 180.0f);
  rot_c = cosf(rot_rad);
  rot_s = sinf(rot_rad);

  for (i = 0U; i < APP_JOY_CAL_DIR_COUNT; i++)
  {
    float dx = g_sensor_joy_cal_dir_avg_x[i];
    float dy = g_sensor_joy_cal_dir_avg_y[i];
    float rx;
    float ry;
    float nx;
    float ny;
    float dot;
    float ortho;
    float primary;
    float abs_ortho;

    rx = (rot_c * dx) - (rot_s * dy);
    ry = (rot_s * dx) + (rot_c * dy);
    if (final_cal->invert_x != 0U)
    {
      rx = -rx;
    }
    if (final_cal->invert_y != 0U)
    {
      ry = -ry;
    }

    nx = rx / sx;
    ny = ry / sy;
    dot = (nx * target_x[i]) + (ny * target_y[i]);
    ortho = (nx * (-target_y[i])) + (ny * target_x[i]);

    if (dot <= 0.0f)
    {
      wrong_dir = 1UL;
    }
    primary = fabsf(dot);
    abs_ortho = fabsf(ortho);

    if (primary < norm_min)
    {
      norm_min = primary;
    }
    if (primary > norm_max)
    {
      norm_max = primary;
    }
    if (abs_ortho > axis_error)
    {
      axis_error = abs_ortho;
    }
  }

  g_sensor_joy_cal_quality.valid = 1UL;
  g_sensor_joy_cal_quality.span_ratio = span_ratio;
  g_sensor_joy_cal_quality.axis_error = axis_error;
  g_sensor_joy_cal_quality.dir_norm_min = norm_min;
  g_sensor_joy_cal_quality.dir_norm_max = norm_max;
  g_sensor_joy_cal_quality.quality_ok =
      ((wrong_dir == 0UL) &&
       (span_ratio <= APP_JOY_CAL_QUALITY_SPAN_RATIO_MAX) &&
       (axis_error <= APP_JOY_CAL_QUALITY_AXIS_ERROR_MAX) &&
       (norm_min >= APP_JOY_CAL_QUALITY_NORM_MIN_MIN) &&
       (norm_max <= APP_JOY_CAL_QUALITY_NORM_MAX_MAX)) ? 1UL : 0UL;
}

static void AppSensorJoyCaptureBegin(uint32_t duration_ms, uint32_t sample_every_ms)
{
  uint32_t settle_ms;

  if (sample_every_ms == 0U)
  {
    sample_every_ms = 1U;
  }

  if (duration_ms == 0U)
  {
    duration_ms = 1U;
  }

  settle_ms = duration_ms / 4U;
  if (settle_ms < 40U)
  {
    settle_ms = 40U;
  }
  if (settle_ms > 120U)
  {
    settle_ms = 120U;
  }

  g_sensor_joy_cal_capture.active = 1U;
  g_sensor_joy_cal_capture.t_start_ms = 0U;
  g_sensor_joy_cal_capture.duration_ms = duration_ms;
  g_sensor_joy_cal_capture.sample_every_ms = sample_every_ms;
  g_sensor_joy_cal_capture.settle_ms = settle_ms;
  g_sensor_joy_cal_capture.last_sample_ms = 0U;
  g_sensor_joy_cal_capture.n = 0U;
  g_sensor_joy_cal_capture.peak_r = 0.0f;
  g_sensor_joy_cal_capture.sum_w = 0.0f;
  g_sensor_joy_cal_capture.sum_x = 0.0f;
  g_sensor_joy_cal_capture.sum_y = 0.0f;
}

static uint8_t AppSensorJoyCaptureStep(uint32_t now_ms, float *progress_out, float *avg_x_out, float *avg_y_out)
{
  uint32_t elapsed_ms;
  float progress;

  if (progress_out != NULL)
  {
    *progress_out = 0.0f;
  }
  if (avg_x_out != NULL)
  {
    *avg_x_out = 0.0f;
  }
  if (avg_y_out != NULL)
  {
    *avg_y_out = 0.0f;
  }

  if ((g_sensor_joy == TX_NULL) || (g_sensor_joy_cal_capture.active == 0U))
  {
    return 1U;
  }

  if (g_sensor_joy_cal_capture.t_start_ms == 0U)
  {
    g_sensor_joy_cal_capture.t_start_ms = now_ms;
    g_sensor_joy_cal_capture.last_sample_ms = now_ms;
  }

  {
    uint32_t slots = 0U;

    if (g_sensor_joy_cal_capture.sample_every_ms > 0U)
    {
      slots = (now_ms - g_sensor_joy_cal_capture.last_sample_ms) / g_sensor_joy_cal_capture.sample_every_ms;
    }
    if (slots > 16U)
    {
      slots = 16U;
    }

    while (slots > 0U)
    {
      float nx = 0.0f;
      float ny = 0.0f;
      float r = 0.0f;
      uint32_t sample_tick;

      g_sensor_joy_cal_capture.last_sample_ms += g_sensor_joy_cal_capture.sample_every_ms;
      sample_tick = g_sensor_joy_cal_capture.last_sample_ms;

      if (TMAGJoy_ReadCalibratedRaw(g_sensor_joy, &nx, &ny, TX_NULL) != 0)
      {
        g_sensor_joy_cal_status.last_error = -508L;
        slots--;
        continue;
      }
      r = sqrtf((nx * nx) + (ny * ny));

      if ((sample_tick - g_sensor_joy_cal_capture.t_start_ms) >= g_sensor_joy_cal_capture.settle_ms)
      {
        if (r > g_sensor_joy_cal_capture.peak_r)
        {
          g_sensor_joy_cal_capture.peak_r = r;
        }

        g_sensor_joy_cal_capture.sum_x += nx;
        g_sensor_joy_cal_capture.sum_y += ny;
        g_sensor_joy_cal_capture.sum_w += 1.0f;
        g_sensor_joy_cal_capture.n++;
      }

      slots--;
    }
  }

  elapsed_ms = (now_ms - g_sensor_joy_cal_capture.t_start_ms);
  progress = (float)elapsed_ms / (float)g_sensor_joy_cal_capture.duration_ms;
  if (progress > 1.0f)
  {
    progress = 1.0f;
  }
  if (progress_out != NULL)
  {
    *progress_out = progress;
  }

  if (elapsed_ms < g_sensor_joy_cal_capture.duration_ms)
  {
    return 0U;
  }

  g_sensor_joy_cal_capture.active = 0U;
  if ((g_sensor_joy_cal_capture.n > 0U) && (g_sensor_joy_cal_capture.sum_w > 1.0e-5f))
  {
    float inv_w = 1.0f / g_sensor_joy_cal_capture.sum_w;
    if (avg_x_out != NULL)
    {
      *avg_x_out = g_sensor_joy_cal_capture.sum_x * inv_w;
    }
    if (avg_y_out != NULL)
    {
      *avg_y_out = g_sensor_joy_cal_capture.sum_y * inv_w;
    }
  }

  return 1U;
}

static uint8_t AppSensorJoyCalApplyDirectionalSolve(void)
{
  float fit_err = 0.0f;
  int solve_rc;

  if (g_sensor_joy == TX_NULL)
  {
    return 0U;
  }

  solve_rc = TMAGJoy_SolveCardinals(g_sensor_joy,
                                    g_sensor_joy_cal_dir_avg_x[APP_JOY_CAL_DIR_UP_INDEX],
                                    g_sensor_joy_cal_dir_avg_y[APP_JOY_CAL_DIR_UP_INDEX],
                                    g_sensor_joy_cal_dir_avg_x[APP_JOY_CAL_DIR_RIGHT_INDEX],
                                    g_sensor_joy_cal_dir_avg_y[APP_JOY_CAL_DIR_RIGHT_INDEX],
                                    g_sensor_joy_cal_dir_avg_x[APP_JOY_CAL_DIR_DOWN_INDEX],
                                    g_sensor_joy_cal_dir_avg_y[APP_JOY_CAL_DIR_DOWN_INDEX],
                                    g_sensor_joy_cal_dir_avg_x[APP_JOY_CAL_DIR_LEFT_INDEX],
                                    g_sensor_joy_cal_dir_avg_y[APP_JOY_CAL_DIR_LEFT_INDEX],
                                    &fit_err);
  (void)fit_err;
  return (solve_rc == 0) ? 1U : 0U;
}

static void AppSensorJoyCalStart(void)
{
  uint32_t i;

  AppSensorJoyCalSnapshot();

  g_sensor_joy_cal_active = 0UL;
  g_sensor_joy_cal_capture.active = 0U;
  g_sensor_joy_cal_stage = (ULONG)APP_JOY_CAL_STAGE_IDLE;
  g_sensor_joy_cal_wait_confirm = 0UL;
  g_sensor_joy_input_gate_valid = 0UL;
  g_sensor_joy_input_neutral_armed = 0UL;
  g_sensor_joy_input_neutral_stable_count = 0UL;
  AppSensorJoyCalQualityReset();

  if ((g_sensor_tmag.state != (ULONG)APP_SENSOR_STATE_READY) || (g_sensor_joy == TX_NULL))
  {
    g_sensor_joy_cal_active = 0UL;
    g_sensor_joy_cal_capture.active = 0U;
    g_sensor_joy_cal_stage = (ULONG)APP_JOY_CAL_STAGE_ERROR;
    g_sensor_joy_cal_wait_confirm = 0UL;
    g_sensor_joy_cal_status.stage = (ULONG)APP_JOY_CAL_STAGE_ERROR;
    g_sensor_joy_cal_status.progress = 0.0f;
    g_sensor_joy_cal_status.last_error = -501L;
    return;
  }

  for (i = 0U; i < APP_JOY_CAL_DIR_COUNT; i++)
  {
    g_sensor_joy_cal_dir_avg_x[i] = 0.0f;
    g_sensor_joy_cal_dir_avg_y[i] = 0.0f;
  }
  (void)memset(&g_sensor_joy_cal_capture, 0, sizeof(g_sensor_joy_cal_capture));

  TMAGJoy_CalNeutral_Begin(g_sensor_joy,
                           (uint32_t)KNOB_SENSOR_JOY_CAL_NEUTRAL_WINDOW_MS,
                           (uint32_t)KNOB_SENSOR_JOY_CAL_NEUTRAL_STEP_MS);

  g_sensor_joy_cal_active = 1UL;
  g_sensor_joy_cal_stage = (ULONG)APP_JOY_CAL_STAGE_NEUTRAL;
  g_sensor_joy_cal_status.stage = (ULONG)APP_JOY_CAL_STAGE_NEUTRAL;
  g_sensor_joy_cal_status.progress = 0.0f;
  g_sensor_joy_cal_status.last_error = 0L;
  g_sensor_joy_cal_status.save_pending = 0UL;
}

static void AppSensorJoyCalStep(void)
{
  float progress = 0.0f;
  float avg_x = 0.0f;
  float avg_y = 0.0f;
  uint32_t now_ms;
  uint8_t done;
  app_joy_cal_stage_t next_stage = APP_JOY_CAL_STAGE_IDLE;

  if ((g_sensor_joy_cal_active == 0UL) || (g_sensor_joy == TX_NULL))
  {
    return;
  }

  if (g_sensor_tmag.state != (ULONG)APP_SENSOR_STATE_READY)
  {
    AppSensorJoyCalRestoreSnapshot();
    g_sensor_joy_cal_active = 0UL;
    g_sensor_joy_cal_capture.active = 0U;
    g_sensor_joy_cal_stage = (ULONG)APP_JOY_CAL_STAGE_ERROR;
    g_sensor_joy_cal_wait_confirm = 0UL;
    g_sensor_joy_cal_status.stage = (ULONG)APP_JOY_CAL_STAGE_ERROR;
    g_sensor_joy_cal_status.progress = 0.0f;
    g_sensor_joy_cal_status.last_error = (g_sensor_tmag.last_error != 0L) ? g_sensor_tmag.last_error : -508L;
    return;
  }

  now_ms = (uint32_t)HAL_GetTick();
  if (g_sensor_joy_cal_stage == (ULONG)APP_JOY_CAL_STAGE_NEUTRAL)
  {
    done = TMAGJoy_CalNeutral_Step(g_sensor_joy, now_ms, &progress);
    g_sensor_joy_cal_status.progress = progress;
    g_sensor_joy_cal_status.stage = (ULONG)APP_JOY_CAL_STAGE_NEUTRAL;
    if (done)
    {
      TMAGJoy_SetRotationDeg(g_sensor_joy, 0.0f);
      TMAGJoy_SetInvert(g_sensor_joy, 0U, 0U);
      TMAGJoy_SetSpan(g_sensor_joy, 1.0f, 1.0f);
      g_sensor_joy_cal_stage = (ULONG)APP_JOY_CAL_STAGE_UP;
      g_sensor_joy_cal_wait_confirm = 1UL;
      g_sensor_joy_cal_status.stage = (ULONG)APP_JOY_CAL_STAGE_UP;
      g_sensor_joy_cal_status.progress = 0.0f;
    }
    return;
  }

  if ((g_sensor_joy_cal_stage >= (ULONG)APP_JOY_CAL_STAGE_UP) &&
      (g_sensor_joy_cal_stage <= (ULONG)APP_JOY_CAL_STAGE_LEFT))
  {
    if (g_sensor_joy_cal_wait_confirm != 0UL)
    {
      g_sensor_joy_cal_status.progress = 0.0f;
      g_sensor_joy_cal_status.stage = g_sensor_joy_cal_stage;
      return;
    }

    done = AppSensorJoyCaptureStep(now_ms, &progress, &avg_x, &avg_y);
    g_sensor_joy_cal_status.progress = progress;
    g_sensor_joy_cal_status.stage = g_sensor_joy_cal_stage;
    if (done == 0U)
    {
      return;
    }

    if (g_sensor_joy_cal_capture.n < APP_JOY_CAL_MIN_DIR_SAMPLES)
    {
      if (g_sensor_joy_cal_status.last_error == 0L)
      {
        g_sensor_joy_cal_status.last_error = -509L;
      }
      g_sensor_joy_cal_wait_confirm = 1UL;
      g_sensor_joy_cal_status.progress = 0.0f;
      return;
    }

    if (AppSensorJoyCalDirectionMatchesStage(g_sensor_joy_cal_stage, avg_x, avg_y) == 0U)
    {
      if (g_sensor_joy_cal_status.last_error == 0L)
      {
        g_sensor_joy_cal_status.last_error = -509L;
      }
      g_sensor_joy_cal_wait_confirm = 1UL;
      g_sensor_joy_cal_status.progress = 0.0f;
      return;
    }

    g_sensor_joy_cal_status.last_error = 0L;

    switch ((app_joy_cal_stage_t)g_sensor_joy_cal_stage)
    {
      case APP_JOY_CAL_STAGE_UP:
        g_sensor_joy_cal_dir_avg_x[APP_JOY_CAL_DIR_UP_INDEX] = avg_x;
        g_sensor_joy_cal_dir_avg_y[APP_JOY_CAL_DIR_UP_INDEX] = avg_y;
        next_stage = APP_JOY_CAL_STAGE_RIGHT;
        break;

      case APP_JOY_CAL_STAGE_RIGHT:
        g_sensor_joy_cal_dir_avg_x[APP_JOY_CAL_DIR_RIGHT_INDEX] = avg_x;
        g_sensor_joy_cal_dir_avg_y[APP_JOY_CAL_DIR_RIGHT_INDEX] = avg_y;
        next_stage = APP_JOY_CAL_STAGE_DOWN;
        break;

      case APP_JOY_CAL_STAGE_DOWN:
        g_sensor_joy_cal_dir_avg_x[APP_JOY_CAL_DIR_DOWN_INDEX] = avg_x;
        g_sensor_joy_cal_dir_avg_y[APP_JOY_CAL_DIR_DOWN_INDEX] = avg_y;
        next_stage = APP_JOY_CAL_STAGE_LEFT;
        break;

      case APP_JOY_CAL_STAGE_LEFT:
        g_sensor_joy_cal_dir_avg_x[APP_JOY_CAL_DIR_LEFT_INDEX] = avg_x;
        g_sensor_joy_cal_dir_avg_y[APP_JOY_CAL_DIR_LEFT_INDEX] = avg_y;
        next_stage = APP_JOY_CAL_STAGE_SWEEP;
        break;

      case APP_JOY_CAL_STAGE_IDLE:
      case APP_JOY_CAL_STAGE_NEUTRAL:
      case APP_JOY_CAL_STAGE_SWEEP:
      case APP_JOY_CAL_STAGE_DONE:
      case APP_JOY_CAL_STAGE_ERROR:
      default:
        next_stage = APP_JOY_CAL_STAGE_ERROR;
        break;
    }

    if (next_stage == APP_JOY_CAL_STAGE_SWEEP)
    {
      if (AppSensorJoyCalApplyDirectionalSolve() == 0U)
      {
        AppSensorJoyCalRestoreSnapshot();
        g_sensor_joy_cal_active = 0UL;
        g_sensor_joy_cal_capture.active = 0U;
        g_sensor_joy_cal_stage = (ULONG)APP_JOY_CAL_STAGE_ERROR;
        g_sensor_joy_cal_wait_confirm = 0UL;
        g_sensor_joy_cal_status.stage = (ULONG)APP_JOY_CAL_STAGE_ERROR;
        g_sensor_joy_cal_status.progress = 0.0f;
        g_sensor_joy_cal_status.last_error = -506L;
        return;
      }

      TMAGJoy_CalExtents_Begin(g_sensor_joy,
                               (uint32_t)KNOB_SENSOR_JOY_CAL_SWEEP_WINDOW_MS,
                               (uint32_t)KNOB_SENSOR_JOY_CAL_SWEEP_STEP_MS);
      g_sensor_joy_cal_stage = (ULONG)APP_JOY_CAL_STAGE_SWEEP;
      g_sensor_joy_cal_wait_confirm = 0UL;
      g_sensor_joy_cal_status.stage = (ULONG)APP_JOY_CAL_STAGE_SWEEP;
      g_sensor_joy_cal_status.progress = 0.0f;
      return;
    }

    g_sensor_joy_cal_stage = (ULONG)next_stage;
    g_sensor_joy_cal_wait_confirm = 1UL;
    g_sensor_joy_cal_status.stage = (ULONG)next_stage;
    g_sensor_joy_cal_status.progress = 0.0f;
    return;
  }

  if (g_sensor_joy_cal_stage == (ULONG)APP_JOY_CAL_STAGE_SWEEP)
  {
    TMAGJoy_Cal final_cal = {0};

    done = TMAGJoy_CalExtents_Step(g_sensor_joy, now_ms, &progress);
    g_sensor_joy_cal_status.progress = progress;
    g_sensor_joy_cal_status.stage = (ULONG)APP_JOY_CAL_STAGE_SWEEP;
    if (done)
    {
      TMAGJoy_GetCal(g_sensor_joy, &final_cal);
      if (AppSensorJoyCalSane(&final_cal) == 0U)
      {
        AppSensorJoyCalRestoreSnapshot();
        g_sensor_joy_cal_active = 0UL;
        g_sensor_joy_cal_capture.active = 0U;
        g_sensor_joy_cal_stage = (ULONG)APP_JOY_CAL_STAGE_ERROR;
        g_sensor_joy_cal_wait_confirm = 0UL;
        g_sensor_joy_cal_status.stage = (ULONG)APP_JOY_CAL_STAGE_ERROR;
        g_sensor_joy_cal_status.progress = 0.0f;
        g_sensor_joy_cal_status.last_error = -510L;
        return;
      }

      {
        float thr_x = 0.0f;
        float thr_y = 0.0f;
        uint8_t deadzone_en = 0U;
        float deadzone_mT = 0.0f;

        TMAGJoy_GetThresholds(g_sensor_joy, &thr_x, &thr_y);
        TMAGJoy_GetAbsDeadzone(g_sensor_joy, &deadzone_en, &deadzone_mT);

        g_sensor_joy_live_status.dir = (ULONG)TMAGJOY_NEUTRAL;
        g_sensor_joy_live_status.input_mask = 0UL;
        g_sensor_joy_live_status.nx = 0.0f;
        g_sensor_joy_live_status.ny = 0.0f;
        g_sensor_joy_live_status.r_abs_mT = 0.0f;
        g_sensor_joy_live_status.center_x_mT = final_cal.cx;
        g_sensor_joy_live_status.center_y_mT = final_cal.cy;
        g_sensor_joy_live_status.span_x_mT = final_cal.sx;
        g_sensor_joy_live_status.span_y_mT = final_cal.sy;
        g_sensor_joy_live_status.rotation_deg = final_cal.rot_deg;
        g_sensor_joy_live_status.invert_x = final_cal.invert_x;
        g_sensor_joy_live_status.invert_y = final_cal.invert_y;
        g_sensor_joy_live_status.threshold_x_mT = thr_x;
        g_sensor_joy_live_status.threshold_y_mT = thr_y;
        g_sensor_joy_live_status.deadzone_enabled = deadzone_en;
        g_sensor_joy_live_status.deadzone_mT = deadzone_mT;
      }

      g_sensor_joy_cal_active = 0UL;
      g_sensor_joy_cal_capture.active = 0U;
      g_sensor_joy_cal_stage = (ULONG)APP_JOY_CAL_STAGE_DONE;
      g_sensor_joy_cal_wait_confirm = 0UL;
      g_sensor_joy_cal_status.stage = (ULONG)APP_JOY_CAL_STAGE_DONE;
      g_sensor_joy_cal_status.progress = 1.0f;
      AppSensorJoyCalComputeQuality(&final_cal);
      g_sensor_joy_input_gate_valid = 1UL;
      AppSensorJoySeedNeutralArm();
    }
  }
}

static ULONG AppSensorJoyDirMask(TMAGJoy_Dir dir)
{
  ULONG prev_mask = g_sensor_joy_input_mask;
  ULONG bit_up = AppInputSourceBit(APP_INPUT_SOURCE_JOY_UP);
  ULONG bit_right = AppInputSourceBit(APP_INPUT_SOURCE_JOY_RIGHT);
  ULONG bit_down = AppInputSourceBit(APP_INPUT_SOURCE_JOY_DOWN);
  ULONG bit_left = AppInputSourceBit(APP_INPUT_SOURCE_JOY_LEFT);
  ULONG mask = 0UL;

  switch (dir)
  {
    case TMAGJOY_UP:
      mask = bit_up;
      break;

    case TMAGJOY_UPRIGHT:
      if ((prev_mask & bit_up) != 0UL)
      {
        mask = bit_up;
      }
      else if ((prev_mask & bit_right) != 0UL)
      {
        mask = bit_right;
      }
      else if (g_sensor_joy != TX_NULL)
      {
        TMAGJoy_Sample s = TMAGJoy_ReadAnalog(g_sensor_joy);
        mask = (fabsf(s.nx) >= fabsf(s.ny)) ? bit_right : bit_up;
      }
      else
      {
        mask = bit_up;
      }
      break;

    case TMAGJOY_RIGHT:
      mask = bit_right;
      break;

    case TMAGJOY_DOWNRIGHT:
      if ((prev_mask & bit_down) != 0UL)
      {
        mask = bit_down;
      }
      else if ((prev_mask & bit_right) != 0UL)
      {
        mask = bit_right;
      }
      else if (g_sensor_joy != TX_NULL)
      {
        TMAGJoy_Sample s = TMAGJoy_ReadAnalog(g_sensor_joy);
        mask = (fabsf(s.nx) >= fabsf(s.ny)) ? bit_right : bit_down;
      }
      else
      {
        mask = bit_down;
      }
      break;

    case TMAGJOY_DOWN:
      mask = bit_down;
      break;

    case TMAGJOY_DOWNLEFT:
      if ((prev_mask & bit_down) != 0UL)
      {
        mask = bit_down;
      }
      else if ((prev_mask & bit_left) != 0UL)
      {
        mask = bit_left;
      }
      else if (g_sensor_joy != TX_NULL)
      {
        TMAGJoy_Sample s = TMAGJoy_ReadAnalog(g_sensor_joy);
        mask = (fabsf(s.nx) >= fabsf(s.ny)) ? bit_left : bit_down;
      }
      else
      {
        mask = bit_down;
      }
      break;

    case TMAGJOY_LEFT:
      mask = bit_left;
      break;

    case TMAGJOY_UPLEFT:
      if ((prev_mask & bit_up) != 0UL)
      {
        mask = bit_up;
      }
      else if ((prev_mask & bit_left) != 0UL)
      {
        mask = bit_left;
      }
      else if (g_sensor_joy != TX_NULL)
      {
        TMAGJoy_Sample s = TMAGJoy_ReadAnalog(g_sensor_joy);
        mask = (fabsf(s.nx) >= fabsf(s.ny)) ? bit_left : bit_up;
      }
      else
      {
        mask = bit_up;
      }
      break;

    case TMAGJOY_NEUTRAL:
    default:
      break;
  }

  return mask;
}

static void AppSensorJoySeedNeutralArm(void)
{
  float nx = 0.0f;
  float ny = 0.0f;
  float r_norm;
  float arm_thresh;

  g_sensor_joy_input_neutral_armed = 0UL;
  g_sensor_joy_input_neutral_stable_count = 0UL;

  if ((g_sensor_joy == TX_NULL) ||
      (g_sensor_joy_input_gate_valid == 0UL) ||
      (g_sensor_tmag.state != (ULONG)APP_SENSOR_STATE_READY))
  {
    return;
  }

  if (TMAGJoy_ReadCalibratedRaw(g_sensor_joy, &nx, &ny, TX_NULL) != 0)
  {
    return;
  }

  r_norm = sqrtf((nx * nx) + (ny * ny));
  if (r_norm > 1.0f)
  {
    r_norm = 1.0f;
  }

  arm_thresh = g_sensor_joy->cfg.digital_thresh_norm * 0.45f;
  if (arm_thresh < 0.15f)
  {
    arm_thresh = 0.15f;
  }
  if (arm_thresh > 0.35f)
  {
    arm_thresh = 0.35f;
  }

  if (r_norm <= arm_thresh)
  {
    g_sensor_joy_input_neutral_armed = 1UL;
    g_sensor_joy_input_neutral_stable_count = 4UL;
  }
}

static VOID AppSensorJoyInputUpdate(uint8_t enabled)
{
  ULONG prev_mask = g_sensor_joy_input_mask;
  ULONG new_mask = 0UL;
  ULONG changed_mask;
  uint8_t live_ok;
  ULONG now_tick = (ULONG)HAL_GetTick();
  ULONG release_stable_needed = (ULONG)KNOB_SENSOR_JOY_RELEASE_STABLE_SAMPLES;
  ULONG bit_up = AppInputSourceBit(APP_INPUT_SOURCE_JOY_UP);
  ULONG bit_right = AppInputSourceBit(APP_INPUT_SOURCE_JOY_RIGHT);
  ULONG bit_down = AppInputSourceBit(APP_INPUT_SOURCE_JOY_DOWN);
  ULONG bit_left = AppInputSourceBit(APP_INPUT_SOURCE_JOY_LEFT);
  TMAGJoy_Dir dir = TMAGJOY_NEUTRAL;

  if ((enabled != 0U) &&
      (g_sensor_joy != TX_NULL) &&
      (g_sensor_tmag.state == (ULONG)APP_SENSOR_STATE_READY))
  {
    dir = TMAGJoy_ReadDigital(g_sensor_joy);
    new_mask = AppSensorJoyDirMask(dir);
  }

  live_ok = AppSensorJoyLiveUpdate(dir, new_mask, enabled);
  if ((live_ok == 0U) &&
      (enabled != 0U) &&
      (g_sensor_joy_input_gate_valid != 0UL))
  {
    /*
     * Transient live-read miss: keep digital path running so neutral-arm can
     * recover instead of deadlocking at gate_valid=1 + neutral_armed=0.
     */
    g_sensor_joy_release_stable_count = 0UL;
    if (new_mask == 0UL)
    {
      new_mask = prev_mask;
    }
  }

  if ((enabled != 0U) &&
      (g_sensor_joy_input_gate_valid != 0UL) &&
      (g_sensor_joy != TX_NULL) &&
      (g_sensor_tmag.state == (ULONG)APP_SENSOR_STATE_READY))
  {
    if (g_sensor_joy_input_neutral_armed == 0UL)
    {
      float nx = g_sensor_joy_live_status.nx;
      float ny = g_sensor_joy_live_status.ny;
      float r_norm = sqrtf((nx * nx) + (ny * ny));
      float arm_thresh = g_sensor_joy->cfg.digital_thresh_norm * 0.45f;

      if (arm_thresh < 0.15f)
      {
        arm_thresh = 0.15f;
      }
      if (arm_thresh > 0.35f)
      {
        arm_thresh = 0.35f;
      }

      if (r_norm <= arm_thresh)
      {
        if (g_sensor_joy_input_neutral_stable_count < 0xFFFFFFFFUL)
        {
          g_sensor_joy_input_neutral_stable_count++;
        }
      }
      else
      {
        g_sensor_joy_input_neutral_stable_count = 0UL;
      }

      if (g_sensor_joy_input_neutral_stable_count >= 4UL)
      {
        g_sensor_joy_input_neutral_armed = 1UL;
      }
    }

    if (g_sensor_joy_input_neutral_armed == 0UL)
    {
      new_mask = 0UL;
      AppSensorJoyLiveUpdate(TMAGJOY_NEUTRAL, 0UL, 0U);
    }
  }
  else
  {
    if ((enabled == 0U) || (g_sensor_joy_input_gate_valid == 0UL) || (g_sensor_joy == TX_NULL))
    {
      g_sensor_joy_input_neutral_armed = 0UL;
      g_sensor_joy_input_neutral_stable_count = 0UL;
      g_sensor_joy_release_stable_count = 0UL;
    }
    else
    {
      /* Keep neutral arm state across transient not-ready periods. */
      new_mask = prev_mask;
      g_sensor_joy_release_stable_count = 0UL;
    }
  }

  if (release_stable_needed == 0UL)
  {
    release_stable_needed = 1UL;
  }

  if ((enabled != 0U) &&
      (g_sensor_joy_input_gate_valid != 0UL) &&
      (g_sensor_joy != TX_NULL) &&
      (g_sensor_tmag.state == (ULONG)APP_SENSOR_STATE_READY))
  {
    if ((prev_mask != 0UL) && (new_mask == 0UL))
    {
      if (g_sensor_joy_release_stable_count < 0xFFFFFFFFUL)
      {
        g_sensor_joy_release_stable_count++;
      }

      if (g_sensor_joy_release_stable_count < release_stable_needed)
      {
        new_mask = prev_mask;
      }
    }
    else
    {
      g_sensor_joy_release_stable_count = 0UL;
    }
  }
  else
  {
    g_sensor_joy_release_stable_count = 0UL;
  }

  changed_mask = (prev_mask ^ new_mask);
  if (changed_mask == 0UL)
  {
    return;
  }

  if ((changed_mask & bit_up) != 0UL)
  {
    if ((new_mask & bit_up) != 0UL)
    {
      AppInputPostRawEvent(APP_INPUT_SOURCE_JOY_UP, APP_INPUT_EDGE_LOW, 0UL, now_tick);
    }
    else
    {
      AppInputPostRawEvent(APP_INPUT_SOURCE_JOY_UP, APP_INPUT_EDGE_HIGH, 1UL, now_tick);
    }
  }

  if ((changed_mask & bit_right) != 0UL)
  {
    if ((new_mask & bit_right) != 0UL)
    {
      AppInputPostRawEvent(APP_INPUT_SOURCE_JOY_RIGHT, APP_INPUT_EDGE_LOW, 0UL, now_tick);
    }
    else
    {
      AppInputPostRawEvent(APP_INPUT_SOURCE_JOY_RIGHT, APP_INPUT_EDGE_HIGH, 1UL, now_tick);
    }
  }

  if ((changed_mask & bit_down) != 0UL)
  {
    if ((new_mask & bit_down) != 0UL)
    {
      AppInputPostRawEvent(APP_INPUT_SOURCE_JOY_DOWN, APP_INPUT_EDGE_LOW, 0UL, now_tick);
    }
    else
    {
      AppInputPostRawEvent(APP_INPUT_SOURCE_JOY_DOWN, APP_INPUT_EDGE_HIGH, 1UL, now_tick);
    }
  }

  if ((changed_mask & bit_left) != 0UL)
  {
    if ((new_mask & bit_left) != 0UL)
    {
      AppInputPostRawEvent(APP_INPUT_SOURCE_JOY_LEFT, APP_INPUT_EDGE_LOW, 0UL, now_tick);
    }
    else
    {
      AppInputPostRawEvent(APP_INPUT_SOURCE_JOY_LEFT, APP_INPUT_EDGE_HIGH, 1UL, now_tick);
    }
  }

  g_sensor_joy_input_mask = new_mask;
}

static void AppSensorJoyCalRequestSave(void)
{
  UINT status = TX_NOT_DONE;

  if (g_sensor_joy_cal_stage != (ULONG)APP_JOY_CAL_STAGE_DONE)
  {
    AppSensorJoyCalCancel();
    return;
  }

  g_sensor_joy_cal_status.save_pending = 1UL;
  g_sensor_joy_cal_status.last_error = 0L;
  status = AppSensorSettingsSaveRequest();
  if (status != TX_SUCCESS)
  {
    g_sensor_joy_cal_status.save_pending = 0UL;
    g_sensor_joy_cal_status.save_fail_count++;
    if (g_sensor_joy_cal_status.last_error == 0L)
    {
      g_sensor_joy_cal_status.last_error = -503L;
    }
  }
}

static UINT AppSensorSettingsSaveRequest(void)
{
  UINT status;
  uint8_t deadzone_en = 0U;
  float deadzone_mT = 0.0f;

  if (g_sensor_joy == TX_NULL)
  {
    g_sensor_joy_cal_status.last_error = -502L;
    return TX_NOT_DONE;
  }

  TMAGJoy_GetCal(g_sensor_joy, &g_storage_joycfg_cal);
  if (AppSensorJoyCalSane(&g_storage_joycfg_cal) == 0U)
  {
    g_sensor_joy_cal_status.last_error = APP_STORAGE_ERR_JOYCFG_INVALID;
    return TX_NOT_DONE;
  }

  TMAGJoy_GetAbsDeadzone(g_sensor_joy, &deadzone_en, &deadzone_mT);
  g_storage_joycfg_deadzone_enabled = (deadzone_en != 0U) ? 1UL : 0UL;
  g_storage_joycfg_deadzone_mT = AppStorageDeadzoneClampMt(deadzone_mT);
  g_storage_joycfg_valid = 1UL;

  status = AppStorageReqPost(APP_STORAGE_REQ_JOYCFG_SAVE, 0UL);
  if (status != TX_SUCCESS)
  {
    g_sensor_joy_cal_status.last_error = -503L;
  }
  return status;
}

static VOID AppSensorHandleModeChange(app_mode_t mode_token)
{
  ULONG prev_mode_token = g_sensor_mode_token;

  switch (mode_token)
  {
    case APP_MODE_FLASHING:
      g_sensor_mode_token = (ULONG)APP_MODE_FLASHING;
      g_sensor_lis_stream_enabled = 0UL;
      g_sensor_lis_profile_requested = APP_SENSOR_LIS_PROFILE_LOW_POWER;
      if ((g_sensor_joy_cal_active != 0UL) ||
          (g_sensor_joy_cal_wait_confirm != 0UL) ||
          ((g_sensor_joy_cal_stage >= (ULONG)APP_JOY_CAL_STAGE_NEUTRAL) &&
           (g_sensor_joy_cal_stage <= (ULONG)APP_JOY_CAL_STAGE_SWEEP)))
      {
        AppSensorJoyCalRestoreSnapshot();
      }
      g_sensor_joy_cal_active = 0UL;
      g_sensor_joy_cal_capture.active = 0U;
      g_sensor_joy_cal_stage = (ULONG)APP_JOY_CAL_STAGE_IDLE;
      g_sensor_joy_cal_wait_confirm = 0UL;
      g_sensor_joy_cal_status.stage = (ULONG)APP_JOY_CAL_STAGE_IDLE;
      g_sensor_joy_cal_status.progress = 0.0f;
      g_sensor_joy_cal_status.save_pending = 0UL;
      AppSensorJoyCalQualityReset();
      AppSensorJoyInputUpdate(0U);
      AppSensorMarkAllSuspended(1U);
      (void)AppSensorHealthFlagsPublish();
      break;

    case APP_MODE_STATIC:
    case APP_MODE_STOP:
      g_sensor_mode_token = (ULONG)mode_token;
      g_sensor_lis_stream_enabled = 0UL;
      g_sensor_lis_profile_requested = APP_SENSOR_LIS_PROFILE_LOW_POWER;
      if (prev_mode_token == (ULONG)APP_MODE_FLASHING)
      {
        AppSensorRunResumeSequence();
      }
      else
      {
        AppSensorLisApplyRequestedProfileNow();
      }
      break;

    case APP_MODE_REALTIME:
      g_sensor_mode_token = (ULONG)APP_MODE_REALTIME;
      g_sensor_lis_stream_enabled = 0UL;
      g_sensor_lis_profile_requested = APP_SENSOR_LIS_PROFILE_LOW_POWER;
      if ((g_sensor_joy_cal_active != 0UL) ||
          (g_sensor_joy_cal_wait_confirm != 0UL) ||
          ((g_sensor_joy_cal_stage >= (ULONG)APP_JOY_CAL_STAGE_NEUTRAL) &&
           (g_sensor_joy_cal_stage <= (ULONG)APP_JOY_CAL_STAGE_SWEEP)))
      {
        AppSensorJoyCalRestoreSnapshot();
      }
      g_sensor_joy_cal_active = 0UL;
      g_sensor_joy_cal_capture.active = 0U;
      g_sensor_joy_cal_stage = (ULONG)APP_JOY_CAL_STAGE_IDLE;
      g_sensor_joy_cal_wait_confirm = 0UL;
      g_sensor_joy_cal_status.stage = (ULONG)APP_JOY_CAL_STAGE_IDLE;
      g_sensor_joy_cal_status.progress = 0.0f;
      g_sensor_joy_cal_status.save_pending = 0UL;
      AppSensorJoyCalQualityReset();
      AppSensorJoyInputUpdate((g_sensor_joy_input_gate_valid != 0UL) ? 1U : 0U);
      AppSensorLisApplyRequestedProfileNow();
      break;

    default:
      break;
  }
}

static uint8_t AppSensorRecoveryNeeded(void)
{
  if ((g_sensor_pmic.state != (ULONG)APP_SENSOR_STATE_READY) &&
      (g_sensor_pmic.state != (ULONG)APP_SENSOR_STATE_SUSPENDED))
  {
    return 1U;
  }

  if ((g_sensor_tmag.state != (ULONG)APP_SENSOR_STATE_READY) &&
      (g_sensor_tmag.state != (ULONG)APP_SENSOR_STATE_SUSPENDED))
  {
    return 1U;
  }

  if ((g_sensor_lis.state != (ULONG)APP_SENSOR_STATE_READY) &&
      (g_sensor_lis.state != (ULONG)APP_SENSOR_STATE_SUSPENDED))
  {
    return 1U;
  }

  return 0U;
}

static uint8_t AppSensorModeHasAny(ULONG mode_mask)
{
  ULONG mode_flags = 0UL;

  if (App_ModeFlags_Get(&mode_flags) != TX_SUCCESS)
  {
    return 0U;
  }

  return ((mode_flags & mode_mask) != 0UL) ? 1U : 0U;
}

static uint8_t AppSensorAutoRecoveryAllowed(void)
{
  ULONG power_flags = 0UL;
  ULONG mode_flags = 0UL;

  if (App_PowerFlags_Get(&power_flags) != TX_SUCCESS)
  {
    return 0U;
  }

  if (App_ModeFlags_Get(&mode_flags) != TX_SUCCESS)
  {
    return 0U;
  }

  if ((power_flags & APP_POWER_FLAG_RUNNING) == 0UL)
  {
    return 0U;
  }

  if ((power_flags & (APP_POWER_FLAG_QUIESCE_REQ | APP_POWER_FLAG_QUIESCED)) != 0UL)
  {
    return 0U;
  }

  if ((mode_flags & (APP_MODE_FLAG_REALTIME | APP_MODE_FLAG_FLASHING)) != 0UL)
  {
    return 0U;
  }

  if ((mode_flags & (APP_MODE_FLAG_STOP | APP_MODE_FLAG_STATIC)) == 0UL)
  {
    return 0U;
  }

  return 1U;
}

UINT App_Display_InvalidateAll(void)
{
  return AppDisplayCmdPost(APP_DISPLAY_CMD_INVALIDATE_ALL, 0UL);
}

UINT App_Display_Present(void)
{
  return AppDisplayCmdPost(APP_DISPLAY_CMD_PRESENT, 0UL);
}

static VOID AppStorageUsbMscSignal(UINT status, ULONG media_status)
{
  g_storage_usb_msc_status = status;
  g_storage_usb_msc_media_status = media_status;
  (void)tx_semaphore_put(&g_sem_storage_usb_msc_done);
}

static VOID AppStorageUsbMscFlagRecover(ULONG reason,
                                        app_storage_req_type_t req_type,
                                        UINT req_status,
                                        ULONG media_status,
                                        ULONG mode_flags)
{
  uint8_t was_pending;

  if (((mode_flags & APP_MODE_FLAG_FLASHING) == 0UL) ||
      (g_usb_device_active == 0UL) ||
      (g_sensor_pmic_vbus_present == 0UL))
  {
    return;
  }

  was_pending = (g_usb_msc_recover_pending != 0UL) ? 1U : 0U;
  g_usb_msc_recover_reason = reason;
  g_usb_msc_recover_last_req_type = (ULONG)req_type;
  g_usb_msc_recover_last_req_status = (ULONG)req_status;
  g_usb_msc_recover_last_media_status = media_status;
  g_usb_msc_recover_last_mode_flags = mode_flags;
  g_usb_msc_recover_last_usb_active_before = g_usb_device_active;
  g_usb_msc_recover_pending = 1UL;
  if (g_usb_msc_recover_trigger_count < 0xFFFFFFFFUL)
  {
    g_usb_msc_recover_trigger_count++;
  }

  /* Trigger immediate recovery handling in thPower (idle-path remains fallback). */
  if (was_pending == 0U)
  {
    (void)AppSysEventPost(APP_SYS_EVT_USB_MSC_RECOVER, reason, (ULONG)req_type, (ULONG)req_status);
  }
}

static VOID AppStorageUsbMscTraceBegin(app_storage_req_type_t req_type, ULONG lba, ULONG number_blocks)
{
  g_storage_usb_msc_last_req_type = (ULONG)req_type;
  g_storage_usb_msc_last_lba = lba;
  g_storage_usb_msc_last_blocks = number_blocks;

  switch (req_type)
  {
    case APP_STORAGE_REQ_USB_MSC_READ:
      if (g_storage_usb_msc_req_read_count < 0xFFFFFFFFUL)
      {
        g_storage_usb_msc_req_read_count++;
      }
      break;

    case APP_STORAGE_REQ_USB_MSC_WRITE:
      if (g_storage_usb_msc_req_write_count < 0xFFFFFFFFUL)
      {
        g_storage_usb_msc_req_write_count++;
      }
      break;

    case APP_STORAGE_REQ_USB_MSC_FLUSH:
      if (g_storage_usb_msc_req_flush_count < 0xFFFFFFFFUL)
      {
        g_storage_usb_msc_req_flush_count++;
      }
      break;

    case APP_STORAGE_REQ_USB_MSC_STATUS:
      if (g_storage_usb_msc_req_status_count < 0xFFFFFFFFUL)
      {
        g_storage_usb_msc_req_status_count++;
      }
      break;

    default:
      break;
  }
}

static VOID AppStorageUsbMscTraceEnd(UINT req_status, ULONG media_status)
{
  g_storage_usb_msc_last_req_status = (ULONG)req_status;
  g_storage_usb_msc_last_media_status = media_status;

  if (req_status != TX_SUCCESS)
  {
    if (g_storage_usb_msc_req_fail_count < 0xFFFFFFFFUL)
    {
      g_storage_usb_msc_req_fail_count++;
    }
  }
}

static UINT AppStorageUsbMscRequest(app_storage_req_type_t req_type, ULONG arg0, ULONG arg1, ULONG arg2, ULONG *media_status_out)
{
  UINT status;
  UINT req_status = TX_NOT_DONE;
  ULONG media_status = 1UL;
  app_storage_req_t msg = {0UL};
  ULONG mode_flags = 0UL;

  status = tx_mutex_get(&g_mtx_storage_usb_msc, APP_USB_MSC_MUTEX_WAIT_TICKS);
  if (status != TX_SUCCESS)
  {
    if (g_storage_usb_msc_req_mutex_fail_count < 0xFFFFFFFFUL)
    {
      g_storage_usb_msc_req_mutex_fail_count++;
    }
    if (media_status_out != TX_NULL)
    {
      *media_status_out = media_status;
    }
    return status;
  }

  while (tx_semaphore_get(&g_sem_storage_usb_msc_done, TX_NO_WAIT) == TX_SUCCESS)
  {
    /* Drain stale completion tokens before posting a new request. */
  }

  g_storage_usb_msc_status = TX_NOT_DONE;
  g_storage_usb_msc_media_status = 1UL;

  mode_flags = (g_eg_mode.tx_event_flags_group_current & APP_MODE_FLAGS_ALL);
  if (((mode_flags & APP_MODE_FLAG_FLASHING) != 0UL) &&
      (AppStorageReqAllowedInFlashing(req_type) == 0U))
  {
    if (g_storage_usb_msc_req_mode_reject_count < 0xFFFFFFFFUL)
    {
      g_storage_usb_msc_req_mode_reject_count++;
    }
    status = TX_NOT_DONE;
  }
  else
  {
    msg.type = (ULONG)req_type;
    msg.arg0 = arg0;
    msg.arg1 = arg1;
    msg.arg2 = arg2;
    status = tx_queue_send(&g_q_storage_req, &msg, APP_USB_MSC_QUEUE_WAIT_TICKS);
    if (status != TX_SUCCESS)
    {
      if (g_storage_usb_msc_req_queue_fail_count < 0xFFFFFFFFUL)
      {
        g_storage_usb_msc_req_queue_fail_count++;
      }
      AppStorageUsbMscFlagRecover(APP_USB_MSC_RECOVER_REASON_QUEUE_FAIL,
                                  req_type,
                                  status,
                                  media_status,
                                  mode_flags);
    }
  }

  if (status == TX_SUCCESS)
  {
    status = tx_semaphore_get(&g_sem_storage_usb_msc_done, APP_USB_MSC_DONE_WAIT_TICKS);
    if (status == TX_SUCCESS)
    {
      req_status = g_storage_usb_msc_status;
      media_status = g_storage_usb_msc_media_status;
    }
    else
    {
      if (g_storage_usb_msc_req_done_fail_count < 0xFFFFFFFFUL)
      {
        g_storage_usb_msc_req_done_fail_count++;
      }
      AppStorageUsbMscFlagRecover(APP_USB_MSC_RECOVER_REASON_DONE_TIMEOUT,
                                  req_type,
                                  status,
                                  media_status,
                                  mode_flags);
      req_status = TX_NOT_DONE;
      media_status = 1UL;
    }
  }
  else
  {
    req_status = status;
    media_status = 1UL;
  }

  (void)tx_mutex_put(&g_mtx_storage_usb_msc);

  if (media_status_out != TX_NULL)
  {
    *media_status_out = media_status;
  }

  return req_status;
}

UINT App_StorageReq_FlashProbe(void)
{
  return AppStorageReqPost(APP_STORAGE_REQ_FLASH_PROBE, 0UL);
}

UINT App_StorageReq_RawSmoke(void)
{
  return AppStorageReqPost(APP_STORAGE_REQ_RAW_SMOKE, 0UL);
}

UINT App_StorageReq_FileXMount(void)
{
  return AppStorageReqPost(APP_STORAGE_REQ_FILEX_MOUNT, 0UL);
}

UINT App_StorageReq_FileXFormat(void)
{
  return AppStorageReqPost(APP_STORAGE_REQ_FILEX_FORMAT, 0UL);
}

UINT App_StorageReq_FileXUnmount(void)
{
  return AppStorageReqPost(APP_STORAGE_REQ_FILEX_UNMOUNT, 0UL);
}

UINT App_StorageReq_JoyCfgLoad(void)
{
  return AppStorageReqPost(APP_STORAGE_REQ_JOYCFG_LOAD, 0UL);
}

UINT App_StorageReq_JoyCfgSave(void)
{
  return AppStorageReqPost(APP_STORAGE_REQ_JOYCFG_SAVE, 0UL);
}

UINT App_StorageReq_AudioCatalogLoad(ULONG catalog_addr)
{
  return AppStorageReqPostEx(APP_STORAGE_REQ_AUDIO_CATALOG_LOAD, catalog_addr, 0UL, 0UL);
}

UINT App_StorageReq_AudioChunkRead(ULONG addr, ULONG len, ULONG token)
{
  return AppStorageReqPostEx(APP_STORAGE_REQ_AUDIO_CHUNK_READ, addr, len, token);
}

UINT App_StorageReq_AudioCatalogInstallEmbedded(void)
{
  return AppStorageReqPost(APP_STORAGE_REQ_AUDIO_CATALOG_INSTALL_EMBEDDED, 0UL);
}

UINT App_StorageReq_AudioCatalogInstallManifestRefs(void)
{
  return AppStorageReqPost(APP_STORAGE_REQ_AUDIO_CATALOG_INSTALL_MANIFEST_REFS, 0UL);
}

UINT App_StorageReq_GamePackageManifestLoad(ULONG manifest_addr, ULONG manifest_size)
{
  return AppStorageReqPostEx(APP_STORAGE_REQ_GAME_PACKAGE_MANIFEST_LOAD, manifest_addr, manifest_size, 0UL);
}

UINT App_StorageReq_GamePackageManifestLoadDefault(void)
{
  return AppStorageReqPost(APP_STORAGE_REQ_GAME_PACKAGE_MANIFEST_LOAD_DEFAULT, 0UL);
}

UINT App_StorageReq_GamePackageManifestErase(void)
{
  return AppStorageReqPost(APP_STORAGE_REQ_GAME_PACKAGE_MANIFEST_ERASE, 0UL);
}

UINT App_StorageReq_GamePackageManifestImportFat(void)
{
  return AppStorageReqPost(APP_STORAGE_REQ_GAME_PACKAGE_MANIFEST_IMPORT_FAT, 0UL);
}

UINT App_StorageReq_GamePackageSceneImportFat(void)
{
  return AppStorageReqPost(APP_STORAGE_REQ_GAME_PACKAGE_SCENE_IMPORT_FAT, 0UL);
}

UINT App_StorageReq_RawAppErase(void)
{
  return AppStorageReqPost(APP_STORAGE_REQ_RAW_APP_ERASE, 0UL);
}

UINT App_StorageReq_GamePackageManifestWriteTest(void)
{
  return AppStorageReqPost(APP_STORAGE_REQ_GAME_PACKAGE_MANIFEST_WRITE_TEST, 0UL);
}

UINT App_StorageReq_SceneMapLoad(ULONG map_addr, ULONG map_size)
{
  return AppStorageReqPostEx(APP_STORAGE_REQ_SCENE_MAP_LOAD, map_addr, map_size, 0UL);
}

UINT App_StorageReq_SceneTilesetLoad(ULONG tileset_addr, ULONG tileset_size)
{
  return AppStorageReqPostEx(APP_STORAGE_REQ_SCENE_TILESET_LOAD, tileset_addr, tileset_size, 0UL);
}

UINT App_StorageSceneLoadStatusGet(app_storage_scene_load_status_t *status_out)
{
  if (status_out == TX_NULL)
  {
    return TX_PTR_ERROR;
  }

  status_out->map_ok_count = g_storage_scene_map_load_ok_count;
  status_out->map_fail_count = g_storage_scene_map_load_fail_count;
  status_out->map_last_status = g_storage_scene_map_last_status;
  status_out->map_loaded = g_storage_scene_map_loaded;
  status_out->tileset_ok_count = g_storage_scene_tileset_load_ok_count;
  status_out->tileset_fail_count = g_storage_scene_tileset_load_fail_count;
  status_out->tileset_last_status = g_storage_scene_tileset_last_status;
  status_out->tileset_loaded = g_storage_scene_tileset_loaded;

  return TX_SUCCESS;
}

UINT App_StorageReq_UsbMscRead(ULONG lba, ULONG number_blocks, uint8_t *data_pointer, ULONG *media_status_out)
{
  ULONG media_status_local = 1UL;
  ULONG *media_status_ptr = media_status_out;
  UINT req_status;

  if (media_status_ptr == TX_NULL)
  {
    media_status_ptr = &media_status_local;
  }

  AppStorageUsbMscTraceBegin(APP_STORAGE_REQ_USB_MSC_READ, lba, number_blocks);
  if (data_pointer == TX_NULL)
  {
    *media_status_ptr = 1UL;
    AppStorageUsbMscTraceEnd(TX_PTR_ERROR, *media_status_ptr);
    return TX_PTR_ERROR;
  }

  req_status = AppStorageUsbMscRequest(APP_STORAGE_REQ_USB_MSC_READ,
                                       (ULONG)(uintptr_t)data_pointer,
                                       lba,
                                       number_blocks,
                                       media_status_ptr);
  AppStorageUsbMscTraceEnd(req_status, *media_status_ptr);
  return req_status;
}

UINT App_StorageReq_UsbMscWrite(ULONG lba, ULONG number_blocks, uint8_t *data_pointer, ULONG *media_status_out)
{
  ULONG media_status_local = 1UL;
  ULONG *media_status_ptr = media_status_out;
  UINT req_status;

  if (media_status_ptr == TX_NULL)
  {
    media_status_ptr = &media_status_local;
  }

  AppStorageUsbMscTraceBegin(APP_STORAGE_REQ_USB_MSC_WRITE, lba, number_blocks);
  if (data_pointer == TX_NULL)
  {
    *media_status_ptr = 1UL;
    AppStorageUsbMscTraceEnd(TX_PTR_ERROR, *media_status_ptr);
    return TX_PTR_ERROR;
  }

  req_status = AppStorageUsbMscRequest(APP_STORAGE_REQ_USB_MSC_WRITE,
                                       (ULONG)(uintptr_t)data_pointer,
                                       lba,
                                       number_blocks,
                                       media_status_ptr);
  AppStorageUsbMscTraceEnd(req_status, *media_status_ptr);
  return req_status;
}

UINT App_StorageReq_UsbMscFlush(ULONG lba, ULONG number_blocks, ULONG *media_status_out)
{
  ULONG media_status_local = 1UL;
  ULONG *media_status_ptr = media_status_out;
  UINT req_status;

  if (media_status_ptr == TX_NULL)
  {
    media_status_ptr = &media_status_local;
  }

  AppStorageUsbMscTraceBegin(APP_STORAGE_REQ_USB_MSC_FLUSH, lba, number_blocks);
  req_status = AppStorageUsbMscRequest(APP_STORAGE_REQ_USB_MSC_FLUSH,
                                       lba,
                                       number_blocks,
                                       0UL,
                                       media_status_ptr);
  AppStorageUsbMscTraceEnd(req_status, *media_status_ptr);
  return req_status;
}

UINT App_StorageReq_UsbMscStatus(ULONG media_id, ULONG *media_status_out)
{
  ULONG media_status_local = 1UL;
  ULONG *media_status_ptr = media_status_out;
  UINT req_status;
  ULONG mode_flags = (g_eg_mode.tx_event_flags_group_current & APP_MODE_FLAGS_ALL);

  if (media_status_ptr == TX_NULL)
  {
    media_status_ptr = &media_status_local;
  }

  AppStorageUsbMscTraceBegin(APP_STORAGE_REQ_USB_MSC_STATUS, media_id, 0UL);

  /*
   * In active FLASHING sessions, hosts may poll status repeatedly and
   * expect prompt completion. Use fast success when media is already known-ready.
   * If not ready, fall through to the owner-thread request path so recovery/open
   * can still occur (avoids sticky "no media" status).
   */
  if (((mode_flags & APP_MODE_FLAG_FLASHING) != 0UL) && (g_usb_device_active != 0UL))
  {
    if ((g_storage_flash_ready != 0UL) && (g_storage_usb_msc_lx_opened != 0UL))
    {
      *media_status_ptr = 0UL;
      AppStorageUsbMscTraceEnd(TX_SUCCESS, *media_status_ptr);
      return TX_SUCCESS;
    }
  }

  req_status = AppStorageUsbMscRequest(APP_STORAGE_REQ_USB_MSC_STATUS,
                                       media_id,
                                       0UL,
                                       0UL,
                                       media_status_ptr);
  AppStorageUsbMscTraceEnd(req_status, *media_status_ptr);
  return req_status;
}

ULONG App_StorageReq_UsbMscGetMediaLastLba(void)
{
  ULONG total_sectors = AppStorageFatTotalSectors();
  return (total_sectors > 0UL) ? (total_sectors - 1UL) : 0UL;
}

ULONG App_StorageReq_UsbMscGetMediaBlockLength(void)
{
  return (ULONG)APP_STORAGE_FAT_BYTES_PER_SECTOR;
}

UINT App_AudioReq_StartTone(void)
{
  return AppAudioCmdPost(APP_AUDIO_CMD_START_TONE, 0UL);
}

UINT App_AudioReq_Stop(void)
{
  return AppAudioCmdPost(APP_AUDIO_CMD_STOP, 0UL);
}

UINT App_AudioReq_PlayEvent(app_audio_event_t event_id)
{
  if ((ULONG)event_id > (ULONG)APP_AUDIO_EVENT_RT_MENU)
  {
    return TX_OPTION_ERROR;
  }

  return AppAudioCmdPost(APP_AUDIO_CMD_PLAY_EVENT, (ULONG)event_id);
}

UINT App_AudioReq_PlayAsset(app_audio_asset_id_t asset_id)
{
  return AppAudioCmdPost(APP_AUDIO_CMD_PLAY_CLIP, (ULONG)asset_id);
}

UINT App_AudioReq_PlayClip(app_audio_clip_t clip_id)
{
  return App_AudioReq_PlayAsset((app_audio_asset_id_t)clip_id);
}

UINT App_AudioReq_SetUserGain(app_audio_user_gain_id_t gain_id, ULONG pct)
{
  ULONG clamped_pct = pct;
  ULONG packed;

  if ((gain_id < APP_AUDIO_USER_GAIN_MASTER) || (gain_id > APP_AUDIO_USER_GAIN_UI))
  {
    return TX_OPTION_ERROR;
  }

  if (clamped_pct > APP_STORAGE_USER_GAIN_MAX_PCT)
  {
    clamped_pct = APP_STORAGE_USER_GAIN_MAX_PCT;
  }

  packed = (((ULONG)gain_id & 0xFFFFUL) << 16) | (clamped_pct & 0xFFFFUL);
  return AppAudioCmdPost(APP_AUDIO_CMD_SET_USER_GAIN, packed);
}

UINT App_SensorReq_JoyCalStart(void)
{
  return AppSensorReqPost(APP_SENSOR_REQ_JOY_CAL_START, 0UL, 0UL, 0UL);
}

UINT App_SensorReq_JoyCalSave(void)
{
  return AppSensorReqPost(APP_SENSOR_REQ_JOY_CAL_SAVE, 0UL, 0UL, 0UL);
}

UINT App_SensorReq_JoyCalCancel(void)
{
  return AppSensorReqPost(APP_SENSOR_REQ_JOY_CAL_CANCEL, 0UL, 0UL, 0UL);
}

UINT App_SensorReq_JoyDeadzoneSet(ULONG deadzone_mT_x10)
{
  return AppSensorReqPost(APP_SENSOR_REQ_JOY_DEADZONE_SET, deadzone_mT_x10, 0UL, 0UL);
}

UINT App_SensorReq_SettingsSave(void)
{
  return AppSensorReqPost(APP_SENSOR_REQ_SETTINGS_SAVE, 0UL, 0UL, 0UL);
}

static VOID AppStorageCaptureDebug(void)
{
  g_storage_at25_dbg.last_op = 0U;
  g_storage_at25_dbg.cmd_status = 0U;
  g_storage_at25_dbg.io_status = 0U;
  g_storage_at25_dbg.reserved0 = 0U;
  g_storage_at25_dbg.addr = 0UL;
  g_storage_at25_dbg.nbytes = 0UL;
  g_storage_at25_dbg.hal_error = 0UL;
  g_storage_at25_dbg.seq = 0UL;
  AT25_GetDebug(&g_storage_at25_dbg);
}

static LONG AppStorageHalToError(HAL_StatusTypeDef hal_status)
{
  switch (hal_status)
  {
    case HAL_OK:
      return APP_STORAGE_ERR_NONE;

    case HAL_ERROR:
      return -1L;

    case HAL_BUSY:
      return -2L;

    case HAL_TIMEOUT:
      return -3L;

    default:
      return -4L;
  }
}

static VOID AppStorageOspiClockEnable(void)
{
  if (g_storage_ospi_clock_enabled != 0UL)
  {
    return;
  }
  __HAL_RCC_OSPIM_CLK_ENABLE();
  __HAL_RCC_OSPI1_CLK_ENABLE();
  g_storage_ospi_clock_enabled = 1UL;
}

static VOID AppStorageOspiClockDisable(void)
{
  if (g_storage_ospi_clock_enabled == 0UL)
  {
    return;
  }
  __HAL_RCC_OSPI1_CLK_DISABLE();
  __HAL_RCC_OSPIM_CLK_DISABLE();
  g_storage_ospi_clock_enabled = 0UL;
}

static UINT AppStorageOspiRecover(void)
{
  HAL_StatusTypeDef hal_status;
  OSPIM_CfgTypeDef ospim_cfg = {0};
  HAL_OSPI_DLYB_CfgTypeDef dlyb_cfg = {0};

  (void)HAL_OSPI_Abort(&hospi1);
  (void)HAL_OSPI_DeInit(&hospi1);

  __HAL_RCC_OSPI1_FORCE_RESET();
  __HAL_RCC_OSPIM_FORCE_RESET();
  __HAL_RCC_OSPI1_RELEASE_RESET();
  __HAL_RCC_OSPIM_RELEASE_RESET();

  AppStorageOspiClockEnable();

  /* Re-apply the same OCTOSPI configuration used at boot. */
  hospi1.Instance = OCTOSPI1;
  hospi1.Init.FifoThreshold = 1;
  hospi1.Init.DualQuad = HAL_OSPI_DUALQUAD_DISABLE;
  hospi1.Init.MemoryType = HAL_OSPI_MEMTYPE_MICRON;
  hospi1.Init.DeviceSize = 24;
  hospi1.Init.ChipSelectHighTime = 2;
  hospi1.Init.FreeRunningClock = HAL_OSPI_FREERUNCLK_DISABLE;
  hospi1.Init.ClockMode = HAL_OSPI_CLOCK_MODE_0;
  hospi1.Init.WrapSize = HAL_OSPI_WRAP_NOT_SUPPORTED;
  hospi1.Init.ClockPrescaler = 8;
  hospi1.Init.SampleShifting = HAL_OSPI_SAMPLE_SHIFTING_NONE;
  hospi1.Init.DelayHoldQuarterCycle = HAL_OSPI_DHQC_DISABLE;
  hospi1.Init.ChipSelectBoundary = 0;
  hospi1.Init.DelayBlockBypass = HAL_OSPI_DELAY_BLOCK_BYPASSED;
  hospi1.Init.MaxTran = 0;
  hospi1.Init.Refresh = 0;

  hal_status = HAL_OSPI_Init(&hospi1);
  if (hal_status != HAL_OK)
  {
    return TX_NOT_DONE;
  }

  ospim_cfg.ClkPort = 1;
  ospim_cfg.NCSPort = 2;
  ospim_cfg.IOLowPort = HAL_OSPIM_IOPORT_1_LOW;
  hal_status = HAL_OSPIM_Config(&hospi1, &ospim_cfg, HAL_OSPI_TIMEOUT_DEFAULT_VALUE);
  if (hal_status != HAL_OK)
  {
    return TX_NOT_DONE;
  }

  dlyb_cfg.Units = 0;
  dlyb_cfg.PhaseSel = 0;
  hal_status = HAL_OSPI_DLYB_SetConfig(&hospi1, &dlyb_cfg);
  if (hal_status != HAL_OK)
  {
    return TX_NOT_DONE;
  }

  return TX_SUCCESS;
}

static UINT AppStorageRunFlashResume(void)
{
  HAL_StatusTypeDef hal_status;

  if (g_storage_flash_quiesced == 0UL)
  {
    return TX_SUCCESS;
  }

  AppStorageOspiClockEnable();

  if (g_storage_flash_in_dpd != 0UL)
  {
    hal_status = AT25_ReleaseDeepPowerDown(&hospi1);
    AppStorageCaptureDebug();
    if (hal_status != HAL_OK)
    {
      if (AppStorageOspiRecover() == TX_SUCCESS)
      {
        hal_status = AT25_ReleaseDeepPowerDown(&hospi1);
        AppStorageCaptureDebug();
      }
      if (hal_status != HAL_OK)
      {
        g_storage_flash_ready = 0UL;
        g_storage_last_error = APP_STORAGE_ERR_BOOTINIT;
        return TX_NOT_DONE;
      }
    }
    g_storage_flash_in_dpd = 0UL;
  }

  g_storage_flash_quiesced = 0UL;
  return TX_SUCCESS;
}

static UINT AppStorageRunFlashQuiesce(void)
{
  HAL_StatusTypeDef hal_status;

  if (g_storage_flash_quiesced != 0UL)
  {
    g_storage_flash_ready = 0UL;
    return TX_SUCCESS;
  }

  if (g_storage_flash_ready == 0UL)
  {
    AppStorageOspiClockDisable();
    g_storage_flash_in_dpd = 0UL;
    g_storage_flash_quiesced = 1UL;
    return TX_SUCCESS;
  }

  hal_status = AT25_EnterDeepPowerDown(&hospi1);
  AppStorageCaptureDebug();
  if (hal_status != HAL_OK)
  {
    g_storage_last_error = APP_STORAGE_ERR_BOOTINIT;
    return TX_NOT_DONE;
  }

  AppStorageOspiClockDisable();
  g_storage_flash_in_dpd = 1UL;
  g_storage_flash_quiesced = 1UL;
  g_storage_flash_ready = 0UL;
  return TX_SUCCESS;
}

static VOID AppStorageInstallIndexClearActive(void)
{
  g_storage_install_index_valid = 0UL;
  g_storage_install_index_active_slot = APP_STORAGE_INSTALL_INDEX_SLOT_INVALID;
  g_storage_install_index_sequence = 0UL;
  g_storage_install_index_package_id = 0UL;
  g_storage_install_index_package_version = 0UL;
  g_storage_install_index_blob_offset = 0UL;
  g_storage_install_index_blob_size = 0UL;
  g_storage_install_index_blob_crc32 = 0UL;
  g_storage_install_index_manifest_addr = 0UL;
  g_storage_install_index_manifest_size = 0UL;
  g_storage_install_index_manifest_crc32 = 0UL;
  g_storage_install_index_record_crc32 = 0UL;
}

static uint8_t AppStorageInstallIndexSeqNewer(uint32_t candidate_seq, uint32_t baseline_seq)
{
  if (candidate_seq == baseline_seq)
  {
    return 0U;
  }

  return (((int32_t)(candidate_seq - baseline_seq)) > 0) ? 1U : 0U;
}

static uint8_t AppStorageInstallIndexRecordValidate(const app_storage_install_index_record_t *record)
{
  uint32_t calc_crc;
  uint64_t manifest_end;
  uint64_t blob_end;

  if (record == (const app_storage_install_index_record_t *)0)
  {
    return 0U;
  }

  if ((record->magic != APP_STORAGE_INSTALL_INDEX_MAGIC) ||
      (record->version != APP_STORAGE_INSTALL_INDEX_VERSION) ||
      (record->header_size != (uint16_t)sizeof(app_storage_install_index_record_t)))
  {
    return 0U;
  }

  if ((record->sequence == 0UL) || (record->sequence == 0xFFFFFFFFUL))
  {
    return 0U;
  }

  if ((record->manifest_size == 0UL) || (record->manifest_size > GAME_PACKAGE_MANIFEST_MAX_BYTES))
  {
    return 0U;
  }

  manifest_end = (uint64_t)record->manifest_addr + (uint64_t)record->manifest_size;
  if ((record->manifest_addr >= APP_STORAGE_FLASH_SIZE_BYTES) ||
      (manifest_end > (uint64_t)APP_STORAGE_FLASH_SIZE_BYTES) ||
      (manifest_end < (uint64_t)record->manifest_addr))
  {
    return 0U;
  }

  if (record->blob_size > 0UL)
  {
    blob_end = (uint64_t)record->blob_offset + (uint64_t)record->blob_size;
    if ((blob_end > ((uint64_t)APP_STORAGE_INSTALLED_DATA_END_ADDR - (uint64_t)KNOB_STORAGE_INSTALLED_BASE_ADDR)) ||
        (blob_end < (uint64_t)record->blob_offset))
    {
      return 0U;
    }
  }

  calc_crc = AppStorageCrc32((const uint8_t *)record, APP_STORAGE_INSTALL_INDEX_RECORD_CRC_SPAN);
  if (calc_crc != record->record_crc32)
  {
    return 0U;
  }

  return 1U;
}

static VOID AppStorageInstallIndexPublish(const app_storage_install_index_record_t *record, uint32_t slot_index)
{
  g_storage_install_index_valid = 1UL;
  g_storage_install_index_active_slot = (ULONG)slot_index;
  g_storage_install_index_sequence = (ULONG)record->sequence;
  g_storage_install_index_package_id = (ULONG)record->package_id;
  g_storage_install_index_package_version = (ULONG)record->package_version;
  g_storage_install_index_blob_offset = (ULONG)record->blob_offset;
  g_storage_install_index_blob_size = (ULONG)record->blob_size;
  g_storage_install_index_blob_crc32 = (ULONG)record->blob_crc32;
  g_storage_install_index_manifest_addr = (ULONG)record->manifest_addr;
  g_storage_install_index_manifest_size = (ULONG)record->manifest_size;
  g_storage_install_index_manifest_crc32 = (ULONG)record->manifest_crc32;
  g_storage_install_index_record_crc32 = (ULONG)record->record_crc32;
}

static UINT AppStorageInstallIndexLoad(void)
{
  static const uint32_t slot_addrs[APP_STORAGE_INSTALL_INDEX_SLOT_COUNT] =
  {
    (uint32_t)APP_STORAGE_INSTALL_INDEX_SLOT0_ADDR,
    (uint32_t)APP_STORAGE_INSTALL_INDEX_SLOT1_ADDR
  };
  app_storage_install_index_record_t record;
  app_storage_install_index_record_t best_record;
  HAL_StatusTypeDef hal_status;
  uint32_t slot_index;
  uint32_t best_slot = APP_STORAGE_INSTALL_INDEX_SLOT_INVALID;
  uint32_t blank_slot_count = 0UL;
  uint8_t best_valid = 0U;

  g_storage_last_op = APP_STORAGE_OP_INSTALL_INDEX_LOAD;
  g_storage_install_index_load_last_status = (ULONG)TX_NOT_DONE;
  AppStorageInstallIndexClearActive();
  (void)memset(&best_record, 0, sizeof(best_record));

  for (slot_index = 0UL; slot_index < APP_STORAGE_INSTALL_INDEX_SLOT_COUNT; slot_index++)
  {
    (void)memset(&record, 0, sizeof(record));
    hal_status = AT25_Read(&hospi1, slot_addrs[slot_index], (uint8_t *)&record, (uint32_t)sizeof(record));
    AppStorageCaptureDebug();
    if (hal_status != HAL_OK)
    {
      continue;
    }

    if ((record.magic == 0xFFFFFFFFUL) &&
        (record.version == 0xFFFFU) &&
        (record.header_size == 0xFFFFU) &&
        (record.sequence == 0xFFFFFFFFUL) &&
        (record.record_crc32 == 0xFFFFFFFFUL))
    {
      blank_slot_count++;
      continue;
    }

    if (AppStorageInstallIndexRecordValidate(&record) == 0U)
    {
      continue;
    }

    if ((best_valid == 0U) || (AppStorageInstallIndexSeqNewer(record.sequence, best_record.sequence) != 0U))
    {
      best_valid = 1U;
      best_slot = slot_index;
      best_record = record;
    }
  }

  if (best_valid == 0U)
  {
    if (blank_slot_count >= APP_STORAGE_INSTALL_INDEX_SLOT_COUNT)
    {
      g_storage_install_index_load_last_status = (ULONG)TX_SUCCESS;
      g_storage_install_index_scan_done = 1UL;
      return TX_SUCCESS;
    }
    g_storage_install_index_load_fail_count++;
    g_storage_install_index_scan_done = 0UL;
    return TX_NOT_DONE;
  }

  AppStorageInstallIndexPublish(&best_record, best_slot);
  g_storage_install_index_load_ok_count++;
  g_storage_install_index_load_last_status = (ULONG)TX_SUCCESS;
  g_storage_install_index_scan_done = 1UL;
  return TX_SUCCESS;
}

static UINT AppStorageInstallIndexCommit(uint32_t manifest_addr, uint32_t manifest_size, const uint8_t *manifest_data)
{
  app_storage_install_index_record_t record;
  app_storage_install_index_record_t verify;
  HAL_StatusTypeDef hal_status;
  const game_package_desc_t *pkg;
  const game_package_runtime_config_t *cfg = (const game_package_runtime_config_t *)0;
  uint32_t route_index;
  uint32_t selected_mode_id = 0UL;
  uint32_t blob_addr = 0UL;
  uint32_t blob_size = 0UL;
  uint64_t blob_end;
  uint32_t target_slot;
  uint32_t target_addr;
  uint32_t next_sequence;

  g_storage_last_op = APP_STORAGE_OP_INSTALL_INDEX_WRITE;
  g_storage_install_index_write_last_status = (ULONG)TX_NOT_DONE;

  pkg = GamePackage_GetActive();
  if (pkg == (const game_package_desc_t *)0)
  {
    g_storage_install_index_write_fail_count++;
    return TX_NOT_DONE;
  }

  if ((pkg->pet_routes != (const game_package_pet_route_t *)0) && (pkg->pet_route_count > 0U))
  {
    for (route_index = 0UL; route_index < pkg->pet_route_count; route_index++)
    {
      if (pkg->pet_routes[route_index].pet_entry_id == (uint32_t)GAME_PET_ENTRY_START_GAME)
      {
        selected_mode_id = pkg->pet_routes[route_index].mode_id;
        break;
      }
    }
  }

  if ((selected_mode_id == 0UL) && (pkg->modes != (const game_package_mode_desc_t *)0) && (pkg->mode_count > 0U))
  {
    selected_mode_id = pkg->modes[0].mode_id;
  }

  if (selected_mode_id != 0UL)
  {
    cfg = GamePackage_GetRuntimeConfigByModeId(selected_mode_id);
  }

  if ((cfg == (const game_package_runtime_config_t *)0) &&
      (pkg->modes != (const game_package_mode_desc_t *)0) &&
      (pkg->mode_count > 0U))
  {
    cfg = &pkg->modes[0].runtime_config;
  }

  if (cfg != (const game_package_runtime_config_t *)0)
  {
    blob_addr = cfg->scene_map_addr;
    blob_size = cfg->scene_map_size_bytes;
  }

  if ((g_storage_install_index_valid != 0UL) &&
      (g_storage_install_index_active_slot != APP_STORAGE_INSTALL_INDEX_SLOT_INVALID))
  {
    next_sequence = (uint32_t)g_storage_install_index_sequence + 1UL;
  }
  else
  {
    next_sequence = 1UL;
  }
  if (next_sequence == 0UL)
  {
    next_sequence = 1UL;
  }

  (void)memset(&record, 0, sizeof(record));
  record.magic = APP_STORAGE_INSTALL_INDEX_MAGIC;
  record.version = APP_STORAGE_INSTALL_INDEX_VERSION;
  record.header_size = (uint16_t)sizeof(app_storage_install_index_record_t);
  record.sequence = next_sequence;
  record.package_id = pkg->package_id;
  record.package_version = pkg->package_version;
  record.manifest_addr = manifest_addr;
  record.manifest_size = manifest_size;
  if ((manifest_data != (const uint8_t *)0) && (manifest_size > 0UL))
  {
    record.manifest_crc32 = AppStorageCrc32(manifest_data, manifest_size);
  }
  else
  {
    record.manifest_crc32 = 0UL;
  }
  record.blob_crc32 = record.manifest_crc32;

  if (blob_size > 0UL)
  {
    blob_end = (uint64_t)blob_addr + (uint64_t)blob_size;
    if ((blob_addr >= (uint32_t)KNOB_STORAGE_INSTALLED_BASE_ADDR) &&
        (blob_end <= (uint64_t)APP_STORAGE_INSTALLED_DATA_END_ADDR) &&
        (blob_end >= (uint64_t)blob_addr))
    {
      record.blob_offset = blob_addr - (uint32_t)KNOB_STORAGE_INSTALLED_BASE_ADDR;
      record.blob_size = blob_size;
    }
  }

  record.record_crc32 = AppStorageCrc32((const uint8_t *)&record, APP_STORAGE_INSTALL_INDEX_RECORD_CRC_SPAN);

  if ((g_storage_install_index_valid != 0UL) && (g_storage_install_index_active_slot == 0UL))
  {
    target_slot = 1UL;
    target_addr = (uint32_t)APP_STORAGE_INSTALL_INDEX_SLOT1_ADDR;
  }
  else
  {
    target_slot = 0UL;
    target_addr = (uint32_t)APP_STORAGE_INSTALL_INDEX_SLOT0_ADDR;
  }

  hal_status = AT25_Erase4K(&hospi1, target_addr);
  AppStorageCaptureDebug();
  if (hal_status != HAL_OK)
  {
    g_storage_install_index_write_fail_count++;
    return TX_NOT_DONE;
  }

  hal_status = AT25_PageProgram(&hospi1, target_addr, (const uint8_t *)&record, (uint32_t)sizeof(record));
  AppStorageCaptureDebug();
  if (hal_status != HAL_OK)
  {
    g_storage_install_index_write_fail_count++;
    return TX_NOT_DONE;
  }

  (void)memset(&verify, 0, sizeof(verify));
  hal_status = AT25_Read(&hospi1, target_addr, (uint8_t *)&verify, (uint32_t)sizeof(verify));
  AppStorageCaptureDebug();
  if (hal_status != HAL_OK)
  {
    g_storage_install_index_write_fail_count++;
    return TX_NOT_DONE;
  }

  if ((AppStorageInstallIndexRecordValidate(&verify) == 0U) ||
      (verify.sequence != record.sequence))
  {
    g_storage_install_index_write_fail_count++;
    return TX_NOT_DONE;
  }

  AppStorageInstallIndexPublish(&verify, target_slot);
  g_storage_install_index_write_ok_count++;
  g_storage_install_index_write_last_status = (ULONG)TX_SUCCESS;
  g_storage_install_index_scan_done = 1UL;
  return TX_SUCCESS;
}

static UINT AppStorageRunFlashProbe(void)
{
  AT25_InitCfg flash_cfg = {0};
  uint32_t jedec_id = 0UL;
  uint8_t boot_ok;

  flash_cfg.bus_mode = AT25_BUS_SPI;
  flash_cfg.read_dummy = 8U;
  flash_cfg.timeout_ms = 100U;

  g_storage_last_op = APP_STORAGE_OP_FLASH_PROBE;
  g_storage_last_error = APP_STORAGE_ERR_NONE;

  AppStorageOspiClockEnable();
  if (AppStorageRunFlashResume() != TX_SUCCESS)
  {
    g_storage_flash_ready = 0UL;
    return TX_NOT_DONE;
  }

  boot_ok = (uint8_t)AT25_BootInit(&hospi1, &flash_cfg, &jedec_id);
  AppStorageCaptureDebug();
  if (boot_ok == 0U)
  {
    if (AppStorageOspiRecover() == TX_SUCCESS)
    {
      boot_ok = (uint8_t)AT25_BootInit(&hospi1, &flash_cfg, &jedec_id);
      AppStorageCaptureDebug();
    }
    if (boot_ok == 0U)
    {
      g_storage_flash_ready = 0UL;
      g_storage_last_error = APP_STORAGE_ERR_BOOTINIT;
      return TX_NOT_DONE;
    }
  }

  g_storage_flash_ready = 1UL;
  g_storage_flash_quiesced = 0UL;
  g_storage_flash_in_dpd = 0UL;
  g_storage_last_jedec_id = (ULONG)jedec_id;
  g_storage_last_op = APP_STORAGE_OP_FLASH_PROBE;
  if (g_storage_install_index_scan_done == 0UL)
  {
    (void)AppStorageInstallIndexLoad();
  }
  g_storage_last_op = APP_STORAGE_OP_FLASH_PROBE;
  return TX_SUCCESS;
}

static UINT AppStorageRunRawSmoke(void)
{
  HAL_StatusTypeDef hal_status;
  ULONG index;
  uint8_t tx_buf[APP_STORAGE_SMOKE_MAX_LEN];
  uint8_t rx_buf[APP_STORAGE_SMOKE_MAX_LEN];
  const uint32_t smoke_addr = (uint32_t)KNOB_STORAGE_SMOKE_ADDR;
  const uint32_t smoke_len = (uint32_t)KNOB_STORAGE_SMOKE_LEN;

  g_storage_last_op = APP_STORAGE_OP_RAW_SMOKE;
  g_storage_last_error = APP_STORAGE_ERR_NONE;

  if ((smoke_addr % APP_STORAGE_SMOKE_SECTOR_SIZE) != 0UL)
  {
    g_storage_last_error = APP_STORAGE_ERR_ALIGN;
    g_storage_smoke_fail_count++;
    return TX_NOT_AVAILABLE;
  }

  if ((smoke_len == 0UL) ||
      (smoke_len > APP_STORAGE_SMOKE_MAX_LEN) ||
      (smoke_addr >= APP_STORAGE_FLASH_SIZE_BYTES) ||
      ((smoke_addr + smoke_len) > APP_STORAGE_FLASH_SIZE_BYTES))
  {
    g_storage_last_error = APP_STORAGE_ERR_RANGE;
    g_storage_smoke_fail_count++;
    return TX_SIZE_ERROR;
  }

  if (AppStorageRunFlashProbe() != TX_SUCCESS)
  {
    g_storage_last_op = APP_STORAGE_OP_RAW_SMOKE;
    g_storage_smoke_fail_count++;
    return TX_NOT_DONE;
  }
  g_storage_last_op = APP_STORAGE_OP_RAW_SMOKE;

  for (index = 0UL; index < smoke_len; index++)
  {
    tx_buf[index] = (uint8_t)(0xA5U ^ (uint8_t)((index * 13UL) + 0x3CU));
  }

  hal_status = AT25_Erase4K(&hospi1, smoke_addr);
  AppStorageCaptureDebug();
  if (hal_status != HAL_OK)
  {
    g_storage_last_error = APP_STORAGE_ERR_ERASE;
    g_storage_smoke_fail_count++;
    return TX_NOT_DONE;
  }

  hal_status = AT25_Read(&hospi1, smoke_addr, rx_buf, smoke_len);
  AppStorageCaptureDebug();
  if (hal_status != HAL_OK)
  {
    g_storage_last_error = APP_STORAGE_ERR_ERASE_VERIFY;
    g_storage_smoke_fail_count++;
    return TX_NOT_DONE;
  }

  for (index = 0UL; index < smoke_len; index++)
  {
    if (rx_buf[index] != 0xFFU)
    {
      g_storage_last_error = APP_STORAGE_ERR_ERASE_VERIFY;
      g_storage_smoke_fail_count++;
      return TX_NOT_DONE;
    }
  }

  hal_status = AT25_PageProgram(&hospi1, smoke_addr, tx_buf, smoke_len);
  AppStorageCaptureDebug();
  if (hal_status != HAL_OK)
  {
    g_storage_last_error = APP_STORAGE_ERR_PROGRAM;
    g_storage_smoke_fail_count++;
    return TX_NOT_DONE;
  }

  hal_status = AT25_Read(&hospi1, smoke_addr, rx_buf, smoke_len);
  AppStorageCaptureDebug();
  if (hal_status != HAL_OK)
  {
    g_storage_last_error = APP_STORAGE_ERR_PROGRAM_VERIFY;
    g_storage_smoke_fail_count++;
    return TX_NOT_DONE;
  }

  for (index = 0UL; index < smoke_len; index++)
  {
    if (rx_buf[index] != tx_buf[index])
    {
      g_storage_last_error = APP_STORAGE_ERR_PROGRAM_VERIFY;
      g_storage_smoke_fail_count++;
      return TX_NOT_DONE;
    }
  }

  hal_status = AT25_Erase4K(&hospi1, smoke_addr);
  AppStorageCaptureDebug();
  if (hal_status != HAL_OK)
  {
    g_storage_last_error = APP_STORAGE_ERR_FINAL_ERASE;
    g_storage_smoke_fail_count++;
    return TX_NOT_DONE;
  }

  hal_status = AT25_Read(&hospi1, smoke_addr, rx_buf, smoke_len);
  AppStorageCaptureDebug();
  if (hal_status != HAL_OK)
  {
    g_storage_last_error = APP_STORAGE_ERR_FINAL_VERIFY;
    g_storage_smoke_fail_count++;
    return TX_NOT_DONE;
  }

  for (index = 0UL; index < smoke_len; index++)
  {
    if (rx_buf[index] != 0xFFU)
    {
      g_storage_last_error = APP_STORAGE_ERR_FINAL_VERIFY;
      g_storage_smoke_fail_count++;
      return TX_NOT_DONE;
    }
  }

  g_storage_last_error = AppStorageHalToError(HAL_OK);
  g_storage_smoke_pass_count++;
  return TX_SUCCESS;
}

static UINT AppStorageRunAudioCatalogLoad(uint32_t catalog_addr)
{
  HAL_StatusTypeDef hal_status;
  app_storage_audio_catalog_header_t header = {0};
  uint64_t table_bytes;
  uint64_t end_addr;
  uint64_t audio_base = (uint64_t)KNOB_STORAGE_AUDIO_CATALOG_ADDR;
  uint64_t audio_limit = audio_base + (uint64_t)KNOB_STORAGE_AUDIO_CATALOG_MAX_BYTES;
  uint32_t table_crc32 = 0UL;
  uint32_t table_addr;

  g_storage_last_op = APP_STORAGE_OP_AUDIO_CATALOG_LOAD;
  g_storage_last_error = APP_STORAGE_ERR_NONE;
  g_storage_audio_catalog_loaded = 0UL;
  g_storage_audio_catalog_addr = (ULONG)catalog_addr;
  g_storage_audio_catalog_entry_count = 0UL;
  g_storage_audio_catalog_version = 0UL;
  g_storage_audio_catalog_table_crc32 = 0UL;
  (void)memset(&g_storage_audio_catalog_header, 0, sizeof(g_storage_audio_catalog_header));
  (void)memset(g_storage_audio_catalog_entries, 0, sizeof(g_storage_audio_catalog_entries));

  if ((catalog_addr >= APP_STORAGE_FLASH_SIZE_BYTES) ||
      (((uint64_t)catalog_addr + (uint64_t)sizeof(header)) > (uint64_t)APP_STORAGE_FLASH_SIZE_BYTES) ||
      ((uint64_t)catalog_addr < audio_base) ||
      (((uint64_t)catalog_addr + (uint64_t)sizeof(header)) > audio_limit))
  {
    g_storage_last_error = APP_STORAGE_ERR_AUDIO_RANGE;
    g_storage_audio_catalog_load_fail_count++;
    return TX_SIZE_ERROR;
  }

  if (AppStorageRunFlashProbe() != TX_SUCCESS)
  {
    g_storage_last_op = APP_STORAGE_OP_AUDIO_CATALOG_LOAD;
    g_storage_last_error = APP_STORAGE_ERR_BOOTINIT;
    g_storage_audio_catalog_load_fail_count++;
    return TX_NOT_DONE;
  }
  g_storage_last_op = APP_STORAGE_OP_AUDIO_CATALOG_LOAD;

  hal_status = AT25_Read(&hospi1, catalog_addr, (uint8_t *)&header, (uint32_t)sizeof(header));
  AppStorageCaptureDebug();
  if (hal_status != HAL_OK)
  {
    g_storage_last_error = APP_STORAGE_ERR_AUDIO_READ;
    g_storage_audio_catalog_load_fail_count++;
    return TX_NOT_DONE;
  }

  if ((header.magic != APP_STORAGE_AUDIO_CATALOG_MAGIC) ||
      (header.version != APP_STORAGE_AUDIO_CATALOG_VERSION) ||
      (header.header_size < (uint16_t)sizeof(header)) ||
      (header.entry_size != (uint16_t)sizeof(app_storage_audio_catalog_entry_t)))
  {
    g_storage_last_error = APP_STORAGE_ERR_AUDIO_CATALOG;
    g_storage_audio_catalog_load_fail_count++;
    return TX_NOT_DONE;
  }

  if ((ULONG)header.entry_count > APP_STORAGE_AUDIO_CATALOG_MAX_ENTRIES)
  {
    g_storage_last_error = APP_STORAGE_ERR_AUDIO_CATALOG;
    g_storage_audio_catalog_load_fail_count++;
    return TX_SIZE_ERROR;
  }

  table_bytes = (uint64_t)header.entry_size * (uint64_t)header.entry_count;
  end_addr = (uint64_t)catalog_addr + (uint64_t)header.header_size + table_bytes;
  if ((end_addr > (uint64_t)APP_STORAGE_FLASH_SIZE_BYTES) ||
      (end_addr > audio_limit))
  {
    g_storage_last_error = APP_STORAGE_ERR_AUDIO_RANGE;
    g_storage_audio_catalog_load_fail_count++;
    return TX_SIZE_ERROR;
  }

  table_addr = catalog_addr + (uint32_t)header.header_size;
  if (header.entry_count != 0UL)
  {
    hal_status = AT25_Read(&hospi1,
                           table_addr,
                           (uint8_t *)g_storage_audio_catalog_entries,
                           (uint32_t)table_bytes);
    AppStorageCaptureDebug();
    if (hal_status != HAL_OK)
    {
      g_storage_last_error = APP_STORAGE_ERR_AUDIO_READ;
      g_storage_audio_catalog_load_fail_count++;
      (void)memset(g_storage_audio_catalog_entries, 0, sizeof(g_storage_audio_catalog_entries));
      return TX_NOT_DONE;
    }

    table_crc32 = AppStorageCrc32((const uint8_t *)g_storage_audio_catalog_entries, (uint32_t)table_bytes);
    if ((header.table_crc32 != 0UL) && (table_crc32 != header.table_crc32))
    {
      g_storage_last_error = APP_STORAGE_ERR_AUDIO_CATALOG;
      g_storage_audio_catalog_load_fail_count++;
      (void)memset(g_storage_audio_catalog_entries, 0, sizeof(g_storage_audio_catalog_entries));
      return TX_NOT_DONE;
    }
  }

  g_storage_audio_catalog_header = header;
  g_storage_audio_catalog_loaded = 1UL;
  g_storage_audio_catalog_entry_count = (ULONG)header.entry_count;
  g_storage_audio_catalog_version = (ULONG)header.version;
  g_storage_audio_catalog_table_crc32 = (ULONG)table_crc32;
  g_storage_audio_catalog_load_ok_count++;
  return TX_SUCCESS;
}

static UINT AppStorageRunAudioChunkRead(uint32_t addr, uint32_t len, uint32_t token)
{
  HAL_StatusTypeDef hal_status;
  uint64_t audio_base = (uint64_t)KNOB_STORAGE_AUDIO_CATALOG_ADDR;
  uint64_t audio_limit = audio_base + (uint64_t)KNOB_STORAGE_AUDIO_CATALOG_MAX_BYTES;

  g_storage_last_op = APP_STORAGE_OP_AUDIO_CHUNK_READ;
  g_storage_last_error = APP_STORAGE_ERR_NONE;
  g_storage_audio_chunk_last_addr = (ULONG)addr;
  g_storage_audio_chunk_last_len = (ULONG)len;
  g_storage_audio_chunk_last_token = (ULONG)token;
  g_storage_audio_chunk_last_crc32 = 0UL;

  if ((len == 0UL) || (len > APP_STORAGE_AUDIO_CHUNK_MAX_BYTES))
  {
    g_storage_last_error = APP_STORAGE_ERR_AUDIO_CHUNK_SIZE;
    g_storage_audio_chunk_read_fail_count++;
    AppStorageAudioChunkCachePublish((ULONG)token,
                                     (ULONG)addr,
                                     (ULONG)len,
                                     (ULONG)TX_NOT_DONE,
                                     0UL,
                                     (const uint8_t *)0);
    return TX_SIZE_ERROR;
  }

  if ((addr >= APP_STORAGE_FLASH_SIZE_BYTES) ||
      (((uint64_t)addr + (uint64_t)len) > (uint64_t)APP_STORAGE_FLASH_SIZE_BYTES) ||
      ((uint64_t)addr < audio_base) ||
      (((uint64_t)addr + (uint64_t)len) > audio_limit))
  {
    g_storage_last_error = APP_STORAGE_ERR_AUDIO_RANGE;
    g_storage_audio_chunk_read_fail_count++;
    AppStorageAudioChunkCachePublish((ULONG)token,
                                     (ULONG)addr,
                                     (ULONG)len,
                                     (ULONG)TX_NOT_DONE,
                                     0UL,
                                     (const uint8_t *)0);
    return TX_SIZE_ERROR;
  }

  /*
   * Chunk reads are high-rate in audio playback; avoid re-probing flash on
   * every chunk once probe has already succeeded in this boot session.
   */
  if ((g_storage_flash_ready == 0UL) && (AppStorageRunFlashProbe() != TX_SUCCESS))
  {
    g_storage_last_op = APP_STORAGE_OP_AUDIO_CHUNK_READ;
    g_storage_last_error = APP_STORAGE_ERR_BOOTINIT;
    g_storage_audio_chunk_read_fail_count++;
    AppStorageAudioChunkCachePublish((ULONG)token,
                                     (ULONG)addr,
                                     (ULONG)len,
                                     (ULONG)TX_NOT_DONE,
                                     0UL,
                                     (const uint8_t *)0);
    return TX_NOT_DONE;
  }
  g_storage_last_op = APP_STORAGE_OP_AUDIO_CHUNK_READ;

  hal_status = AT25_Read(&hospi1, addr, g_storage_audio_chunk_buf, len);
  AppStorageCaptureDebug();
  if (hal_status != HAL_OK)
  {
    g_storage_last_error = APP_STORAGE_ERR_AUDIO_READ;
    g_storage_audio_chunk_read_fail_count++;
    AppStorageAudioChunkCachePublish((ULONG)token,
                                     (ULONG)addr,
                                     (ULONG)len,
                                     (ULONG)TX_NOT_DONE,
                                     0UL,
                                     (const uint8_t *)0);
    return TX_NOT_DONE;
  }

  g_storage_audio_chunk_last_crc32 = (ULONG)AppStorageCrc32(g_storage_audio_chunk_buf, len);
  g_storage_audio_chunk_read_ok_count++;
  AppStorageAudioChunkCachePublish((ULONG)token,
                                   (ULONG)addr,
                                   (ULONG)len,
                                   (ULONG)TX_SUCCESS,
                                   g_storage_audio_chunk_last_crc32,
                                   g_storage_audio_chunk_buf);
  return TX_SUCCESS;
}

static UINT AppStorageRunAudioCatalogInstallEmbedded(void)
{
  app_storage_audio_catalog_header_t header = {0};
  uint32_t asset_id;
  uint32_t asset_count;
  uint32_t entry_count = 0UL;
  uint32_t table_bytes = 0UL;
  uint64_t data_addr = 0ULL;
  uint64_t end_addr = 0ULL;
  uint32_t erase_addr;
  uint32_t erase_end;
  uint64_t region_limit = (uint64_t)APP_STORAGE_FLASH_SIZE_BYTES;
  UINT status;
  HAL_StatusTypeDef hal_status;

  g_storage_last_op = APP_STORAGE_OP_AUDIO_CATALOG_INSTALL;
  g_storage_last_error = APP_STORAGE_ERR_NONE;
  g_storage_audio_catalog_install_last_bytes = 0UL;
  (void)memset(g_storage_audio_catalog_entries, 0, sizeof(g_storage_audio_catalog_entries));

  if (AppStorageRunFlashProbe() != TX_SUCCESS)
  {
    g_storage_last_op = APP_STORAGE_OP_AUDIO_CATALOG_INSTALL;
    g_storage_last_error = APP_STORAGE_ERR_BOOTINIT;
    g_storage_audio_catalog_install_fail_count++;
    return TX_NOT_DONE;
  }
  g_storage_last_op = APP_STORAGE_OP_AUDIO_CATALOG_INSTALL;

  asset_count = AppAudioAssets_Count();
  for (asset_id = 1UL; asset_id < asset_count; asset_id++)
  {
    const app_audio_adpcm_clip_t *clip = AppAudioAssets_GetClip(asset_id);
    app_storage_audio_catalog_entry_t *entry;

    if ((clip == (const app_audio_adpcm_clip_t *)0) ||
        (clip->data == (const uint8_t *)0) ||
        (clip->data_size == 0UL) ||
        (clip->sample_rate_hz == 0UL) ||
        (clip->block_align <= 4U) ||
        (clip->samples_per_block == 0U) ||
        (clip->total_samples == 0UL))
    {
      continue;
    }

    if (entry_count >= APP_STORAGE_AUDIO_CATALOG_MAX_ENTRIES)
    {
      g_storage_last_error = APP_STORAGE_ERR_AUDIO_CATALOG;
      g_storage_audio_catalog_install_fail_count++;
      return TX_SIZE_ERROR;
    }

    entry = &g_storage_audio_catalog_entries[entry_count];
    entry->asset_id = asset_id;
    entry->flags = 0UL;
    entry->data_offset = 0UL;
    entry->data_size = clip->data_size;
    entry->sample_rate_hz = clip->sample_rate_hz;
    entry->block_align = clip->block_align;
    entry->samples_per_block = clip->samples_per_block;
    entry->total_samples = clip->total_samples;
    entry_count++;
  }

  if (entry_count == 0UL)
  {
    g_storage_last_error = APP_STORAGE_ERR_AUDIO_INSTALL;
    g_storage_audio_catalog_install_fail_count++;
    return TX_NOT_DONE;
  }

  table_bytes = entry_count * (uint32_t)sizeof(app_storage_audio_catalog_entry_t);
  data_addr = (uint64_t)KNOB_STORAGE_AUDIO_CATALOG_ADDR + (uint64_t)sizeof(app_storage_audio_catalog_header_t) + (uint64_t)table_bytes;
  data_addr = (data_addr + 3UL) & ~3UL;

  for (asset_id = 0UL; asset_id < entry_count; asset_id++)
  {
    app_storage_audio_catalog_entry_t *entry = &g_storage_audio_catalog_entries[asset_id];
    if (data_addr > 0xFFFFFFFFULL)
    {
      g_storage_last_error = APP_STORAGE_ERR_AUDIO_RANGE;
      g_storage_audio_catalog_install_fail_count++;
      return TX_SIZE_ERROR;
    }
    entry->data_offset = (uint32_t)data_addr;
    data_addr += (uint64_t)entry->data_size;
    data_addr = (data_addr + 3UL) & ~3UL;
  }
  end_addr = data_addr;

  region_limit = (uint64_t)KNOB_STORAGE_AUDIO_CATALOG_ADDR + (uint64_t)KNOB_STORAGE_AUDIO_CATALOG_MAX_BYTES;
  if (region_limit > (uint64_t)KNOB_STORAGE_FAT_BASE_ADDR)
  {
    region_limit = (uint64_t)KNOB_STORAGE_FAT_BASE_ADDR;
  }
  if (end_addr > region_limit)
  {
    g_storage_last_error = APP_STORAGE_ERR_AUDIO_RANGE;
    g_storage_audio_catalog_install_fail_count++;
    return TX_SIZE_ERROR;
  }

  erase_addr = (uint32_t)KNOB_STORAGE_AUDIO_CATALOG_ADDR;
  erase_end = (uint32_t)(((end_addr + APP_STORAGE_SMOKE_SECTOR_SIZE - 1UL) / APP_STORAGE_SMOKE_SECTOR_SIZE) * APP_STORAGE_SMOKE_SECTOR_SIZE);
  for (; erase_addr < erase_end; erase_addr += APP_STORAGE_SMOKE_SECTOR_SIZE)
  {
    hal_status = AT25_Erase4K(&hospi1, erase_addr);
    AppStorageCaptureDebug();
    if (hal_status != HAL_OK)
    {
      g_storage_last_error = APP_STORAGE_ERR_ERASE;
      g_storage_audio_catalog_install_fail_count++;
      return TX_NOT_DONE;
    }
  }

  header.magic = APP_STORAGE_AUDIO_CATALOG_MAGIC;
  header.version = APP_STORAGE_AUDIO_CATALOG_VERSION;
  header.header_size = (uint16_t)sizeof(app_storage_audio_catalog_header_t);
  header.entry_size = (uint16_t)sizeof(app_storage_audio_catalog_entry_t);
  header.reserved0 = 0U;
  header.entry_count = entry_count;
  header.table_crc32 = AppStorageCrc32((const uint8_t *)g_storage_audio_catalog_entries, table_bytes);
  header.reserved1 = 0UL;

  hal_status = AT25_PageProgram(&hospi1,
                                (uint32_t)KNOB_STORAGE_AUDIO_CATALOG_ADDR,
                                (const uint8_t *)&header,
                                (uint32_t)sizeof(header));
  AppStorageCaptureDebug();
  if (hal_status != HAL_OK)
  {
    g_storage_last_error = APP_STORAGE_ERR_PROGRAM;
    g_storage_audio_catalog_install_fail_count++;
    return TX_NOT_DONE;
  }

  hal_status = AT25_PageProgram(&hospi1,
                                (uint32_t)KNOB_STORAGE_AUDIO_CATALOG_ADDR + (uint32_t)sizeof(header),
                                (const uint8_t *)g_storage_audio_catalog_entries,
                                table_bytes);
  AppStorageCaptureDebug();
  if (hal_status != HAL_OK)
  {
    g_storage_last_error = APP_STORAGE_ERR_PROGRAM;
    g_storage_audio_catalog_install_fail_count++;
    return TX_NOT_DONE;
  }

  for (asset_id = 0UL; asset_id < entry_count; asset_id++)
  {
    const app_storage_audio_catalog_entry_t *entry = &g_storage_audio_catalog_entries[asset_id];
    const app_audio_adpcm_clip_t *clip = AppAudioAssets_GetClip(entry->asset_id);
    if ((clip == (const app_audio_adpcm_clip_t *)0) ||
        (clip->data == (const uint8_t *)0) ||
        (clip->data_size != entry->data_size))
    {
      g_storage_last_error = APP_STORAGE_ERR_AUDIO_CATALOG;
      g_storage_audio_catalog_install_fail_count++;
      return TX_NOT_DONE;
    }

    hal_status = AT25_PageProgram(&hospi1, entry->data_offset, clip->data, clip->data_size);
    AppStorageCaptureDebug();
    if (hal_status != HAL_OK)
    {
      g_storage_last_error = APP_STORAGE_ERR_PROGRAM;
      g_storage_audio_catalog_install_fail_count++;
      return TX_NOT_DONE;
    }
  }

  g_storage_audio_catalog_install_last_bytes = (ULONG)(end_addr - (uint64_t)KNOB_STORAGE_AUDIO_CATALOG_ADDR);
  status = AppStorageRunAudioCatalogLoad((uint32_t)KNOB_STORAGE_AUDIO_CATALOG_ADDR);
  if (status != TX_SUCCESS)
  {
    g_storage_last_op = APP_STORAGE_OP_AUDIO_CATALOG_INSTALL;
    g_storage_audio_catalog_install_fail_count++;
    return status;
  }

  g_storage_last_op = APP_STORAGE_OP_AUDIO_CATALOG_INSTALL;
  g_storage_last_error = APP_STORAGE_ERR_NONE;
  g_storage_audio_catalog_install_ok_count++;
  return TX_SUCCESS;
}

static UINT AppStorageRunAudioCatalogInstallManifestRefs(void)
{
  app_storage_audio_catalog_header_t header = {0};
  static const uint32_t kManifestAssetIds[4] = {3001UL, 3002UL, 3003UL, 3004UL};
  static const app_audio_asset_id_t kSourceAssetIds[4] = {
      (app_audio_asset_id_t)APP_AUDIO_ASSET_MUSIC_WHISPERS_IN_THE_FOG,
      (app_audio_asset_id_t)APP_AUDIO_ASSET_SFX_DOOR,
      (app_audio_asset_id_t)APP_AUDIO_ASSET_SFX_GHOST_LAUGH,
      (app_audio_asset_id_t)APP_AUDIO_ASSET_SFX_EXPLOSION};
  uint32_t i;
  uint32_t entry_count = 0UL;
  uint32_t table_bytes = 0UL;
  uint64_t data_addr = 0ULL;
  uint64_t end_addr = 0ULL;
  uint32_t erase_addr;
  uint32_t erase_end;
  uint64_t region_limit = (uint64_t)APP_STORAGE_FLASH_SIZE_BYTES;
  UINT status;
  HAL_StatusTypeDef hal_status;

  g_storage_last_op = APP_STORAGE_OP_AUDIO_CATALOG_INSTALL;
  g_storage_last_error = APP_STORAGE_ERR_NONE;
  g_storage_audio_catalog_install_last_bytes = 0UL;
  (void)memset(g_storage_audio_catalog_entries, 0, sizeof(g_storage_audio_catalog_entries));

  if (AppStorageRunFlashProbe() != TX_SUCCESS)
  {
    g_storage_last_op = APP_STORAGE_OP_AUDIO_CATALOG_INSTALL;
    g_storage_last_error = APP_STORAGE_ERR_BOOTINIT;
    g_storage_audio_catalog_install_fail_count++;
    return TX_NOT_DONE;
  }
  g_storage_last_op = APP_STORAGE_OP_AUDIO_CATALOG_INSTALL;

  for (i = 0UL; i < (uint32_t)(sizeof(kManifestAssetIds) / sizeof(kManifestAssetIds[0])); i++)
  {
    const app_audio_adpcm_clip_t *clip = AppAudioAssets_GetClip((uint32_t)kSourceAssetIds[i]);
    app_storage_audio_catalog_entry_t *entry;

    if ((clip == (const app_audio_adpcm_clip_t *)0) ||
        (clip->data == (const uint8_t *)0) ||
        (clip->data_size == 0UL) ||
        (clip->sample_rate_hz == 0UL) ||
        (clip->block_align <= 4U) ||
        (clip->samples_per_block == 0U) ||
        (clip->total_samples == 0UL))
    {
      g_storage_last_error = APP_STORAGE_ERR_AUDIO_INSTALL;
      g_storage_audio_catalog_install_fail_count++;
      return TX_NOT_DONE;
    }

    if (entry_count >= APP_STORAGE_AUDIO_CATALOG_MAX_ENTRIES)
    {
      g_storage_last_error = APP_STORAGE_ERR_AUDIO_CATALOG;
      g_storage_audio_catalog_install_fail_count++;
      return TX_SIZE_ERROR;
    }

    entry = &g_storage_audio_catalog_entries[entry_count];
    entry->asset_id = kManifestAssetIds[i];
    entry->flags = 0UL;
    entry->data_offset = 0UL;
    entry->data_size = clip->data_size;
    entry->sample_rate_hz = clip->sample_rate_hz;
    entry->block_align = clip->block_align;
    entry->samples_per_block = clip->samples_per_block;
    entry->total_samples = clip->total_samples;
    entry_count++;
  }

  if (entry_count == 0UL)
  {
    g_storage_last_error = APP_STORAGE_ERR_AUDIO_INSTALL;
    g_storage_audio_catalog_install_fail_count++;
    return TX_NOT_DONE;
  }

  table_bytes = entry_count * (uint32_t)sizeof(app_storage_audio_catalog_entry_t);
  data_addr = (uint64_t)KNOB_STORAGE_AUDIO_CATALOG_ADDR + (uint64_t)sizeof(app_storage_audio_catalog_header_t) + (uint64_t)table_bytes;
  data_addr = (data_addr + 3UL) & ~3UL;

  for (i = 0UL; i < entry_count; i++)
  {
    app_storage_audio_catalog_entry_t *entry = &g_storage_audio_catalog_entries[i];
    if (data_addr > 0xFFFFFFFFULL)
    {
      g_storage_last_error = APP_STORAGE_ERR_AUDIO_RANGE;
      g_storage_audio_catalog_install_fail_count++;
      return TX_SIZE_ERROR;
    }
    entry->data_offset = (uint32_t)data_addr;
    data_addr += (uint64_t)entry->data_size;
    data_addr = (data_addr + 3UL) & ~3UL;
  }
  end_addr = data_addr;

  region_limit = (uint64_t)KNOB_STORAGE_AUDIO_CATALOG_ADDR + (uint64_t)KNOB_STORAGE_AUDIO_CATALOG_MAX_BYTES;
  if (region_limit > (uint64_t)KNOB_STORAGE_FAT_BASE_ADDR)
  {
    region_limit = (uint64_t)KNOB_STORAGE_FAT_BASE_ADDR;
  }
  if (end_addr > region_limit)
  {
    g_storage_last_error = APP_STORAGE_ERR_AUDIO_RANGE;
    g_storage_audio_catalog_install_fail_count++;
    return TX_SIZE_ERROR;
  }

  erase_addr = (uint32_t)KNOB_STORAGE_AUDIO_CATALOG_ADDR;
  erase_end = (uint32_t)(((end_addr + APP_STORAGE_SMOKE_SECTOR_SIZE - 1UL) / APP_STORAGE_SMOKE_SECTOR_SIZE) * APP_STORAGE_SMOKE_SECTOR_SIZE);
  for (; erase_addr < erase_end; erase_addr += APP_STORAGE_SMOKE_SECTOR_SIZE)
  {
    hal_status = AT25_Erase4K(&hospi1, erase_addr);
    AppStorageCaptureDebug();
    if (hal_status != HAL_OK)
    {
      g_storage_last_error = APP_STORAGE_ERR_ERASE;
      g_storage_audio_catalog_install_fail_count++;
      return TX_NOT_DONE;
    }
  }

  header.magic = APP_STORAGE_AUDIO_CATALOG_MAGIC;
  header.version = APP_STORAGE_AUDIO_CATALOG_VERSION;
  header.header_size = (uint16_t)sizeof(app_storage_audio_catalog_header_t);
  header.entry_size = (uint16_t)sizeof(app_storage_audio_catalog_entry_t);
  header.reserved0 = 0U;
  header.entry_count = entry_count;
  header.table_crc32 = AppStorageCrc32((const uint8_t *)g_storage_audio_catalog_entries, table_bytes);
  header.reserved1 = 0UL;

  hal_status = AT25_PageProgram(&hospi1,
                                (uint32_t)KNOB_STORAGE_AUDIO_CATALOG_ADDR,
                                (const uint8_t *)&header,
                                (uint32_t)sizeof(header));
  AppStorageCaptureDebug();
  if (hal_status != HAL_OK)
  {
    g_storage_last_error = APP_STORAGE_ERR_PROGRAM;
    g_storage_audio_catalog_install_fail_count++;
    return TX_NOT_DONE;
  }

  hal_status = AT25_PageProgram(&hospi1,
                                (uint32_t)KNOB_STORAGE_AUDIO_CATALOG_ADDR + (uint32_t)sizeof(header),
                                (const uint8_t *)g_storage_audio_catalog_entries,
                                table_bytes);
  AppStorageCaptureDebug();
  if (hal_status != HAL_OK)
  {
    g_storage_last_error = APP_STORAGE_ERR_PROGRAM;
    g_storage_audio_catalog_install_fail_count++;
    return TX_NOT_DONE;
  }

  for (i = 0UL; i < entry_count; i++)
  {
    const app_audio_adpcm_clip_t *clip = AppAudioAssets_GetClip((uint32_t)kSourceAssetIds[i]);
    const app_storage_audio_catalog_entry_t *entry = &g_storage_audio_catalog_entries[i];
    if ((clip == (const app_audio_adpcm_clip_t *)0) ||
        (clip->data == (const uint8_t *)0) ||
        (clip->data_size != entry->data_size))
    {
      g_storage_last_error = APP_STORAGE_ERR_AUDIO_CATALOG;
      g_storage_audio_catalog_install_fail_count++;
      return TX_NOT_DONE;
    }

    hal_status = AT25_PageProgram(&hospi1, entry->data_offset, clip->data, clip->data_size);
    AppStorageCaptureDebug();
    if (hal_status != HAL_OK)
    {
      g_storage_last_error = APP_STORAGE_ERR_PROGRAM;
      g_storage_audio_catalog_install_fail_count++;
      return TX_NOT_DONE;
    }
  }

  g_storage_audio_catalog_install_last_bytes = (ULONG)(end_addr - (uint64_t)KNOB_STORAGE_AUDIO_CATALOG_ADDR);
  status = AppStorageRunAudioCatalogLoad((uint32_t)KNOB_STORAGE_AUDIO_CATALOG_ADDR);
  if (status != TX_SUCCESS)
  {
    g_storage_last_op = APP_STORAGE_OP_AUDIO_CATALOG_INSTALL;
    g_storage_audio_catalog_install_fail_count++;
    return status;
  }

  g_storage_last_op = APP_STORAGE_OP_AUDIO_CATALOG_INSTALL;
  g_storage_last_error = APP_STORAGE_ERR_NONE;
  g_storage_audio_catalog_install_ok_count++;
  return TX_SUCCESS;
}

static UINT AppStorageRunGamePackageManifestLoadInternal(uint32_t manifest_addr,
                                                         uint32_t manifest_size,
                                                         uint8_t commit_install_index,
                                                         ULONG source_kind)
{
  HAL_StatusTypeDef hal_status;
  UINT status;
  const game_package_desc_t *pkg;

  g_storage_last_op = APP_STORAGE_OP_GAME_PACKAGE_MANIFEST_LOAD;
  g_storage_last_error = APP_STORAGE_ERR_NONE;
  g_storage_game_package_manifest_loaded = 0UL;
  g_storage_game_package_manifest_addr = (ULONG)manifest_addr;
  g_storage_game_package_manifest_size = (ULONG)manifest_size;
  g_storage_game_package_manifest_last_status = (ULONG)TX_NOT_DONE;
  g_storage_game_package_id = 0UL;
  g_storage_game_package_version = 0UL;
  g_storage_game_package_mode_count = 0UL;
  g_storage_game_package_pet_route_count = 0UL;
  g_storage_game_package_source = APP_STORAGE_PKG_SRC_NONE;
  (void)memset(g_storage_game_package_manifest_buf, 0, sizeof(g_storage_game_package_manifest_buf));

  if ((manifest_size == 0UL) || (manifest_size > (uint32_t)sizeof(g_storage_game_package_manifest_buf)))
  {
    g_storage_last_error = APP_STORAGE_ERR_GAME_PACKAGE_MANIFEST;
    g_storage_game_package_manifest_load_fail_count++;
    g_storage_game_package_manifest_last_status = (ULONG)TX_SIZE_ERROR;
    return TX_SIZE_ERROR;
  }

  if ((manifest_addr >= APP_STORAGE_FLASH_SIZE_BYTES) ||
      (((uint64_t)manifest_addr + (uint64_t)manifest_size) > (uint64_t)APP_STORAGE_FLASH_SIZE_BYTES))
  {
    g_storage_last_error = APP_STORAGE_ERR_RANGE;
    g_storage_game_package_manifest_load_fail_count++;
    g_storage_game_package_manifest_last_status = (ULONG)TX_SIZE_ERROR;
    return TX_SIZE_ERROR;
  }

  if (AppStorageRunFlashProbe() != TX_SUCCESS)
  {
    g_storage_last_op = APP_STORAGE_OP_GAME_PACKAGE_MANIFEST_LOAD;
    g_storage_last_error = APP_STORAGE_ERR_BOOTINIT;
    g_storage_game_package_manifest_load_fail_count++;
    g_storage_game_package_manifest_last_status = (ULONG)TX_NOT_DONE;
    return TX_NOT_DONE;
  }
  g_storage_last_op = APP_STORAGE_OP_GAME_PACKAGE_MANIFEST_LOAD;

  hal_status = AT25_Read(&hospi1, manifest_addr, g_storage_game_package_manifest_buf, manifest_size);
  AppStorageCaptureDebug();
  if (hal_status != HAL_OK)
  {
    g_storage_last_error = APP_STORAGE_ERR_GAME_PACKAGE_MANIFEST;
    g_storage_game_package_manifest_load_fail_count++;
    g_storage_game_package_manifest_last_status = (ULONG)TX_NOT_DONE;
    return TX_NOT_DONE;
  }

  status = GamePackage_LoadManifestBlob((const void *)g_storage_game_package_manifest_buf, manifest_size);
  g_storage_game_package_manifest_last_status = (ULONG)status;
  if (status != TX_SUCCESS)
  {
    g_storage_last_error = APP_STORAGE_ERR_GAME_PACKAGE_MANIFEST;
    g_storage_game_package_manifest_load_fail_count++;
    return status;
  }

  pkg = GamePackage_GetActive();
  if (pkg == (const game_package_desc_t *)0)
  {
    g_storage_last_error = APP_STORAGE_ERR_GAME_PACKAGE_MANIFEST;
    g_storage_game_package_manifest_load_fail_count++;
    g_storage_game_package_manifest_last_status = (ULONG)TX_NOT_DONE;
    return TX_NOT_DONE;
  }

  g_storage_game_package_manifest_loaded = 1UL;
  g_storage_game_package_id = (ULONG)pkg->package_id;
  g_storage_game_package_version = (ULONG)pkg->package_version;
  g_storage_game_package_mode_count = (ULONG)pkg->mode_count;
  g_storage_game_package_pet_route_count = (ULONG)pkg->pet_route_count;
  g_storage_game_package_source = source_kind;
  g_storage_game_package_manifest_load_ok_count++;
  if (commit_install_index != 0U)
  {
    (void)AppStorageInstallIndexCommit(manifest_addr, manifest_size, g_storage_game_package_manifest_buf);
  }
  g_storage_last_op = APP_STORAGE_OP_GAME_PACKAGE_MANIFEST_LOAD;
  return TX_SUCCESS;
}

static UINT AppStorageResolveDefaultManifestTarget(uint32_t *manifest_addr_out,
                                                   uint32_t *manifest_size_out,
                                                   uint8_t *commit_install_index_out,
                                                   ULONG *source_out)
{
  HAL_StatusTypeDef hal_status;
  game_package_manifest_header_t header = {0};
  uint32_t manifest_addr = (uint32_t)KNOB_STORAGE_GAME_PKG_MANIFEST_ADDR;
  uint32_t max_bytes = (uint32_t)KNOB_STORAGE_GAME_PKG_MANIFEST_MAX_BYTES;
  uint32_t manifest_size;

  if ((manifest_addr_out == (uint32_t *)0) ||
      (manifest_size_out == (uint32_t *)0) ||
      (commit_install_index_out == (uint8_t *)0) ||
      (source_out == (ULONG *)0))
  {
    return TX_PTR_ERROR;
  }

  *manifest_addr_out = 0UL;
  *manifest_size_out = 0UL;
  *commit_install_index_out = 0U;
  *source_out = APP_STORAGE_PKG_SRC_NONE;

  if ((max_bytes == 0UL) ||
      (max_bytes > (uint32_t)sizeof(g_storage_game_package_manifest_buf)) ||
      (max_bytes > GAME_PACKAGE_MANIFEST_MAX_BYTES))
  {
    return TX_SIZE_ERROR;
  }

  if ((manifest_addr >= APP_STORAGE_FLASH_SIZE_BYTES) ||
      (((uint64_t)manifest_addr + (uint64_t)sizeof(header)) > (uint64_t)APP_STORAGE_FLASH_SIZE_BYTES) ||
      (((uint64_t)manifest_addr + (uint64_t)max_bytes) > (uint64_t)APP_STORAGE_FLASH_SIZE_BYTES))
  {
    return TX_SIZE_ERROR;
  }

  hal_status = AT25_Read(&hospi1, manifest_addr, (uint8_t *)&header, (uint32_t)sizeof(header));
  AppStorageCaptureDebug();
  if (hal_status != HAL_OK)
  {
    return TX_NOT_DONE;
  }

  if ((header.magic != GAME_PACKAGE_MANIFEST_MAGIC) ||
      ((header.version != GAME_PACKAGE_MANIFEST_VERSION_V1) &&
       (header.version != GAME_PACKAGE_MANIFEST_VERSION_V2) &&
       (header.version != GAME_PACKAGE_MANIFEST_VERSION_V3) &&
       (header.version != GAME_PACKAGE_MANIFEST_VERSION_V4)) ||
      (((header.version == GAME_PACKAGE_MANIFEST_VERSION_V3) ||
        (header.version == GAME_PACKAGE_MANIFEST_VERSION_V4)) &&
       (header.header_size != (uint16_t)sizeof(game_package_manifest_header_v3_t))) ||
      ((header.version != GAME_PACKAGE_MANIFEST_VERSION_V3) &&
       (header.version != GAME_PACKAGE_MANIFEST_VERSION_V4) &&
       (header.header_size != (uint16_t)sizeof(game_package_manifest_header_t))))
  {
    return TX_NOT_DONE;
  }

  manifest_size = header.total_size;
  if ((manifest_size < (uint32_t)sizeof(game_package_manifest_header_t)) ||
      (manifest_size > max_bytes) ||
      (manifest_size > GAME_PACKAGE_MANIFEST_MAX_BYTES))
  {
    return TX_SIZE_ERROR;
  }

  *manifest_addr_out = manifest_addr;
  *manifest_size_out = manifest_size;
  *commit_install_index_out = 1U;
  *source_out = APP_STORAGE_PKG_SRC_DEFAULT_SLOT;
  return TX_SUCCESS;
}

static UINT AppStorageResolveManifestTargetWithInstallIndex(uint32_t *manifest_addr_out,
                                                            uint32_t *manifest_size_out,
                                                            uint8_t *commit_install_index_out,
                                                            ULONG *source_out)
{
  uint32_t manifest_addr = 0UL;
  uint32_t manifest_size;

  if ((manifest_addr_out == (uint32_t *)0) ||
      (manifest_size_out == (uint32_t *)0) ||
      (commit_install_index_out == (uint8_t *)0) ||
      (source_out == (ULONG *)0))
  {
    return TX_PTR_ERROR;
  }

  *manifest_addr_out = 0UL;
  *manifest_size_out = 0UL;
  *commit_install_index_out = 0U;
  *source_out = APP_STORAGE_PKG_SRC_NONE;

  if (g_storage_install_index_scan_done == 0UL)
  {
    (void)AppStorageInstallIndexLoad();
  }

  if (g_storage_install_index_valid != 0UL)
  {
    manifest_addr = (uint32_t)g_storage_install_index_manifest_addr;
    manifest_size = (uint32_t)g_storage_install_index_manifest_size;

    if ((manifest_size == 0UL) ||
        (manifest_size > (uint32_t)sizeof(g_storage_game_package_manifest_buf)) ||
        (manifest_size > GAME_PACKAGE_MANIFEST_MAX_BYTES) ||
        (manifest_addr >= APP_STORAGE_FLASH_SIZE_BYTES) ||
        (((uint64_t)manifest_addr + (uint64_t)manifest_size) > (uint64_t)APP_STORAGE_FLASH_SIZE_BYTES))
    {
      return TX_SIZE_ERROR;
    }

    *manifest_addr_out = manifest_addr;
    *manifest_size_out = manifest_size;
    *commit_install_index_out = 0U;
    *source_out = APP_STORAGE_PKG_SRC_INSTALL_INDEX;
    return TX_SUCCESS;
  }

  return AppStorageResolveDefaultManifestTarget(manifest_addr_out,
                                                manifest_size_out,
                                                commit_install_index_out,
                                                source_out);
}

static UINT AppStorageRunGamePackageManifestLoad(uint32_t manifest_addr, uint32_t manifest_size)
{
  return AppStorageRunGamePackageManifestLoadInternal(manifest_addr,
                                                      manifest_size,
                                                      1U,
                                                      APP_STORAGE_PKG_SRC_EXPLICIT_RAW);
}

static UINT AppStorageRunGamePackageManifestLoadDefault(void)
{
  UINT status;
  UINT resolve_status;
  uint32_t manifest_addr = 0UL;
  uint32_t manifest_size = 0UL;
  uint8_t commit_install_index = 0U;
  ULONG source_kind = APP_STORAGE_PKG_SRC_NONE;
  uint32_t fallback_manifest_addr = 0UL;
  uint32_t fallback_manifest_size = 0UL;
  uint8_t fallback_commit_install_index = 0U;
  ULONG fallback_source_kind = APP_STORAGE_PKG_SRC_NONE;

  g_storage_last_op = APP_STORAGE_OP_GAME_PACKAGE_MANIFEST_LOAD_DEFAULT;
  g_storage_last_error = APP_STORAGE_ERR_NONE;

  if (AppStorageRunFlashProbe() != TX_SUCCESS)
  {
    g_storage_last_op = APP_STORAGE_OP_GAME_PACKAGE_MANIFEST_LOAD_DEFAULT;
    g_storage_last_error = APP_STORAGE_ERR_BOOTINIT;
    g_storage_game_package_manifest_load_fail_count++;
    g_storage_game_package_manifest_last_status = (ULONG)TX_NOT_DONE;
    return TX_NOT_DONE;
  }
  g_storage_last_op = APP_STORAGE_OP_GAME_PACKAGE_MANIFEST_LOAD_DEFAULT;

  resolve_status = AppStorageResolveManifestTargetWithInstallIndex(&manifest_addr,
                                                                   &manifest_size,
                                                                   &commit_install_index,
                                                                   &source_kind);
  if (resolve_status != TX_SUCCESS)
  {
    /* Bounded recovery: if install-index resolution failed, try the fixed slot once. */
    if (g_storage_install_index_valid != 0UL)
    {
      resolve_status = AppStorageResolveDefaultManifestTarget(&fallback_manifest_addr,
                                                              &fallback_manifest_size,
                                                              &fallback_commit_install_index,
                                                              &fallback_source_kind);
      if (resolve_status == TX_SUCCESS)
      {
        status = AppStorageRunGamePackageManifestLoadInternal(fallback_manifest_addr,
                                                              fallback_manifest_size,
                                                              fallback_commit_install_index,
                                                              fallback_source_kind);
        g_storage_last_op = APP_STORAGE_OP_GAME_PACKAGE_MANIFEST_LOAD_DEFAULT;
        return status;
      }
    }

    g_storage_last_error = APP_STORAGE_ERR_GAME_PACKAGE_MANIFEST;
    g_storage_game_package_manifest_load_fail_count++;
    g_storage_game_package_manifest_last_status = (ULONG)resolve_status;
    return resolve_status;
  }

  status = AppStorageRunGamePackageManifestLoadInternal(manifest_addr,
                                                        manifest_size,
                                                        commit_install_index,
                                                        source_kind);
  g_storage_last_op = APP_STORAGE_OP_GAME_PACKAGE_MANIFEST_LOAD_DEFAULT;
  if ((status != TX_SUCCESS) && (source_kind == APP_STORAGE_PKG_SRC_INSTALL_INDEX))
  {
    /* If index-backed blob fails to load, retry once from fixed slot to self-heal index metadata. */
    resolve_status = AppStorageResolveDefaultManifestTarget(&fallback_manifest_addr,
                                                            &fallback_manifest_size,
                                                            &fallback_commit_install_index,
                                                            &fallback_source_kind);
    if (resolve_status == TX_SUCCESS)
    {
      status = AppStorageRunGamePackageManifestLoadInternal(fallback_manifest_addr,
                                                            fallback_manifest_size,
                                                            fallback_commit_install_index,
                                                            fallback_source_kind);
      g_storage_last_op = APP_STORAGE_OP_GAME_PACKAGE_MANIFEST_LOAD_DEFAULT;
    }
  }
  return status;
}

static UINT AppStorageRunGamePackageManifestImportFat(void)
{
  UINT status = TX_NOT_DONE;
  UINT unmount_status;
  UINT fx_status;
  ULONG actual_bytes = 0UL;
  uint8_t mounted_here = 0U;
  uint8_t file_opened = 0U;
  uint32_t manifest_addr = (uint32_t)KNOB_STORAGE_GAME_PKG_MANIFEST_ADDR;
  uint32_t max_bytes = (uint32_t)KNOB_STORAGE_GAME_PKG_MANIFEST_MAX_BYTES;
  uint32_t manifest_size = 0UL;
  HAL_StatusTypeDef hal_status;
  const game_package_manifest_header_t *header;
  game_package_manifest_view_t view;
  FX_FILE file;

  (void)memset(&view, 0, sizeof(view));
  (void)memset(&file, 0, sizeof(file));

  g_storage_last_op = APP_STORAGE_OP_GAME_PACKAGE_MANIFEST_IMPORT;
  g_storage_last_error = APP_STORAGE_ERR_NONE;
  g_storage_game_package_manifest_import_last_status = (ULONG)TX_NOT_DONE;
  g_storage_game_package_manifest_import_last_bytes = 0UL;

  if ((max_bytes == 0UL) ||
      (max_bytes > (uint32_t)sizeof(g_storage_game_package_manifest_buf)) ||
      (max_bytes > GAME_PACKAGE_MANIFEST_MAX_BYTES))
  {
    g_storage_last_error = APP_STORAGE_ERR_GAME_PACKAGE_MANIFEST;
    status = TX_SIZE_ERROR;
    goto done;
  }

  if (g_storage_filex_mounted == 0UL)
  {
    status = AppStorageRunFileXMount();
    if (status != TX_SUCCESS)
    {
      g_storage_last_op = APP_STORAGE_OP_GAME_PACKAGE_MANIFEST_IMPORT;
      if (g_storage_last_error == APP_STORAGE_ERR_NONE)
      {
        g_storage_last_error = APP_STORAGE_ERR_FILEX_MOUNT;
      }
      goto done;
    }
    mounted_here = 1U;
  }

  fx_status = fx_file_open(&g_storage_fx_media,
                           &file,
                           (CHAR *)APP_STORAGE_FAT_MANIFEST_PATH_PRIMARY,
                           FX_OPEN_FOR_READ);
  if (fx_status != FX_SUCCESS)
  {
    g_storage_filex_last_status = (ULONG)fx_status;
    fx_status = fx_file_open(&g_storage_fx_media,
                             &file,
                             (CHAR *)APP_STORAGE_FAT_MANIFEST_PATH_FALLBACK,
                             FX_OPEN_FOR_READ);
  }
  g_storage_filex_last_status = (ULONG)fx_status;
  if (fx_status != FX_SUCCESS)
  {
    g_storage_last_error = APP_STORAGE_ERR_GAME_PACKAGE_MANIFEST;
    status = TX_NOT_DONE;
    goto done;
  }
  file_opened = 1U;

  if ((file.fx_file_current_file_size == 0ULL) ||
      (file.fx_file_current_file_size > (ULONG64)max_bytes) ||
      (file.fx_file_current_file_size > (ULONG64)sizeof(g_storage_game_package_manifest_buf)))
  {
    g_storage_last_error = APP_STORAGE_ERR_GAME_PACKAGE_MANIFEST;
    status = TX_SIZE_ERROR;
    goto done;
  }

  manifest_size = (uint32_t)file.fx_file_current_file_size;
  (void)memset(g_storage_game_package_manifest_buf, 0, sizeof(g_storage_game_package_manifest_buf));
  fx_status = fx_file_read(&file,
                           g_storage_game_package_manifest_buf,
                           manifest_size,
                           &actual_bytes);
  g_storage_filex_last_status = (ULONG)fx_status;
  if ((fx_status != FX_SUCCESS) || (actual_bytes != (ULONG)manifest_size))
  {
    g_storage_last_error = APP_STORAGE_ERR_GAME_PACKAGE_MANIFEST;
    status = TX_NOT_DONE;
    goto done;
  }

  status = GamePackageManifest_Parse((const void *)g_storage_game_package_manifest_buf,
                                     manifest_size,
                                     &view);
  if (status != TX_SUCCESS)
  {
    g_storage_last_error = APP_STORAGE_ERR_GAME_PACKAGE_MANIFEST;
    goto done;
  }

  header = (const game_package_manifest_header_t *)g_storage_game_package_manifest_buf;
  if (header->total_size != manifest_size)
  {
    g_storage_last_error = APP_STORAGE_ERR_GAME_PACKAGE_MANIFEST;
    status = TX_SIZE_ERROR;
    goto done;
  }

  if (AppStorageRunFlashProbe() != TX_SUCCESS)
  {
    g_storage_last_error = APP_STORAGE_ERR_BOOTINIT;
    status = TX_NOT_DONE;
    goto done;
  }

  status = AppStorageEraseRange4K(manifest_addr, max_bytes);
  if (status != TX_SUCCESS)
  {
    g_storage_last_error = APP_STORAGE_ERR_ERASE;
    goto done;
  }

  hal_status = AT25_PageProgram(&hospi1, manifest_addr, g_storage_game_package_manifest_buf, manifest_size);
  AppStorageCaptureDebug();
  if (hal_status != HAL_OK)
  {
    g_storage_last_error = APP_STORAGE_ERR_PROGRAM;
    status = TX_NOT_DONE;
    goto done;
  }

  status = AppStorageRunGamePackageManifestLoadInternal(manifest_addr,
                                                        manifest_size,
                                                        1U,
                                                        APP_STORAGE_PKG_SRC_DEFAULT_SLOT);
  if (status != TX_SUCCESS)
  {
    g_storage_last_op = APP_STORAGE_OP_GAME_PACKAGE_MANIFEST_IMPORT;
    if (g_storage_last_error == APP_STORAGE_ERR_NONE)
    {
      g_storage_last_error = APP_STORAGE_ERR_GAME_PACKAGE_MANIFEST;
    }
    goto done;
  }

  g_storage_game_package_manifest_import_last_bytes = (ULONG)manifest_size;
  g_storage_last_op = APP_STORAGE_OP_GAME_PACKAGE_MANIFEST_IMPORT;
  g_storage_last_error = APP_STORAGE_ERR_NONE;

done:
  if (file_opened != 0U)
  {
    fx_status = fx_file_close(&file);
    g_storage_filex_last_status = (ULONG)fx_status;
    if ((fx_status != FX_SUCCESS) && (status == TX_SUCCESS))
    {
      status = TX_NOT_DONE;
      if (g_storage_last_error == APP_STORAGE_ERR_NONE)
      {
        g_storage_last_error = APP_STORAGE_ERR_FILEX_UNMOUNT;
      }
    }
  }

  if (mounted_here != 0U)
  {
    unmount_status = AppStorageRunFileXUnmount();
    if ((unmount_status != TX_SUCCESS) && (status == TX_SUCCESS))
    {
      status = unmount_status;
      if (g_storage_last_error == APP_STORAGE_ERR_NONE)
      {
        g_storage_last_error = APP_STORAGE_ERR_FILEX_UNMOUNT;
      }
    }
  }

  g_storage_last_op = APP_STORAGE_OP_GAME_PACKAGE_MANIFEST_IMPORT;
  g_storage_game_package_manifest_import_last_status = (ULONG)status;
  if (status == TX_SUCCESS)
  {
    if (g_storage_game_package_manifest_import_ok_count < 0xFFFFFFFFUL)
    {
      g_storage_game_package_manifest_import_ok_count++;
    }
  }
  else if (g_storage_game_package_manifest_import_fail_count < 0xFFFFFFFFUL)
  {
    g_storage_game_package_manifest_import_fail_count++;
  }

  return status;
}

static const game_package_runtime_config_t *AppStorageResolvePrimaryRuntimeConfig(void)
{
  const game_package_desc_t *pkg;
  const game_package_runtime_config_t *cfg = (const game_package_runtime_config_t *)0;
  uint32_t selected_mode_id = 0UL;
  uint32_t route_index;

  pkg = GamePackage_GetActive();
  if (pkg == (const game_package_desc_t *)0)
  {
    return (const game_package_runtime_config_t *)0;
  }

  if ((pkg->pet_routes != (const game_package_pet_route_t *)0) && (pkg->pet_route_count > 0U))
  {
    for (route_index = 0UL; route_index < pkg->pet_route_count; route_index++)
    {
      if (pkg->pet_routes[route_index].pet_entry_id == (uint32_t)GAME_PET_ENTRY_START_GAME)
      {
        selected_mode_id = pkg->pet_routes[route_index].mode_id;
        break;
      }
    }
  }

  if ((selected_mode_id == 0UL) && (pkg->modes != (const game_package_mode_desc_t *)0) && (pkg->mode_count > 0U))
  {
    selected_mode_id = pkg->modes[0].mode_id;
  }

  if (selected_mode_id != 0UL)
  {
    cfg = GamePackage_GetRuntimeConfigByModeId(selected_mode_id);
  }
  if ((cfg == (const game_package_runtime_config_t *)0) &&
      (pkg->modes != (const game_package_mode_desc_t *)0) &&
      (pkg->mode_count > 0U))
  {
    cfg = &pkg->modes[0].runtime_config;
  }

  return cfg;
}

static uint8_t AppStorageResolveSceneSlotAddrsFromIndex(uint32_t slot_index,
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

static uint8_t AppStorageResolveSceneImportTargetAddrs(uint32_t *map_addr_out,
                                                       uint32_t *tileset_addr_out)
{
  const game_package_runtime_config_t *cfg;
  uint32_t i;

  if ((map_addr_out == (uint32_t *)0) || (tileset_addr_out == (uint32_t *)0))
  {
    return 0U;
  }

  *map_addr_out = (uint32_t)KNOB_GAME_RT_SCENE_MAP_ADDR;
  *tileset_addr_out = (uint32_t)KNOB_GAME_RT_SCENE_TILESET_ADDR;

  if (g_storage_game_package_manifest_loaded == 0UL)
  {
    return 1U;
  }

  cfg = AppStorageResolvePrimaryRuntimeConfig();
  if ((cfg == (const game_package_runtime_config_t *)0) || (cfg->scene_map_id == 0UL))
  {
    return 1U;
  }

  for (i = 0UL; i < (uint32_t)GAME_MAP_REGISTRY_ENTRY_COUNT; i++)
  {
    const game_map_registry_entry_t *entry = &g_game_map_registry_entries[i];
    if ((entry != (const game_map_registry_entry_t *)0) &&
        (entry->scene_map_id == cfg->scene_map_id))
    {
      return AppStorageResolveSceneSlotAddrsFromIndex(entry->slot_index, map_addr_out, tileset_addr_out);
    }
  }

  return 1U;
}

static UINT AppStorageRunGamePackageSceneImportFat(void)
{
  UINT status = TX_NOT_DONE;
  UINT unmount_status;
  UINT fx_status;
  UINT map_load_status;
  UINT tileset_load_status;
  ULONG actual_bytes = 0UL;
  uint8_t mounted_here = 0U;
  uint8_t map_opened = 0U;
  uint8_t tileset_opened = 0U;
  const uint32_t map_slot_size = (uint32_t)KNOB_GAME_RT_SCENE_MAP_SIZE_BYTES;
  const uint32_t tileset_slot_size = (uint32_t)KNOB_GAME_RT_SCENE_TILESET_SIZE_BYTES;
  uint32_t map_addr = (uint32_t)KNOB_GAME_RT_SCENE_MAP_ADDR;
  uint32_t tileset_addr = (uint32_t)KNOB_GAME_RT_SCENE_TILESET_ADDR;
  uint32_t map_size = 0UL;
  uint32_t tileset_size = 0UL;
  uint32_t map_erase_size;
  uint32_t tileset_erase_size;
  uint64_t map_slot_end;
  uint64_t tileset_slot_end;
  HAL_StatusTypeDef hal_status;
  FX_FILE map_file;
  FX_FILE tileset_file;

  (void)memset(&map_file, 0, sizeof(map_file));
  (void)memset(&tileset_file, 0, sizeof(tileset_file));

  g_storage_last_op = APP_STORAGE_OP_GAME_PACKAGE_SCENE_IMPORT;
  g_storage_last_error = APP_STORAGE_ERR_NONE;
  g_storage_game_package_scene_import_last_status = (ULONG)TX_NOT_DONE;
  g_storage_game_package_scene_import_map_bytes = 0UL;
  g_storage_game_package_scene_import_tileset_bytes = 0UL;

  if (AppStorageResolveSceneImportTargetAddrs(&map_addr, &tileset_addr) == 0U)
  {
    g_storage_last_error = APP_STORAGE_ERR_RANGE;
    status = TX_PTR_ERROR;
    goto done;
  }

  if (g_storage_filex_mounted == 0UL)
  {
    status = AppStorageRunFileXMount();
    if (status != TX_SUCCESS)
    {
      g_storage_last_op = APP_STORAGE_OP_GAME_PACKAGE_SCENE_IMPORT;
      if (g_storage_last_error == APP_STORAGE_ERR_NONE)
      {
        g_storage_last_error = APP_STORAGE_ERR_FILEX_MOUNT;
      }
      goto done;
    }
    mounted_here = 1U;
  }

  fx_status = fx_file_open(&g_storage_fx_media,
                           &map_file,
                           (CHAR *)APP_STORAGE_FAT_SCENE_MAP_PATH_PRIMARY,
                           FX_OPEN_FOR_READ);
  if (fx_status != FX_SUCCESS)
  {
    g_storage_filex_last_status = (ULONG)fx_status;
    fx_status = fx_file_open(&g_storage_fx_media,
                             &map_file,
                             (CHAR *)APP_STORAGE_FAT_SCENE_MAP_PATH_FALLBACK,
                             FX_OPEN_FOR_READ);
  }
  g_storage_filex_last_status = (ULONG)fx_status;
  if (fx_status != FX_SUCCESS)
  {
    g_storage_last_error = APP_STORAGE_ERR_GAME_MAP;
    status = TX_NOT_DONE;
    goto done;
  }
  map_opened = 1U;

  if ((map_file.fx_file_current_file_size == 0ULL) ||
      (map_file.fx_file_current_file_size > (ULONG64)sizeof(g_storage_scene_map_blob_buf)))
  {
    g_storage_last_error = APP_STORAGE_ERR_GAME_MAP;
    status = TX_SIZE_ERROR;
    goto done;
  }
  map_size = (uint32_t)map_file.fx_file_current_file_size;
  (void)memset(g_storage_scene_map_blob_buf, 0, sizeof(g_storage_scene_map_blob_buf));
  fx_status = fx_file_read(&map_file,
                           g_storage_scene_map_blob_buf,
                           map_size,
                           &actual_bytes);
  g_storage_filex_last_status = (ULONG)fx_status;
  if ((fx_status != FX_SUCCESS) || (actual_bytes != (ULONG)map_size))
  {
    g_storage_last_error = APP_STORAGE_ERR_GAME_MAP;
    status = TX_NOT_DONE;
    goto done;
  }

  status = GameRuntime_LoadSceneMapBlob((const void *)g_storage_scene_map_blob_buf, map_size);
  GameRuntime_ClearSceneMap();
  if (status != TX_SUCCESS)
  {
    g_storage_last_error = APP_STORAGE_ERR_GAME_MAP;
    goto done;
  }
  g_storage_game_package_scene_import_map_bytes = (ULONG)map_size;

  fx_status = fx_file_close(&map_file);
  g_storage_filex_last_status = (ULONG)fx_status;
  map_opened = 0U;
  if (fx_status != FX_SUCCESS)
  {
    g_storage_last_error = APP_STORAGE_ERR_FILEX_UNMOUNT;
    status = TX_NOT_DONE;
    goto done;
  }

  fx_status = fx_file_open(&g_storage_fx_media,
                           &tileset_file,
                           (CHAR *)APP_STORAGE_FAT_SCENE_TILESET_PATH_PRIMARY,
                           FX_OPEN_FOR_READ);
  if (fx_status != FX_SUCCESS)
  {
    g_storage_filex_last_status = (ULONG)fx_status;
    fx_status = fx_file_open(&g_storage_fx_media,
                             &tileset_file,
                             (CHAR *)APP_STORAGE_FAT_SCENE_TILESET_PATH_FALLBACK,
                             FX_OPEN_FOR_READ);
  }
  g_storage_filex_last_status = (ULONG)fx_status;
  if (fx_status != FX_SUCCESS)
  {
    g_storage_last_error = APP_STORAGE_ERR_GAME_TILESET;
    status = TX_NOT_DONE;
    goto done;
  }
  tileset_opened = 1U;

  if ((tileset_file.fx_file_current_file_size == 0ULL) ||
      (tileset_file.fx_file_current_file_size > (ULONG64)sizeof(g_storage_scene_tileset_blob_buf)))
  {
    g_storage_last_error = APP_STORAGE_ERR_GAME_TILESET;
    status = TX_SIZE_ERROR;
    goto done;
  }
  tileset_size = (uint32_t)tileset_file.fx_file_current_file_size;
  (void)memset(g_storage_scene_tileset_blob_buf, 0, sizeof(g_storage_scene_tileset_blob_buf));
  fx_status = fx_file_read(&tileset_file,
                           g_storage_scene_tileset_blob_buf,
                           tileset_size,
                           &actual_bytes);
  g_storage_filex_last_status = (ULONG)fx_status;
  if ((fx_status != FX_SUCCESS) || (actual_bytes != (ULONG)tileset_size))
  {
    g_storage_last_error = APP_STORAGE_ERR_GAME_TILESET;
    status = TX_NOT_DONE;
    goto done;
  }

  status = GameRuntime_LoadSceneTilesetBlob((const void *)g_storage_scene_tileset_blob_buf, tileset_size);
  GameRuntime_ClearSceneTileset();
  if (status != TX_SUCCESS)
  {
    g_storage_last_error = APP_STORAGE_ERR_GAME_TILESET;
    goto done;
  }
  g_storage_game_package_scene_import_tileset_bytes = (ULONG)tileset_size;

  if ((map_slot_size == 0UL) || (tileset_slot_size == 0UL) ||
      (map_size > map_slot_size) || (tileset_size > tileset_slot_size))
  {
    g_storage_last_error = APP_STORAGE_ERR_RANGE;
    status = TX_SIZE_ERROR;
    goto done;
  }

  map_slot_end = (uint64_t)map_addr + (uint64_t)map_slot_size;
  tileset_slot_end = (uint64_t)tileset_addr + (uint64_t)tileset_slot_size;
  if ((map_slot_end > (uint64_t)APP_STORAGE_FLASH_SIZE_BYTES) ||
      (tileset_slot_end > (uint64_t)APP_STORAGE_FLASH_SIZE_BYTES))
  {
    g_storage_last_error = APP_STORAGE_ERR_RANGE;
    status = TX_SIZE_ERROR;
    goto done;
  }

  if (!((map_slot_end <= (uint64_t)tileset_addr) ||
        (tileset_slot_end <= (uint64_t)map_addr)))
  {
    g_storage_last_error = APP_STORAGE_ERR_RANGE;
    status = TX_SIZE_ERROR;
    goto done;
  }

  fx_status = fx_file_close(&tileset_file);
  g_storage_filex_last_status = (ULONG)fx_status;
  tileset_opened = 0U;
  if (fx_status != FX_SUCCESS)
  {
    g_storage_last_error = APP_STORAGE_ERR_FILEX_UNMOUNT;
    status = TX_NOT_DONE;
    goto done;
  }

  if (AppStorageRunFlashProbe() != TX_SUCCESS)
  {
    g_storage_last_error = APP_STORAGE_ERR_BOOTINIT;
    status = TX_NOT_DONE;
    goto done;
  }

  map_erase_size = (map_size + (uint32_t)APP_STORAGE_SMOKE_SECTOR_SIZE - 1UL) &
                   ~((uint32_t)APP_STORAGE_SMOKE_SECTOR_SIZE - 1UL);
  tileset_erase_size = (tileset_size + (uint32_t)APP_STORAGE_SMOKE_SECTOR_SIZE - 1UL) &
                       ~((uint32_t)APP_STORAGE_SMOKE_SECTOR_SIZE - 1UL);
  if ((map_erase_size == 0UL) || (tileset_erase_size == 0UL) ||
      (map_erase_size < map_size) || (tileset_erase_size < tileset_size))
  {
    g_storage_last_error = APP_STORAGE_ERR_RANGE;
    status = TX_SIZE_ERROR;
    goto done;
  }

  if (((uint64_t)map_addr + (uint64_t)map_erase_size) > (uint64_t)APP_STORAGE_FLASH_SIZE_BYTES)
  {
    g_storage_last_error = APP_STORAGE_ERR_RANGE;
    status = TX_SIZE_ERROR;
    goto done;
  }
  if (((uint64_t)tileset_addr + (uint64_t)tileset_erase_size) > (uint64_t)APP_STORAGE_FLASH_SIZE_BYTES)
  {
    g_storage_last_error = APP_STORAGE_ERR_RANGE;
    status = TX_SIZE_ERROR;
    goto done;
  }

  status = AppStorageEraseRange4K(map_addr, map_erase_size);
  if (status != TX_SUCCESS)
  {
    g_storage_last_error = APP_STORAGE_ERR_ERASE;
    goto done;
  }
  hal_status = AT25_PageProgram(&hospi1, map_addr, g_storage_scene_map_blob_buf, map_size);
  AppStorageCaptureDebug();
  if (hal_status != HAL_OK)
  {
    g_storage_last_error = APP_STORAGE_ERR_PROGRAM;
    status = TX_NOT_DONE;
    goto done;
  }

  status = AppStorageEraseRange4K(tileset_addr, tileset_erase_size);
  if (status != TX_SUCCESS)
  {
    g_storage_last_error = APP_STORAGE_ERR_ERASE;
    goto done;
  }
  hal_status = AT25_PageProgram(&hospi1, tileset_addr, g_storage_scene_tileset_blob_buf, tileset_size);
  AppStorageCaptureDebug();
  if (hal_status != HAL_OK)
  {
    g_storage_last_error = APP_STORAGE_ERR_PROGRAM;
    status = TX_NOT_DONE;
    goto done;
  }

  map_load_status = AppStorageRunSceneMapLoad(map_addr, map_size);
  if (map_load_status != TX_SUCCESS)
  {
    g_storage_last_op = APP_STORAGE_OP_GAME_PACKAGE_SCENE_IMPORT;
    if (g_storage_last_error == APP_STORAGE_ERR_NONE)
    {
      g_storage_last_error = APP_STORAGE_ERR_GAME_MAP;
    }
    status = map_load_status;
    goto done;
  }

  tileset_load_status = AppStorageRunSceneTilesetLoad(tileset_addr, tileset_size);
  if (tileset_load_status != TX_SUCCESS)
  {
    g_storage_last_op = APP_STORAGE_OP_GAME_PACKAGE_SCENE_IMPORT;
    if (g_storage_last_error == APP_STORAGE_ERR_NONE)
    {
      g_storage_last_error = APP_STORAGE_ERR_GAME_TILESET;
    }
    status = tileset_load_status;
    goto done;
  }

  status = TX_SUCCESS;
  g_storage_last_op = APP_STORAGE_OP_GAME_PACKAGE_SCENE_IMPORT;
  g_storage_last_error = APP_STORAGE_ERR_NONE;

done:
  if (map_opened != 0U)
  {
    fx_status = fx_file_close(&map_file);
    g_storage_filex_last_status = (ULONG)fx_status;
    if ((fx_status != FX_SUCCESS) && (status == TX_SUCCESS))
    {
      status = TX_NOT_DONE;
      if (g_storage_last_error == APP_STORAGE_ERR_NONE)
      {
        g_storage_last_error = APP_STORAGE_ERR_FILEX_UNMOUNT;
      }
    }
  }

  if (tileset_opened != 0U)
  {
    fx_status = fx_file_close(&tileset_file);
    g_storage_filex_last_status = (ULONG)fx_status;
    if ((fx_status != FX_SUCCESS) && (status == TX_SUCCESS))
    {
      status = TX_NOT_DONE;
      if (g_storage_last_error == APP_STORAGE_ERR_NONE)
      {
        g_storage_last_error = APP_STORAGE_ERR_FILEX_UNMOUNT;
      }
    }
  }

  if (mounted_here != 0U)
  {
    unmount_status = AppStorageRunFileXUnmount();
    if ((unmount_status != TX_SUCCESS) && (status == TX_SUCCESS))
    {
      status = unmount_status;
      if (g_storage_last_error == APP_STORAGE_ERR_NONE)
      {
        g_storage_last_error = APP_STORAGE_ERR_FILEX_UNMOUNT;
      }
    }
  }

  g_storage_last_op = APP_STORAGE_OP_GAME_PACKAGE_SCENE_IMPORT;
  g_storage_game_package_scene_import_last_status = (ULONG)status;
  if (status == TX_SUCCESS)
  {
    if (g_storage_game_package_scene_import_ok_count < 0xFFFFFFFFUL)
    {
      g_storage_game_package_scene_import_ok_count++;
    }
  }
  else if (g_storage_game_package_scene_import_fail_count < 0xFFFFFFFFUL)
  {
    g_storage_game_package_scene_import_fail_count++;
  }

  return status;
}

static UINT AppStorageEraseRange4K(uint32_t erase_addr, uint32_t erase_size)
{
  uint32_t addr;
  uint32_t end_addr;
  HAL_StatusTypeDef hal_status;

  if ((erase_size == 0UL) ||
      ((erase_addr % APP_STORAGE_SMOKE_SECTOR_SIZE) != 0UL) ||
      ((erase_size % APP_STORAGE_SMOKE_SECTOR_SIZE) != 0UL))
  {
    return TX_SIZE_ERROR;
  }

  end_addr = erase_addr + erase_size;
  if ((end_addr < erase_addr) || ((uint64_t)end_addr > (uint64_t)APP_STORAGE_FLASH_SIZE_BYTES))
  {
    return TX_SIZE_ERROR;
  }

  for (addr = erase_addr; addr < end_addr; addr += APP_STORAGE_SMOKE_SECTOR_SIZE)
  {
    hal_status = AT25_Erase4K(&hospi1, addr);
    AppStorageCaptureDebug();
    if (hal_status != HAL_OK)
    {
      g_storage_last_erase_addr = (ULONG)addr;
      g_storage_last_erase_size = (ULONG)APP_STORAGE_SMOKE_SECTOR_SIZE;
      return TX_NOT_DONE;
    }
  }

  g_storage_last_erase_addr = (ULONG)erase_addr;
  g_storage_last_erase_size = (ULONG)erase_size;
  return TX_SUCCESS;
}

static UINT AppStorageRunGamePackageManifestErase(void)
{
  uint32_t manifest_addr = (uint32_t)KNOB_STORAGE_GAME_PKG_MANIFEST_ADDR;
  uint32_t manifest_size = (uint32_t)KNOB_STORAGE_GAME_PKG_MANIFEST_MAX_BYTES;
  UINT status;

  g_storage_last_op = APP_STORAGE_OP_GAME_PACKAGE_MANIFEST_ERASE;
  g_storage_last_error = APP_STORAGE_ERR_NONE;

  if (AppStorageRunFlashProbe() != TX_SUCCESS)
  {
    g_storage_last_op = APP_STORAGE_OP_GAME_PACKAGE_MANIFEST_ERASE;
    g_storage_last_error = APP_STORAGE_ERR_BOOTINIT;
    g_storage_game_package_manifest_erase_fail_count++;
    return TX_NOT_DONE;
  }
  g_storage_last_op = APP_STORAGE_OP_GAME_PACKAGE_MANIFEST_ERASE;

  status = AppStorageEraseRange4K(manifest_addr, manifest_size);
  if (status != TX_SUCCESS)
  {
    g_storage_last_error = APP_STORAGE_ERR_ERASE;
    g_storage_game_package_manifest_erase_fail_count++;
    return status;
  }

  status = AppStorageEraseRange4K((uint32_t)APP_STORAGE_INSTALL_INDEX_SLOT0_ADDR,
                                  (uint32_t)APP_STORAGE_INSTALL_INDEX_RESERVED_BYTES);
  if (status != TX_SUCCESS)
  {
    g_storage_last_error = APP_STORAGE_ERR_ERASE;
    g_storage_game_package_manifest_erase_fail_count++;
    return status;
  }

  GamePackage_ClearLoadedManifest();
  g_storage_game_package_manifest_loaded = 0UL;
  g_storage_game_package_manifest_addr = 0UL;
  g_storage_game_package_manifest_size = 0UL;
  g_storage_game_package_manifest_last_status = (ULONG)TX_NOT_DONE;
  g_storage_game_package_scene_import_last_status = (ULONG)TX_NOT_DONE;
  g_storage_game_package_scene_import_map_bytes = 0UL;
  g_storage_game_package_scene_import_tileset_bytes = 0UL;
  g_storage_game_package_id = 0UL;
  g_storage_game_package_version = 0UL;
  g_storage_game_package_mode_count = 0UL;
  g_storage_game_package_pet_route_count = 0UL;
  g_storage_game_package_source = APP_STORAGE_PKG_SRC_NONE;
  (void)memset(g_storage_game_package_manifest_buf, 0, sizeof(g_storage_game_package_manifest_buf));
  AppStorageInstallIndexClearActive();
  g_storage_install_index_load_last_status = (ULONG)TX_NOT_DONE;
  g_storage_install_index_write_last_status = (ULONG)TX_NOT_DONE;
  g_storage_install_index_scan_done = 0UL;

  g_storage_game_package_manifest_erase_ok_count++;
  return TX_SUCCESS;
}

static UINT AppStorageRunGamePackageManifestWriteTest(void)
{
  game_package_manifest_header_v3_t header;
  game_package_manifest_mode_v5_t modes[3];
  game_package_manifest_pet_route_t routes[1];
  uint32_t manifest_addr = (uint32_t)KNOB_STORAGE_GAME_PKG_MANIFEST_ADDR;
  uint32_t max_bytes = (uint32_t)KNOB_STORAGE_GAME_PKG_MANIFEST_MAX_BYTES;
  uint32_t modes_offset;
  uint32_t routes_offset;
  uint32_t total_size;
  uint32_t crc32;
  UINT status;
  HAL_StatusTypeDef hal_status;

  g_storage_last_op = APP_STORAGE_OP_GAME_PACKAGE_MANIFEST_WRITE_TEST;
  g_storage_last_error = APP_STORAGE_ERR_NONE;

  if ((max_bytes == 0UL) ||
      (max_bytes > (uint32_t)sizeof(g_storage_game_package_manifest_buf)) ||
      (max_bytes > GAME_PACKAGE_MANIFEST_MAX_BYTES))
  {
    g_storage_last_error = APP_STORAGE_ERR_GAME_PACKAGE_MANIFEST;
    return TX_SIZE_ERROR;
  }

  if (AppStorageRunFlashProbe() != TX_SUCCESS)
  {
    g_storage_last_op = APP_STORAGE_OP_GAME_PACKAGE_MANIFEST_WRITE_TEST;
    g_storage_last_error = APP_STORAGE_ERR_BOOTINIT;
    return TX_NOT_DONE;
  }
  g_storage_last_op = APP_STORAGE_OP_GAME_PACKAGE_MANIFEST_WRITE_TEST;

  status = AppStorageEraseRange4K(manifest_addr, max_bytes);
  if (status != TX_SUCCESS)
  {
    g_storage_last_error = APP_STORAGE_ERR_ERASE;
    return status;
  }

  (void)memset(&header, 0, sizeof(header));
  (void)memset(modes, 0, sizeof(modes));
  (void)memset(routes, 0, sizeof(routes));
  (void)memset(g_storage_game_package_manifest_buf, 0, sizeof(g_storage_game_package_manifest_buf));

  modes[0].mode_id = 1UL;
  modes[0].runtime_kind = (uint16_t)GAME_PACKAGE_RT_KIND_TOPDOWN;
  modes[0].backend_id = (uint16_t)GAME_RUNTIME_BACKEND_TOPDOWN_BASIC;
  modes[0].scene_map_addr = (uint32_t)KNOB_STORAGE_INSTALLED_BASE_ADDR;
  modes[0].scene_map_size_bytes = 0UL;
  modes[0].scene_tileset_addr = (uint32_t)KNOB_STORAGE_INSTALLED_BASE_ADDR + 0x00001000UL;
  modes[0].scene_tileset_size_bytes = 0UL;
  modes[0].topdown_render_scale = 2UL;
  modes[0].topdown_tile_present_mode = (uint32_t)GAME_PACKAGE_TOPDOWN_PRESENT_AUTO;
  modes[0].controller_profile_id = (uint32_t)GAME_PACKAGE_CONTROLLER_PROFILE_TOPDOWN_ANALOG;
  modes[0].camera_profile_id = (uint32_t)GAME_PACKAGE_CAMERA_PROFILE_FOLLOW_DEADZONE;
  modes[0].input_deadzone_permille = 150UL;
  modes[0].input_flags = (uint32_t)(GAME_PACKAGE_INPUT_FLAG_NORMALIZE_DIAGONAL |
                                    GAME_PACKAGE_INPUT_FLAG_ANALOG_PREFERRED);
  modes[0].move_speed_px_s = 72UL;
  modes[0].move_accel_px_s2 = 480UL;
  modes[0].move_decel_px_s2 = 640UL;
  modes[0].camera_deadzone_w_px = 24UL;
  modes[0].camera_deadzone_h_px = 24UL;
  modes[0].camera_follow_permille = 280UL;
  modes[0].camera_max_speed_px_s = 240UL;
  modes[0].camera_lookahead_x_px = 16;
  modes[0].camera_lookahead_y_px = 16;
  modes[0].scene_map_id = 1001UL;
  modes[0].scene_tileset_id = 2001UL;
  modes[0].music_asset_id = 3001UL;
  modes[0].sfx_interact_asset_id = 3002UL;
  modes[0].sfx_confirm_asset_id = 3003UL;
  modes[0].sfx_error_asset_id = 3004UL;
  modes[0].scene_lifecycle = (uint32_t)GAME_PACKAGE_SCENE_LIFECYCLE_RESUMABLE;
  modes[0].resume_domain_id = 1UL;

  modes[1].mode_id = 2UL;
  modes[1].runtime_kind = (uint16_t)GAME_PACKAGE_RT_KIND_SIDESCROLL;
  modes[1].backend_id = (uint16_t)GAME_RUNTIME_BACKEND_RENDER_DEMO_3D_WALK;
  modes[1].scene_map_id = 1002UL;
  modes[1].scene_tileset_id = 2002UL;
  modes[1].scene_lifecycle = (uint32_t)GAME_PACKAGE_SCENE_LIFECYCLE_TRANSIENT;
  modes[1].resume_domain_id = 0UL;

  modes[2] = modes[0];
  modes[2].mode_id = 3UL;
  modes[2].scene_map_id = 1003UL;
  modes[2].scene_tileset_id = 2003UL;
  modes[2].music_asset_id = 3001UL;
  modes[2].sfx_interact_asset_id = 3002UL;
  modes[2].sfx_confirm_asset_id = 3003UL;
  modes[2].sfx_error_asset_id = 3004UL;
  modes[2].scene_lifecycle = (uint32_t)GAME_PACKAGE_SCENE_LIFECYCLE_TRANSIENT;
  modes[2].resume_domain_id = 0UL;

  routes[0].pet_entry_id = (uint16_t)GAME_PET_ENTRY_START_GAME;
  routes[0].mode_id = 1UL;

  modes_offset = (uint32_t)sizeof(game_package_manifest_header_v3_t);
  routes_offset = modes_offset + (uint32_t)sizeof(modes);
  total_size = routes_offset + (uint32_t)sizeof(routes);

  if ((modes_offset & 0x3UL) != 0UL ||
      (routes_offset & 0x3UL) != 0UL ||
      (total_size > max_bytes) ||
      (total_size > GAME_PACKAGE_MANIFEST_MAX_BYTES))
  {
    g_storage_last_error = APP_STORAGE_ERR_GAME_PACKAGE_MANIFEST;
    return TX_SIZE_ERROR;
  }

  header.magic = GAME_PACKAGE_MANIFEST_MAGIC;
  header.version = GAME_PACKAGE_MANIFEST_VERSION;
  header.header_size = (uint16_t)sizeof(game_package_manifest_header_v3_t);
  header.total_size = total_size;
  header.crc32 = 0UL;
  header.package_id = 1UL;
  header.package_version = 1UL;
  header.mode_count = 3U;
  header.pet_route_count = 1U;
  header.modes_offset = modes_offset;
  header.pet_routes_offset = routes_offset;
  header.pet_menu_item_count = 0U;
  header.reserved1 = 0U;
  header.pet_menu_items_offset = 0UL;

  (void)memcpy(g_storage_game_package_manifest_buf, &header, sizeof(header));
  (void)memcpy(g_storage_game_package_manifest_buf + modes_offset, modes, sizeof(modes));
  (void)memcpy(g_storage_game_package_manifest_buf + routes_offset, routes, sizeof(routes));

  crc32 = AppStorageCrc32(g_storage_game_package_manifest_buf, total_size);
  (void)memcpy(g_storage_game_package_manifest_buf + offsetof(game_package_manifest_header_t, crc32),
               &crc32,
               sizeof(crc32));

  hal_status = AT25_PageProgram(&hospi1, manifest_addr, g_storage_game_package_manifest_buf, total_size);
  AppStorageCaptureDebug();
  if (hal_status != HAL_OK)
  {
    g_storage_last_error = APP_STORAGE_ERR_PROGRAM;
    return TX_NOT_DONE;
  }

  status = AppStorageRunGamePackageManifestLoadInternal(manifest_addr,
                                                        total_size,
                                                        1U,
                                                        APP_STORAGE_PKG_SRC_DEFAULT_SLOT);
  g_storage_last_op = APP_STORAGE_OP_GAME_PACKAGE_MANIFEST_WRITE_TEST;
  if (status != TX_SUCCESS)
  {
    g_storage_last_error = APP_STORAGE_ERR_GAME_PACKAGE_MANIFEST;
    return status;
  }

  g_storage_last_error = APP_STORAGE_ERR_NONE;
  return TX_SUCCESS;
}

static UINT AppStorageRunSceneMapLoad(uint32_t map_addr, uint32_t map_size)
{
  HAL_StatusTypeDef hal_status;
  UINT status;
  const game_map_view_t *map_view;
  uint32_t resolved_size = map_size;
  game_map_blob_header_t header_probe;

  g_storage_last_op = APP_STORAGE_OP_SCENE_MAP_LOAD;
  g_storage_last_error = APP_STORAGE_ERR_NONE;
  g_storage_scene_map_loaded = 0UL;
  g_storage_scene_map_addr = (ULONG)map_addr;
  g_storage_scene_map_size = 0UL;
  g_storage_scene_map_last_status = (ULONG)TX_NOT_DONE;
  g_storage_scene_map_width = 0UL;
  g_storage_scene_map_height = 0UL;
  g_storage_scene_map_tile_count = 0UL;
  g_storage_scene_map_object_count = 0UL;
  (void)memset(g_storage_scene_map_blob_buf, 0, sizeof(g_storage_scene_map_blob_buf));

  if (map_addr >= APP_STORAGE_FLASH_SIZE_BYTES)
  {
    g_storage_last_error = APP_STORAGE_ERR_RANGE;
    g_storage_scene_map_load_fail_count++;
    g_storage_scene_map_last_status = (ULONG)TX_SIZE_ERROR;
    return TX_SIZE_ERROR;
  }

  if (AppStorageRunFlashProbe() != TX_SUCCESS)
  {
    g_storage_last_op = APP_STORAGE_OP_SCENE_MAP_LOAD;
    g_storage_last_error = APP_STORAGE_ERR_BOOTINIT;
    g_storage_scene_map_load_fail_count++;
    g_storage_scene_map_last_status = (ULONG)TX_NOT_DONE;
    return TX_NOT_DONE;
  }
  g_storage_last_op = APP_STORAGE_OP_SCENE_MAP_LOAD;

  if (resolved_size == 0UL)
  {
    if (((uint64_t)map_addr + (uint64_t)sizeof(header_probe)) > (uint64_t)APP_STORAGE_FLASH_SIZE_BYTES)
    {
      g_storage_last_error = APP_STORAGE_ERR_RANGE;
      g_storage_scene_map_load_fail_count++;
      g_storage_scene_map_last_status = (ULONG)TX_SIZE_ERROR;
      return TX_SIZE_ERROR;
    }

    hal_status = AT25_Read(&hospi1, map_addr, g_storage_scene_map_blob_buf, (uint32_t)sizeof(header_probe));
    AppStorageCaptureDebug();
    if (hal_status != HAL_OK)
    {
      g_storage_last_error = APP_STORAGE_ERR_GAME_MAP;
      g_storage_scene_map_load_fail_count++;
      g_storage_scene_map_last_status = (ULONG)TX_NOT_DONE;
      return TX_NOT_DONE;
    }

    (void)memcpy(&header_probe, g_storage_scene_map_blob_buf, sizeof(header_probe));
    if ((header_probe.magic != GAME_MAP_BLOB_MAGIC) ||
        ((header_probe.version != GAME_MAP_BLOB_VERSION_V1) &&
         (header_probe.version != GAME_MAP_BLOB_VERSION_V2)) ||
        (header_probe.header_size < (uint16_t)sizeof(game_map_blob_header_t)) ||
        (header_probe.total_size < (uint32_t)header_probe.header_size) ||
        (header_probe.total_size > (uint32_t)sizeof(g_storage_scene_map_blob_buf)))
    {
      g_storage_last_error = APP_STORAGE_ERR_GAME_MAP;
      g_storage_scene_map_load_fail_count++;
      g_storage_scene_map_last_status = (ULONG)TX_SIZE_ERROR;
      return TX_SIZE_ERROR;
    }

    resolved_size = header_probe.total_size;
  }

  if ((resolved_size == 0UL) || (resolved_size > (uint32_t)sizeof(g_storage_scene_map_blob_buf)))
  {
    g_storage_last_error = APP_STORAGE_ERR_GAME_MAP;
    g_storage_scene_map_load_fail_count++;
    g_storage_scene_map_last_status = (ULONG)TX_SIZE_ERROR;
    return TX_SIZE_ERROR;
  }

  if (((uint64_t)map_addr + (uint64_t)resolved_size) > (uint64_t)APP_STORAGE_FLASH_SIZE_BYTES)
  {
    g_storage_last_error = APP_STORAGE_ERR_RANGE;
    g_storage_scene_map_load_fail_count++;
    g_storage_scene_map_last_status = (ULONG)TX_SIZE_ERROR;
    return TX_SIZE_ERROR;
  }

  g_storage_scene_map_size = (ULONG)resolved_size;

  hal_status = AT25_Read(&hospi1, map_addr, g_storage_scene_map_blob_buf, resolved_size);
  AppStorageCaptureDebug();
  if (hal_status != HAL_OK)
  {
    g_storage_last_error = APP_STORAGE_ERR_GAME_MAP;
    g_storage_scene_map_load_fail_count++;
    g_storage_scene_map_last_status = (ULONG)TX_NOT_DONE;
    return TX_NOT_DONE;
  }

  status = GameRuntime_LoadSceneMapBlob((const void *)g_storage_scene_map_blob_buf, resolved_size);
  g_storage_scene_map_last_status = (ULONG)status;
  if (status != TX_SUCCESS)
  {
    g_storage_last_error = APP_STORAGE_ERR_GAME_MAP;
    g_storage_scene_map_load_fail_count++;
    return status;
  }

  map_view = GameRuntime_GetSceneMap();
  if ((map_view == (const game_map_view_t *)0) ||
      (map_view->header == (const game_map_blob_header_t *)0))
  {
    GameRuntime_ClearSceneMap();
    g_storage_last_error = APP_STORAGE_ERR_GAME_MAP;
    g_storage_scene_map_load_fail_count++;
    g_storage_scene_map_last_status = (ULONG)TX_NOT_DONE;
    return TX_NOT_DONE;
  }

  g_storage_scene_map_loaded = 1UL;
  g_storage_scene_map_width = (ULONG)map_view->header->map_width;
  g_storage_scene_map_height = (ULONG)map_view->header->map_height;
  g_storage_scene_map_tile_count = (ULONG)map_view->tile_count;
  g_storage_scene_map_object_count = (ULONG)map_view->object_count;
  g_storage_scene_map_load_ok_count++;
  return TX_SUCCESS;
}

static UINT AppStorageRunSceneTilesetLoad(uint32_t tileset_addr, uint32_t tileset_size)
{
  HAL_StatusTypeDef hal_status;
  UINT status;
  const game_tileset_view_t *tileset_view;
  uint32_t resolved_size = tileset_size;
  game_tileset_blob_header_t header_probe;

  g_storage_last_op = APP_STORAGE_OP_SCENE_TILESET_LOAD;
  g_storage_last_error = APP_STORAGE_ERR_NONE;
  g_storage_scene_tileset_loaded = 0UL;
  g_storage_scene_tileset_addr = (ULONG)tileset_addr;
  g_storage_scene_tileset_size = 0UL;
  g_storage_scene_tileset_last_status = (ULONG)TX_NOT_DONE;
  g_storage_scene_tileset_tile_width = 0UL;
  g_storage_scene_tileset_tile_height = 0UL;
  g_storage_scene_tileset_tile_count = 0UL;
  g_storage_scene_tileset_base_gid = 0UL;
  (void)memset(g_storage_scene_tileset_blob_buf, 0, sizeof(g_storage_scene_tileset_blob_buf));

  if (tileset_addr >= APP_STORAGE_FLASH_SIZE_BYTES)
  {
    g_storage_last_error = APP_STORAGE_ERR_RANGE;
    g_storage_scene_tileset_load_fail_count++;
    g_storage_scene_tileset_last_status = (ULONG)TX_SIZE_ERROR;
    return TX_SIZE_ERROR;
  }

  if (AppStorageRunFlashProbe() != TX_SUCCESS)
  {
    g_storage_last_op = APP_STORAGE_OP_SCENE_TILESET_LOAD;
    g_storage_last_error = APP_STORAGE_ERR_BOOTINIT;
    g_storage_scene_tileset_load_fail_count++;
    g_storage_scene_tileset_last_status = (ULONG)TX_NOT_DONE;
    return TX_NOT_DONE;
  }
  g_storage_last_op = APP_STORAGE_OP_SCENE_TILESET_LOAD;

  if (resolved_size == 0UL)
  {
    if (((uint64_t)tileset_addr + (uint64_t)sizeof(header_probe)) > (uint64_t)APP_STORAGE_FLASH_SIZE_BYTES)
    {
      g_storage_last_error = APP_STORAGE_ERR_RANGE;
      g_storage_scene_tileset_load_fail_count++;
      g_storage_scene_tileset_last_status = (ULONG)TX_SIZE_ERROR;
      return TX_SIZE_ERROR;
    }

    hal_status = AT25_Read(&hospi1, tileset_addr, g_storage_scene_tileset_blob_buf, (uint32_t)sizeof(header_probe));
    AppStorageCaptureDebug();
    if (hal_status != HAL_OK)
    {
      g_storage_last_error = APP_STORAGE_ERR_GAME_TILESET;
      g_storage_scene_tileset_load_fail_count++;
      g_storage_scene_tileset_last_status = (ULONG)TX_NOT_DONE;
      return TX_NOT_DONE;
    }

    (void)memcpy(&header_probe, g_storage_scene_tileset_blob_buf, sizeof(header_probe));
    if ((header_probe.magic != GAME_TILESET_BLOB_MAGIC) ||
        (header_probe.version != GAME_TILESET_BLOB_VERSION) ||
        (header_probe.header_size < (uint16_t)sizeof(game_tileset_blob_header_t)) ||
        (header_probe.total_size < (uint32_t)header_probe.header_size) ||
        (header_probe.total_size > (uint32_t)sizeof(g_storage_scene_tileset_blob_buf)))
    {
      g_storage_last_error = APP_STORAGE_ERR_GAME_TILESET;
      g_storage_scene_tileset_load_fail_count++;
      g_storage_scene_tileset_last_status = (ULONG)TX_SIZE_ERROR;
      return TX_SIZE_ERROR;
    }

    resolved_size = header_probe.total_size;
  }

  if ((resolved_size == 0UL) || (resolved_size > (uint32_t)sizeof(g_storage_scene_tileset_blob_buf)))
  {
    g_storage_last_error = APP_STORAGE_ERR_GAME_TILESET;
    g_storage_scene_tileset_load_fail_count++;
    g_storage_scene_tileset_last_status = (ULONG)TX_SIZE_ERROR;
    return TX_SIZE_ERROR;
  }

  if (((uint64_t)tileset_addr + (uint64_t)resolved_size) > (uint64_t)APP_STORAGE_FLASH_SIZE_BYTES)
  {
    g_storage_last_error = APP_STORAGE_ERR_RANGE;
    g_storage_scene_tileset_load_fail_count++;
    g_storage_scene_tileset_last_status = (ULONG)TX_SIZE_ERROR;
    return TX_SIZE_ERROR;
  }

  g_storage_scene_tileset_size = (ULONG)resolved_size;

  hal_status = AT25_Read(&hospi1, tileset_addr, g_storage_scene_tileset_blob_buf, resolved_size);
  AppStorageCaptureDebug();
  if (hal_status != HAL_OK)
  {
    g_storage_last_error = APP_STORAGE_ERR_GAME_TILESET;
    g_storage_scene_tileset_load_fail_count++;
    g_storage_scene_tileset_last_status = (ULONG)TX_NOT_DONE;
    return TX_NOT_DONE;
  }

  status = GameRuntime_LoadSceneTilesetBlob((const void *)g_storage_scene_tileset_blob_buf, resolved_size);
  g_storage_scene_tileset_last_status = (ULONG)status;
  if (status != TX_SUCCESS)
  {
    g_storage_last_error = APP_STORAGE_ERR_GAME_TILESET;
    g_storage_scene_tileset_load_fail_count++;
    return status;
  }

  tileset_view = GameRuntime_GetSceneTileset();
  if ((tileset_view == (const game_tileset_view_t *)0) ||
      (tileset_view->header == (const game_tileset_blob_header_t *)0))
  {
    GameRuntime_ClearSceneTileset();
    g_storage_last_error = APP_STORAGE_ERR_GAME_TILESET;
    g_storage_scene_tileset_load_fail_count++;
    g_storage_scene_tileset_last_status = (ULONG)TX_NOT_DONE;
    return TX_NOT_DONE;
  }

  g_storage_scene_tileset_loaded = 1UL;
  g_storage_scene_tileset_tile_width = (ULONG)tileset_view->header->tile_width;
  g_storage_scene_tileset_tile_height = (ULONG)tileset_view->header->tile_height;
  g_storage_scene_tileset_tile_count = (ULONG)tileset_view->tile_count;
  g_storage_scene_tileset_base_gid = (ULONG)tileset_view->header->base_gid;
  g_storage_scene_tileset_load_ok_count++;
  return TX_SUCCESS;
}

static UINT AppStorageRunRawAppErase(void)
{
  UINT status;

  g_storage_last_op = APP_STORAGE_OP_RAW_APP_ERASE;
  g_storage_last_error = APP_STORAGE_ERR_NONE;

  if (AppStorageRunFlashProbe() != TX_SUCCESS)
  {
    g_storage_last_op = APP_STORAGE_OP_RAW_APP_ERASE;
    g_storage_last_error = APP_STORAGE_ERR_BOOTINIT;
    g_storage_raw_app_erase_fail_count++;
    return TX_NOT_DONE;
  }
  g_storage_last_op = APP_STORAGE_OP_RAW_APP_ERASE;

  status = AppStorageEraseRange4K((uint32_t)KNOB_STORAGE_SETTINGS_ADDR, APP_STORAGE_SETTINGS_SECTOR_SIZE);
  if (status != TX_SUCCESS)
  {
    g_storage_last_error = APP_STORAGE_ERR_ERASE;
    g_storage_raw_app_erase_fail_count++;
    return status;
  }

  status = AppStorageEraseRange4K((uint32_t)KNOB_STORAGE_SMOKE_ADDR, APP_STORAGE_SMOKE_SECTOR_SIZE);
  if (status != TX_SUCCESS)
  {
    g_storage_last_error = APP_STORAGE_ERR_ERASE;
    g_storage_raw_app_erase_fail_count++;
    return status;
  }

  status = AppStorageEraseRange4K((uint32_t)KNOB_STORAGE_GAME_PKG_MANIFEST_ADDR,
                                  (uint32_t)KNOB_STORAGE_GAME_PKG_MANIFEST_MAX_BYTES);
  if (status != TX_SUCCESS)
  {
    g_storage_last_error = APP_STORAGE_ERR_ERASE;
    g_storage_raw_app_erase_fail_count++;
    return status;
  }

  status = AppStorageEraseRange4K((uint32_t)KNOB_STORAGE_AUDIO_CATALOG_ADDR,
                                  (uint32_t)KNOB_STORAGE_AUDIO_CATALOG_MAX_BYTES);
  if (status != TX_SUCCESS)
  {
    g_storage_last_error = APP_STORAGE_ERR_ERASE;
    g_storage_raw_app_erase_fail_count++;
    return status;
  }

  status = AppStorageEraseRange4K((uint32_t)KNOB_STORAGE_INSTALLED_BASE_ADDR,
                                  (uint32_t)KNOB_STORAGE_INSTALLED_SIZE_BYTES);
  if (status != TX_SUCCESS)
  {
    g_storage_last_error = APP_STORAGE_ERR_ERASE;
    g_storage_raw_app_erase_fail_count++;
    return status;
  }

  g_storage_joycfg_valid = 0UL;
  g_storage_joycfg_last_error = APP_STORAGE_ERR_NONE;
  (void)memset(&g_storage_joycfg_cal, 0, sizeof(g_storage_joycfg_cal));
  g_storage_audio_catalog_loaded = 0UL;
  g_storage_audio_catalog_addr = 0UL;
  g_storage_audio_catalog_entry_count = 0UL;
  g_storage_audio_catalog_version = 0UL;
  g_storage_audio_catalog_table_crc32 = 0UL;
  (void)memset(&g_storage_audio_catalog_header, 0, sizeof(g_storage_audio_catalog_header));
  (void)memset(g_storage_audio_catalog_entries, 0, sizeof(g_storage_audio_catalog_entries));
  (void)memset(g_storage_audio_chunk_buf, 0, sizeof(g_storage_audio_chunk_buf));
  AppStorageAudioChunkCacheReset();
  GamePackage_ClearLoadedManifest();
  g_storage_game_package_manifest_loaded = 0UL;
  g_storage_game_package_manifest_addr = 0UL;
  g_storage_game_package_manifest_size = 0UL;
  g_storage_game_package_manifest_last_status = (ULONG)TX_NOT_DONE;
  g_storage_game_package_id = 0UL;
  g_storage_game_package_version = 0UL;
  g_storage_game_package_mode_count = 0UL;
  g_storage_game_package_pet_route_count = 0UL;
  g_storage_game_package_source = APP_STORAGE_PKG_SRC_NONE;
  (void)memset(g_storage_game_package_manifest_buf, 0, sizeof(g_storage_game_package_manifest_buf));
  AppStorageInstallIndexClearActive();
  g_storage_install_index_load_last_status = (ULONG)TX_NOT_DONE;
  g_storage_install_index_write_last_status = (ULONG)TX_NOT_DONE;
  g_storage_install_index_scan_done = 0UL;
  GameRuntime_ClearSceneMap();
  g_storage_scene_map_loaded = 0UL;
  g_storage_scene_map_addr = 0UL;
  g_storage_scene_map_size = 0UL;
  g_storage_scene_map_last_status = (ULONG)TX_NOT_DONE;
  g_storage_scene_map_width = 0UL;
  g_storage_scene_map_height = 0UL;
  g_storage_scene_map_tile_count = 0UL;
  g_storage_scene_map_object_count = 0UL;
  (void)memset(g_storage_scene_map_blob_buf, 0, sizeof(g_storage_scene_map_blob_buf));
  GameRuntime_ClearSceneTileset();
  g_storage_scene_tileset_loaded = 0UL;
  g_storage_scene_tileset_addr = 0UL;
  g_storage_scene_tileset_size = 0UL;
  g_storage_scene_tileset_last_status = (ULONG)TX_NOT_DONE;
  g_storage_scene_tileset_tile_width = 0UL;
  g_storage_scene_tileset_tile_height = 0UL;
  g_storage_scene_tileset_tile_count = 0UL;
  g_storage_scene_tileset_base_gid = 0UL;
  (void)memset(g_storage_scene_tileset_blob_buf, 0, sizeof(g_storage_scene_tileset_blob_buf));

  g_storage_raw_app_erase_ok_count++;
  return TX_SUCCESS;
}

static ULONG AppStorageFatTotalSectors(void)
{
  return (ULONG)(KNOB_STORAGE_FAT_SIZE_BYTES / APP_STORAGE_FAT_BYTES_PER_SECTOR);
}

static UINT AppStorageUsbMscLevelXOpen(void)
{
  UINT lx_status;
  ULONG lx_state = g_storage_usb_msc_lx_flash.lx_nor_flash_state;

  if ((g_storage_usb_msc_lx_opened != 0UL) || (lx_state == LX_NOR_FLASH_OPENED))
  {
    g_storage_usb_msc_lx_opened = 1UL;
    g_storage_filex_last_status = (ULONG)LX_SUCCESS;
    return TX_SUCCESS;
  }

  if (AppStorageRunFlashProbe() != TX_SUCCESS)
  {
    g_storage_filex_last_status = FX_IO_ERROR;
    return TX_NOT_DONE;
  }

  lx_status = lx_nor_flash_open(&g_storage_usb_msc_lx_flash,
                                (CHAR *)NOR_CUSTOM_DRIVER_NAME,
                                NOR_CUSTOM_DRIVER_INIT);
  g_storage_filex_last_status = (ULONG)lx_status;
  if (lx_status != LX_SUCCESS)
  {
    g_storage_usb_msc_lx_opened = 0UL;
    return TX_NOT_DONE;
  }

  g_storage_usb_msc_lx_opened = 1UL;
  return TX_SUCCESS;
}

static UINT AppStorageUsbMscLevelXClose(void)
{
  UINT lx_status;

  if ((g_storage_usb_msc_lx_opened == 0UL) &&
      (g_storage_usb_msc_lx_flash.lx_nor_flash_state != LX_NOR_FLASH_OPENED))
  {
    return TX_SUCCESS;
  }

  lx_status = lx_nor_flash_close(&g_storage_usb_msc_lx_flash);
  g_storage_filex_last_status = (ULONG)lx_status;
  if (lx_status != LX_SUCCESS)
  {
    return TX_NOT_DONE;
  }

  (void)memset(&g_storage_usb_msc_lx_flash, 0, sizeof(g_storage_usb_msc_lx_flash));
  g_storage_usb_msc_lx_opened = 0UL;
  return TX_SUCCESS;
}

static UINT AppStorageRunFileXMount(void)
{
  UINT fx_status;

  g_storage_last_op = APP_STORAGE_OP_FILEX_MOUNT;
  g_storage_last_error = APP_STORAGE_ERR_NONE;

  if (g_storage_filex_mounted != 0UL)
  {
    return TX_SUCCESS;
  }

  if (AppStorageUsbMscLevelXClose() != TX_SUCCESS)
  {
    g_storage_last_op = APP_STORAGE_OP_FILEX_MOUNT;
    g_storage_last_error = APP_STORAGE_ERR_FILEX_MOUNT;
    g_storage_filex_mount_fail_count++;
    return TX_NOT_DONE;
  }

  if (AppStorageRunFlashProbe() != TX_SUCCESS)
  {
    g_storage_last_op = APP_STORAGE_OP_FILEX_MOUNT;
    g_storage_filex_last_status = FX_IO_ERROR;
    g_storage_last_error = APP_STORAGE_ERR_FILEX_MOUNT;
    g_storage_filex_mount_fail_count++;
    return TX_NOT_DONE;
  }
  g_storage_last_op = APP_STORAGE_OP_FILEX_MOUNT;

  fx_status = fx_media_open(&g_storage_fx_media,
                            (CHAR *)APP_STORAGE_FILEX_MEDIA_NAME,
                            fx_stm32_levelx_nor_driver,
                            (VOID *)NOR_CUSTOM_DRIVER_ID,
                            g_storage_filex_cache,
                            sizeof(g_storage_filex_cache));
  g_storage_filex_last_status = (ULONG)fx_status;
  if (fx_status != FX_SUCCESS)
  {
    g_storage_filex_mounted = 0UL;
    g_storage_last_error = APP_STORAGE_ERR_FILEX_MOUNT;
    g_storage_filex_mount_fail_count++;
    return TX_NOT_DONE;
  }

  g_storage_filex_mounted = 1UL;
  g_storage_filex_mount_count++;
  return TX_SUCCESS;
}

static UINT AppStorageRunFileXUnmount(void)
{
  UINT fx_status;
  UINT flush_status;

  g_storage_last_op = APP_STORAGE_OP_FILEX_UNMOUNT;
  g_storage_last_error = APP_STORAGE_ERR_NONE;

  if (g_storage_filex_mounted == 0UL)
  {
    if (AppStorageUsbMscLevelXClose() != TX_SUCCESS)
    {
      g_storage_last_error = APP_STORAGE_ERR_FILEX_UNMOUNT;
      g_storage_filex_unmount_fail_count++;
      return TX_NOT_DONE;
    }
    g_storage_filex_last_status = FX_SUCCESS;
    return TX_SUCCESS;
  }

  if (AppStorageRunFlashResume() != TX_SUCCESS)
  {
    g_storage_filex_last_status = FX_IO_ERROR;
    g_storage_last_error = APP_STORAGE_ERR_FILEX_UNMOUNT;
    g_storage_filex_unmount_fail_count++;
    return TX_NOT_DONE;
  }

  flush_status = fx_media_flush(&g_storage_fx_media);
  if (flush_status != FX_SUCCESS)
  {
    g_storage_filex_last_status = (ULONG)flush_status;
    g_storage_last_error = APP_STORAGE_ERR_FILEX_UNMOUNT;
    g_storage_filex_unmount_fail_count++;
    return TX_NOT_DONE;
  }

  fx_status = fx_media_close(&g_storage_fx_media);
  g_storage_filex_last_status = (ULONG)fx_status;
  if (fx_status != FX_SUCCESS)
  {
    g_storage_last_error = APP_STORAGE_ERR_FILEX_UNMOUNT;
    g_storage_filex_unmount_fail_count++;
    return TX_NOT_DONE;
  }

  g_storage_filex_mounted = 0UL;
  g_storage_filex_unmount_count++;
  if (AppStorageUsbMscLevelXClose() != TX_SUCCESS)
  {
    g_storage_last_error = APP_STORAGE_ERR_FILEX_UNMOUNT;
    g_storage_filex_unmount_fail_count++;
    return TX_NOT_DONE;
  }
  return TX_SUCCESS;
}

static UINT AppStorageRunFileXFormat(void)
{
  UINT fx_status;
  UINT erase_status;
  ULONG total_sectors = AppStorageFatTotalSectors();

  g_storage_last_op = APP_STORAGE_OP_FILEX_FORMAT;
  g_storage_last_error = APP_STORAGE_ERR_NONE;

  (void)AppStorageRunFileXUnmount();
  if (g_storage_last_error == APP_STORAGE_ERR_FILEX_UNMOUNT)
  {
    g_storage_last_op = APP_STORAGE_OP_FILEX_FORMAT;
    g_storage_last_error = APP_STORAGE_ERR_FILEX_FORMAT;
    return TX_NOT_DONE;
  }

  if (AppStorageRunFlashProbe() != TX_SUCCESS)
  {
    g_storage_last_op = APP_STORAGE_OP_FILEX_FORMAT;
    g_storage_filex_last_status = FX_IO_ERROR;
    g_storage_last_error = APP_STORAGE_ERR_FILEX_FORMAT;
    return TX_NOT_DONE;
  }
  g_storage_last_op = APP_STORAGE_OP_FILEX_FORMAT;

  /* FileX/LevelX format assumes clean NOR bookkeeping state. */
  erase_status = AppStorageEraseRange4K((uint32_t)KNOB_STORAGE_FAT_BASE_ADDR,
                                        (uint32_t)KNOB_STORAGE_FAT_SIZE_BYTES);
  if (erase_status != TX_SUCCESS)
  {
    g_storage_last_op = APP_STORAGE_OP_FILEX_FORMAT;
    g_storage_filex_last_status = FX_IO_ERROR;
    g_storage_last_error = APP_STORAGE_ERR_FILEX_FORMAT;
    return erase_status;
  }
  g_storage_last_op = APP_STORAGE_OP_FILEX_FORMAT;

  fx_status = fx_media_format(&g_storage_fx_media,
                              fx_stm32_levelx_nor_driver,
                              (VOID *)NOR_CUSTOM_DRIVER_ID,
                              g_storage_filex_cache,
                              sizeof(g_storage_filex_cache),
                              (CHAR *)APP_STORAGE_FILEX_VOLUME_NAME,
                              APP_STORAGE_FILEX_NUM_FATS,
                              KNOB_STORAGE_FILEX_DIR_ENTRIES,
                              0U,
                              total_sectors,
                              APP_STORAGE_FAT_BYTES_PER_SECTOR,
                              KNOB_STORAGE_FILEX_SECTORS_PER_CLUSTER,
                              1U,
                              1U);
  g_storage_filex_last_status = (ULONG)fx_status;
  if (fx_status != FX_SUCCESS)
  {
    g_storage_last_error = APP_STORAGE_ERR_FILEX_FORMAT;
    return TX_NOT_DONE;
  }

  g_storage_filex_format_count++;
  if (AppStorageRunFileXMount() != TX_SUCCESS)
  {
    g_storage_last_op = APP_STORAGE_OP_FILEX_FORMAT;
    g_storage_last_error = APP_STORAGE_ERR_FILEX_FORMAT;
    return TX_NOT_DONE;
  }

  g_storage_last_op = APP_STORAGE_OP_FILEX_FORMAT;
  g_storage_last_error = APP_STORAGE_ERR_NONE;
  return TX_SUCCESS;
}

static UINT AppStorageRunUsbMscRead(uint32_t lba, uint32_t number_blocks, uint8_t *data_pointer, ULONG *media_status_out)
{
  UINT lx_status;
  ULONG block_ix;
  ULONG block_offset_bytes;
  ULONG total_sectors = AppStorageFatTotalSectors();
  ULONG media_status = 1UL;

  g_storage_last_op = APP_STORAGE_OP_USB_MSC_READ;
  g_storage_last_error = APP_STORAGE_ERR_NONE;

  if ((data_pointer == TX_NULL) || (number_blocks == 0UL))
  {
    g_storage_last_error = APP_STORAGE_ERR_USB_MSC;
    if (media_status_out != TX_NULL)
    {
      *media_status_out = media_status;
    }
    return TX_PTR_ERROR;
  }

  if (((ULONG)lba >= total_sectors) || ((ULONG)number_blocks > (total_sectors - (ULONG)lba)))
  {
    g_storage_last_error = APP_STORAGE_ERR_USB_MSC;
    if (media_status_out != TX_NULL)
    {
      *media_status_out = media_status;
    }
    return TX_SIZE_ERROR;
  }

  if (AppStorageUsbMscLevelXOpen() != TX_SUCCESS)
  {
    g_storage_last_op = APP_STORAGE_OP_USB_MSC_READ;
    g_storage_last_error = APP_STORAGE_ERR_USB_MSC;
    if (media_status_out != TX_NULL)
    {
      *media_status_out = media_status;
    }
    return TX_NOT_DONE;
  }

  for (block_ix = 0UL; block_ix < (ULONG)number_blocks; block_ix++)
  {
    block_offset_bytes = block_ix * (ULONG)APP_STORAGE_FAT_BYTES_PER_SECTOR;
    lx_status = lx_nor_flash_sector_read(&g_storage_usb_msc_lx_flash,
                                         (ULONG)lba + block_ix,
                                         (VOID *)(data_pointer + block_offset_bytes));
    g_storage_filex_last_status = (ULONG)lx_status;
    if (lx_status != LX_SUCCESS)
    {
      g_storage_last_error = APP_STORAGE_ERR_USB_MSC;
      if (media_status_out != TX_NULL)
      {
        *media_status_out = media_status;
      }
      return TX_NOT_DONE;
    }
  }

  media_status = 0UL;
  if (media_status_out != TX_NULL)
  {
    *media_status_out = media_status;
  }
  return TX_SUCCESS;
}

static UINT AppStorageRunUsbMscWrite(uint32_t lba, uint32_t number_blocks, const uint8_t *data_pointer, ULONG *media_status_out)
{
  UINT lx_status;
  ULONG block_ix;
  ULONG block_offset_bytes;
  ULONG total_sectors = AppStorageFatTotalSectors();
  ULONG media_status = 1UL;

  g_storage_last_op = APP_STORAGE_OP_USB_MSC_WRITE;
  g_storage_last_error = APP_STORAGE_ERR_NONE;

  if ((data_pointer == TX_NULL) || (number_blocks == 0UL))
  {
    g_storage_last_error = APP_STORAGE_ERR_USB_MSC;
    if (media_status_out != TX_NULL)
    {
      *media_status_out = media_status;
    }
    return TX_PTR_ERROR;
  }

  if (((ULONG)lba >= total_sectors) || ((ULONG)number_blocks > (total_sectors - (ULONG)lba)))
  {
    g_storage_last_error = APP_STORAGE_ERR_USB_MSC;
    if (media_status_out != TX_NULL)
    {
      *media_status_out = media_status;
    }
    return TX_SIZE_ERROR;
  }

  if (AppStorageUsbMscLevelXOpen() != TX_SUCCESS)
  {
    g_storage_last_op = APP_STORAGE_OP_USB_MSC_WRITE;
    g_storage_last_error = APP_STORAGE_ERR_USB_MSC;
    if (media_status_out != TX_NULL)
    {
      *media_status_out = media_status;
    }
    return TX_NOT_DONE;
  }

  for (block_ix = 0UL; block_ix < (ULONG)number_blocks; block_ix++)
  {
    block_offset_bytes = block_ix * (ULONG)APP_STORAGE_FAT_BYTES_PER_SECTOR;
    lx_status = lx_nor_flash_sector_write(&g_storage_usb_msc_lx_flash,
                                          (ULONG)lba + block_ix,
                                          (VOID *)(data_pointer + block_offset_bytes));
    g_storage_filex_last_status = (ULONG)lx_status;
    if (lx_status != LX_SUCCESS)
    {
      g_storage_last_error = APP_STORAGE_ERR_USB_MSC;
      if (media_status_out != TX_NULL)
      {
        *media_status_out = media_status;
      }
      return TX_NOT_DONE;
    }
  }

  media_status = 0UL;
  if (media_status_out != TX_NULL)
  {
    *media_status_out = media_status;
  }
  return TX_SUCCESS;
}

static UINT AppStorageRunUsbMscFlush(ULONG *media_status_out)
{
  ULONG media_status = 1UL;

  g_storage_last_op = APP_STORAGE_OP_USB_MSC_FLUSH;
  g_storage_last_error = APP_STORAGE_ERR_NONE;

  if (AppStorageUsbMscLevelXOpen() != TX_SUCCESS)
  {
    g_storage_last_op = APP_STORAGE_OP_USB_MSC_FLUSH;
    g_storage_last_error = APP_STORAGE_ERR_USB_MSC;
    if (media_status_out != TX_NULL)
    {
      *media_status_out = media_status;
    }
    return TX_NOT_DONE;
  }

  g_storage_filex_last_status = (ULONG)LX_SUCCESS;
  media_status = 0UL;
  if (media_status_out != TX_NULL)
  {
    *media_status_out = media_status;
  }
  return TX_SUCCESS;
}

static UINT AppStorageRunUsbMscStatus(ULONG *media_status_out)
{
  ULONG media_status = 1UL;

  g_storage_last_op = APP_STORAGE_OP_USB_MSC_STATUS;
  g_storage_last_error = APP_STORAGE_ERR_NONE;

  if (AppStorageUsbMscLevelXOpen() != TX_SUCCESS)
  {
    g_storage_last_op = APP_STORAGE_OP_USB_MSC_STATUS;
    g_storage_last_error = APP_STORAGE_ERR_USB_MSC;
    if (media_status_out != TX_NULL)
    {
      *media_status_out = media_status;
    }
    return TX_NOT_DONE;
  }

  g_storage_filex_last_status = (ULONG)LX_SUCCESS;
  media_status = 0UL;
  if (media_status_out != TX_NULL)
  {
    *media_status_out = media_status;
  }
  return TX_SUCCESS;
}

static uint32_t AppStorageCrc32(const uint8_t *data, uint32_t len)
{
  uint32_t crc = 0xFFFFFFFFUL;
  uint32_t idx;
  uint32_t bit;

  if (data == TX_NULL)
  {
    return 0UL;
  }

  for (idx = 0UL; idx < len; idx++)
  {
    crc ^= (uint32_t)data[idx];
    for (bit = 0UL; bit < 8UL; bit++)
    {
      if ((crc & 1UL) != 0UL)
      {
        crc = (crc >> 1) ^ 0xEDB88320UL;
      }
      else
      {
        crc >>= 1;
      }
    }
  }

  return ~crc;
}

static uint8_t AppSensorJoyCalSane(const TMAGJoy_Cal *cal)
{
  float sx;
  float sy;
  float max_span;
  float min_span;

  if (cal == TX_NULL)
  {
    return 0U;
  }

  if ((!isfinite(cal->cx)) ||
      (!isfinite(cal->cy)) ||
      (!isfinite(cal->sx)) ||
      (!isfinite(cal->sy)) ||
      (!isfinite(cal->rot_deg)))
  {
    return 0U;
  }

  if ((cal->invert_x > 1U) || (cal->invert_y > 1U))
  {
    return 0U;
  }

  sx = fabsf(cal->sx);
  sy = fabsf(cal->sy);
  if ((sx < 1.0f) || (sy < 1.0f))
  {
    return 0U;
  }

  if (sx >= sy)
  {
    max_span = sx;
    min_span = sy;
  }
  else
  {
    max_span = sy;
    min_span = sx;
  }

  if ((min_span < 1.0f) || ((max_span / min_span) > 32.0f))
  {
    return 0U;
  }

  if (fabsf(cal->rot_deg) > 360.0f)
  {
    return 0U;
  }

  return 1U;
}

static ULONG AppStorageUserGainClampPct(ULONG pct)
{
  if (pct > APP_STORAGE_USER_GAIN_MAX_PCT)
  {
    return APP_STORAGE_USER_GAIN_MAX_PCT;
  }
  return pct;
}

static float AppStorageDeadzoneClampMt(float deadzone_mT)
{
  float min_mT = ((float)KNOB_SENSOR_JOY_NEUTRAL_DEADZONE_MIN_MT_X10) / 10.0f;
  float max_mT = ((float)KNOB_SENSOR_JOY_NEUTRAL_DEADZONE_MAX_MT_X10) / 10.0f;

  if (!isfinite(deadzone_mT))
  {
    deadzone_mT = min_mT;
  }
  if (min_mT < 0.1f)
  {
    min_mT = 0.1f;
  }
  if (max_mT < min_mT)
  {
    max_mT = min_mT;
  }
  if (deadzone_mT < min_mT)
  {
    deadzone_mT = min_mT;
  }
  if (deadzone_mT > max_mT)
  {
    deadzone_mT = max_mT;
  }
  return deadzone_mT;
}

static VOID AppStorageJoyCfgApplyRuntimeDefaults(void)
{
  g_storage_joycfg_deadzone_enabled = 1UL;
  g_storage_joycfg_deadzone_mT = AppStorageDeadzoneClampMt(((float)KNOB_SENSOR_JOY_NEUTRAL_DEADZONE_MIN_MT_X10) / 10.0f);
  g_audio_user_gain_master_pct = APP_STORAGE_USER_GAIN_DEFAULT_PCT;
  g_audio_user_gain_music_pct = APP_STORAGE_USER_GAIN_DEFAULT_PCT;
  g_audio_user_gain_sfx_pct = APP_STORAGE_USER_GAIN_DEFAULT_PCT;
  g_audio_user_gain_ui_pct = APP_STORAGE_USER_GAIN_DEFAULT_PCT;
}

static UINT AppStorageRunJoyCfgLoad(void)
{
  app_storage_joycfg_blob_v2_t blob_v2 = {0};
  app_storage_joycfg_blob_v3_t blob_v3 = {0};
  HAL_StatusTypeDef hal_status;
  uint32_t crc = 0UL;
  uint32_t payload_v3 = (uint32_t)(sizeof(blob_v3) - 16UL);

  g_storage_last_op = APP_STORAGE_OP_JOYCFG_LOAD;
  g_storage_last_error = APP_STORAGE_ERR_NONE;
  g_storage_joycfg_last_error = APP_STORAGE_ERR_NONE;
  AppStorageJoyCfgApplyRuntimeDefaults();

  if (((uint32_t)KNOB_STORAGE_SETTINGS_ADDR % APP_STORAGE_SETTINGS_SECTOR_SIZE) != 0UL)
  {
    g_storage_last_error = APP_STORAGE_ERR_ALIGN;
    g_storage_joycfg_last_error = APP_STORAGE_ERR_ALIGN;
    g_storage_joycfg_valid = 0UL;
    g_storage_joycfg_load_fail_count++;
    g_storage_joycfg_load_seq++;
    return TX_NOT_DONE;
  }

  if (((uint32_t)KNOB_STORAGE_SETTINGS_ADDR + (uint32_t)sizeof(blob_v3)) > APP_STORAGE_FLASH_SIZE_BYTES)
  {
    g_storage_last_error = APP_STORAGE_ERR_JOYCFG_RANGE;
    g_storage_joycfg_last_error = APP_STORAGE_ERR_JOYCFG_RANGE;
    g_storage_joycfg_valid = 0UL;
    g_storage_joycfg_load_fail_count++;
    g_storage_joycfg_load_seq++;
    return TX_NOT_DONE;
  }

  if (AppStorageRunFlashProbe() != TX_SUCCESS)
  {
    g_storage_last_op = APP_STORAGE_OP_JOYCFG_LOAD;
    g_storage_joycfg_last_error = g_storage_last_error;
    g_storage_joycfg_valid = 0UL;
    g_storage_joycfg_load_fail_count++;
    g_storage_joycfg_load_seq++;
    return TX_NOT_DONE;
  }
  g_storage_last_op = APP_STORAGE_OP_JOYCFG_LOAD;

  hal_status = AT25_Read(&hospi1, (uint32_t)KNOB_STORAGE_SETTINGS_ADDR, (uint8_t *)&blob_v3, (uint32_t)sizeof(blob_v3));
  AppStorageCaptureDebug();
  if (hal_status != HAL_OK)
  {
    g_storage_last_error = APP_STORAGE_ERR_JOYCFG_READ;
    g_storage_joycfg_last_error = APP_STORAGE_ERR_JOYCFG_READ;
    g_storage_joycfg_valid = 0UL;
    g_storage_joycfg_load_fail_count++;
    g_storage_joycfg_load_seq++;
    return TX_NOT_DONE;
  }

  if ((blob_v3.magic == 0xFFFFFFFFUL) &&
      (blob_v3.version == 0xFFFFFFFFUL) &&
      (blob_v3.payload_size == 0xFFFFFFFFUL) &&
      (blob_v3.crc32 == 0xFFFFFFFFUL))
  {
    g_storage_joycfg_valid = 0UL;
    g_storage_last_error = APP_STORAGE_ERR_NONE;
    g_storage_joycfg_last_error = APP_STORAGE_ERR_NONE;
    g_storage_joycfg_load_ok_count++;
    g_storage_joycfg_load_seq++;
    return TX_SUCCESS;
  }

  if (blob_v3.magic != APP_STORAGE_JOYCFG_MAGIC)
  {
    g_storage_last_error = APP_STORAGE_ERR_JOYCFG_INVALID;
    g_storage_joycfg_last_error = APP_STORAGE_ERR_JOYCFG_INVALID;
    g_storage_joycfg_valid = 0UL;
    g_storage_joycfg_load_fail_count++;
    g_storage_joycfg_load_seq++;
    return TX_NOT_DONE;
  }

  if (blob_v3.version == APP_STORAGE_JOYCFG_VERSION_V2)
  {
    (void)memset(&blob_v2, 0, sizeof(blob_v2));
    blob_v2.magic = blob_v3.magic;
    blob_v2.version = blob_v3.version;
    blob_v2.payload_size = blob_v3.payload_size;
    blob_v2.crc32 = blob_v3.crc32;
    blob_v2.cal = blob_v3.cal;

    if (blob_v2.payload_size != (uint32_t)sizeof(TMAGJoy_Cal))
    {
      g_storage_last_error = APP_STORAGE_ERR_JOYCFG_INVALID;
      g_storage_joycfg_last_error = APP_STORAGE_ERR_JOYCFG_INVALID;
      g_storage_joycfg_valid = 0UL;
      g_storage_joycfg_load_fail_count++;
      g_storage_joycfg_load_seq++;
      return TX_NOT_DONE;
    }

    crc = AppStorageCrc32((const uint8_t *)&blob_v2.cal, blob_v2.payload_size);
    if ((crc != blob_v2.crc32) || (AppSensorJoyCalSane(&blob_v2.cal) == 0U))
    {
      g_storage_last_error = APP_STORAGE_ERR_JOYCFG_INVALID;
      g_storage_joycfg_last_error = APP_STORAGE_ERR_JOYCFG_INVALID;
      g_storage_joycfg_valid = 0UL;
      g_storage_joycfg_load_fail_count++;
      g_storage_joycfg_load_seq++;
      return TX_NOT_DONE;
    }

    g_storage_joycfg_cal = blob_v2.cal;
  }
  else if (blob_v3.version == APP_STORAGE_JOYCFG_VERSION_V3)
  {
    if (blob_v3.payload_size != payload_v3)
    {
      g_storage_last_error = APP_STORAGE_ERR_JOYCFG_INVALID;
      g_storage_joycfg_last_error = APP_STORAGE_ERR_JOYCFG_INVALID;
      g_storage_joycfg_valid = 0UL;
      g_storage_joycfg_load_fail_count++;
      g_storage_joycfg_load_seq++;
      return TX_NOT_DONE;
    }

    crc = AppStorageCrc32((const uint8_t *)&blob_v3.cal, blob_v3.payload_size);
    if ((crc != blob_v3.crc32) || (AppSensorJoyCalSane(&blob_v3.cal) == 0U))
    {
      g_storage_last_error = APP_STORAGE_ERR_JOYCFG_INVALID;
      g_storage_joycfg_last_error = APP_STORAGE_ERR_JOYCFG_INVALID;
      g_storage_joycfg_valid = 0UL;
      g_storage_joycfg_load_fail_count++;
      g_storage_joycfg_load_seq++;
      return TX_NOT_DONE;
    }

    g_storage_joycfg_cal = blob_v3.cal;
    g_storage_joycfg_deadzone_enabled = (blob_v3.deadzone_enabled != 0UL) ? 1UL : 0UL;
    g_storage_joycfg_deadzone_mT = AppStorageDeadzoneClampMt(blob_v3.deadzone_mT);
    g_audio_user_gain_master_pct = AppStorageUserGainClampPct(blob_v3.user_gain_master_pct);
    g_audio_user_gain_music_pct = AppStorageUserGainClampPct(blob_v3.user_gain_music_pct);
    g_audio_user_gain_sfx_pct = AppStorageUserGainClampPct(blob_v3.user_gain_sfx_pct);
    g_audio_user_gain_ui_pct = AppStorageUserGainClampPct(blob_v3.user_gain_ui_pct);
  }
  else
  {
    g_storage_last_error = APP_STORAGE_ERR_JOYCFG_INVALID;
    g_storage_joycfg_last_error = APP_STORAGE_ERR_JOYCFG_INVALID;
    g_storage_joycfg_valid = 0UL;
    g_storage_joycfg_load_fail_count++;
    g_storage_joycfg_load_seq++;
    return TX_NOT_DONE;
  }

  g_storage_joycfg_valid = 1UL;
  g_storage_last_error = APP_STORAGE_ERR_NONE;
  g_storage_joycfg_last_error = APP_STORAGE_ERR_NONE;
  g_storage_joycfg_load_ok_count++;
  g_storage_joycfg_load_seq++;
  return TX_SUCCESS;
}

static UINT AppStorageRunJoyCfgSave(void)
{
  app_storage_joycfg_blob_v3_t blob = {0};
  app_storage_joycfg_blob_v3_t verify_blob = {0};
  HAL_StatusTypeDef hal_status;
  uint32_t payload_v3 = (uint32_t)(sizeof(blob) - 16UL);

  g_storage_last_op = APP_STORAGE_OP_JOYCFG_SAVE;
  g_storage_last_error = APP_STORAGE_ERR_NONE;
  g_storage_joycfg_last_error = APP_STORAGE_ERR_NONE;

  if (((uint32_t)KNOB_STORAGE_SETTINGS_ADDR % APP_STORAGE_SETTINGS_SECTOR_SIZE) != 0UL)
  {
    g_storage_last_error = APP_STORAGE_ERR_ALIGN;
    g_storage_joycfg_last_error = APP_STORAGE_ERR_ALIGN;
    g_storage_joycfg_save_fail_count++;
    g_storage_joycfg_save_seq++;
    return TX_NOT_DONE;
  }

  if (((uint32_t)KNOB_STORAGE_SETTINGS_ADDR + (uint32_t)sizeof(blob)) > APP_STORAGE_FLASH_SIZE_BYTES)
  {
    g_storage_last_error = APP_STORAGE_ERR_JOYCFG_RANGE;
    g_storage_joycfg_last_error = APP_STORAGE_ERR_JOYCFG_RANGE;
    g_storage_joycfg_save_fail_count++;
    g_storage_joycfg_save_seq++;
    return TX_NOT_DONE;
  }

  if (g_storage_joycfg_valid == 0UL)
  {
    g_storage_last_error = APP_STORAGE_ERR_JOYCFG_NODATA;
    g_storage_joycfg_last_error = APP_STORAGE_ERR_JOYCFG_NODATA;
    g_storage_joycfg_save_fail_count++;
    g_storage_joycfg_save_seq++;
    return TX_NOT_DONE;
  }

  if (AppStorageRunFlashProbe() != TX_SUCCESS)
  {
    g_storage_last_op = APP_STORAGE_OP_JOYCFG_SAVE;
    g_storage_joycfg_last_error = g_storage_last_error;
    g_storage_joycfg_save_fail_count++;
    g_storage_joycfg_save_seq++;
    return TX_NOT_DONE;
  }
  g_storage_last_op = APP_STORAGE_OP_JOYCFG_SAVE;

  blob.magic = APP_STORAGE_JOYCFG_MAGIC;
  blob.version = APP_STORAGE_JOYCFG_VERSION_V3;
  blob.payload_size = payload_v3;
  blob.cal = g_storage_joycfg_cal;
  blob.deadzone_enabled = (g_storage_joycfg_deadzone_enabled != 0UL) ? 1UL : 0UL;
  blob.deadzone_mT = AppStorageDeadzoneClampMt(g_storage_joycfg_deadzone_mT);
  blob.user_gain_master_pct = AppStorageUserGainClampPct(g_audio_user_gain_master_pct);
  blob.user_gain_music_pct = AppStorageUserGainClampPct(g_audio_user_gain_music_pct);
  blob.user_gain_sfx_pct = AppStorageUserGainClampPct(g_audio_user_gain_sfx_pct);
  blob.user_gain_ui_pct = AppStorageUserGainClampPct(g_audio_user_gain_ui_pct);
  blob.crc32 = AppStorageCrc32((const uint8_t *)&blob.cal, blob.payload_size);

  hal_status = AT25_Erase4K(&hospi1, (uint32_t)KNOB_STORAGE_SETTINGS_ADDR);
  AppStorageCaptureDebug();
  if (hal_status != HAL_OK)
  {
    g_storage_last_error = APP_STORAGE_ERR_JOYCFG_ERASE;
    g_storage_joycfg_last_error = APP_STORAGE_ERR_JOYCFG_ERASE;
    g_storage_joycfg_save_fail_count++;
    g_storage_joycfg_save_seq++;
    return TX_NOT_DONE;
  }

  hal_status = AT25_PageProgram(&hospi1, (uint32_t)KNOB_STORAGE_SETTINGS_ADDR, (const uint8_t *)&blob, (uint32_t)sizeof(blob));
  AppStorageCaptureDebug();
  if (hal_status != HAL_OK)
  {
    g_storage_last_error = APP_STORAGE_ERR_JOYCFG_PROGRAM;
    g_storage_joycfg_last_error = APP_STORAGE_ERR_JOYCFG_PROGRAM;
    g_storage_joycfg_save_fail_count++;
    g_storage_joycfg_save_seq++;
    return TX_NOT_DONE;
  }

  hal_status = AT25_Read(&hospi1, (uint32_t)KNOB_STORAGE_SETTINGS_ADDR, (uint8_t *)&verify_blob, (uint32_t)sizeof(verify_blob));
  AppStorageCaptureDebug();
  if ((hal_status != HAL_OK) || (memcmp(&blob, &verify_blob, sizeof(blob)) != 0))
  {
    g_storage_last_error = APP_STORAGE_ERR_JOYCFG_VERIFY;
    g_storage_joycfg_last_error = APP_STORAGE_ERR_JOYCFG_VERIFY;
    g_storage_joycfg_save_fail_count++;
    g_storage_joycfg_save_seq++;
    return TX_NOT_DONE;
  }

  g_storage_last_error = APP_STORAGE_ERR_NONE;
  g_storage_joycfg_last_error = APP_STORAGE_ERR_NONE;
  g_storage_joycfg_save_ok_count++;
  g_storage_joycfg_save_seq++;
  return TX_SUCCESS;
}

#include "threads/app_thread_entry_input.c"

#include "threads/app_thread_entry_ui.c"

#include "threads/app_thread_entry_game.c"

static uint32_t AppRetainedStateCrc32(const uint8_t *data, uint32_t len)
{
  uint32_t crc = 0xFFFFFFFFUL;
  uint32_t i;

  if ((data == (const uint8_t *)0) || (len == 0UL))
  {
    return 0UL;
  }

  while (len-- > 0UL)
  {
    crc ^= (uint32_t)(*data++);
    for (i = 0UL; i < 8UL; i++)
    {
      if ((crc & 1UL) != 0UL)
      {
        crc = (crc >> 1) ^ 0xEDB88320UL;
      }
      else
      {
        crc >>= 1;
      }
    }
  }

  return ~crc;
}

static uint8_t AppRetainedStateBlobHeaderValid(const app_retained_state_blob_t *blob)
{
  if (blob == (const app_retained_state_blob_t *)0)
  {
    return 0U;
  }
  if (blob->magic != APP_RETAINED_STATE_MAGIC)
  {
    return 0U;
  }
  if (blob->version != APP_RETAINED_STATE_VERSION)
  {
    return 0U;
  }
  if (blob->size != (uint16_t)sizeof(app_retained_state_blob_t))
  {
    return 0U;
  }
  return 1U;
}

static uint8_t AppRetainedStateBlobCrcValid(const app_retained_state_blob_t *blob)
{
  app_retained_state_blob_t probe;
  uint32_t crc_saved;
  uint32_t crc_calc;

  if (AppRetainedStateBlobHeaderValid(blob) == 0U)
  {
    return 0U;
  }

  probe = *blob;
  crc_saved = probe.crc32;
  probe.crc32 = 0UL;
  crc_calc = AppRetainedStateCrc32((const uint8_t *)&probe, (uint32_t)sizeof(probe));
  return (crc_calc == crc_saved) ? 1U : 0U;
}

static VOID AppRetainedStateSyncDebugFromBlob(const app_retained_state_blob_t *blob, uint8_t crc_ok)
{
  if (blob == (const app_retained_state_blob_t *)0)
  {
    g_retained_state_magic = 0UL;
    g_retained_state_valid_mask = 0UL;
    g_retained_state_seq = 0UL;
    g_retained_state_crc_ok = 0UL;
    g_retained_state_game_mode_id = 0UL;
    g_retained_state_game_backend_id = 0UL;
    g_retained_state_game_topdown_valid = 0UL;
    return;
  }
  g_retained_state_magic = (ULONG)blob->magic;
  g_retained_state_valid_mask = (ULONG)blob->valid_mask;
  g_retained_state_seq = (ULONG)blob->seq;
  g_retained_state_crc_ok = (crc_ok != 0U) ? 1UL : 0UL;
  g_retained_state_game_mode_id = (ULONG)blob->game.mode_id;
  g_retained_state_game_backend_id = (ULONG)blob->game.backend_id;
  g_retained_state_game_topdown_valid = (ULONG)blob->game.topdown.valid;
}

static VOID AppRetainedStateBlobFinalize(app_retained_state_blob_t *blob)
{
  if (blob == (app_retained_state_blob_t *)0)
  {
    return;
  }
  blob->magic = APP_RETAINED_STATE_MAGIC;
  blob->version = APP_RETAINED_STATE_VERSION;
  blob->size = (uint16_t)sizeof(*blob);
  blob->crc32 = 0UL;
  blob->crc32 = AppRetainedStateCrc32((const uint8_t *)blob, (uint32_t)sizeof(*blob));
}

static VOID AppRetainedStateBlobReset(app_retained_state_blob_t *blob)
{
  if (blob == (app_retained_state_blob_t *)0)
  {
    return;
  }
  (void)memset(blob, 0, sizeof(*blob));
  AppRetainedStateBlobFinalize(blob);
}

static UINT AppRetainedStateRestorePet(void)
{
  uint8_t crc_ok = AppRetainedStateBlobCrcValid(&g_retained_state_blob);

  AppRetainedStateSyncDebugFromBlob(&g_retained_state_blob, crc_ok);
  if (crc_ok == 0U)
  {
    AppRetainedStateBlobReset(&g_retained_state_blob);
    AppRetainedStateSyncDebugFromBlob(&g_retained_state_blob, 1U);
    if (g_retained_state_load_fail_count < 0xFFFFFFFFUL)
    {
      g_retained_state_load_fail_count++;
    }
    return TX_NOT_DONE;
  }

  if ((g_retained_state_blob.valid_mask & APP_RETAINED_VALID_PET) == 0UL)
  {
    if (g_retained_state_load_ok_count < 0xFFFFFFFFUL)
    {
      g_retained_state_load_ok_count++;
    }
    return TX_NOT_DONE;
  }

  if ((g_retained_state_blob.pet_state > APP_PET_STATE_RESTING) ||
      (g_retained_state_blob.pet_last_action > APP_PET_ACTION_REST) ||
      (g_retained_state_blob.pet_hunger_pct > 100UL) ||
      (g_retained_state_blob.pet_energy_pct > 100UL) ||
      (g_retained_state_blob.pet_mood_pct > 100UL))
  {
    g_retained_state_blob.valid_mask &= ~APP_RETAINED_VALID_PET;
    g_retained_state_blob.seq++;
    AppRetainedStateBlobFinalize(&g_retained_state_blob);
    AppRetainedStateSyncDebugFromBlob(&g_retained_state_blob, 1U);
    if (g_retained_state_load_fail_count < 0xFFFFFFFFUL)
    {
      g_retained_state_load_fail_count++;
    }
    return TX_NOT_DONE;
  }

  g_pet_state = (ULONG)g_retained_state_blob.pet_state;
  g_pet_tick_count = (ULONG)g_retained_state_blob.pet_tick_count;
  g_pet_wake_count = (ULONG)g_retained_state_blob.pet_wake_count;
  g_pet_last_action = (ULONG)g_retained_state_blob.pet_last_action;
  g_pet_hunger_pct = (ULONG)g_retained_state_blob.pet_hunger_pct;
  g_pet_energy_pct = (ULONG)g_retained_state_blob.pet_energy_pct;
  g_pet_mood_pct = (ULONG)g_retained_state_blob.pet_mood_pct;
  if (g_retained_state_load_ok_count < 0xFFFFFFFFUL)
  {
    g_retained_state_load_ok_count++;
  }
  return TX_SUCCESS;
}

static UINT AppRetainedStateSavePet(void)
{
  app_retained_state_blob_t work;

  if (tx_mutex_get(&g_mtx_retained, TX_WAIT_FOREVER) != TX_SUCCESS)
  {
    if (g_retained_state_save_fail_count < 0xFFFFFFFFUL)
    {
      g_retained_state_save_fail_count++;
    }
    return TX_NOT_AVAILABLE;
  }

  if (AppRetainedStateBlobCrcValid(&g_retained_state_blob) != 0U)
  {
    work = g_retained_state_blob;
  }
  else
  {
    AppRetainedStateBlobReset(&work);
  }

  work.pet_state = (uint32_t)g_pet_state;
  work.pet_tick_count = (uint32_t)g_pet_tick_count;
  work.pet_wake_count = (uint32_t)g_pet_wake_count;
  work.pet_last_action = (uint32_t)g_pet_last_action;
  work.pet_hunger_pct = (uint32_t)g_pet_hunger_pct;
  work.pet_energy_pct = (uint32_t)g_pet_energy_pct;
  work.pet_mood_pct = (uint32_t)g_pet_mood_pct;
  work.valid_mask |= APP_RETAINED_VALID_PET;
  work.seq++;
  AppRetainedStateBlobFinalize(&work);
  g_retained_state_blob = work;
  AppRetainedStateSyncDebugFromBlob(&g_retained_state_blob, 1U);
  if (g_retained_state_save_ok_count < 0xFFFFFFFFUL)
  {
    g_retained_state_save_ok_count++;
  }

  (void)tx_mutex_put(&g_mtx_retained);
  return TX_SUCCESS;
}

static UINT AppRetainedStateSaveGameTopdown(uint32_t mode_id,
                                            uint32_t backend_id,
                                            uint32_t scene_map_id,
                                            uint32_t scene_tileset_id,
                                            const game_mode_topdown_basic_snapshot_t *snapshot)
{
  app_retained_state_blob_t work;

  if (snapshot == (const game_mode_topdown_basic_snapshot_t *)0)
  {
    return TX_PTR_ERROR;
  }

  if (tx_mutex_get(&g_mtx_retained, TX_WAIT_FOREVER) != TX_SUCCESS)
  {
    if (g_retained_state_game_save_fail_count < 0xFFFFFFFFUL)
    {
      g_retained_state_game_save_fail_count++;
    }
    return TX_NOT_AVAILABLE;
  }

  if (AppRetainedStateBlobCrcValid(&g_retained_state_blob) != 0U)
  {
    work = g_retained_state_blob;
  }
  else
  {
    AppRetainedStateBlobReset(&work);
  }

  work.game.mode_id = mode_id;
  work.game.backend_id = backend_id;
  work.game.scene_map_id = scene_map_id;
  work.game.scene_tileset_id = scene_tileset_id;
  work.game.topdown = *snapshot;
  work.valid_mask |= APP_RETAINED_VALID_GAME;
  work.seq++;
  AppRetainedStateBlobFinalize(&work);
  g_retained_state_blob = work;
  AppRetainedStateSyncDebugFromBlob(&g_retained_state_blob, 1U);
  if (g_retained_state_save_ok_count < 0xFFFFFFFFUL)
  {
    g_retained_state_save_ok_count++;
  }
  if (g_retained_state_game_save_ok_count < 0xFFFFFFFFUL)
  {
    g_retained_state_game_save_ok_count++;
  }

  (void)tx_mutex_put(&g_mtx_retained);
  return TX_SUCCESS;
}

static UINT AppRetainedStateRestoreGameTopdown(uint32_t mode_id,
                                               uint32_t backend_id,
                                               game_mode_topdown_basic_snapshot_t *snapshot_out,
                                               uint32_t *scene_map_id_out,
                                               uint32_t *scene_tileset_id_out)
{
  UINT status = TX_NOT_DONE;

  if (snapshot_out == (game_mode_topdown_basic_snapshot_t *)0)
  {
    return TX_PTR_ERROR;
  }
  if (scene_map_id_out != (uint32_t *)0)
  {
    *scene_map_id_out = 0UL;
  }
  if (scene_tileset_id_out != (uint32_t *)0)
  {
    *scene_tileset_id_out = 0UL;
  }

  if (tx_mutex_get(&g_mtx_retained, TX_WAIT_FOREVER) != TX_SUCCESS)
  {
    if (g_retained_state_game_restore_fail_count < 0xFFFFFFFFUL)
    {
      g_retained_state_game_restore_fail_count++;
    }
    return TX_NOT_AVAILABLE;
  }

  if ((AppRetainedStateBlobCrcValid(&g_retained_state_blob) != 0U) &&
      ((g_retained_state_blob.valid_mask & APP_RETAINED_VALID_GAME) != 0UL) &&
      (g_retained_state_blob.game.backend_id == backend_id) &&
      (g_retained_state_blob.game.mode_id == mode_id) &&
      (g_retained_state_blob.game.topdown.valid != 0U) &&
      (g_retained_state_blob.game.topdown.version == GAME_MODE_TOPDOWN_BASIC_SNAPSHOT_VERSION))
  {
    *snapshot_out = g_retained_state_blob.game.topdown;
    if (scene_map_id_out != (uint32_t *)0)
    {
      *scene_map_id_out = g_retained_state_blob.game.scene_map_id;
    }
    if (scene_tileset_id_out != (uint32_t *)0)
    {
      *scene_tileset_id_out = g_retained_state_blob.game.scene_tileset_id;
    }
    status = TX_SUCCESS;
    if (g_retained_state_game_restore_ok_count < 0xFFFFFFFFUL)
    {
      g_retained_state_game_restore_ok_count++;
    }
  }
  else
  {
    if (g_retained_state_game_restore_fail_count < 0xFFFFFFFFUL)
    {
      g_retained_state_game_restore_fail_count++;
    }
  }
  AppRetainedStateSyncDebugFromBlob(&g_retained_state_blob,
                                    AppRetainedStateBlobCrcValid(&g_retained_state_blob));

  (void)tx_mutex_put(&g_mtx_retained);
  return status;
}

static UINT AppRetainedStateClearGame(void)
{
  app_retained_state_blob_t work;

  if (tx_mutex_get(&g_mtx_retained, TX_WAIT_FOREVER) != TX_SUCCESS)
  {
    if (g_retained_state_save_fail_count < 0xFFFFFFFFUL)
    {
      g_retained_state_save_fail_count++;
    }
    return TX_NOT_AVAILABLE;
  }

  if (AppRetainedStateBlobCrcValid(&g_retained_state_blob) != 0U)
  {
    work = g_retained_state_blob;
  }
  else
  {
    AppRetainedStateBlobReset(&work);
  }

  work.valid_mask &= ~APP_RETAINED_VALID_GAME;
  (void)memset(&work.game, 0, sizeof(work.game));
  work.seq++;
  AppRetainedStateBlobFinalize(&work);
  g_retained_state_blob = work;
  AppRetainedStateSyncDebugFromBlob(&g_retained_state_blob, 1U);
  if (g_retained_state_save_ok_count < 0xFFFFFFFFUL)
  {
    g_retained_state_save_ok_count++;
  }

  (void)tx_mutex_put(&g_mtx_retained);
  return TX_SUCCESS;
}

UINT App_SysEvent_ModeSet(app_mode_t mode_token)
{
  return AppSysEventPost(APP_SYS_EVT_MODE_SET, (ULONG)mode_token, 0UL, 0UL);
}

UINT App_SysEvent_QuiesceReq(void)
{
  return AppSysEventPost(APP_SYS_EVT_QUIESCE_REQ, 0UL, 0UL, 0UL);
}

UINT App_SysEvent_ResumeReq(void)
{
  return AppSysEventPost(APP_SYS_EVT_RESUME_REQ, 0UL, 0UL, 0UL);
}

UINT App_SysEvent_QuiesceAck(ULONG ack_mask)
{
  return AppSysEventPost(APP_SYS_EVT_QUIESCE_ACK, ack_mask, 0UL, 0UL);
}

UINT App_SysEvent_PerfHint(ULONG present_ticks, ULONG draw_ticks, ULONG dirty_rows, ULONG full_flush)
{
  ULONG hint_meta = (dirty_rows & 0xFFFFUL);

  if (full_flush != 0UL)
  {
    hint_meta |= (1UL << 16);
  }

  return AppSysEventPost(APP_SYS_EVT_PERF_HINT, present_ticks, draw_ticks, hint_meta);
}

UINT App_SysEvent_UsbVbusPresent(ULONG present)
{
  return AppSysEventPost(APP_SYS_EVT_USB_VBUS_PRESENT, (present != 0UL) ? 1UL : 0UL, 0UL, 0UL);
}

UINT App_UsbVbusPresent_Get(ULONG *present_out)
{
  if (present_out == TX_NULL)
  {
    return TX_PTR_ERROR;
  }

  *present_out = (g_sensor_pmic_vbus_present != 0UL) ? 1UL : 0UL;
  return TX_SUCCESS;
}

UINT App_UsbFlashPrompt_GetPending(ULONG *pending_out)
{
  if (pending_out == TX_NULL)
  {
    return TX_PTR_ERROR;
  }

  *pending_out = (g_usb_flash_prompt_pending != 0UL) ? 1UL : 0UL;
  return TX_SUCCESS;
}

UINT App_UsbFlashPrompt_Clear(void)
{
  g_usb_flash_prompt_pending = 0UL;
  return TX_SUCCESS;
}

UINT App_UsbFlash_RequestEnter(void)
{
  if (g_sensor_pmic_vbus_present == 0UL)
  {
    return TX_NOT_DONE;
  }

  g_usb_flash_prompt_pending = 0UL;
  return App_SysEvent_ModeSet(APP_MODE_FLASHING);
}

UINT App_PetReq_Action(app_pet_action_t action_id)
{
  return App_PetReq_ActionWithHold(action_id, 0UL);
}

UINT App_PetReq_ActionWithHold(app_pet_action_t action_id, ULONG hold_ms)
{
  return AppSysEventPost(APP_SYS_EVT_PET_ACTION, (ULONG)action_id, hold_ms, 0UL);
}

UINT App_ModeFlags_Get(ULONG *mode_flags_out)
{
  UINT status;
  ULONG actual_flags = 0UL;

  if (mode_flags_out == TX_NULL)
  {
    return TX_PTR_ERROR;
  }

  status = tx_event_flags_get(&g_eg_mode, APP_MODE_FLAGS_ALL, TX_OR, &actual_flags, TX_NO_WAIT);
  if ((status == TX_SUCCESS) || (status == TX_NO_EVENTS))
  {
    *mode_flags_out = (actual_flags & APP_MODE_FLAGS_ALL);
    return TX_SUCCESS;
  }

  return status;
}

UINT App_PowerFlags_Get(ULONG *power_flags_out)
{
  UINT status;
  ULONG actual_flags = 0UL;

  if (power_flags_out == TX_NULL)
  {
    return TX_PTR_ERROR;
  }

  status = tx_event_flags_get(&g_eg_power, APP_POWER_FLAGS_ALL, TX_OR, &actual_flags, TX_NO_WAIT);
  if ((status == TX_SUCCESS) || (status == TX_NO_EVENTS))
  {
    *power_flags_out = (actual_flags & APP_POWER_FLAGS_ALL);
    return TX_SUCCESS;
  }

  return status;
}

UINT App_Power_Stop2TimebaseTelemetryClear(void)
{
  AppPowerStop2TimebaseTelemetryClear();
  return TX_SUCCESS;
}

UINT App_RetainedStateClear(void)
{
  UINT status = TX_SUCCESS;
  uint8_t retained_lock_held = 0U;

  /* debug.gdb can invoke this helper outside normal thread context. */
  if (tx_thread_identify() != TX_NULL)
  {
    status = tx_mutex_get(&g_mtx_retained, TX_WAIT_FOREVER);
    if (status != TX_SUCCESS)
    {
      if (g_retained_state_save_fail_count < 0xFFFFFFFFUL)
      {
        g_retained_state_save_fail_count++;
      }
      return status;
    }
    retained_lock_held = 1U;
  }

  AppRetainedStateBlobReset(&g_retained_state_blob);
  AppRetainedStateSyncDebugFromBlob(&g_retained_state_blob, 1U);
  if (g_retained_state_save_ok_count < 0xFFFFFFFFUL)
  {
    g_retained_state_save_ok_count++;
  }
  if (retained_lock_held != 0U)
  {
    (void)tx_mutex_put(&g_mtx_retained);
  }

  return TX_SUCCESS;
}

UINT App_SensorHealthFlags_Get(ULONG *sensor_flags_out)
{
  UINT status;
  ULONG actual_flags = 0UL;

  if (sensor_flags_out == TX_NULL)
  {
    return TX_PTR_ERROR;
  }

  status = tx_event_flags_get(&g_eg_sensor_health, APP_SENSOR_HEALTH_FLAGS_ALL, TX_OR, &actual_flags, TX_NO_WAIT);
  if ((status == TX_SUCCESS) || (status == TX_NO_EVENTS))
  {
    *sensor_flags_out = (actual_flags & APP_SENSOR_HEALTH_FLAGS_ALL);
    return TX_SUCCESS;
  }

  return status;
}

UINT App_SensorSnapshot_Get(app_sensor_snapshot_t *snapshot_out)
{
  if (snapshot_out == TX_NULL)
  {
    return TX_PTR_ERROR;
  }

  (void)memset(snapshot_out, 0, sizeof(*snapshot_out));

  if (g_sensor_tmag.state == (ULONG)APP_SENSOR_STATE_READY)
  {
    snapshot_out->valid_mask |= APP_SENSOR_SNAPSHOT_VALID_JOY;
  }
  snapshot_out->joy_dir = g_sensor_joy_live_status.dir;
  snapshot_out->joy_input_mask = g_sensor_joy_live_status.input_mask;
  snapshot_out->joy_nx = g_sensor_joy_live_status.nx;
  snapshot_out->joy_ny = g_sensor_joy_live_status.ny;
  snapshot_out->joy_r_abs_mT = g_sensor_joy_live_status.r_abs_mT;
  snapshot_out->joy_deadzone_enabled = g_sensor_joy_live_status.deadzone_enabled;
  snapshot_out->joy_deadzone_mT = g_sensor_joy_live_status.deadzone_mT;
  snapshot_out->joy_last_sample_tick = g_sensor_tmag.last_success_tick;

  if (g_sensor_lis.state == (ULONG)APP_SENSOR_STATE_READY)
  {
    snapshot_out->valid_mask |= APP_SENSOR_SNAPSHOT_VALID_LIS;
  }
  snapshot_out->lis_x_raw = (LONG)g_sensor_lis_live.x_raw;
  snapshot_out->lis_y_raw = (LONG)g_sensor_lis_live.y_raw;
  snapshot_out->lis_z_raw = (LONG)g_sensor_lis_live.z_raw;
  snapshot_out->lis_status = g_sensor_lis_live.status;
  snapshot_out->lis_sample_count = g_sensor_lis_live.sample_count;
  snapshot_out->lis_last_sample_tick = g_sensor_lis_live.last_sample_tick;
  snapshot_out->lis_last_error = g_sensor_lis_live.last_error;

  return TX_SUCCESS;
}

UINT App_SensorReq_Poll(app_sensor_target_t targets)
{
  return AppSensorReqPost(APP_SENSOR_REQ_POLL, ((ULONG)targets & APP_SENSOR_TARGET_MASK_ALL), 0UL, 0UL);
}

UINT App_SensorReq_ConfigDefaults(app_sensor_target_t targets)
{
  return AppSensorReqPost(APP_SENSOR_REQ_CONFIG_DEFAULTS, ((ULONG)targets & APP_SENSOR_TARGET_MASK_ALL), 0UL, 0UL);
}

UINT App_SensorReq_HealthSnapshot(void)
{
  return AppSensorReqPost(APP_SENSOR_REQ_HEALTH_SNAPSHOT, 0UL, 0UL, 0UL);
}

UINT App_SensorReq_LisSetProfile(app_sensor_lis_profile_t profile)
{
  if ((profile != APP_SENSOR_LIS_PROFILE_LOW_POWER) &&
      (profile != APP_SENSOR_LIS_PROFILE_LIVE))
  {
    return TX_OPTION_ERROR;
  }

  return AppSensorReqPost(APP_SENSOR_REQ_LIS_SET_PROFILE, (ULONG)profile, 0UL, 0UL);
}

UINT App_SensorReq_LisSetLowPower(void)
{
  return App_SensorReq_LisSetProfile(APP_SENSOR_LIS_PROFILE_LOW_POWER);
}

UINT App_SensorReq_LisSetLive(void)
{
  return App_SensorReq_LisSetProfile(APP_SENSOR_LIS_PROFILE_LIVE);
}

UINT App_SensorReq_LisStreamStart(void)
{
  return AppSensorReqPost(APP_SENSOR_REQ_LIS_STREAM_START, 0UL, 0UL, 0UL);
}

UINT App_SensorReq_LisStreamStop(void)
{
  return AppSensorReqPost(APP_SENSOR_REQ_LIS_STREAM_STOP, 0UL, 0UL, 0UL);
}

UINT App_SensorReq_LisStepEnable(void)
{
  return AppSensorReqPost(APP_SENSOR_REQ_LIS_STEP_ENABLE, 0UL, 0UL, 0UL);
}

UINT App_SensorReq_LisStepDisable(void)
{
  return AppSensorReqPost(APP_SENSOR_REQ_LIS_STEP_DISABLE, 0UL, 0UL, 0UL);
}

UINT App_SensorReq_LisStepReset(void)
{
  return AppSensorReqPost(APP_SENSOR_REQ_LIS_STEP_RESET, 0UL, 0UL, 0UL);
}

static VOID AppPowerPerfClockApplyFail(ULONG stage, HAL_StatusTypeDef hal_status)
{
  g_power_perf_clock_apply_last_stage = stage;
  g_power_perf_clock_apply_last_hal = (LONG)hal_status;
  if (g_power_perf_clock_apply_fail_count < 0xFFFFFFFFUL)
  {
    g_power_perf_clock_apply_fail_count++;
  }
}

static UINT AppPowerPerfRetuneThreadXSysTick(ULONG fail_stage)
{
  uint32_t hclk_hz;
  uint32_t reload;

  hclk_hz = HAL_RCC_GetHCLKFreq();
  if (hclk_hz == 0UL)
  {
    AppPowerPerfClockApplyFail(fail_stage, HAL_ERROR);
    return TX_NOT_DONE;
  }

  reload = (hclk_hz / (uint32_t)TX_TIMER_TICKS_PER_SECOND);
  if (reload == 0UL)
  {
    reload = 1UL;
  }
  if (reload > 0x01000000UL)
  {
    AppPowerPerfClockApplyFail(fail_stage, HAL_ERROR);
    return TX_NOT_DONE;
  }

  SysTick->LOAD = (reload - 1UL);
  SysTick->VAL = 0UL;
  return TX_SUCCESS;
}

static UINT AppPowerPerfRestorePeriphClocks(void)
{
  RCC_PeriphCLKInitTypeDef periph_clk = {0};
  HAL_StatusTypeDef hal_status;

  periph_clk.PeriphClockSelection = RCC_PERIPHCLK_OSPI | RCC_PERIPHCLK_SAI1;
  periph_clk.Sai1ClockSelection = RCC_SAI1CLKSOURCE_PLL2;
  periph_clk.OspiClockSelection = RCC_OSPICLKSOURCE_PLL2;
  /* Keep periph clocks anchored to fixed 16 MHz source, independent of base MSI changes. */
  periph_clk.PLL2.PLL2Source = RCC_PLLSOURCE_HSI;
  periph_clk.PLL2.PLL2M = 1;
  periph_clk.PLL2.PLL2N = 32;
  periph_clk.PLL2.PLL2P = 125;
  periph_clk.PLL2.PLL2Q = 8;
  periph_clk.PLL2.PLL2R = 2;
  periph_clk.PLL2.PLL2RGE = RCC_PLLVCIRANGE_1;
  periph_clk.PLL2.PLL2FRACN = 0;
  periph_clk.PLL2.PLL2ClockOut = RCC_PLL2_DIVP | RCC_PLL2_DIVQ;

  hal_status = HAL_RCCEx_PeriphCLKConfig(&periph_clk);
  if (hal_status != HAL_OK)
  {
    AppPowerPerfClockApplyFail(APP_POWER_CLK_STAGE_NORM_PERIPH, hal_status);
    return TX_NOT_DONE;
  }

  return TX_SUCCESS;
}

static UINT AppPowerPerfApplyNormClock(void)
{
  g_power_perf_clock_apply_last_stage = APP_POWER_CLK_STAGE_NONE;
  g_power_perf_clock_apply_last_hal = 0L;

  /* CubeMX base clock config is authoritative and must survive regeneration. */
  SystemClock_Config();
  g_power_perf_base_sysclk_mhz = (HAL_RCC_GetSysClockFreq() + 500000UL) / 1000000UL;
  if (g_power_perf_base_sysclk_mhz == 0UL)
  {
    g_power_perf_base_sysclk_mhz = g_power_perf_profiles[APP_POWER_PERF_PROFILE_NORM].sysclk_mhz;
  }
  /* No current STOP path needs MSIK once the MSIK-kernel peripherals are gated. */
  __HAL_RCC_MSIKSTOP_DISABLE();

  if (AppPowerPerfRestorePeriphClocks() != TX_SUCCESS)
  {
    return TX_NOT_DONE;
  }
  AppPowerStopClockPolicyApply();
  if (((g_eg_mode.tx_event_flags_group_current & APP_MODE_FLAG_FLASHING) == 0UL) &&
      (g_usb_device_active == 0UL))
  {
    (void)AppUsbDeviceHardwareOff();
  }
  if (AppPowerPerfRetuneThreadXSysTick(APP_POWER_CLK_STAGE_NORM_SYSTICK) != TX_SUCCESS)
  {
    return TX_NOT_DONE;
  }

  g_power_perf_clock_apply_last_stage = APP_POWER_CLK_STAGE_NONE;
  g_power_perf_clock_apply_last_hal = 0L;
  return TX_SUCCESS;
}

static ULONG AppPowerPerfBoostFloorProfile(void)
{
  ULONG base_mhz = g_power_perf_base_sysclk_mhz;
  ULONG i;

  if (base_mhz == 0UL)
  {
    base_mhz = g_power_perf_profiles[APP_POWER_PERF_PROFILE_NORM].sysclk_mhz;
  }

  for (i = (APP_POWER_PERF_PROFILE_NORM + 1UL); i <= APP_POWER_PERF_PROFILE_MAX; ++i)
  {
    if (g_power_perf_profiles[i].sysclk_mhz >= base_mhz)
    {
      return i;
    }
  }

  return APP_POWER_PERF_PROFILE_MAX;
}

static ULONG AppPowerPerfTopProfileCap(void)
{
  ULONG cap_mhz = (ULONG)KNOB_TURBO_CLOCK_MHZ;
  ULONG base_mhz = g_power_perf_base_sysclk_mhz;
  ULONG i;

  if (base_mhz == 0UL)
  {
    base_mhz = g_power_perf_profiles[APP_POWER_PERF_PROFILE_NORM].sysclk_mhz;
  }
  if (cap_mhz < base_mhz)
  {
    cap_mhz = base_mhz;
  }
  if (cap_mhz > g_power_perf_profiles[APP_POWER_PERF_PROFILE_MAX].sysclk_mhz)
  {
    cap_mhz = g_power_perf_profiles[APP_POWER_PERF_PROFILE_MAX].sysclk_mhz;
  }

  for (i = APP_POWER_PERF_PROFILE_MAX; i > APP_POWER_PERF_PROFILE_NORM; --i)
  {
    if (g_power_perf_profiles[i].sysclk_mhz <= cap_mhz)
    {
      return i;
    }
  }

  return APP_POWER_PERF_PROFILE_NORM;
}

static uint32_t AppPowerPerfFlashLatencyForMHz(ULONG sysclk_mhz)
{
  if (sysclk_mhz > 128UL)
  {
    return FLASH_LATENCY_4;
  }
  if (sysclk_mhz > 96UL)
  {
    return FLASH_LATENCY_3;
  }
  if (sysclk_mhz > 64UL)
  {
    return FLASH_LATENCY_2;
  }
  if (sysclk_mhz > 32UL)
  {
    return FLASH_LATENCY_1;
  }
  return FLASH_LATENCY_0;
}

static UINT AppPowerPerfApplyPllClock(ULONG pll_n, ULONG pll_r, ULONG sysclk_mhz)
{
  RCC_OscInitTypeDef osc = {0};
  RCC_ClkInitTypeDef clk = {0};
  uint32_t target_flash_latency;
  HAL_StatusTypeDef hal_status;

  if ((pll_n == 0UL) || (pll_r == 0UL) || (pll_r > 128UL) ||
      ((pll_r != 1UL) && ((pll_r & 1UL) != 0UL)))
  {
    AppPowerPerfClockApplyFail(APP_POWER_CLK_STAGE_PERF_TARGET, HAL_ERROR);
    return TX_NOT_DONE;
  }

  g_power_perf_clock_apply_last_stage = APP_POWER_CLK_STAGE_NONE;
  g_power_perf_clock_apply_last_hal = 0L;

  target_flash_latency = AppPowerPerfFlashLatencyForMHz(sysclk_mhz);

  /* Move SYSCLK to MSI before retuning PLL so HAL can safely reconfigure it. */
  clk.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 |
                  RCC_CLOCKTYPE_PCLK2 | RCC_CLOCKTYPE_PCLK3;
  clk.SYSCLKSource = RCC_SYSCLKSOURCE_MSI;
  clk.AHBCLKDivider = RCC_SYSCLK_DIV1;
  clk.APB1CLKDivider = RCC_HCLK_DIV2;
  clk.APB2CLKDivider = RCC_HCLK_DIV2;
  clk.APB3CLKDivider = RCC_HCLK_DIV8;
  hal_status = HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_4);
  if (hal_status != HAL_OK)
  {
    AppPowerPerfClockApplyFail(APP_POWER_CLK_STAGE_PERF_SYSCLK, hal_status);
    return TX_NOT_DONE;
  }

  hal_status = HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1);
  if (hal_status != HAL_OK)
  {
    AppPowerPerfClockApplyFail(APP_POWER_CLK_STAGE_PERF_VOS, hal_status);
    return TX_NOT_DONE;
  }

  osc.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  osc.HSIState = RCC_HSI_ON;
  osc.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  osc.PLL.PLLState = RCC_PLL_OFF;
  hal_status = HAL_RCC_OscConfig(&osc);
  if (hal_status != HAL_OK)
  {
    AppPowerPerfClockApplyFail(APP_POWER_CLK_STAGE_PERF_PLL_ON, hal_status);
    return TX_NOT_DONE;
  }

  osc.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  osc.HSIState = RCC_HSI_ON;
  osc.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  osc.PLL.PLLState = RCC_PLL_ON;
  osc.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  osc.PLL.PLLM = 1;
  osc.PLL.PLLN = (uint32_t)pll_n;
  osc.PLL.PLLP = 2;
  osc.PLL.PLLQ = 2;
  osc.PLL.PLLR = (uint32_t)pll_r;
  osc.PLL.PLLRGE = RCC_PLLVCIRANGE_1;
  osc.PLL.PLLFRACN = 0;
  osc.PLL.PLLMBOOST = RCC_PLLMBOOST_DIV1;
  hal_status = HAL_RCC_OscConfig(&osc);
  if (hal_status != HAL_OK)
  {
    AppPowerPerfClockApplyFail(APP_POWER_CLK_STAGE_PERF_PLL_ON, hal_status);
    return TX_NOT_DONE;
  }

  clk.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 |
                  RCC_CLOCKTYPE_PCLK2 | RCC_CLOCKTYPE_PCLK3;
  clk.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  clk.AHBCLKDivider = RCC_SYSCLK_DIV1;
  clk.APB1CLKDivider = RCC_HCLK_DIV2;
  clk.APB2CLKDivider = RCC_HCLK_DIV2;
  clk.APB3CLKDivider = RCC_HCLK_DIV8;
  hal_status = HAL_RCC_ClockConfig(&clk, target_flash_latency);
  if (hal_status != HAL_OK)
  {
    AppPowerPerfClockApplyFail(APP_POWER_CLK_STAGE_PERF_SYSCLK, hal_status);
    return TX_NOT_DONE;
  }
  if (AppPowerPerfRetuneThreadXSysTick(APP_POWER_CLK_STAGE_PERF_SYSTICK) != TX_SUCCESS)
  {
    return TX_NOT_DONE;
  }

  g_power_perf_clock_apply_last_stage = APP_POWER_CLK_STAGE_NONE;
  g_power_perf_clock_apply_last_hal = 0L;
  return TX_SUCCESS;
}

static VOID AppPowerPerfReset(void)
{
  g_power_perf_profile_target = g_power_perf_profile_current;
  g_power_perf_hint_inflight = 0UL;
  g_power_perf_last_present_ticks = 0UL;
  g_power_perf_last_draw_ticks = 0UL;
  g_power_perf_last_dirty_rows = 0UL;
  g_power_perf_last_full_flush = 0UL;
  g_power_perf_miss_streak = 0UL;
  g_power_perf_headroom_streak = 0UL;
}

static uint8_t AppPowerPerfCanSwitch(ULONG now_tick)
{
  ULONG min_dwell = (ULONG)KNOB_POWER_PERF_MIN_DWELL_TICKS;

  if (min_dwell == 0UL)
  {
    return 1U;
  }

  if ((g_power_perf_last_switch_tick == 0UL) ||
      ((ULONG)(now_tick - g_power_perf_last_switch_tick) >= min_dwell))
  {
    return 1U;
  }

  if (g_power_perf_dwell_block_count < 0xFFFFFFFFUL)
  {
    g_power_perf_dwell_block_count++;
  }
  return 0U;
}

static VOID AppPowerPerfSetProfile(ULONG next_profile, ULONG now_tick)
{
  UINT clock_status;
  uint8_t bypass_dwell_up = 0U;
  ULONG profile_floor;
  ULONG profile_cap;
  ULONG mode_flags;
  ULONG audio_floor = APP_POWER_PERF_PROFILE_NORM;
  ULONG prev_profile;

  if (g_power_perf_force_up_no_dwell != 0UL)
  {
    bypass_dwell_up = 1U;
    g_power_perf_force_up_no_dwell = 0UL;
  }

  profile_floor = AppPowerPerfBoostFloorProfile();
  profile_cap = AppPowerPerfTopProfileCap();
  if ((next_profile > APP_POWER_PERF_PROFILE_NORM) && (profile_floor > profile_cap))
  {
    next_profile = APP_POWER_PERF_PROFILE_NORM;
  }
  else if ((next_profile > APP_POWER_PERF_PROFILE_NORM) && (next_profile < profile_floor))
  {
    next_profile = profile_floor;
  }
  if (next_profile > profile_cap)
  {
    next_profile = profile_cap;
  }

  mode_flags = (g_eg_mode.tx_event_flags_group_current & APP_MODE_FLAGS_ALL);
  if (((mode_flags & APP_MODE_FLAG_REALTIME) != 0UL) &&
      (g_power_perf_audio_boost_active != 0UL))
  {
    audio_floor = (ULONG)KNOB_POWER_PERF_REALTIME_ENTRY_PROFILE;
    if (audio_floor > APP_POWER_PERF_PROFILE_MAX)
    {
      audio_floor = APP_POWER_PERF_PROFILE_MAX;
    }
    if ((audio_floor > APP_POWER_PERF_PROFILE_NORM) && (audio_floor < profile_floor))
    {
      audio_floor = profile_floor;
    }
    if (audio_floor > profile_cap)
    {
      audio_floor = profile_cap;
    }
    if (next_profile < audio_floor)
    {
      next_profile = audio_floor;
    }
  }

  if (next_profile > APP_POWER_PERF_PROFILE_MAX)
  {
    return;
  }

  g_power_perf_profile_target = next_profile;
  if (next_profile == g_power_perf_profile_current)
  {
    return;
  }

  if ((bypass_dwell_up == 0U) || (next_profile <= g_power_perf_profile_current))
  {
    if (AppPowerPerfCanSwitch(now_tick) == 0U)
    {
      return;
    }
  }

  prev_profile = g_power_perf_profile_current;
  if (next_profile == APP_POWER_PERF_PROFILE_NORM)
  {
    clock_status = AppPowerPerfApplyNormClock();
  }
  else
  {
    clock_status = AppPowerPerfApplyPllClock(g_power_perf_profiles[next_profile].pll_n,
                                             g_power_perf_profiles[next_profile].pll_r,
                                             g_power_perf_profiles[next_profile].sysclk_mhz);
  }
  if (clock_status != TX_SUCCESS)
  {
    return;
  }

  g_power_perf_profile_current = next_profile;
  g_power_perf_last_switch_tick = now_tick;
  g_power_perf_miss_streak = 0UL;
  g_power_perf_headroom_streak = 0UL;

  if (next_profile > prev_profile)
  {
    if (g_power_perf_up_count < 0xFFFFFFFFUL)
    {
      g_power_perf_up_count++;
    }
  }
  else if (next_profile < prev_profile)
  {
    if (g_power_perf_down_count < 0xFFFFFFFFUL)
    {
      g_power_perf_down_count++;
    }
  }
}

static VOID AppPowerPerfOnModeChange(app_mode_t mode_token, ULONG now_tick)
{
  ULONG entry_profile;

  if (mode_token == APP_MODE_FLASHING)
  {
    /*
     * USB MSC transport is timing-sensitive on host probes.
     * Keep FLASHING at highest profile to maximize USB/RTOS service headroom.
     */
    AppPowerPerfSetProfile(APP_POWER_PERF_PROFILE_TURBO, now_tick);
    AppPowerPerfReset();
    return;
  }

  if (mode_token != APP_MODE_REALTIME)
  {
    AppPowerPerfSetProfile(APP_POWER_PERF_PROFILE_NORM, now_tick);
    AppPowerPerfReset();
    return;
  }

  entry_profile = (ULONG)KNOB_POWER_PERF_REALTIME_ENTRY_PROFILE;
  if (entry_profile > APP_POWER_PERF_PROFILE_MAX)
  {
    entry_profile = APP_POWER_PERF_PROFILE_TURBO;
  }

  /* REALTIME enters at a tunable profile to control startup headroom. */
  AppPowerPerfSetProfile(entry_profile, now_tick);
  AppPowerPerfReset();
}

static VOID AppPowerPerfHandleHint(ULONG present_ticks, ULONG draw_ticks, ULONG hint_meta, ULONG now_tick)
{
  ULONG mode_flags;
  ULONG budget_ticks;
  ULONG miss_margin;
  ULONG headroom_margin;
  ULONG boost_floor;
  ULONG up_streak;
  ULONG down_streak;
  ULONG dirty_rows;
  ULONG full_flush;
  ULONG io_ticks;
  ULONG work_ticks;
  ULONG overlap_ticks;
  ULONG serial_ticks;
  ULONG min_ticks;

  mode_flags = (g_eg_mode.tx_event_flags_group_current & APP_MODE_FLAGS_ALL);
  if ((mode_flags & (APP_MODE_FLAG_REALTIME | APP_MODE_FLAG_STATIC)) == 0UL)
  {
    return;
  }

  budget_ticks = (ULONG)KNOB_POWER_PERF_FRAME_BUDGET_TICKS;
  if (budget_ticks == 0UL)
  {
    return;
  }

  miss_margin = (ULONG)KNOB_POWER_PERF_MISS_MARGIN_TICKS;
  headroom_margin = (ULONG)KNOB_POWER_PERF_HEADROOM_MARGIN_TICKS;

  g_power_perf_last_present_ticks = present_ticks;
  g_power_perf_last_draw_ticks = draw_ticks;

  dirty_rows = (hint_meta & 0xFFFFUL);
  full_flush = ((hint_meta >> 16) & 0x1UL);
  if (full_flush != 0UL)
  {
    dirty_rows = DISPLAY_HEIGHT;
  }
  if (dirty_rows > DISPLAY_HEIGHT)
  {
    dirty_rows = DISPLAY_HEIGHT;
  }
  g_power_perf_last_dirty_rows = dirty_rows;
  g_power_perf_last_full_flush = full_flush;

  io_ticks = present_ticks;
  if ((full_flush == 0UL) && (DISPLAY_HEIGHT > 0U))
  {
    io_ticks = (present_ticks * dirty_rows + (ULONG)DISPLAY_HEIGHT - 1UL) / (ULONG)DISPLAY_HEIGHT;
  }
  overlap_ticks = draw_ticks;
  min_ticks = io_ticks;
  if (io_ticks > overlap_ticks)
  {
    overlap_ticks = io_ticks;
    min_ticks = draw_ticks;
  }
  serial_ticks = draw_ticks + io_ticks;
  work_ticks = overlap_ticks;
  if ((DISPLAY_HEIGHT > 0U) && (dirty_rows > 0UL) && (min_ticks > 0UL))
  {
    /* Scale from max(draw,io) at low dirty coverage toward draw+io at full flush. */
    work_ticks = overlap_ticks + ((min_ticks * dirty_rows + ((ULONG)DISPLAY_HEIGHT / 2UL)) / (ULONG)DISPLAY_HEIGHT);
  }
  if (work_ticks > serial_ticks)
  {
    work_ticks = serial_ticks;
  }

  if (work_ticks > (budget_ticks + miss_margin))
  {
    if (g_power_perf_miss_streak < 0xFFFFFFFFUL)
    {
      g_power_perf_miss_streak++;
    }
    g_power_perf_headroom_streak = 0UL;
  }
  else if ((work_ticks + headroom_margin) < budget_ticks)
  {
    if (g_power_perf_headroom_streak < 0xFFFFFFFFUL)
    {
      g_power_perf_headroom_streak++;
    }
    g_power_perf_miss_streak = 0UL;
  }
  else
  {
    g_power_perf_miss_streak = 0UL;
    g_power_perf_headroom_streak = 0UL;
  }

  up_streak = (ULONG)KNOB_POWER_PERF_UP_STREAK_FRAMES;
  down_streak = (ULONG)KNOB_POWER_PERF_DOWN_STREAK_FRAMES;
  boost_floor = AppPowerPerfBoostFloorProfile();
  if (up_streak == 0UL)
  {
    up_streak = 1UL;
  }
  if (down_streak == 0UL)
  {
    down_streak = 1UL;
  }

  if (g_power_perf_miss_streak >= up_streak)
  {
    AppPowerPerfSetProfile(g_power_perf_profile_current + 1UL, now_tick);
  }
  else if (g_power_perf_headroom_streak >= down_streak)
  {
    if (g_power_perf_profile_current > APP_POWER_PERF_PROFILE_NORM)
    {
      if (g_power_perf_profile_current > boost_floor)
      {
        AppPowerPerfSetProfile(g_power_perf_profile_current - 1UL, now_tick);
      }
      else
      {
        AppPowerPerfSetProfile(APP_POWER_PERF_PROFILE_NORM, now_tick);
      }
    }
  }
}

static VOID AppPowerPerfHintPost(ULONG present_ticks, ULONG draw_ticks, ULONG dirty_rows, ULONG full_flush)
{
  ULONG mode_flags;
  ULONG power_flags;
  ULONG stride;
  ULONG hint_meta;
  UINT status;

  mode_flags = (g_eg_mode.tx_event_flags_group_current & APP_MODE_FLAGS_ALL);
  if ((mode_flags & APP_MODE_FLAG_REALTIME) == 0UL)
  {
    return;
  }

  power_flags = (g_eg_power.tx_event_flags_group_current & APP_POWER_FLAGS_ALL);
  if ((power_flags & APP_POWER_FLAG_RUNNING) == 0UL)
  {
    return;
  }
  if ((power_flags & (APP_POWER_FLAG_QUIESCE_REQ | APP_POWER_FLAG_QUIESCED)) != 0UL)
  {
    return;
  }

  if (g_power_perf_hint_seq < 0xFFFFFFFFUL)
  {
    g_power_perf_hint_seq++;
  }
  stride = (ULONG)KNOB_POWER_PERF_HINT_STRIDE;
  if (stride == 0UL)
  {
    stride = 1UL;
  }
  if ((g_power_perf_hint_seq % stride) != 0UL)
  {
    return;
  }

  if (g_power_perf_hint_inflight != 0UL)
  {
    if (g_power_perf_hint_drop_count < 0xFFFFFFFFUL)
    {
      g_power_perf_hint_drop_count++;
    }
    return;
  }

  g_power_perf_hint_inflight = 1UL;
  hint_meta = (dirty_rows & 0xFFFFUL);
  if (full_flush != 0UL)
  {
    hint_meta |= (1UL << 16);
  }
  status = AppSysEventPost(APP_SYS_EVT_PERF_HINT, present_ticks, draw_ticks, hint_meta);
  if (status == TX_SUCCESS)
  {
    if (g_power_perf_hint_post_count < 0xFFFFFFFFUL)
    {
      g_power_perf_hint_post_count++;
    }
  }
  else
  {
    g_power_perf_hint_inflight = 0UL;
    if (g_power_perf_hint_drop_count < 0xFFFFFFFFUL)
    {
      g_power_perf_hint_drop_count++;
    }
  }
}

static UINT AppModeTokenToFlag(app_mode_t mode_token, ULONG *mode_flag_out)
{
  if (mode_flag_out == TX_NULL)
  {
    return TX_PTR_ERROR;
  }

  switch (mode_token)
  {
    case APP_MODE_STOP:
      *mode_flag_out = APP_MODE_FLAG_STOP;
      break;
    case APP_MODE_STATIC:
      *mode_flag_out = APP_MODE_FLAG_STATIC;
      break;
    case APP_MODE_REALTIME:
      *mode_flag_out = APP_MODE_FLAG_REALTIME;
      break;
    case APP_MODE_FLASHING:
      *mode_flag_out = APP_MODE_FLAG_FLASHING;
      break;
    default:
      return TX_OPTION_ERROR;
  }

  return TX_SUCCESS;
}

static UINT AppSetModeFlag(app_mode_t mode_token)
{
  UINT status;
  ULONG mode_flag = 0UL;

  status = AppModeTokenToFlag(mode_token, &mode_flag);
  if (status != TX_SUCCESS)
  {
    return status;
  }

  status = tx_event_flags_set(&g_eg_mode, ~APP_MODE_FLAGS_ALL, TX_AND);
  if (status != TX_SUCCESS)
  {
    return status;
  }

  return tx_event_flags_set(&g_eg_mode, mode_flag, TX_OR);
}

static UINT AppSysEventPost(app_sys_event_type_t event_type, ULONG arg0, ULONG arg1, ULONG arg2)
{
  app_sys_event_t msg = {0UL};

  msg.type = (ULONG)event_type;
  msg.arg0 = arg0;
  msg.arg1 = arg1;
  msg.arg2 = arg2;

  return tx_queue_send(&g_q_sys_events, &msg, TX_NO_WAIT);
}

static UINT AppDisplayCmdPost(app_display_cmd_type_t cmd_type, ULONG arg0)
{
  app_display_cmd_t msg = {0UL};
  UINT status;

  msg.type = (ULONG)cmd_type;
  msg.arg0 = arg0;

  if (cmd_type == APP_DISPLAY_CMD_PRESENT)
  {
    if (g_display_present_pending != 0UL)
    {
      if (g_q_display_cmd.tx_queue_enqueued == 0UL)
      {
        /* Recover from stale pending latch so PRESENT cannot deadlock. */
        g_display_present_pending = 0UL;
      }
      else
      {
        g_display_present_coalesce_count++;
        return TX_SUCCESS;
      }
    }
    if (g_display_present_pending != 0UL)
    {
      g_display_present_coalesce_count++;
      return TX_SUCCESS;
    }
    g_display_present_pending = 1UL;
  }

  status = tx_queue_send(&g_q_display_cmd, &msg, TX_NO_WAIT);
  if (status == TX_SUCCESS)
  {
    if (cmd_type == APP_DISPLAY_CMD_PRESENT)
    {
      g_display_present_post_count++;
    }
    return TX_SUCCESS;
  }

  if (cmd_type == APP_DISPLAY_CMD_PRESENT)
  {
    g_display_present_pending = 0UL;
    g_display_present_send_fail_count++;
  }
  return status;
}

static UINT AppStorageReqPostEx(app_storage_req_type_t req_type, ULONG arg0, ULONG arg1, ULONG arg2)
{
  app_storage_req_t msg = {0UL};
  ULONG mode_flags = (g_eg_mode.tx_event_flags_group_current & APP_MODE_FLAGS_ALL);

  if (((mode_flags & APP_MODE_FLAG_FLASHING) != 0UL) &&
      (AppStorageReqAllowedInFlashing(req_type) == 0U))
  {
    return TX_NOT_DONE;
  }

  msg.type = (ULONG)req_type;
  msg.arg0 = arg0;
  msg.arg1 = arg1;
  msg.arg2 = arg2;

  return tx_queue_send(&g_q_storage_req, &msg, TX_NO_WAIT);
}

static UINT AppStorageReqPost(app_storage_req_type_t req_type, ULONG arg0)
{
  return AppStorageReqPostEx(req_type, arg0, 0UL, 0UL);
}

static UINT AppInputCmdPost(app_input_cmd_type_t cmd_type, ULONG arg0)
{
  app_input_cmd_t msg = {0UL};

  msg.type = (ULONG)cmd_type;
  msg.arg0 = arg0;

  return tx_queue_send(&g_q_input_cmd, &msg, TX_NO_WAIT);
}

static UINT AppAudioCmdPost(app_audio_cmd_type_t cmd_type, ULONG arg0)
{
  app_audio_cmd_t msg = {0UL};
  UINT status;

  msg.type = (ULONG)cmd_type;
  msg.arg0 = arg0;

  status = tx_queue_send(&g_q_audio_cmd, &msg, TX_NO_WAIT);
  if (status == TX_SUCCESS)
  {
    if (g_audio_cmd_post_count < 0xFFFFFFFFUL)
    {
      g_audio_cmd_post_count++;
    }
  }
  else if (g_audio_cmd_drop_count < 0xFFFFFFFFUL)
  {
    g_audio_cmd_drop_count++;
  }

  return status;
}

static UINT AppSensorReqPost(app_sensor_req_type_t req_type, ULONG arg0, ULONG arg1, ULONG arg2)
{
  app_sensor_req_t msg = {0UL};

  msg.type = (ULONG)req_type;
  msg.arg0 = arg0;
  msg.arg1 = arg1;
  msg.arg2 = arg2;

  return tx_queue_send(&g_q_sensor_req, &msg, TX_NO_WAIT);
}

static UINT AppUsbDeviceStart(void)
{
  HAL_StatusTypeDef hal_status;
  UINT hw_status;

  if (g_usb_device_active != 0UL)
  {
    return TX_SUCCESS;
  }

  /*
   * Ensure each FLASHING session starts from a fully stopped PCD.
   * Stale controller state between detach/attach cycles can wedge BOT
   * at first INQUIRY (host waits then cancels).
   */
  if (hpcd_USB_OTG_FS.State != HAL_PCD_STATE_RESET)
  {
    (void)AppUsbDeviceStop();
  }

  /* Do not allow PCD/IRQ activity unless USBX init has completed successfully. */
  if ((g_usbx_device_pool_create_status != TX_SUCCESS) ||
      (g_usbx_device_init_status != 0U) ||
      (g_usbx_init_stage < 101U) ||
      (g_usbx_init_error_code != 0U))
  {
    g_usb_device_last_error = -6L;
    if (g_usb_device_start_fail_count < 0xFFFFFFFFUL)
    {
      g_usb_device_start_fail_count++;
    }
    return TX_NOT_DONE;
  }

  hw_status = AppUsbDeviceHardwareInit();
  if (hw_status != TX_SUCCESS)
  {
    g_usb_device_last_error = -7L;
    if (g_usb_device_start_fail_count < 0xFFFFFFFFUL)
    {
      g_usb_device_start_fail_count++;
    }
    return hw_status;
  }

  /*
   * Keep EP0 bring-up simple: start once, connect once.
   * Repeated stop/start churn has shown intermittent descriptor xact errors.
   */
  hal_status = HAL_PCD_Start(&hpcd_USB_OTG_FS);
  if ((hal_status != HAL_OK) && (hal_status != HAL_BUSY))
  {
    g_usb_device_last_error = -1L;
    if (g_usb_device_start_fail_count < 0xFFFFFFFFUL)
    {
      g_usb_device_start_fail_count++;
    }
    (void)AppUsbDeviceHardwareOff();
    return TX_NOT_DONE;
  }

  NVIC_ClearPendingIRQ(OTG_FS_IRQn);
  HAL_NVIC_EnableIRQ(OTG_FS_IRQn);

  hal_status = HAL_PCD_DevConnect(&hpcd_USB_OTG_FS);
  if ((hal_status != HAL_OK) && (hal_status != HAL_BUSY))
  {
    g_usb_device_last_error = -2L;
    if (g_usb_device_start_fail_count < 0xFFFFFFFFUL)
    {
      g_usb_device_start_fail_count++;
    }
    (void)AppUsbDeviceHardwareOff();
    return TX_NOT_DONE;
  }

  g_usb_device_active = 1UL;
  g_usb_device_last_error = 0L;
  if (g_usb_device_start_ok_count < 0xFFFFFFFFUL)
  {
    g_usb_device_start_ok_count++;
  }
  return TX_SUCCESS;
}

static UINT AppUsbDeviceStop(void)
{
  return AppUsbDeviceStopWithGrace(0UL);
}

static UINT AppUsbDeviceStopWithGrace(ULONG disconnect_grace_ticks)
{
  HAL_StatusTypeDef hal_status;
  UINT hw_status;
  uint8_t had_error = 0U;
  uint8_t need_teardown = ((g_usb_device_active != 0UL) || (hpcd_USB_OTG_FS.State != HAL_PCD_STATE_RESET)) ? 1U : 0U;

  if (need_teardown != 0U)
  {
    hal_status = HAL_PCD_DevDisconnect(&hpcd_USB_OTG_FS);
    if ((hal_status != HAL_OK) && (hal_status != HAL_BUSY))
    {
      had_error = 1U;
      g_usb_device_last_error = -3L;
    }

    /* Give host MSC/BOT a bounded detach window before hard stop. */
    if (disconnect_grace_ticks > 0UL)
    {
      tx_thread_sleep(disconnect_grace_ticks);
    }

    /*
     * Fully stop the controller between sessions.
     * Disconnect alone is insufficient for reliable host re-enumeration.
     */
    hal_status = HAL_PCD_Stop(&hpcd_USB_OTG_FS);
    if ((hal_status != HAL_OK) && (hal_status != HAL_BUSY))
    {
      had_error = 1U;
      g_usb_device_last_error = -4L;
    }
  }

  HAL_NVIC_DisableIRQ(OTG_FS_IRQn);
  NVIC_ClearPendingIRQ(OTG_FS_IRQn);
  hw_status = AppUsbDeviceHardwareOff();
  if (hw_status != TX_SUCCESS)
  {
    had_error = 1U;
    if (g_usb_device_last_error == 0L)
    {
      g_usb_device_last_error = -5L;
    }
  }

  /* Never retain active state after a stop attempt. */
  g_usb_device_active = 0UL;

  if (need_teardown == 0U)
  {
    g_usb_device_last_error = 0L;
    return TX_SUCCESS;
  }

  if (had_error == 0U)
  {
    g_usb_device_last_error = 0L;
    if (g_usb_device_stop_ok_count < 0xFFFFFFFFUL)
    {
      g_usb_device_stop_ok_count++;
    }
    return TX_SUCCESS;
  }

  if (g_usb_device_stop_fail_count < 0xFFFFFFFFUL)
  {
    g_usb_device_stop_fail_count++;
  }
  return TX_NOT_DONE;
}

static VOID AppPowerStopClockPolicyApply(void)
{
  CLEAR_BIT(RCC->AHB1SMENR, RCC_AHB1SMENR_GPDMA1SMEN);
  CLEAR_BIT(RCC->AHB2SMENR1, RCC_AHB2SMENR1_OTGSMEN | RCC_AHB2SMENR1_OCTOSPIMSMEN);
  CLEAR_BIT(RCC->AHB2SMENR2, RCC_AHB2SMENR2_OCTOSPI1SMEN);
  CLEAR_BIT(RCC->AHB3SMENR, RCC_AHB3SMENR_LPDMA1SMEN);
  CLEAR_BIT(RCC->APB1SMENR1, RCC_APB1SMENR1_TIM2SMEN);
  CLEAR_BIT(RCC->APB2SMENR, RCC_APB2SMENR_SAI1SMEN);
  CLEAR_BIT(RCC->APB3SMENR, RCC_APB3SMENR_SPI3SMEN | RCC_APB3SMENR_LPUART1SMEN | RCC_APB3SMENR_I2C3SMEN);
}

static UINT AppUsbClock48Set(uint32_t hsi48_state)
{
  RCC_OscInitTypeDef osc = {0};
  HAL_StatusTypeDef hal_status;

  osc.OscillatorType = RCC_OSCILLATORTYPE_HSI48;
  osc.HSI48State = hsi48_state;
  osc.PLL.PLLState = RCC_PLL_NONE;

  hal_status = HAL_RCC_OscConfig(&osc);
  return (hal_status == HAL_OK) ? TX_SUCCESS : TX_NOT_DONE;
}

static VOID AppUsbVddUsbSet(UINT enabled)
{
  UINT pwr_clk_was_disabled = (__HAL_RCC_PWR_IS_CLK_DISABLED() != 0U) ? 1U : 0U;

  if (pwr_clk_was_disabled != 0U)
  {
    __HAL_RCC_PWR_CLK_ENABLE();
  }

  if (enabled != 0U)
  {
    HAL_PWREx_EnableVddUSB();
  }
  else
  {
    HAL_PWREx_DisableVddUSB();
  }

  if (pwr_clk_was_disabled != 0U)
  {
    __HAL_RCC_PWR_CLK_DISABLE();
  }
}

static UINT AppUsbDeviceHardwareInit(void)
{
  if (hpcd_USB_OTG_FS.State != HAL_PCD_STATE_RESET)
  {
    return TX_SUCCESS;
  }

  if (AppUsbClock48Set(RCC_HSI48_ON) != TX_SUCCESS)
  {
    return TX_NOT_DONE;
  }

  AppUsbVddUsbSet(1U);

  if (HAL_PCD_Init(&hpcd_USB_OTG_FS) != HAL_OK)
  {
    (void)AppUsbDeviceHardwareOff();
    return TX_NOT_DONE;
  }

  if (HAL_PCDEx_SetRxFiFo(&hpcd_USB_OTG_FS, 0x80U) != HAL_OK)
  {
    (void)AppUsbDeviceHardwareOff();
    return TX_NOT_DONE;
  }

  if (HAL_PCDEx_SetTxFiFo(&hpcd_USB_OTG_FS, 0U, 0x40U) != HAL_OK)
  {
    (void)AppUsbDeviceHardwareOff();
    return TX_NOT_DONE;
  }

  if (HAL_PCDEx_SetTxFiFo(&hpcd_USB_OTG_FS, 1U, 0x80U) != HAL_OK)
  {
    (void)AppUsbDeviceHardwareOff();
    return TX_NOT_DONE;
  }

  return TX_SUCCESS;
}

static UINT AppUsbDeviceHardwareOff(void)
{
  HAL_StatusTypeDef hal_status = HAL_OK;
  uint8_t had_error = 0U;

  HAL_NVIC_DisableIRQ(OTG_FS_IRQn);
  NVIC_ClearPendingIRQ(OTG_FS_IRQn);

  if (hpcd_USB_OTG_FS.State != HAL_PCD_STATE_RESET)
  {
    hal_status = HAL_PCD_DeInit(&hpcd_USB_OTG_FS);
    if (hal_status != HAL_OK)
    {
      had_error = 1U;
    }
  }
  else
  {
    __HAL_RCC_USB_CLK_DISABLE();
  }
  __HAL_RCC_USB_CLK_DISABLE();

  AppUsbVddUsbSet(0U);
  if (AppUsbClock48Set(RCC_HSI48_OFF) != TX_SUCCESS)
  {
    had_error = 1U;
  }

  return (had_error == 0U) ? TX_SUCCESS : TX_NOT_DONE;
}

static UINT AppPowerFlagsUpdate(ULONG set_mask, ULONG clear_mask)
{
  UINT status;

  if (clear_mask != 0UL)
  {
    status = tx_event_flags_set(&g_eg_power, ~clear_mask, TX_AND);
    if (status != TX_SUCCESS)
    {
      return status;
    }
  }

  if (set_mask != 0UL)
  {
    return tx_event_flags_set(&g_eg_power, set_mask, TX_OR);
  }

  return TX_SUCCESS;
}

static uint8_t AppInputTranslateRaw(const app_input_raw_evt_t *raw_evt, app_input_action_evt_t *action_evt)
{
  if ((raw_evt == NULL) || (action_evt == NULL))
  {
    return 0U;
  }

  if ((raw_evt->edge != APP_INPUT_EDGE_LOW) && (raw_evt->edge != APP_INPUT_EDGE_HIGH))
  {
    return 0U;
  }

  action_evt->source = raw_evt->source;
  action_evt->event = (raw_evt->edge == APP_INPUT_EDGE_LOW) ? (ULONG)APP_INPUT_EVENT_PRESS : (ULONG)APP_INPUT_EVENT_RELEASE;
  action_evt->tick = raw_evt->tick;
  action_evt->action = (ULONG)AppInputActionForSource(raw_evt->source);
  action_evt->pressed_mask = 0UL;
  if (action_evt->action == (ULONG)APP_INPUT_ACTION_NONE)
  {
    return 0U;
  }

  if (((ULONG)KNOB_INPUT_BOOT_LONG_ONLY != 0UL) && (raw_evt->source == APP_INPUT_SOURCE_BTN_BOOT))
  {
    return 0U;
  }

  return 1U;
}

static app_input_action_t AppInputActionForSource(ULONG source)
{
  switch (source)
  {
    case APP_INPUT_SOURCE_BTN_A:
      return APP_INPUT_ACTION_BTN_A;
    case APP_INPUT_SOURCE_BTN_B:
      return APP_INPUT_ACTION_BTN_B;
    case APP_INPUT_SOURCE_BTN_L:
      return APP_INPUT_ACTION_BTN_L;
    case APP_INPUT_SOURCE_BTN_R:
      return APP_INPUT_ACTION_BTN_R;
    case APP_INPUT_SOURCE_BTN_BOOT:
      return APP_INPUT_ACTION_BTN_BOOT;
    case APP_INPUT_SOURCE_JOY_UP:
      return APP_INPUT_ACTION_JOY_UP;
    case APP_INPUT_SOURCE_JOY_RIGHT:
      return APP_INPUT_ACTION_JOY_RIGHT;
    case APP_INPUT_SOURCE_JOY_DOWN:
      return APP_INPUT_ACTION_JOY_DOWN;
    case APP_INPUT_SOURCE_JOY_LEFT:
      return APP_INPUT_ACTION_JOY_LEFT;
    default:
      return APP_INPUT_ACTION_NONE;
  }
}

static uint8_t AppInputSourceValid(ULONG source)
{
  return ((source >= APP_INPUT_SOURCE_BTN_A) && (source <= APP_INPUT_SOURCE_MAX)) ? 1U : 0U;
}

static ULONG AppInputSourceBit(ULONG source)
{
  if (AppInputSourceValid(source) == 0U)
  {
    return 0UL;
  }

  return APP_INPUT_SRCBIT(source);
}

static uint8_t AppInputSourceIsPhysicalButton(ULONG source)
{
  return ((source >= APP_INPUT_SOURCE_BTN_A) && (source <= APP_INPUT_SOURCE_BTN_BOOT)) ? 1U : 0U;
}

static uint8_t AppInputSourceIsJoystick(ULONG source)
{
  return ((source >= APP_INPUT_SOURCE_JOY_UP) && (source <= APP_INPUT_SOURCE_JOY_LEFT)) ? 1U : 0U;
}

static uint8_t AppInputReadPhysicalLevel(ULONG source, ULONG *level_out)
{
  GPIO_TypeDef *port = TX_NULL;
  uint16_t pin = 0U;

  if (level_out == TX_NULL)
  {
    return 0U;
  }

  switch (source)
  {
    case APP_INPUT_SOURCE_BTN_A:
      port = BTN_A_GPIO_Port;
      pin = BTN_A_Pin;
      break;

    case APP_INPUT_SOURCE_BTN_B:
      port = BTN_B_GPIO_Port;
      pin = BTN_B_Pin;
      break;

    case APP_INPUT_SOURCE_BTN_L:
      port = BTN_L_GPIO_Port;
      pin = BTN_L_Pin;
      break;

    case APP_INPUT_SOURCE_BTN_R:
      port = BTN_R_GPIO_Port;
      pin = BTN_R_Pin;
      break;

    case APP_INPUT_SOURCE_BTN_BOOT:
      port = BTN_BOOT_GPIO_Port;
      pin = BTN_BOOT_Pin;
      break;

    default:
      return 0U;
  }

  *level_out = (HAL_GPIO_ReadPin(port, pin) == GPIO_PIN_SET) ? 1UL : 0UL;
  return 1U;
}

static VOID AppInputRefreshPhysicalIdleLevels(void)
{
  ULONG source;
  ULONG level = 0UL;

  g_input_physical_idle_valid_mask = 0UL;
  for (source = APP_INPUT_SOURCE_BTN_A; source <= APP_INPUT_SOURCE_BTN_BOOT; source++)
  {
    if (AppInputReadPhysicalLevel(source, &level) != 0U)
    {
      g_input_physical_idle_level[source] = level;
      g_input_physical_idle_valid_mask |= AppInputSourceBit(source);
    }
  }
}

static uint8_t AppInputPhysicalPressedFromLevel(ULONG source, ULONG level, ULONG *pressed_out)
{
  if ((pressed_out == TX_NULL) || (AppInputSourceIsPhysicalButton(source) == 0U))
  {
    return 0U;
  }
  /* A/B/L/R/BOOT are active-high in this board path (EXTI both edges).
   * Keep this aligned with hardware truth-table and ISR trace behavior. */
  *pressed_out = (level != 0UL) ? 1UL : 0UL;
  return 1U;
}

static uint8_t AppInputSourceRepeatEnabled(ULONG source)
{
  ULONG bit = AppInputSourceBit(source);

  if (bit == 0UL)
  {
    return 0U;
  }

  return (((ULONG)KNOB_INPUT_REPEAT_ENABLE_MASK & bit) != 0UL) ? 1U : 0U;
}

static uint8_t AppInputSourceLongEnabled(ULONG source)
{
  ULONG bit = AppInputSourceBit(source);

  if (bit == 0UL)
  {
    return 0U;
  }

  return (((ULONG)KNOB_INPUT_LONG_ENABLE_MASK & bit) != 0UL) ? 1U : 0U;
}

static ULONG AppInputRepeatPeriodTicks(ULONG source, ULONG held_ticks)
{
  ULONG stage1_after = (ULONG)KNOB_INPUT_REPEAT_ACCEL_STAGE1_AFTER_TICKS;
  ULONG stage2_after = (ULONG)KNOB_INPUT_REPEAT_ACCEL_STAGE2_AFTER_TICKS;
  ULONG period_base = (ULONG)KNOB_INPUT_REPEAT_PERIOD_TICKS;
  ULONG period_stage1 = (ULONG)KNOB_INPUT_REPEAT_PERIOD_STAGE1_TICKS;
  ULONG period_stage2 = (ULONG)KNOB_INPUT_REPEAT_PERIOD_STAGE2_TICKS;

  if (AppInputSourceIsJoystick(source) != 0U)
  {
    stage1_after = (ULONG)KNOB_INPUT_JOY_REPEAT_ACCEL_STAGE1_AFTER_TICKS;
    stage2_after = (ULONG)KNOB_INPUT_JOY_REPEAT_ACCEL_STAGE2_AFTER_TICKS;
    period_base = (ULONG)KNOB_INPUT_JOY_REPEAT_PERIOD_TICKS;
    period_stage1 = (ULONG)KNOB_INPUT_JOY_REPEAT_PERIOD_STAGE1_TICKS;
    period_stage2 = (ULONG)KNOB_INPUT_JOY_REPEAT_PERIOD_STAGE2_TICKS;
  }

  if (stage2_after > 0UL)
  {
    if (held_ticks >= stage2_after)
    {
      return period_stage2;
    }
  }

  if (stage1_after > 0UL)
  {
    if (held_ticks >= stage1_after)
    {
      return period_stage1;
    }
  }

  return period_base;
}

static UINT AppInputPushActionToQueue(TX_QUEUE *queue, const app_input_action_evt_t *evt, ULONG *drop_oldest_counter)
{
  UINT status;
  app_input_action_evt_t discarded_evt;

  if ((queue == TX_NULL) || (evt == TX_NULL))
  {
    return TX_PTR_ERROR;
  }

  status = tx_queue_send(queue, (VOID *)evt, TX_NO_WAIT);
  if (status == TX_QUEUE_FULL)
  {
    if (tx_queue_receive(queue, &discarded_evt, TX_NO_WAIT) == TX_SUCCESS)
    {
      if (drop_oldest_counter != TX_NULL)
      {
        (*drop_oldest_counter)++;
      }
      status = tx_queue_send(queue, (VOID *)evt, TX_NO_WAIT);
    }
  }

  return status;
}

static VOID AppInputDropQueuedRepeatsForSource(TX_QUEUE *queue, ULONG source)
{
  UINT status;
  ULONG count;
  app_input_action_evt_t evt;

  if ((queue == TX_NULL) || (AppInputSourceValid(source) == 0U))
  {
    return;
  }

  count = (ULONG)queue->tx_queue_enqueued;
  while (count > 0UL)
  {
    status = tx_queue_receive(queue, &evt, TX_NO_WAIT);
    if (status != TX_SUCCESS)
    {
      break;
    }

    if (!((evt.source == source) && (evt.event == (ULONG)APP_INPUT_EVENT_REPEAT)))
    {
      (void)tx_queue_send(queue, (VOID *)&evt, TX_NO_WAIT);
    }
    count--;
  }
}

static uint8_t AppSensorJoyLiveUpdate(TMAGJoy_Dir dir, ULONG input_mask, uint8_t enabled)
{
  ULONG fail_threshold;
  TMAGJoy_Cal cal;
  float thr_x = 0.0f;
  float thr_y = 0.0f;
  uint8_t deadzone_en = 0U;
  float deadzone_mT = 0.0f;

  if ((enabled == 0U) || (g_sensor_joy == TX_NULL) ||
      (g_sensor_tmag.state != (ULONG)APP_SENSOR_STATE_READY))
  {
    g_sensor_joy_live_read_fail_streak = 0UL;
    g_sensor_joy_live_status.dir = (ULONG)TMAGJOY_NEUTRAL;
    g_sensor_joy_live_status.input_mask = 0UL;
    g_sensor_joy_live_status.nx = 0.0f;
    g_sensor_joy_live_status.ny = 0.0f;
    g_sensor_joy_live_status.r_abs_mT = 0.0f;
    return 1U;
  }

  g_sensor_joy_live_status.dir = (ULONG)dir;
  g_sensor_joy_live_status.input_mask = input_mask;
  AppSensorJoyEnsureAbsDeadzoneConfigured();
  if (TMAGJoy_ReadCalibratedShaped(g_sensor_joy,
                                   &g_sensor_joy_live_status.nx,
                                   &g_sensor_joy_live_status.ny,
                                   &g_sensor_joy_live_status.r_abs_mT) != 0)
  {
    g_sensor_joy_live_status.dir = (ULONG)TMAGJOY_NEUTRAL;
    g_sensor_joy_live_status.input_mask = 0UL;
    g_sensor_joy_live_status.nx = 0.0f;
    g_sensor_joy_live_status.ny = 0.0f;
    g_sensor_joy_live_status.r_abs_mT = 0.0f;
    g_sensor_tmag.last_error = -209L;

    if (g_sensor_joy_live_read_fail_streak < 0xFFFFFFFFUL)
    {
      g_sensor_joy_live_read_fail_streak++;
    }

    fail_threshold = (ULONG)KNOB_SENSOR_RECOVERY_MAX_ATTEMPTS * 2UL;
    if (fail_threshold < 3UL)
    {
      fail_threshold = 3UL;
    }
    if (g_sensor_joy_live_read_fail_streak >= fail_threshold)
    {
      AppSensorTmagMarkRuntimeFault(-209L);
      g_sensor_joy_live_read_fail_streak = 0UL;
    }

    return 0U;
  }

  g_sensor_joy_live_read_fail_streak = 0UL;
  if (g_sensor_tmag.last_error == -209L)
  {
    g_sensor_tmag.last_error = 0L;
  }

  TMAGJoy_GetCal(g_sensor_joy, &cal);
  g_sensor_joy_live_status.center_x_mT = cal.cx;
  g_sensor_joy_live_status.center_y_mT = cal.cy;
  g_sensor_joy_live_status.span_x_mT = cal.sx;
  g_sensor_joy_live_status.span_y_mT = cal.sy;
  g_sensor_joy_live_status.rotation_deg = cal.rot_deg;
  g_sensor_joy_live_status.invert_x = cal.invert_x;
  g_sensor_joy_live_status.invert_y = cal.invert_y;

  TMAGJoy_GetThresholds(g_sensor_joy, &thr_x, &thr_y);
  g_sensor_joy_live_status.threshold_x_mT = thr_x;
  g_sensor_joy_live_status.threshold_y_mT = thr_y;

  TMAGJoy_GetAbsDeadzone(g_sensor_joy, &deadzone_en, &deadzone_mT);
  g_sensor_joy_live_status.deadzone_enabled = deadzone_en;
  g_sensor_joy_live_status.deadzone_mT = deadzone_mT;
  return 1U;
}

static void AppSensorJoyEnsureAbsDeadzoneConfigured(void)
{
  uint8_t deadzone_en = 0U;
  float deadzone_mT = 0.0f;
  float target_mT = AppStorageDeadzoneClampMt(g_storage_joycfg_deadzone_mT);
  uint8_t target_en = (g_storage_joycfg_deadzone_enabled != 0UL) ? 1U : 0U;

  if (g_sensor_joy == TX_NULL)
  {
    return;
  }

  TMAGJoy_GetAbsDeadzone(g_sensor_joy, &deadzone_en, &deadzone_mT);
  if ((deadzone_en != target_en) || (fabsf(deadzone_mT - target_mT) > 0.05f))
  {
    TMAGJoy_SetAbsDeadzone(g_sensor_joy, target_en, target_mT);
  }
}

static uint8_t AppInputTickDue(ULONG now_tick, ULONG deadline_tick)
{
  return (((LONG)(now_tick - deadline_tick)) >= 0L) ? 1U : 0U;
}

static uint8_t AppInputShouldDebounce(ULONG source, ULONG edge, ULONG tick)
{
  app_input_button_state_t *state;
  ULONG debounce_ticks = (ULONG)KNOB_INPUT_DEBOUNCE_TICKS;

  if (AppInputSourceValid(source) == 0U)
  {
    return 1U;
  }

  if ((edge != APP_INPUT_EDGE_LOW) && (edge != APP_INPUT_EDGE_HIGH))
  {
    return 1U;
  }

  state = &g_input_button_state[source];

  /* Drop duplicate edges that don't change logical pressed state. */
  if ((edge == APP_INPUT_EDGE_LOW) && (state->pressed != 0UL))
  {
    return 1U;
  }
  if ((edge == APP_INPUT_EDGE_HIGH) && (state->pressed == 0UL) && (state->edge_seen != 0UL))
  {
    return 1U;
  }

  if (AppInputSourceIsJoystick(source) != 0U)
  {
    debounce_ticks = (ULONG)KNOB_INPUT_JOY_DEBOUNCE_TICKS;
  }

  if (state->edge_seen == 0UL)
  {
    /* Ignore initial release while idle, but do not arm debounce yet. */
    if (edge == APP_INPUT_EDGE_HIGH)
    {
      return 1U;
    }

    state->edge_seen = 1UL;
    state->last_edge_tick = tick;
    return 0U;
  }

  if ((debounce_ticks > 0UL) && ((tick - state->last_edge_tick) < debounce_ticks))
  {
    /* Always accept release transitions to prevent stuck-pressed latches. */
    if ((edge == APP_INPUT_EDGE_HIGH) && (state->pressed != 0UL))
    {
      g_input_release_pass_count++;
    }
    else
    {
      return 1U;
    }
  }

  state->last_edge_tick = tick;
  return 0U;
}

static UINT AppInputPostSystemEvent(app_sys_event_type_t event_type, ULONG arg0, ULONG arg1, ULONG arg2)
{
  return AppSysEventPost(event_type, arg0, arg1, arg2);
}

static VOID AppInputProcessRepeat(ULONG now_tick)
{
  ULONG source;
  ULONG mode_flags = (g_eg_mode.tx_event_flags_group_current & APP_MODE_FLAGS_ALL);

  for (source = APP_INPUT_SOURCE_BTN_A; source <= APP_INPUT_SOURCE_MAX; source++)
  {
    app_input_button_state_t *state = &g_input_button_state[source];
    app_input_action_evt_t evt = {0};
    ULONG held_ticks;
    ULONG repeat_delay_ticks;
    ULONG repeat_period_ticks;

    if (state->pressed == 0UL)
    {
      continue;
    }

    if (AppInputSourceIsPhysicalButton(source) != 0U)
    {
      ULONG level_now = 0UL;
      ULONG pressed_now = 1UL;

      if ((AppInputReadPhysicalLevel(source, &level_now) != 0U) &&
          (AppInputPhysicalPressedFromLevel(source, level_now, &pressed_now) != 0U) &&
          (pressed_now == 0UL))
      {
        state->pressed = 0UL;
        state->long_sent = 0UL;
        state->next_repeat_tick = 0UL;
        state->last_edge_tick = now_tick;
        g_input_release_reconcile_count++;
        continue;
      }
    }

    if ((mode_flags & APP_MODE_FLAG_STOP) != 0UL)
    {
      if (AppInputSourceIsJoystick(source) != 0U)
      {
        state->pressed = 0UL;
      }
      state->long_sent = 0UL;
      state->next_repeat_tick = 0UL;
      continue;
    }

    if (((mode_flags & APP_MODE_FLAG_FLASHING) != 0UL) && (source != APP_INPUT_SOURCE_BTN_BOOT))
    {
      state->next_repeat_tick = 0UL;
      continue;
    }

    if ((g_sensor_joy_cal_status.save_pending != 0UL) &&
        ((source == APP_INPUT_SOURCE_BTN_A) || (source == APP_INPUT_SOURCE_BTN_B)))
    {
      state->next_repeat_tick = 0UL;
      continue;
    }

    if (AppInputSourceLongEnabled(source) != 0U)
    {
      if ((state->long_sent == 0UL) &&
          ((ULONG)KNOB_INPUT_LONG_PRESS_TICKS > 0UL) &&
          (AppInputTickDue(now_tick, state->press_tick + (ULONG)KNOB_INPUT_LONG_PRESS_TICKS) != 0U))
      {
        evt.source = source;
        evt.tick = now_tick;
        evt.event = (ULONG)APP_INPUT_EVENT_LONG;
        evt.pressed_mask = 0UL;
        state->long_sent = 1UL;
        evt.action = (ULONG)AppInputActionForSource(source);

        if (evt.action != (ULONG)APP_INPUT_ACTION_NONE)
        {
          g_input_long_emit_count++;
          AppInputRouteAction(&evt);
        }
      }
    }

    if ((mode_flags & APP_MODE_FLAG_FLASHING) != 0UL)
    {
      /* In FLASHING, only BOOT long-press remains active. */
      state->next_repeat_tick = 0UL;
      continue;
    }

    if (AppInputSourceIsJoystick(source) != 0U)
    {
      repeat_delay_ticks = (ULONG)KNOB_INPUT_JOY_REPEAT_DELAY_TICKS;
      repeat_period_ticks = (ULONG)KNOB_INPUT_JOY_REPEAT_PERIOD_TICKS;
    }
    else
    {
      repeat_delay_ticks = (ULONG)KNOB_INPUT_REPEAT_DELAY_TICKS;
      repeat_period_ticks = (ULONG)KNOB_INPUT_REPEAT_PERIOD_TICKS;
    }

    if ((AppInputSourceRepeatEnabled(source) == 0U) ||
        (repeat_delay_ticks == 0UL) ||
        (repeat_period_ticks == 0UL))
    {
      continue;
    }

    if (state->next_repeat_tick == 0UL)
    {
      state->next_repeat_tick = (state->press_tick + repeat_delay_ticks);
    }

    if (AppInputTickDue(now_tick, state->next_repeat_tick) == 0U)
    {
      continue;
    }

    evt.source = source;
    evt.event = (ULONG)APP_INPUT_EVENT_REPEAT;
    evt.tick = now_tick;
    evt.pressed_mask = 0UL;
    evt.action = (ULONG)AppInputActionForSource(source);

    held_ticks = (ULONG)(now_tick - state->press_tick);
    state->next_repeat_tick = (now_tick + AppInputRepeatPeriodTicks(source, held_ticks));
    if (evt.action != (ULONG)APP_INPUT_ACTION_NONE)
    {
      g_input_repeat_emit_count++;
      AppInputRouteAction(&evt);
    }
  }
}

static uint8_t AppInputIsOfficialActivitySource(ULONG source)
{
  switch (source)
  {
    case APP_INPUT_SOURCE_BTN_A:
    case APP_INPUT_SOURCE_BTN_B:
    case APP_INPUT_SOURCE_BTN_L:
    case APP_INPUT_SOURCE_BTN_R:
    case APP_INPUT_SOURCE_JOY_UP:
    case APP_INPUT_SOURCE_JOY_RIGHT:
    case APP_INPUT_SOURCE_JOY_DOWN:
    case APP_INPUT_SOURCE_JOY_LEFT:
      return 1U;
    default:
      return 0U;
  }
}

static VOID AppInputRouteAction(const app_input_action_evt_t *action_evt)
{
  ULONG mode_flags;
  ULONG source;
  ULONG pressed_mask = 0UL;
  UINT status;
  app_input_action_evt_t routed_evt;

  if (action_evt == NULL)
  {
    return;
  }

  if (action_evt->event == (ULONG)APP_INPUT_EVENT_REPEAT)
  {
    if ((AppInputSourceValid(action_evt->source) == 0U) ||
        (g_input_button_state[action_evt->source].pressed == 0UL))
    {
      return;
    }
  }

  routed_evt = *action_evt;

  for (source = APP_INPUT_SOURCE_BTN_A; source <= APP_INPUT_SOURCE_MAX; source++)
  {
    if (g_input_button_state[source].pressed != 0UL)
    {
      pressed_mask |= AppInputSourceBit(source);
    }
  }
  routed_evt.pressed_mask = pressed_mask;

  mode_flags = (g_eg_mode.tx_event_flags_group_current & APP_MODE_FLAGS_ALL);
  g_input_action_total_count++;
  g_input_action_last = routed_evt.action;
  g_input_action_last_mode = mode_flags;

  if ((mode_flags & APP_MODE_FLAG_STOP) != 0UL)
  {
    if ((routed_evt.source == APP_INPUT_SOURCE_BTN_BOOT) ||
        (AppInputSourceIsJoystick(routed_evt.source) != 0U))
    {
      g_input_action_ignored_count++;
      return;
    }
    if (routed_evt.event != (ULONG)APP_INPUT_EVENT_PRESS)
    {
      g_input_action_ignored_count++;
      return;
    }
  }

  if ((mode_flags & APP_MODE_FLAG_FLASHING) == 0UL)
  {
    uint8_t post_activity = 0U;
    if ((routed_evt.event == (ULONG)APP_INPUT_EVENT_PRESS) &&
        (AppInputIsOfficialActivitySource(routed_evt.source) != 0U))
    {
      post_activity = 1U;
    }

    if (post_activity != 0U)
    {
      status = AppInputPostSystemEvent(APP_SYS_EVT_INPUT_ACTIVITY, routed_evt.action, routed_evt.source, routed_evt.tick);
      if (status == TX_SUCCESS)
      {
        g_input_sys_activity_post_count++;
        g_input_sys_activity_last_tick = routed_evt.tick;
      }
      else
      {
        g_input_sys_activity_drop_count++;
      }
    }
  }

  if (routed_evt.action == (ULONG)APP_INPUT_ACTION_BTN_BOOT)
  {
    /* BOOT long-press system override path (also allowed in FLASHING). */
    if (((ULONG)KNOB_INPUT_BOOT_LONG_ONLY != 0UL) &&
        ((routed_evt.source != APP_INPUT_SOURCE_BTN_BOOT) || (routed_evt.event != (ULONG)APP_INPUT_EVENT_LONG)))
    {
      g_input_action_ignored_count++;
      return;
    }

    g_input_action_system_route_count++;
    status = AppInputPostSystemEvent(APP_SYS_EVT_INPUT_MENU, routed_evt.action, routed_evt.source, routed_evt.tick);
    if (status == TX_SUCCESS)
    {
      g_input_sys_menu_post_count++;
    }
    else
    {
      g_input_sys_menu_drop_count++;
    }
    return;
  }

  if ((mode_flags & APP_MODE_FLAG_FLASHING) != 0UL)
  {
    if ((routed_evt.action == (ULONG)APP_INPUT_ACTION_BTN_B) &&
        (routed_evt.event == (ULONG)APP_INPUT_EVENT_PRESS))
    {
      g_input_action_system_route_count++;
      status = AppInputPostSystemEvent(APP_SYS_EVT_MODE_SET, (ULONG)APP_MODE_STATIC, 0UL, 0UL);
      if (status == TX_SUCCESS)
      {
        g_input_sys_menu_post_count++;
      }
      else
      {
        g_input_sys_menu_drop_count++;
      }
      return;
    }

    g_input_action_ignored_count++;
    return;
  }

  if ((mode_flags & APP_MODE_FLAG_REALTIME) != 0UL)
  {
    if (routed_evt.event != (ULONG)APP_INPUT_EVENT_PRESS)
    {
      /* REALTIME action queue only carries edge PRESS events. */
      g_input_action_ignored_count++;
      return;
    }
    g_input_action_game_route_count++;
    status = AppInputPushActionToQueue(&g_q_game_events, &routed_evt, &g_input_action_game_drop_oldest_count);
    if (status == TX_SUCCESS)
    {
      g_input_action_game_post_count++;
    }
    else
    {
      g_input_action_game_drop_count++;
    }
    return;
  }

  if ((mode_flags & (APP_MODE_FLAG_STOP | APP_MODE_FLAG_STATIC)) != 0UL)
  {
    g_input_action_ui_route_count++;
    status = AppInputPushActionToQueue(&g_q_ui_events, &routed_evt, &g_input_action_ui_drop_oldest_count);
    if (status == TX_SUCCESS)
    {
      g_input_action_ui_post_count++;
    }
    else
    {
      g_input_action_ui_drop_count++;
    }
    return;
  }

  g_input_action_ignored_count++;
}

static uint8_t AppInputResolveSource(uint16_t gpio_pin, GPIO_TypeDef **port_out, ULONG *source_out)
{
  if ((port_out == NULL) || (source_out == NULL))
  {
    return 0U;
  }

  switch (gpio_pin)
  {
    case BTN_A_Pin:
      *port_out = BTN_A_GPIO_Port;
      *source_out = APP_INPUT_SOURCE_BTN_A;
      return 1U;

    case BTN_B_Pin:
      *port_out = BTN_B_GPIO_Port;
      *source_out = APP_INPUT_SOURCE_BTN_B;
      return 1U;

    case BTN_L_Pin:
      *port_out = BTN_L_GPIO_Port;
      *source_out = APP_INPUT_SOURCE_BTN_L;
      return 1U;

    case BTN_R_Pin:
      *port_out = BTN_R_GPIO_Port;
      *source_out = APP_INPUT_SOURCE_BTN_R;
      return 1U;

    case BTN_BOOT_Pin:
      *port_out = BTN_BOOT_GPIO_Port;
      *source_out = APP_INPUT_SOURCE_BTN_BOOT;
      return 1U;

    default:
      return 0U;
  }
}

static VOID AppInputPostRawEvent(ULONG source, ULONG edge, ULONG level, ULONG tick)
{
  app_input_raw_evt_t evt = {0};
  UINT status;

  evt.source = source;
  evt.edge = edge;
  evt.level = level;
  evt.tick = tick;

  if (g_q_input_raw.tx_queue_start == TX_NULL)
  {
    g_input_raw_drop_count++;
    return;
  }

  status = tx_queue_send(&g_q_input_raw, &evt, TX_NO_WAIT);
  if (status == TX_SUCCESS)
  {
    g_input_raw_post_count++;
  }
  else
  {
    g_input_raw_drop_count++;
  }
}

static void AppInputHandleExti(uint16_t GPIO_Pin, ULONG edge)
{
  GPIO_TypeDef *port = NULL;
  ULONG source;
  ULONG level;
  ULONG logical_edge;
  ULONG tick;

  if ((edge != APP_INPUT_EDGE_HIGH) && (edge != APP_INPUT_EDGE_LOW))
  {
    return;
  }

  if (GPIO_Pin == ADP5360_INT_Pin)
  {
    if (g_q_sensor_req.tx_queue_start != TX_NULL)
    {
      (void)AppSensorReqPost(APP_SENSOR_REQ_PMIC_IRQ, 0UL, 0UL, 0UL);
    }
    return;
  }

  if (AppInputResolveSource(GPIO_Pin, &port, &source) == 0U)
  {
    return;
  }

  (void)port;
  level = (edge == APP_INPUT_EDGE_HIGH) ? 1UL : 0UL;
  logical_edge = edge;
  if (AppInputSourceIsPhysicalButton(source) != 0U)
  {
    ULONG pressed_now = 0UL;
    if (AppInputPhysicalPressedFromLevel(source, level, &pressed_now) != 0U)
    {
      logical_edge = (pressed_now != 0UL) ? APP_INPUT_EDGE_LOW : APP_INPUT_EDGE_HIGH;
    }
  }
  tick = (ULONG)HAL_GetTick();

  AppInputPostRawEvent(source, logical_edge, level, tick);
}

void HAL_GPIO_EXTI_Rising_Callback(uint16_t GPIO_Pin)
{
  AppInputHandleExti(GPIO_Pin, APP_INPUT_EDGE_HIGH);
}

void HAL_GPIO_EXTI_Falling_Callback(uint16_t GPIO_Pin)
{
  AppInputHandleExti(GPIO_Pin, APP_INPUT_EDGE_LOW);
}

void HAL_SAI_TxHalfCpltCallback(SAI_HandleTypeDef *hsai)
{
  if ((hsai == TX_NULL) || (hsai->Instance != SAI1_Block_A) || (g_audio_state != APP_AUDIO_STATE_ACTIVE))
  {
    return;
  }

  g_audio_half_irq_count++;
  g_audio_dma_half_pending++;
  g_audio_dma_events |= APP_AUDIO_DMA_HALF_FLAG;
  (void)tx_event_flags_set(&g_eg_audio_dma, APP_AUDIO_DMA_HALF_FLAG, TX_OR);
}

void HAL_SAI_TxCpltCallback(SAI_HandleTypeDef *hsai)
{
  if ((hsai == TX_NULL) || (hsai->Instance != SAI1_Block_A) || (g_audio_state != APP_AUDIO_STATE_ACTIVE))
  {
    return;
  }

  g_audio_full_irq_count++;
  g_audio_dma_full_pending++;
  g_audio_dma_events |= APP_AUDIO_DMA_FULL_FLAG;
  (void)tx_event_flags_set(&g_eg_audio_dma, APP_AUDIO_DMA_FULL_FLAG, TX_OR);
}

void HAL_SAI_ErrorCallback(SAI_HandleTypeDef *hsai)
{
  if ((hsai == TX_NULL) || (hsai->Instance != SAI1_Block_A) || (g_audio_state != APP_AUDIO_STATE_ACTIVE))
  {
    return;
  }

  g_audio_error_irq_count++;
  g_audio_dma_error_pending++;
  g_audio_dma_events |= APP_AUDIO_DMA_ERROR_FLAG;
  (void)tx_event_flags_set(&g_eg_audio_dma, APP_AUDIO_DMA_ERROR_FLAG, TX_OR);
  g_audio_last_error = (LONG)hsai->ErrorCode;
}

/* USER CODE END 1 */
