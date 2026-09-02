#include "ps_hw6_owner_services.h"

#include <string.h>

#include "display_renderer.h"
#include "LS013B7DH05.h"
#include "ps_dev_audio.h"
#include "ps_egg_state_loader.h"
#include "ps_hw6_rtos_probe.h"
#include "ps_lpbam_display_buffers.h"
#include "ps_lpbam_display_queue.h"
#include "ps_package_reader.h"
#include "ps_scene_runtime.h"
#include "stm32_lpbam.h"
#include "stm32u5xx_ll_spi.h"
#include "main.h"
#include "knobs_autogen.h"
#include "ps_ui_router.h"
#include "ps_dev_adp5360.h"
#include "ps_hw_i2c3.h"

#define PS_HW6_OWNER_PHASE_INIT             (0x6700UL)
#define PS_HW6_OWNER_PHASE_POWER            (0x6701UL)
#define PS_HW6_OWNER_PHASE_DISPLAY          (0x6702UL)
#define PS_HW6_OWNER_PHASE_AUDIO            (0x6703UL)
#define PS_HW6_OWNER_PHASE_COMPLETE         (0x67FFUL)

#define PS_HW6_PMIC_ADDRESS_7BIT            (0x46U)

#define PS_HW6_DISPLAY_PATTERN_ID           (0x54455354UL)
#define PS_HW6_DISPLAY_PRESENT_TIMEOUT_MS   (250U)
#define PS_HW6_DISPLAY_UI_PATTERN_ID         (0x55495047UL)
#define PS_HW6_DISPLAY_BLINK_PATTERN_ID      (0x424C4E4BUL)
#define PS_HW6_DISPLAY_LPBAM_CLEAR_PATTERN   (1UL)
#define PS_HW6_DISPLAY_LPBAM_CLEAR_BOOT_HOLD (2UL)
#define PS_HW6_DISPLAY_LPBAM_CLEAR_UI_RENDER (3UL)
#define PS_HW6_DISPLAY_LPBAM_CLEAR_ABORT     (4UL)
#define PS_HW6_DISPLAY_LPBAM_CLEAR_BLINK     (5UL)
#define PS_HW6_DISPLAY_LPBAM_CLEAR_SHIPPING  (6UL)
#define PS_HW6_DISPLAY_LPBAM_LPTIM_PRESCALER (128UL)
#define PS_HW6_DISPLAY_MILLISECONDS_PER_SECOND (1000UL)
#define PS_HW6_DISPLAY_DRIVER_API_VERSION    (2UL)
#define PS_HW6_DISPLAY_DRIVER_STATE_READY    (1UL)
#define PS_HW6_DISPLAY_DRIVER_STATE_HOLD     (2UL)
#define PS_HW6_DISPLAY_DRIVER_STATE_FAULT    (3UL)

#define PS_HW6_AUDIO_SAMPLE_RATE_HZ         (16000UL)
#define PS_HW6_AUDIO_TONE_HZ                (1000UL)
#define PS_HW6_AUDIO_DURATION_MS            (750UL)
#define PS_HW6_AUDIO_AMPLITUDE              (3000)
#define PS_HW6_AUDIO_TONE_BUFFER_FRAMES     (1024U)
#define PS_HW6_AUDIO_TONE_BUFFER_HALFWORDS  \
  (PS_HW6_AUDIO_TONE_BUFFER_FRAMES * 2U)
#define PS_HW6_AUDIO_STREAM_BUFFER_FRAMES \
  ((uint32_t)KNOB_AUDIO_PCM_DMA_FRAMES)
#define PS_HW6_AUDIO_STREAM_HALF_FRAMES \
  (PS_HW6_AUDIO_STREAM_BUFFER_FRAMES / 2UL)
#define PS_HW6_AUDIO_STREAM_BUFFER_HALFWORDS \
  (PS_HW6_AUDIO_STREAM_BUFFER_FRAMES * 2UL)
#define PS_HW6_AUDIO_STREAM_SOURCE_WINDOW_COUNT (2UL)
#define PS_HW6_AUDIO_ADPCM_BLOCK_HEADER_BYTES (6UL)
#define PS_HW6_AUDIO_AMP_SETTLE_TICKS       (2UL)
#define PS_HW6_AUDIO_DURATION_TICKS \
  ((PS_HW6_AUDIO_DURATION_MS * TX_TIMER_TICKS_PER_SECOND + 999UL) / 1000UL)
#define PS_HW6_AUDIO_COMPLETION_MARGIN_TICKS \
  ((KNOB_AUDIO_DMA_COMPLETION_MARGIN_MS * TX_TIMER_TICKS_PER_SECOND + 999UL) / \
   1000UL)
#define PS_HW6_AUDIO_STREAM_REFILL_TIMEOUT_TICKS \
  (((PS_HW6_AUDIO_STREAM_HALF_FRAMES * TX_TIMER_TICKS_PER_SECOND + \
     PS_EGG_STATE_LOADER_AUDIO_SAMPLE_RATE_HZ - 1UL) / \
    PS_EGG_STATE_LOADER_AUDIO_SAMPLE_RATE_HZ) + \
   PS_HW6_AUDIO_COMPLETION_MARGIN_TICKS)

#if ((KNOB_AUDIO_PCM_DMA_FRAMES < 2) || \
     ((KNOB_AUDIO_PCM_DMA_FRAMES & 1) != 0))
#error "KNOB_AUDIO_PCM_DMA_FRAMES must be an even, non-zero frame count"
#endif

extern I2C_HandleTypeDef hi2c3;
extern RTC_HandleTypeDef hrtc;
extern SAI_HandleTypeDef hsai_BlockA1;
extern DMA_HandleTypeDef handle_GPDMA1_Channel3;
extern LPTIM_HandleTypeDef hlptim1;
extern SPI_HandleTypeDef hspi3;
extern DMA_HandleTypeDef handle_GPDMA1_Channel0;
extern DMA_HandleTypeDef handle_LPDMA1_Channel0;

volatile PS_HW6_OwnerProbe g_ps_hw6_owner_probe;

static ps_dev_adp5360_t ps_hw6_pmic;
static ps_dev_audio_t ps_hw6_audio;
static LS013B7DH05 ps_hw6_display;
static uint32_t ps_hw6_display_driver_initialized;
static uint32_t ps_hw6_display_driver_state;
static uint32_t ps_hw6_display_driver_operation_count;
static uint32_t ps_hw6_display_driver_last_status;
static uint32_t ps_hw6_display_lpbam_debug_force_ready_once;
static uint32_t ps_hw6_display_lpbam_compiled;
static uint32_t ps_hw6_display_lpbam_compiled_cadence_ms;
static uint32_t ps_hw6_display_lpbam_compiled_page;
static uint32_t ps_hw6_display_lpbam_compiled_render_count;
static uint32_t ps_hw6_display_lpbam_prearmed;
static uint32_t ps_hw6_display_lpbam_active;
static int16_t ps_hw6_audio_tone_buffer[PS_HW6_AUDIO_TONE_BUFFER_HALFWORDS]
  __attribute__((aligned(4)));
static int16_t ps_hw6_audio_stream_buffer[
  PS_HW6_AUDIO_STREAM_BUFFER_HALFWORDS]
  __attribute__((aligned(4)));
static uint8_t ps_hw6_audio_stream_source_windows[
  PS_HW6_AUDIO_STREAM_SOURCE_WINDOW_COUNT][PS_PACKAGE_READER_WINDOW_BYTES]
  __attribute__((aligned(4)));

typedef struct
{
  const ps_egg_state_loader_audio_cue_t *cue;
  uint32_t source_offset;
  uint32_t block_index;
  uint32_t block_payload_offset;
  uint32_t block_sample_count;
  uint32_t block_sample_index;
  uint32_t decoded_sample_count;
  int32_t predictor;
  int32_t step_index;
  uint32_t source_window_offset;
  uint32_t source_window_length;
  uint32_t source_window_valid;
  uint32_t source_window_index;
  uint32_t source_window_read_count;
  uint32_t source_window_failure_count;
  uint32_t source_window_bytes;
  uint32_t source_window_last_status;
  uint32_t source_prefetch_offset;
  uint32_t source_prefetch_length;
  uint32_t source_prefetch_index;
  uint32_t source_prefetch_pending;
  uint32_t source_prefetch_start_count;
  uint32_t source_prefetch_complete_count;
  uint32_t source_prefetch_miss_count;
  uint32_t source_prefetch_cleanup_status;
} ps_hw6_audio_stream_decoder_t;

static const int16_t ps_hw6_sine_16[16] =
{
  0, 1148, 2121, 2772, 3000, 2772, 2121, 1148,
  0, -1148, -2121, -2772, -3000, -2772, -2121, -1148
};

static const int8_t ps_hw6_ima_index_table[8] =
{
  -1, -1, -1, -1, 2, 4, 6, 8
};

static const int16_t ps_hw6_ima_step_table[89] =
{
  7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31,
  34, 37, 41, 45, 50, 55, 60, 66, 73, 80, 88, 97, 107, 118, 130,
  143, 157, 173, 190, 209, 230, 253, 279, 307, 337, 371, 408, 449,
  494, 544, 598, 658, 724, 796, 876, 963, 1060, 1166, 1282, 1411,
  1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024, 3327, 3660,
  4026, 4428, 4871, 5358, 5894, 6484, 7132, 7845, 8630, 9493,
  10442, 11487, 12635, 13899, 15289, 16818, 18500, 20350, 22385,
  24623, 27086, 29794, 32767
};

static void PS_HW6_UpdateDisplayDriverProbe(void)
{
  g_ps_hw6_owner_probe.display_driver_api_version =
    PS_HW6_DISPLAY_DRIVER_API_VERSION;
  g_ps_hw6_owner_probe.display_driver_state = ps_hw6_display_driver_state;
  g_ps_hw6_owner_probe.display_driver_operation_count =
    ps_hw6_display_driver_operation_count;
  g_ps_hw6_owner_probe.display_driver_last_status =
    ps_hw6_display_driver_last_status;
}

static HAL_StatusTypeDef PS_HW6_DisplayDriverInit(SPI_HandleTypeDef *bus)
{
  if (bus == NULL)
  {
    ps_hw6_display_driver_state = PS_HW6_DISPLAY_DRIVER_STATE_FAULT;
    ps_hw6_display_driver_last_status = (uint32_t)HAL_ERROR;
    return HAL_ERROR;
  }

  (void)memset(&ps_hw6_display, 0, sizeof(ps_hw6_display));
  ps_hw6_display.Bus = bus;
  ps_hw6_display_driver_initialized = 1UL;
  ps_hw6_display_driver_state = PS_HW6_DISPLAY_DRIVER_STATE_READY;
  ps_hw6_display_driver_last_status = (uint32_t)HAL_OK;
  return HAL_OK;
}

static HAL_StatusTypeDef PS_HW6_DisplayOwner_PresentRendererRows(
  volatile uint32_t *init_status,
  volatile uint32_t *present_status)
{
  const uint16_t *dirty_rows = NULL;
  uint32_t dirty_row_count;
  HAL_StatusTypeDef init_result;
  HAL_StatusTypeDef present_result = HAL_ERROR;

  if (init_status != NULL)
  {
    *init_status = (uint32_t)HAL_ERROR;
  }
  if (present_status != NULL)
  {
    *present_status = (uint32_t)HAL_ERROR;
  }
  if (ps_hw6_display_driver_initialized == 0UL)
  {
    ps_hw6_display_driver_state = PS_HW6_DISPLAY_DRIVER_STATE_FAULT;
    ps_hw6_display_driver_last_status = (uint32_t)HAL_ERROR;
    return HAL_ERROR;
  }

  ps_hw6_display_driver_operation_count++;
  dirty_row_count = DisplayRenderer_GetDirtyRows(&dirty_rows);
  /* LCD_Init issues the panel all-clear command; row presents must not. */
  init_result = (ps_hw6_display.Bus == &hspi3) ? HAL_OK : HAL_ERROR;
  if (init_status != NULL)
  {
    *init_status = (uint32_t)init_result;
  }
  if (init_result == HAL_OK)
  {
    if (dirty_row_count == 0UL)
    {
      present_result = HAL_OK;
    }
    else if ((dirty_rows == NULL) ||
             (dirty_row_count > DISPLAY_RENDERER_DIRTY_ROW_MAX))
    {
      present_result = HAL_ERROR;
    }
    else
    {
      present_result = LCD_PresentRows_DMA(
        &ps_hw6_display,
        DisplayRenderer_GetBuffer(),
        dirty_rows,
        (uint16_t)dirty_row_count,
        PS_HW6_DISPLAY_PRESENT_TIMEOUT_MS);
    }
  }
  if (present_status != NULL)
  {
    *present_status = (uint32_t)present_result;
  }
  if ((init_result == HAL_OK) && (present_result == HAL_OK))
  {
    DisplayRenderer_CommitPresentedFrame();
  }

  ps_hw6_display_driver_last_status = (uint32_t)
    ((init_result == HAL_OK) ? present_result : init_result);
  ps_hw6_display_driver_state =
    (ps_hw6_display_driver_last_status == (uint32_t)HAL_OK) ?
    PS_HW6_DISPLAY_DRIVER_STATE_HOLD : PS_HW6_DISPLAY_DRIVER_STATE_FAULT;
  return (HAL_StatusTypeDef)ps_hw6_display_driver_last_status;
}

static HAL_StatusTypeDef PS_HW6_DisplayOwner_ClearPanel(
  uint32_t *clear_status)
{
  HAL_StatusTypeDef status;

  if (clear_status != NULL)
  {
    *clear_status = (uint32_t)HAL_ERROR;
  }
  if (ps_hw6_display_driver_initialized == 0UL)
  {
    ps_hw6_display_driver_state = PS_HW6_DISPLAY_DRIVER_STATE_FAULT;
    ps_hw6_display_driver_last_status = (uint32_t)HAL_ERROR;
    return HAL_ERROR;
  }

  ps_hw6_display_driver_operation_count++;
  status = LCD_Init(&ps_hw6_display, &hspi3);
  if (clear_status != NULL)
  {
    *clear_status = (uint32_t)status;
  }
  ps_hw6_display_driver_last_status = (uint32_t)status;
  ps_hw6_display_driver_state = (status == HAL_OK) ?
    PS_HW6_DISPLAY_DRIVER_STATE_HOLD : PS_HW6_DISPLAY_DRIVER_STATE_FAULT;
  return status;
}

static void PS_HW6_DisplayOwner_ApplyRendererStats(
  uint32_t pattern_id,
  const display_renderer_stats_t *stats)
{
  if (stats == NULL)
  {
    return;
  }

  g_ps_hw6_owner_probe.display_width = stats->width;
  g_ps_hw6_owner_probe.display_height = stats->height;
  g_ps_hw6_owner_probe.display_pattern_id = pattern_id;
  g_ps_hw6_owner_probe.display_framebuffer_hash = stats->framebuffer_hash;
  g_ps_hw6_owner_probe.display_black_pixels = stats->black_pixels;
  g_ps_hw6_owner_probe.display_dirty_row_count = stats->dirty_row_count;
  g_ps_hw6_owner_probe.display_dirty_first_row = stats->dirty_first_row;
  g_ps_hw6_owner_probe.display_dirty_last_row = stats->dirty_last_row;
  g_ps_hw6_owner_probe.display_renderer_primitive_id = stats->primitive_id;
  g_ps_hw6_owner_probe.display_renderer_previous_focus_row =
    stats->previous_focus_row;
  g_ps_hw6_owner_probe.display_renderer_current_focus_row =
    stats->current_focus_row;
}
static void PS_HW6_UpdateAudioDriverProbe(void)
{
  g_ps_hw6_owner_probe.audio_driver_api_version = ps_hw6_audio.api_version;
  g_ps_hw6_owner_probe.audio_driver_state = ps_hw6_audio.state;
  g_ps_hw6_owner_probe.audio_driver_operation_count =
    ps_hw6_audio.operation_count;
  g_ps_hw6_owner_probe.audio_driver_last_status = ps_hw6_audio.last_status;
  g_ps_hw6_owner_probe.audio_post_stop_resume_mark_count =
    ps_hw6_audio.post_stop_resume_mark_count;
  g_ps_hw6_owner_probe.audio_post_stop_recovery_pending =
    ps_hw6_audio.post_stop_recovery_pending;
  g_ps_hw6_owner_probe.audio_post_stop_recovery_attempt_count =
    ps_hw6_audio.post_stop_recovery_attempt_count;
  g_ps_hw6_owner_probe.audio_post_stop_recovery_success_count =
    ps_hw6_audio.post_stop_recovery_success_count;
  g_ps_hw6_owner_probe.audio_post_stop_recovery_status =
    ps_hw6_audio.post_stop_recovery_status;
}

static void PS_HW6_DisplayOwner_ClearLpbamReadiness(uint32_t reason)
{
  uint32_t preserve_prepare_result =
    ((reason == PS_HW6_DISPLAY_LPBAM_CLEAR_BLINK) &&
     ((ps_hw6_display_lpbam_compiled != 0UL) ||
      ((g_ps_hw6_owner_probe.display_lpbam_prepare_status ==
        (uint32_t)HAL_ERROR) &&
       (g_ps_hw6_owner_probe.display_lpbam_admission_status ==
        (uint32_t)HAL_ERROR) &&
       (g_ps_hw6_owner_probe.display_lpbam_admission_reason !=
        PS_LPBAM_ADMISSION_REASON_NONE)))) ? 1UL : 0UL;

  if (reason != PS_HW6_DISPLAY_LPBAM_CLEAR_BLINK)
  {
    ps_hw6_display_lpbam_compiled = 0UL;
    ps_hw6_display_lpbam_compiled_cadence_ms = 0UL;
    ps_hw6_display_lpbam_compiled_page = 0UL;
    ps_hw6_display_lpbam_compiled_render_count = 0UL;
  }
  g_ps_hw6_owner_probe.display_lpbam_ready = 0UL;
  g_ps_hw6_owner_probe.display_lpbam_ready_page =
    PS_HW6_OWNER_STATUS_NOT_RUN;
  g_ps_hw6_owner_probe.display_lpbam_ready_render_count =
    g_ps_hw6_owner_probe.display_ui_render_count;
  g_ps_hw6_owner_probe.display_lpbam_clear_count++;
  g_ps_hw6_owner_probe.display_lpbam_clear_reason = reason;
  g_ps_hw6_owner_probe.display_lpbam_status =
    PS_HW6_OWNER_STATUS_NOT_RUN;
  if (preserve_prepare_result == 0UL)
  {
    g_ps_hw6_owner_probe.display_lpbam_prepare_status =
      PS_HW6_OWNER_STATUS_NOT_RUN;
    g_ps_hw6_owner_probe.display_lpbam_admission_status =
      PS_HW6_OWNER_STATUS_NOT_RUN;
    g_ps_hw6_owner_probe.display_lpbam_admission_reason =
      PS_LPBAM_ADMISSION_REASON_NONE;
  }
}

static void PS_HW6_DisplayOwner_ResetLpbamPrepareProbe(void)
{
  uint32_t element;
  uint32_t frame;

  g_ps_hw6_owner_probe.display_lpbam_prearmed =
    ps_hw6_display_lpbam_prearmed;
  g_ps_hw6_owner_probe.display_lpbam_active = ps_hw6_display_lpbam_active;
  g_ps_hw6_owner_probe.display_lpbam_animation_id =
    DISPLAY_RENDERER_ANIMATION_NONE;
  g_ps_hw6_owner_probe.display_lpbam_source_primitive_id =
    DISPLAY_RENDERER_PRIMITIVE_NONE;
  g_ps_hw6_owner_probe.display_lpbam_focus_row = DISPLAY_RENDERER_ROW_NONE;
  g_ps_hw6_owner_probe.display_lpbam_phase_count = 0UL;
  g_ps_hw6_owner_probe.display_lpbam_sequence_frame_count = 0UL;
  g_ps_hw6_owner_probe.display_lpbam_cadence_ms = 0UL;
  g_ps_hw6_owner_probe.display_lpbam_current_phase = 0UL;
  g_ps_hw6_owner_probe.display_lpbam_next_deadline_tick = 0UL;
  g_ps_hw6_owner_probe.display_lpbam_sequence_start_frame = 0UL;
  g_ps_hw6_owner_probe.display_lpbam_candidate_row_count = 0UL;
  for (frame = 0UL; frame < PS_HW6_OWNER_LPBAM_SEQUENCE_MAX; ++frame)
  {
    g_ps_hw6_owner_probe.display_lpbam_sequence_phase[frame] = 0UL;
  }
  g_ps_hw6_owner_probe.display_lpbam_element_count = 0UL;
  for (element = 0UL;
       element < PS_HW6_OWNER_LPBAM_ELEMENT_MAX;
       ++element)
  {
    g_ps_hw6_owner_probe.display_lpbam_element_id[element] = 0UL;
    g_ps_hw6_owner_probe.display_lpbam_element_phase_count[element] = 0UL;
  }
  g_ps_hw6_owner_probe.display_lpbam_cursor_start_row = 0UL;
  g_ps_hw6_owner_probe.display_lpbam_cursor_row_count = 0UL;
  g_ps_hw6_owner_probe.display_lpbam_cursor_start_column = 0UL;
  g_ps_hw6_owner_probe.display_lpbam_cursor_column_count = 0UL;
  g_ps_hw6_owner_probe.display_lpbam_payload_frame_count = 0UL;
  g_ps_hw6_owner_probe.display_lpbam_payload_chunk_count = 0UL;
  g_ps_hw6_owner_probe.display_lpbam_payload_bytes = 0UL;
  g_ps_hw6_owner_probe.display_lpbam_admission_api_version =
    PS_LPBAM_DISPLAY_ADMISSION_API_VERSION;
  g_ps_hw6_owner_probe.display_lpbam_admission_status =
    PS_HW6_OWNER_STATUS_NOT_RUN;
  g_ps_hw6_owner_probe.display_lpbam_admission_reason =
    PS_LPBAM_ADMISSION_REASON_NONE;
  g_ps_hw6_owner_probe.display_lpbam_admission_sequence_capacity =
    PS_LPBAM_DISPLAY_SEQUENCE_MAX;
  g_ps_hw6_owner_probe.display_lpbam_admission_chunk_capacity =
    PS_LPBAM_DISPLAY_MAX_CHUNKS;
  g_ps_hw6_owner_probe.display_lpbam_admission_payload_used_bytes = 0UL;
  g_ps_hw6_owner_probe.display_lpbam_admission_payload_capacity_bytes = 0UL;
  g_ps_hw6_owner_probe.display_lpbam_compile_mode = 0UL;
  g_ps_hw6_owner_probe.display_lpbam_preferred_status =
    PS_HW6_OWNER_STATUS_NOT_RUN;
  g_ps_hw6_owner_probe.display_lpbam_preferred_reason =
    PS_LPBAM_ADMISSION_REASON_NONE;
  g_ps_hw6_owner_probe.display_lpbam_preferred_sequence_used = 0UL;
  g_ps_hw6_owner_probe.display_lpbam_preferred_chunk_used = 0UL;
  g_ps_hw6_owner_probe.display_lpbam_preferred_payload_used_bytes = 0UL;
  g_ps_hw6_owner_probe.display_lpbam_guaranteed_attempt_count = 0UL;
  g_ps_hw6_owner_probe.display_lpbam_guaranteed_status =
    PS_HW6_OWNER_STATUS_NOT_RUN;
  g_ps_hw6_owner_probe.display_lpbam_compiled_sequence_frame_count = 0UL;
  g_ps_hw6_owner_probe.display_lpbam_compiled_sequence_start_frame = 0UL;
  for (frame = 0UL; frame < PS_HW6_OWNER_LPBAM_SEQUENCE_MAX; ++frame)
  {
    g_ps_hw6_owner_probe.display_lpbam_compiled_sequence_phase[frame] = 0UL;
  }
  g_ps_hw6_owner_probe.display_lpbam_fill_status =
    PS_HW6_OWNER_STATUS_NOT_RUN;
  g_ps_hw6_owner_probe.display_lpbam_clock_status =
    PS_HW6_OWNER_STATUS_NOT_RUN;
  g_ps_hw6_owner_probe.display_lpbam_link_status =
    PS_HW6_OWNER_STATUS_NOT_RUN;
  g_ps_hw6_owner_probe.display_lpbam_prearm_status =
    PS_HW6_OWNER_STATUS_NOT_RUN;
  g_ps_hw6_owner_probe.display_lpbam_commit_status =
    PS_HW6_OWNER_STATUS_NOT_RUN;
  g_ps_hw6_owner_probe.display_lpbam_start_status =
    PS_HW6_OWNER_STATUS_NOT_RUN;
  g_ps_hw6_owner_probe.display_lpbam_dma_start_status =
    PS_HW6_OWNER_STATUS_NOT_RUN;
  g_ps_hw6_owner_probe.display_lpbam_lptim_init_status =
    PS_HW6_OWNER_STATUS_NOT_RUN;
  g_ps_hw6_owner_probe.display_lpbam_lptim_oc_status =
    PS_HW6_OWNER_STATUS_NOT_RUN;
  g_ps_hw6_owner_probe.display_lpbam_lptim_arr_status =
    PS_HW6_OWNER_STATUS_NOT_RUN;
  g_ps_hw6_owner_probe.display_lpbam_lptim_cmp_status =
    PS_HW6_OWNER_STATUS_NOT_RUN;
  g_ps_hw6_owner_probe.display_lpbam_lptim_start_status =
    PS_HW6_OWNER_STATUS_NOT_RUN;
  g_ps_hw6_owner_probe.display_lpbam_lptim_restore_status =
    PS_HW6_OWNER_STATUS_NOT_RUN;
  g_ps_hw6_owner_probe.display_lpbam_lptim_cr_after_config =
    PS_HW6_OWNER_STATUS_NOT_RUN;
  g_ps_hw6_owner_probe.display_lpbam_lptim_cfgr_after_config =
    PS_HW6_OWNER_STATUS_NOT_RUN;
  g_ps_hw6_owner_probe.display_lpbam_lptim_ccmr1_after_config =
    PS_HW6_OWNER_STATUS_NOT_RUN;
  g_ps_hw6_owner_probe.display_lpbam_lptim_arr_after_config =
    PS_HW6_OWNER_STATUS_NOT_RUN;
  g_ps_hw6_owner_probe.display_lpbam_lptim_cmp_after_config =
    PS_HW6_OWNER_STATUS_NOT_RUN;
  g_ps_hw6_owner_probe.display_lpbam_rcc_srdamr_before =
    PS_HW6_OWNER_STATUS_NOT_RUN;
  g_ps_hw6_owner_probe.display_lpbam_rcc_srdamr_after =
    PS_HW6_OWNER_STATUS_NOT_RUN;
  g_ps_hw6_owner_probe.display_lpbam_spi_autocr_before =
    PS_HW6_OWNER_STATUS_NOT_RUN;
  g_ps_hw6_owner_probe.display_lpbam_spi_autocr_after =
    PS_HW6_OWNER_STATUS_NOT_RUN;
  g_ps_hw6_owner_probe.display_lpbam_spi_kernel_hz =
    PS_HW6_OWNER_STATUS_NOT_RUN;
  g_ps_hw6_owner_probe.display_lpbam_spi_init_direction =
    PS_HW6_OWNER_STATUS_NOT_RUN;
  g_ps_hw6_owner_probe.display_lpbam_spi_init_prescaler =
    PS_HW6_OWNER_STATUS_NOT_RUN;
  g_ps_hw6_owner_probe.display_lpbam_spi_init_ss_idleness =
    PS_HW6_OWNER_STATUS_NOT_RUN;
  g_ps_hw6_owner_probe.display_lpbam_spi_cfg1_after_init =
    PS_HW6_OWNER_STATUS_NOT_RUN;
  g_ps_hw6_owner_probe.display_lpbam_spi_cfg2_after_init =
    PS_HW6_OWNER_STATUS_NOT_RUN;
  g_ps_hw6_owner_probe.display_lpbam_dma_state_after_start =
    PS_HW6_OWNER_STATUS_NOT_RUN;
  g_ps_hw6_owner_probe.display_lpbam_dma_error_after_start =
    PS_HW6_OWNER_STATUS_NOT_RUN;
  g_ps_hw6_owner_probe.display_lpbam_queue_node_count = 0UL;
  g_ps_hw6_owner_probe.display_lpbam_abort_lptim_status =
    PS_HW6_OWNER_STATUS_NOT_RUN;
  g_ps_hw6_owner_probe.display_lpbam_abort_dma_status =
    PS_HW6_OWNER_STATUS_NOT_RUN;
  g_ps_hw6_owner_probe.display_lpbam_abort_unlink_status =
    PS_HW6_OWNER_STATUS_NOT_RUN;
  g_ps_hw6_owner_probe.display_lpbam_abort_spi_status =
    PS_HW6_OWNER_STATUS_NOT_RUN;
  g_ps_hw6_owner_probe.display_lpbam_restore_status =
    PS_HW6_OWNER_STATUS_NOT_RUN;
}

static void PS_HW6_DisplayOwner_RecordLpbamSpiInitProbe(void)
{
  if (hspi3.Instance == NULL)
  {
    return;
  }

  g_ps_hw6_owner_probe.display_lpbam_spi_kernel_hz =
    HAL_RCCEx_GetPeriphCLKFreq(RCC_PERIPHCLK_SPI3);
  g_ps_hw6_owner_probe.display_lpbam_spi_init_direction =
    hspi3.Init.Direction;
  g_ps_hw6_owner_probe.display_lpbam_spi_init_prescaler =
    hspi3.Init.BaudRatePrescaler;
  g_ps_hw6_owner_probe.display_lpbam_spi_init_ss_idleness =
    hspi3.Init.MasterSSIdleness;
  g_ps_hw6_owner_probe.display_lpbam_spi_cfg1_after_init =
    hspi3.Instance->CFG1;
  g_ps_hw6_owner_probe.display_lpbam_spi_cfg2_after_init =
    hspi3.Instance->CFG2;
}

static uint32_t PS_HW6_DisplayOwner_GetAwakeDmaState(void)
{
  return (hspi3.hdmatx != NULL) ? HAL_DMA_GetState(hspi3.hdmatx) :
    (uint32_t)HAL_DMA_STATE_RESET;
}

static uint32_t PS_HW6_DisplayOwner_GetAwakeDmaError(void)
{
  return (hspi3.hdmatx != NULL) ? HAL_DMA_GetError(hspi3.hdmatx) :
    HAL_DMA_ERROR_NO_XFER;
}

static HAL_StatusTypeDef PS_HW6_DisplayOwner_EnableLpbamAutonomousClocks(void)
{
  SPI_AutonomousModeConfTypeDef autonomous = {0};

  if ((hspi3.Instance == NULL) || (hlptim1.Instance == NULL) ||
      (handle_LPDMA1_Channel0.Instance == NULL))
  {
    return HAL_ERROR;
  }

  g_ps_hw6_owner_probe.display_lpbam_rcc_srdamr_before = RCC->SRDAMR;
  g_ps_hw6_owner_probe.display_lpbam_spi_autocr_before =
    hspi3.Instance->AUTOCR;

  __HAL_RCC_SPI3_CLKAM_ENABLE();
  __HAL_RCC_LPTIM1_CLKAM_ENABLE();
  __HAL_RCC_LPDMA1_CLKAM_ENABLE();
  __HAL_RCC_SRAM4_CLKAM_ENABLE();
  __HAL_RCC_SPI3_CLK_SLEEP_ENABLE();
  __HAL_RCC_LPTIM1_CLK_SLEEP_ENABLE();
  __HAL_RCC_LPDMA1_CLK_SLEEP_ENABLE();
  __HAL_RCC_SRAM4_CLK_SLEEP_ENABLE();
  __HAL_RCC_MSIK_ENABLE();
  __HAL_RCC_MSIKSTOP_ENABLE();

  /* LPDMA gates the first transaction of each logical frame. Continuation
     transactions must accept their CSTART nodes without another timer edge. */
  autonomous.TriggerState = SPI_AUTO_MODE_DISABLE;
  autonomous.TriggerSelection = SPI_GRP2_LPTIM1_CH1_TRG;
  autonomous.TriggerPolarity = SPI_TRIG_POLARITY_RISING;
  if (HAL_SPIEx_SetConfigAutonomousMode(&hspi3, &autonomous) != HAL_OK)
  {
    return HAL_ERROR;
  }
  PS_HW6_DisplayOwner_RecordLpbamSpiInitProbe();
  g_ps_hw6_owner_probe.display_lpbam_spi_autocr_after =
    hspi3.Instance->AUTOCR;

  if (ADV_LPBAM_SPI_EnableDMARequests(SPI3) != LPBAM_OK)
  {
    return HAL_ERROR;
  }

  g_ps_hw6_owner_probe.display_lpbam_rcc_srdamr_after = RCC->SRDAMR;
  g_ps_hw6_owner_probe.display_lpbam_spi_autocr_after =
    hspi3.Instance->AUTOCR;
  return HAL_OK;
}

static void PS_HW6_DisplayOwner_RecordLptimConfigProbe(void)
{
  if (hlptim1.Instance == NULL)
  {
    return;
  }

  g_ps_hw6_owner_probe.display_lpbam_lptim_cr_after_config =
    hlptim1.Instance->CR;
  g_ps_hw6_owner_probe.display_lpbam_lptim_cfgr_after_config =
    hlptim1.Instance->CFGR;
  g_ps_hw6_owner_probe.display_lpbam_lptim_ccmr1_after_config =
    hlptim1.Instance->CCMR1;
  g_ps_hw6_owner_probe.display_lpbam_lptim_arr_after_config =
    hlptim1.Instance->ARR;
  g_ps_hw6_owner_probe.display_lpbam_lptim_cmp_after_config =
    hlptim1.Instance->CCR1;
}

static void PS_HW6_DisplayOwner_SetLptimBaseInit(uint32_t prescaler,
                                                 uint32_t period)
{
  hlptim1.Instance = LPTIM1;
  hlptim1.Init.Clock.Source = LPTIM_CLOCKSOURCE_APBCLOCK_LPOSC;
  hlptim1.Init.Clock.Prescaler = prescaler;
  hlptim1.Init.Trigger.Source = LPTIM_TRIGSOURCE_SOFTWARE;
  hlptim1.Init.Period = period;
  hlptim1.Init.UpdateMode = LPTIM_UPDATE_IMMEDIATE;
  hlptim1.Init.CounterSource = LPTIM_COUNTERSOURCE_INTERNAL;
  hlptim1.Init.Input1Source = LPTIM_INPUT1SOURCE_GPIO;
  hlptim1.Init.Input2Source = LPTIM_INPUT2SOURCE_GPIO;
  hlptim1.Init.RepetitionCounter = 0UL;
}

static HAL_StatusTypeDef PS_HW6_DisplayOwner_WaitLptimFlag(uint32_t flag)
{
  for (uint32_t spin = 0U; spin < 100000U; ++spin)
  {
    if (__HAL_LPTIM_GET_FLAG(&hlptim1, flag) != RESET)
    {
      return HAL_OK;
    }
  }

  return HAL_TIMEOUT;
}

static HAL_StatusTypeDef PS_HW6_DisplayOwner_GetLpbamTimerCounts(
  uint32_t cadence_ms,
  uint32_t *period_counts,
  uint32_t *compare_counts)
{
  uint32_t lptim_kernel_hz;
  uint64_t denominator;
  uint64_t counts;

  if ((cadence_ms == 0UL) ||
      (period_counts == NULL) || (compare_counts == NULL))
  {
    return HAL_ERROR;
  }

  lptim_kernel_hz = HAL_RCCEx_GetPeriphCLKFreq(RCC_PERIPHCLK_LPTIM1);
  denominator = (uint64_t)PS_HW6_DISPLAY_LPBAM_LPTIM_PRESCALER *
                (uint64_t)PS_HW6_DISPLAY_MILLISECONDS_PER_SECOND;
  counts = ((uint64_t)lptim_kernel_hz *
            (uint64_t)cadence_ms +
            (denominator / 2ULL)) / denominator;
  if ((lptim_kernel_hz == 0UL) || (counts < 2ULL) ||
      (counts > 65535ULL))
  {
    return HAL_ERROR;
  }

  *period_counts = (uint32_t)counts;
  *compare_counts = (uint32_t)(counts - 1ULL);
  return HAL_OK;
}

static HAL_StatusTypeDef PS_HW6_DisplayOwner_ConfigLpbamLptim(
  uint32_t cadence_ms)
{
  LPTIM_OC_ConfigTypeDef oc = {0};
  HAL_StatusTypeDef status;
  uint32_t period_counts;
  uint32_t compare_counts;

  __HAL_RCC_MSIK_ENABLE();
  if (__HAL_RCC_GET_FLAG(RCC_FLAG_MSIKRDY) == 0U)
  {
    return HAL_TIMEOUT;
  }
  if (PS_HW6_DisplayOwner_GetLpbamTimerCounts(cadence_ms,
                                               &period_counts,
                                               &compare_counts) != HAL_OK)
  {
    return HAL_ERROR;
  }

  if (hlptim1.Instance != NULL)
  {
    __HAL_LPTIM_DISABLE(&hlptim1);
    (void)HAL_LPTIM_DeInit(&hlptim1);
  }

  PS_HW6_DisplayOwner_SetLptimBaseInit(LPTIM_PRESCALER_DIV1, 65535UL);
  status = HAL_LPTIM_Init(&hlptim1);
  g_ps_hw6_owner_probe.display_lpbam_lptim_init_status =
    (uint32_t)status;
  if (status != HAL_OK)
  {
    PS_HW6_DisplayOwner_RecordLptimConfigProbe();
    return status;
  }

  oc.Pulse = 0UL;
  oc.OCPolarity = LPTIM_OCPOLARITY_HIGH;
  status = HAL_LPTIM_OC_ConfigChannel(&hlptim1, &oc, LPTIM_CHANNEL_1);
  g_ps_hw6_owner_probe.display_lpbam_lptim_oc_status =
    (uint32_t)status;
  if (status != HAL_OK)
  {
    PS_HW6_DisplayOwner_RecordLptimConfigProbe();
    return status;
  }

  __HAL_LPTIM_DISABLE(&hlptim1);
  MODIFY_REG(hlptim1.Instance->CFGR,
             LPTIM_CFGR_PRESC,
             LPTIM_PRESCALER_DIV128);
  __HAL_LPTIM_ENABLE(&hlptim1);

  __HAL_LPTIM_CLEAR_FLAG(&hlptim1, LPTIM_FLAG_ARROK);
  __HAL_LPTIM_AUTORELOAD_SET(&hlptim1, period_counts);
  status = PS_HW6_DisplayOwner_WaitLptimFlag(LPTIM_FLAG_ARROK);
  g_ps_hw6_owner_probe.display_lpbam_lptim_arr_status =
    (uint32_t)status;
  if (status != HAL_OK)
  {
    PS_HW6_DisplayOwner_RecordLptimConfigProbe();
    return status;
  }

  __HAL_LPTIM_CLEAR_FLAG(&hlptim1, LPTIM_FLAG_CMP1OK);
  __HAL_LPTIM_COMPARE_SET(&hlptim1,
                          LPTIM_CHANNEL_1,
                          compare_counts);
  status = PS_HW6_DisplayOwner_WaitLptimFlag(LPTIM_FLAG_CMP1OK);
  g_ps_hw6_owner_probe.display_lpbam_lptim_cmp_status =
    (uint32_t)status;
  PS_HW6_DisplayOwner_RecordLptimConfigProbe();
  return status;
}

static HAL_StatusTypeDef PS_HW6_DisplayOwner_RestoreLptimAfterLpbam(void)
{
  LPTIM_IC_ConfigTypeDef ic = {0};
  HAL_StatusTypeDef status;

  if (hlptim1.Instance != NULL)
  {
    __HAL_LPTIM_DISABLE(&hlptim1);
    (void)HAL_LPTIM_DeInit(&hlptim1);
  }

  PS_HW6_DisplayOwner_SetLptimBaseInit(LPTIM_PRESCALER_DIV1, 65535UL);
  status = HAL_LPTIM_Init(&hlptim1);
  if (status != HAL_OK)
  {
    return status;
  }

  ic.ICInputSource = LPTIM_IC1SOURCE_COMP1;
  ic.ICPrescaler = LPTIM_ICPSC_DIV1;
  ic.ICPolarity = LPTIM_ICPOLARITY_RISING;
  ic.ICFilter = LPTIM_ICFLT_CLOCK_DIV1;
  return HAL_LPTIM_IC_ConfigChannel(&hlptim1, &ic, LPTIM_CHANNEL_1);
}

static HAL_StatusTypeDef PS_HW6_DisplayOwner_LinkLpbamQueue(void)
{
  HAL_StatusTypeDef status;

  if (handle_LPDMA1_Channel0.Instance == NULL)
  {
    return HAL_ERROR;
  }

  handle_LPDMA1_Channel0.InitLinkedList.Priority =
    DMA_LOW_PRIORITY_HIGH_WEIGHT;
  handle_LPDMA1_Channel0.InitLinkedList.LinkStepMode =
    DMA_LSM_FULL_EXECUTION;
  handle_LPDMA1_Channel0.InitLinkedList.TransferEventMode =
    DMA_TCEM_LAST_LL_ITEM_TRANSFER;
  handle_LPDMA1_Channel0.InitLinkedList.LinkedListMode =
    DMA_LINKEDLIST_CIRCULAR;

  status = HAL_DMAEx_List_Init(&handle_LPDMA1_Channel0);
  if (status != HAL_OK)
  {
    return status;
  }

  status = PS_LpbamDisplayQueue_Link(&handle_LPDMA1_Channel0);
  g_ps_hw6_owner_probe.display_lpbam_queue_node_count =
    PS_LpbamDisplayQueue_GetNodeCount();
  return status;
}

static HAL_StatusTypeDef PS_HW6_DisplayOwner_AbortDisplaySpiAfterLpbam(void)
{
  if (hspi3.Instance == NULL)
  {
    return HAL_ERROR;
  }

  return HAL_SPI_Abort(&hspi3);
}

static HAL_StatusTypeDef PS_HW6_DisplayOwner_RestoreDisplaySpiAfterLpbam(void)
{
  SPI_AutonomousModeConfTypeDef autonomous = {0};
  HAL_StatusTypeDef status;

  if (hspi3.Instance == NULL)
  {
    return HAL_ERROR;
  }

  LL_SPI_DisableDMAReq_TX(SPI3);
  status = HAL_SPI_DeInit(&hspi3);
  if (status != HAL_OK)
  {
    return status;
  }

  status = HAL_SPI_Init(&hspi3);
  if (status != HAL_OK)
  {
    return status;
  }

  /* HAL_SPI_Init reruns MSP setup, where CubeMX links LPDMA last. */
  __HAL_LINKDMA(&hspi3, hdmatx, handle_GPDMA1_Channel0);

  autonomous.TriggerState = SPI_AUTO_MODE_DISABLE;
  autonomous.TriggerSelection = SPI_GRP2_LPTIM1_CH1_TRG;
  autonomous.TriggerPolarity = SPI_TRIG_POLARITY_RISING;
  return HAL_SPIEx_SetConfigAutonomousMode(&hspi3, &autonomous);
}

static HAL_StatusTypeDef PS_HW6_DisplayOwner_StopLpbamPlayback(void)
{
  HAL_StatusTypeDef lptim_status;
  HAL_StatusTypeDef dma_status;
  HAL_StatusTypeDef unlink_status;
  HAL_StatusTypeDef spi_status;
  HAL_StatusTypeDef lptim_restore_status;
  HAL_StatusTypeDef restore_status;
  HAL_StatusTypeDef final_status = HAL_OK;

  if ((ps_hw6_display_lpbam_active == 0UL) &&
      (ps_hw6_display_lpbam_prearmed == 0UL))
  {
    g_ps_hw6_owner_probe.display_lpbam_prearmed = 0UL;
    g_ps_hw6_owner_probe.display_lpbam_active = 0UL;
    return HAL_OK;
  }

  if (ps_hw6_display_lpbam_active != 0UL)
  {
    lptim_status = HAL_LPTIM_PWM_Stop(&hlptim1, LPTIM_CHANNEL_1);
    dma_status = HAL_DMA_Abort(&handle_LPDMA1_Channel0);
    spi_status = PS_HW6_DisplayOwner_AbortDisplaySpiAfterLpbam();
  }
  else
  {
    lptim_status = HAL_OK;
    dma_status = HAL_OK;
    spi_status = HAL_OK;
  }
  g_ps_hw6_owner_probe.display_lpbam_abort_lptim_status =
    (uint32_t)lptim_status;
  __HAL_LPTIM_DISABLE(&hlptim1);
  g_ps_hw6_owner_probe.display_lpbam_abort_dma_status =
    (uint32_t)dma_status;

  unlink_status = HAL_DMAEx_List_UnLinkQ(&handle_LPDMA1_Channel0);
  g_ps_hw6_owner_probe.display_lpbam_abort_unlink_status =
    (uint32_t)unlink_status;

  g_ps_hw6_owner_probe.display_lpbam_abort_spi_status =
    (uint32_t)spi_status;

  lptim_restore_status = PS_HW6_DisplayOwner_RestoreLptimAfterLpbam();
  g_ps_hw6_owner_probe.display_lpbam_lptim_restore_status =
    (uint32_t)lptim_restore_status;

  restore_status = PS_HW6_DisplayOwner_RestoreDisplaySpiAfterLpbam();
  g_ps_hw6_owner_probe.display_lpbam_restore_status =
    (uint32_t)restore_status;

  if ((lptim_status != HAL_OK) || (dma_status != HAL_OK) ||
      (unlink_status != HAL_OK) || (spi_status != HAL_OK) ||
      (lptim_restore_status != HAL_OK) || (restore_status != HAL_OK))
  {
    final_status = HAL_ERROR;
  }

  ps_hw6_display_lpbam_prearmed = 0UL;
  ps_hw6_display_lpbam_active = 0UL;
  g_ps_hw6_owner_probe.display_lpbam_prearmed = 0UL;
  g_ps_hw6_owner_probe.display_lpbam_active = 0UL;
  return final_status;
}

static HAL_StatusTypeDef PS_HW6_DisplayOwner_PrearmLpbamPlayback(void)
{
  HAL_StatusTypeDef status;

  status = PS_HW6_DisplayOwner_LinkLpbamQueue();
  g_ps_hw6_owner_probe.display_lpbam_link_status = (uint32_t)status;
  if (status != HAL_OK)
  {
    return status;
  }

  ps_hw6_display_lpbam_prearmed = 1UL;
  g_ps_hw6_owner_probe.display_lpbam_prearmed = 1UL;
  return HAL_OK;
}

HAL_StatusTypeDef PS_HW6_DisplayOwner_CommitLpbamStop2(void)
{
  HAL_StatusTypeDef status;

  g_ps_hw6_owner_probe.display_lpbam_commit_count++;
  g_ps_hw6_owner_probe.display_lpbam_commit_tick =
    (uint32_t)tx_time_get();

  if ((ps_hw6_display_lpbam_prearmed == 0UL) ||
      (ps_hw6_display_lpbam_active != 0UL) ||
      (g_ps_hw6_owner_probe.display_lpbam_ready == 0UL) ||
      (g_ps_hw6_owner_probe.display_lpbam_status != (uint32_t)HAL_OK))
  {
    g_ps_hw6_owner_probe.display_lpbam_commit_status =
      (uint32_t)HAL_ERROR;
    g_ps_hw6_owner_probe.display_lpbam_start_status =
      (uint32_t)HAL_ERROR;
    return HAL_ERROR;
  }

  status = HAL_DMAEx_List_Start(&handle_LPDMA1_Channel0);
  g_ps_hw6_owner_probe.display_lpbam_dma_start_status =
    (uint32_t)status;
  g_ps_hw6_owner_probe.display_lpbam_dma_state_after_start =
    handle_LPDMA1_Channel0.State;
  g_ps_hw6_owner_probe.display_lpbam_dma_error_after_start =
    handle_LPDMA1_Channel0.ErrorCode;
  if (status != HAL_OK)
  {
    g_ps_hw6_owner_probe.display_lpbam_commit_status =
      (uint32_t)status;
    g_ps_hw6_owner_probe.display_lpbam_start_status =
      (uint32_t)status;
    return status;
  }

  ps_hw6_display_lpbam_prearmed = 0UL;
  ps_hw6_display_lpbam_compiled = 0UL;
  ps_hw6_display_lpbam_active = 1UL;
  g_ps_hw6_owner_probe.display_lpbam_prearmed = 0UL;
  g_ps_hw6_owner_probe.display_lpbam_active = 1UL;
  __HAL_LPTIM_RESET_COUNTER(&hlptim1);
  status = HAL_LPTIM_PWM_Start(&hlptim1, LPTIM_CHANNEL_1);
  g_ps_hw6_owner_probe.display_lpbam_lptim_start_status =
    (uint32_t)status;
  if (status != HAL_OK)
  {
    g_ps_hw6_owner_probe.display_lpbam_commit_status =
      (uint32_t)status;
    g_ps_hw6_owner_probe.display_lpbam_start_status =
      (uint32_t)status;
    return status;
  }

  g_ps_hw6_owner_probe.display_lpbam_commit_status =
    (uint32_t)HAL_OK;
  g_ps_hw6_owner_probe.display_lpbam_start_status =
    (uint32_t)HAL_OK;
  return HAL_OK;
}

static void PS_HW6_PrepareDisplayUIPage(uint32_t page,
                                        uint32_t calibration_page,
                                        uint32_t focus_index,
                                        uint32_t shutdown_state,
                                        uint32_t shutdown_countdown_seconds,
                                        const ps_scene_render_model_t *
                                          scene_model)
{
  display_renderer_stats_t stats;

  DisplayRenderer_PrepareUIPage(page,
                                calibration_page,
                                focus_index,
                                shutdown_state,
                                shutdown_countdown_seconds,
                                scene_model,
                                &stats);
  PS_HW6_DisplayOwner_ApplyRendererStats(
    PS_HW6_DISPLAY_UI_PATTERN_ID, &stats);
  g_ps_hw6_owner_probe.display_ui_primitive_id = stats.primitive_id;
  g_ps_hw6_owner_probe.display_ui_previous_focus_row =
    stats.previous_focus_row;
  g_ps_hw6_owner_probe.display_ui_current_focus_row =
    stats.current_focus_row;
}

static void PS_HW6_DisplayOwner_SnapshotSceneWaitingTimeline(void)
{
  const display_renderer_waiting_animation_t *animation;
  uint32_t presentation_id;
  uint32_t sequence_count;
  uint32_t settled_frame;
  uint32_t cadence_ms;
  uint32_t frame;

  g_ps_hw6_owner_probe.display_waiting_presentation_id = 0UL;
  g_ps_hw6_owner_probe.display_waiting_sequence_frame_count = 0UL;
  g_ps_hw6_owner_probe.display_waiting_settled_sequence_frame = 0UL;
  g_ps_hw6_owner_probe.display_waiting_cadence_ms = 0UL;
  g_ps_hw6_owner_probe.display_waiting_element_count = 0UL;
  for (frame = 0UL; frame < PS_HW6_OWNER_LPBAM_SEQUENCE_MAX; ++frame)
  {
    g_ps_hw6_owner_probe.display_waiting_sequence_phase[frame] = 0UL;
  }
  g_ps_hw6_owner_probe.display_waiting_snapshot_status =
    PS_HW6_OWNER_STATUS_UNAVAILABLE;

  if (DisplayRenderer_GetSceneWaitingTimeline(
        &presentation_id,
        &sequence_count,
        &settled_frame,
        &cadence_ms) == 0UL)
  {
    return;
  }
  animation = DisplayRenderer_GetWaitingAnimation(settled_frame, 0UL);
  if ((DisplayRenderer_ValidateWaitingAnimation(animation) == 0UL) ||
      (animation->sequence_frame_count != sequence_count) ||
      (settled_frame >= sequence_count))
  {
    return;
  }

  g_ps_hw6_owner_probe.display_waiting_presentation_id = presentation_id;
  g_ps_hw6_owner_probe.display_waiting_sequence_frame_count = sequence_count;
  g_ps_hw6_owner_probe.display_waiting_settled_sequence_frame =
    settled_frame;
  g_ps_hw6_owner_probe.display_waiting_cadence_ms = cadence_ms;
  g_ps_hw6_owner_probe.display_waiting_element_count =
    animation->element_count;
  for (frame = 0UL; frame < sequence_count; ++frame)
  {
    g_ps_hw6_owner_probe.display_waiting_sequence_phase[frame] =
      animation->sequence_phase[frame];
  }
  g_ps_hw6_owner_probe.display_waiting_snapshot_status = (uint32_t)HAL_OK;
}

static void PS_HW6_DisplayOwner_PublishStateWaitingVisual(
  uint32_t page,
  uint32_t focus_index,
  const ps_scene_render_model_t *scene_model)
{
  ps_scene_waiting_visual_bounds_t cursor_bounds;
  const ps_scene_waiting_visual_t *visual;
  uint32_t bounds_ready;

  if ((page == (uint32_t)PS_UI_ROUTER_PAGE_INTERACTION_CUE) ||
      (page == (uint32_t)PS_UI_ROUTER_PAGE_INTERACTION_ACTIVATION))
  {
    DisplayRenderer_ClearSceneWaitingVisual();
    return;
  }
  if ((page == (uint32_t)PS_UI_ROUTER_PAGE_RUNTIME_HANDOFF) &&
      (PS_SceneRuntime_StateSceneActive() != 0UL))
  {
    bounds_ready = DisplayRenderer_GetSceneFocusLogicalBounds(
      scene_model, &cursor_bounds);
  }
  else
  {
    bounds_ready = DisplayRenderer_GetListCursorLogicalBounds(
      focus_index, &cursor_bounds);
  }
  if (bounds_ready == 0UL)
  {
    DisplayRenderer_ClearSceneWaitingVisual();
    return;
  }

  if ((page == (uint32_t)PS_UI_ROUTER_PAGE_RUNTIME_HANDOFF) &&
      (PS_SceneRuntime_StateSceneActive() != 0UL))
  {
    visual = PS_SceneRuntime_ResolveStateSceneWaitingVisual(
      scene_model, &cursor_bounds);
  }
  else
  {
    visual = PS_SceneRuntime_ResolveShellStateWaitingVisual(
      page, focus_index, &cursor_bounds);
  }
  if ((visual == NULL) ||
      (DisplayRenderer_PublishSceneWaitingVisual(visual) == 0UL))
  {
    DisplayRenderer_ClearSceneWaitingVisual();
  }
}

static uint32_t PS_HW6_PrepareDisplayCursorBlink(uint32_t visible)
{
  display_renderer_stats_t stats;
  uint32_t ready;

  ready = DisplayRenderer_PrepareCursorBlinkFrame(visible, &stats);
  PS_HW6_DisplayOwner_ApplyRendererStats(
    PS_HW6_DISPLAY_BLINK_PATTERN_ID, &stats);
  return ready;
}

static void PS_HW6_PrepareDisplayPattern(void)
{
  display_renderer_stats_t stats;

  DisplayRenderer_PreparePattern(&stats);
  PS_HW6_DisplayOwner_ApplyRendererStats(
    PS_HW6_DISPLAY_PATTERN_ID, &stats);
}
static void PS_HW6_PrepareAudioTone(void)
{
  uint32_t frame;

  for (frame = 0UL; frame < PS_HW6_AUDIO_TONE_BUFFER_FRAMES; ++frame)
  {
    int16_t sample = ps_hw6_sine_16[frame & 15UL];
    ps_hw6_audio_tone_buffer[(frame * 2UL) + 0UL] = sample;
    ps_hw6_audio_tone_buffer[(frame * 2UL) + 1UL] = sample;
  }

  g_ps_hw6_owner_probe.audio_sample_rate_hz = PS_HW6_AUDIO_SAMPLE_RATE_HZ;
  g_ps_hw6_owner_probe.audio_tone_hz = PS_HW6_AUDIO_TONE_HZ;
  g_ps_hw6_owner_probe.audio_duration_ms = PS_HW6_AUDIO_DURATION_MS;
  g_ps_hw6_owner_probe.audio_amplitude = PS_HW6_AUDIO_AMPLITUDE;
  g_ps_hw6_owner_probe.audio_buffer_halfwords =
    PS_HW6_AUDIO_TONE_BUFFER_HALFWORDS;
}

UINT PS_HW6_OwnerServices_Init(void)
{
  UINT status;
  uint32_t i;

  (void)memset((void *)&g_ps_hw6_owner_probe, 0,
               sizeof(g_ps_hw6_owner_probe));
  g_ps_hw6_owner_probe.magic = PS_HW6_OWNER_PROBE_MAGIC;
  g_ps_hw6_owner_probe.version = PS_HW6_OWNER_PROBE_VERSION;
  g_ps_hw6_owner_probe.phase = PS_HW6_OWNER_PHASE_INIT;
  g_ps_hw6_owner_probe.power_command_send_status =
    PS_HW6_OWNER_STATUS_NOT_RUN;
  g_ps_hw6_owner_probe.display_command_send_status =
    PS_HW6_OWNER_STATUS_NOT_RUN;
  g_ps_hw6_owner_probe.display_ack_wait_status =
    PS_HW6_OWNER_STATUS_NOT_RUN;
  g_ps_hw6_owner_probe.audio_command_send_status =
    PS_HW6_OWNER_STATUS_NOT_RUN;
  g_ps_hw6_owner_probe.audio_ack_wait_status =
    PS_HW6_OWNER_STATUS_NOT_RUN;
  g_ps_hw6_owner_probe.display_init_status =
    PS_HW6_OWNER_STATUS_NOT_RUN;
  g_ps_hw6_owner_probe.display_present_status =
    PS_HW6_OWNER_STATUS_NOT_RUN;
  g_ps_hw6_owner_probe.display_ack_set_status =
    PS_HW6_OWNER_STATUS_NOT_RUN;
  g_ps_hw6_owner_probe.display_ui_status =
    PS_HW6_OWNER_STATUS_NOT_RUN;
  g_ps_hw6_owner_probe.display_shipping_clear_status =
    PS_HW6_OWNER_STATUS_NOT_RUN;
  g_ps_hw6_owner_probe.display_renderer_previous_focus_row =
    DISPLAY_RENDERER_ROW_NONE;
  g_ps_hw6_owner_probe.display_renderer_current_focus_row =
    DISPLAY_RENDERER_ROW_NONE;
  g_ps_hw6_owner_probe.display_ui_previous_focus_row =
    DISPLAY_RENDERER_ROW_NONE;
  g_ps_hw6_owner_probe.display_ui_current_focus_row =
    DISPLAY_RENDERER_ROW_NONE;
  g_ps_hw6_owner_probe.display_blink_status =
    PS_HW6_OWNER_STATUS_NOT_RUN;
  g_ps_hw6_owner_probe.display_lpbam_status =
    PS_HW6_OWNER_STATUS_NOT_RUN;
  g_ps_hw6_owner_probe.display_lpbam_ready_page =
    PS_HW6_OWNER_STATUS_NOT_RUN;
  g_ps_hw6_owner_probe.display_lpbam_prepare_status =
    PS_HW6_OWNER_STATUS_NOT_RUN;
  ps_hw6_display_lpbam_debug_force_ready_once = 0UL;
  ps_hw6_display_lpbam_prearmed = 0UL;
  ps_hw6_display_lpbam_active = 0UL;
  PS_HW6_DisplayOwner_ResetLpbamPrepareProbe();
  g_ps_hw6_owner_probe.display_lpbam_abort_status =
    PS_HW6_OWNER_STATUS_NOT_RUN;
  g_ps_hw6_owner_probe.display_lpbam_ui_invalidate_status =
    PS_HW6_OWNER_STATUS_NOT_RUN;
  g_ps_hw6_owner_probe.audio_start_status =
    PS_HW6_OWNER_STATUS_NOT_RUN;
  g_ps_hw6_owner_probe.audio_rearm_status =
    PS_HW6_OWNER_STATUS_NOT_RUN;
  g_ps_hw6_owner_probe.audio_completion_wait_status =
    PS_HW6_OWNER_STATUS_NOT_RUN;
  g_ps_hw6_owner_probe.audio_completion_callback_status =
    PS_HW6_OWNER_STATUS_NOT_RUN;
  g_ps_hw6_owner_probe.audio_stop_status =
    PS_HW6_OWNER_STATUS_NOT_RUN;
  g_ps_hw6_owner_probe.audio_ack_set_status =
    PS_HW6_OWNER_STATUS_NOT_RUN;
  g_ps_hw6_owner_probe.audio_sfx_cue_index =
    PS_HW6_OWNER_STATUS_NOT_RUN;
  g_ps_hw6_owner_probe.audio_sfx_asset_index =
    PS_HW6_OWNER_STATUS_NOT_RUN;
  g_ps_hw6_owner_probe.audio_sfx_decode_status =
    PS_HW6_OWNER_STATUS_NOT_RUN;
  g_ps_hw6_owner_probe.audio_sfx_stream_wait_status =
    PS_HW6_OWNER_STATUS_NOT_RUN;
  g_ps_hw6_owner_probe.audio_sfx_stream_wait_preempt_disable_before =
    PS_HW6_OWNER_STATUS_NOT_RUN;
  g_ps_hw6_owner_probe.audio_sfx_stream_wait_system_state_before =
    PS_HW6_OWNER_STATUS_NOT_RUN;
  g_ps_hw6_owner_probe.audio_sfx_stream_wait_current_thread_before =
    PS_HW6_OWNER_STATUS_NOT_RUN;
  g_ps_hw6_owner_probe.audio_sfx_stream_status =
    PS_HW6_OWNER_STATUS_NOT_RUN;
  g_ps_hw6_owner_probe.audio_sfx_package_backed = 0UL;
  g_ps_hw6_owner_probe.audio_sfx_source_window_read_count = 0UL;
  g_ps_hw6_owner_probe.audio_sfx_source_window_failure_count = 0UL;
  g_ps_hw6_owner_probe.audio_sfx_source_window_bytes = 0UL;
  g_ps_hw6_owner_probe.audio_sfx_source_window_status =
    PS_HW6_OWNER_STATUS_NOT_RUN;
  g_ps_hw6_owner_probe.power_driver_mr_shipping_mode_status =
    PS_HW6_OWNER_STATUS_NOT_RUN;
  g_ps_hw6_owner_probe.power_driver_fuel_gauge_prepare_status =
    PS_HW6_OWNER_STATUS_NOT_RUN;
  g_ps_hw6_owner_probe.power_driver_thermistor_config_status =
    PS_HW6_OWNER_STATUS_NOT_RUN;
  g_ps_hw6_owner_probe.power_driver_charger_profile_status =
    PS_HW6_OWNER_STATUS_NOT_RUN;
  g_ps_hw6_owner_probe.power_driver_interrupt_config_status =
    PS_HW6_OWNER_STATUS_NOT_RUN;
  g_ps_hw6_owner_probe.power_driver_software_shipping_mode_status =
    PS_HW6_OWNER_STATUS_NOT_RUN;
  g_ps_hw6_owner_probe.power_software_ship_request_count = 0UL;
  g_ps_hw6_owner_probe.power_software_ship_request_tick = 0UL;

  for (i = 0U; i < PS_HW6_OWNER_POWER_REGISTER_COUNT; ++i)
  {
    g_ps_hw6_owner_probe.power_register_address[i] =
      ps_dev_adp5360_power_register(i);
    g_ps_hw6_owner_probe.power_register_value[i] = 0UL;
    g_ps_hw6_owner_probe.power_lease_get_status[i] =
      PS_HW6_OWNER_STATUS_NOT_RUN;
    g_ps_hw6_owner_probe.power_transfer_status[i] =
      PS_HW6_OWNER_STATUS_NOT_RUN;
    g_ps_hw6_owner_probe.power_transfer_error[i] =
      PS_HW6_OWNER_STATUS_NOT_RUN;
    g_ps_hw6_owner_probe.power_lease_put_status[i] =
      PS_HW6_OWNER_STATUS_NOT_RUN;
  }

  for (i = 0U; i < PS_HW6_OWNER_CHARGER_CONFIG_REGISTER_COUNT; ++i)
  {
    g_ps_hw6_owner_probe.power_charger_config_address[i] =
      ps_dev_adp5360_charger_config_register(i);
    g_ps_hw6_owner_probe.power_charger_config_value[i] = 0UL;
    g_ps_hw6_owner_probe.power_charger_config_status[i] =
      PS_HW6_OWNER_STATUS_NOT_RUN;
    g_ps_hw6_owner_probe.power_charger_config_hal_status[i] =
      PS_HW6_OWNER_STATUS_NOT_RUN;
    g_ps_hw6_owner_probe.power_charger_config_hal_error[i] =
      PS_HW6_OWNER_STATUS_NOT_RUN;
  }

  for (i = 0U; i < PS_HW6_OWNER_INTERRUPT_REGISTER_COUNT; ++i)
  {
    g_ps_hw6_owner_probe.power_interrupt_register_address[i] =
      ps_dev_adp5360_interrupt_register(i);
    g_ps_hw6_owner_probe.power_interrupt_register_value[i] = 0UL;
    g_ps_hw6_owner_probe.power_interrupt_register_status[i] =
      PS_HW6_OWNER_STATUS_NOT_RUN;
    g_ps_hw6_owner_probe.power_interrupt_register_hal_status[i] =
      PS_HW6_OWNER_STATUS_NOT_RUN;
    g_ps_hw6_owner_probe.power_interrupt_register_hal_error[i] =
      PS_HW6_OWNER_STATUS_NOT_RUN;
  }

  for (i = 0U; i < PS_HW6_OWNER_INTERRUPT_FLAG_REGISTER_COUNT; ++i)
  {
    g_ps_hw6_owner_probe.power_interrupt_clear_address[i] =
      ps_dev_adp5360_interrupt_register(i + 2U);
    g_ps_hw6_owner_probe.power_interrupt_clear_value[i] = 0UL;
    g_ps_hw6_owner_probe.power_interrupt_clear_status[i] =
      PS_HW6_OWNER_STATUS_NOT_RUN;
    g_ps_hw6_owner_probe.power_interrupt_clear_hal_status[i] =
      PS_HW6_OWNER_STATUS_NOT_RUN;
    g_ps_hw6_owner_probe.power_interrupt_clear_hal_error[i] =
      PS_HW6_OWNER_STATUS_NOT_RUN;
  }

  HAL_GPIO_WritePin(SD_MODE_GPIO_Port, SD_MODE_Pin, GPIO_PIN_RESET);
  PS_HW6_PrepareDisplayPattern();
  PS_HW6_PrepareAudioTone();
  g_ps_hw6_owner_probe.display_driver_init_status =
    (uint32_t)PS_HW6_DisplayDriverInit(&hspi3);
  PS_HW6_UpdateDisplayDriverProbe();
  g_ps_hw6_owner_probe.audio_driver_init_status =
    (uint32_t)ps_dev_audio_init(&ps_hw6_audio,
                                &hsai_BlockA1,
                                &handle_GPDMA1_Channel3,
                                SD_MODE_GPIO_Port,
                                SD_MODE_Pin);
  PS_HW6_UpdateAudioDriverProbe();
  status = PS_HW_I2C3_Init(&hi2c3);
  g_ps_hw6_owner_probe.services_init_status = status;
  if (status == TX_SUCCESS)
  {
    ps_status_t driver_status = ps_dev_adp5360_init(
      &ps_hw6_pmic, PS_HW6_PMIC_ADDRESS_7BIT);
    g_ps_hw6_owner_probe.power_driver_init_status =
      (uint32_t)driver_status;
  }
  else
  {
    g_ps_hw6_owner_probe.power_driver_init_status =
      (uint32_t)PS_STATUS_NOT_INITIALIZED;
  }
  g_ps_hw6_owner_probe.power_driver_api_version =
    ps_hw6_pmic.api_version;
  g_ps_hw6_owner_probe.power_driver_state = ps_hw6_pmic.state;
  g_ps_hw6_owner_probe.power_driver_operation_count =
    ps_hw6_pmic.operation_count;
  g_ps_hw6_owner_probe.power_driver_last_status =
    ps_hw6_pmic.last_status;
  return status;
}

HAL_StatusTypeDef PS_HW6_PowerOwner_EnableMrShippingMode(void)
{
  ps_status_t status;

  status = ps_dev_adp5360_enable_mr_shipping_mode(&ps_hw6_pmic);
  g_ps_hw6_owner_probe.power_driver_mr_shipping_mode_status =
    (uint32_t)status;
  g_ps_hw6_owner_probe.power_driver_api_version =
    ps_hw6_pmic.api_version;
  g_ps_hw6_owner_probe.power_driver_state = ps_hw6_pmic.state;
  g_ps_hw6_owner_probe.power_driver_operation_count =
    ps_hw6_pmic.operation_count;
  g_ps_hw6_owner_probe.power_driver_last_status =
    ps_hw6_pmic.last_status;

  return (status == PS_STATUS_OK) ? HAL_OK : HAL_ERROR;
}

HAL_StatusTypeDef PS_HW6_PowerOwner_PrepareFuelGauge(void)
{
  ps_status_t status;

  status = ps_dev_adp5360_prepare_fuel_gauge(&ps_hw6_pmic);
  g_ps_hw6_owner_probe.power_driver_fuel_gauge_prepare_status =
    (uint32_t)status;
  g_ps_hw6_owner_probe.power_driver_api_version =
    ps_hw6_pmic.api_version;
  g_ps_hw6_owner_probe.power_driver_state = ps_hw6_pmic.state;
  g_ps_hw6_owner_probe.power_driver_operation_count =
    ps_hw6_pmic.operation_count;
  g_ps_hw6_owner_probe.power_driver_last_status =
    ps_hw6_pmic.last_status;

  return (status == PS_STATUS_OK) ? HAL_OK : HAL_ERROR;
}

HAL_StatusTypeDef PS_HW6_PowerOwner_ConfigureThermistor(void)
{
  ps_status_t status;

  status = ps_dev_adp5360_configure_thermistor(
    &ps_hw6_pmic,
    (uint8_t)KNOB_POWER_CHARGER_THERMISTOR_CONTROL);
  g_ps_hw6_owner_probe.power_driver_thermistor_config_status =
    (uint32_t)status;
  g_ps_hw6_owner_probe.power_driver_api_version =
    ps_hw6_pmic.api_version;
  g_ps_hw6_owner_probe.power_driver_state = ps_hw6_pmic.state;
  g_ps_hw6_owner_probe.power_driver_operation_count =
    ps_hw6_pmic.operation_count;
  g_ps_hw6_owner_probe.power_driver_last_status =
    ps_hw6_pmic.last_status;

  return (status == PS_STATUS_OK) ? HAL_OK : HAL_ERROR;
}

HAL_StatusTypeDef PS_HW6_PowerOwner_ConfigureChargerProfile(void)
{
  ps_dev_adp5360_charger_profile_t profile;
  ps_status_t status;

  profile.vbus_ilim = (uint8_t)KNOB_POWER_CHARGER_VBUS_ILIM;
  profile.termination_setting =
    (uint8_t)KNOB_POWER_CHARGER_TERMINATION_SETTING;
  profile.current_setting = (uint8_t)KNOB_POWER_CHARGER_CURRENT_SETTING;
  profile.function_setting = (uint8_t)KNOB_POWER_CHARGER_FUNCTION_SETTING;
  profile.thermistor_control =
    (uint8_t)KNOB_POWER_CHARGER_THERMISTOR_CONTROL;

  status = ps_dev_adp5360_configure_charger_profile(&ps_hw6_pmic, &profile);
  g_ps_hw6_owner_probe.power_driver_charger_profile_status =
    (uint32_t)status;
  g_ps_hw6_owner_probe.power_driver_api_version =
    ps_hw6_pmic.api_version;
  g_ps_hw6_owner_probe.power_driver_state = ps_hw6_pmic.state;
  g_ps_hw6_owner_probe.power_driver_operation_count =
    ps_hw6_pmic.operation_count;
  g_ps_hw6_owner_probe.power_driver_last_status =
    ps_hw6_pmic.last_status;

  return (status == PS_STATUS_OK) ? HAL_OK : HAL_ERROR;
}

HAL_StatusTypeDef PS_HW6_PowerOwner_ConfigurePmicInterrupts(void)
{
  ps_dev_adp5360_interrupt_profile_t profile;
  ps_status_t status;

  profile.enable1 = (uint8_t)KNOB_POWER_PMIC_INTERRUPT_ENABLE1;
  profile.enable2 = (uint8_t)KNOB_POWER_PMIC_INTERRUPT_ENABLE2;

  status = ps_dev_adp5360_configure_interrupts(&ps_hw6_pmic, &profile);
  g_ps_hw6_owner_probe.power_driver_interrupt_config_status =
    (uint32_t)status;
  g_ps_hw6_owner_probe.power_driver_api_version =
    ps_hw6_pmic.api_version;
  g_ps_hw6_owner_probe.power_driver_state = ps_hw6_pmic.state;
  g_ps_hw6_owner_probe.power_driver_operation_count =
    ps_hw6_pmic.operation_count;
  g_ps_hw6_owner_probe.power_driver_last_status =
    ps_hw6_pmic.last_status;

  return (status == PS_STATUS_OK) ? HAL_OK : HAL_ERROR;
}

HAL_StatusTypeDef PS_HW6_PowerOwner_EnterSoftwareShipmentMode(void)
{
  ps_status_t status;

  g_ps_hw6_owner_probe.power_software_ship_request_count++;
  g_ps_hw6_owner_probe.power_software_ship_request_tick =
    (uint32_t)tx_time_get();

  status = ps_dev_adp5360_enter_shipment_mode(&ps_hw6_pmic);
  g_ps_hw6_owner_probe.power_driver_software_shipping_mode_status =
    (uint32_t)status;
  g_ps_hw6_owner_probe.power_driver_api_version =
    ps_hw6_pmic.api_version;
  g_ps_hw6_owner_probe.power_driver_state = ps_hw6_pmic.state;
  g_ps_hw6_owner_probe.power_driver_operation_count =
    ps_hw6_pmic.operation_count;
  g_ps_hw6_owner_probe.power_driver_last_status =
    ps_hw6_pmic.last_status;

  return (status == PS_STATUS_OK) ? HAL_OK : HAL_ERROR;
}

HAL_StatusTypeDef PS_HW6_PowerOwner_RunSnapshot(void)
{
  static ps_dev_adp5360_power_snapshot_t snapshot;
  ps_status_t status;
  uint32_t index;

  g_ps_hw6_owner_probe.phase = PS_HW6_OWNER_PHASE_POWER;
  g_ps_hw6_owner_probe.power_complete = 0UL;
  g_ps_hw6_owner_probe.power_success = 0UL;

  status = ps_dev_adp5360_read_power_snapshot(&ps_hw6_pmic, &snapshot);
  for (index = 0U; index < PS_HW6_OWNER_POWER_REGISTER_COUNT; ++index)
  {
    g_ps_hw6_owner_probe.power_register_address[index] =
      snapshot.register_address[index];
    g_ps_hw6_owner_probe.power_register_value[index] =
      snapshot.register_value[index];
    g_ps_hw6_owner_probe.power_lease_get_status[index] =
      snapshot.acquire_tx_status;
    g_ps_hw6_owner_probe.power_transfer_status[index] =
      snapshot.register_hal_status[index];
    g_ps_hw6_owner_probe.power_transfer_error[index] =
      snapshot.register_hal_error[index];
    g_ps_hw6_owner_probe.power_lease_put_status[index] =
      snapshot.release_tx_status;
  }

  (void)ps_hw_i2c3_diagnostics(
    (uint32_t *)&g_ps_hw6_owner_probe.power_i2c_state_after,
    (uint32_t *)&g_ps_hw6_owner_probe.power_i2c_error_after);
  g_ps_hw6_owner_probe.power_driver_api_version = ps_hw6_pmic.api_version;
  g_ps_hw6_owner_probe.power_driver_state = ps_hw6_pmic.state;
  g_ps_hw6_owner_probe.power_driver_operation_count =
    ps_hw6_pmic.operation_count;
  g_ps_hw6_owner_probe.power_driver_last_status = ps_hw6_pmic.last_status;
  g_ps_hw6_owner_probe.power_driver_function_ready_mask =
    snapshot.function_ready_mask;
  g_ps_hw6_owner_probe.power_driver_read_ok_mask = snapshot.read_ok_mask;
  g_ps_hw6_owner_probe.power_driver_expected_match_mask =
    snapshot.expected_match_mask;
  g_ps_hw6_owner_probe.power_identity_match = snapshot.identity_match;
  g_ps_hw6_owner_probe.power_fault_clear = snapshot.fault_clear;
  g_ps_hw6_owner_probe.power_rails_ready = snapshot.rails_ready;
  g_ps_hw6_owner_probe.power_charger_status1 = snapshot.charger_status1;
  g_ps_hw6_owner_probe.power_charger_status2 = snapshot.charger_status2;
  g_ps_hw6_owner_probe.power_charger_status1_status =
    snapshot.charger_status1_status;
  g_ps_hw6_owner_probe.power_charger_status2_status =
    snapshot.charger_status2_status;
  g_ps_hw6_owner_probe.power_charger_status1_hal_status =
    snapshot.charger_status1_hal_status;
  g_ps_hw6_owner_probe.power_charger_status1_hal_error =
    snapshot.charger_status1_hal_error;
  g_ps_hw6_owner_probe.power_charger_status2_hal_status =
    snapshot.charger_status2_hal_status;
  g_ps_hw6_owner_probe.power_charger_status2_hal_error =
    snapshot.charger_status2_hal_error;
  g_ps_hw6_owner_probe.power_charger_thermistor_control =
    snapshot.charger_thermistor_control;
  g_ps_hw6_owner_probe.power_charger_thermistor_control_status =
    snapshot.charger_thermistor_control_status;
  g_ps_hw6_owner_probe.power_charger_thermistor_control_hal_status =
    snapshot.charger_thermistor_control_hal_status;
  g_ps_hw6_owner_probe.power_charger_thermistor_control_hal_error =
    snapshot.charger_thermistor_control_hal_error;
  g_ps_hw6_owner_probe.power_charger_monitor_read_ok_mask =
    snapshot.charger_monitor_read_ok_mask;
  g_ps_hw6_owner_probe.power_charger_config_read_ok_mask =
    snapshot.charger_config_read_ok_mask;
  for (index = 0U; index < PS_HW6_OWNER_CHARGER_CONFIG_REGISTER_COUNT;
       ++index)
  {
    g_ps_hw6_owner_probe.power_charger_config_address[index] =
      snapshot.charger_config_address[index];
    g_ps_hw6_owner_probe.power_charger_config_value[index] =
      snapshot.charger_config_value[index];
    g_ps_hw6_owner_probe.power_charger_config_status[index] =
      (uint32_t)snapshot.charger_config_status[index];
    g_ps_hw6_owner_probe.power_charger_config_hal_status[index] =
      snapshot.charger_config_hal_status[index];
    g_ps_hw6_owner_probe.power_charger_config_hal_error[index] =
      snapshot.charger_config_hal_error[index];
  }
  g_ps_hw6_owner_probe.power_interrupt_read_ok_mask =
    snapshot.interrupt_read_ok_mask;
  for (index = 0U; index < PS_HW6_OWNER_INTERRUPT_REGISTER_COUNT;
       ++index)
  {
    g_ps_hw6_owner_probe.power_interrupt_register_address[index] =
      snapshot.interrupt_register_address[index];
    g_ps_hw6_owner_probe.power_interrupt_register_value[index] =
      snapshot.interrupt_register_value[index];
    g_ps_hw6_owner_probe.power_interrupt_register_status[index] =
      (uint32_t)snapshot.interrupt_register_status[index];
    g_ps_hw6_owner_probe.power_interrupt_register_hal_status[index] =
      snapshot.interrupt_register_hal_status[index];
    g_ps_hw6_owner_probe.power_interrupt_register_hal_error[index] =
      snapshot.interrupt_register_hal_error[index];
  }
  g_ps_hw6_owner_probe.power_interrupt_clear_ok_mask =
    snapshot.interrupt_clear_ok_mask;
  for (index = 0U; index < PS_HW6_OWNER_INTERRUPT_FLAG_REGISTER_COUNT;
       ++index)
  {
    g_ps_hw6_owner_probe.power_interrupt_clear_address[index] =
      snapshot.interrupt_clear_address[index];
    g_ps_hw6_owner_probe.power_interrupt_clear_value[index] =
      snapshot.interrupt_clear_value[index];
    g_ps_hw6_owner_probe.power_interrupt_clear_status[index] =
      (uint32_t)snapshot.interrupt_clear_status[index];
    g_ps_hw6_owner_probe.power_interrupt_clear_hal_status[index] =
      snapshot.interrupt_clear_hal_status[index];
    g_ps_hw6_owner_probe.power_interrupt_clear_hal_error[index] =
      snapshot.interrupt_clear_hal_error[index];
  }
  g_ps_hw6_owner_probe.power_charger_mode = snapshot.charger_mode;
  g_ps_hw6_owner_probe.power_charger_status = snapshot.charger_status;
  g_ps_hw6_owner_probe.power_charger_charge_type =
    snapshot.charger_charge_type;
  g_ps_hw6_owner_probe.power_charger_health = snapshot.charger_health;
  g_ps_hw6_owner_probe.power_battery_status = snapshot.battery_status;
  g_ps_hw6_owner_probe.power_battery_thermal_status =
    snapshot.battery_thermal_status;
  g_ps_hw6_owner_probe.power_battery_present = snapshot.battery_present;
  g_ps_hw6_owner_probe.power_vbus_ok = snapshot.vbus_ok;
  g_ps_hw6_owner_probe.power_mcu_vbus_present =
    (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_9) == GPIO_PIN_SET) ? 1UL : 0UL;
  g_ps_hw6_owner_probe.power_vbus_agree =
    (g_ps_hw6_owner_probe.power_vbus_ok ==
     g_ps_hw6_owner_probe.power_mcu_vbus_present) ? 1UL : 0UL;
  if (g_ps_hw6_owner_probe.power_vbus_agree == 0UL)
  {
    g_ps_hw6_owner_probe.power_vbus_disagree_count++;
    g_ps_hw6_owner_probe.power_vbus_last_disagree_tick =
      (uint32_t)tx_time_get();
  }
  g_ps_hw6_owner_probe.power_battery_ok = snapshot.battery_ok;
  g_ps_hw6_owner_probe.power_charge_complete = snapshot.charge_complete;
  for (index = 0U; index < PS_DEV_ADP5360_FUEL_REGISTER_COUNT; ++index)
  {
    g_ps_hw6_owner_probe.power_fuel_register_address[index] =
      snapshot.fuel_register_address[index];
    g_ps_hw6_owner_probe.power_fuel_register_value[index] =
      snapshot.fuel_register_value[index];
    g_ps_hw6_owner_probe.power_fuel_register_status[index] =
      (uint32_t)snapshot.fuel_register_status[index];
    g_ps_hw6_owner_probe.power_fuel_register_hal_status[index] =
      snapshot.fuel_register_hal_status[index];
    g_ps_hw6_owner_probe.power_fuel_register_hal_error[index] =
      snapshot.fuel_register_hal_error[index];
  }
  g_ps_hw6_owner_probe.power_fuel_read_ok_mask = snapshot.fuel_read_ok_mask;
  g_ps_hw6_owner_probe.power_fuel_soc_percent = snapshot.fuel_soc_percent;
  g_ps_hw6_owner_probe.power_fuel_vbat_mv = snapshot.fuel_vbat_mv;
  g_ps_hw6_owner_probe.power_fuel_vbat_h = snapshot.fuel_vbat_h;
  g_ps_hw6_owner_probe.power_fuel_vbat_l = snapshot.fuel_vbat_l;
  g_ps_hw6_owner_probe.power_regulator_read_ok_mask =
    snapshot.regulator_read_ok_mask;
  g_ps_hw6_owner_probe.power_regulator_buck_cfg =
    snapshot.regulator_buck_cfg;
  g_ps_hw6_owner_probe.power_regulator_buck_output =
    snapshot.regulator_buck_output;
  g_ps_hw6_owner_probe.power_regulator_buckbst_cfg =
    snapshot.regulator_buckbst_cfg;
  g_ps_hw6_owner_probe.power_regulator_buckbst_output =
    snapshot.regulator_buckbst_output;
  g_ps_hw6_owner_probe.power_regulator_vout1_ok =
    snapshot.regulator_vout1_ok;
  g_ps_hw6_owner_probe.power_regulator_vout2_ok =
    snapshot.regulator_vout2_ok;
  g_ps_hw6_owner_probe.power_regulator_battery_ok =
    snapshot.regulator_battery_ok;
  g_ps_hw6_owner_probe.power_complete = 1UL;
  g_ps_hw6_owner_probe.power_success = (status == PS_STATUS_OK) ? 1UL : 0UL;
  return (status == PS_STATUS_OK) ? HAL_OK : HAL_ERROR;
}

HAL_StatusTypeDef PS_HW6_DisplayOwner_RunPattern(void)
{
  HAL_StatusTypeDef driver_status;

  g_ps_hw6_owner_probe.phase = PS_HW6_OWNER_PHASE_DISPLAY;
  PS_HW6_DisplayOwner_ClearLpbamReadiness(
    PS_HW6_DISPLAY_LPBAM_CLEAR_PATTERN);
  g_ps_hw6_owner_probe.display_complete = 0UL;
  g_ps_hw6_owner_probe.display_success = 0UL;
  g_ps_hw6_owner_probe.display_rtc_state = HAL_RTC_GetState(&hrtc);
  g_ps_hw6_owner_probe.display_rtc_cr = hrtc.Instance->CR;
  g_ps_hw6_owner_probe.display_spi_state_before = HAL_SPI_GetState(&hspi3);

  driver_status = PS_HW6_DisplayOwner_PresentRendererRows(
    &g_ps_hw6_owner_probe.display_init_status,
    &g_ps_hw6_owner_probe.display_present_status);

  g_ps_hw6_owner_probe.display_dma_done = LCD_FlushDMA_IsDone() ? 1UL : 0UL;
  g_ps_hw6_owner_probe.display_spi_state_after = HAL_SPI_GetState(&hspi3);
  g_ps_hw6_owner_probe.display_spi_error_after = HAL_SPI_GetError(&hspi3);
  g_ps_hw6_owner_probe.display_dma_state_after =
    PS_HW6_DisplayOwner_GetAwakeDmaState();
  g_ps_hw6_owner_probe.display_dma_error_after =
    PS_HW6_DisplayOwner_GetAwakeDmaError();
  PS_HW6_UpdateDisplayDriverProbe();
  g_ps_hw6_owner_probe.display_complete = 1UL;

  if ((driver_status == HAL_OK) &&
      (g_ps_hw6_owner_probe.display_dma_done != 0UL) &&
      (g_ps_hw6_owner_probe.display_rtc_state == HAL_RTC_STATE_READY) &&
      ((g_ps_hw6_owner_probe.display_rtc_cr & RTC_CR_COE) != 0UL) &&
      (g_ps_hw6_owner_probe.display_spi_error_after == HAL_SPI_ERROR_NONE) &&
      (g_ps_hw6_owner_probe.display_dma_error_after == HAL_DMA_ERROR_NONE))
  {
    g_ps_hw6_owner_probe.display_success = 1UL;
    return HAL_OK;
  }

  return HAL_ERROR;
}

static HAL_StatusTypeDef PS_HW6_DisplayOwner_ClearHold(
  uint32_t page,
  uint32_t lpbam_clear_reason)
{
  uint32_t clear_hal_status = (uint32_t)HAL_ERROR;
  HAL_StatusTypeDef driver_status;

  g_ps_hw6_owner_probe.phase = PS_HW6_OWNER_PHASE_DISPLAY;
  PS_HW6_DisplayOwner_ClearLpbamReadiness(
    lpbam_clear_reason);
  g_ps_hw6_owner_probe.display_complete = 0UL;
  g_ps_hw6_owner_probe.display_success = 0UL;
  g_ps_hw6_owner_probe.display_ui_request_count++;
  g_ps_hw6_owner_probe.display_ui_page = page;
  g_ps_hw6_owner_probe.display_ui_calibration_page = PS_UI_ROUTER_CAL_NONE;
  g_ps_hw6_owner_probe.display_ui_focus_index = 0UL;
  g_ps_hw6_owner_probe.display_ui_shutdown_state =
    PS_UI_ROUTER_SHUTDOWN_NONE;
  g_ps_hw6_owner_probe.display_ui_shutdown_countdown_seconds = 0UL;
  g_ps_hw6_owner_probe.display_rtc_state = HAL_RTC_GetState(&hrtc);
  g_ps_hw6_owner_probe.display_rtc_cr = hrtc.Instance->CR;
  g_ps_hw6_owner_probe.display_spi_state_before = HAL_SPI_GetState(&hspi3);

  DisplayRenderer_ClearWhite();
  g_ps_hw6_owner_probe.display_framebuffer_hash =
    DisplayRenderer_FramebufferHash();
  g_ps_hw6_owner_probe.display_black_pixels = 0UL;
  g_ps_hw6_owner_probe.display_width = DISPLAY_WIDTH;
  g_ps_hw6_owner_probe.display_height = DISPLAY_HEIGHT;
  g_ps_hw6_owner_probe.display_pattern_id = PS_HW6_DISPLAY_UI_PATTERN_ID;
  g_ps_hw6_owner_probe.display_dirty_row_count = DISPLAY_HEIGHT;
  g_ps_hw6_owner_probe.display_dirty_first_row = 1UL;
  g_ps_hw6_owner_probe.display_dirty_last_row = DISPLAY_HEIGHT;
  g_ps_hw6_owner_probe.display_renderer_primitive_id =
    DISPLAY_RENDERER_PRIMITIVE_NONE;
  g_ps_hw6_owner_probe.display_renderer_previous_focus_row =
    DISPLAY_RENDERER_ROW_NONE;
  g_ps_hw6_owner_probe.display_renderer_current_focus_row =
    DISPLAY_RENDERER_ROW_NONE;
  g_ps_hw6_owner_probe.display_ui_primitive_id =
    DISPLAY_RENDERER_PRIMITIVE_NONE;
  g_ps_hw6_owner_probe.display_ui_previous_focus_row =
    DISPLAY_RENDERER_ROW_NONE;
  g_ps_hw6_owner_probe.display_ui_current_focus_row =
    DISPLAY_RENDERER_ROW_NONE;

  driver_status = PS_HW6_DisplayOwner_ClearPanel(&clear_hal_status);
  if (driver_status == HAL_OK)
  {
    DisplayRenderer_CommitPresentedFrame();
  }
  g_ps_hw6_owner_probe.display_init_status = clear_hal_status;
  g_ps_hw6_owner_probe.display_present_status =
    PS_HW6_OWNER_STATUS_NOT_RUN;
  g_ps_hw6_owner_probe.display_dma_done = LCD_FlushDMA_IsDone() ? 1UL : 0UL;
  g_ps_hw6_owner_probe.display_spi_state_after = HAL_SPI_GetState(&hspi3);
  g_ps_hw6_owner_probe.display_spi_error_after = HAL_SPI_GetError(&hspi3);
  g_ps_hw6_owner_probe.display_dma_state_after =
    PS_HW6_DisplayOwner_GetAwakeDmaState();
  g_ps_hw6_owner_probe.display_dma_error_after =
    PS_HW6_DisplayOwner_GetAwakeDmaError();
  g_ps_hw6_owner_probe.display_ui_status = (uint32_t)driver_status;
  PS_HW6_UpdateDisplayDriverProbe();
  g_ps_hw6_owner_probe.display_complete = 1UL;

  if ((driver_status == HAL_OK) &&
      (g_ps_hw6_owner_probe.display_rtc_state == HAL_RTC_STATE_READY) &&
      ((g_ps_hw6_owner_probe.display_rtc_cr & RTC_CR_COE) != 0UL) &&
      (g_ps_hw6_owner_probe.display_spi_error_after == HAL_SPI_ERROR_NONE))
  {
    g_ps_hw6_owner_probe.display_success = 1UL;
    return HAL_OK;
  }

  return HAL_ERROR;
}

HAL_StatusTypeDef PS_HW6_DisplayOwner_ClearBootHold(void)
{
  return PS_HW6_DisplayOwner_ClearHold(
    PS_UI_ROUTER_PAGE_BOOTSTRAP,
    PS_HW6_DISPLAY_LPBAM_CLEAR_BOOT_HOLD);
}

HAL_StatusTypeDef PS_HW6_DisplayOwner_ClearForShipping(void)
{
  HAL_StatusTypeDef status = PS_HW6_DisplayOwner_ClearHold(
    PS_UI_ROUTER_PAGE_SHUTDOWN,
    PS_HW6_DISPLAY_LPBAM_CLEAR_SHIPPING);

  g_ps_hw6_owner_probe.display_shipping_clear_count++;
  g_ps_hw6_owner_probe.display_shipping_clear_status = (uint32_t)status;
  g_ps_hw6_owner_probe.display_shipping_clear_tick = (uint32_t)tx_time_get();
  return status;
}

HAL_StatusTypeDef PS_HW6_DisplayOwner_RenderUI(
  uint32_t page,
  uint32_t calibration_page,
  uint32_t focus_index,
  uint32_t shutdown_state,
  uint32_t shutdown_countdown_seconds)
{
  HAL_StatusTypeDef driver_status;
  HAL_StatusTypeDef invalidate_status;
  const ps_scene_render_model_t *scene_model = NULL;

  g_ps_hw6_owner_probe.phase = PS_HW6_OWNER_PHASE_DISPLAY;
  g_ps_hw6_owner_probe.display_complete = 0UL;
  g_ps_hw6_owner_probe.display_success = 0UL;
  if ((ps_hw6_display_lpbam_prearmed != 0UL) ||
      (ps_hw6_display_lpbam_active != 0UL))
  {
    g_ps_hw6_owner_probe.display_lpbam_ui_invalidate_count++;
    g_ps_hw6_owner_probe.display_lpbam_ui_invalidate_tick =
      (uint32_t)tx_time_get();
    g_ps_hw6_owner_probe.display_lpbam_ui_invalidate_prearmed =
      ps_hw6_display_lpbam_prearmed;
    g_ps_hw6_owner_probe.display_lpbam_ui_invalidate_active =
      ps_hw6_display_lpbam_active;
    invalidate_status = PS_HW6_DisplayOwner_AbortLpbamStop2();
    g_ps_hw6_owner_probe.display_lpbam_ui_invalidate_status =
      (uint32_t)invalidate_status;
    if (invalidate_status != HAL_OK)
    {
      g_ps_hw6_owner_probe.display_complete = 1UL;
      g_ps_hw6_owner_probe.display_ui_status =
        (uint32_t)invalidate_status;
      return invalidate_status;
    }
  }
  PS_HW6_DisplayOwner_ClearLpbamReadiness(
    PS_HW6_DISPLAY_LPBAM_CLEAR_UI_RENDER);
  g_ps_hw6_owner_probe.display_ui_request_count++;
  g_ps_hw6_owner_probe.display_ui_page = page;
  g_ps_hw6_owner_probe.display_ui_calibration_page = calibration_page;
  g_ps_hw6_owner_probe.display_ui_focus_index = focus_index;
  g_ps_hw6_owner_probe.display_ui_shutdown_state = shutdown_state;
  g_ps_hw6_owner_probe.display_ui_shutdown_countdown_seconds =
    shutdown_countdown_seconds;
  g_ps_hw6_owner_probe.display_rtc_state = HAL_RTC_GetState(&hrtc);
  g_ps_hw6_owner_probe.display_rtc_cr = hrtc.Instance->CR;
  g_ps_hw6_owner_probe.display_spi_state_before = HAL_SPI_GetState(&hspi3);

  if (((page == (uint32_t)PS_UI_ROUTER_PAGE_RUNTIME_HANDOFF) ||
       (page == (uint32_t)PS_UI_ROUTER_PAGE_INTERACTION_CUE)) &&
      (PS_SceneRuntime_StateSceneActive() != 0UL))
  {
    scene_model = PS_SceneRuntime_ResolveStateSceneRenderModel();
  }

  PS_HW6_PrepareDisplayUIPage(page,
                              calibration_page,
                              focus_index,
                              shutdown_state,
                              shutdown_countdown_seconds,
                              scene_model);
  PS_HW6_DisplayOwner_PublishStateWaitingVisual(
    page,
    g_ps_hw6_owner_probe.display_ui_current_focus_row,
    scene_model);
  driver_status = PS_HW6_DisplayOwner_PresentRendererRows(
    &g_ps_hw6_owner_probe.display_init_status,
    &g_ps_hw6_owner_probe.display_present_status);

  g_ps_hw6_owner_probe.display_dma_done = LCD_FlushDMA_IsDone() ? 1UL : 0UL;
  g_ps_hw6_owner_probe.display_spi_state_after = HAL_SPI_GetState(&hspi3);
  g_ps_hw6_owner_probe.display_spi_error_after = HAL_SPI_GetError(&hspi3);
  g_ps_hw6_owner_probe.display_dma_state_after =
    PS_HW6_DisplayOwner_GetAwakeDmaState();
  g_ps_hw6_owner_probe.display_dma_error_after =
    PS_HW6_DisplayOwner_GetAwakeDmaError();
  g_ps_hw6_owner_probe.display_ui_status = (uint32_t)driver_status;
  PS_HW6_UpdateDisplayDriverProbe();
  g_ps_hw6_owner_probe.display_complete = 1UL;

  if ((driver_status == HAL_OK) &&
      (g_ps_hw6_owner_probe.display_dma_done != 0UL) &&
      (g_ps_hw6_owner_probe.display_rtc_state == HAL_RTC_STATE_READY) &&
      ((g_ps_hw6_owner_probe.display_rtc_cr & RTC_CR_COE) != 0UL) &&
      (g_ps_hw6_owner_probe.display_spi_error_after == HAL_SPI_ERROR_NONE) &&
      (g_ps_hw6_owner_probe.display_dma_error_after == HAL_DMA_ERROR_NONE))
  {
    g_ps_hw6_owner_probe.display_ui_render_count++;
    g_ps_hw6_owner_probe.display_success = 1UL;
    PS_HW6_DisplayOwner_SnapshotSceneWaitingTimeline();
    return HAL_OK;
  }

  return HAL_ERROR;
}

HAL_StatusTypeDef PS_HW6_DisplayOwner_RenderCursorBlink(uint32_t visible)
{
  HAL_StatusTypeDef driver_status;
  uint32_t renderer_ready;

  g_ps_hw6_owner_probe.phase = PS_HW6_OWNER_PHASE_DISPLAY;
  PS_HW6_DisplayOwner_ClearLpbamReadiness(
    PS_HW6_DISPLAY_LPBAM_CLEAR_BLINK);
  g_ps_hw6_owner_probe.display_blink_request_count++;
  g_ps_hw6_owner_probe.display_blink_tick = (uint32_t)tx_time_get();
  g_ps_hw6_owner_probe.display_blink_phase = (visible == 0UL) ? 0UL : 1UL;
  g_ps_hw6_owner_probe.display_rtc_state = HAL_RTC_GetState(&hrtc);
  g_ps_hw6_owner_probe.display_rtc_cr = hrtc.Instance->CR;
  g_ps_hw6_owner_probe.display_spi_state_before = HAL_SPI_GetState(&hspi3);

  renderer_ready = PS_HW6_PrepareDisplayCursorBlink(visible);
  if (renderer_ready == 0UL)
  {
    g_ps_hw6_owner_probe.display_blink_status =
      PS_HW6_OWNER_STATUS_UNAVAILABLE;
    PS_HW6_UpdateDisplayDriverProbe();
    return HAL_ERROR;
  }

  g_ps_hw6_owner_probe.display_complete = 0UL;
  g_ps_hw6_owner_probe.display_success = 0UL;
  driver_status = PS_HW6_DisplayOwner_PresentRendererRows(
    &g_ps_hw6_owner_probe.display_init_status,
    &g_ps_hw6_owner_probe.display_present_status);

  g_ps_hw6_owner_probe.display_dma_done = LCD_FlushDMA_IsDone() ? 1UL : 0UL;
  g_ps_hw6_owner_probe.display_spi_state_after = HAL_SPI_GetState(&hspi3);
  g_ps_hw6_owner_probe.display_spi_error_after = HAL_SPI_GetError(&hspi3);
  g_ps_hw6_owner_probe.display_dma_state_after =
    PS_HW6_DisplayOwner_GetAwakeDmaState();
  g_ps_hw6_owner_probe.display_dma_error_after =
    PS_HW6_DisplayOwner_GetAwakeDmaError();
  g_ps_hw6_owner_probe.display_blink_status = (uint32_t)driver_status;
  PS_HW6_UpdateDisplayDriverProbe();
  g_ps_hw6_owner_probe.display_complete = 1UL;

  if ((driver_status == HAL_OK) &&
      (g_ps_hw6_owner_probe.display_dma_done != 0UL) &&
      (g_ps_hw6_owner_probe.display_rtc_state == HAL_RTC_STATE_READY) &&
      ((g_ps_hw6_owner_probe.display_rtc_cr & RTC_CR_COE) != 0UL) &&
      (g_ps_hw6_owner_probe.display_spi_error_after == HAL_SPI_ERROR_NONE) &&
      (g_ps_hw6_owner_probe.display_dma_error_after == HAL_DMA_ERROR_NONE))
  {
    g_ps_hw6_owner_probe.display_blink_render_count++;
    g_ps_hw6_owner_probe.display_success = 1UL;
    return HAL_OK;
  }

  return HAL_ERROR;
}

HAL_StatusTypeDef PS_HW6_DisplayOwner_RenderWaitingSequenceFrame(
  uint32_t sequence_frame)
{
  display_renderer_stats_t stats;
  HAL_StatusTypeDef driver_status;

  if (DisplayRenderer_PrepareWaitingAnimationFrame(
        sequence_frame, &stats) == 0UL)
  {
    return HAL_ERROR;
  }

  PS_HW6_DisplayOwner_ApplyRendererStats(
    stats.primitive_id, &stats);
  g_ps_hw6_owner_probe.display_complete = 0UL;
  g_ps_hw6_owner_probe.display_success = 0UL;
  driver_status = PS_HW6_DisplayOwner_PresentRendererRows(
    &g_ps_hw6_owner_probe.display_init_status,
    &g_ps_hw6_owner_probe.display_present_status);
  g_ps_hw6_owner_probe.display_dma_done = LCD_FlushDMA_IsDone() ? 1UL : 0UL;
  g_ps_hw6_owner_probe.display_spi_state_after = HAL_SPI_GetState(&hspi3);
  g_ps_hw6_owner_probe.display_spi_error_after = HAL_SPI_GetError(&hspi3);
  g_ps_hw6_owner_probe.display_dma_state_after =
    PS_HW6_DisplayOwner_GetAwakeDmaState();
  g_ps_hw6_owner_probe.display_dma_error_after =
    PS_HW6_DisplayOwner_GetAwakeDmaError();
  PS_HW6_UpdateDisplayDriverProbe();
  g_ps_hw6_owner_probe.display_complete = 1UL;

  if ((driver_status == HAL_OK) &&
      (g_ps_hw6_owner_probe.display_dma_done != 0UL) &&
      (g_ps_hw6_owner_probe.display_spi_error_after == HAL_SPI_ERROR_NONE) &&
      (g_ps_hw6_owner_probe.display_dma_error_after == HAL_DMA_ERROR_NONE))
  {
    g_ps_hw6_owner_probe.display_success = 1UL;
    return HAL_OK;
  }
  return HAL_ERROR;
}

static HAL_StatusTypeDef
PS_HW6_DisplayOwner_CompileWaitingAnimationPayload(
  const display_renderer_waiting_animation_t *animation)
{
  HAL_StatusTypeDef status;
  uint8_t (*previous_frame)[LINE_WIDTH] = ps_lpbam_display_frame_a;
  uint8_t (*target_frame)[LINE_WIDTH] = ps_lpbam_display_frame_b;
  uint16_t sequence_step;
  uint16_t sequence_frame;

  status = PS_LpbamDisplay_BeginPreparedAnimation(
    animation->candidate_rows,
    animation->candidate_row_count,
    (uint16_t)animation->sequence_frame_count,
    (uint16_t)animation->sequence_start_frame);
  if ((status == HAL_OK) &&
      (DisplayRenderer_CopyWaitingAnimationFrame(
         animation,
         animation->sequence_start_frame,
         &previous_frame[0][0],
         sizeof(ps_lpbam_display_frame_a)) == 0UL))
  {
    status = PS_LpbamDisplay_FinishPreparedAnimation();
  }

  for (sequence_step = 0U;
       (status == HAL_OK) &&
       (sequence_step < animation->sequence_frame_count);
       ++sequence_step)
  {
    uint8_t (*swap_frame)[LINE_WIDTH];

    sequence_frame = (uint16_t)(
      (animation->sequence_start_frame + sequence_step + 1UL) %
      animation->sequence_frame_count);
    if (DisplayRenderer_CopyWaitingAnimationFrame(
          animation,
          sequence_frame,
          &target_frame[0][0],
          sizeof(ps_lpbam_display_frame_b)) == 0UL)
    {
      status = PS_LpbamDisplay_FinishPreparedAnimation();
      break;
    }

    status = PS_LpbamDisplay_AppendPreparedTransition(
      previous_frame, target_frame);
    swap_frame = previous_frame;
    previous_frame = target_frame;
    target_frame = swap_frame;
  }
  if (status == HAL_OK)
  {
    status = PS_LpbamDisplay_FinishPreparedAnimation();
  }

  return status;
}

static void PS_HW6_DisplayOwner_RecordWaitingAnimation(
  const display_renderer_waiting_animation_t *animation)
{
  uint16_t index;

  g_ps_hw6_owner_probe.display_lpbam_animation_id =
    animation->animation_id;
  g_ps_hw6_owner_probe.display_lpbam_source_primitive_id =
    animation->source_primitive_id;
  g_ps_hw6_owner_probe.display_lpbam_focus_row = animation->focus_row;
  g_ps_hw6_owner_probe.display_lpbam_phase_count = animation->phase_count;
  g_ps_hw6_owner_probe.display_lpbam_sequence_frame_count =
    animation->sequence_frame_count;
  g_ps_hw6_owner_probe.display_lpbam_cadence_ms = animation->cadence_ms;
  g_ps_hw6_owner_probe.display_lpbam_current_phase =
    animation->current_phase;
  g_ps_hw6_owner_probe.display_lpbam_next_deadline_tick =
    animation->next_deadline_tick;
  g_ps_hw6_owner_probe.display_lpbam_sequence_start_frame =
    animation->sequence_start_frame;
  g_ps_hw6_owner_probe.display_lpbam_candidate_row_count =
    animation->candidate_row_count;
  g_ps_hw6_owner_probe.display_lpbam_element_count =
    animation->element_count;
  for (index = 0U; index < animation->element_count; ++index)
  {
    g_ps_hw6_owner_probe.display_lpbam_element_id[index] =
      animation->elements[index].element_id;
    g_ps_hw6_owner_probe.display_lpbam_element_phase_count[index] =
      animation->elements[index].phase_count;
  }
  for (index = 0U;
       index < DISPLAY_RENDERER_WAITING_SEQUENCE_MAX;
       ++index)
  {
    g_ps_hw6_owner_probe.display_lpbam_sequence_phase[index] =
      animation->sequence_phase[index];
  }

  g_ps_hw6_owner_probe.display_lpbam_cursor_start_row =
    animation->panel_bounds.start_row;
  g_ps_hw6_owner_probe.display_lpbam_cursor_row_count =
    animation->panel_bounds.row_count;
  g_ps_hw6_owner_probe.display_lpbam_cursor_start_column =
    animation->panel_bounds.start_column;
  g_ps_hw6_owner_probe.display_lpbam_cursor_column_count =
    animation->panel_bounds.column_count;
}

static void PS_HW6_DisplayOwner_RecordLpbamAdmission(void)
{
  g_ps_hw6_owner_probe.display_lpbam_admission_api_version =
    ps_lpbam_display_admission.api_version;
  g_ps_hw6_owner_probe.display_lpbam_admission_status =
    ps_lpbam_display_admission.status;
  g_ps_hw6_owner_probe.display_lpbam_admission_reason =
    ps_lpbam_display_admission.reason;
  g_ps_hw6_owner_probe.display_lpbam_admission_sequence_capacity =
    ps_lpbam_display_admission.sequence_capacity;
  g_ps_hw6_owner_probe.display_lpbam_admission_chunk_capacity =
    ps_lpbam_display_admission.chunk_capacity;
  g_ps_hw6_owner_probe.display_lpbam_admission_payload_used_bytes =
    ps_lpbam_display_admission.payload_used_bytes;
  g_ps_hw6_owner_probe.display_lpbam_admission_payload_capacity_bytes =
    ps_lpbam_display_admission.payload_capacity_bytes;
}

static uint32_t PS_HW6_DisplayOwner_CanUseGuaranteedFallback(
  uint32_t admission_reason)
{
  return ((admission_reason == PS_LPBAM_ADMISSION_REASON_SEQUENCE) ||
          (admission_reason == PS_LPBAM_ADMISSION_REASON_CHUNKS) ||
          (admission_reason == PS_LPBAM_ADMISSION_REASON_PAYLOAD)) ? 1UL : 0UL;
}

static HAL_StatusTypeDef
PS_HW6_DisplayOwner_CompileLpbamStop2WithAnimationPhase(
  uint32_t sequence_start_frame,
  uint32_t next_deadline_tick)
{
  const display_renderer_waiting_animation_t *preferred_animation;
  const display_renderer_waiting_animation_t *selected_animation;
  HAL_StatusTypeDef status;

  g_ps_hw6_owner_probe.phase = PS_HW6_OWNER_PHASE_DISPLAY;
  g_ps_hw6_owner_probe.display_lpbam_prepare_count++;
  g_ps_hw6_owner_probe.display_lpbam_prepare_tick =
    (uint32_t)tx_time_get();
  g_ps_hw6_owner_probe.display_lpbam_ready_render_count =
    g_ps_hw6_owner_probe.display_ui_render_count;
  PS_HW6_DisplayOwner_ResetLpbamPrepareProbe();

  if (ps_hw6_display_lpbam_debug_force_ready_once != 0UL)
  {
    ps_hw6_display_lpbam_debug_force_ready_once = 0UL;
    g_ps_hw6_owner_probe.display_lpbam_debug_force_ready_count++;
    g_ps_hw6_owner_probe.display_lpbam_ready = 1UL;
    g_ps_hw6_owner_probe.display_lpbam_ready_page =
      g_ps_ui_router_probe.current_page;
    g_ps_hw6_owner_probe.display_lpbam_status = (uint32_t)HAL_OK;
    g_ps_hw6_owner_probe.display_lpbam_prepare_status =
      (uint32_t)HAL_OK;
    return HAL_OK;
  }

  g_ps_hw6_owner_probe.display_lpbam_ready = 0UL;
  g_ps_hw6_owner_probe.display_lpbam_ready_page =
    PS_HW6_OWNER_STATUS_NOT_RUN;
  ps_hw6_display_lpbam_compiled = 0UL;

  if ((g_ps_hw6_owner_probe.display_complete == 0UL) ||
      (g_ps_hw6_owner_probe.display_success == 0UL) ||
      (g_ps_hw6_owner_probe.display_ui_status != (uint32_t)HAL_OK) ||
      (g_ps_hw6_owner_probe.display_ui_page != g_ps_ui_router_probe.current_page))
  {
    g_ps_hw6_owner_probe.display_lpbam_status = (uint32_t)HAL_ERROR;
    g_ps_hw6_owner_probe.display_lpbam_prepare_status =
      (uint32_t)HAL_ERROR;
    return HAL_ERROR;
  }

  preferred_animation = DisplayRenderer_GetWaitingAnimation(
    sequence_start_frame, next_deadline_tick);
  if ((DisplayRenderer_ValidateWaitingAnimation(preferred_animation) == 0UL) ||
      (preferred_animation->sequence_frame_count >
       PS_LPBAM_DISPLAY_SEQUENCE_MAX))
  {
    g_ps_hw6_owner_probe.display_lpbam_status =
      PS_HW6_OWNER_STATUS_UNAVAILABLE;
    g_ps_hw6_owner_probe.display_lpbam_prepare_status =
      PS_HW6_OWNER_STATUS_UNAVAILABLE;
    return HAL_ERROR;
  }

  if ((ps_hw6_display_lpbam_active != 0UL) ||
      (ps_hw6_display_lpbam_prearmed != 0UL))
  {
    status = PS_HW6_DisplayOwner_StopLpbamPlayback();
    if (status != HAL_OK)
    {
      g_ps_hw6_owner_probe.display_lpbam_status = (uint32_t)status;
      g_ps_hw6_owner_probe.display_lpbam_prepare_status =
        (uint32_t)status;
      return status;
    }
  }

  selected_animation = preferred_animation;
  status = PS_HW6_DisplayOwner_CompileWaitingAnimationPayload(
    preferred_animation);
  g_ps_hw6_owner_probe.display_lpbam_preferred_status = (uint32_t)status;
  g_ps_hw6_owner_probe.display_lpbam_preferred_reason =
    ps_lpbam_display_admission.reason;
  g_ps_hw6_owner_probe.display_lpbam_preferred_sequence_used =
    ps_lpbam_display_admission.sequence_used;
  g_ps_hw6_owner_probe.display_lpbam_preferred_chunk_used =
    ps_lpbam_display_admission.chunk_used;
  g_ps_hw6_owner_probe.display_lpbam_preferred_payload_used_bytes =
    ps_lpbam_display_admission.payload_used_bytes;
  if (status == HAL_OK)
  {
    g_ps_hw6_owner_probe.display_lpbam_compile_mode =
      PS_HW6_OWNER_LPBAM_COMPILE_PREFERRED;
  }
  else if (PS_HW6_DisplayOwner_CanUseGuaranteedFallback(
             ps_lpbam_display_admission.reason) != 0UL)
  {
    selected_animation = DisplayRenderer_GetGuaranteedWaitingAnimation(
      preferred_animation);
    g_ps_hw6_owner_probe.display_lpbam_guaranteed_attempt_count++;
    if ((DisplayRenderer_ValidateWaitingAnimation(selected_animation) != 0UL) &&
        (selected_animation->sequence_frame_count <=
         PS_LPBAM_DISPLAY_SEQUENCE_MAX))
    {
      status = PS_HW6_DisplayOwner_CompileWaitingAnimationPayload(
        selected_animation);
    }
    else
    {
      status = HAL_ERROR;
    }
    g_ps_hw6_owner_probe.display_lpbam_guaranteed_status =
      (uint32_t)status;
    if (status == HAL_OK)
    {
      g_ps_hw6_owner_probe.display_lpbam_compile_mode =
        PS_HW6_OWNER_LPBAM_COMPILE_GUARANTEED;
    }
  }

  g_ps_hw6_owner_probe.display_lpbam_fill_status = (uint32_t)status;
  PS_HW6_DisplayOwner_RecordLpbamAdmission();
  if (status != HAL_OK)
  {
    g_ps_hw6_owner_probe.display_lpbam_status = (uint32_t)status;
    g_ps_hw6_owner_probe.display_lpbam_prepare_status =
      (uint32_t)status;
    return status;
  }

  g_ps_hw6_owner_probe.display_lpbam_payload_frame_count =
    ps_lpbam_display_active_sequence_count;
  g_ps_hw6_owner_probe.display_lpbam_payload_chunk_count =
    ps_lpbam_display_active_chunk_count;
  g_ps_hw6_owner_probe.display_lpbam_payload_bytes =
    ps_lpbam_display_payload_wire_bytes;

  PS_HW6_DisplayOwner_RecordWaitingAnimation(selected_animation);
  g_ps_hw6_owner_probe.display_lpbam_compiled_sequence_frame_count =
    selected_animation->sequence_frame_count;
  g_ps_hw6_owner_probe.display_lpbam_compiled_sequence_start_frame =
    selected_animation->sequence_start_frame;
  for (uint16_t compiled_frame = 0U;
       compiled_frame < DISPLAY_RENDERER_WAITING_SEQUENCE_MAX;
       ++compiled_frame)
  {
    g_ps_hw6_owner_probe.display_lpbam_compiled_sequence_phase[
      compiled_frame] = selected_animation->sequence_phase[compiled_frame];
  }
  if (DisplayRenderer_SelectWaitingAnimation(selected_animation) == 0UL)
  {
    g_ps_hw6_owner_probe.display_lpbam_status = (uint32_t)HAL_ERROR;
    g_ps_hw6_owner_probe.display_lpbam_prepare_status =
      (uint32_t)HAL_ERROR;
    return HAL_ERROR;
  }

  status = PS_LpbamDisplayQueue_Build();
  if (status != HAL_OK)
  {
    g_ps_hw6_owner_probe.display_lpbam_status = (uint32_t)status;
    g_ps_hw6_owner_probe.display_lpbam_prepare_status =
      (uint32_t)status;
    return status;
  }

  ps_hw6_display_lpbam_compiled = 1UL;
  ps_hw6_display_lpbam_compiled_cadence_ms = selected_animation->cadence_ms;
  ps_hw6_display_lpbam_compiled_page = g_ps_ui_router_probe.current_page;
  ps_hw6_display_lpbam_compiled_render_count =
    g_ps_hw6_owner_probe.display_ui_render_count;
  g_ps_hw6_owner_probe.display_lpbam_status =
    PS_HW6_OWNER_STATUS_NOT_RUN;
  g_ps_hw6_owner_probe.display_lpbam_prepare_status =
    (uint32_t)HAL_OK;
  return HAL_OK;
}

HAL_StatusTypeDef PS_HW6_DisplayOwner_PrearmCompiledLpbamStop2(void)
{
  HAL_StatusTypeDef status;

  if ((ps_hw6_display_lpbam_compiled == 0UL) ||
      (ps_hw6_display_lpbam_prearmed != 0UL) ||
      (ps_hw6_display_lpbam_active != 0UL) ||
      (ps_hw6_display_lpbam_compiled_page !=
       g_ps_ui_router_probe.current_page) ||
      (ps_hw6_display_lpbam_compiled_render_count !=
       g_ps_hw6_owner_probe.display_ui_render_count))
  {
    return HAL_ERROR;
  }

  status = PS_HW6_DisplayOwner_EnableLpbamAutonomousClocks();
  g_ps_hw6_owner_probe.display_lpbam_clock_status = (uint32_t)status;
  if (status == HAL_OK)
  {
    status = PS_HW6_DisplayOwner_ConfigLpbamLptim(
      ps_hw6_display_lpbam_compiled_cadence_ms);
  }
  if (status == HAL_OK)
  {
    status = PS_HW6_DisplayOwner_PrearmLpbamPlayback();
  }

  g_ps_hw6_owner_probe.display_lpbam_prearm_status = (uint32_t)status;
  if (status != HAL_OK)
  {
    ps_hw6_display_lpbam_compiled = 0UL;
    if ((ps_hw6_display_lpbam_active != 0UL) ||
        (ps_hw6_display_lpbam_prearmed != 0UL))
    {
      (void)PS_HW6_DisplayOwner_StopLpbamPlayback();
    }
    else if ((g_ps_hw6_owner_probe.display_lpbam_clock_status ==
              (uint32_t)HAL_OK) ||
             (g_ps_hw6_owner_probe.display_lpbam_spi_autocr_after !=
              PS_HW6_OWNER_STATUS_NOT_RUN))
    {
      HAL_StatusTypeDef lptim_restore_status =
        PS_HW6_DisplayOwner_RestoreLptimAfterLpbam();
      HAL_StatusTypeDef restore_status =
        PS_HW6_DisplayOwner_RestoreDisplaySpiAfterLpbam();
      g_ps_hw6_owner_probe.display_lpbam_lptim_restore_status =
        (uint32_t)lptim_restore_status;
      g_ps_hw6_owner_probe.display_lpbam_restore_status =
        (uint32_t)restore_status;
    }
    g_ps_hw6_owner_probe.display_lpbam_status = (uint32_t)status;
    return status;
  }

  g_ps_hw6_owner_probe.display_lpbam_ready = 1UL;
  g_ps_hw6_owner_probe.display_lpbam_ready_page =
    g_ps_ui_router_probe.current_page;
  g_ps_hw6_owner_probe.display_lpbam_ready_render_count =
    g_ps_hw6_owner_probe.display_ui_render_count;
  g_ps_hw6_owner_probe.display_lpbam_status = (uint32_t)HAL_OK;
  return HAL_OK;
}

HAL_StatusTypeDef PS_HW6_DisplayOwner_CompileLpbamStop2ForAnimationPhase(
  uint32_t sequence_start_frame,
  uint32_t next_deadline_tick)
{
  return PS_HW6_DisplayOwner_CompileLpbamStop2WithAnimationPhase(
    sequence_start_frame,
    next_deadline_tick);
}

HAL_StatusTypeDef PS_HW6_DisplayOwner_PrepareLpbamStop2(void)
{
  HAL_StatusTypeDef status =
    PS_HW6_DisplayOwner_CompileLpbamStop2WithAnimationPhase(0UL, 0UL);

  return (status == HAL_OK) ?
    PS_HW6_DisplayOwner_PrearmCompiledLpbamStop2() : status;
}

HAL_StatusTypeDef PS_HW6_DisplayOwner_PrepareLpbamStop2ForAnimationPhase(
  uint32_t sequence_start_frame,
  uint32_t next_deadline_tick)
{
  HAL_StatusTypeDef status =
    PS_HW6_DisplayOwner_CompileLpbamStop2WithAnimationPhase(
      sequence_start_frame,
      next_deadline_tick);

  return (status == HAL_OK) ?
    PS_HW6_DisplayOwner_PrearmCompiledLpbamStop2() : status;
}

void PS_HW6_DisplayOwner_DebugForceNextLpbamReady(void)
{
  ps_hw6_display_lpbam_debug_force_ready_once = 1UL;
}

HAL_StatusTypeDef PS_HW6_DisplayOwner_AbortLpbamStop2(void)
{
  HAL_StatusTypeDef status;

  g_ps_hw6_owner_probe.phase = PS_HW6_OWNER_PHASE_DISPLAY;
  g_ps_hw6_owner_probe.display_lpbam_abort_count++;
  g_ps_hw6_owner_probe.display_lpbam_abort_tick =
    (uint32_t)tx_time_get();
  status = PS_HW6_DisplayOwner_StopLpbamPlayback();
  PS_HW6_DisplayOwner_ClearLpbamReadiness(
    PS_HW6_DISPLAY_LPBAM_CLEAR_ABORT);
  g_ps_hw6_owner_probe.display_lpbam_abort_status =
    (uint32_t)status;
  return status;
}

HAL_StatusTypeDef PS_HW6_DisplayOwner_AbortLpbamStop2AndResume(void)
{
  ps_lpbam_display_progress_t progress;
  HAL_StatusTypeDef snapshot_status;
  HAL_StatusTypeDef abort_status;
  HAL_StatusTypeDef render_status = HAL_ERROR;
  uint32_t preferred_map_status = 0UL;
  uint32_t preferred_sequence_frame = 0UL;
  uint32_t preferred_sequence_count = 0UL;
  uint32_t phase = 0UL;

  g_ps_hw6_owner_probe.phase = PS_HW6_OWNER_PHASE_DISPLAY;
  g_ps_hw6_owner_probe.display_lpbam_abort_count++;
  g_ps_hw6_owner_probe.display_lpbam_abort_tick =
    (uint32_t)tx_time_get();
  g_ps_hw6_owner_probe.display_lpbam_wake_snapshot_status =
    PS_HW6_OWNER_STATUS_NOT_RUN;
  g_ps_hw6_owner_probe.display_lpbam_wake_progress_state =
    PS_LPBAM_DISPLAY_PROGRESS_INVALID;
  g_ps_hw6_owner_probe.display_lpbam_wake_sequence_index = 0UL;
  g_ps_hw6_owner_probe.display_lpbam_wake_sequence_frame = 0UL;
  g_ps_hw6_owner_probe.display_lpbam_wake_phase = 0UL;
  g_ps_hw6_owner_probe.display_lpbam_wake_preferred_sequence_frame = 0UL;
  g_ps_hw6_owner_probe.display_lpbam_wake_preferred_sequence_count = 0UL;
  g_ps_hw6_owner_probe.display_lpbam_wake_preferred_map_status =
    PS_HW6_OWNER_STATUS_NOT_RUN;
  g_ps_hw6_owner_probe.display_lpbam_wake_node_index = 0UL;
  g_ps_hw6_owner_probe.display_lpbam_wake_node_count = 0UL;
  g_ps_hw6_owner_probe.display_lpbam_wake_render_status =
    PS_HW6_OWNER_STATUS_NOT_RUN;
  g_ps_hw6_owner_probe.display_lpbam_wake_lptim_count =
    hlptim1.Instance->CNT;
  g_ps_hw6_owner_probe.display_lpbam_wake_lptim_period =
    hlptim1.Instance->ARR + 1UL;
  snapshot_status = PS_LpbamDisplayQueue_SnapshotProgress(
    &handle_LPDMA1_Channel0, &progress);
  g_ps_hw6_owner_probe.display_lpbam_wake_snapshot_status =
    (uint32_t)snapshot_status;

  if (snapshot_status == HAL_OK)
  {
    phase = g_ps_hw6_owner_probe.display_lpbam_sequence_phase[
      progress.sequence_frame];
    g_ps_hw6_owner_probe.display_lpbam_wake_progress_state =
      progress.state;
    g_ps_hw6_owner_probe.display_lpbam_wake_sequence_index =
      progress.sequence_index;
    g_ps_hw6_owner_probe.display_lpbam_wake_sequence_frame =
      progress.sequence_frame;
    g_ps_hw6_owner_probe.display_lpbam_wake_phase = phase;
    g_ps_hw6_owner_probe.display_lpbam_wake_node_index =
      progress.node_index;
    g_ps_hw6_owner_probe.display_lpbam_wake_node_count =
      progress.node_count;
  }

  abort_status = PS_HW6_DisplayOwner_StopLpbamPlayback();
  if ((snapshot_status == HAL_OK) && (abort_status == HAL_OK))
  {
    render_status = PS_HW6_DisplayOwner_RenderWaitingSequenceFrame(
      progress.sequence_frame);
    if (render_status == HAL_OK)
    {
      preferred_map_status =
        DisplayRenderer_ResumePreferredWaitingAnimation(
          progress.sequence_frame,
          &preferred_sequence_frame,
          &preferred_sequence_count);
      if (preferred_map_status != 0UL)
      {
        const display_renderer_waiting_animation_t *preferred_animation =
          DisplayRenderer_GetSelectedWaitingAnimation();

        if (preferred_animation != NULL)
        {
          PS_HW6_DisplayOwner_RecordWaitingAnimation(preferred_animation);
        }
        else
        {
          preferred_map_status = 0UL;
        }
      }
    }
  }
  PS_HW6_DisplayOwner_ClearLpbamReadiness(
    PS_HW6_DISPLAY_LPBAM_CLEAR_ABORT);
  g_ps_hw6_owner_probe.display_lpbam_wake_render_status =
    (uint32_t)render_status;
  g_ps_hw6_owner_probe.display_lpbam_wake_preferred_sequence_frame =
    preferred_sequence_frame;
  g_ps_hw6_owner_probe.display_lpbam_wake_preferred_sequence_count =
    preferred_sequence_count;
  g_ps_hw6_owner_probe.display_lpbam_wake_preferred_map_status =
    (preferred_map_status != 0UL) ? (uint32_t)HAL_OK : (uint32_t)HAL_ERROR;
  g_ps_hw6_owner_probe.display_lpbam_abort_status =
    (uint32_t)abort_status;

  return ((snapshot_status == HAL_OK) && (abort_status == HAL_OK) &&
          (render_status == HAL_OK) &&
          (preferred_map_status != 0UL)) ? HAL_OK : HAL_ERROR;
}

static int32_t PS_HW6_AudioClampPcm(int32_t sample)
{
  if (sample > 32767)
  {
    return 32767;
  }
  if (sample < -32768)
  {
    return -32768;
  }
  return sample;
}

static HAL_StatusTypeDef PS_HW6_AudioStreamStartPrefetch(
  ps_hw6_audio_stream_decoder_t *decoder)
{
  const ps_egg_state_loader_audio_cue_t *cue;
  uint32_t next_offset;
  uint32_t window_length;
  uint32_t window_index;
  UINT read_status;

  if ((decoder == NULL) || (decoder->cue == NULL))
  {
    return HAL_ERROR;
  }
  cue = decoder->cue;
  if (cue->package_backed == 0UL)
  {
    return HAL_OK;
  }
  if ((cue->package_backed != 1UL) ||
      (decoder->source_window_valid == 0UL) ||
      (decoder->source_prefetch_pending != 0UL) ||
      (decoder->source_window_offset >
       (UINT32_MAX - decoder->source_window_length)))
  {
    return HAL_ERROR;
  }

  next_offset = decoder->source_window_offset +
    decoder->source_window_length;
  if (next_offset >= cue->adpcm_size)
  {
    return HAL_OK;
  }
  if (cue->package_offset > (UINT32_MAX - next_offset))
  {
    return HAL_ERROR;
  }

  window_length = cue->adpcm_size - next_offset;
  if (window_length > PS_PACKAGE_READER_WINDOW_BYTES)
  {
    window_length = PS_PACKAGE_READER_WINDOW_BYTES;
  }
  window_index = (decoder->source_window_index + 1UL) %
    PS_HW6_AUDIO_STREAM_SOURCE_WINDOW_COUNT;
  read_status = PS_HW6_RTOS_BeginAudioPackageWindowRead(
    cue->package_offset + next_offset,
    ps_hw6_audio_stream_source_windows[window_index],
    window_length);
  decoder->source_window_last_status = (uint32_t)read_status;
  if (read_status != TX_SUCCESS)
  {
    decoder->source_window_failure_count++;
    return HAL_ERROR;
  }

  decoder->source_prefetch_offset = next_offset;
  decoder->source_prefetch_length = window_length;
  decoder->source_prefetch_index = window_index;
  decoder->source_prefetch_pending = 1UL;
  decoder->source_prefetch_start_count++;
  return HAL_OK;
}

static HAL_StatusTypeDef PS_HW6_AudioStreamFinishPrefetch(
  ps_hw6_audio_stream_decoder_t *decoder,
  uint32_t wait_for_completion)
{
  UINT read_status;

  if ((decoder == NULL) || (decoder->source_prefetch_pending == 0UL))
  {
    return HAL_ERROR;
  }

  read_status = (wait_for_completion != 0UL) ?
    PS_HW6_RTOS_WaitFinishAudioPackageWindowRead() :
    PS_HW6_RTOS_TryFinishAudioPackageWindowRead();
  decoder->source_window_last_status = (uint32_t)read_status;
  if (read_status != TX_SUCCESS)
  {
    decoder->source_window_failure_count++;
    return HAL_ERROR;
  }

  decoder->source_prefetch_pending = 0UL;
  decoder->source_prefetch_complete_count++;
  decoder->source_window_read_count++;
  decoder->source_window_bytes += decoder->source_prefetch_length;
  return HAL_OK;
}

static HAL_StatusTypeDef PS_HW6_AudioStreamDrainPrefetch(
  ps_hw6_audio_stream_decoder_t *decoder)
{
  HAL_StatusTypeDef status = HAL_OK;

  if (decoder == NULL)
  {
    return HAL_ERROR;
  }
  if (decoder->source_prefetch_pending != 0UL)
  {
    status = PS_HW6_AudioStreamFinishPrefetch(decoder, 1UL);
  }
  decoder->source_prefetch_cleanup_status =
    (status == HAL_OK) ? (uint32_t)TX_SUCCESS :
    decoder->source_window_last_status;
  return status;
}

static HAL_StatusTypeDef PS_HW6_AudioStreamReadByte(
  ps_hw6_audio_stream_decoder_t *decoder,
  uint32_t source_offset,
  uint8_t *value)
{
  const ps_egg_state_loader_audio_cue_t *cue;

  if ((decoder == NULL) || (decoder->cue == NULL) || (value == NULL))
  {
    return HAL_ERROR;
  }
  cue = decoder->cue;
  if (source_offset >= cue->adpcm_size)
  {
    return HAL_ERROR;
  }
  if (cue->package_backed == 0UL)
  {
    if (cue->adpcm == NULL)
    {
      return HAL_ERROR;
    }
    *value = cue->adpcm[source_offset];
    return HAL_OK;
  }
  if (cue->package_backed != 1UL)
  {
    return HAL_ERROR;
  }

  if (decoder->source_window_valid == 0UL)
  {
    uint32_t window_length = cue->adpcm_size - source_offset;
    UINT read_status;

    if ((cue->package_offset > (UINT32_MAX - source_offset)) ||
        (window_length == 0UL))
    {
      return HAL_ERROR;
    }
    if (window_length > PS_PACKAGE_READER_WINDOW_BYTES)
    {
      window_length = PS_PACKAGE_READER_WINDOW_BYTES;
    }
    decoder->source_window_index = 0UL;
    read_status = PS_HW6_RTOS_ReadAudioPackageWindow(
      cue->package_offset + source_offset,
      ps_hw6_audio_stream_source_windows[decoder->source_window_index],
      window_length);
    decoder->source_window_last_status = (uint32_t)read_status;
    if (read_status != TX_SUCCESS)
    {
      decoder->source_window_failure_count++;
      return HAL_ERROR;
    }
    decoder->source_window_offset = source_offset;
    decoder->source_window_length = window_length;
    decoder->source_window_valid = 1UL;
    decoder->source_window_read_count++;
    decoder->source_window_bytes += window_length;
  }
  else if ((source_offset < decoder->source_window_offset) ||
           ((source_offset - decoder->source_window_offset) >=
           decoder->source_window_length))
  {
    if ((decoder->source_prefetch_pending == 0UL) ||
        (source_offset < decoder->source_prefetch_offset) ||
        ((source_offset - decoder->source_prefetch_offset) >=
         decoder->source_prefetch_length))
    {
      decoder->source_window_last_status = (uint32_t)TX_NOT_AVAILABLE;
      decoder->source_window_failure_count++;
      decoder->source_prefetch_miss_count++;
      return HAL_ERROR;
    }
    if (PS_HW6_AudioStreamFinishPrefetch(decoder, 0UL) != HAL_OK)
    {
      decoder->source_prefetch_miss_count++;
      return HAL_ERROR;
    }
    decoder->source_window_offset = decoder->source_prefetch_offset;
    decoder->source_window_length = decoder->source_prefetch_length;
    decoder->source_window_index = decoder->source_prefetch_index;
    if (PS_HW6_AudioStreamStartPrefetch(decoder) != HAL_OK)
    {
      return HAL_ERROR;
    }
  }

  *value = ps_hw6_audio_stream_source_windows[decoder->source_window_index][
    source_offset - decoder->source_window_offset];
  return HAL_OK;
}

static HAL_StatusTypeDef PS_HW6_AudioStreamLoadBlock(
  ps_hw6_audio_stream_decoder_t *decoder)
{
  const ps_egg_state_loader_audio_cue_t *cue;
  uint32_t source_offset;
  uint32_t block_sample_count;
  uint32_t packed_count;
  uint8_t header[PS_HW6_AUDIO_ADPCM_BLOCK_HEADER_BYTES];
  uint32_t index;

  if ((decoder == NULL) || (decoder->cue == NULL) ||
      (decoder->block_index >= decoder->cue->block_count))
  {
    return HAL_ERROR;
  }

  cue = decoder->cue;
  source_offset = decoder->source_offset;
  if ((source_offset + PS_HW6_AUDIO_ADPCM_BLOCK_HEADER_BYTES) >
      cue->adpcm_size)
  {
    return HAL_ERROR;
  }
  for (index = 0UL; index < sizeof(header); ++index)
  {
    if (PS_HW6_AudioStreamReadByte(decoder, source_offset + index,
                                   &header[index]) != HAL_OK)
    {
      return HAL_ERROR;
    }
  }
  decoder->predictor = (int32_t)(int16_t)(
    ((uint16_t)header[0]) | ((uint16_t)header[1] << 8U));
  decoder->step_index = (int32_t)header[2];
  block_sample_count = (uint32_t)header[4] |
    ((uint32_t)header[5] << 8U);
  if ((header[3] != 0U) ||
      (decoder->step_index > 88) || (block_sample_count == 0UL) ||
      (block_sample_count > PS_EGG_STATE_LOADER_AUDIO_BLOCK_SAMPLES) ||
      ((decoder->decoded_sample_count + block_sample_count) >
       cue->sample_count))
  {
    return HAL_ERROR;
  }

  packed_count = ((block_sample_count - 1UL) + 1UL) / 2UL;
  decoder->block_payload_offset = source_offset + 6UL;
  decoder->source_offset = decoder->block_payload_offset + packed_count;
  if (decoder->source_offset > cue->adpcm_size)
  {
    return HAL_ERROR;
  }
  decoder->block_sample_count = block_sample_count;
  decoder->block_sample_index = 0UL;
  decoder->block_index++;
  return HAL_OK;
}

static HAL_StatusTypeDef PS_HW6_AudioStreamDecoderInit(
  ps_hw6_audio_stream_decoder_t *decoder,
  const ps_egg_state_loader_audio_cue_t *cue)
{
  if ((decoder == NULL) || (cue == NULL) ||
      ((cue->package_backed == 0UL) && (cue->adpcm == NULL)) ||
      ((cue->package_backed == 1UL) && (cue->package_offset == 0UL)) ||
      (cue->package_backed > 1UL) ||
      (cue->adpcm_size == 0UL) || (cue->sample_count == 0UL) ||
      (cue->sample_count > PS_EGG_STATE_LOADER_AUDIO_SAMPLE_MAX) ||
      (cue->block_count == 0UL))
  {
    return HAL_ERROR;
  }

  (void)memset(decoder, 0, sizeof(*decoder));
  decoder->cue = cue;
  decoder->source_window_last_status = PS_HW6_OWNER_STATUS_NOT_RUN;
  decoder->source_prefetch_cleanup_status = PS_HW6_OWNER_STATUS_NOT_RUN;
  return HAL_OK;
}

static HAL_StatusTypeDef PS_HW6_AudioStreamDecodeFrames(
  ps_hw6_audio_stream_decoder_t *decoder,
  int16_t *destination,
  uint32_t frame_capacity,
  uint32_t *decoded_frames,
  uint32_t *source_finished)
{
  const ps_egg_state_loader_audio_cue_t *cue;
  uint32_t output_frame = 0UL;

  if ((decoder == NULL) || (decoder->cue == NULL) ||
      (destination == NULL) || (frame_capacity == 0UL) ||
      (decoded_frames == NULL) || (source_finished == NULL))
  {
    return HAL_ERROR;
  }

  cue = decoder->cue;
  (void)memset(destination, 0,
               frame_capacity * 2UL * sizeof(destination[0]));
  while ((output_frame < frame_capacity) &&
         (decoder->decoded_sample_count < cue->sample_count))
  {
    int32_t sample;

    if (decoder->block_sample_index >= decoder->block_sample_count)
    {
      if (PS_HW6_AudioStreamLoadBlock(decoder) != HAL_OK)
      {
        return HAL_ERROR;
      }
    }

    if (decoder->block_sample_index == 0UL)
    {
      sample = decoder->predictor;
    }
    else
    {
      uint32_t nibble_index = decoder->block_sample_index - 1UL;
      uint8_t packed;
      uint32_t nibble;
      int32_t step = ps_hw6_ima_step_table[decoder->step_index];
      int32_t delta = step >> 3U;

      if (PS_HW6_AudioStreamReadByte(
            decoder,
            decoder->block_payload_offset + (nibble_index / 2UL),
            &packed) != HAL_OK)
      {
        return HAL_ERROR;
      }
      nibble = ((nibble_index & 1UL) == 0UL) ?
        ((uint32_t)packed & 0x0FUL) : ((uint32_t)packed >> 4U);
      if ((nibble & 4UL) != 0UL)
      {
        delta += step;
      }
      if ((nibble & 2UL) != 0UL)
      {
        delta += step >> 1U;
      }
      if ((nibble & 1UL) != 0UL)
      {
        delta += step >> 2U;
      }
      decoder->predictor = ((nibble & 8UL) != 0UL) ?
        (decoder->predictor - delta) : (decoder->predictor + delta);
      decoder->predictor = PS_HW6_AudioClampPcm(decoder->predictor);
      decoder->step_index +=
        (int32_t)ps_hw6_ima_index_table[nibble & 7UL];
      if (decoder->step_index < 0)
      {
        decoder->step_index = 0;
      }
      else if (decoder->step_index > 88)
      {
        decoder->step_index = 88;
      }
      sample = decoder->predictor;
    }

    sample = PS_HW6_AudioClampPcm(
      (sample * (int32_t)cue->volume) / 255);
    destination[output_frame * 2UL] = (int16_t)sample;
    destination[(output_frame * 2UL) + 1UL] = (int16_t)sample;
    decoder->block_sample_index++;
    decoder->decoded_sample_count++;
    output_frame++;
  }

  *decoded_frames = output_frame;
  *source_finished = 0UL;
  if (decoder->decoded_sample_count == cue->sample_count)
  {
    if ((decoder->block_index != cue->block_count) ||
        (decoder->source_offset != cue->adpcm_size) ||
        (decoder->block_sample_index != decoder->block_sample_count))
    {
      return HAL_ERROR;
    }
    *source_finished = 1UL;
  }
  return HAL_OK;
}

HAL_StatusTypeDef PS_HW6_AudioOwner_VerifyIdle(void)
{
  ps_dev_audio_play_result_t result;
  ps_status_t driver_status;

  g_ps_hw6_owner_probe.phase = PS_HW6_OWNER_PHASE_AUDIO;
  g_ps_hw6_owner_probe.audio_complete = 0UL;
  g_ps_hw6_owner_probe.audio_success = 0UL;

  driver_status = ps_dev_audio_verify_idle(&ps_hw6_audio, &result);
  g_ps_hw6_owner_probe.audio_sd_state_after = result.sd_state_after;
  g_ps_hw6_owner_probe.audio_sai_state_after = result.sai_state_after;
  g_ps_hw6_owner_probe.audio_sai_error_after = result.sai_error_after;
  g_ps_hw6_owner_probe.audio_dma_state_after = result.dma_state_after;
  g_ps_hw6_owner_probe.audio_dma_error_after = result.dma_error_after;
  PS_HW6_UpdateAudioDriverProbe();
  g_ps_hw6_owner_probe.audio_complete = 1UL;
  if (driver_status == PS_STATUS_OK)
  {
    g_ps_hw6_owner_probe.audio_success = 1UL;
    return HAL_OK;
  }
  return HAL_ERROR;
}

HAL_StatusTypeDef PS_HW6_AudioOwner_MarkPostStopResume(void)
{
  ps_status_t driver_status;

  driver_status = ps_dev_audio_mark_post_stop_resume(&ps_hw6_audio);
  PS_HW6_UpdateAudioDriverProbe();
  return (driver_status == PS_STATUS_OK) ? HAL_OK : HAL_ERROR;
}

HAL_StatusTypeDef PS_HW6_AudioOwner_RunTone(void)
{
  ps_dev_audio_play_result_t result;
  ps_status_t driver_status;

  g_ps_hw6_owner_probe.phase = PS_HW6_OWNER_PHASE_AUDIO;
  g_ps_hw6_owner_probe.audio_complete = 0UL;
  g_ps_hw6_owner_probe.audio_success = 0UL;

  PS_HW6_PrepareAudioTone();
  driver_status = ps_dev_audio_play_dma(
    &ps_hw6_audio,
    ps_hw6_audio_tone_buffer,
    PS_HW6_AUDIO_TONE_BUFFER_HALFWORDS,
    PS_HW6_AUDIO_AMP_SETTLE_TICKS,
    PS_HW6_AUDIO_DURATION_TICKS + PS_HW6_AUDIO_COMPLETION_MARGIN_TICKS,
    4096000UL,
    &result);

  g_ps_hw6_owner_probe.audio_sai_kernel_hz = result.sai_kernel_hz;
  g_ps_hw6_owner_probe.audio_sd_state_before = result.sd_state_before;
  g_ps_hw6_owner_probe.audio_sd_state_enabled = result.sd_state_enabled;
  g_ps_hw6_owner_probe.audio_rearm_status = result.rearm_status;
  g_ps_hw6_owner_probe.audio_start_status = result.start_hal_status;
  g_ps_hw6_owner_probe.audio_completion_wait_status =
    result.completion_wait_status;
  g_ps_hw6_owner_probe.audio_completion_callback_status =
    result.completion_callback_status;
  g_ps_hw6_owner_probe.audio_pre_cleanup_dma_irq_delta =
    result.pre_cleanup_dma_irq_delta;
  g_ps_hw6_owner_probe.audio_pre_cleanup_tx_callback_delta =
    result.pre_cleanup_tx_callback_delta;
  g_ps_hw6_owner_probe.audio_pre_cleanup_error_callback_delta =
    result.pre_cleanup_error_callback_delta;
  g_ps_hw6_owner_probe.audio_pre_cleanup_sai_kernel_hz =
    result.pre_cleanup_sai_kernel_hz;
  g_ps_hw6_owner_probe.audio_pre_cleanup_sai_state =
    result.pre_cleanup_sai_state;
  g_ps_hw6_owner_probe.audio_pre_cleanup_sai_error =
    result.pre_cleanup_sai_error;
  g_ps_hw6_owner_probe.audio_pre_cleanup_sai_cr1 =
    result.pre_cleanup_sai_cr1;
  g_ps_hw6_owner_probe.audio_pre_cleanup_sai_cr2 =
    result.pre_cleanup_sai_cr2;
  g_ps_hw6_owner_probe.audio_pre_cleanup_sai_frcr =
    result.pre_cleanup_sai_frcr;
  g_ps_hw6_owner_probe.audio_pre_cleanup_sai_slotr =
    result.pre_cleanup_sai_slotr;
  g_ps_hw6_owner_probe.audio_pre_cleanup_sai_imr =
    result.pre_cleanup_sai_imr;
  g_ps_hw6_owner_probe.audio_pre_cleanup_sai_sr =
    result.pre_cleanup_sai_sr;
  g_ps_hw6_owner_probe.audio_pre_cleanup_sai_gcr =
    result.pre_cleanup_sai_gcr;
  g_ps_hw6_owner_probe.audio_pre_cleanup_dma_state =
    result.pre_cleanup_dma_state;
  g_ps_hw6_owner_probe.audio_pre_cleanup_dma_error =
    result.pre_cleanup_dma_error;
  g_ps_hw6_owner_probe.audio_pre_cleanup_dma_ccr =
    result.pre_cleanup_dma_ccr;
  g_ps_hw6_owner_probe.audio_pre_cleanup_dma_csr =
    result.pre_cleanup_dma_csr;
  g_ps_hw6_owner_probe.audio_pre_cleanup_dma_cbr1 =
    result.pre_cleanup_dma_cbr1;
  g_ps_hw6_owner_probe.audio_pre_cleanup_dma_ctr1 =
    result.pre_cleanup_dma_ctr1;
  g_ps_hw6_owner_probe.audio_pre_cleanup_dma_ctr2 =
    result.pre_cleanup_dma_ctr2;
  g_ps_hw6_owner_probe.audio_pre_cleanup_dma_csar =
    result.pre_cleanup_dma_csar;
  g_ps_hw6_owner_probe.audio_pre_cleanup_dma_cdar =
    result.pre_cleanup_dma_cdar;
  g_ps_hw6_owner_probe.audio_pre_cleanup_dma_cllr =
    result.pre_cleanup_dma_cllr;
  g_ps_hw6_owner_probe.audio_stop_status = result.stop_hal_status;
  g_ps_hw6_owner_probe.audio_sd_state_after = result.sd_state_after;
  g_ps_hw6_owner_probe.audio_sai_state_after = result.sai_state_after;
  g_ps_hw6_owner_probe.audio_sai_error_after = result.sai_error_after;
  g_ps_hw6_owner_probe.audio_dma_state_after = result.dma_state_after;
  g_ps_hw6_owner_probe.audio_dma_error_after = result.dma_error_after;
  PS_HW6_UpdateAudioDriverProbe();
  g_ps_hw6_owner_probe.audio_complete = 1UL;

  if (driver_status == PS_STATUS_OK)
  {
    g_ps_hw6_owner_probe.audio_success = 1UL;
    return HAL_OK;
  }

  return HAL_ERROR;
}

HAL_StatusTypeDef PS_HW6_AudioOwner_RunSfx(uint32_t cue_index)
{
  ps_egg_state_loader_audio_cue_t cue;
  ps_dev_audio_play_result_t result;
  ps_hw6_audio_stream_decoder_t decoder;
  ps_status_t driver_status;
  ps_status_t playback_status;
  HAL_StatusTypeDef decode_status;
  HAL_StatusTypeDef prefetch_cleanup_status;
  uint32_t first_half_decoded = 0UL;
  uint32_t second_half_decoded = 0UL;
  uint32_t decoded_frames = 0UL;
  uint32_t source_finished = 0UL;
  uint32_t source_finished_before;
  uint32_t stream_event = 0UL;
  uint32_t final_event;
  uint32_t silence_frames = 0UL;
  uint32_t refill_count = 0UL;
  int16_t *destination;

  g_ps_hw6_owner_probe.phase = PS_HW6_OWNER_PHASE_AUDIO;
  g_ps_hw6_owner_probe.audio_complete = 0UL;
  g_ps_hw6_owner_probe.audio_success = 0UL;
  g_ps_hw6_owner_probe.audio_sfx_request_count++;
  g_ps_hw6_owner_probe.audio_sfx_cue_index = cue_index;

  if (PS_EggStateLoader_GetAudioCue(cue_index, &cue) == 0UL)
  {
    g_ps_hw6_owner_probe.audio_sfx_decode_status = (uint32_t)HAL_ERROR;
    return HAL_ERROR;
  }
  g_ps_hw6_owner_probe.audio_sfx_asset_index = cue.asset_index;
  g_ps_hw6_owner_probe.audio_sfx_priority = cue.priority;
  g_ps_hw6_owner_probe.audio_sfx_volume = cue.volume;
  g_ps_hw6_owner_probe.audio_sfx_adpcm_bytes = cue.adpcm_size;
  g_ps_hw6_owner_probe.audio_sfx_sample_count = cue.sample_count;
  g_ps_hw6_owner_probe.audio_sfx_block_count = cue.block_count;
  g_ps_hw6_owner_probe.audio_sfx_stream_buffer_frames =
    PS_HW6_AUDIO_STREAM_BUFFER_FRAMES;
  g_ps_hw6_owner_probe.audio_sfx_stream_half_frames =
    PS_HW6_AUDIO_STREAM_HALF_FRAMES;
  g_ps_hw6_owner_probe.audio_sfx_stream_refill_count = 0UL;
  g_ps_hw6_owner_probe.audio_sfx_stream_first_half_callback_count = 0UL;
  g_ps_hw6_owner_probe.audio_sfx_stream_second_half_callback_count = 0UL;
  g_ps_hw6_owner_probe.audio_sfx_stream_underrun_count = 0UL;
  g_ps_hw6_owner_probe.audio_sfx_stream_silence_frames = 0UL;
  g_ps_hw6_owner_probe.audio_sfx_stream_wait_status =
    PS_HW6_OWNER_STATUS_NOT_RUN;
  g_ps_hw6_owner_probe.audio_sfx_stream_status =
    PS_HW6_OWNER_STATUS_NOT_RUN;
  g_ps_hw6_owner_probe.audio_sfx_package_backed = cue.package_backed;
  g_ps_hw6_owner_probe.audio_sfx_source_window_read_count = 0UL;
  g_ps_hw6_owner_probe.audio_sfx_source_window_failure_count = 0UL;
  g_ps_hw6_owner_probe.audio_sfx_source_window_bytes = 0UL;
  g_ps_hw6_owner_probe.audio_sfx_source_window_status =
    PS_HW6_OWNER_STATUS_NOT_RUN;
  g_ps_hw6_owner_probe.audio_sfx_source_prefetch_start_count = 0UL;
  g_ps_hw6_owner_probe.audio_sfx_source_prefetch_complete_count = 0UL;
  g_ps_hw6_owner_probe.audio_sfx_source_prefetch_miss_count = 0UL;
  g_ps_hw6_owner_probe.audio_sfx_source_prefetch_pending = 0UL;
  g_ps_hw6_owner_probe.audio_sfx_source_prefetch_cleanup_status =
    PS_HW6_OWNER_STATUS_NOT_RUN;
  g_ps_hw6_owner_probe.audio_sample_rate_hz =
    PS_EGG_STATE_LOADER_AUDIO_SAMPLE_RATE_HZ;
  g_ps_hw6_owner_probe.audio_buffer_halfwords =
    PS_HW6_AUDIO_STREAM_BUFFER_HALFWORDS;

  decode_status = PS_HW6_AudioStreamDecoderInit(&decoder, &cue);
  if (decode_status != HAL_OK)
  {
    g_ps_hw6_owner_probe.audio_sfx_decode_status = (uint32_t)decode_status;
    return HAL_ERROR;
  }

  decode_status = PS_HW6_AudioStreamDecodeFrames(
    &decoder,
    &ps_hw6_audio_stream_buffer[0],
    PS_HW6_AUDIO_STREAM_HALF_FRAMES,
    &first_half_decoded,
    &source_finished);
  if (decode_status == HAL_OK)
  {
    silence_frames += PS_HW6_AUDIO_STREAM_HALF_FRAMES - first_half_decoded;
    decode_status = PS_HW6_AudioStreamDecodeFrames(
      &decoder,
      &ps_hw6_audio_stream_buffer[
        PS_HW6_AUDIO_STREAM_HALF_FRAMES * 2UL],
      PS_HW6_AUDIO_STREAM_HALF_FRAMES,
      &second_half_decoded,
      &source_finished);
    silence_frames += PS_HW6_AUDIO_STREAM_HALF_FRAMES - second_half_decoded;
  }
  if ((decode_status == HAL_OK) && (source_finished == 0UL))
  {
    decode_status = PS_HW6_AudioStreamStartPrefetch(&decoder);
  }
  g_ps_hw6_owner_probe.audio_sfx_decode_status = (uint32_t)decode_status;
  g_ps_hw6_owner_probe.audio_sfx_decoded_samples =
    decoder.decoded_sample_count;
  g_ps_hw6_owner_probe.audio_sfx_stream_silence_frames = silence_frames;
  if (decode_status != HAL_OK)
  {
    return HAL_ERROR;
  }

  final_event = 0UL;
  if (source_finished != 0UL)
  {
    final_event = (second_half_decoded != 0UL) ?
      PS_DEV_AUDIO_STREAM_EVENT_SECOND_HALF :
      PS_DEV_AUDIO_STREAM_EVENT_FIRST_HALF;
  }
  driver_status = ps_dev_audio_stream_start(
    &ps_hw6_audio,
    ps_hw6_audio_stream_buffer,
    PS_HW6_AUDIO_STREAM_BUFFER_HALFWORDS,
    PS_HW6_AUDIO_AMP_SETTLE_TICKS,
    4096000UL,
    &result);
  playback_status = driver_status;
  while (playback_status == PS_STATUS_OK)
  {
    stream_event = 0UL;
    playback_status = ps_dev_audio_stream_wait(
      &ps_hw6_audio,
      PS_HW6_AUDIO_STREAM_REFILL_TIMEOUT_TICKS,
      &stream_event,
      &result);
    if (playback_status != PS_STATUS_OK)
    {
      break;
    }
    if (stream_event == final_event)
    {
      break;
    }

    destination = (stream_event == PS_DEV_AUDIO_STREAM_EVENT_FIRST_HALF) ?
      &ps_hw6_audio_stream_buffer[0] :
      &ps_hw6_audio_stream_buffer[PS_HW6_AUDIO_STREAM_HALF_FRAMES * 2UL];
    source_finished_before = source_finished;
    decode_status = PS_HW6_AudioStreamDecodeFrames(
      &decoder,
      destination,
      PS_HW6_AUDIO_STREAM_HALF_FRAMES,
      &decoded_frames,
      &source_finished);
    if (decode_status != HAL_OK)
    {
      playback_status = PS_STATUS_IO_ERROR;
      break;
    }
    silence_frames += PS_HW6_AUDIO_STREAM_HALF_FRAMES - decoded_frames;
    refill_count++;
    if ((source_finished_before == 0UL) && (source_finished != 0UL))
    {
      final_event = stream_event;
    }
    playback_status = ps_dev_audio_stream_release_half(&ps_hw6_audio,
                                                        stream_event);
  }
  if (driver_status == PS_STATUS_OK)
  {
    driver_status = ps_dev_audio_stream_stop(&ps_hw6_audio,
                                             playback_status,
                                             &result);
  }
  prefetch_cleanup_status = PS_HW6_AudioStreamDrainPrefetch(&decoder);
  if ((driver_status == PS_STATUS_OK) &&
      (prefetch_cleanup_status != HAL_OK))
  {
    driver_status = PS_STATUS_IO_ERROR;
  }

  g_ps_hw6_owner_probe.audio_sai_kernel_hz = result.sai_kernel_hz;
  g_ps_hw6_owner_probe.audio_sd_state_before = result.sd_state_before;
  g_ps_hw6_owner_probe.audio_sd_state_enabled = result.sd_state_enabled;
  g_ps_hw6_owner_probe.audio_rearm_status = result.rearm_status;
  g_ps_hw6_owner_probe.audio_start_status = result.start_hal_status;
  g_ps_hw6_owner_probe.audio_completion_wait_status =
    result.completion_wait_status;
  g_ps_hw6_owner_probe.audio_completion_callback_status =
    result.completion_callback_status;
  g_ps_hw6_owner_probe.audio_pre_cleanup_dma_irq_delta =
    result.pre_cleanup_dma_irq_delta;
  g_ps_hw6_owner_probe.audio_pre_cleanup_tx_callback_delta =
    result.pre_cleanup_tx_callback_delta;
  g_ps_hw6_owner_probe.audio_pre_cleanup_error_callback_delta =
    result.pre_cleanup_error_callback_delta;
  g_ps_hw6_owner_probe.audio_pre_cleanup_sai_kernel_hz =
    result.pre_cleanup_sai_kernel_hz;
  g_ps_hw6_owner_probe.audio_pre_cleanup_sai_state =
    result.pre_cleanup_sai_state;
  g_ps_hw6_owner_probe.audio_pre_cleanup_sai_error =
    result.pre_cleanup_sai_error;
  g_ps_hw6_owner_probe.audio_pre_cleanup_sai_cr1 =
    result.pre_cleanup_sai_cr1;
  g_ps_hw6_owner_probe.audio_pre_cleanup_sai_cr2 =
    result.pre_cleanup_sai_cr2;
  g_ps_hw6_owner_probe.audio_pre_cleanup_sai_frcr =
    result.pre_cleanup_sai_frcr;
  g_ps_hw6_owner_probe.audio_pre_cleanup_sai_slotr =
    result.pre_cleanup_sai_slotr;
  g_ps_hw6_owner_probe.audio_pre_cleanup_sai_imr =
    result.pre_cleanup_sai_imr;
  g_ps_hw6_owner_probe.audio_pre_cleanup_sai_sr =
    result.pre_cleanup_sai_sr;
  g_ps_hw6_owner_probe.audio_pre_cleanup_sai_gcr =
    result.pre_cleanup_sai_gcr;
  g_ps_hw6_owner_probe.audio_pre_cleanup_dma_state =
    result.pre_cleanup_dma_state;
  g_ps_hw6_owner_probe.audio_pre_cleanup_dma_error =
    result.pre_cleanup_dma_error;
  g_ps_hw6_owner_probe.audio_pre_cleanup_dma_ccr =
    result.pre_cleanup_dma_ccr;
  g_ps_hw6_owner_probe.audio_pre_cleanup_dma_csr =
    result.pre_cleanup_dma_csr;
  g_ps_hw6_owner_probe.audio_pre_cleanup_dma_cbr1 =
    result.pre_cleanup_dma_cbr1;
  g_ps_hw6_owner_probe.audio_pre_cleanup_dma_ctr1 =
    result.pre_cleanup_dma_ctr1;
  g_ps_hw6_owner_probe.audio_pre_cleanup_dma_ctr2 =
    result.pre_cleanup_dma_ctr2;
  g_ps_hw6_owner_probe.audio_pre_cleanup_dma_csar =
    result.pre_cleanup_dma_csar;
  g_ps_hw6_owner_probe.audio_pre_cleanup_dma_cdar =
    result.pre_cleanup_dma_cdar;
  g_ps_hw6_owner_probe.audio_pre_cleanup_dma_cllr =
    result.pre_cleanup_dma_cllr;
  g_ps_hw6_owner_probe.audio_stop_status = result.stop_hal_status;
  g_ps_hw6_owner_probe.audio_sd_state_after = result.sd_state_after;
  g_ps_hw6_owner_probe.audio_sai_state_after = result.sai_state_after;
  g_ps_hw6_owner_probe.audio_sai_error_after = result.sai_error_after;
  g_ps_hw6_owner_probe.audio_dma_state_after = result.dma_state_after;
  g_ps_hw6_owner_probe.audio_dma_error_after = result.dma_error_after;
  g_ps_hw6_owner_probe.audio_sfx_decode_status = (uint32_t)decode_status;
  g_ps_hw6_owner_probe.audio_sfx_decoded_samples =
    decoder.decoded_sample_count;
  g_ps_hw6_owner_probe.audio_sfx_stream_refill_count = refill_count;
  g_ps_hw6_owner_probe.audio_sfx_stream_first_half_callback_count =
    result.stream_first_half_callback_count;
  g_ps_hw6_owner_probe.audio_sfx_stream_second_half_callback_count =
    result.stream_second_half_callback_count;
  g_ps_hw6_owner_probe.audio_sfx_stream_underrun_count =
    result.stream_underrun_count;
  g_ps_hw6_owner_probe.audio_sfx_stream_silence_frames = silence_frames;
  g_ps_hw6_owner_probe.audio_sfx_stream_wait_status = result.stream_wait_status;
  g_ps_hw6_owner_probe.audio_sfx_stream_wait_preempt_disable_before =
    result.stream_wait_preempt_disable_before;
  g_ps_hw6_owner_probe.audio_sfx_stream_wait_system_state_before =
    result.stream_wait_system_state_before;
  g_ps_hw6_owner_probe.audio_sfx_stream_wait_current_thread_before =
    result.stream_wait_current_thread_before;
  g_ps_hw6_owner_probe.audio_sfx_stream_status = (uint32_t)driver_status;
  g_ps_hw6_owner_probe.audio_sfx_source_window_read_count =
    decoder.source_window_read_count;
  g_ps_hw6_owner_probe.audio_sfx_source_window_failure_count =
    decoder.source_window_failure_count;
  g_ps_hw6_owner_probe.audio_sfx_source_window_bytes =
    decoder.source_window_bytes;
  g_ps_hw6_owner_probe.audio_sfx_source_window_status =
    decoder.source_window_last_status;
  g_ps_hw6_owner_probe.audio_sfx_source_prefetch_start_count =
    decoder.source_prefetch_start_count;
  g_ps_hw6_owner_probe.audio_sfx_source_prefetch_complete_count =
    decoder.source_prefetch_complete_count;
  g_ps_hw6_owner_probe.audio_sfx_source_prefetch_miss_count =
    decoder.source_prefetch_miss_count;
  g_ps_hw6_owner_probe.audio_sfx_source_prefetch_pending =
    decoder.source_prefetch_pending;
  g_ps_hw6_owner_probe.audio_sfx_source_prefetch_cleanup_status =
    decoder.source_prefetch_cleanup_status;
  PS_HW6_UpdateAudioDriverProbe();
  g_ps_hw6_owner_probe.audio_complete = 1UL;
  if (driver_status == PS_STATUS_OK)
  {
    g_ps_hw6_owner_probe.audio_success = 1UL;
    return HAL_OK;
  }
  return HAL_ERROR;
}

void PS_HW6_OwnerServices_MarkComplete(void)
{
  g_ps_hw6_owner_probe.phase = PS_HW6_OWNER_PHASE_COMPLETE;
}
