#include "ps_hw6_owner_services.h"

#include <string.h>

#include "LS013B7DH05.h"
#include "main.h"
#include "ps_hw_i2c3.h"

#define PS_HW6_OWNER_PHASE_INIT             (0x6700UL)
#define PS_HW6_OWNER_PHASE_POWER            (0x6701UL)
#define PS_HW6_OWNER_PHASE_DISPLAY          (0x6702UL)
#define PS_HW6_OWNER_PHASE_AUDIO            (0x6703UL)
#define PS_HW6_OWNER_PHASE_COMPLETE         (0x67FFUL)

#define PS_HW6_PMIC_ADDRESS_7BIT            (0x46U)
#define PS_HW6_PMIC_LEASE_WAIT_TICKS        (10UL)
#define PS_HW6_PMIC_TRANSFER_TIMEOUT_MS     (50U)
#define PS_HW6_PMIC_RAIL_PGOOD_MASK         (0x03U)

#define PS_HW6_DISPLAY_PATTERN_ID           (0x54455354UL)
#define PS_HW6_DISPLAY_PRESENT_TIMEOUT_MS   (250U)

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
extern SPI_HandleTypeDef hspi3;
extern DMA_HandleTypeDef handle_LPDMA1_Channel0;

volatile PS_HW6_OwnerProbe g_ps_hw6_owner_probe;

static LS013B7DH05 ps_hw6_display;
static uint8_t ps_hw6_display_framebuffer[BUFFER_LENGTH];
static int16_t ps_hw6_audio_buffer[PS_HW6_AUDIO_BUFFER_HALFWORDS]
  __attribute__((aligned(4)));

static const uint8_t ps_hw6_pmic_registers[PS_HW6_OWNER_POWER_REGISTER_COUNT] =
{
  0x00U, 0x29U, 0x2AU, 0x2BU, 0x2CU, 0x2EU, 0x2FU
};

static const uint8_t ps_hw6_pmic_expected[PS_HW6_OWNER_POWER_REGISTER_COUNT] =
{
  0x10U, 0x31U, 0x18U, 0x18U, 0x13U, 0x00U, 0x07U
};

static const int16_t ps_hw6_sine_16[16] =
{
  0, 1148, 2121, 2772, 3000, 2772, 2121, 1148,
  0, -1148, -2121, -2772, -3000, -2772, -2121, -1148
};

static uint32_t PS_HW6_DisplaySetBlack(uint16_t x, uint16_t y)
{
  uint32_t index;
  uint8_t mask;

  if ((x >= DISPLAY_WIDTH) || (y >= DISPLAY_HEIGHT))
  {
    return 0UL;
  }

  index = ((uint32_t)y * LINE_WIDTH) + ((uint32_t)x >> 3U);
  mask = (uint8_t)(1U << (x & 7U));
  if ((ps_hw6_display_framebuffer[index] & mask) == 0U)
  {
    return 0UL;
  }

  ps_hw6_display_framebuffer[index] &= (uint8_t)~mask;
  return 1UL;
}

static uint32_t PS_HW6_DisplayHorizontalLine(uint16_t x0,
                                             uint16_t x1,
                                             uint16_t y)
{
  uint32_t count = 0UL;
  uint16_t x;

  for (x = x0; x <= x1; ++x)
  {
    count += PS_HW6_DisplaySetBlack(x, y);
  }
  return count;
}

static uint32_t PS_HW6_DisplayVerticalLine(uint16_t x,
                                           uint16_t y0,
                                           uint16_t y1)
{
  uint32_t count = 0UL;
  uint16_t y;

  for (y = y0; y <= y1; ++y)
  {
    count += PS_HW6_DisplaySetBlack(x, y);
  }
  return count;
}

static uint32_t PS_HW6_DisplayFilledRect(uint16_t x0,
                                         uint16_t y0,
                                         uint16_t width,
                                         uint16_t height)
{
  uint32_t count = 0UL;
  uint16_t y;

  for (y = y0; y < (uint16_t)(y0 + height); ++y)
  {
    count += PS_HW6_DisplayHorizontalLine(x0,
                                          (uint16_t)(x0 + width - 1U),
                                          y);
  }
  return count;
}

static void PS_HW6_PrepareDisplayPattern(void)
{
  uint32_t black_pixels = 0UL;
  uint32_t hash = 2166136261UL;
  uint16_t i;

  (void)memset(ps_hw6_display_framebuffer, 0xFF,
               sizeof(ps_hw6_display_framebuffer));

  for (i = 0U; i < 2U; ++i)
  {
    black_pixels += PS_HW6_DisplayHorizontalLine(
      i, (uint16_t)(DISPLAY_WIDTH - 1U - i), i);
    black_pixels += PS_HW6_DisplayHorizontalLine(
      i, (uint16_t)(DISPLAY_WIDTH - 1U - i),
      (uint16_t)(DISPLAY_HEIGHT - 1U - i));
    black_pixels += PS_HW6_DisplayVerticalLine(
      i, i, (uint16_t)(DISPLAY_HEIGHT - 1U - i));
    black_pixels += PS_HW6_DisplayVerticalLine(
      (uint16_t)(DISPLAY_WIDTH - 1U - i), i,
      (uint16_t)(DISPLAY_HEIGHT - 1U - i));
  }

  black_pixels += PS_HW6_DisplayHorizontalLine(
    8U, (uint16_t)(DISPLAY_WIDTH - 9U), DISPLAY_HEIGHT / 2U);
  black_pixels += PS_HW6_DisplayVerticalLine(
    DISPLAY_WIDTH / 2U, 8U, (uint16_t)(DISPLAY_HEIGHT - 9U));

  black_pixels += PS_HW6_DisplayFilledRect(8U, 8U, 10U, 10U);
  black_pixels += PS_HW6_DisplayHorizontalLine(
    (uint16_t)(DISPLAY_WIDTH - 19U), (uint16_t)(DISPLAY_WIDTH - 9U), 8U);
  black_pixels += PS_HW6_DisplayVerticalLine(
    (uint16_t)(DISPLAY_WIDTH - 9U), 8U, 18U);
  for (i = 0U; i < 11U; ++i)
  {
    black_pixels += PS_HW6_DisplaySetBlack(
      (uint16_t)(8U + i), (uint16_t)(DISPLAY_HEIGHT - 19U + i));
    black_pixels += PS_HW6_DisplaySetBlack(
      (uint16_t)(18U - i), (uint16_t)(DISPLAY_HEIGHT - 19U + i));
  }
  for (i = 0U; i < 12U; ++i)
  {
    if ((i & 1U) == 0U)
    {
      black_pixels += PS_HW6_DisplayFilledRect(
        (uint16_t)(DISPLAY_WIDTH - 20U + i),
        (uint16_t)(DISPLAY_HEIGHT - 20U), 1U, 12U);
    }
  }

  for (i = 0U; i < BUFFER_LENGTH; ++i)
  {
    hash ^= ps_hw6_display_framebuffer[i];
    hash *= 16777619UL;
  }

  g_ps_hw6_owner_probe.display_width = DISPLAY_WIDTH;
  g_ps_hw6_owner_probe.display_height = DISPLAY_HEIGHT;
  g_ps_hw6_owner_probe.display_pattern_id = PS_HW6_DISPLAY_PATTERN_ID;
  g_ps_hw6_owner_probe.display_framebuffer_hash = hash;
  g_ps_hw6_owner_probe.display_black_pixels = black_pixels;
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
  g_ps_hw6_owner_probe.audio_start_status =
    PS_HW6_OWNER_STATUS_NOT_RUN;
  g_ps_hw6_owner_probe.audio_stop_status =
    PS_HW6_OWNER_STATUS_NOT_RUN;
  g_ps_hw6_owner_probe.audio_ack_set_status =
    PS_HW6_OWNER_STATUS_NOT_RUN;

  for (i = 0U; i < PS_HW6_OWNER_POWER_REGISTER_COUNT; ++i)
  {
    g_ps_hw6_owner_probe.power_register_address[i] =
      ps_hw6_pmic_registers[i];
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

  HAL_GPIO_WritePin(SD_MODE_GPIO_Port, SD_MODE_Pin, GPIO_PIN_RESET);
  PS_HW6_PrepareDisplayPattern();
  PS_HW6_PrepareAudioTone();
  status = PS_HW_I2C3_Init(&hi2c3);
  g_ps_hw6_owner_probe.services_init_status = status;
  return status;
}

HAL_StatusTypeDef PS_HW6_PowerOwner_RunSnapshot(void)
{
  HAL_StatusTypeDef overall = HAL_OK;
  uint32_t i;

  g_ps_hw6_owner_probe.phase = PS_HW6_OWNER_PHASE_POWER;
  g_ps_hw6_owner_probe.power_complete = 0UL;
  g_ps_hw6_owner_probe.power_success = 0UL;

  for (i = 0U; i < PS_HW6_OWNER_POWER_REGISTER_COUNT; ++i)
  {
    uint8_t value = 0U;
    PS_HW_I2C3_Result result = PS_HW_I2C3_ReadRegister(
      PS_HW6_PMIC_ADDRESS_7BIT,
      ps_hw6_pmic_registers[i],
      &value,
      PS_HW6_PMIC_LEASE_WAIT_TICKS,
      PS_HW6_PMIC_TRANSFER_TIMEOUT_MS);

    g_ps_hw6_owner_probe.power_register_value[i] = value;
    g_ps_hw6_owner_probe.power_lease_get_status[i] = result.acquire_status;
    g_ps_hw6_owner_probe.power_transfer_status[i] = result.transfer_status;
    g_ps_hw6_owner_probe.power_transfer_error[i] = result.transfer_error;
    g_ps_hw6_owner_probe.power_lease_put_status[i] = result.release_status;

    if ((result.acquire_status != TX_SUCCESS) ||
        (result.transfer_status != HAL_OK) ||
        (result.release_status != TX_SUCCESS) ||
        (value != ps_hw6_pmic_expected[i]))
    {
      overall = HAL_ERROR;
    }
  }

  g_ps_hw6_owner_probe.power_i2c_state_after = HAL_I2C_GetState(&hi2c3);
  g_ps_hw6_owner_probe.power_i2c_error_after = HAL_I2C_GetError(&hi2c3);
  g_ps_hw6_owner_probe.power_identity_match =
    (g_ps_hw6_owner_probe.power_register_value[0] == 0x10U) ? 1UL : 0UL;
  g_ps_hw6_owner_probe.power_fault_clear =
    (g_ps_hw6_owner_probe.power_register_value[5] == 0x00U) ? 1UL : 0UL;
  g_ps_hw6_owner_probe.power_rails_ready =
    ((g_ps_hw6_owner_probe.power_register_value[6] &
      PS_HW6_PMIC_RAIL_PGOOD_MASK) == PS_HW6_PMIC_RAIL_PGOOD_MASK) ?
    1UL : 0UL;
  g_ps_hw6_owner_probe.power_complete = 1UL;
  g_ps_hw6_owner_probe.power_success = (overall == HAL_OK) ? 1UL : 0UL;
  return overall;
}

HAL_StatusTypeDef PS_HW6_DisplayOwner_RunPattern(void)
{
  HAL_StatusTypeDef init_status;
  HAL_StatusTypeDef present_status = HAL_ERROR;

  g_ps_hw6_owner_probe.phase = PS_HW6_OWNER_PHASE_DISPLAY;
  g_ps_hw6_owner_probe.display_complete = 0UL;
  g_ps_hw6_owner_probe.display_success = 0UL;
  g_ps_hw6_owner_probe.display_rtc_state = HAL_RTC_GetState(&hrtc);
  g_ps_hw6_owner_probe.display_rtc_cr = hrtc.Instance->CR;
  g_ps_hw6_owner_probe.display_spi_state_before = HAL_SPI_GetState(&hspi3);

  init_status = LCD_Init(&ps_hw6_display, &hspi3);
  g_ps_hw6_owner_probe.display_init_status = init_status;
  if (init_status == HAL_OK)
  {
    present_status = LCD_PresentFull_DMA(&ps_hw6_display,
                                         ps_hw6_display_framebuffer,
                                         PS_HW6_DISPLAY_PRESENT_TIMEOUT_MS);
  }

  g_ps_hw6_owner_probe.display_present_status = present_status;
  g_ps_hw6_owner_probe.display_dma_done =
    LCD_FlushDMA_IsDone() ? 1UL : 0UL;
  g_ps_hw6_owner_probe.display_spi_state_after = HAL_SPI_GetState(&hspi3);
  g_ps_hw6_owner_probe.display_spi_error_after = HAL_SPI_GetError(&hspi3);
  g_ps_hw6_owner_probe.display_dma_state_after =
    HAL_DMA_GetState(&handle_LPDMA1_Channel0);
  g_ps_hw6_owner_probe.display_dma_error_after =
    HAL_DMA_GetError(&handle_LPDMA1_Channel0);
  g_ps_hw6_owner_probe.display_complete = 1UL;

  if ((init_status == HAL_OK) &&
      (present_status == HAL_OK) &&
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

HAL_StatusTypeDef PS_HW6_AudioOwner_RunTone(void)
{
  HAL_StatusTypeDef start_status;
  HAL_StatusTypeDef stop_status = HAL_ERROR;

  g_ps_hw6_owner_probe.phase = PS_HW6_OWNER_PHASE_AUDIO;
  g_ps_hw6_owner_probe.audio_complete = 0UL;
  g_ps_hw6_owner_probe.audio_success = 0UL;
  g_ps_hw6_owner_probe.audio_sai_kernel_hz =
    HAL_RCCEx_GetPeriphCLKFreq(RCC_PERIPHCLK_SAI1);
  g_ps_hw6_owner_probe.audio_sd_state_before =
    HAL_GPIO_ReadPin(SD_MODE_GPIO_Port, SD_MODE_Pin);

  (void)HAL_SAI_DMAStop(&hsai_BlockA1);
  HAL_GPIO_WritePin(SD_MODE_GPIO_Port, SD_MODE_Pin, GPIO_PIN_SET);
  g_ps_hw6_owner_probe.audio_sd_state_enabled =
    HAL_GPIO_ReadPin(SD_MODE_GPIO_Port, SD_MODE_Pin);
  tx_thread_sleep(PS_HW6_AUDIO_AMP_SETTLE_TICKS);

  start_status = HAL_SAI_Transmit_DMA(
    &hsai_BlockA1,
    (uint8_t *)ps_hw6_audio_buffer,
    PS_HW6_AUDIO_BUFFER_HALFWORDS);
  g_ps_hw6_owner_probe.audio_start_status = start_status;
  if (start_status == HAL_OK)
  {
    tx_thread_sleep(PS_HW6_AUDIO_DURATION_TICKS);
    stop_status = HAL_SAI_DMAStop(&hsai_BlockA1);
  }

  g_ps_hw6_owner_probe.audio_stop_status = stop_status;
  HAL_GPIO_WritePin(SD_MODE_GPIO_Port, SD_MODE_Pin, GPIO_PIN_RESET);
  g_ps_hw6_owner_probe.audio_sd_state_after =
    HAL_GPIO_ReadPin(SD_MODE_GPIO_Port, SD_MODE_Pin);
  g_ps_hw6_owner_probe.audio_sai_state_after = HAL_SAI_GetState(&hsai_BlockA1);
  g_ps_hw6_owner_probe.audio_sai_error_after = HAL_SAI_GetError(&hsai_BlockA1);
  g_ps_hw6_owner_probe.audio_dma_state_after =
    HAL_DMA_GetState(&handle_GPDMA1_Channel3);
  g_ps_hw6_owner_probe.audio_dma_error_after =
    HAL_DMA_GetError(&handle_GPDMA1_Channel3);
  g_ps_hw6_owner_probe.audio_complete = 1UL;

  if ((start_status == HAL_OK) &&
      (stop_status == HAL_OK) &&
      (g_ps_hw6_owner_probe.audio_sai_kernel_hz == 4096000UL) &&
      (g_ps_hw6_owner_probe.audio_sd_state_enabled != 0UL) &&
      (g_ps_hw6_owner_probe.audio_sd_state_after == 0UL) &&
      (g_ps_hw6_owner_probe.audio_sai_error_after == HAL_SAI_ERROR_NONE) &&
      (g_ps_hw6_owner_probe.audio_dma_error_after == HAL_DMA_ERROR_NONE))
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
