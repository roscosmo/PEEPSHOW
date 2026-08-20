#include "ps_hw6_owner_services.h"

#include <string.h>

#include "display_renderer.h"
#include "LS013B7DH05.h"
#include "ps_dev_audio.h"
#include "ps_lpbam_display_buffers.h"
#include "ps_lpbam_display_queue.h"
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
#define PS_HW6_DISPLAY_LPBAM_LPTIM_PRESCALER (128UL)
#define PS_HW6_DISPLAY_LPBAM_CADENCE_DIVISOR (2UL)
#define PS_HW6_DISPLAY_MILLISECONDS_PER_SECOND (1000UL)
#define PS_HW6_DISPLAY_DRIVER_API_VERSION    (1UL)
#define PS_HW6_DISPLAY_DRIVER_STATE_READY    (1UL)
#define PS_HW6_DISPLAY_DRIVER_STATE_HOLD     (2UL)
#define PS_HW6_DISPLAY_DRIVER_STATE_FAULT    (3UL)

#define PS_HW6_AUDIO_SAMPLE_RATE_HZ         (16000UL)
#define PS_HW6_AUDIO_TONE_HZ                (1000UL)
#define PS_HW6_AUDIO_DURATION_MS            (750UL)
#define PS_HW6_AUDIO_AMPLITUDE              (3000)
#define PS_HW6_AUDIO_BUFFER_FRAMES          (1024U)
#define PS_HW6_AUDIO_BUFFER_HALFWORDS       (PS_HW6_AUDIO_BUFFER_FRAMES * 2U)
#define PS_HW6_AUDIO_AMP_SETTLE_TICKS       (2UL)
#define PS_HW6_AUDIO_DURATION_TICKS \
  ((PS_HW6_AUDIO_DURATION_MS * TX_TIMER_TICKS_PER_SECOND + 999UL) / 1000UL)

extern I2C_HandleTypeDef hi2c3;
extern RTC_HandleTypeDef hrtc;
extern SAI_HandleTypeDef hsai_BlockA1;
extern DMA_HandleTypeDef handle_GPDMA1_Channel3;
extern LPTIM_HandleTypeDef hlptim1;
extern SPI_HandleTypeDef hspi3;
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
static uint32_t ps_hw6_display_lpbam_prearmed;
static uint32_t ps_hw6_display_lpbam_active;
static int16_t ps_hw6_audio_buffer[PS_HW6_AUDIO_BUFFER_HALFWORDS]
  __attribute__((aligned(4)));

static const int16_t ps_hw6_sine_16[16] =
{
  0, 1148, 2121, 2772, 3000, 2772, 2121, 1148,
  0, -1148, -2121, -2772, -3000, -2772, -2121, -1148
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
}

static void PS_HW6_DisplayOwner_ClearLpbamReadiness(uint32_t reason)
{
  g_ps_hw6_owner_probe.display_lpbam_ready = 0UL;
  g_ps_hw6_owner_probe.display_lpbam_ready_page =
    PS_HW6_OWNER_STATUS_NOT_RUN;
  g_ps_hw6_owner_probe.display_lpbam_ready_render_count =
    g_ps_hw6_owner_probe.display_ui_render_count;
  g_ps_hw6_owner_probe.display_lpbam_clear_count++;
  g_ps_hw6_owner_probe.display_lpbam_clear_reason = reason;
  g_ps_hw6_owner_probe.display_lpbam_status =
    PS_HW6_OWNER_STATUS_NOT_RUN;
}

static void PS_HW6_DisplayOwner_ResetLpbamPrepareProbe(void)
{
  g_ps_hw6_owner_probe.display_lpbam_prearmed =
    ps_hw6_display_lpbam_prearmed;
  g_ps_hw6_owner_probe.display_lpbam_active = ps_hw6_display_lpbam_active;
  g_ps_hw6_owner_probe.display_lpbam_animation_id =
    DISPLAY_RENDERER_ANIMATION_NONE;
  g_ps_hw6_owner_probe.display_lpbam_source_primitive_id =
    DISPLAY_RENDERER_PRIMITIVE_NONE;
  g_ps_hw6_owner_probe.display_lpbam_focus_row = DISPLAY_RENDERER_ROW_NONE;
  g_ps_hw6_owner_probe.display_lpbam_cursor_start_row = 0UL;
  g_ps_hw6_owner_probe.display_lpbam_cursor_row_count = 0UL;
  g_ps_hw6_owner_probe.display_lpbam_cursor_start_column = 0UL;
  g_ps_hw6_owner_probe.display_lpbam_cursor_column_count = 0UL;
  g_ps_hw6_owner_probe.display_lpbam_payload_frame_count = 0UL;
  g_ps_hw6_owner_probe.display_lpbam_payload_chunk_count = 0UL;
  g_ps_hw6_owner_probe.display_lpbam_payload_bytes = 0UL;
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

  autonomous.TriggerState = SPI_AUTO_MODE_ENABLE;
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
  uint32_t *period_counts,
  uint32_t *compare_counts)
{
  uint32_t lptim_kernel_hz;
  uint64_t denominator;
  uint64_t counts;

  if ((period_counts == NULL) || (compare_counts == NULL))
  {
    return HAL_ERROR;
  }

  lptim_kernel_hz = HAL_RCCEx_GetPeriphCLKFreq(RCC_PERIPHCLK_LPTIM1);
  denominator = (uint64_t)PS_HW6_DISPLAY_LPBAM_LPTIM_PRESCALER *
                (uint64_t)PS_HW6_DISPLAY_MILLISECONDS_PER_SECOND *
                (uint64_t)PS_HW6_DISPLAY_LPBAM_CADENCE_DIVISOR;
  counts = ((uint64_t)lptim_kernel_hz *
            (uint64_t)KNOB_DISPLAY_CURSOR_BLINK_PERIOD_MS +
            (denominator / 2ULL)) / denominator;
  if ((lptim_kernel_hz == 0UL) || (counts < 2ULL) ||
      (counts > 65535ULL))
  {
    return HAL_ERROR;
  }

  *period_counts = (uint32_t)counts;
  *compare_counts = (uint32_t)(counts / 2ULL);
  return HAL_OK;
}

static HAL_StatusTypeDef PS_HW6_DisplayOwner_ConfigLpbamLptim(void)
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
  if (PS_HW6_DisplayOwner_GetLpbamTimerCounts(&period_counts,
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

  status = PS_LpbamDisplayQueue_Build();
  if (status != HAL_OK)
  {
    g_ps_hw6_owner_probe.display_lpbam_link_status = (uint32_t)status;
    return status;
  }

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
                                        uint32_t shutdown_countdown_seconds)
{
  display_renderer_stats_t stats;

  DisplayRenderer_PrepareUIPage(page,
                                calibration_page,
                                focus_index,
                                shutdown_state,
                                shutdown_countdown_seconds,
                                &stats);
  PS_HW6_DisplayOwner_ApplyRendererStats(
    PS_HW6_DISPLAY_UI_PATTERN_ID, &stats);
  g_ps_hw6_owner_probe.display_ui_primitive_id = stats.primitive_id;
  g_ps_hw6_owner_probe.display_ui_previous_focus_row =
    stats.previous_focus_row;
  g_ps_hw6_owner_probe.display_ui_current_focus_row =
    stats.current_focus_row;
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

  for (frame = 0UL; frame < PS_HW6_AUDIO_BUFFER_FRAMES; ++frame)
  {
    int16_t sample = ps_hw6_sine_16[frame & 15UL];
    ps_hw6_audio_buffer[(frame * 2UL) + 0UL] = sample;
    ps_hw6_audio_buffer[(frame * 2UL) + 1UL] = sample;
  }

  g_ps_hw6_owner_probe.audio_sample_rate_hz = PS_HW6_AUDIO_SAMPLE_RATE_HZ;
  g_ps_hw6_owner_probe.audio_tone_hz = PS_HW6_AUDIO_TONE_HZ;
  g_ps_hw6_owner_probe.audio_duration_ms = PS_HW6_AUDIO_DURATION_MS;
  g_ps_hw6_owner_probe.audio_amplitude = PS_HW6_AUDIO_AMPLITUDE;
  g_ps_hw6_owner_probe.audio_buffer_halfwords = PS_HW6_AUDIO_BUFFER_HALFWORDS;
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
  g_ps_hw6_owner_probe.audio_start_status =
    PS_HW6_OWNER_STATUS_NOT_RUN;
  g_ps_hw6_owner_probe.audio_stop_status =
    PS_HW6_OWNER_STATUS_NOT_RUN;
  g_ps_hw6_owner_probe.audio_ack_set_status =
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
    HAL_DMA_GetState(&handle_LPDMA1_Channel0);
  g_ps_hw6_owner_probe.display_dma_error_after =
    HAL_DMA_GetError(&handle_LPDMA1_Channel0);
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

HAL_StatusTypeDef PS_HW6_DisplayOwner_ClearBootHold(void)
{
  uint32_t clear_hal_status = (uint32_t)HAL_ERROR;
  HAL_StatusTypeDef driver_status;

  g_ps_hw6_owner_probe.phase = PS_HW6_OWNER_PHASE_DISPLAY;
  PS_HW6_DisplayOwner_ClearLpbamReadiness(
    PS_HW6_DISPLAY_LPBAM_CLEAR_BOOT_HOLD);
  g_ps_hw6_owner_probe.display_complete = 0UL;
  g_ps_hw6_owner_probe.display_success = 0UL;
  g_ps_hw6_owner_probe.display_ui_request_count++;
  g_ps_hw6_owner_probe.display_ui_page = PS_UI_ROUTER_PAGE_BOOTSTRAP;
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
    HAL_DMA_GetState(&handle_LPDMA1_Channel0);
  g_ps_hw6_owner_probe.display_dma_error_after =
    HAL_DMA_GetError(&handle_LPDMA1_Channel0);
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

HAL_StatusTypeDef PS_HW6_DisplayOwner_RenderUI(
  uint32_t page,
  uint32_t calibration_page,
  uint32_t focus_index,
  uint32_t shutdown_state,
  uint32_t shutdown_countdown_seconds)
{
  HAL_StatusTypeDef driver_status;

  g_ps_hw6_owner_probe.phase = PS_HW6_OWNER_PHASE_DISPLAY;
  PS_HW6_DisplayOwner_ClearLpbamReadiness(
    PS_HW6_DISPLAY_LPBAM_CLEAR_UI_RENDER);
  g_ps_hw6_owner_probe.display_complete = 0UL;
  g_ps_hw6_owner_probe.display_success = 0UL;
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

  PS_HW6_PrepareDisplayUIPage(page,
                              calibration_page,
                              focus_index,
                              shutdown_state,
                              shutdown_countdown_seconds);
  driver_status = PS_HW6_DisplayOwner_PresentRendererRows(
    &g_ps_hw6_owner_probe.display_init_status,
    &g_ps_hw6_owner_probe.display_present_status);

  g_ps_hw6_owner_probe.display_dma_done = LCD_FlushDMA_IsDone() ? 1UL : 0UL;
  g_ps_hw6_owner_probe.display_spi_state_after = HAL_SPI_GetState(&hspi3);
  g_ps_hw6_owner_probe.display_spi_error_after = HAL_SPI_GetError(&hspi3);
  g_ps_hw6_owner_probe.display_dma_state_after =
    HAL_DMA_GetState(&handle_LPDMA1_Channel0);
  g_ps_hw6_owner_probe.display_dma_error_after =
    HAL_DMA_GetError(&handle_LPDMA1_Channel0);
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
    HAL_DMA_GetState(&handle_LPDMA1_Channel0);
  g_ps_hw6_owner_probe.display_dma_error_after =
    HAL_DMA_GetError(&handle_LPDMA1_Channel0);
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

static HAL_StatusTypeDef PS_HW6_DisplayOwner_PrepareLpbamStop2WithCursorPhase(uint32_t current_visible)
{
  display_renderer_animation_intent_t animation_intent;
  display_renderer_panel_region_t cursor_region;
  HAL_StatusTypeDef status;
  uint32_t chunk_total = 0UL;
  uint16_t frame;

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

  if ((DisplayRenderer_GetWaitingAnimationIntent(&animation_intent) == 0UL) ||
      (animation_intent.animation_id !=
       DISPLAY_RENDERER_ANIMATION_CURSOR_BLINK))
  {
    g_ps_hw6_owner_probe.display_lpbam_status =
      PS_HW6_OWNER_STATUS_UNAVAILABLE;
    g_ps_hw6_owner_probe.display_lpbam_prepare_status =
      PS_HW6_OWNER_STATUS_UNAVAILABLE;
    return HAL_ERROR;
  }

  cursor_region = animation_intent.panel_region;
  g_ps_hw6_owner_probe.display_lpbam_animation_id =
    animation_intent.animation_id;
  g_ps_hw6_owner_probe.display_lpbam_source_primitive_id =
    animation_intent.source_primitive_id;
  g_ps_hw6_owner_probe.display_lpbam_focus_row = animation_intent.focus_row;

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

  g_ps_hw6_owner_probe.display_lpbam_cursor_start_row =
    cursor_region.start_row;
  g_ps_hw6_owner_probe.display_lpbam_cursor_row_count =
    cursor_region.row_count;
  g_ps_hw6_owner_probe.display_lpbam_cursor_start_column =
    cursor_region.start_column;
  g_ps_hw6_owner_probe.display_lpbam_cursor_column_count =
    cursor_region.column_count;

  status = PS_LpbamDisplay_BuildCursorBlinkBuffersFromFrame(
    DisplayRenderer_GetBuffer(),
    cursor_region.start_row,
    cursor_region.row_count,
    cursor_region.start_column,
    cursor_region.column_count,
    (current_visible == 0UL) ? 0U : 1U);
  g_ps_hw6_owner_probe.display_lpbam_fill_status = (uint32_t)status;
  if (status != HAL_OK)
  {
    g_ps_hw6_owner_probe.display_lpbam_status = (uint32_t)status;
    g_ps_hw6_owner_probe.display_lpbam_prepare_status =
      (uint32_t)status;
    return status;
  }

  for (frame = 0U; frame < ps_lpbam_display_active_frame_count; ++frame)
  {
    chunk_total += ps_lpbam_display_active_chunk_count[frame];
  }
  g_ps_hw6_owner_probe.display_lpbam_payload_frame_count =
    ps_lpbam_display_active_frame_count;
  g_ps_hw6_owner_probe.display_lpbam_payload_chunk_count = chunk_total;
  g_ps_hw6_owner_probe.display_lpbam_payload_bytes =
    ps_lpbam_display_frame_len;

  status = PS_HW6_DisplayOwner_EnableLpbamAutonomousClocks();
  g_ps_hw6_owner_probe.display_lpbam_clock_status = (uint32_t)status;
  if (status == HAL_OK)
  {
    status = PS_HW6_DisplayOwner_ConfigLpbamLptim();
  }
  if (status == HAL_OK)
  {
    status = PS_HW6_DisplayOwner_PrearmLpbamPlayback();
  }

  g_ps_hw6_owner_probe.display_lpbam_prearm_status = (uint32_t)status;
  if (status != HAL_OK)
  {
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
    g_ps_hw6_owner_probe.display_lpbam_prepare_status =
      (uint32_t)status;
    return status;
  }

  g_ps_hw6_owner_probe.display_lpbam_ready = 1UL;
  g_ps_hw6_owner_probe.display_lpbam_ready_page =
    g_ps_ui_router_probe.current_page;
  g_ps_hw6_owner_probe.display_lpbam_status = (uint32_t)HAL_OK;
  g_ps_hw6_owner_probe.display_lpbam_prepare_status =
    (uint32_t)HAL_OK;
  return HAL_OK;
}

HAL_StatusTypeDef PS_HW6_DisplayOwner_PrepareLpbamStop2(void)
{
  return PS_HW6_DisplayOwner_PrepareLpbamStop2WithCursorPhase(0UL);
}

HAL_StatusTypeDef PS_HW6_DisplayOwner_PrepareLpbamStop2ForCursorPhase(
  uint32_t current_visible)
{
  return PS_HW6_DisplayOwner_PrepareLpbamStop2WithCursorPhase(
    current_visible);
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

HAL_StatusTypeDef PS_HW6_AudioOwner_RunTone(void)
{
  ps_dev_audio_play_result_t result;
  ps_status_t driver_status;

  g_ps_hw6_owner_probe.phase = PS_HW6_OWNER_PHASE_AUDIO;
  g_ps_hw6_owner_probe.audio_complete = 0UL;
  g_ps_hw6_owner_probe.audio_success = 0UL;

  driver_status = ps_dev_audio_play_dma(
    &ps_hw6_audio,
    ps_hw6_audio_buffer,
    PS_HW6_AUDIO_BUFFER_HALFWORDS,
    PS_HW6_AUDIO_AMP_SETTLE_TICKS,
    PS_HW6_AUDIO_DURATION_TICKS,
    4096000UL,
    &result);

  g_ps_hw6_owner_probe.audio_sai_kernel_hz = result.sai_kernel_hz;
  g_ps_hw6_owner_probe.audio_sd_state_before = result.sd_state_before;
  g_ps_hw6_owner_probe.audio_sd_state_enabled = result.sd_state_enabled;
  g_ps_hw6_owner_probe.audio_start_status = result.start_hal_status;
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

void PS_HW6_OwnerServices_MarkComplete(void)
{
  g_ps_hw6_owner_probe.phase = PS_HW6_OWNER_PHASE_COMPLETE;
}
