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
#include "render_demo.h"
#include "th_mode.h"
#include "ui/ui_router.h"
#include "knobs_autogen.h"
#include <string.h>
#include <math.h>

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
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
  APP_INPUT_ACTION_CONFIRM = 1U,
  APP_INPUT_ACTION_CANCEL = 2U,
  APP_INPUT_ACTION_LEFT = 3U,
  APP_INPUT_ACTION_RIGHT = 4U,
  APP_INPUT_ACTION_MENU = 5U,
  APP_INPUT_ACTION_UP = 6U,
  APP_INPUT_ACTION_DOWN = 7U
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
  APP_UI_PAGE_JOY_CAL = 1U
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
} app_storage_joycfg_blob_t;

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
#define APP_POWER_PERF_PROFILE_TURBO (1UL)
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

#define APP_DISPLAY_VLT_ACTIVE_LEVEL   (GPIO_PIN_RESET)
#define APP_AUDIO_CHANNEL_COUNT        2UL
#define APP_AUDIO_DMA_SAMPLE_COUNT     (KNOB_AUDIO_DMA_FRAMES * APP_AUDIO_CHANNEL_COUNT)
#define APP_AUDIO_STATE_STOPPED        0UL
#define APP_AUDIO_STATE_ACTIVE         1UL
#define APP_AUDIO_DMA_HALF_FLAG        (1UL << 0)
#define APP_AUDIO_DMA_FULL_FLAG        (1UL << 1)
#define APP_AUDIO_DMA_ERROR_FLAG       (1UL << 2)

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
#define APP_STORAGE_FILEX_VOLUME_NAME  "PEEPSHOW"
#define APP_STORAGE_FILEX_MEDIA_NAME   "PS_FAT"
#define APP_STORAGE_SETTINGS_SECTOR_SIZE 4096UL
#define APP_STORAGE_JOYCFG_MAGIC       0x4A594346UL
#define APP_STORAGE_JOYCFG_VERSION     2UL
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
static ULONG g_renderer_lock_error_count;

static TX_THREAD g_th_storage;
static ULONG g_th_storage_stack[KNOB_RTOS_STORAGE_THREAD_STACK_BYTES / sizeof(ULONG)];

static TX_QUEUE g_q_storage_req;
static ULONG g_q_storage_req_storage[KNOB_RTOS_QSTORAGE_REQ_DEPTH * ((ULONG)APP_STORAGE_REQ_WORDS)];

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
static ULONG g_power_pending_ack_mask;
static ULONG g_power_quiesce_wait_active;
static ULONG g_power_quiesce_wait_elapsed_ticks;
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
static ULONG g_storage_flash_ready;
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
static ULONG g_storage_joycfg_valid;
static LONG g_storage_joycfg_last_error;
static ULONG g_storage_joycfg_load_ok_count;
static ULONG g_storage_joycfg_load_fail_count;
static ULONG g_storage_joycfg_save_ok_count;
static ULONG g_storage_joycfg_save_fail_count;
static ULONG g_storage_joycfg_load_seq;
static ULONG g_storage_joycfg_save_seq;
static TMAGJoy_Cal g_storage_joycfg_cal;
const ULONG g_storage_settings_addr_dbg = (ULONG)KNOB_STORAGE_SETTINGS_ADDR;
static AT25_Debug g_storage_at25_dbg;
static FX_MEDIA g_storage_fx_media;
static UCHAR g_storage_filex_cache[KNOB_STORAGE_FILEX_CACHE_BYTES];
extern SPI_HandleTypeDef hspi3;
extern SAI_HandleTypeDef hsai_BlockA1;
extern OSPI_HandleTypeDef hospi1;
static LS013B7DH05 g_display_dev;
static uint8_t g_display_ready;
static uint16_t g_display_dirty_rows[DISPLAY_HEIGHT];
static ULONG g_dbg_display_stack_min_sp;
static ULONG g_dbg_display_stack_sample_count;
static int16_t g_audio_dma_buffer[KNOB_AUDIO_DMA_FRAMES * APP_AUDIO_CHANNEL_COUNT];
static volatile ULONG g_audio_dma_events;
static volatile ULONG g_audio_state;
static ULONG g_audio_start_count;
static ULONG g_audio_stop_count;
static ULONG g_audio_restart_count;
static ULONG g_audio_underflow_count;
static ULONG g_audio_half_irq_count;
static ULONG g_audio_full_irq_count;
static ULONG g_audio_error_irq_count;
static volatile LONG g_audio_last_error;
static uint32_t g_audio_phase_accum;
static uint32_t g_audio_phase_step;
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
static ULONG g_input_physical_idle_level[APP_INPUT_SOURCE_COUNT];
static ULONG g_input_physical_idle_valid_mask;
static ULONG g_power_input_activity_count;
static ULONG g_power_last_input_tick;
static ULONG g_power_menu_event_count;
static ULONG g_game_exit_to_static_pending;
static ULONG g_power_perf_profile_current;
static ULONG g_power_perf_profile_target;
static ULONG g_power_perf_last_switch_tick;
static ULONG g_power_perf_hint_seq;
static ULONG g_power_perf_hint_post_count;
static ULONG g_power_perf_hint_drop_count;
static ULONG g_power_perf_hint_rx_count;
static ULONG g_power_perf_hint_inflight;
static ULONG g_power_perf_last_present_ticks;
static ULONG g_power_perf_miss_streak;
static ULONG g_power_perf_headroom_streak;
static ULONG g_power_perf_up_count;
static ULONG g_power_perf_down_count;
static ULONG g_power_perf_dwell_block_count;
static ULONG g_ui_page_current;
static ULONG g_ui_page_dirty;
static ULONG g_ui_mode_flags_last;
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
static VOID AppDisplayThreadEntry(ULONG thread_input);
static VOID AppStorageThreadEntry(ULONG thread_input);
static VOID AppInputThreadEntry(ULONG thread_input);
static VOID AppUiThreadEntry(ULONG thread_input);
static VOID AppGameThreadEntry(ULONG thread_input);
static VOID AppAudioThreadEntry(ULONG thread_input);
static VOID AppSensorThreadEntry(ULONG thread_input);
static VOID AppPowerThreadEntry(ULONG thread_input);
static VOID AppStorageCaptureDebug(void);
static LONG AppStorageHalToError(HAL_StatusTypeDef hal_status);
static UINT AppStorageRunFlashProbe(void);
static UINT AppStorageRunRawSmoke(void);
static ULONG AppStorageFatTotalSectors(void);
static UINT AppStorageRunFileXMount(void);
static UINT AppStorageRunFileXFormat(void);
static UINT AppStorageRunFileXUnmount(void);
static uint32_t AppStorageCrc32(const uint8_t *data, uint32_t len);
static uint8_t AppSensorJoyCalSane(const TMAGJoy_Cal *cal);
static UINT AppStorageRunJoyCfgLoad(void);
static UINT AppStorageRunJoyCfgSave(void);
static UINT AppModeTokenToFlag(app_mode_t mode_token, ULONG *mode_flag_out);
static UINT AppSetModeFlag(app_mode_t mode_token);
static UINT AppSysEventPost(app_sys_event_type_t event_type, ULONG arg0, ULONG arg1, ULONG arg2);
static UINT AppDisplayCmdPost(app_display_cmd_type_t cmd_type, ULONG arg0);
static UINT AppStorageReqPost(app_storage_req_type_t req_type, ULONG arg0);
static UINT AppInputCmdPost(app_input_cmd_type_t cmd_type, ULONG arg0);
static UINT AppAudioCmdPost(app_audio_cmd_type_t cmd_type, ULONG arg0);
static UINT AppSensorReqPost(app_sensor_req_type_t req_type, ULONG arg0, ULONG arg1, ULONG arg2);
static UINT AppPowerFlagsUpdate(ULONG set_mask, ULONG clear_mask);
static VOID AppPowerPerfReset(void);
static VOID AppPowerPerfOnModeChange(app_mode_t mode_token, ULONG now_tick);
static VOID AppPowerPerfHandleHint(ULONG present_ticks, ULONG now_tick);
static VOID AppPowerPerfHintPost(ULONG present_ticks);
static UINT AppSensorHealthFlagsWrite(ULONG set_mask, ULONG clear_mask);
static UINT AppSensorHealthFlagsPublish(void);
static VOID AppSensorMarkAllSuspended(void);
static VOID AppSensorMarkAllOff(void);
static VOID AppSensorRunResumeSequence(void);
static VOID AppSensorRunPollSequence(ULONG targets_mask);
static VOID AppSensorApplyDefaults(ULONG targets_mask);
static VOID AppSensorHandleModeChange(app_mode_t mode_token);
static uint8_t AppSensorRecoveryNeeded(void);
static uint8_t AppSensorModeHasAny(ULONG mode_mask);
static uint8_t AppSensorAutoRecoveryAllowed(void);
static void AppSensorJoyCalResetStatus(void);
static void AppSensorJoyCalApplyLoadedIfReady(void);
static void AppSensorJoyCalStart(void);
static void AppSensorJoyCalStep(void);
static void AppSensorJoyCalQualityReset(void);
static void AppSensorJoyCalComputeQuality(const TMAGJoy_Cal *final_cal);
static void AppSensorJoyCalSnapshot(void);
static void AppSensorJoyCalRestoreSnapshot(void);
static void AppSensorJoyCalCancel(void);
static void AppSensorJoyCaptureBegin(uint32_t duration_ms, uint32_t sample_every_ms);
static uint8_t AppSensorJoyCaptureStep(uint32_t now_ms, float *progress_out, float *avg_x_out, float *avg_y_out);
static uint8_t AppSensorJoyCalApplyDirectionalSolve(void);
static uint8_t AppSensorJoyCalDirectionMatchesStage(ULONG stage, float avg_x, float avg_y);
static void AppSensorJoyCalRequestSave(void);
static uint8_t AppSensorJoyLiveUpdate(TMAGJoy_Dir dir, ULONG input_mask, uint8_t enabled);
static void AppSensorJoyEnsureAbsDeadzoneConfigured(void);
static VOID AppSensorJoyInputUpdate(uint8_t enabled);
static void AppSensorJoySeedNeutralArm(void);
static ULONG AppSensorJoyDirMask(TMAGJoy_Dir dir);
static VOID AppSensorTmagMarkRuntimeFault(LONG error_code);
static VOID AppSensorResetRecoveryState(app_sensor_fsm_t *dev);
static uint8_t AppSensorRetryDue(ULONG now_ticks, ULONG deadline_ticks);
static VOID AppSensorBusRecoverPulseDelay(void);
static uint8_t AppSensorBusRecover(LONG *error_out);
static uint8_t AppSensorProbeWithRecovery(uint8_t (*probe_fn)(LONG *), LONG *error_out);
static uint8_t AppSensorBusSanityCheck(LONG *error_out);
static VOID AppSensorPmicPolicyRefresh(void);
static VOID AppSensorPmicUpdateBatteryHealth(ULONG vbat_mV, ULONG soc_pct);
static VOID AppSensorPmicRuntimeReset(void);
static VOID AppSensorPmicRecordTransportError(LONG error_code);
static VOID AppSensorPmicRecordFaultMask(uint8_t fault_mask);
static uint8_t AppSensorPmicForceIsofetOff(LONG *error_out);
static uint8_t AppSensorPmicGuardApply(ULONG vbat_mV, LONG *error_out);
static uint8_t AppSensorProbePmic(LONG *error_out);
static uint8_t AppSensorProbeTmag(LONG *error_out);
static uint8_t AppSensorPollPmic(LONG *error_out);
static uint8_t AppSensorPollTmag(LONG *error_out);
static uint8_t AppSensorLisResolveDevice(stmdev_ctx_t *driver_ctx, app_sensor_lis_ctx_t *lis_ctx, uint8_t *whoami_out);
static uint8_t AppSensorLisOdrIsValid(uint8_t odr);
static uint8_t AppSensorLisOdrSupportsBw(uint8_t odr);
static uint8_t AppSensorLisResolveOdrKnob(ULONG knob_value, uint8_t fallback_odr);
static uint8_t AppSensorLisResolveBwKnob(ULONG knob_value, uint8_t fallback_bw);
static uint8_t AppSensorLisResolveFsKnob(ULONG knob_value, uint8_t fallback_fs);
static uint8_t AppSensorLisApplyProfile(const stmdev_ctx_t *driver_ctx, app_sensor_lis_profile_t profile, LONG *error_out);
static uint8_t AppSensorLisApplyStepConfig(const stmdev_ctx_t *driver_ctx, ULONG step_enable, LONG *error_out);
static VOID AppSensorLisMarkRuntimeFault(LONG error_code);
static VOID AppSensorLisApplyRequestedProfileNow(void);
static VOID AppSensorLisResetStepCounterNow(void);
static int32_t AppSensorLisRead(void *ctx, uint8_t reg, uint8_t *data, uint16_t len);
static int32_t AppSensorLisWrite(void *ctx, uint8_t reg, const uint8_t *data, uint16_t len);
static uint8_t AppSensorProbeLis(LONG *error_out);
static uint8_t AppSensorPollLis(LONG *error_out);
static uint8_t AppSensorPollLisStreamFast(LONG *error_out);
static VOID AppSensorLisRefreshStepStatusNow(void);
static VOID AppSensorDeviceInit(app_sensor_fsm_t *dev, uint8_t (*probe_fn)(LONG *));
static VOID AppSensorDevicePoll(app_sensor_fsm_t *dev, uint8_t (*poll_fn)(LONG *), uint8_t (*probe_fn)(LONG *));
static void AppDisplaySetTranslator(uint8_t enabled);
static void AppDisplaySetCs(uint8_t active);
static HAL_StatusTypeDef AppDisplayEnsureReady(void);
static uint8_t AppModeToThMode(app_mode_t mode_token, th_mode_t *mode_out);
static void AppDisplayPrepareBootstrapFrame(void);
static void AppDebugDisplayStackSample(void);
static UINT AppRendererLock(void);
static VOID AppRendererUnlock(void);
static HAL_StatusTypeDef AppDisplayPresent(void);
static VOID AppAudioFillFrames(ULONG frame_offset, ULONG frame_count);
static UINT AppAudioStartTone(void);
static UINT AppAudioStop(void);
static VOID AppAudioProcessDmaEvents(void);
static uint8_t AppInputResolveSource(uint16_t gpio_pin, GPIO_TypeDef **port_out, ULONG *source_out);
static uint8_t AppInputTranslateRaw(const app_input_raw_evt_t *raw_evt, app_input_action_evt_t *action_evt);
static VOID AppInputRouteAction(const app_input_action_evt_t *action_evt);
static uint8_t AppInputSourceValid(ULONG source);
static ULONG AppInputSourceBit(ULONG source);
static uint8_t AppInputSourceIsPhysicalButton(ULONG source);
static uint8_t AppInputSourceIsJoystick(ULONG source);
static uint8_t AppInputReadPhysicalLevel(ULONG source, ULONG *level_out);
static VOID AppInputRefreshPhysicalIdleLevels(void);
static uint8_t AppInputPhysicalPressedFromLevel(ULONG source, ULONG level, ULONG *pressed_out);
static uint8_t AppInputSourceRepeatEnabled(ULONG source);
static uint8_t AppInputSourceLongEnabled(ULONG source);
static ULONG AppInputRepeatPeriodTicks(ULONG source, ULONG held_ticks);
static UINT AppInputPushActionToQueue(TX_QUEUE *queue, const app_input_action_evt_t *evt, ULONG *drop_oldest_counter);
static VOID AppInputDropQueuedRepeatsForSource(TX_QUEUE *queue, ULONG source);
static uint8_t AppInputTickDue(ULONG now_tick, ULONG deadline_tick);
static uint8_t AppInputShouldDebounce(ULONG source, ULONG edge, ULONG tick);
static VOID AppInputProcessRepeat(ULONG now_tick);
static UINT AppInputPostSystemEvent(app_sys_event_type_t event_type, ULONG arg0, ULONG arg1, ULONG arg2);
static VOID AppInputPostRawEvent(ULONG source, ULONG edge, ULONG level, ULONG tick);
static UINT AppUiRouterJoyCalStart(void);
static UINT AppUiRouterJoyCalSave(void);
static UINT AppUiRouterJoyCalCancel(void);
static ULONG AppUiMapInputEdgeToRouter(ULONG edge);
static ULONG AppUiMapInputEventToRouter(ULONG event);
static VOID AppUiBuildRouterState(ui_router_state_t *state_out);
static VOID AppUiSyncDebugState(void);
static void AppUiHandleTick(void);
static void AppUiEnterPage(app_ui_page_t page);
static uint8_t AppUiHandleAction(const app_input_action_evt_t *evt);
static uint8_t AppGameHandleAction(const app_input_action_evt_t *evt);

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

  ret = AppSetModeFlag(APP_MODE_STOP);
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

  ret = AppPowerFlagsUpdate(APP_POWER_FLAG_RUNNING, (APP_POWER_FLAGS_ALL & ~APP_POWER_FLAG_RUNNING));
  if (ret != TX_SUCCESS)
  {
    return ret;
  }
  g_power_pending_ack_mask = 0UL;
  g_power_quiesce_wait_active = 0UL;
  g_power_quiesce_wait_elapsed_ticks = 0UL;
  g_sensor_bus_fault = 0UL;
  g_sensor_mode_token = (ULONG)APP_MODE_STOP;
  (void)memset(&g_sensor_pmic_live, 0, sizeof(g_sensor_pmic_live));
  g_sensor_pmic_live.guard_enabled = (KNOB_SENSOR_PMIC_GUARD_ENABLE != 0) ? 1UL : 0UL;
  g_sensor_pmic_live.cutoff_mv = (ULONG)KNOB_SENSOR_PMIC_CUTOFF_MV;
  g_sensor_pmic_live.cutoff_hys_mv = (ULONG)KNOB_SENSOR_PMIC_CUTOFF_HYS_MV;
  g_sensor_pmic_live.cutoff_confirm_samples = (ULONG)KNOB_SENSOR_PMIC_CUTOFF_CONFIRM_SAMPLES;
  g_storage_flash_ready = 0UL;
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
  g_storage_joycfg_valid = 0UL;
  g_storage_joycfg_last_error = APP_STORAGE_ERR_NONE;
  g_storage_joycfg_load_ok_count = 0UL;
  g_storage_joycfg_load_fail_count = 0UL;
  g_storage_joycfg_save_ok_count = 0UL;
  g_storage_joycfg_save_fail_count = 0UL;
  g_storage_joycfg_load_seq = 0UL;
  g_storage_joycfg_save_seq = 0UL;
  g_storage_joycfg_cal.cx = 0.0f;
  g_storage_joycfg_cal.cy = 0.0f;
  g_storage_joycfg_cal.sx = 1.0f;
  g_storage_joycfg_cal.sy = 1.0f;
  g_storage_joycfg_cal.rot_deg = 0.0f;
  g_storage_joycfg_cal.invert_x = 0U;
  g_storage_joycfg_cal.invert_y = 0U;
  g_storage_at25_dbg.last_op = 0U;
  g_storage_at25_dbg.cmd_status = 0U;
  g_storage_at25_dbg.io_status = 0U;
  g_storage_at25_dbg.reserved0 = 0U;
  g_storage_at25_dbg.addr = 0UL;
  g_storage_at25_dbg.nbytes = 0UL;
  g_storage_at25_dbg.hal_error = 0UL;
  g_storage_at25_dbg.seq = 0UL;
  g_audio_dma_events = 0UL;
  g_audio_state = APP_AUDIO_STATE_STOPPED;
  g_audio_start_count = 0UL;
  g_audio_stop_count = 0UL;
  g_audio_restart_count = 0UL;
  g_audio_underflow_count = 0UL;
  g_audio_half_irq_count = 0UL;
  g_audio_full_irq_count = 0UL;
  g_audio_error_irq_count = 0UL;
  g_audio_last_error = 0L;
  g_audio_phase_accum = 0UL;
  g_audio_phase_step = 0UL;
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
  (void)memset(g_input_physical_idle_level, 0, sizeof(g_input_physical_idle_level));
  g_input_physical_idle_valid_mask = 0UL;
  g_power_input_activity_count = 0UL;
  g_power_last_input_tick = 0UL;
  g_power_menu_event_count = 0UL;
  g_game_exit_to_static_pending = 0UL;
  g_power_perf_profile_current = APP_POWER_PERF_PROFILE_NORM;
  g_power_perf_profile_target = APP_POWER_PERF_PROFILE_NORM;
  g_power_perf_last_switch_tick = 0UL;
  g_power_perf_hint_seq = 0UL;
  g_power_perf_hint_post_count = 0UL;
  g_power_perf_hint_drop_count = 0UL;
  g_power_perf_hint_rx_count = 0UL;
  g_power_perf_hint_inflight = 0UL;
  g_power_perf_last_present_ticks = 0UL;
  g_power_perf_miss_streak = 0UL;
  g_power_perf_headroom_streak = 0UL;
  g_power_perf_up_count = 0UL;
  g_power_perf_down_count = 0UL;
  g_power_perf_dwell_block_count = 0UL;
  g_ui_page_current = (ULONG)APP_UI_PAGE_HOME;
  g_ui_page_dirty = 1UL;
  g_ui_mode_flags_last = 0UL;
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

  /* Keep debug-facing API entry points linked for debug.gdb scripted calls. */
  {
    UINT (*volatile keep_mode_set)(app_mode_t) = App_SysEvent_ModeSet;
    UINT (*volatile keep_quiesce)(void) = App_SysEvent_QuiesceReq;
    UINT (*volatile keep_resume)(void) = App_SysEvent_ResumeReq;
    UINT (*volatile keep_quiesce_ack)(ULONG) = App_SysEvent_QuiesceAck;
    UINT (*volatile keep_mode_flags_get)(ULONG *) = App_ModeFlags_Get;
    UINT (*volatile keep_power_flags_get)(ULONG *) = App_PowerFlags_Get;
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
    const ULONG *volatile keep_storage_settings_addr_dbg = &g_storage_settings_addr_dbg;
    UINT (*volatile keep_storage_joycfg_load)(void) = App_StorageReq_JoyCfgLoad;
    UINT (*volatile keep_storage_joycfg_save)(void) = App_StorageReq_JoyCfgSave;
    UINT (*volatile keep_audio_start_tone)(void) = App_AudioReq_StartTone;
    UINT (*volatile keep_audio_stop)(void) = App_AudioReq_Stop;
    UINT (*volatile keep_sensor_joycal_start)(void) = App_SensorReq_JoyCalStart;
    UINT (*volatile keep_sensor_joycal_save)(void) = App_SensorReq_JoyCalSave;
    UINT (*volatile keep_sensor_joycal_cancel)(void) = App_SensorReq_JoyCalCancel;
    (void)keep_mode_set;
    (void)keep_quiesce;
    (void)keep_resume;
    (void)keep_quiesce_ack;
    (void)keep_mode_flags_get;
    (void)keep_power_flags_get;
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
    (void)keep_storage_settings_addr_dbg;
    (void)keep_storage_joycfg_load;
    (void)keep_storage_joycfg_save;
    (void)keep_audio_start_tone;
    (void)keep_audio_stop;
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
_Static_assert((KNOB_STORAGE_SMOKE_ADDR < APP_STORAGE_FLASH_SIZE_BYTES), "Storage smoke address must be within flash range");
_Static_assert((KNOB_STORAGE_SMOKE_ADDR % APP_STORAGE_SMOKE_SECTOR_SIZE) == 0U, "Storage smoke address must be 4KiB aligned");
_Static_assert((KNOB_STORAGE_SMOKE_LEN > 0), "Storage smoke length must be > 0");
_Static_assert((KNOB_STORAGE_SMOKE_LEN <= APP_STORAGE_SMOKE_MAX_LEN), "Storage smoke length must be <= 256 bytes");
_Static_assert((KNOB_STORAGE_SMOKE_ADDR + KNOB_STORAGE_SMOKE_LEN) <= APP_STORAGE_FLASH_SIZE_BYTES, "Storage smoke range must fit in flash");
_Static_assert((KNOB_STORAGE_FAT_BASE_ADDR < APP_STORAGE_FLASH_SIZE_BYTES), "Storage FAT base must be within flash range");
_Static_assert((KNOB_STORAGE_FAT_BASE_ADDR % APP_STORAGE_FAT_BLOCK_SIZE) == 0U, "Storage FAT base must be 4KiB aligned");
_Static_assert((KNOB_STORAGE_FAT_SIZE_BYTES > 0), "Storage FAT size must be > 0");
_Static_assert((KNOB_STORAGE_FAT_SIZE_BYTES % APP_STORAGE_FAT_BLOCK_SIZE) == 0U, "Storage FAT size must be 4KiB aligned");
_Static_assert((KNOB_STORAGE_FAT_SIZE_BYTES % APP_STORAGE_FAT_BYTES_PER_SECTOR) == 0U, "Storage FAT size must be sector aligned");
_Static_assert((KNOB_STORAGE_FAT_BASE_ADDR + KNOB_STORAGE_FAT_SIZE_BYTES) <= APP_STORAGE_FLASH_SIZE_BYTES, "Storage FAT region must fit in flash");
_Static_assert((KNOB_STORAGE_FILEX_CACHE_BYTES >= APP_STORAGE_FAT_BYTES_PER_SECTOR), "Storage FileX cache must be >= sector size");
_Static_assert((KNOB_STORAGE_FILEX_CACHE_BYTES % sizeof(ULONG)) == 0U, "Storage FileX cache must align to ULONG");
_Static_assert((KNOB_STORAGE_FILEX_SECTORS_PER_CLUSTER > 0), "Storage FileX sectors/cluster must be > 0");
_Static_assert((KNOB_STORAGE_FILEX_DIR_ENTRIES > 0), "Storage FileX dir entries must be > 0");
_Static_assert((((KNOB_STORAGE_SMOKE_ADDR + KNOB_STORAGE_SMOKE_LEN) <= KNOB_STORAGE_FAT_BASE_ADDR) || (KNOB_STORAGE_SMOKE_ADDR >= (KNOB_STORAGE_FAT_BASE_ADDR + KNOB_STORAGE_FAT_SIZE_BYTES))), "Storage smoke region must not overlap FAT region");
_Static_assert((KNOB_STORAGE_SETTINGS_ADDR < APP_STORAGE_FLASH_SIZE_BYTES), "Storage settings address must be within flash range");
_Static_assert((KNOB_STORAGE_SETTINGS_ADDR % APP_STORAGE_SETTINGS_SECTOR_SIZE) == 0U, "Storage settings address must be 4KiB aligned");
_Static_assert((KNOB_STORAGE_SETTINGS_ADDR + sizeof(app_storage_joycfg_blob_t)) <= APP_STORAGE_FLASH_SIZE_BYTES, "Storage settings blob must fit in flash");
_Static_assert((((KNOB_STORAGE_SETTINGS_ADDR + sizeof(app_storage_joycfg_blob_t)) <= KNOB_STORAGE_FAT_BASE_ADDR) || (KNOB_STORAGE_SETTINGS_ADDR >= (KNOB_STORAGE_FAT_BASE_ADDR + KNOB_STORAGE_FAT_SIZE_BYTES))), "Storage settings region must not overlap FAT region");
_Static_assert((((KNOB_STORAGE_SETTINGS_ADDR + sizeof(app_storage_joycfg_blob_t)) <= KNOB_STORAGE_SMOKE_ADDR) || (KNOB_STORAGE_SETTINGS_ADDR >= (KNOB_STORAGE_SMOKE_ADDR + KNOB_STORAGE_SMOKE_LEN))), "Storage settings region must not overlap smoke region");
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
  uint16_t box_w = (RENDER_WIDTH > 8U) ? (uint16_t)(RENDER_WIDTH - 8U) : RENDER_WIDTH;
  uint16_t box_h = (RENDER_HEIGHT > 8U) ? (uint16_t)(RENDER_HEIGHT - 8U) : RENDER_HEIGHT;

  Render_Init();
  renderClear(RENDER_COLOR_WHITE);
  Render_SetModeIndicator(TH_MODE_STATIC);
  renderDrawRectOutline(4U, 4U, box_w, box_h, RENDER_LAYER_UI, RENDER_COLOR_BLACK, 2U);
  renderDrawTextScaled(16U, 20U, "PEEPSHOW", RENDER_LAYER_UI, RENDER_COLOR_BLACK, 2U);
  renderDrawText(18U, 52U, "DISPLAY PATH OK", RENDER_LAYER_UI, RENDER_COLOR_BLACK);
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

static HAL_StatusTypeDef AppDisplayPresent(void)
{
  uint16_t dirty_count = 0U;
  bool full_flush = false;
  const uint8_t *framebuffer = NULL;
  HAL_StatusTypeDef hal_status = AppDisplayEnsureReady();

  AppDebugDisplayStackSample();

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

static VOID AppAudioFillFrames(ULONG frame_offset, ULONG frame_count)
{
  ULONG frame_index;
  ULONG sample_index = (frame_offset * APP_AUDIO_CHANNEL_COUNT);
  int16_t high = (int16_t)KNOB_AUDIO_TEST_TONE_AMPLITUDE;
  int16_t low = (int16_t)(-(int32_t)KNOB_AUDIO_TEST_TONE_AMPLITUDE);

  for (frame_index = 0UL; frame_index < frame_count; frame_index++)
  {
    int16_t sample = ((g_audio_phase_accum & 0x80000000UL) != 0UL) ? high : low;

    g_audio_phase_accum += g_audio_phase_step;
    g_audio_dma_buffer[sample_index] = sample;
    g_audio_dma_buffer[sample_index + 1UL] = sample;
    sample_index += APP_AUDIO_CHANNEL_COUNT;
  }
}

static UINT AppAudioStartTone(void)
{
  HAL_StatusTypeDef hal_status;

  if (g_audio_state == APP_AUDIO_STATE_ACTIVE)
  {
    return TX_SUCCESS;
  }

  g_audio_phase_accum = 0UL;
  g_audio_phase_step = (uint32_t)(((uint64_t)KNOB_AUDIO_TEST_TONE_HZ << 32) / (uint64_t)KNOB_AUDIO_SAMPLE_RATE);
  g_audio_dma_events = 0UL;
  AppAudioFillFrames(0UL, KNOB_AUDIO_DMA_FRAMES);
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
    return TX_NOT_DONE;
  }

  HAL_GPIO_WritePin(SD_MODE_GPIO_Port, SD_MODE_Pin, GPIO_PIN_SET);
  g_audio_state = APP_AUDIO_STATE_ACTIVE;
  g_audio_start_count++;
  return TX_SUCCESS;
}

static UINT AppAudioStop(void)
{
  HAL_StatusTypeDef hal_status;

  /* Gate ISR callbacks before stopping DMA to prevent re-arm races. */
  g_audio_state = APP_AUDIO_STATE_STOPPED;
  g_audio_dma_events = 0UL;
  /* Mute amp first to avoid audible tail/chirp while stopping DMA. */
  HAL_GPIO_WritePin(SD_MODE_GPIO_Port, SD_MODE_Pin, GPIO_PIN_RESET);
  hal_status = HAL_SAI_DMAStop(&hsai_BlockA1);
  if (hal_status == HAL_BUSY)
  {
    hal_status = HAL_OK;
  }
  g_audio_last_error = (LONG)hal_status;
  g_audio_stop_count++;
  return (hal_status == HAL_OK) ? TX_SUCCESS : TX_NOT_DONE;
}

static VOID AppAudioProcessDmaEvents(void)
{
  ULONG events = g_audio_dma_events;

  if (g_audio_state != APP_AUDIO_STATE_ACTIVE)
  {
    g_audio_dma_events = 0UL;
    return;
  }

  if ((events & APP_AUDIO_DMA_ERROR_FLAG) != 0UL)
  {
    g_audio_underflow_count++;
    g_audio_dma_events &= ~APP_AUDIO_DMA_ERROR_FLAG;
    (void)AppAudioStop();
    return;
  }

  if ((events & (APP_AUDIO_DMA_HALF_FLAG | APP_AUDIO_DMA_FULL_FLAG)) != 0UL)
  {
    g_audio_dma_events &= ~(APP_AUDIO_DMA_HALF_FLAG | APP_AUDIO_DMA_FULL_FLAG);
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

static VOID AppSensorMarkAllSuspended(void)
{
  g_sensor_lis_stream_enabled = 0UL;

  if (g_sensor_pmic.state != (ULONG)APP_SENSOR_STATE_FAULT)
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
  g_sensor_pmic_live.charger_state = 0UL;
  g_sensor_pmic_live.battery_uv = 0UL;
  g_sensor_pmic_live.battery_ov = 0UL;
  g_sensor_pmic_live.vbat_mV = 0UL;
  g_sensor_pmic_live.vbat_raw = 0UL;
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
  uint16_t vbat_mV = 0U;
  uint16_t vbat_raw = 0U;
  uint8_t soc_percent = 0U;
  uint8_t soc_raw = 0U;
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

  g_sensor_pmic_live.vbat_mV = (ULONG)vbat_mV;
  g_sensor_pmic_live.vbat_raw = (ULONG)vbat_raw;
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
    TMAGJoy_InitOnce();
    g_sensor_joy = UI_GetJoy();
    g_sensor_joy_input_gate_valid = 0UL;
    g_sensor_joy_input_neutral_armed = 0UL;
    g_sensor_joy_input_neutral_stable_count = 0UL;
    g_sensor_joy_live_read_fail_streak = 0UL;
    AppSensorJoyCalApplyLoadedIfReady();
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

static void AppSensorJoyCalApplyLoadedIfReady(void)
{
  if (g_storage_joycfg_load_seq != g_sensor_joycfg_seen_load_seq)
  {
    g_sensor_joycfg_seen_load_seq = g_storage_joycfg_load_seq;
    g_sensor_joy_cal_status.load_ok_count = g_storage_joycfg_load_ok_count;
    g_sensor_joy_cal_status.load_fail_count = g_storage_joycfg_load_fail_count;

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
  UINT status;

  if (g_sensor_joy_cal_stage != (ULONG)APP_JOY_CAL_STAGE_DONE)
  {
    AppSensorJoyCalCancel();
    return;
  }

  if (g_sensor_joy == TX_NULL)
  {
    AppSensorJoyCalRestoreSnapshot();
    g_sensor_joy_cal_status.last_error = -502L;
    g_sensor_joy_cal_status.save_fail_count++;
    return;
  }

  TMAGJoy_GetCal(g_sensor_joy, &g_storage_joycfg_cal);
  if (AppSensorJoyCalSane(&g_storage_joycfg_cal) == 0U)
  {
    AppSensorJoyCalRestoreSnapshot();
    g_sensor_joy_cal_status.last_error = APP_STORAGE_ERR_JOYCFG_INVALID;
    g_sensor_joy_cal_status.save_fail_count++;
    return;
  }
  g_storage_joycfg_valid = 1UL;

  g_sensor_joy_cal_status.save_pending = 1UL;
  g_sensor_joy_cal_status.last_error = 0L;
  status = AppStorageReqPost(APP_STORAGE_REQ_JOYCFG_SAVE, 0UL);
  if (status != TX_SUCCESS)
  {
    g_sensor_joy_cal_status.save_pending = 0UL;
    g_sensor_joy_cal_status.save_fail_count++;
    g_sensor_joy_cal_status.last_error = -503L;
  }
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
      AppSensorMarkAllSuspended();
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

UINT App_AudioReq_StartTone(void)
{
  return AppAudioCmdPost(APP_AUDIO_CMD_START_TONE, 0UL);
}

UINT App_AudioReq_Stop(void)
{
  return AppAudioCmdPost(APP_AUDIO_CMD_STOP, 0UL);
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

#include "threads/app_thread_entry_display.c"

#include "threads/app_thread_entry_storage.c"

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

  boot_ok = (uint8_t)AT25_BootInit(&hospi1, &flash_cfg, &jedec_id);
  AppStorageCaptureDebug();
  if (boot_ok == 0U)
  {
    g_storage_flash_ready = 0UL;
    g_storage_last_error = APP_STORAGE_ERR_BOOTINIT;
    return TX_NOT_DONE;
  }

  g_storage_flash_ready = 1UL;
  g_storage_last_jedec_id = (ULONG)jedec_id;
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

static ULONG AppStorageFatTotalSectors(void)
{
  return (ULONG)(KNOB_STORAGE_FAT_SIZE_BYTES / APP_STORAGE_FAT_BYTES_PER_SECTOR);
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
    g_storage_filex_last_status = FX_SUCCESS;
    return TX_SUCCESS;
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
  return TX_SUCCESS;
}

static UINT AppStorageRunFileXFormat(void)
{
  UINT fx_status;
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

  fx_status = fx_media_format(&g_storage_fx_media,
                              fx_stm32_levelx_nor_driver,
                              (VOID *)NOR_CUSTOM_DRIVER_ID,
                              g_storage_filex_cache,
                              sizeof(g_storage_filex_cache),
                              (CHAR *)APP_STORAGE_FILEX_VOLUME_NAME,
                              1U,
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

static UINT AppStorageRunJoyCfgLoad(void)
{
  app_storage_joycfg_blob_t blob = {0};
  HAL_StatusTypeDef hal_status;
  uint32_t crc = 0UL;

  g_storage_last_op = APP_STORAGE_OP_JOYCFG_LOAD;
  g_storage_last_error = APP_STORAGE_ERR_NONE;
  g_storage_joycfg_last_error = APP_STORAGE_ERR_NONE;

  if (((uint32_t)KNOB_STORAGE_SETTINGS_ADDR % APP_STORAGE_SETTINGS_SECTOR_SIZE) != 0UL)
  {
    g_storage_last_error = APP_STORAGE_ERR_ALIGN;
    g_storage_joycfg_last_error = APP_STORAGE_ERR_ALIGN;
    g_storage_joycfg_valid = 0UL;
    g_storage_joycfg_load_fail_count++;
    g_storage_joycfg_load_seq++;
    return TX_NOT_DONE;
  }

  if (((uint32_t)KNOB_STORAGE_SETTINGS_ADDR + (uint32_t)sizeof(blob)) > APP_STORAGE_FLASH_SIZE_BYTES)
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

  hal_status = AT25_Read(&hospi1, (uint32_t)KNOB_STORAGE_SETTINGS_ADDR, (uint8_t *)&blob, (uint32_t)sizeof(blob));
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

  if ((blob.magic == 0xFFFFFFFFUL) &&
      (blob.version == 0xFFFFFFFFUL) &&
      (blob.payload_size == 0xFFFFFFFFUL) &&
      (blob.crc32 == 0xFFFFFFFFUL))
  {
    g_storage_joycfg_valid = 0UL;
    g_storage_last_error = APP_STORAGE_ERR_NONE;
    g_storage_joycfg_last_error = APP_STORAGE_ERR_NONE;
    g_storage_joycfg_load_ok_count++;
    g_storage_joycfg_load_seq++;
    return TX_SUCCESS;
  }

  if ((blob.magic != APP_STORAGE_JOYCFG_MAGIC) ||
      (blob.version != APP_STORAGE_JOYCFG_VERSION) ||
      (blob.payload_size != (uint32_t)sizeof(TMAGJoy_Cal)))
  {
    g_storage_last_error = APP_STORAGE_ERR_JOYCFG_INVALID;
    g_storage_joycfg_last_error = APP_STORAGE_ERR_JOYCFG_INVALID;
    g_storage_joycfg_valid = 0UL;
    g_storage_joycfg_load_fail_count++;
    g_storage_joycfg_load_seq++;
    return TX_NOT_DONE;
  }

  crc = AppStorageCrc32((const uint8_t *)&blob.cal, blob.payload_size);
  if (crc != blob.crc32)
  {
    g_storage_last_error = APP_STORAGE_ERR_JOYCFG_INVALID;
    g_storage_joycfg_last_error = APP_STORAGE_ERR_JOYCFG_INVALID;
    g_storage_joycfg_valid = 0UL;
    g_storage_joycfg_load_fail_count++;
    g_storage_joycfg_load_seq++;
    return TX_NOT_DONE;
  }

  if (AppSensorJoyCalSane(&blob.cal) == 0U)
  {
    g_storage_last_error = APP_STORAGE_ERR_JOYCFG_INVALID;
    g_storage_joycfg_last_error = APP_STORAGE_ERR_JOYCFG_INVALID;
    g_storage_joycfg_valid = 0UL;
    g_storage_joycfg_load_fail_count++;
    g_storage_joycfg_load_seq++;
    return TX_NOT_DONE;
  }

  g_storage_joycfg_cal = blob.cal;
  g_storage_joycfg_valid = 1UL;
  g_storage_last_error = APP_STORAGE_ERR_NONE;
  g_storage_joycfg_last_error = APP_STORAGE_ERR_NONE;
  g_storage_joycfg_load_ok_count++;
  g_storage_joycfg_load_seq++;
  return TX_SUCCESS;
}

static UINT AppStorageRunJoyCfgSave(void)
{
  app_storage_joycfg_blob_t blob = {0};
  app_storage_joycfg_blob_t verify_blob = {0};
  HAL_StatusTypeDef hal_status;

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
  blob.version = APP_STORAGE_JOYCFG_VERSION;
  blob.payload_size = (uint32_t)sizeof(TMAGJoy_Cal);
  blob.cal = g_storage_joycfg_cal;
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

static UINT AppUiRouterJoyCalStart(void)
{
  return App_SensorReq_JoyCalStart();
}

static UINT AppUiRouterJoyCalSave(void)
{
  return App_SensorReq_JoyCalSave();
}

static UINT AppUiRouterJoyCalCancel(void)
{
  return App_SensorReq_JoyCalCancel();
}

static ULONG AppUiMapInputEdgeToRouter(ULONG edge)
{
  switch (edge)
  {
    case APP_INPUT_EDGE_LOW:
      return UI_EVENT_PRESS;
    case APP_INPUT_EDGE_HIGH:
      return UI_EVENT_RELEASE;
    case APP_INPUT_EDGE_REPEAT:
      return UI_EVENT_REPEAT;
    default:
      return 0UL;
  }
}

static ULONG AppUiMapInputEventToRouter(ULONG event)
{
  switch ((app_input_event_t)event)
  {
    case APP_INPUT_EVENT_PRESS:
      return UI_EVENT_PRESS;
    case APP_INPUT_EVENT_RELEASE:
      return UI_EVENT_RELEASE;
    case APP_INPUT_EVENT_REPEAT:
      return UI_EVENT_REPEAT;
    case APP_INPUT_EVENT_LONG:
      return UI_EVENT_LONG;
    default:
      return 0UL;
  }
}

static VOID AppUiBuildRouterState(ui_router_state_t *state_out)
{
  if (state_out == TX_NULL)
  {
    return;
  }

  (void)memset(state_out, 0, sizeof(*state_out));
  state_out->mode_flags = (g_eg_mode.tx_event_flags_group_current & APP_MODE_FLAGS_ALL);
  state_out->last_input_source = g_input_last_source;
  state_out->last_input_action = g_input_action_last;
  state_out->last_input_edge = AppUiMapInputEdgeToRouter(g_input_last_edge);
  state_out->joy_cal_status.stage = g_sensor_joy_cal_status.stage;
  state_out->joy_cal_status.progress = g_sensor_joy_cal_status.progress;
  state_out->joy_cal_status.last_error = g_sensor_joy_cal_status.last_error;
  state_out->joy_cal_status.save_pending = g_sensor_joy_cal_status.save_pending;
  state_out->joy_cal_status.save_ok_count = g_sensor_joy_cal_status.save_ok_count;
  state_out->joy_cal_status.save_fail_count = g_sensor_joy_cal_status.save_fail_count;
  state_out->joy_cal_status.load_ok_count = g_sensor_joy_cal_status.load_ok_count;
  state_out->joy_cal_status.load_fail_count = g_sensor_joy_cal_status.load_fail_count;
  state_out->joy_cal_active = g_sensor_joy_cal_active;
  state_out->joy_cal_quality.valid = g_sensor_joy_cal_quality.valid;
  state_out->joy_cal_quality.quality_ok = g_sensor_joy_cal_quality.quality_ok;
  state_out->joy_cal_quality.span_ratio = g_sensor_joy_cal_quality.span_ratio;
  state_out->joy_cal_quality.axis_error = g_sensor_joy_cal_quality.axis_error;
  state_out->joy_cal_quality.dir_norm_min = g_sensor_joy_cal_quality.dir_norm_min;
  state_out->joy_cal_quality.dir_norm_max = g_sensor_joy_cal_quality.dir_norm_max;
  state_out->joy_live.dir = g_sensor_joy_live_status.dir;
  state_out->joy_live.input_mask = g_sensor_joy_live_status.input_mask;
  state_out->joy_live.deadzone_enabled = g_sensor_joy_live_status.deadzone_enabled;
  state_out->joy_live.invert_x = g_sensor_joy_live_status.invert_x;
  state_out->joy_live.invert_y = g_sensor_joy_live_status.invert_y;
  state_out->joy_live.nx = g_sensor_joy_live_status.nx;
  state_out->joy_live.ny = g_sensor_joy_live_status.ny;
  state_out->joy_live.r_abs_mT = g_sensor_joy_live_status.r_abs_mT;
  state_out->joy_live.center_x_mT = g_sensor_joy_live_status.center_x_mT;
  state_out->joy_live.center_y_mT = g_sensor_joy_live_status.center_y_mT;
  state_out->joy_live.span_x_mT = g_sensor_joy_live_status.span_x_mT;
  state_out->joy_live.span_y_mT = g_sensor_joy_live_status.span_y_mT;
  state_out->joy_live.rotation_deg = g_sensor_joy_live_status.rotation_deg;
  state_out->joy_live.threshold_x_mT = g_sensor_joy_live_status.threshold_x_mT;
  state_out->joy_live.threshold_y_mT = g_sensor_joy_live_status.threshold_y_mT;
  state_out->joy_live.deadzone_mT = g_sensor_joy_live_status.deadzone_mT;
  state_out->pmic_live.fsm_state = g_sensor_pmic.state;
  state_out->pmic_live.fsm_fail_count = g_sensor_pmic.fail_count;
  state_out->pmic_live.fsm_recovery_attempts = g_sensor_pmic.recovery_attempts;
  state_out->pmic_live.fsm_last_error = g_sensor_pmic.last_error;
  state_out->pmic_live.sample_count = g_sensor_pmic_live.sample_count;
  state_out->pmic_live.fail_count = g_sensor_pmic_live.fail_count;
  state_out->pmic_live.last_sample_tick = g_sensor_pmic_live.last_sample_tick;
  state_out->pmic_live.last_error = g_sensor_pmic_live.last_error;
  state_out->pmic_live.last_transport_error = g_sensor_pmic_live.last_transport_error;
  state_out->pmic_live.charging_enabled_cfg = g_sensor_pmic_live.charging_enabled_cfg;
  state_out->pmic_live.charging_active = g_sensor_pmic_live.charging_active;
  state_out->pmic_live.battery_soc_percent = g_sensor_pmic_live.battery_soc_percent;
  state_out->pmic_live.battery_soc_raw = g_sensor_pmic_live.battery_soc_raw;
  state_out->pmic_live.battery_health_state = g_sensor_pmic_live.battery_health_state;
  state_out->pmic_live.battery_health_reason = g_sensor_pmic_live.battery_health_reason;
  state_out->pmic_live.charger_state = g_sensor_pmic_live.charger_state;
  state_out->pmic_live.battery_uv = g_sensor_pmic_live.battery_uv;
  state_out->pmic_live.battery_ov = g_sensor_pmic_live.battery_ov;
  state_out->pmic_live.vbat_mV = g_sensor_pmic_live.vbat_mV;
  state_out->pmic_live.vbat_raw = g_sensor_pmic_live.vbat_raw;
  state_out->pmic_live.fault_raw = g_sensor_pmic_live.fault_raw;
  state_out->pmic_live.status2_raw = g_sensor_pmic_live.status2_raw;
  state_out->pmic_live.pgood_raw = g_sensor_pmic_live.pgood_raw;
  state_out->lis_live.fsm_state = g_sensor_lis.state;
  state_out->lis_live.fsm_fail_count = g_sensor_lis.fail_count;
  state_out->lis_live.fsm_recovery_attempts = g_sensor_lis.recovery_attempts;
  state_out->lis_live.fsm_last_error = g_sensor_lis.last_error;
  state_out->lis_live.stream_enabled = g_sensor_lis_stream_enabled;
  state_out->lis_live.profile_requested = (ULONG)g_sensor_lis_profile_requested;
  state_out->lis_live.profile_applied = (ULONG)g_sensor_lis_profile_applied;
  state_out->lis_live.addr = g_sensor_lis_live.addr;
  state_out->lis_live.whoami = g_sensor_lis_live.whoami;
  state_out->lis_live.status = g_sensor_lis_live.status;
  state_out->lis_live.sample_count = g_sensor_lis_live.sample_count;
  state_out->lis_live.fail_count = g_sensor_lis_live.fail_count;
  state_out->lis_live.last_sample_tick = g_sensor_lis_live.last_sample_tick;
  state_out->lis_live.last_error = g_sensor_lis_live.last_error;
  state_out->lis_live.x_raw = (LONG)g_sensor_lis_live.x_raw;
  state_out->lis_live.y_raw = (LONG)g_sensor_lis_live.y_raw;
  state_out->lis_live.z_raw = (LONG)g_sensor_lis_live.z_raw;
  state_out->lis_live.step_enabled = g_sensor_lis_live.step_enabled;
  state_out->lis_live.step_count = g_sensor_lis_live.step_count;
  state_out->lis_live.step_detected = g_sensor_lis_live.step_detected;
  state_out->lis_live.tilt_detected = g_sensor_lis_live.tilt_detected;
  state_out->lis_live.sigmot_detected = g_sensor_lis_live.sigmot_detected;
}

static VOID AppUiSyncDebugState(void)
{
  g_ui_page_current = (ULONG)UiRouter_GetCurrentPage();
  g_ui_page_dirty = (UiRouter_IsDirty() != 0U) ? 1UL : 0UL;
}

static void AppUiHandleTick(void)
{
  ui_router_state_t state;
  ULONG mode_flags;
  uint8_t static_now;
  uint8_t static_prev;

  AppUiBuildRouterState(&state);
  UiRouter_UpdateState(&state);
  AppUiSyncDebugState();

  mode_flags = (state.mode_flags & APP_MODE_FLAGS_ALL);
  static_now = ((mode_flags & APP_MODE_FLAG_STATIC) != 0UL) ? 1U : 0U;
  static_prev = ((g_ui_mode_flags_last & APP_MODE_FLAG_STATIC) != 0UL) ? 1U : 0U;

  if ((static_now != 0U) && (static_prev == 0U))
  {
    app_ui_page_t entry_page = APP_UI_PAGE_HOME;

    if ((ULONG)KNOB_UI_STATIC_ENTRY_POINT == APP_UI_STATIC_ENTRY_JOY_CAL)
    {
      entry_page = APP_UI_PAGE_JOY_CAL;
    }
    else if ((ULONG)KNOB_UI_STATIC_ENTRY_POINT == APP_UI_STATIC_ENTRY_HOME)
    {
      entry_page = APP_UI_PAGE_HOME;
    }
    else
    {
      if ((g_sensor_tmag.state == (ULONG)APP_SENSOR_STATE_READY) &&
          (g_sensor_joy_input_gate_valid == 0UL) &&
          (g_sensor_joy_cal_active == 0UL))
      {
        entry_page = APP_UI_PAGE_JOY_CAL;
      }
    }

    AppUiEnterPage(entry_page);
  }

  g_ui_mode_flags_last = mode_flags;

  if ((mode_flags & (APP_MODE_FLAG_STOP | APP_MODE_FLAG_STATIC)) == 0UL)
  {
    return;
  }

  UiRouter_Tick();

  if (UiRouter_IsDirty() != 0U)
  {
    if (AppRendererLock() == TX_SUCCESS)
    {
      UiRouter_Render();
      AppRendererUnlock();
      (void)App_Display_Present();
      UiRouter_ClearDirty();
      AppUiSyncDebugState();
    }
  }
}

static void AppUiEnterPage(app_ui_page_t page)
{
  if (page == APP_UI_PAGE_JOY_CAL)
  {
    UiRouter_RequestPage(UI_PAGE_JOY_CAL);
  }
  else
  {
    UiRouter_RequestPage(UI_PAGE_HOME);
  }
  AppUiSyncDebugState();
}

static uint8_t AppUiHandleAction(const app_input_action_evt_t *evt)
{
  ULONG mode_flags;
  ui_action_evt_t ui_evt;

  if (evt == NULL)
  {
    return 0U;
  }

  mode_flags = (g_eg_mode.tx_event_flags_group_current & APP_MODE_FLAGS_ALL);
  if ((mode_flags & (APP_MODE_FLAG_STOP | APP_MODE_FLAG_STATIC)) == 0UL)
  {
    return 0U;
  }

  switch ((app_input_action_t)evt->action)
  {
    case APP_INPUT_ACTION_CONFIRM:
    case APP_INPUT_ACTION_CANCEL:
    case APP_INPUT_ACTION_LEFT:
    case APP_INPUT_ACTION_RIGHT:
    case APP_INPUT_ACTION_MENU:
    case APP_INPUT_ACTION_UP:
    case APP_INPUT_ACTION_DOWN:
      break;
    default:
      return 0U;
  }

  ui_evt.action = evt->action;
  ui_evt.source = evt->source;
  ui_evt.event = AppUiMapInputEventToRouter(evt->event);
  ui_evt.tick = evt->tick;
  ui_evt.pressed_mask = evt->pressed_mask;
  if (ui_evt.event == 0UL)
  {
    return 0U;
  }

  if (UiRouter_HandleAction(&ui_evt) != 0U)
  {
    AppUiSyncDebugState();
    return 1U;
  }

  AppUiSyncDebugState();
  return 0U;
}

static uint8_t AppGameHandleAction(const app_input_action_evt_t *evt)
{
  ULONG mode_flags;
  UINT status;

  if (evt == NULL)
  {
    return 0U;
  }

  mode_flags = (g_eg_mode.tx_event_flags_group_current & APP_MODE_FLAGS_ALL);
  if ((mode_flags & APP_MODE_FLAG_REALTIME) == 0UL)
  {
    return 0U;
  }

  if (evt->event != (ULONG)APP_INPUT_EVENT_PRESS)
  {
    return 0U;
  }

  switch (evt->action)
  {
    case APP_INPUT_ACTION_CONFIRM:
      RenderDemo_ToggleBackground();
      return 1U;

    case APP_INPUT_ACTION_LEFT:
    case APP_INPUT_ACTION_RIGHT:
      RenderDemo_ToggleCube();
      return 1U;

    case APP_INPUT_ACTION_CANCEL:
      status = App_SysEvent_ModeSet(APP_MODE_STATIC);
      if (status == TX_SUCCESS)
      {
        g_game_exit_to_static_pending = 0UL;
      }
      else
      {
        g_game_exit_to_static_pending = 1UL;
      }
      return 1U;

    case APP_INPUT_ACTION_UP:
    case APP_INPUT_ACTION_DOWN:
      return 1U;

    default:
      return 0U;
  }
}

#include "threads/app_thread_entry_audio.c"

#include "threads/app_thread_entry_sensor.c"

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

static VOID AppPowerPerfReset(void)
{
  g_power_perf_profile_current = APP_POWER_PERF_PROFILE_NORM;
  g_power_perf_profile_target = APP_POWER_PERF_PROFILE_NORM;
  g_power_perf_hint_inflight = 0UL;
  g_power_perf_last_present_ticks = 0UL;
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
  if (next_profile > APP_POWER_PERF_PROFILE_TURBO)
  {
    return;
  }

  g_power_perf_profile_target = next_profile;
  if (next_profile == g_power_perf_profile_current)
  {
    return;
  }

  if (AppPowerPerfCanSwitch(now_tick) == 0U)
  {
    return;
  }

  /* Clock-switch hook: thPower is the only owner allowed to apply SYSCLK profile changes. */
  g_power_perf_profile_current = next_profile;
  g_power_perf_last_switch_tick = now_tick;
  g_power_perf_miss_streak = 0UL;
  g_power_perf_headroom_streak = 0UL;

  if (next_profile == APP_POWER_PERF_PROFILE_TURBO)
  {
    if (g_power_perf_up_count < 0xFFFFFFFFUL)
    {
      g_power_perf_up_count++;
    }
  }
  else
  {
    if (g_power_perf_down_count < 0xFFFFFFFFUL)
    {
      g_power_perf_down_count++;
    }
  }
}

static VOID AppPowerPerfOnModeChange(app_mode_t mode_token, ULONG now_tick)
{
  if (mode_token != APP_MODE_REALTIME)
  {
    AppPowerPerfReset();
    g_power_perf_last_switch_tick = now_tick;
    return;
  }

  AppPowerPerfReset();
  g_power_perf_last_switch_tick = now_tick;
}

static VOID AppPowerPerfHandleHint(ULONG present_ticks, ULONG now_tick)
{
  ULONG mode_flags;
  ULONG budget_ticks;
  ULONG miss_margin;
  ULONG headroom_margin;
  ULONG up_streak_frames;
  ULONG down_streak_frames;

  mode_flags = (g_eg_mode.tx_event_flags_group_current & APP_MODE_FLAGS_ALL);
  if ((mode_flags & APP_MODE_FLAG_REALTIME) == 0UL)
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
  up_streak_frames = (ULONG)KNOB_POWER_PERF_UP_STREAK_FRAMES;
  down_streak_frames = (ULONG)KNOB_POWER_PERF_DOWN_STREAK_FRAMES;
  if (up_streak_frames == 0UL)
  {
    up_streak_frames = 1UL;
  }
  if (down_streak_frames == 0UL)
  {
    down_streak_frames = 1UL;
  }

  g_power_perf_last_present_ticks = present_ticks;

  if (present_ticks > (budget_ticks + miss_margin))
  {
    if (g_power_perf_miss_streak < 0xFFFFFFFFUL)
    {
      g_power_perf_miss_streak++;
    }
    g_power_perf_headroom_streak = 0UL;
  }
  else if ((present_ticks + headroom_margin) < budget_ticks)
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

  if ((g_power_perf_profile_current == APP_POWER_PERF_PROFILE_NORM) &&
      (g_power_perf_miss_streak >= up_streak_frames))
  {
    AppPowerPerfSetProfile(APP_POWER_PERF_PROFILE_TURBO, now_tick);
  }
  else if ((g_power_perf_profile_current == APP_POWER_PERF_PROFILE_TURBO) &&
           (g_power_perf_headroom_streak >= down_streak_frames))
  {
    AppPowerPerfSetProfile(APP_POWER_PERF_PROFILE_NORM, now_tick);
  }
}

static VOID AppPowerPerfHintPost(ULONG present_ticks)
{
  ULONG mode_flags;
  ULONG power_flags;
  ULONG stride;
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
  status = AppSysEventPost(APP_SYS_EVT_PERF_HINT, present_ticks, 0UL, 0UL);
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

#include "threads/app_thread_entry_power.c"

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

static UINT AppStorageReqPost(app_storage_req_type_t req_type, ULONG arg0)
{
  app_storage_req_t msg = {0UL};

  msg.type = (ULONG)req_type;
  msg.arg0 = arg0;

  return tx_queue_send(&g_q_storage_req, &msg, TX_NO_WAIT);
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

  msg.type = (ULONG)cmd_type;
  msg.arg0 = arg0;

  return tx_queue_send(&g_q_audio_cmd, &msg, TX_NO_WAIT);
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
  action_evt->action = (ULONG)APP_INPUT_ACTION_NONE;
  action_evt->pressed_mask = 0UL;

  switch (raw_evt->source)
  {
    case APP_INPUT_SOURCE_BTN_A:
      action_evt->action = (ULONG)APP_INPUT_ACTION_CONFIRM;
      break;

    case APP_INPUT_SOURCE_BTN_B:
      action_evt->action = (ULONG)APP_INPUT_ACTION_CANCEL;
      break;

    case APP_INPUT_SOURCE_BTN_L:
      action_evt->action = (ULONG)APP_INPUT_ACTION_LEFT;
      break;

    case APP_INPUT_SOURCE_BTN_R:
      action_evt->action = (ULONG)APP_INPUT_ACTION_RIGHT;
      break;

    case APP_INPUT_SOURCE_BTN_BOOT:
      action_evt->action = (ULONG)APP_INPUT_ACTION_MENU;
      break;

    case APP_INPUT_SOURCE_JOY_UP:
      action_evt->action = (ULONG)APP_INPUT_ACTION_UP;
      break;

    case APP_INPUT_SOURCE_JOY_RIGHT:
      action_evt->action = (ULONG)APP_INPUT_ACTION_RIGHT;
      break;

    case APP_INPUT_SOURCE_JOY_DOWN:
      action_evt->action = (ULONG)APP_INPUT_ACTION_DOWN;
      break;

    case APP_INPUT_SOURCE_JOY_LEFT:
      action_evt->action = (ULONG)APP_INPUT_ACTION_LEFT;
      break;

    default:
      return 0U;
  }

  if (((ULONG)KNOB_INPUT_BOOT_LONG_ONLY != 0UL) && (raw_evt->source == APP_INPUT_SOURCE_BTN_BOOT))
  {
    return 0U;
  }

  return 1U;
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
  ULONG bit;

  if ((pressed_out == TX_NULL) || (AppInputSourceIsPhysicalButton(source) == 0U))
  {
    return 0U;
  }

  bit = AppInputSourceBit(source);
  if (bit == 0UL)
  {
    return 0U;
  }

  if ((g_input_physical_idle_valid_mask & bit) == 0UL)
  {
    g_input_physical_idle_level[source] = level;
    g_input_physical_idle_valid_mask |= bit;
  }

  *pressed_out = (level != g_input_physical_idle_level[source]) ? 1UL : 0UL;

  if (*pressed_out == 0UL)
  {
    /* Track idle drift only when button is currently released. */
    g_input_physical_idle_level[source] = level;
  }

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
  float fallback_mT = ((float)KNOB_SENSOR_JOY_NEUTRAL_DEADZONE_MIN_MT_X10) / 10.0f;

  if (g_sensor_joy == TX_NULL)
  {
    return;
  }

  if (fallback_mT < 0.1f)
  {
    fallback_mT = 0.1f;
  }

  TMAGJoy_GetAbsDeadzone(g_sensor_joy, &deadzone_en, &deadzone_mT);
  if ((deadzone_en == 0U) || (deadzone_mT < 0.1f))
  {
    TMAGJoy_SetAbsDeadzone(g_sensor_joy, 1U, fallback_mT);
  }
}

static uint8_t AppInputTickDue(ULONG now_tick, ULONG deadline_tick)
{
  return (((LONG)(now_tick - deadline_tick)) >= 0L) ? 1U : 0U;
}

static uint8_t AppInputShouldDebounce(ULONG source, ULONG edge, ULONG tick)
{
  app_input_button_state_t *state;

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

  if (state->edge_seen == 0UL)
  {
    state->edge_seen = 1UL;
    state->last_edge_tick = tick;
    /* Ignore initial release while idle; accept first press. */
    return (edge == APP_INPUT_EDGE_LOW) ? 0U : 1U;
  }

  if ((KNOB_INPUT_DEBOUNCE_TICKS > 0U) && ((tick - state->last_edge_tick) < KNOB_INPUT_DEBOUNCE_TICKS))
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

        switch (source)
        {
          case APP_INPUT_SOURCE_BTN_A:
            evt.action = (ULONG)APP_INPUT_ACTION_CONFIRM;
            break;
          case APP_INPUT_SOURCE_BTN_B:
            evt.action = (ULONG)APP_INPUT_ACTION_CANCEL;
            break;
          case APP_INPUT_SOURCE_BTN_L:
          case APP_INPUT_SOURCE_JOY_LEFT:
            evt.action = (ULONG)APP_INPUT_ACTION_LEFT;
            break;
          case APP_INPUT_SOURCE_BTN_R:
          case APP_INPUT_SOURCE_JOY_RIGHT:
            evt.action = (ULONG)APP_INPUT_ACTION_RIGHT;
            break;
          case APP_INPUT_SOURCE_JOY_UP:
            evt.action = (ULONG)APP_INPUT_ACTION_UP;
            break;
          case APP_INPUT_SOURCE_JOY_DOWN:
            evt.action = (ULONG)APP_INPUT_ACTION_DOWN;
            break;
          case APP_INPUT_SOURCE_BTN_BOOT:
            evt.action = (ULONG)APP_INPUT_ACTION_MENU;
            break;
          default:
            evt.action = (ULONG)APP_INPUT_ACTION_NONE;
            break;
        }

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

    switch (source)
    {
      case APP_INPUT_SOURCE_BTN_A:
        evt.action = (ULONG)APP_INPUT_ACTION_CONFIRM;
        break;
      case APP_INPUT_SOURCE_BTN_B:
        evt.action = (ULONG)APP_INPUT_ACTION_CANCEL;
        break;
      case APP_INPUT_SOURCE_BTN_L:
      case APP_INPUT_SOURCE_JOY_LEFT:
        evt.action = (ULONG)APP_INPUT_ACTION_LEFT;
        break;
      case APP_INPUT_SOURCE_BTN_R:
      case APP_INPUT_SOURCE_JOY_RIGHT:
        evt.action = (ULONG)APP_INPUT_ACTION_RIGHT;
        break;
      case APP_INPUT_SOURCE_JOY_UP:
        evt.action = (ULONG)APP_INPUT_ACTION_UP;
        break;
      case APP_INPUT_SOURCE_JOY_DOWN:
        evt.action = (ULONG)APP_INPUT_ACTION_DOWN;
        break;
      default:
        evt.action = (ULONG)APP_INPUT_ACTION_NONE;
        break;
    }

    held_ticks = (ULONG)(now_tick - state->press_tick);
    state->next_repeat_tick = (now_tick + AppInputRepeatPeriodTicks(source, held_ticks));
    if (evt.action != (ULONG)APP_INPUT_ACTION_NONE)
    {
      g_input_repeat_emit_count++;
      AppInputRouteAction(&evt);
    }
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

  if ((mode_flags & APP_MODE_FLAG_FLASHING) == 0UL)
  {
    uint8_t post_activity = 0U;

    if ((mode_flags & (APP_MODE_FLAG_STOP | APP_MODE_FLAG_STATIC)) != 0UL)
    {
      post_activity = 1U;
    }
    else if ((mode_flags & APP_MODE_FLAG_REALTIME) != 0UL)
    {
      ULONG min_interval_ticks = (ULONG)KNOB_INPUT_REALTIME_ACTIVITY_MIN_TICKS;

      if ((min_interval_ticks == 0UL) ||
          (g_input_sys_activity_last_tick == 0UL) ||
          (AppInputTickDue(routed_evt.tick, g_input_sys_activity_last_tick + min_interval_ticks) != 0U))
      {
        post_activity = 1U;
      }
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

  if (routed_evt.action == (ULONG)APP_INPUT_ACTION_MENU)
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
  g_audio_dma_events |= APP_AUDIO_DMA_HALF_FLAG;
}

void HAL_SAI_TxCpltCallback(SAI_HandleTypeDef *hsai)
{
  if ((hsai == TX_NULL) || (hsai->Instance != SAI1_Block_A) || (g_audio_state != APP_AUDIO_STATE_ACTIVE))
  {
    return;
  }

  {
    HAL_StatusTypeDef hal_status;

    g_audio_full_irq_count++;
    g_audio_dma_events |= APP_AUDIO_DMA_FULL_FLAG;

    hal_status = HAL_SAI_Transmit_DMA(hsai,
                                      (uint8_t *)g_audio_dma_buffer,
                                      (uint16_t)APP_AUDIO_DMA_SAMPLE_COUNT);
    g_audio_last_error = (LONG)hal_status;
    if (hal_status == HAL_OK)
    {
      g_audio_restart_count++;
    }
    else
    {
      g_audio_dma_events |= APP_AUDIO_DMA_ERROR_FLAG;
    }
  }
}

void HAL_SAI_ErrorCallback(SAI_HandleTypeDef *hsai)
{
  if ((hsai == TX_NULL) || (hsai->Instance != SAI1_Block_A) || (g_audio_state != APP_AUDIO_STATE_ACTIVE))
  {
    return;
  }

  g_audio_error_irq_count++;
  g_audio_dma_events |= APP_AUDIO_DMA_ERROR_FLAG;
  g_audio_last_error = (LONG)hsai->ErrorCode;
}

/* USER CODE END 1 */
