/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
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
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "ADP5360.h"
#include "LS013B7DH05.h"
#include "LIS2DUX12.h"
#include "ps_display_renderer.h"
#include "tmag3001.h"
#include <math.h>

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef struct
{
  uint32_t magic;
  uint32_t phase;
  uint32_t tick_start;
  uint32_t tick_end;
  uint8_t public_addr_7bit;
  uint8_t hal_addr_shifted;
  uint8_t ready_shifted_status;
  uint8_t ready_unshifted_status;
  uint8_t driver_init_status;
  uint32_t ready_shifted_error;
  uint32_t ready_unshifted_error;
  uint8_t raw_reg[55];
  uint8_t raw_reg_status[55];
  uint32_t raw_reg_error[55];
  uint16_t vbat_mv;
  uint16_t buck_target_mv;
  uint16_t buckboost_target_mv;
  uint8_t bat_soc_percent;
  uint8_t buck_config;
  uint8_t buckboost_config;
  uint8_t charger_status1;
  uint8_t charger_status2;
  uint8_t fault;
  uint8_t pgood_status;
  uint8_t read_count;
  uint8_t complete;
} PsPhase0bPmicProbe;

typedef struct
{
  uint32_t magic;
  uint32_t phase;
  uint32_t tick;
  uint32_t system_core_clock;
  uint32_t sysclk_hz;
  uint32_t hclk_hz;
  uint32_t pclk1_hz;
  uint32_t pclk2_hz;
  uint32_t pclk3_hz;
  uint32_t rcc_cr;
  uint32_t rcc_csr;
  uint32_t rcc_cfgr1;
  uint32_t rcc_cfgr2;
  uint32_t rcc_cfgr3;
  uint32_t rcc_icscr1;
  uint32_t pwr_vosr;
  uint8_t flag_oblrst;
  uint8_t flag_pinrst;
  uint8_t flag_borrst;
  uint8_t flag_sftrst;
  uint8_t flag_iwdgrst;
  uint8_t flag_wwdgrst;
  uint8_t flag_lpwrst;
  uint8_t complete;
} PsPhase0ResetClockProbe;

typedef struct
{
  uint32_t magic;
  uint32_t phase;
  uint32_t tick;
  uint32_t spi3_state;
  uint32_t rtc_state;
  uint32_t spi3_kernel_hz;
  uint32_t gpiod_odr;
  uint32_t gpiod_idr;
  uint8_t vlt_lcd_pin_state;
  uint8_t spi3_nssp_disabled;
  uint8_t spi3_lsb_first;
  uint8_t spi3_nss_active_high;
  uint8_t complete;
} PsPhase1DisplayBaselineProbe;

typedef struct
{
  uint32_t magic;
  uint32_t phase;
  uint32_t tick_start;
  uint32_t tick_last_tx;
  uint32_t frame_counter;
  uint32_t tx_ok_count;
  uint32_t tx_error_count;
  uint32_t last_hal_error;
  uint32_t payload_len;
  uint8_t last_fill_value;
  uint8_t vlt_lcd_pin_state;
  uint8_t last_tx_status;
  uint8_t complete;
} PsPhase1DisplaySelfTestProbe;

typedef struct
{
  uint32_t magic;
  uint32_t phase;
  uint32_t tick_start;
  uint32_t tick_end;
  uint32_t ospi_kernel_hz;
  uint32_t ospi_state;
  uint32_t ospi_error;
  uint32_t ospi_sr;
  uint8_t release_status;
  uint8_t jedec_status;
  uint8_t status1_status;
  uint8_t status2_status;
  uint8_t status3_status;
  uint8_t jedec_id[3];
  uint8_t status1;
  uint8_t status2;
  uint8_t status3;
  uint8_t write_enable_erase_status;
  uint8_t erase_status;
  uint8_t erase_wait_status;
  uint8_t blank_read_status;
  uint8_t write_enable_program_status;
  uint8_t program_status;
  uint8_t program_wait_status;
  uint8_t dma_program_status;
  uint8_t dma_program_wait_status;
  uint8_t verify_read_status;
  uint8_t dma_read_status;
  uint8_t dma_wait_status;
  uint8_t write_enable_cleanup_status;
  uint8_t cleanup_erase_status;
  uint8_t cleanup_wait_status;
  uint8_t cleanup_read_status;
  uint8_t deep_power_down_status;
  uint8_t wake_status;
  uint8_t wake_jedec_status;
  uint8_t wake_read_status;
  uint8_t complete;
  uint32_t scratch_address;
  uint32_t test_length;
  uint32_t erase_blank_mismatch_count;
  uint32_t verify_mismatch_count;
  uint32_t dma_verify_mismatch_count;
  uint32_t cleanup_blank_mismatch_count;
  uint32_t wake_blank_mismatch_count;
  uint8_t verify_sample[16];
  uint8_t dma_verify_sample[16];
  uint8_t wake_jedec_id[3];
} PsPhase2StorageProbe;

typedef struct
{
  uint32_t magic;
  uint32_t phase;
  uint32_t tick_start;
  uint32_t tick_end;
  uint32_t lptim_kernel_hz;
  uint32_t requested_hz;
  uint32_t sweep_start_hz;
  uint32_t sweep_end_hz;
  uint32_t sweep_steps_requested;
  uint32_t sweep_steps_completed;
  uint32_t pattern_steps_requested;
  uint32_t pattern_steps_completed;
  uint32_t failed_step;
  uint32_t period;
  uint32_t pulse;
  uint32_t sd_mode_state_before;
  uint32_t sd_mode_state_after;
  uint32_t lptim_state_after;
  uint32_t lptim_isr_after;
  uint32_t sai_kernel_hz;
  uint32_t speaker_sample_rate_hz;
  uint32_t speaker_tone_hz;
  uint32_t speaker_buffer_halfwords;
  uint32_t speaker_half_cplt_count;
  uint32_t speaker_cplt_count;
  uint32_t speaker_error_count;
  uint32_t speaker_segments_requested;
  uint32_t speaker_segments_completed;
  uint32_t speaker_failed_segment;
  uint32_t speaker_final_waveform;
  uint32_t speaker_final_amplitude;
  uint32_t sai_state_after;
  uint32_t sai_error_after;
  uint32_t sai_sr_after;
  uint8_t reinit_status;
  uint8_t channel_config_status;
  uint8_t start_status;
  uint8_t stop_status;
  uint8_t speaker_start_status;
  uint8_t speaker_stop_status;
  uint8_t final_step_status;
  uint8_t complete;
} PsPhase3AudioProbe;

typedef struct
{
  uint32_t magic;
  uint32_t phase;
  uint32_t tick_start;
  uint32_t tick_last_edge;
  uint32_t idle_mask;
  uint32_t current_mask;
  uint32_t last_edge_pin;
  uint32_t last_edge_level;
  uint32_t total_edges;
  uint32_t start_edges;
  uint32_t a_edges;
  uint32_t b_edges;
  uint32_t l_edges;
  uint32_t r_edges;
  uint32_t start_presses;
  uint32_t start_releases;
  uint32_t a_presses;
  uint32_t a_releases;
  uint32_t b_presses;
  uint32_t b_releases;
  uint32_t l_presses;
  uint32_t l_releases;
  uint32_t r_presses;
  uint32_t r_releases;
  uint8_t complete;
} PsPhase4ButtonProbe;

typedef struct
{
  uint32_t magic;
  uint32_t phase;
  uint32_t tick_start;
  uint32_t tick_last_sample;
  uint32_t sample_count;
  uint32_t adc_error_count;
  uint32_t adc_state_after;
  uint32_t adc_error_after;
  uint32_t raw_sum;
  uint16_t disabled_sample;
  uint16_t last_sample;
  uint16_t min_sample;
  uint16_t max_sample;
  uint8_t calibration_status;
  uint8_t last_start_status;
  uint8_t last_poll_status;
  uint8_t last_stop_status;
  uint8_t phot_en_state_after;
  uint8_t complete;
} PsPhase5LightProbe;

typedef struct
{
  uint32_t magic;
  uint32_t phase;
  uint32_t tick_start;
  uint32_t tick_end;
  uint8_t expected_public_addr_7bit;
  uint8_t expected_hal_addr_shifted;
  uint8_t alt_public_addr_7bit;
  uint8_t alt_hal_addr_shifted;
  uint8_t ready_expected_status;
  uint8_t ready_alt_status;
  uint8_t driver_whoami_status;
  uint8_t raw_whoami_status;
  uint8_t raw_status_status;
  uint8_t raw_ctrl1_status;
  uint8_t raw_ctrl4_status;
  uint8_t raw_md1_status;
  uint8_t whoami;
  uint8_t raw_whoami;
  uint8_t raw_status;
  uint8_t raw_ctrl1;
  uint8_t raw_ctrl4;
  uint8_t raw_md1_cfg;
  uint32_t ready_expected_error;
  uint32_t ready_alt_error;
  uint32_t i2c_error_after;
  uint8_t complete;
} PsPhase4ImuProbe;

typedef struct
{
  uint32_t magic;
  uint32_t phase;
  uint32_t tick_start;
  uint32_t tick_end;
  uint8_t scanned_public_addr_7bit[4];
  uint8_t scanned_hal_addr_shifted[4];
  uint8_t ready_status[4];
  uint32_t ready_error[4];
  uint32_t full_scan_count;
  uint8_t full_scan_hit_bitmap[16];
  uint8_t detected_public_addr_7bit;
  uint8_t detected_hal_addr_shifted;
  uint8_t read_device_id_status;
  uint8_t read_manufacturer_lsb_status;
  uint8_t read_manufacturer_msb_status;
  uint8_t read_conv_status_status;
  uint8_t read_device_status_status;
  uint8_t read_result_window_status;
  uint8_t device_id;
  uint8_t manufacturer_lsb;
  uint8_t manufacturer_msb;
  uint8_t conv_status;
  uint8_t device_status;
  uint8_t result_window[7];
  int16_t x_raw;
  int16_t y_raw;
  int16_t z_raw;
  uint8_t result_window_conv_status;
  uint32_t i2c_error_after;
  uint8_t complete;
} PsPhase4TmagProbe;

#define PS_TMAG3001_CAPTURE_SAMPLES   (200U)

typedef struct
{
  uint32_t tick;
  int16_t x_raw;
  int16_t y_raw;
  int16_t z_raw;
  uint8_t read_status;
  uint8_t conv_status;
} PsPhase4TmagSample;

typedef struct
{
  uint32_t magic;
  uint32_t phase;
  uint32_t tick_start;
  uint32_t tick_capture_start;
  uint32_t tick_capture_end;
  uint32_t sample_period_ms;
  uint32_t samples_requested;
  uint32_t samples_completed;
  uint32_t read_error_count;
  uint32_t i2c_error_after;
  uint8_t address_7bit;
  uint8_t address_shifted;
  uint8_t write_device_config_1_status;
  uint8_t write_sensor_config_1_status;
  uint8_t write_sensor_config_2_status;
  uint8_t write_device_config_2_status;
  uint8_t read_device_config_1_status;
  uint8_t read_sensor_config_1_status;
  uint8_t read_sensor_config_2_status;
  uint8_t read_device_config_2_status;
  uint8_t device_config_1_value;
  uint8_t sensor_config_1_value;
  uint8_t sensor_config_2_value;
  uint8_t device_config_2_value;
  uint8_t start_cue_status;
  uint8_t end_cue_status[2];
  uint8_t complete;
  PsPhase4TmagSample samples[PS_TMAG3001_CAPTURE_SAMPLES];
} PsPhase4TmagCaptureProbe;

typedef struct
{
  uint32_t magic;
  uint32_t phase;
  uint32_t tick_start;
  uint32_t tick_release;
  uint32_t tick_end;
  uint32_t lpuart_kernel_hz;
  uint32_t gpioc_moder_before;
  uint32_t gpioc_pupdr_before;
  uint32_t gpioc_idr_before;
  uint32_t gpioc_odr_before;
  uint32_t gpioc_idr_after_release;
  uint32_t gpioc_odr_after_release;
  uint32_t gpioc_idr_after_boot_wait;
  uint32_t uart_state_after;
  uint32_t uart_error_after;
  uint32_t active_baud_after;
  uint16_t boot_rx_len;
  uint16_t command_rx_len[12];
  uint16_t phone_rx_len;
  uint16_t phone_data_rx_len;
  uint16_t phone_echo_tx_count;
  uint32_t phone_detect_tick;
  uint8_t phone_detected;
  uint8_t phone_sps_detected;
  uint8_t data_mode_tx_status;
  uint8_t hello_tx_status;
  uint8_t nrst_before;
  uint8_t nrst_after_release;
  uint8_t nrst_after_boot_wait;
  uint8_t nrst_after_final_reset;
  uint8_t sw1_level_before;
  uint8_t sw2_level_before;
  uint8_t dtr_level_before;
  uint8_t dsr_level_before;
  uint8_t command_tx_status[12];
  uint8_t boot_rx[96];
  uint8_t command_rx[12][96];
  uint8_t phone_rx[512];
  uint8_t phone_data_rx[512];
  uint8_t complete;
} PsPhase7NinaProbe;

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define PS_PHASE0B_PROBE_MAGIC        (0x50304231UL)
#define PS_PHASE0B_PROBE_PHASE        (0x00000B00UL)
#define PS_ADP5360_PUBLIC_ADDR_7BIT   (0x46U)
#define PS_ADP5360_HAL_ADDR_SHIFTED   (PS_ADP5360_PUBLIC_ADDR_7BIT << 1)
#define PS_I2C_TIMEOUT_MS             (25U)
#define PS_ADP5360_REG_COUNT          (0x37U)
#define PS_ADP5360_REG_INT_FLAG1      (0x34U)
#define PS_ADP5360_REG_INT_FLAG2      (0x35U)
#define PS_PHASE0_CLOCK_MAGIC         (0x5030434CUL)
#define PS_PHASE0_CLOCK_PHASE         (0x00000C10UL)
#define PS_PHASE1_DISPLAY_MAGIC       (0x50314453UL)
#define PS_PHASE1_DISPLAY_PHASE       (0x00001000UL)
#define PS_PHASE1_DISPLAY_SELFTEST_MAGIC (0x50314454UL)
#define PS_PHASE1_DISPLAY_SELFTEST_PHASE (0x00001020UL)
#define PS_PHASE2_STORAGE_MAGIC       (0x50325354UL)
#define PS_PHASE2_STORAGE_PHASE       (0x00002000UL)
#define PS_PHASE3_AUDIO_MAGIC         (0x50334155UL)
#define PS_PHASE3_AUDIO_PHASE         (0x00003000UL)
#define PS_PHASE4_BUTTON_MAGIC        (0x50344254UL)
#define PS_PHASE4_BUTTON_PHASE        (0x00004000UL)
#define PS_PHASE5_LIGHT_MAGIC         (0x50354C54UL)
#define PS_PHASE5_LIGHT_PHASE         (0x00005000UL)
#define PS_PHASE4_IMU_MAGIC           (0x5034494DUL)
#define PS_PHASE4_IMU_PHASE           (0x00004100UL)
#define PS_PHASE4_TMAG_MAGIC          (0x5034544DUL)
#define PS_PHASE4_TMAG_PHASE          (0x00004200UL)
#define PS_PHASE4_TMAG_CAPTURE_MAGIC  (0x50345443UL)
#define PS_PHASE4_TMAG_CAPTURE_PHASE  (0x00004210UL)
#define PS_PHASE7_NINA_MAGIC          (0x50374E4EUL)
#define PS_PHASE7_NINA_PHASE          (0x00007000UL)
#define PS_FW0_RUN_PMIC_PROBE         (0U)
#define PS_FW0_RUN_RESET_CLOCK_PROBE  (0U)
#define PS_FW0_RUN_DISPLAY_BASELINE   (0U)
#define PS_FW0_RUN_DISPLAY_SELFTEST   (0U)
#define PS_FW0_RUN_STORAGE_PROBE      (0U)
#define PS_FW0_RUN_BUTTON_PROBE       (0U)
#define PS_FW0_RUN_LIGHT_INIT_PROBE   (0U)
#define PS_FW0_RUN_LIGHT_PERIODIC     (0U)
#define PS_FW0_RUN_IMU_ID_PROBE       (0U)
#define PS_FW0_RUN_TMAG_ID_PROBE      (0U)
#define PS_FW0_RUN_TMAG_CAPTURE       (0U)
#define PS_FW0_RUN_AUDIO_PROBE        (0U)
#define PS_FW0_RUN_NINA_PROBE         (0U)
#define PS_FW0_RUN_DEEP_ISOLATION     (0U)
#define PS_DEEP_ISOLATION_BASELINE_MS (5000U)
#define PS_DEEP_ISOLATION_PARK_MS     (5000U)
#define PS_LIS2DUX12_PUBLIC_ADDR_7BIT (0x18U)
#define PS_LIS2DUX12_ALT_ADDR_7BIT    (0x19U)
#define PS_LIS2DUX12_HAL_ADDR_SHIFTED (PS_LIS2DUX12_PUBLIC_ADDR_7BIT << 1)
#define PS_LIS2DUX12_ALT_HAL_SHIFTED  (PS_LIS2DUX12_ALT_ADDR_7BIT << 1)
#define PS_LIS2DUX12_I2C_TIMEOUT_MS   (25U)
#define PS_TMAG3001_FIRST_ADDR_7BIT   (0x34U)
#define PS_TMAG3001_SCAN_COUNT        (4U)
#define PS_TMAG3001_I2C_TIMEOUT_MS    (25U)
#define PS_TMAG3001_REG_DEVICE_CONFIG_1 (0x00U)
#define PS_TMAG3001_REG_DEVICE_CONFIG_2 (0x01U)
#define PS_TMAG3001_REG_SENSOR_CONFIG_1 (0x02U)
#define PS_TMAG3001_REG_SENSOR_CONFIG_2 (0x03U)
#define PS_TMAG3001_REG_DEVICE_ID     (0x0DU)
#define PS_TMAG3001_REG_MFR_ID_LSB    (0x0EU)
#define PS_TMAG3001_REG_MFR_ID_MSB    (0x0FU)
#define PS_TMAG3001_REG_X_RESULT_MSB  (0x12U)
#define PS_TMAG3001_REG_CONV_STATUS   (0x18U)
#define PS_TMAG3001_REG_DEVICE_STATUS (0x1CU)
#define PS_TMAG3001_RESULT_WINDOW_LEN  (7U)
#define PS_TMAG3001_CAPTURE_PREP_MS   (5000U)
#define PS_TMAG3001_CAPTURE_PERIOD_MS (100U)
#define PS_TMAG3001_CAPTURE_CUE_HZ    (1600UL)
#define PS_TMAG3001_CAPTURE_CUE_MS    (300U)
#define PS_TMAG3001_ACTIVE_DEVICE_CONFIG_1 DEVICE_CONFIG_1_CONV_AVG_1xAverage
#define PS_TMAG3001_ACTIVE_SENSOR_CONFIG_1 SENSOR_CONFIG_1_MAG_CH_EN_XYZ
#define PS_TMAG3001_ACTIVE_SENSOR_CONFIG_2 SENSOR_CONFIG_2_DEFAULT
#define PS_TMAG3001_ACTIVE_DEVICE_CONFIG_2 DEVICE_CONFIG_2_OPERATING_MODE_Continuous
#define PS_TMAG3001_ACTIVE_SETTLE_MS   (10U)
#define PS_I2C_SCAN_FIRST_ADDR_7BIT   (0x08U)
#define PS_I2C_SCAN_LAST_ADDR_7BIT    (0x77U)
#define PS_NINA_RESET_ASSERT_MS       (20U)
#define PS_NINA_BOOT_WAIT_MS          (750U)
#define PS_NINA_RX_WINDOW_MS          (500U)
#define PS_NINA_UART_BYTE_TIMEOUT_MS  (10U)
#define PS_NINA_TX_TIMEOUT_MS         (250U)
#define PS_NINA_DISCOVERY_COMMAND_COUNT (12U)
#define PS_NINA_DISCOVERY_RX_BYTES    (96U)
#define PS_NINA_PHONE_CONNECT_WINDOW_MS (120000U)
#define PS_NINA_PHONE_DATA_WINDOW_MS  (120000U)
#define PS_BUTTON_MASK_START          (1UL << 0)
#define PS_BUTTON_MASK_A              (1UL << 1)
#define PS_BUTTON_MASK_B              (1UL << 2)
#define PS_BUTTON_MASK_L              (1UL << 3)
#define PS_BUTTON_MASK_R              (1UL << 4)
#define PS_LIGHT_SETTLE_MS            (5U)
#define PS_LIGHT_SAMPLE_PERIOD_MS     (250U)
#define PS_LIGHT_ADC_TIMEOUT_MS       (20U)
#define PS_DISPLAY_SETTLE_MS          (5U)
#define PS_DISPLAY_FRAME_PERIOD_MS    (1000U)
#define PS_DISPLAY_DMA_TIMEOUT_MS     (1000U)
#define PS_DISPLAY_PARTIAL_START_ROW  (73U)
#define PS_DISPLAY_PARTIAL_ROW_COUNT  (24U)
#define PS_OSPI_TIMEOUT_MS            (100U)
#define PS_OSPI_DMA_TIMEOUT_MS        (100U)
#define PS_AT25_BUSY_TIMEOUT_MS       (1000U)
#define PS_AT25_SCRATCH_ADDRESS       (0x00FFF000UL)
#define PS_AT25_TEST_LENGTH           (256U)
#define PS_AT25_CMD_RELEASE_DPD       (0xABU)
#define PS_AT25_CMD_DEEP_POWER_DOWN   (0xB9U)
#define PS_AT25_CMD_WRITE_ENABLE      (0x06U)
#define PS_AT25_CMD_READ_JEDEC_ID     (0x9FU)
#define PS_AT25_CMD_READ_STATUS1      (0x05U)
#define PS_AT25_CMD_READ_STATUS2      (0x35U)
#define PS_AT25_CMD_READ_STATUS3      (0x15U)
#define PS_AT25_CMD_READ_DATA         (0x03U)
#define PS_AT25_CMD_PAGE_PROGRAM      (0x02U)
#define PS_AT25_CMD_SECTOR_ERASE_4K   (0x20U)
#define PS_AT25_STATUS1_WIP           (0x01U)
#define PS_BBB_LPTIM_KERNEL_HZ        (4000000UL)
#define PS_BBB_SWEEP_START_HZ         (800UL)
#define PS_BBB_SWEEP_END_HZ           (4000UL)
#define PS_BBB_SWEEP_STEPS            (24UL)
#define PS_BBB_SWEEP_STEP_MS          (35U)
#define PS_BBB_PATTERN_STEPS          (6UL)
#define PS_BBB_PATTERN_TONE_MS        (90U)
#define PS_BBB_PATTERN_GAP_MS         (70U)
#define PS_SPEAKER_SAMPLE_RATE_HZ     (16000UL)
#define PS_SPEAKER_SWEEP_START_HZ     (250UL)
#define PS_SPEAKER_SWEEP_END_HZ       (1000UL)
#define PS_SPEAKER_SWEEP_STEPS        (16UL)
#define PS_SPEAKER_SWEEP_STEP_MS      (2000U)
#define PS_SPEAKER_SWEEP_GAP_MS       (500U)
#define PS_SPEAKER_SWEEP_AMPLITUDE    (4500)
#define PS_SPEAKER_BUFFER_FRAMES      (1024UL)
#define PS_SPEAKER_BUFFER_HALFWORDS   (PS_SPEAKER_BUFFER_FRAMES * 2UL)
#define PS_SPEAKER_WAVE_SINE          (1UL)

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;

I2C_HandleTypeDef hi2c3;

LPTIM_HandleTypeDef hlptim1;

UART_HandleTypeDef hlpuart1;

OSPI_HandleTypeDef hospi1;
DMA_HandleTypeDef handle_GPDMA1_Channel5;
DMA_HandleTypeDef handle_GPDMA1_Channel4;

SAI_HandleTypeDef hsai_BlockA1;
DMA_NodeTypeDef Node_GPDMA1_Channel3;
DMA_QListTypeDef List_GPDMA1_Channel3;
DMA_HandleTypeDef handle_GPDMA1_Channel3;

SPI_HandleTypeDef hspi3;
DMA_HandleTypeDef handle_LPDMA1_Channel0;

/* USER CODE BEGIN PV */
volatile PsPhase0bPmicProbe g_ps_phase0b_pmic_probe;
volatile PsPhase0ResetClockProbe g_ps_phase0_reset_clock_probe;
volatile PsPhase1DisplayBaselineProbe g_ps_phase1_display_baseline_probe;
volatile PsPhase1DisplaySelfTestProbe g_ps_phase1_display_selftest_probe;
volatile PsPhase2StorageProbe g_ps_phase2_storage_probe;
volatile PsPhase3AudioProbe g_ps_phase3_audio_probe;
volatile PsPhase4ButtonProbe g_ps_phase4_button_probe;
volatile PsPhase5LightProbe g_ps_phase5_light_probe;
volatile PsPhase4ImuProbe g_ps_phase4_imu_probe;
volatile PsPhase4TmagProbe g_ps_phase4_tmag_probe;
volatile PsPhase4TmagCaptureProbe g_ps_phase4_tmag_capture_probe;
volatile PsPhase7NinaProbe g_ps_phase7_nina_probe;
static volatile uint8_t g_ps_ospi_rx_dma_done;
static volatile uint8_t g_ps_ospi_tx_dma_done;
static volatile uint8_t g_ps_ospi_dma_error;
static uint16_t g_ps_speaker_dma_buffer[PS_SPEAKER_BUFFER_HALFWORDS];
static LS013B7DH05 g_ps_phase1_lcd;
static PsDisplayRenderer g_ps_phase1_display_renderer;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
void PeriphCommonClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_LPDMA1_Init(void);
static void MX_GPDMA1_Init(void);
static void MX_LPTIM1_Init(void);
static void MX_ADC1_Init(void);
static void MX_I2C3_Init(void);
static void MX_ICACHE_Init(void);
static void MX_LPUART1_UART_Init(void);
static void MX_OCTOSPI1_Init(void);
static void MX_SAI1_Init(void);
static void MX_SPI3_Init(void);
/* USER CODE BEGIN PFP */
static void PS_Phase0B_RunPmicProbe(void);
static uint16_t PS_Phase0B_DecodeBuckTargetMv(uint8_t raw);
static uint16_t PS_Phase0B_DecodeBuckBoostTargetMv(uint8_t raw);
static void PS_Phase0_RecordResetClockProbe(void);
static void PS_Phase1_RecordDisplayBaselineProbe(void);
static void PS_Phase1_DisplaySelfTestStep(void);
static void PS_Phase2_RunStorageProbe(void);
static void __attribute__((unused)) PS_Phase3_RunAudioProbe(void);
static void PS_Phase4_InitButtonProbe(void);
static uint32_t PS_Phase4_ReadButtonMask(void);
static void PS_Phase5_InitLightProbe(void);
static void PS_Phase5_LightProbeStep(void);
static HAL_StatusTypeDef PS_Phase5_ReadLightSample(uint16_t *sample);
static void PS_Phase4_RunImuIdentityProbe(void);
static int32_t PS_LIS2DUX12_ReadReg(void *handle, uint8_t reg, uint8_t *data, uint16_t len);
static int32_t PS_LIS2DUX12_WriteReg(void *handle, uint8_t reg, const uint8_t *data, uint16_t len);
static HAL_StatusTypeDef PS_LIS2DUX12_ReadRaw(uint8_t reg, uint8_t *data);
static void PS_Phase4_RunTmagIdentityProbe(void);
static HAL_StatusTypeDef PS_TMAG3001_ReadRaw(uint8_t address, uint8_t reg, uint8_t *data);
static void PS_Phase4_RunTmagCaptureProbe(void);
static HAL_StatusTypeDef PS_TMAG3001_ReadResultWindow(uint8_t address, uint8_t *data);
static HAL_StatusTypeDef PS_Phase4_TmagCaptureCue(uint32_t count);
static void __attribute__((unused)) PS_Phase7_RunNinaProbe(void);
static void PS_Phase7_NinaCommandProbe(uint32_t index, const uint8_t *command, uint16_t length);
static uint16_t PS_Phase7_NinaReceiveWindow(uint8_t *buffer, uint16_t capacity, uint32_t window_ms);
static uint16_t PS_Phase7_NinaReceiveUntilPhoneSps(uint8_t *buffer, uint16_t capacity, uint32_t window_ms);
static uint8_t PS_Phase7_CommandBuffersContain(const char *needle);
static uint16_t PS_Phase7_NinaReceiveEchoWindow(uint8_t *buffer, uint16_t capacity, uint32_t window_ms);
static uint8_t PS_Phase7_BufferContains(const uint8_t *buffer, uint16_t count, const char *needle);
static void PS_RunDeepIsolationStop2Probe(void);
static void PS_Phase3_PrepareSpeakerBuffer(uint32_t waveform, int16_t amplitude, uint32_t frequency_hz);
static HAL_StatusTypeDef PS_Phase3_RunSpeakerSegment(volatile PsPhase3AudioProbe *probe,
                                                     uint32_t segment_index,
                                                     uint32_t waveform,
                                                     int16_t amplitude,
                                                     uint32_t frequency_hz,
                                                     uint32_t duration_ms);
static HAL_StatusTypeDef __attribute__((unused)) PS_Phase3_RunBbbTone(volatile PsPhase3AudioProbe *probe,
                                                                      uint32_t step_index,
                                                                      uint32_t frequency_hz,
                                                                      uint32_t duration_ms);
static HAL_StatusTypeDef PS_Ospi_CommandOnly(uint8_t instruction);
static HAL_StatusTypeDef PS_Ospi_CommandAddress(uint8_t instruction, uint32_t address);
static HAL_StatusTypeDef PS_Ospi_Read(uint8_t instruction, uint8_t *data, uint32_t length);
static HAL_StatusTypeDef PS_Ospi_ReadAddress(uint8_t instruction,
                                             uint32_t address,
                                             uint8_t *data,
                                             uint32_t length);
static HAL_StatusTypeDef PS_AT25_WaitWhileBusy(uint32_t timeout_ms);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
static void PS_RunDeepIsolationStop2Probe(void)
{
  GPIO_InitTypeDef gpio = {0};

  HAL_Delay(PS_DEEP_ISOLATION_BASELINE_MS);

  HAL_GPIO_WritePin(PHOT_EN_GPIO_Port, PHOT_EN_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(SD_MODE_GPIO_Port, SD_MODE_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(NINA_NRST_GPIO_Port, NINA_NRST_Pin, GPIO_PIN_RESET);

  HAL_UART_DeInit(&hlpuart1);
  HAL_ADC_DeInit(&hadc1);
  HAL_OSPI_DeInit(&hospi1);
  HAL_SPI_DeInit(&hspi3);
  HAL_LPTIM_PWM_Stop(&hlptim1, LPTIM_CHANNEL_1);
  HAL_LPTIM_DeInit(&hlptim1);
  HAL_SAI_DMAStop(&hsai_BlockA1);
  HAL_SAI_DeInit(&hsai_BlockA1);
  HAL_ICACHE_Disable();

  gpio.Mode = GPIO_MODE_ANALOG;
  gpio.Pull = GPIO_NOPULL;

  gpio.Pin = GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3 | GPIO_PIN_4 |
             GPIO_PIN_5 | GPIO_PIN_9 | GPIO_PIN_11 | GPIO_PIN_12;
  HAL_GPIO_Init(GPIOA, &gpio);

  gpio.Pin = GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15 |
             GPIO_PIN_4;
  HAL_GPIO_Init(GPIOB, &gpio);

  gpio.Pin = NINA_SW1_Pin | NINA_SW2_Pin | NINA_DTR_Pin | NINA_DSR_Pin |
             GPIO_PIN_11 | GPIO_PIN_15;
  HAL_GPIO_Init(GPIOC, &gpio);

  HAL_Delay(PS_DEEP_ISOLATION_PARK_MS);

  HAL_I2C_DeInit(&hi2c3);

  __HAL_RCC_PLL2_DISABLE();
  __HAL_RCC_HSI_DISABLE();
  __HAL_RCC_MSIKSTOP_DISABLE();
  __HAL_RCC_MSIK_DISABLE();

  HAL_DBGMCU_DisableDBGStopMode();
  HAL_PWREx_EnableUltraLowPowerMode();
  HAL_SuspendTick();
  SysTick->CTRL = 0U;

  NVIC->ICER[0] = 0xFFFFFFFFUL;
  NVIC->ICER[1] = 0xFFFFFFFFUL;
  NVIC->ICPR[0] = 0xFFFFFFFFUL;
  NVIC->ICPR[1] = 0xFFFFFFFFUL;
  __HAL_PWR_CLEAR_FLAG(PWR_FLAG_STOPF);
  SET_BIT(PWR->WUSCR, PWR_WUSCR_CWUF);

  __disable_irq();
  __DSB();
  __ISB();

  for (;;)
  {
    HAL_PWREx_EnterSTOP2Mode(PWR_STOPENTRY_WFI);
  }
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* Configure the peripherals common clocks */
  PeriphCommonClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_LPDMA1_Init();
  MX_GPDMA1_Init();
  MX_LPTIM1_Init();
  MX_ADC1_Init();
  MX_I2C3_Init();
  MX_ICACHE_Init();
  MX_LPUART1_UART_Init();
  MX_OCTOSPI1_Init();
  MX_SAI1_Init();
  MX_SPI3_Init();
  /* USER CODE BEGIN 2 */
  if (PS_FW0_RUN_PMIC_PROBE != 0U)
  {
    PS_Phase0B_RunPmicProbe();
  }
  if (PS_FW0_RUN_RESET_CLOCK_PROBE != 0U)
  {
    PS_Phase0_RecordResetClockProbe();
  }
  if (PS_FW0_RUN_DISPLAY_BASELINE != 0U)
  {
    PS_Phase1_RecordDisplayBaselineProbe();
  }
  if (PS_FW0_RUN_STORAGE_PROBE != 0U)
  {
    PS_Phase2_RunStorageProbe();
  }
  if (PS_FW0_RUN_BUTTON_PROBE != 0U)
  {
    PS_Phase4_InitButtonProbe();
  }
  if (PS_FW0_RUN_LIGHT_INIT_PROBE != 0U)
  {
    PS_Phase5_InitLightProbe();
  }
  if (PS_FW0_RUN_IMU_ID_PROBE != 0U)
  {
    PS_Phase4_RunImuIdentityProbe();
  }
  if (PS_FW0_RUN_TMAG_ID_PROBE != 0U)
  {
    PS_Phase4_RunTmagIdentityProbe();
  }
  if (PS_FW0_RUN_TMAG_CAPTURE != 0U)
  {
    PS_Phase4_RunTmagCaptureProbe();
  }
  if (PS_FW0_RUN_AUDIO_PROBE != 0U)
  {
    PS_Phase3_RunAudioProbe();
  }
  if (PS_FW0_RUN_NINA_PROBE != 0U)
  {
    PS_Phase7_RunNinaProbe();
  }
  if (PS_FW0_RUN_DEEP_ISOLATION != 0U)
  {
    PS_RunDeepIsolationStop2Probe();
  }

  /* USER CODE END 2 */

  MX_ThreadX_Init();

  /* We should never get here as control is now taken by the scheduler */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    if (PS_FW0_RUN_DISPLAY_SELFTEST != 0U)
    {
      PS_Phase1_DisplaySelfTestStep();
    }
    if (PS_FW0_RUN_LIGHT_PERIODIC != 0U)
    {
      PS_Phase5_LightProbeStep();
    }
    HAL_Delay(10U);
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE3) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI|RCC_OSCILLATORTYPE_LSE
                              |RCC_OSCILLATORTYPE_MSI|RCC_OSCILLATORTYPE_MSIK;
  RCC_OscInitStruct.LSEState = RCC_LSE_BYPASS;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.MSIState = RCC_MSI_ON;
  RCC_OscInitStruct.MSICalibrationValue = RCC_MSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.MSIClockRange = RCC_MSIRANGE_1;
  RCC_OscInitStruct.MSIKClockRange = RCC_MSIKRANGE_4;
  RCC_OscInitStruct.MSIKState = RCC_MSIK_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2
                              |RCC_CLOCKTYPE_PCLK3;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_MSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB3CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }

  /** Enable the force of MSIK in stop mode
  */
  __HAL_RCC_MSIKSTOP_ENABLE();
}

/**
  * @brief Peripherals Common Clock Configuration
  * @retval None
  */
void PeriphCommonClock_Config(void)
{
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Initializes the common periph clock
  */
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_OSPI|RCC_PERIPHCLK_SAI1;
  PeriphClkInit.Sai1ClockSelection = RCC_SAI1CLKSOURCE_PLL2;
  PeriphClkInit.OspiClockSelection = RCC_OSPICLKSOURCE_PLL2;
  PeriphClkInit.PLL2.PLL2Source = RCC_PLLSOURCE_HSI;
  PeriphClkInit.PLL2.PLL2M = 1;
  PeriphClkInit.PLL2.PLL2N = 32;
  PeriphClkInit.PLL2.PLL2P = 125;
  PeriphClkInit.PLL2.PLL2Q = 4;
  PeriphClkInit.PLL2.PLL2R = 2;
  PeriphClkInit.PLL2.PLL2RGE = RCC_PLLVCIRANGE_1;
  PeriphClkInit.PLL2.PLL2FRACN = 0;
  PeriphClkInit.PLL2.PLL2ClockOut = RCC_PLL2_DIVP|RCC_PLL2_DIVQ;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief ADC1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC1_Init(void)
{

  /* USER CODE BEGIN ADC1_Init 0 */

  /* USER CODE END ADC1_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC1_Init 1 */

  /* USER CODE END ADC1_Init 1 */

  /** Common config
  */
  hadc1.Instance = ADC1;
  hadc1.Init.ClockPrescaler = ADC_CLOCK_ASYNC_DIV1;
  hadc1.Init.Resolution = ADC_RESOLUTION_14B;
  hadc1.Init.GainCompensation = 0;
  hadc1.Init.ScanConvMode = ADC_SCAN_DISABLE;
  hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  hadc1.Init.LowPowerAutoWait = DISABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;
  hadc1.Init.NbrOfConversion = 1;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc1.Init.DMAContinuousRequests = DISABLE;
  hadc1.Init.TriggerFrequencyMode = ADC_TRIGGER_FREQ_HIGH;
  hadc1.Init.Overrun = ADC_OVR_DATA_PRESERVED;
  hadc1.Init.LeftBitShift = ADC_LEFTBITSHIFT_NONE;
  hadc1.Init.ConversionDataManagement = ADC_CONVERSIONDATA_DR;
  hadc1.Init.OversamplingMode = DISABLE;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_4;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLETIME_814CYCLES;
  sConfig.SingleDiff = ADC_SINGLE_ENDED;
  sConfig.OffsetNumber = ADC_OFFSET_NONE;
  sConfig.Offset = 0;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */

  /* USER CODE END ADC1_Init 2 */

}

/**
  * @brief GPDMA1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPDMA1_Init(void)
{

  /* USER CODE BEGIN GPDMA1_Init 0 */

  /* USER CODE END GPDMA1_Init 0 */

  /* Peripheral clock enable */
  __HAL_RCC_GPDMA1_CLK_ENABLE();

  /* GPDMA1 interrupt Init */
    HAL_NVIC_SetPriority(GPDMA1_Channel3_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(GPDMA1_Channel3_IRQn);
    HAL_NVIC_SetPriority(GPDMA1_Channel4_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(GPDMA1_Channel4_IRQn);
    HAL_NVIC_SetPriority(GPDMA1_Channel5_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(GPDMA1_Channel5_IRQn);

  /* USER CODE BEGIN GPDMA1_Init 1 */

  /* USER CODE END GPDMA1_Init 1 */
  /* USER CODE BEGIN GPDMA1_Init 2 */

  /* USER CODE END GPDMA1_Init 2 */

}

/**
  * @brief I2C3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C3_Init(void)
{

  /* USER CODE BEGIN I2C3_Init 0 */

  /* USER CODE END I2C3_Init 0 */

  /* USER CODE BEGIN I2C3_Init 1 */

  /* USER CODE END I2C3_Init 1 */
  hi2c3.Instance = I2C3;
  hi2c3.Init.Timing = 0x00506682;
  hi2c3.Init.OwnAddress1 = 0;
  hi2c3.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c3.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c3.Init.OwnAddress2 = 0;
  hi2c3.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  hi2c3.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c3.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c3) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Analogue filter
  */
  if (HAL_I2CEx_ConfigAnalogFilter(&hi2c3, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Digital filter
  */
  if (HAL_I2CEx_ConfigDigitalFilter(&hi2c3, 0) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C3_Init 2 */

  /* USER CODE END I2C3_Init 2 */

}

/**
  * @brief ICACHE Initialization Function
  * @param None
  * @retval None
  */
static void MX_ICACHE_Init(void)
{

  /* USER CODE BEGIN ICACHE_Init 0 */

  /* USER CODE END ICACHE_Init 0 */

  /* USER CODE BEGIN ICACHE_Init 1 */

  /* USER CODE END ICACHE_Init 1 */

  /** Enable instruction cache in 1-way (direct mapped cache)
  */
  if (HAL_ICACHE_ConfigAssociativityMode(ICACHE_1WAY) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_ICACHE_Enable() != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ICACHE_Init 2 */

  /* USER CODE END ICACHE_Init 2 */

}

/**
  * @brief LPDMA1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_LPDMA1_Init(void)
{

  /* USER CODE BEGIN LPDMA1_Init 0 */

  /* USER CODE END LPDMA1_Init 0 */

  /* Peripheral clock enable */
  __HAL_RCC_LPDMA1_CLK_ENABLE();

  /* LPDMA1 interrupt Init */
    HAL_NVIC_SetPriority(LPDMA1_Channel0_IRQn, 15, 0);
    HAL_NVIC_EnableIRQ(LPDMA1_Channel0_IRQn);

  /* USER CODE BEGIN LPDMA1_Init 1 */

  /* USER CODE END LPDMA1_Init 1 */
  /* USER CODE BEGIN LPDMA1_Init 2 */

  /* USER CODE END LPDMA1_Init 2 */

}

/**
  * @brief LPTIM1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_LPTIM1_Init(void)
{

  /* USER CODE BEGIN LPTIM1_Init 0 */

  /* USER CODE END LPTIM1_Init 0 */

  LPTIM_OC_ConfigTypeDef sConfig1 = {0};

  /* USER CODE BEGIN LPTIM1_Init 1 */

  /* USER CODE END LPTIM1_Init 1 */
  hlptim1.Instance = LPTIM1;
  hlptim1.Init.Clock.Source = LPTIM_CLOCKSOURCE_APBCLOCK_LPOSC;
  hlptim1.Init.Clock.Prescaler = LPTIM_PRESCALER_DIV1;
  hlptim1.Init.Trigger.Source = LPTIM_TRIGSOURCE_SOFTWARE;
  hlptim1.Init.Period = 65535;
  hlptim1.Init.UpdateMode = LPTIM_UPDATE_IMMEDIATE;
  hlptim1.Init.CounterSource = LPTIM_COUNTERSOURCE_INTERNAL;
  hlptim1.Init.Input1Source = LPTIM_INPUT1SOURCE_GPIO;
  hlptim1.Init.Input2Source = LPTIM_INPUT2SOURCE_GPIO;
  hlptim1.Init.RepetitionCounter = 0;
  if (HAL_LPTIM_Init(&hlptim1) != HAL_OK)
  {
    Error_Handler();
  }
  sConfig1.Pulse = 0;
  sConfig1.OCPolarity = LPTIM_OCPOLARITY_HIGH;
  if (HAL_LPTIM_OC_ConfigChannel(&hlptim1, &sConfig1, LPTIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN LPTIM1_Init 2 */

  /* USER CODE END LPTIM1_Init 2 */
  HAL_LPTIM_MspPostInit(&hlptim1);

}

/**
  * @brief LPUART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_LPUART1_UART_Init(void)
{

  /* USER CODE BEGIN LPUART1_Init 0 */

  /* USER CODE END LPUART1_Init 0 */

  /* USER CODE BEGIN LPUART1_Init 1 */

  /* USER CODE END LPUART1_Init 1 */
  hlpuart1.Instance = LPUART1;
  hlpuart1.Init.BaudRate = 115200;
  hlpuart1.Init.WordLength = UART_WORDLENGTH_8B;
  hlpuart1.Init.StopBits = UART_STOPBITS_1;
  hlpuart1.Init.Parity = UART_PARITY_NONE;
  hlpuart1.Init.Mode = UART_MODE_TX_RX;
  hlpuart1.Init.HwFlowCtl = UART_HWCONTROL_RTS_CTS;
  hlpuart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  hlpuart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  hlpuart1.FifoMode = UART_FIFOMODE_DISABLE;
  if (HAL_UART_Init(&hlpuart1) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetTxFifoThreshold(&hlpuart1, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetRxFifoThreshold(&hlpuart1, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_DisableFifoMode(&hlpuart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN LPUART1_Init 2 */

  /* USER CODE END LPUART1_Init 2 */

}

/**
  * @brief OCTOSPI1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_OCTOSPI1_Init(void)
{

  /* USER CODE BEGIN OCTOSPI1_Init 0 */

  /* USER CODE END OCTOSPI1_Init 0 */

  OSPIM_CfgTypeDef sOspiManagerCfg = {0};
  HAL_OSPI_DLYB_CfgTypeDef HAL_OSPI_DLYB_Cfg_Struct = {0};

  /* USER CODE BEGIN OCTOSPI1_Init 1 */

  /* USER CODE END OCTOSPI1_Init 1 */
  /* OCTOSPI1 parameter configuration*/
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
  if (HAL_OSPI_Init(&hospi1) != HAL_OK)
  {
    Error_Handler();
  }
  sOspiManagerCfg.ClkPort = 1;
  sOspiManagerCfg.NCSPort = 2;
  sOspiManagerCfg.IOLowPort = HAL_OSPIM_IOPORT_1_LOW;
  if (HAL_OSPIM_Config(&hospi1, &sOspiManagerCfg, HAL_OSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
  {
    Error_Handler();
  }
  HAL_OSPI_DLYB_Cfg_Struct.Units = 0;
  HAL_OSPI_DLYB_Cfg_Struct.PhaseSel = 0;
  if (HAL_OSPI_DLYB_SetConfig(&hospi1, &HAL_OSPI_DLYB_Cfg_Struct) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN OCTOSPI1_Init 2 */

  /* USER CODE END OCTOSPI1_Init 2 */

}

/**
  * @brief SAI1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SAI1_Init(void)
{

  /* USER CODE BEGIN SAI1_Init 0 */

  /* USER CODE END SAI1_Init 0 */

  /* USER CODE BEGIN SAI1_Init 1 */

  /* USER CODE END SAI1_Init 1 */
  hsai_BlockA1.Instance = SAI1_Block_A;
  hsai_BlockA1.Init.AudioMode = SAI_MODEMASTER_TX;
  hsai_BlockA1.Init.Synchro = SAI_ASYNCHRONOUS;
  hsai_BlockA1.Init.OutputDrive = SAI_OUTPUTDRIVE_DISABLE;
  hsai_BlockA1.Init.NoDivider = SAI_MASTERDIVIDER_ENABLE;
  hsai_BlockA1.Init.FIFOThreshold = SAI_FIFOTHRESHOLD_EMPTY;
  hsai_BlockA1.Init.AudioFrequency = SAI_AUDIO_FREQUENCY_192K;
  hsai_BlockA1.Init.SynchroExt = SAI_SYNCEXT_DISABLE;
  hsai_BlockA1.Init.MckOutput = SAI_MCK_OUTPUT_DISABLE;
  hsai_BlockA1.Init.MonoStereoMode = SAI_STEREOMODE;
  hsai_BlockA1.Init.CompandingMode = SAI_NOCOMPANDING;
  hsai_BlockA1.Init.TriState = SAI_OUTPUT_NOTRELEASED;
  if (HAL_SAI_InitProtocol(&hsai_BlockA1, SAI_I2S_STANDARD, SAI_PROTOCOL_DATASIZE_16BIT, 2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SAI1_Init 2 */

  /* USER CODE END SAI1_Init 2 */

}

/**
  * @brief SPI3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI3_Init(void)
{

  /* USER CODE BEGIN SPI3_Init 0 */

  /* USER CODE END SPI3_Init 0 */

  SPI_AutonomousModeConfTypeDef HAL_SPI_AutonomousMode_Cfg_Struct = {0};

  /* USER CODE BEGIN SPI3_Init 1 */

  /* USER CODE END SPI3_Init 1 */
  /* SPI3 parameter configuration*/
  hspi3.Instance = SPI3;
  hspi3.Init.Mode = SPI_MODE_MASTER;
  hspi3.Init.Direction = SPI_DIRECTION_1LINE;
  hspi3.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi3.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi3.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi3.Init.NSS = SPI_NSS_HARD_OUTPUT;
  hspi3.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_4;
  hspi3.Init.FirstBit = SPI_FIRSTBIT_LSB;
  hspi3.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi3.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi3.Init.CRCPolynomial = 0x7;
  hspi3.Init.NSSPMode = SPI_NSS_PULSE_DISABLE;
  hspi3.Init.NSSPolarity = SPI_NSS_POLARITY_HIGH;
  hspi3.Init.FifoThreshold = SPI_FIFO_THRESHOLD_01DATA;
  hspi3.Init.MasterSSIdleness = SPI_MASTER_SS_IDLENESS_15CYCLE;
  hspi3.Init.MasterInterDataIdleness = SPI_MASTER_INTERDATA_IDLENESS_00CYCLE;
  hspi3.Init.MasterReceiverAutoSusp = SPI_MASTER_RX_AUTOSUSP_DISABLE;
  hspi3.Init.MasterKeepIOState = SPI_MASTER_KEEP_IO_STATE_DISABLE;
  hspi3.Init.IOSwap = SPI_IO_SWAP_DISABLE;
  hspi3.Init.ReadyMasterManagement = SPI_RDY_MASTER_MANAGEMENT_INTERNALLY;
  hspi3.Init.ReadyPolarity = SPI_RDY_POLARITY_HIGH;
  if (HAL_SPI_Init(&hspi3) != HAL_OK)
  {
    Error_Handler();
  }
  HAL_SPI_AutonomousMode_Cfg_Struct.TriggerState = SPI_AUTO_MODE_DISABLE;
  HAL_SPI_AutonomousMode_Cfg_Struct.TriggerSelection = SPI_GRP2_LPTIM1_CH1_TRG;
  HAL_SPI_AutonomousMode_Cfg_Struct.TriggerPolarity = SPI_TRIG_POLARITY_RISING;
  if (HAL_SPIEx_SetConfigAutonomousMode(&hspi3, &HAL_SPI_AutonomousMode_Cfg_Struct) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI3_Init 2 */

  /* USER CODE END SPI3_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
/* USER CODE BEGIN MX_GPIO_Init_1 */
/* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, PHOT_EN_Pin|NINA_NRST_Pin|SD_MODE_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(VLT_LCD_GPIO_Port, VLT_LCD_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pins : PC15 NINA_SW1_Pin NINA_SW2_Pin NINA_DTR_Pin
                           NINA_DSR_Pin PC11 */
  GPIO_InitStruct.Pin = GPIO_PIN_15|NINA_SW1_Pin|NINA_SW2_Pin|NINA_DTR_Pin
                          |NINA_DSR_Pin|GPIO_PIN_11;
  GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pins : PH0 PH1 PH3 */
  GPIO_InitStruct.Pin = GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_3;
  GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOH, &GPIO_InitStruct);

  /*Configure GPIO pins : PHOT_EN_Pin NINA_NRST_Pin SD_MODE_Pin */
  GPIO_InitStruct.Pin = PHOT_EN_Pin|NINA_NRST_Pin|SD_MODE_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pins : PA1 PA5 PA9 PA11
                           PA12 */
  GPIO_InitStruct.Pin = GPIO_PIN_1|GPIO_PIN_5|GPIO_PIN_9|GPIO_PIN_11
                          |GPIO_PIN_12;
  GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : BTN_START_Pin */
  GPIO_InitStruct.Pin = BTN_START_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING_FALLING;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(BTN_START_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : PB14 PB15 PB4 */
  GPIO_InitStruct.Pin = GPIO_PIN_14|GPIO_PIN_15|GPIO_PIN_4;
  GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : VLT_LCD_Pin */
  GPIO_InitStruct.Pin = VLT_LCD_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(VLT_LCD_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : BTN_A_Pin BTN_B_Pin BTN_L_Pin BTN_R_Pin */
  GPIO_InitStruct.Pin = BTN_A_Pin|BTN_B_Pin|BTN_L_Pin|BTN_R_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING_FALLING;
  GPIO_InitStruct.Pull = GPIO_PULLDOWN;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI4_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI4_IRQn);

  HAL_NVIC_SetPriority(EXTI5_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI5_IRQn);

  HAL_NVIC_SetPriority(EXTI6_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI6_IRQn);

  HAL_NVIC_SetPriority(EXTI7_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI7_IRQn);

  HAL_NVIC_SetPriority(EXTI8_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI8_IRQn);

/* USER CODE BEGIN MX_GPIO_Init_2 */
/* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
static void PS_Phase0B_RunPmicProbe(void)
{
  volatile PsPhase0bPmicProbe *probe = &g_ps_phase0b_pmic_probe;

  probe->magic = PS_PHASE0B_PROBE_MAGIC;
  probe->phase = PS_PHASE0B_PROBE_PHASE;
  probe->public_addr_7bit = PS_ADP5360_PUBLIC_ADDR_7BIT;
  probe->hal_addr_shifted = PS_ADP5360_HAL_ADDR_SHIFTED;
  probe->tick_start = HAL_GetTick();

  probe->ready_shifted_status = (uint8_t)HAL_I2C_IsDeviceReady(&hi2c3,
                                                               PS_ADP5360_HAL_ADDR_SHIFTED,
                                                               2,
                                                               PS_I2C_TIMEOUT_MS);
  probe->ready_shifted_error = HAL_I2C_GetError(&hi2c3);

  probe->ready_unshifted_status = (uint8_t)HAL_I2C_IsDeviceReady(&hi2c3,
                                                                 PS_ADP5360_PUBLIC_ADDR_7BIT,
                                                                 1,
                                                                 PS_I2C_TIMEOUT_MS);
  probe->ready_unshifted_error = HAL_I2C_GetError(&hi2c3);

  if (probe->ready_shifted_status == (uint8_t)HAL_OK)
  {
    probe->driver_init_status = (uint8_t)ADP5360_init();

    for (uint8_t reg = 0; reg < PS_ADP5360_REG_COUNT; reg++)
    {
      uint8_t value = 0U;
      HAL_StatusTypeDef status;

      if ((reg == PS_ADP5360_REG_INT_FLAG1) || (reg == PS_ADP5360_REG_INT_FLAG2))
      {
        continue;
      }

      status = ADP5360_read(reg, &value, 1U);
      probe->raw_reg[reg] = value;
      probe->raw_reg_status[reg] = (uint8_t)status;
      probe->raw_reg_error[reg] = HAL_I2C_GetError(&hi2c3);
      if (status == HAL_OK)
      {
        probe->read_count++;
      }
    }
  }

  probe->charger_status1 = probe->raw_reg[0x08U];
  probe->charger_status2 = probe->raw_reg[0x09U];
  probe->bat_soc_percent = probe->raw_reg[0x21U] & 0x7FU;
  uint16_t vbat_mv = 0U;
  (void)ADP5360_get_vbat(&vbat_mv, NULL);
  probe->vbat_mv = vbat_mv;
  probe->buck_config = probe->raw_reg[0x29U];
  uint16_t buck_target_mv = 0U;
  if (ADP5360_get_buck_vout(&buck_target_mv, NULL) == HAL_OK)
  {
    probe->buck_target_mv = buck_target_mv;
  }
  else
  {
    probe->buck_target_mv = PS_Phase0B_DecodeBuckTargetMv(probe->raw_reg[0x2AU]);
  }
  probe->buckboost_config = probe->raw_reg[0x2BU];
  uint16_t buckboost_target_mv = 0U;
  if (ADP5360_get_buckboost_vout(&buckboost_target_mv, NULL) == HAL_OK)
  {
    probe->buckboost_target_mv = buckboost_target_mv;
  }
  else
  {
    probe->buckboost_target_mv = PS_Phase0B_DecodeBuckBoostTargetMv(probe->raw_reg[0x2CU]);
  }
  uint8_t fault = 0U;
  (void)ADP5360_get_fault(&fault);
  probe->fault = fault;
  uint8_t pgood_status = 0U;
  (void)ADP5360_read_u8(ADP5360_REG_PGOOD_STATUS, &pgood_status);
  probe->pgood_status = pgood_status;
  probe->tick_end = HAL_GetTick();
  probe->complete = 1U;
}

static uint16_t PS_Phase0B_DecodeBuckTargetMv(uint8_t raw)
{
  return (uint16_t)(600U + ((uint16_t)(raw & 0x3FU) * 50U));
}

static uint16_t PS_Phase0B_DecodeBuckBoostTargetMv(uint8_t raw)
{
  uint8_t code = raw & 0x3FU;

  if (code <= 11U)
  {
    return (uint16_t)(1800U + ((uint16_t)code * 100U));
  }

  return (uint16_t)(2950U + ((uint16_t)(code - 12U) * 50U));
}

static void PS_Phase0_RecordResetClockProbe(void)
{
  volatile PsPhase0ResetClockProbe *probe = &g_ps_phase0_reset_clock_probe;

  probe->magic = PS_PHASE0_CLOCK_MAGIC;
  probe->phase = PS_PHASE0_CLOCK_PHASE;
  probe->tick = HAL_GetTick();
  probe->system_core_clock = SystemCoreClock;
  probe->sysclk_hz = HAL_RCC_GetSysClockFreq();
  probe->hclk_hz = HAL_RCC_GetHCLKFreq();
  probe->pclk1_hz = HAL_RCC_GetPCLK1Freq();
  probe->pclk2_hz = HAL_RCC_GetPCLK2Freq();
  probe->pclk3_hz = HAL_RCC_GetPCLK3Freq();
  probe->rcc_cr = RCC->CR;
  probe->rcc_csr = RCC->CSR;
  probe->rcc_cfgr1 = RCC->CFGR1;
  probe->rcc_cfgr2 = RCC->CFGR2;
  probe->rcc_cfgr3 = RCC->CFGR3;
  probe->rcc_icscr1 = RCC->ICSCR1;
  probe->pwr_vosr = PWR->VOSR;
  probe->flag_oblrst = (uint8_t)__HAL_RCC_GET_FLAG(RCC_FLAG_OBLRST);
  probe->flag_pinrst = (uint8_t)__HAL_RCC_GET_FLAG(RCC_FLAG_PINRST);
  probe->flag_borrst = (uint8_t)__HAL_RCC_GET_FLAG(RCC_FLAG_BORRST);
  probe->flag_sftrst = (uint8_t)__HAL_RCC_GET_FLAG(RCC_FLAG_SFTRST);
  probe->flag_iwdgrst = (uint8_t)__HAL_RCC_GET_FLAG(RCC_FLAG_IWDGRST);
  probe->flag_wwdgrst = (uint8_t)__HAL_RCC_GET_FLAG(RCC_FLAG_WWDGRST);
  probe->flag_lpwrst = (uint8_t)__HAL_RCC_GET_FLAG(RCC_FLAG_LPWRRST);
  probe->complete = 1U;
}

static void PS_Phase1_RecordDisplayBaselineProbe(void)
{
  volatile PsPhase1DisplayBaselineProbe *probe = &g_ps_phase1_display_baseline_probe;

  probe->magic = PS_PHASE1_DISPLAY_MAGIC;
  probe->phase = PS_PHASE1_DISPLAY_PHASE;
  probe->tick = HAL_GetTick();
  probe->spi3_state = (uint32_t)HAL_SPI_GetState(&hspi3);
  probe->rtc_state = 0xFFFFFFFFU;
  probe->spi3_kernel_hz = HAL_RCCEx_GetPeriphCLKFreq(RCC_PERIPHCLK_SPI3);
  probe->gpiod_odr = GPIOD->ODR;
  probe->gpiod_idr = GPIOD->IDR;
  probe->vlt_lcd_pin_state = (uint8_t)HAL_GPIO_ReadPin(VLT_LCD_GPIO_Port, VLT_LCD_Pin);
  probe->spi3_nssp_disabled = (hspi3.Init.NSSPMode == SPI_NSS_PULSE_DISABLE) ? 1U : 0U;
  probe->spi3_lsb_first = (hspi3.Init.FirstBit == SPI_FIRSTBIT_LSB) ? 1U : 0U;
  probe->spi3_nss_active_high = (hspi3.Init.NSSPolarity == SPI_NSS_POLARITY_HIGH) ? 1U : 0U;
  probe->complete = 1U;
}

static void PS_Phase1_DisplaySelfTestStep(void)
{
  static uint8_t initialized = 0U;
  static uint8_t phase = 0U;
  volatile PsPhase1DisplaySelfTestProbe *probe = &g_ps_phase1_display_selftest_probe;
  HAL_StatusTypeDef status;

  if (initialized == 0U)
  {
    probe->magic = PS_PHASE1_DISPLAY_SELFTEST_MAGIC;
    probe->phase = PS_PHASE1_DISPLAY_SELFTEST_PHASE;
    probe->tick_start = HAL_GetTick();
    probe->payload_len = (uint32_t)(1U + (DISPLAY_HEIGHT * (1U + LINE_WIDTH + 1U)) + 1U);

    HAL_GPIO_WritePin(VLT_LCD_GPIO_Port, VLT_LCD_Pin, GPIO_PIN_SET);
    HAL_Delay(PS_DISPLAY_SETTLE_MS);
    status = LCD_Init(&g_ps_phase1_lcd, &hspi3);
    if (status == HAL_OK)
    {
      status = PS_DisplayRenderer_Init(&g_ps_phase1_display_renderer,
                                       &g_ps_phase1_lcd,
                                       PS_DISPLAY_DMA_TIMEOUT_MS);
    }
    probe->last_tx_status = (uint8_t)status;
    probe->last_hal_error = HAL_SPI_GetError(&hspi3);
    if (status != HAL_OK)
    {
      probe->tx_error_count++;
      probe->complete = 1U;
      HAL_Delay(PS_DISPLAY_FRAME_PERIOD_MS);
      return;
    }
    initialized = 1U;
  }

  if (phase >= 2U)
  {
    probe->tick_last_tx = HAL_GetTick();
    probe->vlt_lcd_pin_state = (uint8_t)HAL_GPIO_ReadPin(VLT_LCD_GPIO_Port, VLT_LCD_Pin);
    probe->complete = 1U;
    HAL_Delay(PS_DISPLAY_FRAME_PERIOD_MS);
    return;
  }

  if (phase == 0U)
  {
    status = PS_DisplayRenderer_Fill(&g_ps_phase1_display_renderer, 0xAAU);
    if (status == HAL_OK)
    {
      status = PS_DisplayRenderer_Present(&g_ps_phase1_display_renderer);
    }
    probe->last_fill_value = 0xAAU;
  }
  else
  {
    status = PS_DisplayRenderer_FillRowRange(&g_ps_phase1_display_renderer,
                                             PS_DISPLAY_PARTIAL_START_ROW,
                                             PS_DISPLAY_PARTIAL_ROW_COUNT,
                                             0x00U);
    if (status == HAL_OK)
    {
      status = PS_DisplayRenderer_Present(&g_ps_phase1_display_renderer);
    }
    probe->last_fill_value = 0x00U;
  }

  probe->tick_last_tx = HAL_GetTick();
  probe->vlt_lcd_pin_state = (uint8_t)HAL_GPIO_ReadPin(VLT_LCD_GPIO_Port, VLT_LCD_Pin);
  probe->last_tx_status = (uint8_t)status;
  probe->last_hal_error = HAL_SPI_GetError(&hspi3);
  probe->frame_counter++;
  if (status == HAL_OK)
  {
    probe->tx_ok_count++;
  }
  else
  {
    probe->tx_error_count++;
  }
  probe->complete = 1U;

  if (status == HAL_OK)
  {
    phase++;
  }
  HAL_Delay(PS_DISPLAY_FRAME_PERIOD_MS);
}

static HAL_StatusTypeDef PS_Ospi_CommandOnly(uint8_t instruction)
{
  OSPI_RegularCmdTypeDef command = {0};

  command.OperationType = HAL_OSPI_OPTYPE_COMMON_CFG;
  command.FlashId = HAL_OSPI_FLASH_ID_1;
  command.Instruction = instruction;
  command.InstructionMode = HAL_OSPI_INSTRUCTION_1_LINE;
  command.InstructionSize = HAL_OSPI_INSTRUCTION_8_BITS;
  command.InstructionDtrMode = HAL_OSPI_INSTRUCTION_DTR_DISABLE;
  command.AddressMode = HAL_OSPI_ADDRESS_NONE;
  command.AddressDtrMode = HAL_OSPI_ADDRESS_DTR_DISABLE;
  command.AlternateBytesMode = HAL_OSPI_ALTERNATE_BYTES_NONE;
  command.AlternateBytesDtrMode = HAL_OSPI_ALTERNATE_BYTES_DTR_DISABLE;
  command.DataMode = HAL_OSPI_DATA_NONE;
  command.DataDtrMode = HAL_OSPI_DATA_DTR_DISABLE;
  command.DummyCycles = 0U;
  command.DQSMode = HAL_OSPI_DQS_DISABLE;
  command.SIOOMode = HAL_OSPI_SIOO_INST_EVERY_CMD;

  return HAL_OSPI_Command(&hospi1, &command, PS_OSPI_TIMEOUT_MS);
}

static HAL_StatusTypeDef PS_Ospi_CommandAddress(uint8_t instruction, uint32_t address)
{
  OSPI_RegularCmdTypeDef command = {0};

  command.OperationType = HAL_OSPI_OPTYPE_COMMON_CFG;
  command.FlashId = HAL_OSPI_FLASH_ID_1;
  command.Instruction = instruction;
  command.InstructionMode = HAL_OSPI_INSTRUCTION_1_LINE;
  command.InstructionSize = HAL_OSPI_INSTRUCTION_8_BITS;
  command.InstructionDtrMode = HAL_OSPI_INSTRUCTION_DTR_DISABLE;
  command.Address = address;
  command.AddressMode = HAL_OSPI_ADDRESS_1_LINE;
  command.AddressSize = HAL_OSPI_ADDRESS_24_BITS;
  command.AddressDtrMode = HAL_OSPI_ADDRESS_DTR_DISABLE;
  command.AlternateBytesMode = HAL_OSPI_ALTERNATE_BYTES_NONE;
  command.AlternateBytesDtrMode = HAL_OSPI_ALTERNATE_BYTES_DTR_DISABLE;
  command.DataMode = HAL_OSPI_DATA_NONE;
  command.DataDtrMode = HAL_OSPI_DATA_DTR_DISABLE;
  command.DummyCycles = 0U;
  command.DQSMode = HAL_OSPI_DQS_DISABLE;
  command.SIOOMode = HAL_OSPI_SIOO_INST_EVERY_CMD;

  return HAL_OSPI_Command(&hospi1, &command, PS_OSPI_TIMEOUT_MS);
}

static HAL_StatusTypeDef PS_Ospi_Read(uint8_t instruction, uint8_t *data, uint32_t length)
{
  OSPI_RegularCmdTypeDef command = {0};
  HAL_StatusTypeDef status;

  if ((data == NULL) || (length == 0U))
  {
    return HAL_ERROR;
  }

  command.OperationType = HAL_OSPI_OPTYPE_COMMON_CFG;
  command.FlashId = HAL_OSPI_FLASH_ID_1;
  command.Instruction = instruction;
  command.InstructionMode = HAL_OSPI_INSTRUCTION_1_LINE;
  command.InstructionSize = HAL_OSPI_INSTRUCTION_8_BITS;
  command.InstructionDtrMode = HAL_OSPI_INSTRUCTION_DTR_DISABLE;
  command.AddressMode = HAL_OSPI_ADDRESS_NONE;
  command.AddressDtrMode = HAL_OSPI_ADDRESS_DTR_DISABLE;
  command.AlternateBytesMode = HAL_OSPI_ALTERNATE_BYTES_NONE;
  command.AlternateBytesDtrMode = HAL_OSPI_ALTERNATE_BYTES_DTR_DISABLE;
  command.DataMode = HAL_OSPI_DATA_1_LINE;
  command.NbData = length;
  command.DataDtrMode = HAL_OSPI_DATA_DTR_DISABLE;
  command.DummyCycles = 0U;
  command.DQSMode = HAL_OSPI_DQS_DISABLE;
  command.SIOOMode = HAL_OSPI_SIOO_INST_EVERY_CMD;

  status = HAL_OSPI_Command(&hospi1, &command, PS_OSPI_TIMEOUT_MS);
  if (status != HAL_OK)
  {
    return status;
  }

  return HAL_OSPI_Receive(&hospi1, data, PS_OSPI_TIMEOUT_MS);
}

static HAL_StatusTypeDef PS_Ospi_ReadAddress(uint8_t instruction,
                                             uint32_t address,
                                             uint8_t *data,
                                             uint32_t length)
{
  OSPI_RegularCmdTypeDef command = {0};
  HAL_StatusTypeDef status;

  if ((data == NULL) || (length == 0U))
  {
    return HAL_ERROR;
  }

  command.OperationType = HAL_OSPI_OPTYPE_COMMON_CFG;
  command.FlashId = HAL_OSPI_FLASH_ID_1;
  command.Instruction = instruction;
  command.InstructionMode = HAL_OSPI_INSTRUCTION_1_LINE;
  command.InstructionSize = HAL_OSPI_INSTRUCTION_8_BITS;
  command.InstructionDtrMode = HAL_OSPI_INSTRUCTION_DTR_DISABLE;
  command.Address = address;
  command.AddressMode = HAL_OSPI_ADDRESS_1_LINE;
  command.AddressSize = HAL_OSPI_ADDRESS_24_BITS;
  command.AddressDtrMode = HAL_OSPI_ADDRESS_DTR_DISABLE;
  command.AlternateBytesMode = HAL_OSPI_ALTERNATE_BYTES_NONE;
  command.AlternateBytesDtrMode = HAL_OSPI_ALTERNATE_BYTES_DTR_DISABLE;
  command.DataMode = HAL_OSPI_DATA_1_LINE;
  command.NbData = length;
  command.DataDtrMode = HAL_OSPI_DATA_DTR_DISABLE;
  command.DummyCycles = 0U;
  command.DQSMode = HAL_OSPI_DQS_DISABLE;
  command.SIOOMode = HAL_OSPI_SIOO_INST_EVERY_CMD;

  status = HAL_OSPI_Command(&hospi1, &command, PS_OSPI_TIMEOUT_MS);
  if (status != HAL_OK)
  {
    return status;
  }

  return HAL_OSPI_Receive(&hospi1, data, PS_OSPI_TIMEOUT_MS);
}

static HAL_StatusTypeDef PS_Ospi_ReadAddress_DMA(uint8_t instruction,
                                                 uint32_t address,
                                                 uint8_t *data,
                                                 uint32_t length)
{
  OSPI_RegularCmdTypeDef command = {0};
  uint32_t start_tick;
  HAL_StatusTypeDef status;

  if ((data == NULL) || (length == 0U))
  {
    return HAL_ERROR;
  }

  command.OperationType = HAL_OSPI_OPTYPE_COMMON_CFG;
  command.FlashId = HAL_OSPI_FLASH_ID_1;
  command.Instruction = instruction;
  command.InstructionMode = HAL_OSPI_INSTRUCTION_1_LINE;
  command.InstructionSize = HAL_OSPI_INSTRUCTION_8_BITS;
  command.InstructionDtrMode = HAL_OSPI_INSTRUCTION_DTR_DISABLE;
  command.Address = address;
  command.AddressMode = HAL_OSPI_ADDRESS_1_LINE;
  command.AddressSize = HAL_OSPI_ADDRESS_24_BITS;
  command.AddressDtrMode = HAL_OSPI_ADDRESS_DTR_DISABLE;
  command.AlternateBytesMode = HAL_OSPI_ALTERNATE_BYTES_NONE;
  command.AlternateBytesDtrMode = HAL_OSPI_ALTERNATE_BYTES_DTR_DISABLE;
  command.DataMode = HAL_OSPI_DATA_1_LINE;
  command.NbData = length;
  command.DataDtrMode = HAL_OSPI_DATA_DTR_DISABLE;
  command.DummyCycles = 0U;
  command.DQSMode = HAL_OSPI_DQS_DISABLE;
  command.SIOOMode = HAL_OSPI_SIOO_INST_EVERY_CMD;

  status = HAL_OSPI_Command(&hospi1, &command, PS_OSPI_TIMEOUT_MS);
  if (status != HAL_OK)
  {
    return status;
  }

  __HAL_LINKDMA(&hospi1, hdma, handle_GPDMA1_Channel5);
  g_ps_ospi_rx_dma_done = 0U;
  g_ps_ospi_dma_error = 0U;

  status = HAL_OSPI_Receive_DMA(&hospi1, data);
  if (status != HAL_OK)
  {
    return status;
  }

  start_tick = HAL_GetTick();
  while ((g_ps_ospi_rx_dma_done == 0U) && (g_ps_ospi_dma_error == 0U))
  {
    if ((HAL_GetTick() - start_tick) > PS_OSPI_DMA_TIMEOUT_MS)
    {
      (void)HAL_OSPI_Abort(&hospi1);
      return HAL_TIMEOUT;
    }
  }

  if (g_ps_ospi_dma_error != 0U)
  {
    return HAL_ERROR;
  }

  return HAL_OK;
}

static HAL_StatusTypeDef PS_Ospi_WriteAddress_DMA(uint8_t instruction,
                                                  uint32_t address,
                                                  uint8_t *data,
                                                  uint32_t length)
{
  OSPI_RegularCmdTypeDef command = {0};
  uint32_t start_tick;
  HAL_StatusTypeDef status;

  if ((data == NULL) || (length == 0U))
  {
    return HAL_ERROR;
  }

  command.OperationType = HAL_OSPI_OPTYPE_COMMON_CFG;
  command.FlashId = HAL_OSPI_FLASH_ID_1;
  command.Instruction = instruction;
  command.InstructionMode = HAL_OSPI_INSTRUCTION_1_LINE;
  command.InstructionSize = HAL_OSPI_INSTRUCTION_8_BITS;
  command.InstructionDtrMode = HAL_OSPI_INSTRUCTION_DTR_DISABLE;
  command.Address = address;
  command.AddressMode = HAL_OSPI_ADDRESS_1_LINE;
  command.AddressSize = HAL_OSPI_ADDRESS_24_BITS;
  command.AddressDtrMode = HAL_OSPI_ADDRESS_DTR_DISABLE;
  command.AlternateBytesMode = HAL_OSPI_ALTERNATE_BYTES_NONE;
  command.AlternateBytesDtrMode = HAL_OSPI_ALTERNATE_BYTES_DTR_DISABLE;
  command.DataMode = HAL_OSPI_DATA_1_LINE;
  command.NbData = length;
  command.DataDtrMode = HAL_OSPI_DATA_DTR_DISABLE;
  command.DummyCycles = 0U;
  command.DQSMode = HAL_OSPI_DQS_DISABLE;
  command.SIOOMode = HAL_OSPI_SIOO_INST_EVERY_CMD;

  status = HAL_OSPI_Command(&hospi1, &command, PS_OSPI_TIMEOUT_MS);
  if (status != HAL_OK)
  {
    return status;
  }

  __HAL_LINKDMA(&hospi1, hdma, handle_GPDMA1_Channel4);
  g_ps_ospi_tx_dma_done = 0U;
  g_ps_ospi_dma_error = 0U;

  status = HAL_OSPI_Transmit_DMA(&hospi1, data);
  if (status != HAL_OK)
  {
    return status;
  }

  start_tick = HAL_GetTick();
  while ((g_ps_ospi_tx_dma_done == 0U) && (g_ps_ospi_dma_error == 0U))
  {
    if ((HAL_GetTick() - start_tick) > PS_OSPI_DMA_TIMEOUT_MS)
    {
      (void)HAL_OSPI_Abort(&hospi1);
      return HAL_TIMEOUT;
    }
  }

  if (g_ps_ospi_dma_error != 0U)
  {
    return HAL_ERROR;
  }

  return HAL_OK;
}

static HAL_StatusTypeDef PS_AT25_WaitWhileBusy(uint32_t timeout_ms)
{
  uint32_t start_tick = HAL_GetTick();
  uint8_t status1 = 0U;
  HAL_StatusTypeDef status;

  do
  {
    status = PS_Ospi_Read(PS_AT25_CMD_READ_STATUS1, &status1, 1U);
    if (status != HAL_OK)
    {
      return status;
    }

    if ((status1 & PS_AT25_STATUS1_WIP) == 0U)
    {
      return HAL_OK;
    }
  } while ((HAL_GetTick() - start_tick) <= timeout_ms);

  return HAL_TIMEOUT;
}

static void PS_Phase2_RunStorageProbe(void)
{
  volatile PsPhase2StorageProbe *probe = &g_ps_phase2_storage_probe;
  uint8_t jedec_id[3] = {0U, 0U, 0U};
  uint8_t status1 = 0U;
  uint8_t status2 = 0U;
  uint8_t status3 = 0U;
  uint8_t wake_jedec_id[3] = {0U, 0U, 0U};
  uint8_t write_buffer[PS_AT25_TEST_LENGTH];
  uint8_t read_buffer[PS_AT25_TEST_LENGTH];
  HAL_StatusTypeDef status;

  probe->magic = PS_PHASE2_STORAGE_MAGIC;
  probe->phase = PS_PHASE2_STORAGE_PHASE;
  probe->tick_start = HAL_GetTick();
  probe->ospi_kernel_hz = HAL_RCCEx_GetPeriphCLKFreq(RCC_PERIPHCLK_OSPI);
  probe->ospi_state = (uint32_t)HAL_OSPI_GetState(&hospi1);
  probe->ospi_sr = OCTOSPI1->SR;
  probe->scratch_address = PS_AT25_SCRATCH_ADDRESS;
  probe->test_length = PS_AT25_TEST_LENGTH;

  status = PS_Ospi_CommandOnly(PS_AT25_CMD_RELEASE_DPD);
  probe->release_status = (uint8_t)status;
  HAL_Delay(1U);

  status = PS_Ospi_Read(PS_AT25_CMD_READ_JEDEC_ID, jedec_id, sizeof(jedec_id));
  probe->jedec_status = (uint8_t)status;
  probe->jedec_id[0] = jedec_id[0];
  probe->jedec_id[1] = jedec_id[1];
  probe->jedec_id[2] = jedec_id[2];

  status = PS_Ospi_Read(PS_AT25_CMD_READ_STATUS1, &status1, 1U);
  probe->status1_status = (uint8_t)status;
  probe->status1 = status1;

  status = PS_Ospi_Read(PS_AT25_CMD_READ_STATUS2, &status2, 1U);
  probe->status2_status = (uint8_t)status;
  probe->status2 = status2;

  status = PS_Ospi_Read(PS_AT25_CMD_READ_STATUS3, &status3, 1U);
  probe->status3_status = (uint8_t)status;
  probe->status3 = status3;

  probe->write_enable_erase_status = (uint8_t)PS_Ospi_CommandOnly(PS_AT25_CMD_WRITE_ENABLE);
  probe->erase_status = (uint8_t)PS_Ospi_CommandAddress(PS_AT25_CMD_SECTOR_ERASE_4K,
                                                        PS_AT25_SCRATCH_ADDRESS);
  probe->erase_wait_status = (uint8_t)PS_AT25_WaitWhileBusy(PS_AT25_BUSY_TIMEOUT_MS);

  probe->blank_read_status = (uint8_t)PS_Ospi_ReadAddress(PS_AT25_CMD_READ_DATA,
                                                          PS_AT25_SCRATCH_ADDRESS,
                                                          read_buffer,
                                                          sizeof(read_buffer));
  for (uint32_t i = 0U; i < sizeof(read_buffer); i++)
  {
    if (read_buffer[i] != 0xFFU)
    {
      probe->erase_blank_mismatch_count++;
    }
  }

  for (uint32_t i = 0U; i < sizeof(write_buffer); i++)
  {
    write_buffer[i] = (uint8_t)(0xA5U ^ (uint8_t)i);
    read_buffer[i] = 0U;
  }

  probe->write_enable_program_status = (uint8_t)PS_Ospi_CommandOnly(PS_AT25_CMD_WRITE_ENABLE);
  probe->dma_program_status = (uint8_t)PS_Ospi_WriteAddress_DMA(PS_AT25_CMD_PAGE_PROGRAM,
                                                                PS_AT25_SCRATCH_ADDRESS,
                                                                write_buffer,
                                                                sizeof(write_buffer));
  probe->dma_program_wait_status = (uint8_t)((g_ps_ospi_tx_dma_done != 0U) ? HAL_OK : HAL_TIMEOUT);
  probe->program_wait_status = (uint8_t)PS_AT25_WaitWhileBusy(PS_AT25_BUSY_TIMEOUT_MS);

  for (uint32_t i = 0U; i < sizeof(read_buffer); i++)
  {
    read_buffer[i] = 0U;
  }
  probe->dma_read_status = (uint8_t)PS_Ospi_ReadAddress_DMA(PS_AT25_CMD_READ_DATA,
                                                            PS_AT25_SCRATCH_ADDRESS,
                                                            read_buffer,
                                                            sizeof(read_buffer));
  probe->dma_wait_status = (uint8_t)((g_ps_ospi_rx_dma_done != 0U) ? HAL_OK : HAL_TIMEOUT);
  for (uint32_t i = 0U; i < sizeof(read_buffer); i++)
  {
    if (read_buffer[i] != write_buffer[i])
    {
      probe->dma_verify_mismatch_count++;
    }
  }
  for (uint32_t i = 0U; i < sizeof(probe->dma_verify_sample); i++)
  {
    probe->dma_verify_sample[i] = read_buffer[i];
  }

  for (uint32_t i = 0U; i < sizeof(read_buffer); i++)
  {
    read_buffer[i] = 0U;
  }
  probe->verify_read_status = (uint8_t)PS_Ospi_ReadAddress(PS_AT25_CMD_READ_DATA,
                                                           PS_AT25_SCRATCH_ADDRESS,
                                                           read_buffer,
                                                           sizeof(read_buffer));
  for (uint32_t i = 0U; i < sizeof(read_buffer); i++)
  {
    if (read_buffer[i] != write_buffer[i])
    {
      probe->verify_mismatch_count++;
    }
  }
  for (uint32_t i = 0U; i < sizeof(probe->verify_sample); i++)
  {
    probe->verify_sample[i] = read_buffer[i];
  }

  probe->write_enable_cleanup_status = (uint8_t)PS_Ospi_CommandOnly(PS_AT25_CMD_WRITE_ENABLE);
  probe->cleanup_erase_status = (uint8_t)PS_Ospi_CommandAddress(PS_AT25_CMD_SECTOR_ERASE_4K,
                                                                PS_AT25_SCRATCH_ADDRESS);
  probe->cleanup_wait_status = (uint8_t)PS_AT25_WaitWhileBusy(PS_AT25_BUSY_TIMEOUT_MS);
  probe->cleanup_read_status = (uint8_t)PS_Ospi_ReadAddress(PS_AT25_CMD_READ_DATA,
                                                            PS_AT25_SCRATCH_ADDRESS,
                                                            read_buffer,
                                                            sizeof(read_buffer));
  for (uint32_t i = 0U; i < sizeof(read_buffer); i++)
  {
    if (read_buffer[i] != 0xFFU)
    {
      probe->cleanup_blank_mismatch_count++;
    }
  }

  probe->deep_power_down_status = (uint8_t)PS_Ospi_CommandOnly(PS_AT25_CMD_DEEP_POWER_DOWN);
  HAL_Delay(1U);
  probe->wake_status = (uint8_t)PS_Ospi_CommandOnly(PS_AT25_CMD_RELEASE_DPD);
  HAL_Delay(1U);

  probe->wake_jedec_status = (uint8_t)PS_Ospi_Read(PS_AT25_CMD_READ_JEDEC_ID,
                                                   wake_jedec_id,
                                                   sizeof(wake_jedec_id));
  probe->wake_jedec_id[0] = wake_jedec_id[0];
  probe->wake_jedec_id[1] = wake_jedec_id[1];
  probe->wake_jedec_id[2] = wake_jedec_id[2];
  probe->wake_read_status = (uint8_t)PS_Ospi_ReadAddress(PS_AT25_CMD_READ_DATA,
                                                         PS_AT25_SCRATCH_ADDRESS,
                                                         read_buffer,
                                                         sizeof(read_buffer));
  for (uint32_t i = 0U; i < sizeof(read_buffer); i++)
  {
    if (read_buffer[i] != 0xFFU)
    {
      probe->wake_blank_mismatch_count++;
    }
  }

  probe->ospi_state = (uint32_t)HAL_OSPI_GetState(&hospi1);
  probe->ospi_error = HAL_OSPI_GetError(&hospi1);
  probe->ospi_sr = OCTOSPI1->SR;
  probe->tick_end = HAL_GetTick();
  probe->complete = 1U;
}

static void PS_Phase4_InitButtonProbe(void)
{
  volatile PsPhase4ButtonProbe *probe = &g_ps_phase4_button_probe;

  probe->magic = PS_PHASE4_BUTTON_MAGIC;
  probe->phase = PS_PHASE4_BUTTON_PHASE;
  probe->tick_start = HAL_GetTick();
  probe->idle_mask = PS_Phase4_ReadButtonMask();
  probe->current_mask = probe->idle_mask;
  probe->complete = 1U;
}

static uint32_t PS_Phase4_ReadButtonMask(void)
{
  uint32_t mask = 0U;

  if (HAL_GPIO_ReadPin(BTN_START_GPIO_Port, BTN_START_Pin) == GPIO_PIN_RESET)
  {
    mask |= PS_BUTTON_MASK_START;
  }
  if (HAL_GPIO_ReadPin(BTN_A_GPIO_Port, BTN_A_Pin) == GPIO_PIN_SET)
  {
    mask |= PS_BUTTON_MASK_A;
  }
  if (HAL_GPIO_ReadPin(BTN_B_GPIO_Port, BTN_B_Pin) == GPIO_PIN_SET)
  {
    mask |= PS_BUTTON_MASK_B;
  }
  if (HAL_GPIO_ReadPin(BTN_L_GPIO_Port, BTN_L_Pin) == GPIO_PIN_SET)
  {
    mask |= PS_BUTTON_MASK_L;
  }
  if (HAL_GPIO_ReadPin(BTN_R_GPIO_Port, BTN_R_Pin) == GPIO_PIN_SET)
  {
    mask |= PS_BUTTON_MASK_R;
  }

  return mask;
}

static void PS_Phase5_InitLightProbe(void)
{
  volatile PsPhase5LightProbe *probe = &g_ps_phase5_light_probe;
  uint16_t sample = 0U;

  probe->magic = PS_PHASE5_LIGHT_MAGIC;
  probe->phase = PS_PHASE5_LIGHT_PHASE;
  probe->tick_start = HAL_GetTick();
  probe->min_sample = 0xFFFFU;
  probe->calibration_status = (uint8_t)HAL_ADCEx_Calibration_Start(&hadc1,
                                                                   ADC_CALIB_OFFSET,
                                                                   ADC_SINGLE_ENDED);

  HAL_GPIO_WritePin(PHOT_EN_GPIO_Port, PHOT_EN_Pin, GPIO_PIN_RESET);
  HAL_Delay(2U);
  if (PS_Phase5_ReadLightSample(&sample) == HAL_OK)
  {
    probe->disabled_sample = sample;
  }
  else
  {
    probe->adc_error_count++;
  }

  probe->phot_en_state_after = (uint8_t)HAL_GPIO_ReadPin(PHOT_EN_GPIO_Port, PHOT_EN_Pin);
  probe->complete = 1U;
}

static void PS_Phase5_LightProbeStep(void)
{
  volatile PsPhase5LightProbe *probe = &g_ps_phase5_light_probe;
  uint32_t now = HAL_GetTick();
  uint16_t sample = 0U;

  if ((now - probe->tick_last_sample) < PS_LIGHT_SAMPLE_PERIOD_MS)
  {
    return;
  }

  HAL_GPIO_WritePin(PHOT_EN_GPIO_Port, PHOT_EN_Pin, GPIO_PIN_SET);
  HAL_Delay(PS_LIGHT_SETTLE_MS);

  if (PS_Phase5_ReadLightSample(&sample) == HAL_OK)
  {
    probe->last_sample = sample;
    probe->raw_sum += sample;
    probe->sample_count++;
    if (sample < probe->min_sample)
    {
      probe->min_sample = sample;
    }
    if (sample > probe->max_sample)
    {
      probe->max_sample = sample;
    }
  }
  else
  {
    probe->adc_error_count++;
  }

  HAL_GPIO_WritePin(PHOT_EN_GPIO_Port, PHOT_EN_Pin, GPIO_PIN_RESET);
  probe->phot_en_state_after = (uint8_t)HAL_GPIO_ReadPin(PHOT_EN_GPIO_Port, PHOT_EN_Pin);
  probe->adc_state_after = (uint32_t)HAL_ADC_GetState(&hadc1);
  probe->adc_error_after = HAL_ADC_GetError(&hadc1);
  probe->tick_last_sample = now;
}

static HAL_StatusTypeDef PS_Phase5_ReadLightSample(uint16_t *sample)
{
  volatile PsPhase5LightProbe *probe = &g_ps_phase5_light_probe;
  HAL_StatusTypeDef status;

  status = HAL_ADC_Start(&hadc1);
  probe->last_start_status = (uint8_t)status;
  if (status != HAL_OK)
  {
    return status;
  }

  status = HAL_ADC_PollForConversion(&hadc1, PS_LIGHT_ADC_TIMEOUT_MS);
  probe->last_poll_status = (uint8_t)status;
  if (status == HAL_OK)
  {
    *sample = (uint16_t)HAL_ADC_GetValue(&hadc1);
  }

  probe->last_stop_status = (uint8_t)HAL_ADC_Stop(&hadc1);
  return status;
}

static void PS_Phase4_RunImuIdentityProbe(void)
{
  volatile PsPhase4ImuProbe *probe = &g_ps_phase4_imu_probe;
  stmdev_ctx_t ctx = {0};
  uint8_t value = 0U;

  probe->magic = PS_PHASE4_IMU_MAGIC;
  probe->phase = PS_PHASE4_IMU_PHASE;
  probe->tick_start = HAL_GetTick();
  probe->expected_public_addr_7bit = PS_LIS2DUX12_PUBLIC_ADDR_7BIT;
  probe->expected_hal_addr_shifted = PS_LIS2DUX12_HAL_ADDR_SHIFTED;
  probe->alt_public_addr_7bit = PS_LIS2DUX12_ALT_ADDR_7BIT;
  probe->alt_hal_addr_shifted = PS_LIS2DUX12_ALT_HAL_SHIFTED;

  probe->ready_expected_status = (uint8_t)HAL_I2C_IsDeviceReady(&hi2c3,
                                                                PS_LIS2DUX12_HAL_ADDR_SHIFTED,
                                                                2U,
                                                                PS_LIS2DUX12_I2C_TIMEOUT_MS);
  probe->ready_expected_error = HAL_I2C_GetError(&hi2c3);
  if (probe->ready_expected_status != (uint8_t)HAL_OK)
  {
    HAL_Delay(25U);
    probe->ready_expected_status = (uint8_t)HAL_I2C_IsDeviceReady(&hi2c3,
                                                                  PS_LIS2DUX12_HAL_ADDR_SHIFTED,
                                                                  2U,
                                                                  PS_LIS2DUX12_I2C_TIMEOUT_MS);
    probe->ready_expected_error = HAL_I2C_GetError(&hi2c3);
  }

  probe->ready_alt_status = (uint8_t)HAL_I2C_IsDeviceReady(&hi2c3,
                                                           PS_LIS2DUX12_ALT_HAL_SHIFTED,
                                                           1U,
                                                           PS_LIS2DUX12_I2C_TIMEOUT_MS);
  probe->ready_alt_error = HAL_I2C_GetError(&hi2c3);

  ctx.read_reg = PS_LIS2DUX12_ReadReg;
  ctx.write_reg = PS_LIS2DUX12_WriteReg;
  ctx.handle = (void *)(uintptr_t)PS_LIS2DUX12_HAL_ADDR_SHIFTED;

  probe->driver_whoami_status = (uint8_t)lis2dux12_device_id_get(&ctx, &value);
  probe->whoami = value;
  value = 0U;
  probe->raw_whoami_status = (uint8_t)PS_LIS2DUX12_ReadRaw(LIS2DUX12_WHO_AM_I, &value);
  probe->raw_whoami = value;
  value = 0U;
  probe->raw_status_status = (uint8_t)PS_LIS2DUX12_ReadRaw(LIS2DUX12_STATUS, &value);
  probe->raw_status = value;
  value = 0U;
  probe->raw_ctrl1_status = (uint8_t)PS_LIS2DUX12_ReadRaw(LIS2DUX12_CTRL1, &value);
  probe->raw_ctrl1 = value;
  value = 0U;
  probe->raw_ctrl4_status = (uint8_t)PS_LIS2DUX12_ReadRaw(LIS2DUX12_CTRL4, &value);
  probe->raw_ctrl4 = value;
  value = 0U;
  probe->raw_md1_status = (uint8_t)PS_LIS2DUX12_ReadRaw(LIS2DUX12_MD1_CFG, &value);
  probe->raw_md1_cfg = value;
  probe->i2c_error_after = HAL_I2C_GetError(&hi2c3);
  probe->tick_end = HAL_GetTick();
  probe->complete = 1U;
}

static int32_t PS_LIS2DUX12_ReadReg(void *handle, uint8_t reg, uint8_t *data, uint16_t len)
{
  const uint16_t address = (uint16_t)(uintptr_t)handle;

  if (HAL_I2C_Mem_Read(&hi2c3,
                       address,
                       reg,
                       I2C_MEMADD_SIZE_8BIT,
                       data,
                       len,
                       PS_LIS2DUX12_I2C_TIMEOUT_MS) == HAL_OK)
  {
    return 0;
  }

  return -1;
}

static int32_t PS_LIS2DUX12_WriteReg(void *handle, uint8_t reg, const uint8_t *data, uint16_t len)
{
  (void)handle;
  (void)reg;
  (void)data;
  (void)len;

  return -1;
}

static HAL_StatusTypeDef PS_LIS2DUX12_ReadRaw(uint8_t reg, uint8_t *data)
{
  return HAL_I2C_Mem_Read(&hi2c3,
                          PS_LIS2DUX12_HAL_ADDR_SHIFTED,
                          reg,
                          I2C_MEMADD_SIZE_8BIT,
                          data,
                          1U,
                          PS_LIS2DUX12_I2C_TIMEOUT_MS);
}

static void PS_Phase4_RunTmagIdentityProbe(void)
{
  volatile PsPhase4TmagProbe *probe = &g_ps_phase4_tmag_probe;
  uint8_t result_window[PS_TMAG3001_RESULT_WINDOW_LEN] = {0};
  uint8_t value = 0U;

  probe->magic = PS_PHASE4_TMAG_MAGIC;
  probe->phase = PS_PHASE4_TMAG_PHASE;
  probe->tick_start = HAL_GetTick();

  for (uint32_t addr = PS_I2C_SCAN_FIRST_ADDR_7BIT; addr <= PS_I2C_SCAN_LAST_ADDR_7BIT; addr++)
  {
    if (HAL_I2C_IsDeviceReady(&hi2c3,
                              (uint16_t)(addr << 1),
                              1U,
                              PS_TMAG3001_I2C_TIMEOUT_MS) == HAL_OK)
    {
      probe->full_scan_hit_bitmap[addr >> 3] |= (uint8_t)(1U << (addr & 0x07U));
      probe->full_scan_count++;
    }
  }

  for (uint32_t i = 0U; i < PS_TMAG3001_SCAN_COUNT; i++)
  {
    const uint8_t public_addr = (uint8_t)(PS_TMAG3001_FIRST_ADDR_7BIT + i);
    const uint8_t hal_addr = (uint8_t)(public_addr << 1);

    probe->scanned_public_addr_7bit[i] = public_addr;
    probe->scanned_hal_addr_shifted[i] = hal_addr;
    probe->ready_status[i] = (uint8_t)HAL_I2C_IsDeviceReady(&hi2c3,
                                                            hal_addr,
                                                            1U,
                                                            PS_TMAG3001_I2C_TIMEOUT_MS);
    probe->ready_error[i] = HAL_I2C_GetError(&hi2c3);

    if ((probe->ready_status[i] == (uint8_t)HAL_OK) && (probe->detected_public_addr_7bit == 0U))
    {
      probe->detected_public_addr_7bit = public_addr;
      probe->detected_hal_addr_shifted = hal_addr;
    }
  }

  if (probe->detected_public_addr_7bit != 0U)
  {
    value = 0U;
    probe->read_device_id_status = (uint8_t)PS_TMAG3001_ReadRaw(probe->detected_hal_addr_shifted,
                                                                PS_TMAG3001_REG_DEVICE_ID,
                                                                &value);
    probe->device_id = value;
    value = 0U;
    probe->read_manufacturer_lsb_status = (uint8_t)PS_TMAG3001_ReadRaw(probe->detected_hal_addr_shifted,
                                                                       PS_TMAG3001_REG_MFR_ID_LSB,
                                                                       &value);
    probe->manufacturer_lsb = value;
    value = 0U;
    probe->read_manufacturer_msb_status = (uint8_t)PS_TMAG3001_ReadRaw(probe->detected_hal_addr_shifted,
                                                                       PS_TMAG3001_REG_MFR_ID_MSB,
                                                                       &value);
    probe->manufacturer_msb = value;
    value = 0U;
    probe->read_conv_status_status = (uint8_t)PS_TMAG3001_ReadRaw(probe->detected_hal_addr_shifted,
                                                                  PS_TMAG3001_REG_CONV_STATUS,
                                                                  &value);
    probe->conv_status = value;
    value = 0U;
    probe->read_device_status_status = (uint8_t)PS_TMAG3001_ReadRaw(probe->detected_hal_addr_shifted,
                                                                    PS_TMAG3001_REG_DEVICE_STATUS,
                                                                    &value);
    probe->device_status = value;
    probe->read_result_window_status = (uint8_t)HAL_I2C_Mem_Read(&hi2c3,
                                                                 probe->detected_hal_addr_shifted,
                                                                 PS_TMAG3001_REG_X_RESULT_MSB,
                                                                 I2C_MEMADD_SIZE_8BIT,
                                                                 result_window,
                                                                 PS_TMAG3001_RESULT_WINDOW_LEN,
                                                                 PS_TMAG3001_I2C_TIMEOUT_MS);
    if (probe->read_result_window_status == (uint8_t)HAL_OK)
    {
      for (uint32_t i = 0U; i < PS_TMAG3001_RESULT_WINDOW_LEN; i++)
      {
        probe->result_window[i] = result_window[i];
      }
      probe->x_raw = (int16_t)(((uint16_t)probe->result_window[0] << 8) |
                               (uint16_t)probe->result_window[1]);
      probe->y_raw = (int16_t)(((uint16_t)probe->result_window[2] << 8) |
                               (uint16_t)probe->result_window[3]);
      probe->z_raw = (int16_t)(((uint16_t)probe->result_window[4] << 8) |
                               (uint16_t)probe->result_window[5]);
      probe->result_window_conv_status = probe->result_window[6];
    }
  }

  probe->i2c_error_after = HAL_I2C_GetError(&hi2c3);
  probe->tick_end = HAL_GetTick();
  probe->complete = 1U;
}

static HAL_StatusTypeDef PS_TMAG3001_ReadRaw(uint8_t address, uint8_t reg, uint8_t *data)
{
  return HAL_I2C_Mem_Read(&hi2c3,
                          address,
                          reg,
                          I2C_MEMADD_SIZE_8BIT,
                          data,
                          1U,
                          PS_TMAG3001_I2C_TIMEOUT_MS);
}

static void PS_Phase4_RunTmagCaptureProbe(void)
{
  volatile PsPhase4TmagCaptureProbe *probe = &g_ps_phase4_tmag_capture_probe;
  uint8_t result_window[PS_TMAG3001_RESULT_WINDOW_LEN] = {0};
  uint8_t readback = 0U;
  uint8_t address = g_ps_phase4_tmag_probe.detected_hal_addr_shifted;

  probe->magic = PS_PHASE4_TMAG_CAPTURE_MAGIC;
  probe->phase = PS_PHASE4_TMAG_CAPTURE_PHASE;
  probe->tick_start = HAL_GetTick();
  probe->sample_period_ms = PS_TMAG3001_CAPTURE_PERIOD_MS;
  probe->samples_requested = PS_TMAG3001_CAPTURE_SAMPLES;
  probe->address_7bit = g_ps_phase4_tmag_probe.detected_public_addr_7bit;
  probe->address_shifted = address;
  probe->device_config_1_value = PS_TMAG3001_ACTIVE_DEVICE_CONFIG_1;
  probe->sensor_config_1_value = PS_TMAG3001_ACTIVE_SENSOR_CONFIG_1;
  probe->sensor_config_2_value = PS_TMAG3001_ACTIVE_SENSOR_CONFIG_2;
  probe->device_config_2_value = PS_TMAG3001_ACTIVE_DEVICE_CONFIG_2;

  if (address == 0U)
  {
    probe->i2c_error_after = HAL_I2C_GetError(&hi2c3);
    probe->tick_capture_end = HAL_GetTick();
    probe->complete = 1U;
    return;
  }

  TMAG3001_HAL_SetI2CAddress(probe->address_7bit);

  TMAG3001setSampleRate(0U);
  probe->write_device_config_1_status = (uint8_t)TMAG3001_HAL_GetLastStatus();
  TMAG3001enableMagChannels(MAG_CH_EN_XYZ);
  probe->write_sensor_config_1_status = (uint8_t)TMAG3001_HAL_GetLastStatus();
  TMAG3001setRanges(0U, 0U);
  probe->write_sensor_config_2_status = (uint8_t)TMAG3001_HAL_GetLastStatus();
  TMAG3001enterContinuousMeasureMode();
  probe->write_device_config_2_status = (uint8_t)TMAG3001_HAL_GetLastStatus();
  HAL_Delay(PS_TMAG3001_ACTIVE_SETTLE_MS);

  readback = 0U;
  probe->read_device_config_1_status =
    (uint8_t)PS_TMAG3001_ReadRaw(address, PS_TMAG3001_REG_DEVICE_CONFIG_1, &readback);
  probe->device_config_1_value = readback;
  readback = 0U;
  probe->read_sensor_config_1_status =
    (uint8_t)PS_TMAG3001_ReadRaw(address, PS_TMAG3001_REG_SENSOR_CONFIG_1, &readback);
  probe->sensor_config_1_value = readback;
  readback = 0U;
  probe->read_sensor_config_2_status =
    (uint8_t)PS_TMAG3001_ReadRaw(address, PS_TMAG3001_REG_SENSOR_CONFIG_2, &readback);
  probe->sensor_config_2_value = readback;
  readback = 0U;
  probe->read_device_config_2_status =
    (uint8_t)PS_TMAG3001_ReadRaw(address, PS_TMAG3001_REG_DEVICE_CONFIG_2, &readback);
  probe->device_config_2_value = readback;

  HAL_Delay(PS_TMAG3001_CAPTURE_PREP_MS);
  probe->start_cue_status = (uint8_t)PS_Phase4_TmagCaptureCue(1U);
  probe->tick_capture_start = HAL_GetTick();

  for (uint32_t i = 0U; i < PS_TMAG3001_CAPTURE_SAMPLES; i++)
  {
    volatile PsPhase4TmagSample *sample = &probe->samples[i];
    HAL_StatusTypeDef status;

    for (uint32_t j = 0U; j < PS_TMAG3001_RESULT_WINDOW_LEN; j++)
    {
      result_window[j] = 0U;
    }

    sample->tick = HAL_GetTick();
    status = PS_TMAG3001_ReadResultWindow(address, result_window);
    sample->read_status = (uint8_t)status;

    if (status == HAL_OK)
    {
      sample->x_raw = (int16_t)(((uint16_t)result_window[0] << 8) |
                                (uint16_t)result_window[1]);
      sample->y_raw = (int16_t)(((uint16_t)result_window[2] << 8) |
                                (uint16_t)result_window[3]);
      sample->z_raw = (int16_t)(((uint16_t)result_window[4] << 8) |
                                (uint16_t)result_window[5]);
      sample->conv_status = result_window[6];
      probe->samples_completed = i + 1U;
    }
    else
    {
      probe->read_error_count++;
    }

    HAL_Delay(PS_TMAG3001_CAPTURE_PERIOD_MS);
  }

  probe->tick_capture_end = HAL_GetTick();
  probe->end_cue_status[0] = (uint8_t)PS_Phase4_TmagCaptureCue(1U);
  probe->end_cue_status[1] = (uint8_t)PS_Phase4_TmagCaptureCue(1U);
  probe->i2c_error_after = HAL_I2C_GetError(&hi2c3);
  probe->complete = 1U;
}

static HAL_StatusTypeDef PS_TMAG3001_ReadResultWindow(uint8_t address, uint8_t *data)
{
  return HAL_I2C_Mem_Read(&hi2c3,
                          address,
                          PS_TMAG3001_REG_X_RESULT_MSB,
                          I2C_MEMADD_SIZE_8BIT,
                          data,
                          PS_TMAG3001_RESULT_WINDOW_LEN,
                          PS_TMAG3001_I2C_TIMEOUT_MS);
}

static HAL_StatusTypeDef PS_Phase4_TmagCaptureCue(uint32_t count)
{
  HAL_StatusTypeDef status = HAL_OK;
  const uint32_t period = (PS_BBB_LPTIM_KERNEL_HZ / PS_TMAG3001_CAPTURE_CUE_HZ) - 1UL;
  const uint32_t pulse = period / 2UL;

  for (uint32_t i = 0U; i < count; i++)
  {
    __HAL_LPTIM_AUTORELOAD_SET(&hlptim1, period);
    __HAL_LPTIM_COMPARE_SET(&hlptim1, LPTIM_CHANNEL_1, pulse);

    status = HAL_LPTIM_PWM_Start(&hlptim1, LPTIM_CHANNEL_1);
    if (status != HAL_OK)
    {
      return status;
    }

    HAL_Delay(PS_TMAG3001_CAPTURE_CUE_MS);

    status = HAL_LPTIM_PWM_Stop(&hlptim1, LPTIM_CHANNEL_1);
    if (status != HAL_OK)
    {
      return status;
    }

    HAL_Delay(PS_TMAG3001_CAPTURE_CUE_MS);
  }

  return status;
}

static void PS_Phase7_RunNinaProbe(void)
{
  volatile PsPhase7NinaProbe *probe = &g_ps_phase7_nina_probe;
  static const uint8_t command_0[] = "AT\r\n";
  static const uint8_t command_1[] = "ATI0\r\n";
  static const uint8_t command_2[] = "ATI9\r\n";
  static const uint8_t command_3[] = "ATI10\r\n";
  static const uint8_t command_4[] = "AT+CGMI\r\n";
  static const uint8_t command_5[] = "AT+CGMM\r\n";
  static const uint8_t command_6[] = "AT+CGMR\r\n";
  static const uint8_t command_7[] = "AT+CGSN\r\n";
  static const uint8_t command_8[] = "AT+GMI\r\n";
  static const uint8_t command_9[] = "AT+GMM\r\n";
  static const uint8_t command_10[] = "AT+GMR\r\n";
  static const uint8_t command_11[] = "AT+GSN\r\n";
  static const uint8_t phone_query_0[] = "AT+UBTLN?\r\n";
  static const uint8_t phone_query_1[] = "AT+UBTLE?\r\n";
  static const uint8_t phone_query_2[] = "AT+UBTDM?\r\n";
  static const uint8_t phone_query_3[] = "AT+UBTCM?\r\n";
  static const uint8_t phone_query_4[] = "AT+UBTPM?\r\n";
  static const uint8_t phone_query_5[] = "AT+UBTSM?\r\n";
  static const uint8_t phone_query_6[] = "AT+UDSC=0\r\n";
  static const uint8_t phone_query_7[] = "AT+UDSF=0\r\n";
  static const uint8_t phone_query_8[] = "AT+UBTDM=3\r\n";
  static const uint8_t phone_query_9[] = "AT+UBTCM=2\r\n";
  static const uint8_t data_mode_cmd[] = "ATO1\r\n";
  static const uint8_t hello[] = "\r\npeepshow nina sps hello\r\ntype text and press send\r\n";

  probe->magic = PS_PHASE7_NINA_MAGIC;
  probe->phase = PS_PHASE7_NINA_PHASE;
  probe->tick_start = HAL_GetTick();
  probe->lpuart_kernel_hz = HAL_RCCEx_GetPeriphCLKFreq(RCC_PERIPHCLK_LPUART1);
  probe->gpioc_moder_before = GPIOC->MODER;
  probe->gpioc_pupdr_before = GPIOC->PUPDR;
  probe->gpioc_idr_before = GPIOC->IDR;
  probe->gpioc_odr_before = GPIOC->ODR;
  probe->nrst_before = (uint8_t)HAL_GPIO_ReadPin(NINA_NRST_GPIO_Port, NINA_NRST_Pin);
  probe->sw1_level_before = (uint8_t)HAL_GPIO_ReadPin(NINA_SW1_GPIO_Port, NINA_SW1_Pin);
  probe->sw2_level_before = (uint8_t)HAL_GPIO_ReadPin(NINA_SW2_GPIO_Port, NINA_SW2_Pin);
  probe->dtr_level_before = (uint8_t)HAL_GPIO_ReadPin(NINA_DTR_GPIO_Port, NINA_DTR_Pin);
  probe->dsr_level_before = (uint8_t)HAL_GPIO_ReadPin(NINA_DSR_GPIO_Port, NINA_DSR_Pin);

  HAL_GPIO_WritePin(NINA_NRST_GPIO_Port, NINA_NRST_Pin, GPIO_PIN_RESET);
  HAL_Delay(PS_NINA_RESET_ASSERT_MS);
  HAL_GPIO_WritePin(NINA_NRST_GPIO_Port, NINA_NRST_Pin, GPIO_PIN_SET);
  probe->tick_release = HAL_GetTick();
  probe->nrst_after_release = (uint8_t)HAL_GPIO_ReadPin(NINA_NRST_GPIO_Port, NINA_NRST_Pin);
  probe->gpioc_idr_after_release = GPIOC->IDR;
  probe->gpioc_odr_after_release = GPIOC->ODR;

  HAL_Delay(PS_NINA_BOOT_WAIT_MS);
  probe->nrst_after_boot_wait = (uint8_t)HAL_GPIO_ReadPin(NINA_NRST_GPIO_Port, NINA_NRST_Pin);
  probe->gpioc_idr_after_boot_wait = GPIOC->IDR;

  probe->boot_rx_len = PS_Phase7_NinaReceiveWindow((uint8_t *)probe->boot_rx,
                                                   sizeof(probe->boot_rx),
                                                   PS_NINA_RX_WINDOW_MS);

  PS_Phase7_NinaCommandProbe(0U, command_0, (uint16_t)(sizeof(command_0) - 1U));
  PS_Phase7_NinaCommandProbe(1U, command_1, (uint16_t)(sizeof(command_1) - 1U));
  PS_Phase7_NinaCommandProbe(2U, command_2, (uint16_t)(sizeof(command_2) - 1U));
  PS_Phase7_NinaCommandProbe(3U, command_3, (uint16_t)(sizeof(command_3) - 1U));
  PS_Phase7_NinaCommandProbe(4U, command_4, (uint16_t)(sizeof(command_4) - 1U));
  PS_Phase7_NinaCommandProbe(5U, command_5, (uint16_t)(sizeof(command_5) - 1U));
  PS_Phase7_NinaCommandProbe(6U, command_6, (uint16_t)(sizeof(command_6) - 1U));
  PS_Phase7_NinaCommandProbe(7U, command_7, (uint16_t)(sizeof(command_7) - 1U));
  PS_Phase7_NinaCommandProbe(8U, command_8, (uint16_t)(sizeof(command_8) - 1U));
  PS_Phase7_NinaCommandProbe(9U, command_9, (uint16_t)(sizeof(command_9) - 1U));
  PS_Phase7_NinaCommandProbe(10U, command_10, (uint16_t)(sizeof(command_10) - 1U));
  PS_Phase7_NinaCommandProbe(11U, command_11, (uint16_t)(sizeof(command_11) - 1U));
  PS_Phase7_NinaCommandProbe(0U, phone_query_0, (uint16_t)(sizeof(phone_query_0) - 1U));
  PS_Phase7_NinaCommandProbe(1U, phone_query_1, (uint16_t)(sizeof(phone_query_1) - 1U));
  PS_Phase7_NinaCommandProbe(2U, phone_query_2, (uint16_t)(sizeof(phone_query_2) - 1U));
  PS_Phase7_NinaCommandProbe(3U, phone_query_3, (uint16_t)(sizeof(phone_query_3) - 1U));
  PS_Phase7_NinaCommandProbe(4U, phone_query_4, (uint16_t)(sizeof(phone_query_4) - 1U));
  PS_Phase7_NinaCommandProbe(5U, phone_query_5, (uint16_t)(sizeof(phone_query_5) - 1U));
  PS_Phase7_NinaCommandProbe(6U, phone_query_6, (uint16_t)(sizeof(phone_query_6) - 1U));
  PS_Phase7_NinaCommandProbe(7U, phone_query_7, (uint16_t)(sizeof(phone_query_7) - 1U));
  PS_Phase7_NinaCommandProbe(8U, phone_query_8, (uint16_t)(sizeof(phone_query_8) - 1U));
  PS_Phase7_NinaCommandProbe(9U, phone_query_9, (uint16_t)(sizeof(phone_query_9) - 1U));
  probe->phone_detected = PS_Phase7_CommandBuffersContain("+UUBTACLC:");
  probe->phone_sps_detected = PS_Phase7_CommandBuffersContain("+UUDPC:");
  if (probe->phone_sps_detected == 0U)
  {
    probe->phone_rx_len = PS_Phase7_NinaReceiveUntilPhoneSps((uint8_t *)probe->phone_rx,
                                                             sizeof(probe->phone_rx),
                                                             PS_NINA_PHONE_CONNECT_WINDOW_MS);
    if (PS_Phase7_BufferContains((const uint8_t *)probe->phone_rx,
                                 probe->phone_rx_len,
                                 "+UUBTACLC:") != 0U)
    {
      probe->phone_detected = 1U;
    }
    if (PS_Phase7_BufferContains((const uint8_t *)probe->phone_rx,
                                 probe->phone_rx_len,
                                 "+UUDPC:") != 0U)
    {
      probe->phone_sps_detected = 1U;
    }
  }
  if (probe->phone_sps_detected != 0U)
  {
    probe->data_mode_tx_status = (uint8_t)HAL_UART_Transmit(&hlpuart1,
                                                            data_mode_cmd,
                                                            (uint16_t)(sizeof(data_mode_cmd) - 1U),
                                                            PS_NINA_TX_TIMEOUT_MS);
    (void)PS_Phase7_NinaReceiveWindow((uint8_t *)probe->phone_rx,
                                      sizeof(probe->phone_rx),
                                      1000U);
    probe->hello_tx_status = (uint8_t)HAL_UART_Transmit(&hlpuart1,
                                                        hello,
                                                        (uint16_t)(sizeof(hello) - 1U),
                                                        PS_NINA_TX_TIMEOUT_MS);
    probe->phone_data_rx_len = PS_Phase7_NinaReceiveEchoWindow((uint8_t *)probe->phone_data_rx,
                                                               sizeof(probe->phone_data_rx),
                                                               PS_NINA_PHONE_DATA_WINDOW_MS);
  }

  probe->active_baud_after = hlpuart1.Init.BaudRate;
  probe->uart_state_after = (uint32_t)HAL_UART_GetState(&hlpuart1);
  probe->uart_error_after = HAL_UART_GetError(&hlpuart1);
  HAL_GPIO_WritePin(NINA_NRST_GPIO_Port, NINA_NRST_Pin, GPIO_PIN_RESET);
  HAL_Delay(PS_NINA_RESET_ASSERT_MS);
  probe->nrst_after_final_reset = (uint8_t)HAL_GPIO_ReadPin(NINA_NRST_GPIO_Port, NINA_NRST_Pin);
  probe->tick_end = HAL_GetTick();
  probe->complete = 1U;
}

static void PS_Phase7_NinaCommandProbe(uint32_t index, const uint8_t *command, uint16_t length)
{
  volatile PsPhase7NinaProbe *probe = &g_ps_phase7_nina_probe;

  if (index >= PS_NINA_DISCOVERY_COMMAND_COUNT)
  {
    return;
  }

  (void)PS_Phase7_NinaReceiveWindow((uint8_t *)probe->command_rx[index],
                                    PS_NINA_DISCOVERY_RX_BYTES,
                                    50U);
  probe->command_tx_status[index] = (uint8_t)HAL_UART_Transmit(&hlpuart1,
                                                               command,
                                                               length,
                                                               PS_NINA_TX_TIMEOUT_MS);
  probe->command_rx_len[index] =
    PS_Phase7_NinaReceiveWindow((uint8_t *)probe->command_rx[index],
                                PS_NINA_DISCOVERY_RX_BYTES,
                                PS_NINA_RX_WINDOW_MS);
}

static uint16_t PS_Phase7_NinaReceiveWindow(uint8_t *buffer, uint16_t capacity, uint32_t window_ms)
{
  uint16_t count = 0U;
  uint32_t start = HAL_GetTick();

  while (((HAL_GetTick() - start) < window_ms) && (count < capacity))
  {
    uint8_t byte = 0U;
    if (HAL_UART_Receive(&hlpuart1, &byte, 1U, PS_NINA_UART_BYTE_TIMEOUT_MS) == HAL_OK)
    {
      buffer[count] = byte;
      count++;
    }
  }

  return count;
}

static uint16_t PS_Phase7_NinaReceiveUntilPhoneSps(uint8_t *buffer, uint16_t capacity, uint32_t window_ms)
{
  uint16_t count = 0U;
  uint32_t start = HAL_GetTick();
  volatile PsPhase7NinaProbe *probe = &g_ps_phase7_nina_probe;

  while (((HAL_GetTick() - start) < window_ms) && (count < capacity))
  {
    uint8_t byte = 0U;
    if (HAL_UART_Receive(&hlpuart1, &byte, 1U, PS_NINA_UART_BYTE_TIMEOUT_MS) == HAL_OK)
    {
      buffer[count] = byte;
      count++;
      probe->phone_rx_len = count;
      if (PS_Phase7_BufferContains(buffer, count, "+UUDPC:") != 0U)
      {
        probe->phone_detect_tick = HAL_GetTick();
        break;
      }
    }
  }

  return count;
}

static uint8_t PS_Phase7_CommandBuffersContain(const char *needle)
{
  volatile PsPhase7NinaProbe *probe = &g_ps_phase7_nina_probe;

  for (uint32_t i = 0U; i < PS_NINA_DISCOVERY_COMMAND_COUNT; i++)
  {
    if (PS_Phase7_BufferContains((const uint8_t *)probe->command_rx[i],
                                 probe->command_rx_len[i],
                                 needle) != 0U)
    {
      return 1U;
    }
  }

  return 0U;
}

static uint16_t PS_Phase7_NinaReceiveEchoWindow(uint8_t *buffer, uint16_t capacity, uint32_t window_ms)
{
  uint16_t count = 0U;
  uint32_t start = HAL_GetTick();
  volatile PsPhase7NinaProbe *probe = &g_ps_phase7_nina_probe;

  while (((HAL_GetTick() - start) < window_ms) && (count < capacity))
  {
    uint8_t byte = 0U;
    if (HAL_UART_Receive(&hlpuart1, &byte, 1U, PS_NINA_UART_BYTE_TIMEOUT_MS) == HAL_OK)
    {
      buffer[count] = byte;
      count++;
      probe->phone_data_rx_len = count;
      if (HAL_UART_Transmit(&hlpuart1, &byte, 1U, PS_NINA_TX_TIMEOUT_MS) == HAL_OK)
      {
        probe->phone_echo_tx_count++;
      }
    }
  }

  return count;
}

static uint8_t PS_Phase7_BufferContains(const uint8_t *buffer, uint16_t count, const char *needle)
{
  uint16_t needle_len = 0U;

  while (needle[needle_len] != '\0')
  {
    needle_len++;
  }

  if ((needle_len == 0U) || (count < needle_len))
  {
    return 0U;
  }

  for (uint16_t i = 0U; i <= (uint16_t)(count - needle_len); i++)
  {
    uint8_t match = 1U;
    for (uint16_t j = 0U; j < needle_len; j++)
    {
      if (buffer[i + j] != (uint8_t)needle[j])
      {
        match = 0U;
        break;
      }
    }
    if (match != 0U)
    {
      return 1U;
    }
  }

  return 0U;
}

static void PS_Phase3_RunAudioProbe(void)
{
  volatile PsPhase3AudioProbe *probe = &g_ps_phase3_audio_probe;
  static const uint32_t speaker_response_hz[PS_SPEAKER_SWEEP_STEPS] =
  {
    1000UL, 875UL, 750UL, 688UL,
    625UL, 562UL, 500UL, 469UL,
    438UL, 406UL, 375UL, 344UL,
    312UL, 281UL, 250UL, 219UL,
  };

  HAL_GPIO_WritePin(SD_MODE_GPIO_Port, SD_MODE_Pin, GPIO_PIN_RESET);
  (void)HAL_LPTIM_PWM_Stop(&hlptim1, LPTIM_CHANNEL_1);

  probe->magic = PS_PHASE3_AUDIO_MAGIC;
  probe->phase = PS_PHASE3_AUDIO_PHASE;
  probe->tick_start = HAL_GetTick();
  probe->lptim_kernel_hz = HAL_RCCEx_GetPeriphCLKFreq(RCC_PERIPHCLK_LPTIM1);
  probe->sai_kernel_hz = HAL_RCCEx_GetPeriphCLKFreq(RCC_PERIPHCLK_SAI1);
  probe->speaker_sample_rate_hz = PS_SPEAKER_SAMPLE_RATE_HZ;
  probe->speaker_tone_hz = PS_SPEAKER_SWEEP_END_HZ;
  probe->speaker_buffer_halfwords = PS_SPEAKER_BUFFER_HALFWORDS;
  probe->requested_hz = 1000UL;
  probe->sweep_start_hz = PS_SPEAKER_SWEEP_START_HZ;
  probe->sweep_end_hz = PS_SPEAKER_SWEEP_END_HZ;
  probe->sweep_steps_requested = PS_SPEAKER_SWEEP_STEPS;
  probe->pattern_steps_requested = 0U;
  probe->speaker_segments_requested = PS_SPEAKER_SWEEP_STEPS;
  probe->sd_mode_state_before = (uint32_t)HAL_GPIO_ReadPin(SD_MODE_GPIO_Port, SD_MODE_Pin);

  for (uint32_t segment = 0U; segment < PS_SPEAKER_SWEEP_STEPS; segment++)
  {
    HAL_StatusTypeDef status = PS_Phase3_RunSpeakerSegment(probe,
                                                           segment + 1U,
                                                           PS_SPEAKER_WAVE_SINE,
                                                           PS_SPEAKER_SWEEP_AMPLITUDE,
                                                           speaker_response_hz[segment],
                                                           PS_SPEAKER_SWEEP_STEP_MS);
    if (status != HAL_OK)
    {
      probe->speaker_failed_segment = segment + 1U;
      break;
    }
  }

  HAL_GPIO_WritePin(SD_MODE_GPIO_Port, SD_MODE_Pin, GPIO_PIN_RESET);
  probe->sd_mode_state_after = (uint32_t)HAL_GPIO_ReadPin(SD_MODE_GPIO_Port, SD_MODE_Pin);
  probe->lptim_state_after = (uint32_t)HAL_LPTIM_GetState(&hlptim1);
  probe->lptim_isr_after = LPTIM1->ISR;
  probe->sai_state_after = (uint32_t)HAL_SAI_GetState(&hsai_BlockA1);
  probe->sai_error_after = HAL_SAI_GetError(&hsai_BlockA1);
  probe->sai_sr_after = SAI1_Block_A->SR;
  probe->tick_end = HAL_GetTick();
  probe->complete = 1U;
}

static HAL_StatusTypeDef PS_Phase3_RunSpeakerSegment(volatile PsPhase3AudioProbe *probe,
                                                     uint32_t segment_index,
                                                     uint32_t waveform,
                                                     int16_t amplitude,
                                                     uint32_t frequency_hz,
                                                     uint32_t duration_ms)
{
  HAL_StatusTypeDef status;

  PS_Phase3_PrepareSpeakerBuffer(waveform, amplitude, frequency_hz);
  probe->speaker_tone_hz = frequency_hz;
  probe->speaker_final_waveform = waveform;
  probe->speaker_final_amplitude = (uint32_t)(uint16_t)amplitude;

  HAL_GPIO_WritePin(SD_MODE_GPIO_Port, SD_MODE_Pin, GPIO_PIN_SET);
  HAL_Delay(20U);

  status = HAL_SAI_Transmit_DMA(&hsai_BlockA1,
                                (uint8_t *)g_ps_speaker_dma_buffer,
                                PS_SPEAKER_BUFFER_HALFWORDS);
  probe->speaker_start_status = (uint8_t)status;
  if (status != HAL_OK)
  {
    return status;
  }

  HAL_Delay(duration_ms);

  status = HAL_SAI_DMAStop(&hsai_BlockA1);
  probe->speaker_stop_status = (uint8_t)status;
  HAL_GPIO_WritePin(SD_MODE_GPIO_Port, SD_MODE_Pin, GPIO_PIN_RESET);
  HAL_Delay(PS_SPEAKER_SWEEP_GAP_MS);

  if (status == HAL_OK)
  {
    probe->speaker_segments_completed = segment_index;
  }

  return status;
}

static HAL_StatusTypeDef PS_Phase3_RunBbbTone(volatile PsPhase3AudioProbe *probe,
                                              uint32_t step_index,
                                              uint32_t frequency_hz,
                                              uint32_t duration_ms)
{
  HAL_StatusTypeDef status;
  uint32_t period;
  uint32_t pulse;

  if ((frequency_hz == 0UL) || (frequency_hz > PS_BBB_LPTIM_KERNEL_HZ))
  {
    return HAL_ERROR;
  }

  period = (PS_BBB_LPTIM_KERNEL_HZ / frequency_hz) - 1UL;
  pulse = period / 2UL;
  probe->requested_hz = frequency_hz;
  probe->period = period;
  probe->pulse = pulse;

  status = HAL_LPTIM_PWM_Start(&hlptim1, LPTIM_CHANNEL_1);
  probe->start_status = (uint8_t)status;
  if (status != HAL_OK)
  {
    return status;
  }

  __HAL_LPTIM_AUTORELOAD_SET(&hlptim1, period);
  __HAL_LPTIM_COMPARE_SET(&hlptim1, LPTIM_CHANNEL_1, pulse);

  HAL_Delay(duration_ms);

  status = HAL_LPTIM_PWM_Stop(&hlptim1, LPTIM_CHANNEL_1);
  probe->stop_status = (uint8_t)status;
  HAL_Delay(PS_BBB_PATTERN_GAP_MS);

  if (status == HAL_OK)
  {
    if (step_index <= PS_BBB_PATTERN_STEPS)
    {
      probe->pattern_steps_completed = step_index;
    }
    else
    {
      probe->sweep_steps_completed = step_index - PS_BBB_PATTERN_STEPS;
    }
  }

  return status;
}

static void PS_Phase3_PrepareSpeakerBuffer(uint32_t waveform, int16_t amplitude, uint32_t frequency_hz)
{
  const float phase_step =
    (6.28318530718f *
     (float)(((frequency_hz * PS_SPEAKER_BUFFER_FRAMES) +
              (PS_SPEAKER_SAMPLE_RATE_HZ / 2UL)) /
             PS_SPEAKER_SAMPLE_RATE_HZ)) /
    (float)PS_SPEAKER_BUFFER_FRAMES;

  for (uint32_t frame = 0U; frame < PS_SPEAKER_BUFFER_FRAMES; frame++)
  {
    int16_t sample = 0;

    if (waveform == PS_SPEAKER_WAVE_SINE)
    {
      sample = (int16_t)(sinf((float)frame * phase_step) * (float)amplitude);
    }

    g_ps_speaker_dma_buffer[(frame * 2UL) + 0UL] = (uint16_t)sample;
    g_ps_speaker_dma_buffer[(frame * 2UL) + 1UL] = (uint16_t)sample;
  }
}

void HAL_OSPI_RxCpltCallback(OSPI_HandleTypeDef *hospi)
{
  if (hospi->Instance == OCTOSPI1)
  {
    g_ps_ospi_rx_dma_done = 1U;
  }
}

void HAL_OSPI_TxCpltCallback(OSPI_HandleTypeDef *hospi)
{
  if (hospi->Instance == OCTOSPI1)
  {
    g_ps_ospi_tx_dma_done = 1U;
  }
}

void HAL_OSPI_ErrorCallback(OSPI_HandleTypeDef *hospi)
{
  if (hospi->Instance == OCTOSPI1)
  {
    g_ps_ospi_dma_error = 1U;
  }
}

void HAL_GPIO_EXTI_Rising_Callback(uint16_t GPIO_Pin)
{
  if (GPIO_Pin == BTN_START_Pin)
  {
    (void)PS_ThreadXPostButtonEdgeFromISR(GPIO_Pin, 0U);
  }
  else if ((GPIO_Pin == BTN_A_Pin) ||
           (GPIO_Pin == BTN_B_Pin) ||
           (GPIO_Pin == BTN_L_Pin) ||
           (GPIO_Pin == BTN_R_Pin))
  {
    (void)PS_ThreadXPostButtonEdgeFromISR(GPIO_Pin, 1U);
  }
}

void HAL_GPIO_EXTI_Falling_Callback(uint16_t GPIO_Pin)
{
  if (GPIO_Pin == BTN_START_Pin)
  {
    (void)PS_ThreadXPostButtonEdgeFromISR(GPIO_Pin, 1U);
  }
  else if ((GPIO_Pin == BTN_A_Pin) ||
           (GPIO_Pin == BTN_B_Pin) ||
           (GPIO_Pin == BTN_L_Pin) ||
           (GPIO_Pin == BTN_R_Pin))
  {
    (void)PS_ThreadXPostButtonEdgeFromISR(GPIO_Pin, 0U);
  }
}

void HAL_SAI_TxHalfCpltCallback(SAI_HandleTypeDef *hsai)
{
  if (hsai->Instance == SAI1_Block_A)
  {
    g_ps_phase3_audio_probe.speaker_half_cplt_count++;
  }
}

void HAL_SAI_TxCpltCallback(SAI_HandleTypeDef *hsai)
{
  if (hsai->Instance == SAI1_Block_A)
  {
    g_ps_phase3_audio_probe.speaker_cplt_count++;
  }
}

void HAL_SAI_ErrorCallback(SAI_HandleTypeDef *hsai)
{
  if (hsai->Instance == SAI1_Block_A)
  {
    g_ps_phase3_audio_probe.speaker_error_count++;
  }
}

/* USER CODE END 4 */

/**
  * @brief  Period elapsed callback in non blocking mode
  * @note   This function is called  when TIM6 interrupt took place, inside
  * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
  * a global variable "uwTick" used as application time base.
  * @param  htim : TIM handle
  * @retval None
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  /* USER CODE BEGIN Callback 0 */

  /* USER CODE END Callback 0 */
  if (htim->Instance == TIM6) {
    HAL_IncTick();
  }
  /* USER CODE BEGIN Callback 1 */

  /* USER CODE END Callback 1 */
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

