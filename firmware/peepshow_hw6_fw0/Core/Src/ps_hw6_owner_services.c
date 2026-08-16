#include "ps_hw6_owner_services.h"

#include <string.h>

#include "display_renderer.h"
#include "LS013B7DH05.h"
#include "ps_dev_audio.h"
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
#define PS_HW6_DISPLAY_LPBAM_CLEAR_PATTERN   (1UL)
#define PS_HW6_DISPLAY_LPBAM_CLEAR_BOOT_HOLD (2UL)
#define PS_HW6_DISPLAY_LPBAM_CLEAR_UI_RENDER (3UL)
#define PS_HW6_DISPLAY_LPBAM_CLEAR_ABORT     (4UL)
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

static HAL_StatusTypeDef PS_HW6_DisplayOwner_PresentFull(
  volatile uint32_t *init_status,
  volatile uint32_t *present_status)
{
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
  init_result = LCD_Init(&ps_hw6_display, &hspi3);
  if (init_status != NULL)
  {
    *init_status = (uint32_t)init_result;
  }
  if (init_result == HAL_OK)
  {
    present_result = LCD_PresentFull_DMA(
      &ps_hw6_display,
      DisplayRenderer_GetBuffer(),
      PS_HW6_DISPLAY_PRESENT_TIMEOUT_MS);
  }
  if (present_status != NULL)
  {
    *present_status = (uint32_t)present_result;
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
  g_ps_hw6_owner_probe.display_lpbam_status =
    PS_HW6_OWNER_STATUS_NOT_RUN;
  g_ps_hw6_owner_probe.display_lpbam_ready_page =
    PS_HW6_OWNER_STATUS_NOT_RUN;
  g_ps_hw6_owner_probe.display_lpbam_prepare_status =
    PS_HW6_OWNER_STATUS_NOT_RUN;
  ps_hw6_display_lpbam_debug_force_ready_once = 0UL;
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

  driver_status = PS_HW6_DisplayOwner_PresentFull(
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

  driver_status = PS_HW6_DisplayOwner_ClearPanel(&clear_hal_status);
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
  driver_status = PS_HW6_DisplayOwner_PresentFull(
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

HAL_StatusTypeDef PS_HW6_DisplayOwner_PrepareLpbamStop2(void)
{
  g_ps_hw6_owner_probe.phase = PS_HW6_OWNER_PHASE_DISPLAY;
  g_ps_hw6_owner_probe.display_lpbam_prepare_count++;
  g_ps_hw6_owner_probe.display_lpbam_prepare_tick =
    (uint32_t)tx_time_get();
  g_ps_hw6_owner_probe.display_lpbam_ready_render_count =
    g_ps_hw6_owner_probe.display_ui_render_count;

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
  g_ps_hw6_owner_probe.display_lpbam_status =
    PS_HW6_OWNER_STATUS_UNAVAILABLE;
  g_ps_hw6_owner_probe.display_lpbam_prepare_status =
    PS_HW6_OWNER_STATUS_UNAVAILABLE;
  return HAL_ERROR;
}

void PS_HW6_DisplayOwner_DebugForceNextLpbamReady(void)
{
  ps_hw6_display_lpbam_debug_force_ready_once = 1UL;
}

HAL_StatusTypeDef PS_HW6_DisplayOwner_AbortLpbamStop2(void)
{
  g_ps_hw6_owner_probe.phase = PS_HW6_OWNER_PHASE_DISPLAY;
  g_ps_hw6_owner_probe.display_lpbam_abort_count++;
  g_ps_hw6_owner_probe.display_lpbam_abort_tick =
    (uint32_t)tx_time_get();
  PS_HW6_DisplayOwner_ClearLpbamReadiness(
    PS_HW6_DISPLAY_LPBAM_CLEAR_ABORT);
  g_ps_hw6_owner_probe.display_lpbam_abort_status =
    (uint32_t)HAL_OK;
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
