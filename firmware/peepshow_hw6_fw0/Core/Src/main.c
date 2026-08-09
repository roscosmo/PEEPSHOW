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
#include <ps_hw6_peripheral_probe.h>

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef struct
{
  uint32_t magic;
  uint32_t version;
  uint32_t phase;
  uint32_t complete;
  uint32_t reset_flags;
  uint32_t device_id;
  uint32_t revision_id;
  uint32_t sysclk_hz;
  uint32_t expected_output_mask;
  uint32_t output_mask;
  uint32_t heartbeat;
  uint32_t last_tick;
  uint32_t error_count;
  uint32_t error_phase;
  uint32_t error_code;
  uint32_t assert_count;
  uint32_t assert_line;
  char assert_file[96];
} PS_HW6_FW0_Probe;

#define PS_HW6_ADP5360_REGISTER_COUNT 14U
#define PS_HW6_ADP5360_MAP_REGISTER_COUNT 0x37U
#define PS_HW6_ADP5360_PROFILE_REGISTER_COUNT 6U
#define PS_HW6_ADP5360_PROFILE_GUARD_REGISTER_COUNT 5U
#define PS_HW6_ADP5360_FUEL_REGISTER_COUNT 2U
#define PS_HW6_ADP5360_FUEL_SAMPLE_COUNT 13U

typedef struct
{
  uint32_t magic;
  uint32_t version;
  uint32_t phase;
  uint32_t complete;
  uint32_t success;
  uint32_t start_tick;
  uint32_t end_tick;
  uint32_t duration_ticks;
  uint32_t address_7bit;
  uint32_t address_hal;
  uint32_t i2c_state_before;
  uint32_t i2c_error_before;
  uint32_t ready_status;
  uint32_t ready_error;
  uint32_t i2c_state_after;
  uint32_t i2c_error_after;
  uint32_t attempted_count;
  uint32_t read_count;
  uint32_t failure_count;
  uint32_t read_ok_mask;
  uint8_t register_address[PS_HW6_ADP5360_REGISTER_COUNT];
  uint8_t register_value[PS_HW6_ADP5360_REGISTER_COUNT];
  uint8_t register_status[PS_HW6_ADP5360_REGISTER_COUNT];
  uint8_t reserved0[2];
  uint32_t register_error[PS_HW6_ADP5360_REGISTER_COUNT];
  uint32_t manufacturer_model_id;
  uint32_t manufacturer_id;
  uint32_t model_id;
  uint32_t identity_match;
  uint32_t silicon_revision;
  uint32_t charger_status1;
  uint32_t charger_status2;
  uint32_t battery_soc_percent;
  uint32_t vbat_raw;
  uint32_t vbat_mv;
  uint32_t buck_config;
  uint32_t buck_vout;
  uint32_t buckboost_config;
  uint32_t buckboost_vout;
  uint32_t supervisory;
  uint32_t fault;
  uint32_t pgood;
  uint32_t vout1_ok;
  uint32_t vout2_ok;
  uint32_t rails_ready;
  uint32_t vbus_ok;
  uint32_t battery_ok;
  uint32_t mr_pressed;
} PS_HW6_ADP5360_Probe;

typedef struct
{
  uint32_t magic;
  uint32_t version;
  uint32_t phase;
  uint32_t complete;
  uint32_t success;
  uint32_t start_tick;
  uint32_t end_tick;
  uint32_t duration_ticks;
  uint32_t attempted_count;
  uint32_t read_count;
  uint32_t failure_count;
  uint8_t register_value[PS_HW6_ADP5360_MAP_REGISTER_COUNT];
  uint8_t register_status[PS_HW6_ADP5360_MAP_REGISTER_COUNT];
  uint8_t reserved0[2];
  uint32_t register_error[PS_HW6_ADP5360_MAP_REGISTER_COUNT];
  uint32_t identity_match;
  uint32_t charger_function;
  uint32_t battery_thermistor_control;
  uint32_t battery_protection_control;
  uint32_t battery_capacity_code;
  uint32_t battery_capacity_mah;
  uint32_t fuel_gauge_mode;
  uint32_t fuel_gauge_enabled;
  uint32_t fuel_gauge_sleep_mode;
  uint32_t buck_config;
  uint32_t buck_target_mv;
  uint32_t buckboost_config;
  uint32_t buckboost_target_mv;
  uint32_t supervisory;
  uint32_t fault;
  uint32_t pgood;
  uint32_t interrupt_enable1;
  uint32_t interrupt_enable2;
  uint32_t interrupt_flag1;
  uint32_t interrupt_flag2;
  uint32_t shipmode;
} PS_HW6_ADP5360_MapProbe;

typedef struct
{
  uint32_t magic;
  uint32_t version;
  uint32_t phase;
  uint32_t complete;
  uint32_t success;
  uint32_t skipped;
  uint32_t blocked_phase;
  uint32_t start_tick;
  uint32_t end_tick;
  uint32_t duration_ticks;
  uint32_t guard_required_mask;
  uint32_t guard_pass_mask;
  uint32_t guard_read_ok_mask;
  uint32_t snapshot_ok_mask;
  uint32_t write_ok_mask;
  uint32_t verify_ok_mask;
  uint32_t candidate_match_mask;
  uint32_t restore_attempted;
  uint32_t restore_ok_mask;
  uint32_t restore_verify_ok_mask;
  uint32_t restore_match_mask;
  uint32_t write_attempted_count;
  uint32_t restore_attempted_count;
  uint32_t prewrite_pgood_status;
  uint32_t prewrite_pgood_error;
  uint32_t prewrite_pgood_value;
  uint32_t postwrite_pgood_status;
  uint32_t postwrite_pgood_error;
  uint32_t postwrite_pgood_value;
  uint32_t postwrite_vbus_absent;
  uint32_t final_fault_status;
  uint32_t final_fault_error;
  uint32_t final_fault_value;
  uint32_t final_fault_clear;
  uint32_t final_pgood_status;
  uint32_t final_pgood_error;
  uint32_t final_pgood_value;
  uint32_t final_vbus_absent;
  uint8_t guard_address[PS_HW6_ADP5360_PROFILE_GUARD_REGISTER_COUNT];
  uint8_t guard_value[PS_HW6_ADP5360_PROFILE_GUARD_REGISTER_COUNT];
  uint8_t guard_status[PS_HW6_ADP5360_PROFILE_GUARD_REGISTER_COUNT];
  uint8_t reserved0[1];
  uint32_t guard_error[PS_HW6_ADP5360_PROFILE_GUARD_REGISTER_COUNT];
  uint8_t register_address[PS_HW6_ADP5360_PROFILE_REGISTER_COUNT];
  uint8_t original_value[PS_HW6_ADP5360_PROFILE_REGISTER_COUNT];
  uint8_t candidate_value[PS_HW6_ADP5360_PROFILE_REGISTER_COUNT];
  uint8_t candidate_readback[PS_HW6_ADP5360_PROFILE_REGISTER_COUNT];
  uint8_t restored_readback[PS_HW6_ADP5360_PROFILE_REGISTER_COUNT];
  uint8_t snapshot_status[PS_HW6_ADP5360_PROFILE_REGISTER_COUNT];
  uint8_t write_status[PS_HW6_ADP5360_PROFILE_REGISTER_COUNT];
  uint8_t verify_status[PS_HW6_ADP5360_PROFILE_REGISTER_COUNT];
  uint8_t restore_status[PS_HW6_ADP5360_PROFILE_REGISTER_COUNT];
  uint8_t restore_verify_status[PS_HW6_ADP5360_PROFILE_REGISTER_COUNT];
  uint32_t snapshot_error[PS_HW6_ADP5360_PROFILE_REGISTER_COUNT];
  uint32_t write_error[PS_HW6_ADP5360_PROFILE_REGISTER_COUNT];
  uint32_t verify_error[PS_HW6_ADP5360_PROFILE_REGISTER_COUNT];
  uint32_t restore_error[PS_HW6_ADP5360_PROFILE_REGISTER_COUNT];
  uint32_t restore_verify_error[PS_HW6_ADP5360_PROFILE_REGISTER_COUNT];
} PS_HW6_ADP5360_ProfileProbe;

typedef struct
{
  uint32_t magic;
  uint32_t version;
  uint32_t phase;
  uint32_t complete;
  uint32_t success;
  uint32_t skipped;
  uint32_t blocked_phase;
  uint32_t start_tick;
  uint32_t end_tick;
  uint32_t duration_ticks;
  uint32_t guard_required_mask;
  uint32_t guard_pass_mask;
  uint32_t guard_read_ok_mask;
  uint32_t snapshot_ok_mask;
  uint32_t write_ok_mask;
  uint32_t verify_ok_mask;
  uint32_t candidate_match_mask;
  uint32_t reset_write_ok_mask;
  uint32_t sample_count;
  uint32_t sample_soc_ok_mask;
  uint32_t sample_vbat_h_ok_mask;
  uint32_t sample_vbat_l_ok_mask;
  uint32_t restore_attempted;
  uint32_t restore_ok_mask;
  uint32_t restore_verify_ok_mask;
  uint32_t restore_match_mask;
  uint32_t prewrite_pgood_status;
  uint32_t prewrite_pgood_error;
  uint32_t prewrite_pgood_value;
  uint32_t postwrite_pgood_status;
  uint32_t postwrite_pgood_error;
  uint32_t postwrite_pgood_value;
  uint32_t postwrite_vbus_absent;
  uint32_t first_sample_failure_index;
  uint32_t first_sample_failure_register;
  uint32_t first_sample_failure_status;
  uint32_t first_sample_failure_error;
  uint32_t final_fault_status;
  uint32_t final_fault_error;
  uint32_t final_fault_value;
  uint32_t final_fault_clear;
  uint32_t final_pgood_status;
  uint32_t final_pgood_error;
  uint32_t final_pgood_value;
  uint32_t final_vbus_absent;
  uint8_t guard_address[PS_HW6_ADP5360_PROFILE_GUARD_REGISTER_COUNT];
  uint8_t guard_value[PS_HW6_ADP5360_PROFILE_GUARD_REGISTER_COUNT];
  uint8_t guard_status[PS_HW6_ADP5360_PROFILE_GUARD_REGISTER_COUNT];
  uint8_t reserved0[1];
  uint32_t guard_error[PS_HW6_ADP5360_PROFILE_GUARD_REGISTER_COUNT];
  uint8_t register_address[PS_HW6_ADP5360_FUEL_REGISTER_COUNT];
  uint8_t original_value[PS_HW6_ADP5360_FUEL_REGISTER_COUNT];
  uint8_t candidate_value[PS_HW6_ADP5360_FUEL_REGISTER_COUNT];
  uint8_t candidate_readback[PS_HW6_ADP5360_FUEL_REGISTER_COUNT];
  uint8_t restored_readback[PS_HW6_ADP5360_FUEL_REGISTER_COUNT];
  uint8_t snapshot_status[PS_HW6_ADP5360_FUEL_REGISTER_COUNT];
  uint8_t write_status[PS_HW6_ADP5360_FUEL_REGISTER_COUNT];
  uint8_t verify_status[PS_HW6_ADP5360_FUEL_REGISTER_COUNT];
  uint8_t restore_status[PS_HW6_ADP5360_FUEL_REGISTER_COUNT];
  uint8_t restore_verify_status[PS_HW6_ADP5360_FUEL_REGISTER_COUNT];
  uint8_t reset_value[2];
  uint8_t reset_status[2];
  uint32_t snapshot_error[PS_HW6_ADP5360_FUEL_REGISTER_COUNT];
  uint32_t write_error[PS_HW6_ADP5360_FUEL_REGISTER_COUNT];
  uint32_t verify_error[PS_HW6_ADP5360_FUEL_REGISTER_COUNT];
  uint32_t restore_error[PS_HW6_ADP5360_FUEL_REGISTER_COUNT];
  uint32_t restore_verify_error[PS_HW6_ADP5360_FUEL_REGISTER_COUNT];
  uint32_t reset_error[2];
  uint32_t sample_tick[PS_HW6_ADP5360_FUEL_SAMPLE_COUNT];
  uint32_t sample_vbat_mv[PS_HW6_ADP5360_FUEL_SAMPLE_COUNT];
  uint8_t sample_soc[PS_HW6_ADP5360_FUEL_SAMPLE_COUNT];
  uint8_t sample_vbat_h[PS_HW6_ADP5360_FUEL_SAMPLE_COUNT];
  uint8_t sample_vbat_l[PS_HW6_ADP5360_FUEL_SAMPLE_COUNT];
  uint8_t reserved1[1];
} PS_HW6_ADP5360_FuelProbe;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define PS_HW6_FW0_PROBE_MAGIC                  0x48364630UL
#define PS_HW6_FW0_PROBE_VERSION                0x00000001UL

#define PS_HW6_FW0_PHASE_RESET                  0x00006000UL
#define PS_HW6_FW0_PHASE_HAL_READY              0x00006001UL
#define PS_HW6_FW0_PHASE_CLOCK_READY            0x00006002UL
#define PS_HW6_FW0_PHASE_GPIO_READY             0x00006003UL
#define PS_HW6_FW0_PHASE_RUNNING                0x000060FFUL
#define PS_HW6_FW0_PHASE_ASSERT                 0x00006A00UL
#define PS_HW6_FW0_PHASE_ERROR                  0x00006E00UL

#define PS_HW6_FW0_OUTPUT_PWR_DBG               (1UL << 0)
#define PS_HW6_FW0_OUTPUT_NINA_NRST             (1UL << 1)
#define PS_HW6_FW0_OUTPUT_SD_MODE               (1UL << 2)
#define PS_HW6_FW0_OUTPUT_FLASH_NCS             (1UL << 3)
#define PS_HW6_FW0_OUTPUT_DISPLAY_NSS           (1UL << 4)
#define PS_HW6_FW0_EXPECTED_OUTPUT_MASK          PS_HW6_FW0_OUTPUT_FLASH_NCS

#define PS_HW6_FW0_ERROR_NONE                   0x00000000UL
#define PS_HW6_FW0_ERROR_OUTPUT_MISMATCH        0x00000001UL
#define PS_HW6_FW0_ERROR_HANDLER                0x00000002UL
#define PS_HW6_FW0_ERROR_ASSERT                 0x00000003UL

#define PS_HW6_ADP5360_PROBE_MAGIC              0x4836504DUL
#define PS_HW6_ADP5360_PROBE_VERSION            0x00000001UL
#define PS_HW6_ADP5360_PHASE_RESET              0x00006100UL
#define PS_HW6_ADP5360_PHASE_READY_CHECK        0x00006101UL
#define PS_HW6_ADP5360_PHASE_READING            0x00006102UL
#define PS_HW6_ADP5360_PHASE_COMPLETE           0x000061FFUL
#define PS_HW6_ADP5360_ADDRESS_7BIT             0x46U
#define PS_HW6_ADP5360_ADDRESS_HAL              (PS_HW6_ADP5360_ADDRESS_7BIT << 1U)
#define PS_HW6_ADP5360_EXPECTED_ID              0x10U
#define PS_HW6_ADP5360_ALL_READS_MASK           ((1UL << PS_HW6_ADP5360_REGISTER_COUNT) - 1UL)
#define PS_HW6_ADP5360_TIMEOUT_MS               100U

#define PS_HW6_ADP5360_MAP_MAGIC                0x48364D50UL
#define PS_HW6_ADP5360_MAP_VERSION              0x00000001UL
#define PS_HW6_ADP5360_MAP_PHASE_RESET          0x00006200UL
#define PS_HW6_ADP5360_MAP_PHASE_READING        0x00006201UL
#define PS_HW6_ADP5360_MAP_PHASE_COMPLETE       0x000062FFUL

#define PS_HW6_ADP5360_PROFILE_MAGIC             0x48365046UL
#define PS_HW6_ADP5360_PROFILE_VERSION           0x00000001UL
#define PS_HW6_ADP5360_PROFILE_PHASE_RESET       0x00006300UL
#define PS_HW6_ADP5360_PROFILE_PHASE_SAFETY      0x00006301UL
#define PS_HW6_ADP5360_PROFILE_PHASE_SNAPSHOT    0x00006302UL
#define PS_HW6_ADP5360_PROFILE_PHASE_WRITE       0x00006303UL
#define PS_HW6_ADP5360_PROFILE_PHASE_VERIFY      0x00006304UL
#define PS_HW6_ADP5360_PROFILE_PHASE_RESTORE     0x00006305UL
#define PS_HW6_ADP5360_PROFILE_PHASE_FINAL_CHECK 0x00006306UL
#define PS_HW6_ADP5360_PROFILE_PHASE_COMPLETE    0x000063FFUL
#define PS_HW6_ADP5360_PROFILE_GUARD_IDENTITY    (1UL << 0)
#define PS_HW6_ADP5360_PROFILE_GUARD_VBUS_ABSENT (1UL << 1)
#define PS_HW6_ADP5360_PROFILE_GUARD_CHARGER_OFF (1UL << 2)
#define PS_HW6_ADP5360_PROFILE_GUARD_FAULT_CLEAR (1UL << 3)
#define PS_HW6_ADP5360_PROFILE_GUARD_SW_CHG_OFF  (1UL << 4)
#define PS_HW6_ADP5360_PROFILE_GUARD_VBUS_RECHECK (1UL << 5)
#define PS_HW6_ADP5360_PROFILE_INITIAL_GUARD_MASK 0x0000001FUL
#define PS_HW6_ADP5360_PROFILE_REQUIRED_GUARD_MASK 0x0000003FUL
#define PS_HW6_ADP5360_PROFILE_ALL_REGISTERS_MASK \
  ((1UL << PS_HW6_ADP5360_PROFILE_REGISTER_COUNT) - 1UL)

#define PS_HW6_ADP5360_FUEL_MAGIC                 0x48364647UL
#define PS_HW6_ADP5360_FUEL_VERSION               0x00000001UL
#define PS_HW6_ADP5360_FUEL_PHASE_RESET           0x00006400UL
#define PS_HW6_ADP5360_FUEL_PHASE_SAFETY          0x00006401UL
#define PS_HW6_ADP5360_FUEL_PHASE_SNAPSHOT        0x00006402UL
#define PS_HW6_ADP5360_FUEL_PHASE_CONFIGURE       0x00006403UL
#define PS_HW6_ADP5360_FUEL_PHASE_RESET_SOC       0x00006404UL
#define PS_HW6_ADP5360_FUEL_PHASE_SAMPLE          0x00006405UL
#define PS_HW6_ADP5360_FUEL_PHASE_RESTORE         0x00006406UL
#define PS_HW6_ADP5360_FUEL_PHASE_FINAL_CHECK     0x00006407UL
#define PS_HW6_ADP5360_FUEL_PHASE_COMPLETE        0x000064FFUL
#define PS_HW6_ADP5360_FUEL_ALL_REGISTERS_MASK \
  ((1UL << PS_HW6_ADP5360_FUEL_REGISTER_COUNT) - 1UL)
#define PS_HW6_ADP5360_FUEL_ALL_SAMPLES_MASK \
  ((1UL << PS_HW6_ADP5360_FUEL_SAMPLE_COUNT) - 1UL)
#define PS_HW6_ADP5360_FUEL_SAMPLE_PERIOD_MS      1000U
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

I2C_HandleTypeDef hi2c3;

LPTIM_HandleTypeDef hlptim1;

UART_HandleTypeDef hlpuart1;

OSPI_HandleTypeDef hospi1;
DMA_HandleTypeDef handle_GPDMA1_Channel5;
DMA_HandleTypeDef handle_GPDMA1_Channel4;

RTC_HandleTypeDef hrtc;

SAI_HandleTypeDef hsai_BlockA1;
DMA_NodeTypeDef Node_GPDMA1_Channel3;
DMA_QListTypeDef List_GPDMA1_Channel3;
DMA_HandleTypeDef handle_GPDMA1_Channel3;

SPI_HandleTypeDef hspi3;
DMA_HandleTypeDef handle_LPDMA1_Channel0;

PCD_HandleTypeDef hpcd_USB_OTG_FS;

/* USER CODE BEGIN PV */
volatile PS_HW6_FW0_Probe g_ps_hw6_fw0_probe;
volatile PS_HW6_ADP5360_Probe g_ps_hw6_adp5360_probe;
volatile PS_HW6_ADP5360_MapProbe g_ps_hw6_adp5360_map_probe;
volatile PS_HW6_ADP5360_ProfileProbe g_ps_hw6_adp5360_profile_probe;
volatile PS_HW6_ADP5360_FuelProbe g_ps_hw6_adp5360_fuel_probe;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
void PeriphCommonClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_GPDMA1_Init(void);
static void MX_LPDMA1_Init(void);
static void MX_I2C3_Init(void);
static void MX_LPUART1_UART_Init(void);
static void MX_OCTOSPI1_Init(void);
static void MX_RTC_Init(void);
static void MX_SAI1_Init(void);
static void MX_LPTIM1_Init(void);
static void MX_SPI3_Init(void);
static void MX_USB_OTG_FS_PCD_Init(void);
/* USER CODE BEGIN PFP */
static uint32_t PS_HW6_FW0_ReadOutputMask(void);
static void PS_HW6_FW0_RecordError(uint32_t phase, uint32_t code);
static void PS_HW6_FW0_DebugBreak(void);
static void PS_HW6_FW0_CopyAssertFile(const uint8_t *file);
static HAL_StatusTypeDef PS_HW6_ADP5360_ReadRegister(uint8_t address,
                                                     uint8_t *value);
static HAL_StatusTypeDef PS_HW6_ADP5360_WriteRegister(uint8_t address,
                                                      uint8_t value);
static void PS_HW6_ADP5360_RunReadOnlyProbe(void)
    __attribute__((unused));
static void PS_HW6_ADP5360_RunReadOnlyMapProbe(void)
    __attribute__((unused));
static void PS_HW6_ADP5360_RunReversibleProfileProbe(void)
    __attribute__((unused));
static void PS_HW6_ADP5360_RunFuelGaugeSweepProbe(void)
    __attribute__((unused));
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
static uint32_t PS_HW6_FW0_ReadOutputMask(void)
{
  uint32_t mask = 0U;

  if (HAL_GPIO_ReadPin(GPIOH, GPIO_PIN_1) == GPIO_PIN_SET)
  {
    mask |= PS_HW6_FW0_OUTPUT_PWR_DBG;
  }
  if (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_6) == GPIO_PIN_SET)
  {
    mask |= PS_HW6_FW0_OUTPUT_NINA_NRST;
  }
  if (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_9) == GPIO_PIN_SET)
  {
    mask |= PS_HW6_FW0_OUTPUT_SD_MODE;
  }
  if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0) == GPIO_PIN_SET)
  {
    mask |= PS_HW6_FW0_OUTPUT_FLASH_NCS;
  }
  if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_15) == GPIO_PIN_SET)
  {
    mask |= PS_HW6_FW0_OUTPUT_DISPLAY_NSS;
  }

  return mask;
}

static void PS_HW6_FW0_RecordError(uint32_t phase, uint32_t code)
{
  g_ps_hw6_fw0_probe.error_count++;
  g_ps_hw6_fw0_probe.error_phase = phase;
  g_ps_hw6_fw0_probe.error_code = code;
  g_ps_hw6_fw0_probe.phase = PS_HW6_FW0_PHASE_ERROR;
  g_ps_hw6_fw0_probe.complete = 0U;
}

static void PS_HW6_FW0_DebugBreak(void)
{
  if ((CoreDebug->DHCSR & CoreDebug_DHCSR_C_DEBUGEN_Msk) != 0U)
  {
    __BKPT(0);
  }
}

static void PS_HW6_FW0_CopyAssertFile(const uint8_t *file)
{
  uint32_t index = 0U;

  if (file != NULL)
  {
    while ((index + 1U) < sizeof(g_ps_hw6_fw0_probe.assert_file))
    {
      const uint8_t character = file[index];
      if (character == 0U)
      {
        break;
      }

      g_ps_hw6_fw0_probe.assert_file[index] = (char)character;
      index++;
    }
  }

  g_ps_hw6_fw0_probe.assert_file[index] = '\0';
}

static HAL_StatusTypeDef PS_HW6_ADP5360_ReadRegister(uint8_t address,
                                                     uint8_t *value)
{
  return HAL_I2C_Mem_Read(&hi2c3,
                          PS_HW6_ADP5360_ADDRESS_HAL,
                          address,
                          I2C_MEMADD_SIZE_8BIT,
                          value,
                          1U,
                          PS_HW6_ADP5360_TIMEOUT_MS);
}

static HAL_StatusTypeDef PS_HW6_ADP5360_WriteRegister(uint8_t address,
                                                      uint8_t value)
{
  return HAL_I2C_Mem_Write(&hi2c3,
                           PS_HW6_ADP5360_ADDRESS_HAL,
                           address,
                           I2C_MEMADD_SIZE_8BIT,
                           &value,
                           1U,
                           PS_HW6_ADP5360_TIMEOUT_MS);
}

static void PS_HW6_ADP5360_RunReadOnlyProbe(void)
{
  /* Keep the original bounded baseline stable for direct evidence comparison. */
  static const uint8_t register_address[PS_HW6_ADP5360_REGISTER_COUNT] =
  {
    0x00U, 0x01U, 0x08U, 0x09U, 0x21U, 0x25U, 0x26U,
    0x29U, 0x2AU, 0x2BU, 0x2CU, 0x2DU, 0x2EU, 0x2FU
  };
  uint32_t index;

  g_ps_hw6_adp5360_probe.magic = PS_HW6_ADP5360_PROBE_MAGIC;
  g_ps_hw6_adp5360_probe.version = PS_HW6_ADP5360_PROBE_VERSION;
  g_ps_hw6_adp5360_probe.phase = PS_HW6_ADP5360_PHASE_RESET;
  g_ps_hw6_adp5360_probe.address_7bit = PS_HW6_ADP5360_ADDRESS_7BIT;
  g_ps_hw6_adp5360_probe.address_hal = PS_HW6_ADP5360_ADDRESS_HAL;
  g_ps_hw6_adp5360_probe.i2c_state_before = HAL_I2C_GetState(&hi2c3);
  g_ps_hw6_adp5360_probe.i2c_error_before = HAL_I2C_GetError(&hi2c3);

  for (index = 0U; index < PS_HW6_ADP5360_REGISTER_COUNT; index++)
  {
    g_ps_hw6_adp5360_probe.register_address[index] = register_address[index];
    g_ps_hw6_adp5360_probe.register_value[index] = 0xFFU;
    g_ps_hw6_adp5360_probe.register_status[index] = 0xFFU;
  }

  HAL_GPIO_WritePin(GPIOH, GPIO_PIN_1, GPIO_PIN_SET);
  g_ps_hw6_adp5360_probe.start_tick = HAL_GetTick();
  g_ps_hw6_adp5360_probe.phase = PS_HW6_ADP5360_PHASE_READY_CHECK;
  g_ps_hw6_adp5360_probe.ready_status =
      HAL_I2C_IsDeviceReady(&hi2c3,
                            PS_HW6_ADP5360_ADDRESS_HAL,
                            2U,
                            PS_HW6_ADP5360_TIMEOUT_MS);
  g_ps_hw6_adp5360_probe.ready_error = HAL_I2C_GetError(&hi2c3);

  if (g_ps_hw6_adp5360_probe.ready_status == HAL_OK)
  {
    g_ps_hw6_adp5360_probe.phase = PS_HW6_ADP5360_PHASE_READING;
    for (index = 0U; index < PS_HW6_ADP5360_REGISTER_COUNT; index++)
    {
      uint8_t value = 0U;
      const HAL_StatusTypeDef status =
          PS_HW6_ADP5360_ReadRegister(register_address[index], &value);

      g_ps_hw6_adp5360_probe.attempted_count++;
      g_ps_hw6_adp5360_probe.register_status[index] = (uint8_t)status;
      g_ps_hw6_adp5360_probe.register_error[index] = HAL_I2C_GetError(&hi2c3);
      if (status == HAL_OK)
      {
        g_ps_hw6_adp5360_probe.register_value[index] = value;
        g_ps_hw6_adp5360_probe.read_count++;
        g_ps_hw6_adp5360_probe.read_ok_mask |= (1UL << index);
      }
      else
      {
        g_ps_hw6_adp5360_probe.failure_count++;
      }
    }
  }
  else
  {
    g_ps_hw6_adp5360_probe.failure_count = PS_HW6_ADP5360_REGISTER_COUNT;
  }

  g_ps_hw6_adp5360_probe.manufacturer_model_id =
      g_ps_hw6_adp5360_probe.register_value[0];
  g_ps_hw6_adp5360_probe.manufacturer_id =
      (g_ps_hw6_adp5360_probe.register_value[0] >> 4U) & 0x0FU;
  g_ps_hw6_adp5360_probe.model_id =
      g_ps_hw6_adp5360_probe.register_value[0] & 0x0FU;
  g_ps_hw6_adp5360_probe.identity_match =
      ((g_ps_hw6_adp5360_probe.read_ok_mask & 0x1UL) != 0U) &&
      (g_ps_hw6_adp5360_probe.register_value[0] == PS_HW6_ADP5360_EXPECTED_ID);
  g_ps_hw6_adp5360_probe.silicon_revision =
      g_ps_hw6_adp5360_probe.register_value[1] & 0x0FU;
  g_ps_hw6_adp5360_probe.charger_status1 =
      g_ps_hw6_adp5360_probe.register_value[2];
  g_ps_hw6_adp5360_probe.charger_status2 =
      g_ps_hw6_adp5360_probe.register_value[3];
  g_ps_hw6_adp5360_probe.battery_soc_percent =
      g_ps_hw6_adp5360_probe.register_value[4] & 0x7FU;
  g_ps_hw6_adp5360_probe.vbat_raw =
      ((uint32_t)g_ps_hw6_adp5360_probe.register_value[5] << 5U) |
      ((uint32_t)g_ps_hw6_adp5360_probe.register_value[6] >> 3U);
  g_ps_hw6_adp5360_probe.vbat_mv = g_ps_hw6_adp5360_probe.vbat_raw;
  g_ps_hw6_adp5360_probe.buck_config =
      g_ps_hw6_adp5360_probe.register_value[7];
  g_ps_hw6_adp5360_probe.buck_vout =
      g_ps_hw6_adp5360_probe.register_value[8];
  g_ps_hw6_adp5360_probe.buckboost_config =
      g_ps_hw6_adp5360_probe.register_value[9];
  g_ps_hw6_adp5360_probe.buckboost_vout =
      g_ps_hw6_adp5360_probe.register_value[10];
  g_ps_hw6_adp5360_probe.supervisory =
      g_ps_hw6_adp5360_probe.register_value[11];
  g_ps_hw6_adp5360_probe.fault =
      g_ps_hw6_adp5360_probe.register_value[12];
  g_ps_hw6_adp5360_probe.pgood =
      g_ps_hw6_adp5360_probe.register_value[13];
  g_ps_hw6_adp5360_probe.vout1_ok =
      g_ps_hw6_adp5360_probe.register_value[13] & 0x01U;
  g_ps_hw6_adp5360_probe.vout2_ok =
      (g_ps_hw6_adp5360_probe.register_value[13] >> 1U) & 0x01U;
  g_ps_hw6_adp5360_probe.rails_ready =
      (g_ps_hw6_adp5360_probe.register_value[13] & 0x03U) == 0x03U;
  g_ps_hw6_adp5360_probe.vbus_ok =
      (g_ps_hw6_adp5360_probe.register_value[13] >> 3U) & 0x01U;
  g_ps_hw6_adp5360_probe.battery_ok =
      (g_ps_hw6_adp5360_probe.register_value[13] >> 2U) & 0x01U;
  g_ps_hw6_adp5360_probe.mr_pressed =
      (g_ps_hw6_adp5360_probe.register_value[13] >> 5U) & 0x01U;
  g_ps_hw6_adp5360_probe.i2c_state_after = HAL_I2C_GetState(&hi2c3);
  g_ps_hw6_adp5360_probe.i2c_error_after = HAL_I2C_GetError(&hi2c3);
  g_ps_hw6_adp5360_probe.end_tick = HAL_GetTick();
  g_ps_hw6_adp5360_probe.duration_ticks =
      g_ps_hw6_adp5360_probe.end_tick - g_ps_hw6_adp5360_probe.start_tick;
  HAL_GPIO_WritePin(GPIOH, GPIO_PIN_1, GPIO_PIN_RESET);

  g_ps_hw6_adp5360_probe.success =
      (g_ps_hw6_adp5360_probe.ready_status == HAL_OK) &&
      (g_ps_hw6_adp5360_probe.read_ok_mask == PS_HW6_ADP5360_ALL_READS_MASK) &&
      (g_ps_hw6_adp5360_probe.identity_match != 0U);
  g_ps_hw6_adp5360_probe.complete = 1U;
  g_ps_hw6_adp5360_probe.phase = PS_HW6_ADP5360_PHASE_COMPLETE;
}

static void PS_HW6_ADP5360_RunReadOnlyMapProbe(void)
{
  uint32_t address;

  g_ps_hw6_adp5360_map_probe.magic = PS_HW6_ADP5360_MAP_MAGIC;
  g_ps_hw6_adp5360_map_probe.version = PS_HW6_ADP5360_MAP_VERSION;
  g_ps_hw6_adp5360_map_probe.phase = PS_HW6_ADP5360_MAP_PHASE_RESET;
  g_ps_hw6_adp5360_map_probe.start_tick = HAL_GetTick();
  g_ps_hw6_adp5360_map_probe.phase = PS_HW6_ADP5360_MAP_PHASE_READING;

  for (address = 0U; address < PS_HW6_ADP5360_MAP_REGISTER_COUNT; address++)
  {
    uint8_t value = 0xFFU;
    const HAL_StatusTypeDef status =
        PS_HW6_ADP5360_ReadRegister((uint8_t)address, &value);

    g_ps_hw6_adp5360_map_probe.attempted_count++;
    g_ps_hw6_adp5360_map_probe.register_status[address] = (uint8_t)status;
    g_ps_hw6_adp5360_map_probe.register_error[address] = HAL_I2C_GetError(&hi2c3);
    if (status == HAL_OK)
    {
      g_ps_hw6_adp5360_map_probe.register_value[address] = value;
      g_ps_hw6_adp5360_map_probe.read_count++;
    }
    else
    {
      g_ps_hw6_adp5360_map_probe.register_value[address] = 0xFFU;
      g_ps_hw6_adp5360_map_probe.failure_count++;
    }
  }

  g_ps_hw6_adp5360_map_probe.identity_match =
      g_ps_hw6_adp5360_map_probe.register_value[0x00U] ==
      PS_HW6_ADP5360_EXPECTED_ID;
  g_ps_hw6_adp5360_map_probe.charger_function =
      g_ps_hw6_adp5360_map_probe.register_value[0x07U];
  g_ps_hw6_adp5360_map_probe.battery_thermistor_control =
      g_ps_hw6_adp5360_map_probe.register_value[0x0AU];
  g_ps_hw6_adp5360_map_probe.battery_protection_control =
      g_ps_hw6_adp5360_map_probe.register_value[0x11U];
  g_ps_hw6_adp5360_map_probe.battery_capacity_code =
      g_ps_hw6_adp5360_map_probe.register_value[0x20U];
  g_ps_hw6_adp5360_map_probe.battery_capacity_mah =
      (uint32_t)g_ps_hw6_adp5360_map_probe.register_value[0x20U] * 2U;
  g_ps_hw6_adp5360_map_probe.fuel_gauge_mode =
      g_ps_hw6_adp5360_map_probe.register_value[0x27U];
  g_ps_hw6_adp5360_map_probe.fuel_gauge_enabled =
      g_ps_hw6_adp5360_map_probe.register_value[0x27U] & 0x01U;
  g_ps_hw6_adp5360_map_probe.fuel_gauge_sleep_mode =
      (g_ps_hw6_adp5360_map_probe.register_value[0x27U] >> 1U) & 0x01U;
  g_ps_hw6_adp5360_map_probe.buck_config =
      g_ps_hw6_adp5360_map_probe.register_value[0x29U];
  g_ps_hw6_adp5360_map_probe.buck_target_mv =
      600U +
      ((uint32_t)(g_ps_hw6_adp5360_map_probe.register_value[0x2AU] & 0x3FU) *
       50U);
  g_ps_hw6_adp5360_map_probe.buckboost_config =
      g_ps_hw6_adp5360_map_probe.register_value[0x2BU];
  {
    const uint32_t code =
        g_ps_hw6_adp5360_map_probe.register_value[0x2CU] & 0x3FU;
    g_ps_hw6_adp5360_map_probe.buckboost_target_mv =
        (code <= 11U) ? (1800U + (code * 100U))
                      : (2950U + ((code - 12U) * 50U));
  }
  g_ps_hw6_adp5360_map_probe.supervisory =
      g_ps_hw6_adp5360_map_probe.register_value[0x2DU];
  g_ps_hw6_adp5360_map_probe.fault =
      g_ps_hw6_adp5360_map_probe.register_value[0x2EU];
  g_ps_hw6_adp5360_map_probe.pgood =
      g_ps_hw6_adp5360_map_probe.register_value[0x2FU];
  g_ps_hw6_adp5360_map_probe.interrupt_enable1 =
      g_ps_hw6_adp5360_map_probe.register_value[0x32U];
  g_ps_hw6_adp5360_map_probe.interrupt_enable2 =
      g_ps_hw6_adp5360_map_probe.register_value[0x33U];
  g_ps_hw6_adp5360_map_probe.interrupt_flag1 =
      g_ps_hw6_adp5360_map_probe.register_value[0x34U];
  g_ps_hw6_adp5360_map_probe.interrupt_flag2 =
      g_ps_hw6_adp5360_map_probe.register_value[0x35U];
  g_ps_hw6_adp5360_map_probe.shipmode =
      g_ps_hw6_adp5360_map_probe.register_value[0x36U];

  g_ps_hw6_adp5360_map_probe.end_tick = HAL_GetTick();
  g_ps_hw6_adp5360_map_probe.duration_ticks =
      g_ps_hw6_adp5360_map_probe.end_tick -
      g_ps_hw6_adp5360_map_probe.start_tick;
  g_ps_hw6_adp5360_map_probe.success =
      (g_ps_hw6_adp5360_map_probe.read_count ==
       PS_HW6_ADP5360_MAP_REGISTER_COUNT) &&
      (g_ps_hw6_adp5360_map_probe.identity_match != 0U);
  g_ps_hw6_adp5360_map_probe.complete = 1U;
  g_ps_hw6_adp5360_map_probe.phase = PS_HW6_ADP5360_MAP_PHASE_COMPLETE;
}

static void PS_HW6_ADP5360_RunReversibleProfileProbe(void)
{
  static const uint8_t guard_address[PS_HW6_ADP5360_PROFILE_GUARD_REGISTER_COUNT] =
  {
    0x00U, 0x08U, 0x2EU, 0x2FU, 0x07U
  };
  static const uint8_t register_address[PS_HW6_ADP5360_PROFILE_REGISTER_COUNT] =
  {
    0x0AU, 0x03U, 0x04U, 0x20U, 0x27U, 0x07U
  };
  static const uint8_t fixed_candidate_value[PS_HW6_ADP5360_PROFILE_REGISTER_COUNT] =
  {
    0x80U, 0x82U, 0x3FU, 0xE1U, 0x53U, 0x00U
  };
  uint32_t index;
  uint8_t value;
  HAL_StatusTypeDef status;

  g_ps_hw6_adp5360_profile_probe = (PS_HW6_ADP5360_ProfileProbe){0};
  g_ps_hw6_adp5360_profile_probe.magic = PS_HW6_ADP5360_PROFILE_MAGIC;
  g_ps_hw6_adp5360_profile_probe.version = PS_HW6_ADP5360_PROFILE_VERSION;
  g_ps_hw6_adp5360_profile_probe.phase = PS_HW6_ADP5360_PROFILE_PHASE_RESET;
  g_ps_hw6_adp5360_profile_probe.guard_required_mask =
      PS_HW6_ADP5360_PROFILE_REQUIRED_GUARD_MASK;

  for (index = 0U;
       index < PS_HW6_ADP5360_PROFILE_GUARD_REGISTER_COUNT;
       index++)
  {
    g_ps_hw6_adp5360_profile_probe.guard_address[index] = guard_address[index];
    g_ps_hw6_adp5360_profile_probe.guard_value[index] = 0xFFU;
    g_ps_hw6_adp5360_profile_probe.guard_status[index] = 0xFFU;
  }

  for (index = 0U; index < PS_HW6_ADP5360_PROFILE_REGISTER_COUNT; index++)
  {
    g_ps_hw6_adp5360_profile_probe.register_address[index] =
        register_address[index];
    g_ps_hw6_adp5360_profile_probe.original_value[index] = 0xFFU;
    g_ps_hw6_adp5360_profile_probe.candidate_value[index] =
        fixed_candidate_value[index];
    g_ps_hw6_adp5360_profile_probe.candidate_readback[index] = 0xFFU;
    g_ps_hw6_adp5360_profile_probe.restored_readback[index] = 0xFFU;
    g_ps_hw6_adp5360_profile_probe.snapshot_status[index] = 0xFFU;
    g_ps_hw6_adp5360_profile_probe.write_status[index] = 0xFFU;
    g_ps_hw6_adp5360_profile_probe.verify_status[index] = 0xFFU;
    g_ps_hw6_adp5360_profile_probe.restore_status[index] = 0xFFU;
    g_ps_hw6_adp5360_profile_probe.restore_verify_status[index] = 0xFFU;
  }

  g_ps_hw6_adp5360_profile_probe.prewrite_pgood_value = 0xFFU;
  g_ps_hw6_adp5360_profile_probe.postwrite_pgood_value = 0xFFU;
  g_ps_hw6_adp5360_profile_probe.final_fault_value = 0xFFU;
  g_ps_hw6_adp5360_profile_probe.final_pgood_value = 0xFFU;
  g_ps_hw6_adp5360_profile_probe.start_tick = HAL_GetTick();
  g_ps_hw6_adp5360_profile_probe.phase = PS_HW6_ADP5360_PROFILE_PHASE_SAFETY;

  for (index = 0U;
       index < PS_HW6_ADP5360_PROFILE_GUARD_REGISTER_COUNT;
       index++)
  {
    value = 0xFFU;
    status = PS_HW6_ADP5360_ReadRegister(guard_address[index], &value);
    g_ps_hw6_adp5360_profile_probe.guard_status[index] = (uint8_t)status;
    g_ps_hw6_adp5360_profile_probe.guard_error[index] = HAL_I2C_GetError(&hi2c3);
    if (status == HAL_OK)
    {
      g_ps_hw6_adp5360_profile_probe.guard_value[index] = value;
      g_ps_hw6_adp5360_profile_probe.guard_read_ok_mask |= (1UL << index);
    }
  }

  if (((g_ps_hw6_adp5360_profile_probe.guard_read_ok_mask & (1UL << 0)) != 0U) &&
      (g_ps_hw6_adp5360_profile_probe.guard_value[0] == PS_HW6_ADP5360_EXPECTED_ID))
  {
    g_ps_hw6_adp5360_profile_probe.guard_pass_mask |=
        PS_HW6_ADP5360_PROFILE_GUARD_IDENTITY;
  }
  if (((g_ps_hw6_adp5360_profile_probe.guard_read_ok_mask & (1UL << 3)) != 0U) &&
      ((g_ps_hw6_adp5360_profile_probe.guard_value[3] & 0x08U) == 0U))
  {
    g_ps_hw6_adp5360_profile_probe.guard_pass_mask |=
        PS_HW6_ADP5360_PROFILE_GUARD_VBUS_ABSENT;
  }
  if (((g_ps_hw6_adp5360_profile_probe.guard_read_ok_mask & (1UL << 1)) != 0U) &&
      (g_ps_hw6_adp5360_profile_probe.guard_value[1] == 0x00U))
  {
    g_ps_hw6_adp5360_profile_probe.guard_pass_mask |=
        PS_HW6_ADP5360_PROFILE_GUARD_CHARGER_OFF;
  }
  if (((g_ps_hw6_adp5360_profile_probe.guard_read_ok_mask & (1UL << 2)) != 0U) &&
      (g_ps_hw6_adp5360_profile_probe.guard_value[2] == 0x00U))
  {
    g_ps_hw6_adp5360_profile_probe.guard_pass_mask |=
        PS_HW6_ADP5360_PROFILE_GUARD_FAULT_CLEAR;
  }
  if (((g_ps_hw6_adp5360_profile_probe.guard_read_ok_mask & (1UL << 4)) != 0U) &&
      ((g_ps_hw6_adp5360_profile_probe.guard_value[4] & 0x01U) == 0U))
  {
    g_ps_hw6_adp5360_profile_probe.guard_pass_mask |=
        PS_HW6_ADP5360_PROFILE_GUARD_SW_CHG_OFF;
  }

  if ((g_ps_hw6_adp5360_profile_probe.guard_pass_mask &
       PS_HW6_ADP5360_PROFILE_INITIAL_GUARD_MASK) !=
      PS_HW6_ADP5360_PROFILE_INITIAL_GUARD_MASK)
  {
    g_ps_hw6_adp5360_profile_probe.skipped = 1U;
    g_ps_hw6_adp5360_profile_probe.blocked_phase =
        PS_HW6_ADP5360_PROFILE_PHASE_SAFETY;
    goto profile_complete;
  }

  g_ps_hw6_adp5360_profile_probe.phase = PS_HW6_ADP5360_PROFILE_PHASE_SNAPSHOT;
  for (index = 0U; index < PS_HW6_ADP5360_PROFILE_REGISTER_COUNT; index++)
  {
    value = 0xFFU;
    status = PS_HW6_ADP5360_ReadRegister(register_address[index], &value);
    g_ps_hw6_adp5360_profile_probe.snapshot_status[index] = (uint8_t)status;
    g_ps_hw6_adp5360_profile_probe.snapshot_error[index] = HAL_I2C_GetError(&hi2c3);
    if (status == HAL_OK)
    {
      g_ps_hw6_adp5360_profile_probe.original_value[index] = value;
      g_ps_hw6_adp5360_profile_probe.snapshot_ok_mask |= (1UL << index);
    }
  }

  if (g_ps_hw6_adp5360_profile_probe.snapshot_ok_mask !=
      PS_HW6_ADP5360_PROFILE_ALL_REGISTERS_MASK)
  {
    g_ps_hw6_adp5360_profile_probe.skipped = 1U;
    g_ps_hw6_adp5360_profile_probe.blocked_phase =
        PS_HW6_ADP5360_PROFILE_PHASE_SNAPSHOT;
    goto profile_complete;
  }

  g_ps_hw6_adp5360_profile_probe.candidate_value[5] =
      (g_ps_hw6_adp5360_profile_probe.original_value[5] | 0x80U) & 0xFEU;

  value = 0xFFU;
  status = PS_HW6_ADP5360_ReadRegister(0x2FU, &value);
  g_ps_hw6_adp5360_profile_probe.prewrite_pgood_status = (uint32_t)status;
  g_ps_hw6_adp5360_profile_probe.prewrite_pgood_error = HAL_I2C_GetError(&hi2c3);
  if (status == HAL_OK)
  {
    g_ps_hw6_adp5360_profile_probe.prewrite_pgood_value = value;
    if ((value & 0x08U) == 0U)
    {
      g_ps_hw6_adp5360_profile_probe.guard_pass_mask |=
          PS_HW6_ADP5360_PROFILE_GUARD_VBUS_RECHECK;
    }
  }

  if ((g_ps_hw6_adp5360_profile_probe.guard_pass_mask &
       PS_HW6_ADP5360_PROFILE_REQUIRED_GUARD_MASK) !=
      PS_HW6_ADP5360_PROFILE_REQUIRED_GUARD_MASK)
  {
    g_ps_hw6_adp5360_profile_probe.skipped = 1U;
    g_ps_hw6_adp5360_profile_probe.blocked_phase =
        PS_HW6_ADP5360_PROFILE_PHASE_SAFETY;
    goto profile_complete;
  }

  HAL_GPIO_WritePin(GPIOH, GPIO_PIN_1, GPIO_PIN_SET);
  g_ps_hw6_adp5360_profile_probe.phase = PS_HW6_ADP5360_PROFILE_PHASE_WRITE;
  for (index = 0U; index < PS_HW6_ADP5360_PROFILE_REGISTER_COUNT; index++)
  {
    status = PS_HW6_ADP5360_WriteRegister(
        register_address[index],
        g_ps_hw6_adp5360_profile_probe.candidate_value[index]);
    g_ps_hw6_adp5360_profile_probe.write_attempted_count++;
    g_ps_hw6_adp5360_profile_probe.write_status[index] = (uint8_t)status;
    g_ps_hw6_adp5360_profile_probe.write_error[index] = HAL_I2C_GetError(&hi2c3);
    if (status != HAL_OK)
    {
      g_ps_hw6_adp5360_profile_probe.blocked_phase =
          PS_HW6_ADP5360_PROFILE_PHASE_WRITE;
      break;
    }
    g_ps_hw6_adp5360_profile_probe.write_ok_mask |= (1UL << index);
  }

  value = 0xFFU;
  status = PS_HW6_ADP5360_ReadRegister(0x2FU, &value);
  g_ps_hw6_adp5360_profile_probe.postwrite_pgood_status = (uint32_t)status;
  g_ps_hw6_adp5360_profile_probe.postwrite_pgood_error = HAL_I2C_GetError(&hi2c3);
  if (status == HAL_OK)
  {
    g_ps_hw6_adp5360_profile_probe.postwrite_pgood_value = value;
    g_ps_hw6_adp5360_profile_probe.postwrite_vbus_absent =
        ((value & 0x08U) == 0U);
  }

  if ((g_ps_hw6_adp5360_profile_probe.write_ok_mask ==
       PS_HW6_ADP5360_PROFILE_ALL_REGISTERS_MASK) &&
      (g_ps_hw6_adp5360_profile_probe.postwrite_vbus_absent != 0U))
  {
    g_ps_hw6_adp5360_profile_probe.phase = PS_HW6_ADP5360_PROFILE_PHASE_VERIFY;
    for (index = 0U; index < PS_HW6_ADP5360_PROFILE_REGISTER_COUNT; index++)
    {
      value = 0xFFU;
      status = PS_HW6_ADP5360_ReadRegister(register_address[index], &value);
      g_ps_hw6_adp5360_profile_probe.verify_status[index] = (uint8_t)status;
      g_ps_hw6_adp5360_profile_probe.verify_error[index] = HAL_I2C_GetError(&hi2c3);
      if (status == HAL_OK)
      {
        g_ps_hw6_adp5360_profile_probe.candidate_readback[index] = value;
        g_ps_hw6_adp5360_profile_probe.verify_ok_mask |= (1UL << index);
        if (value == g_ps_hw6_adp5360_profile_probe.candidate_value[index])
        {
          g_ps_hw6_adp5360_profile_probe.candidate_match_mask |= (1UL << index);
        }
      }
    }
    if ((g_ps_hw6_adp5360_profile_probe.verify_ok_mask !=
         PS_HW6_ADP5360_PROFILE_ALL_REGISTERS_MASK) ||
        (g_ps_hw6_adp5360_profile_probe.candidate_match_mask !=
         PS_HW6_ADP5360_PROFILE_ALL_REGISTERS_MASK))
    {
      g_ps_hw6_adp5360_profile_probe.blocked_phase =
          PS_HW6_ADP5360_PROFILE_PHASE_VERIFY;
    }
  }
  else if (g_ps_hw6_adp5360_profile_probe.blocked_phase == 0U)
  {
    g_ps_hw6_adp5360_profile_probe.blocked_phase =
        (g_ps_hw6_adp5360_profile_probe.postwrite_vbus_absent == 0U)
            ? PS_HW6_ADP5360_PROFILE_PHASE_SAFETY
            : PS_HW6_ADP5360_PROFILE_PHASE_WRITE;
  }

  g_ps_hw6_adp5360_profile_probe.phase = PS_HW6_ADP5360_PROFILE_PHASE_RESTORE;
  g_ps_hw6_adp5360_profile_probe.restore_attempted = 1U;
  for (index = PS_HW6_ADP5360_PROFILE_REGISTER_COUNT; index > 0U; index--)
  {
    const uint32_t restore_index = index - 1U;

    status = PS_HW6_ADP5360_WriteRegister(
        register_address[restore_index],
        g_ps_hw6_adp5360_profile_probe.original_value[restore_index]);
    g_ps_hw6_adp5360_profile_probe.restore_attempted_count++;
    g_ps_hw6_adp5360_profile_probe.restore_status[restore_index] =
        (uint8_t)status;
    g_ps_hw6_adp5360_profile_probe.restore_error[restore_index] =
        HAL_I2C_GetError(&hi2c3);
    if (status == HAL_OK)
    {
      g_ps_hw6_adp5360_profile_probe.restore_ok_mask |=
          (1UL << restore_index);
    }
  }

  for (index = 0U; index < PS_HW6_ADP5360_PROFILE_REGISTER_COUNT; index++)
  {
    value = 0xFFU;
    status = PS_HW6_ADP5360_ReadRegister(register_address[index], &value);
    g_ps_hw6_adp5360_profile_probe.restore_verify_status[index] =
        (uint8_t)status;
    g_ps_hw6_adp5360_profile_probe.restore_verify_error[index] =
        HAL_I2C_GetError(&hi2c3);
    if (status == HAL_OK)
    {
      g_ps_hw6_adp5360_profile_probe.restored_readback[index] = value;
      g_ps_hw6_adp5360_profile_probe.restore_verify_ok_mask |= (1UL << index);
      if (value == g_ps_hw6_adp5360_profile_probe.original_value[index])
      {
        g_ps_hw6_adp5360_profile_probe.restore_match_mask |= (1UL << index);
      }
    }
  }

  g_ps_hw6_adp5360_profile_probe.phase =
      PS_HW6_ADP5360_PROFILE_PHASE_FINAL_CHECK;
  value = 0xFFU;
  status = PS_HW6_ADP5360_ReadRegister(0x2EU, &value);
  g_ps_hw6_adp5360_profile_probe.final_fault_status = (uint32_t)status;
  g_ps_hw6_adp5360_profile_probe.final_fault_error = HAL_I2C_GetError(&hi2c3);
  if (status == HAL_OK)
  {
    g_ps_hw6_adp5360_profile_probe.final_fault_value = value;
    g_ps_hw6_adp5360_profile_probe.final_fault_clear = (value == 0x00U);
  }

  value = 0xFFU;
  status = PS_HW6_ADP5360_ReadRegister(0x2FU, &value);
  g_ps_hw6_adp5360_profile_probe.final_pgood_status = (uint32_t)status;
  g_ps_hw6_adp5360_profile_probe.final_pgood_error = HAL_I2C_GetError(&hi2c3);
  if (status == HAL_OK)
  {
    g_ps_hw6_adp5360_profile_probe.final_pgood_value = value;
    g_ps_hw6_adp5360_profile_probe.final_vbus_absent =
        ((value & 0x08U) == 0U);
  }

  if (((g_ps_hw6_adp5360_profile_probe.restore_ok_mask !=
        PS_HW6_ADP5360_PROFILE_ALL_REGISTERS_MASK) ||
       (g_ps_hw6_adp5360_profile_probe.restore_verify_ok_mask !=
        PS_HW6_ADP5360_PROFILE_ALL_REGISTERS_MASK) ||
       (g_ps_hw6_adp5360_profile_probe.restore_match_mask !=
        PS_HW6_ADP5360_PROFILE_ALL_REGISTERS_MASK)) &&
      (g_ps_hw6_adp5360_profile_probe.blocked_phase == 0U))
  {
    g_ps_hw6_adp5360_profile_probe.blocked_phase =
        PS_HW6_ADP5360_PROFILE_PHASE_RESTORE;
  }
  if (((g_ps_hw6_adp5360_profile_probe.final_fault_clear == 0U) ||
       (g_ps_hw6_adp5360_profile_probe.final_vbus_absent == 0U)) &&
      (g_ps_hw6_adp5360_profile_probe.blocked_phase == 0U))
  {
    g_ps_hw6_adp5360_profile_probe.blocked_phase =
        PS_HW6_ADP5360_PROFILE_PHASE_FINAL_CHECK;
  }

  g_ps_hw6_adp5360_profile_probe.success =
      ((g_ps_hw6_adp5360_profile_probe.guard_pass_mask &
        PS_HW6_ADP5360_PROFILE_REQUIRED_GUARD_MASK) ==
       PS_HW6_ADP5360_PROFILE_REQUIRED_GUARD_MASK) &&
      (g_ps_hw6_adp5360_profile_probe.snapshot_ok_mask ==
       PS_HW6_ADP5360_PROFILE_ALL_REGISTERS_MASK) &&
      (g_ps_hw6_adp5360_profile_probe.write_ok_mask ==
       PS_HW6_ADP5360_PROFILE_ALL_REGISTERS_MASK) &&
      (g_ps_hw6_adp5360_profile_probe.postwrite_vbus_absent != 0U) &&
      (g_ps_hw6_adp5360_profile_probe.verify_ok_mask ==
       PS_HW6_ADP5360_PROFILE_ALL_REGISTERS_MASK) &&
      (g_ps_hw6_adp5360_profile_probe.candidate_match_mask ==
       PS_HW6_ADP5360_PROFILE_ALL_REGISTERS_MASK) &&
      (g_ps_hw6_adp5360_profile_probe.restore_ok_mask ==
       PS_HW6_ADP5360_PROFILE_ALL_REGISTERS_MASK) &&
      (g_ps_hw6_adp5360_profile_probe.restore_verify_ok_mask ==
       PS_HW6_ADP5360_PROFILE_ALL_REGISTERS_MASK) &&
      (g_ps_hw6_adp5360_profile_probe.restore_match_mask ==
       PS_HW6_ADP5360_PROFILE_ALL_REGISTERS_MASK) &&
      (g_ps_hw6_adp5360_profile_probe.final_fault_clear != 0U) &&
      (g_ps_hw6_adp5360_profile_probe.final_vbus_absent != 0U);

profile_complete:
  HAL_GPIO_WritePin(GPIOH, GPIO_PIN_1, GPIO_PIN_RESET);
  g_ps_hw6_adp5360_profile_probe.end_tick = HAL_GetTick();
  g_ps_hw6_adp5360_profile_probe.duration_ticks =
      g_ps_hw6_adp5360_profile_probe.end_tick -
      g_ps_hw6_adp5360_profile_probe.start_tick;
  g_ps_hw6_adp5360_profile_probe.complete = 1U;
  g_ps_hw6_adp5360_profile_probe.phase =
      PS_HW6_ADP5360_PROFILE_PHASE_COMPLETE;
}

static void PS_HW6_ADP5360_RunFuelGaugeSweepProbe(void)
{
  static const uint8_t guard_address[PS_HW6_ADP5360_PROFILE_GUARD_REGISTER_COUNT] =
  {
    0x00U, 0x08U, 0x2EU, 0x2FU, 0x07U
  };
  static const uint8_t register_address[PS_HW6_ADP5360_FUEL_REGISTER_COUNT] =
  {
    0x20U, 0x27U
  };
  static const uint8_t candidate_value[PS_HW6_ADP5360_FUEL_REGISTER_COUNT] =
  {
    0xE1U, 0x51U
  };
  static const uint8_t reset_value[2] = {0x80U, 0x00U};
  uint32_t index;
  uint32_t sample_start_tick = 0U;
  uint8_t value;
  HAL_StatusTypeDef status;

  g_ps_hw6_adp5360_fuel_probe = (PS_HW6_ADP5360_FuelProbe){0};
  g_ps_hw6_adp5360_fuel_probe.magic = PS_HW6_ADP5360_FUEL_MAGIC;
  g_ps_hw6_adp5360_fuel_probe.version = PS_HW6_ADP5360_FUEL_VERSION;
  g_ps_hw6_adp5360_fuel_probe.phase = PS_HW6_ADP5360_FUEL_PHASE_RESET;
  g_ps_hw6_adp5360_fuel_probe.guard_required_mask =
      PS_HW6_ADP5360_PROFILE_REQUIRED_GUARD_MASK;
  g_ps_hw6_adp5360_fuel_probe.first_sample_failure_index = 0xFFFFFFFFUL;
  g_ps_hw6_adp5360_fuel_probe.first_sample_failure_register = 0xFFFFFFFFUL;
  g_ps_hw6_adp5360_fuel_probe.prewrite_pgood_value = 0xFFU;
  g_ps_hw6_adp5360_fuel_probe.postwrite_pgood_value = 0xFFU;
  g_ps_hw6_adp5360_fuel_probe.final_fault_value = 0xFFU;
  g_ps_hw6_adp5360_fuel_probe.final_pgood_value = 0xFFU;

  for (index = 0U;
       index < PS_HW6_ADP5360_PROFILE_GUARD_REGISTER_COUNT;
       index++)
  {
    g_ps_hw6_adp5360_fuel_probe.guard_address[index] = guard_address[index];
    g_ps_hw6_adp5360_fuel_probe.guard_value[index] = 0xFFU;
    g_ps_hw6_adp5360_fuel_probe.guard_status[index] = 0xFFU;
  }
  for (index = 0U; index < PS_HW6_ADP5360_FUEL_REGISTER_COUNT; index++)
  {
    g_ps_hw6_adp5360_fuel_probe.register_address[index] =
        register_address[index];
    g_ps_hw6_adp5360_fuel_probe.original_value[index] = 0xFFU;
    g_ps_hw6_adp5360_fuel_probe.candidate_value[index] =
        candidate_value[index];
    g_ps_hw6_adp5360_fuel_probe.candidate_readback[index] = 0xFFU;
    g_ps_hw6_adp5360_fuel_probe.restored_readback[index] = 0xFFU;
    g_ps_hw6_adp5360_fuel_probe.snapshot_status[index] = 0xFFU;
    g_ps_hw6_adp5360_fuel_probe.write_status[index] = 0xFFU;
    g_ps_hw6_adp5360_fuel_probe.verify_status[index] = 0xFFU;
    g_ps_hw6_adp5360_fuel_probe.restore_status[index] = 0xFFU;
    g_ps_hw6_adp5360_fuel_probe.restore_verify_status[index] = 0xFFU;
  }
  for (index = 0U; index < 2U; index++)
  {
    g_ps_hw6_adp5360_fuel_probe.reset_value[index] = reset_value[index];
    g_ps_hw6_adp5360_fuel_probe.reset_status[index] = 0xFFU;
  }
  for (index = 0U; index < PS_HW6_ADP5360_FUEL_SAMPLE_COUNT; index++)
  {
    g_ps_hw6_adp5360_fuel_probe.sample_soc[index] = 0xFFU;
    g_ps_hw6_adp5360_fuel_probe.sample_vbat_h[index] = 0xFFU;
    g_ps_hw6_adp5360_fuel_probe.sample_vbat_l[index] = 0xFFU;
    g_ps_hw6_adp5360_fuel_probe.sample_vbat_mv[index] = 0xFFFFFFFFUL;
  }

  g_ps_hw6_adp5360_fuel_probe.start_tick = HAL_GetTick();
  g_ps_hw6_adp5360_fuel_probe.phase = PS_HW6_ADP5360_FUEL_PHASE_SAFETY;

  for (index = 0U;
       index < PS_HW6_ADP5360_PROFILE_GUARD_REGISTER_COUNT;
       index++)
  {
    value = 0xFFU;
    status = PS_HW6_ADP5360_ReadRegister(guard_address[index], &value);
    g_ps_hw6_adp5360_fuel_probe.guard_status[index] = (uint8_t)status;
    g_ps_hw6_adp5360_fuel_probe.guard_error[index] = HAL_I2C_GetError(&hi2c3);
    if (status == HAL_OK)
    {
      g_ps_hw6_adp5360_fuel_probe.guard_value[index] = value;
      g_ps_hw6_adp5360_fuel_probe.guard_read_ok_mask |= (1UL << index);
    }
  }

  if (((g_ps_hw6_adp5360_fuel_probe.guard_read_ok_mask & (1UL << 0)) != 0U) &&
      (g_ps_hw6_adp5360_fuel_probe.guard_value[0] == PS_HW6_ADP5360_EXPECTED_ID))
  {
    g_ps_hw6_adp5360_fuel_probe.guard_pass_mask |=
        PS_HW6_ADP5360_PROFILE_GUARD_IDENTITY;
  }
  if (((g_ps_hw6_adp5360_fuel_probe.guard_read_ok_mask & (1UL << 3)) != 0U) &&
      ((g_ps_hw6_adp5360_fuel_probe.guard_value[3] & 0x08U) == 0U))
  {
    g_ps_hw6_adp5360_fuel_probe.guard_pass_mask |=
        PS_HW6_ADP5360_PROFILE_GUARD_VBUS_ABSENT;
  }
  if (((g_ps_hw6_adp5360_fuel_probe.guard_read_ok_mask & (1UL << 1)) != 0U) &&
      (g_ps_hw6_adp5360_fuel_probe.guard_value[1] == 0x00U))
  {
    g_ps_hw6_adp5360_fuel_probe.guard_pass_mask |=
        PS_HW6_ADP5360_PROFILE_GUARD_CHARGER_OFF;
  }
  if (((g_ps_hw6_adp5360_fuel_probe.guard_read_ok_mask & (1UL << 2)) != 0U) &&
      (g_ps_hw6_adp5360_fuel_probe.guard_value[2] == 0x00U))
  {
    g_ps_hw6_adp5360_fuel_probe.guard_pass_mask |=
        PS_HW6_ADP5360_PROFILE_GUARD_FAULT_CLEAR;
  }
  if (((g_ps_hw6_adp5360_fuel_probe.guard_read_ok_mask & (1UL << 4)) != 0U) &&
      ((g_ps_hw6_adp5360_fuel_probe.guard_value[4] & 0x01U) == 0U))
  {
    g_ps_hw6_adp5360_fuel_probe.guard_pass_mask |=
        PS_HW6_ADP5360_PROFILE_GUARD_SW_CHG_OFF;
  }
  if ((g_ps_hw6_adp5360_fuel_probe.guard_pass_mask &
       PS_HW6_ADP5360_PROFILE_INITIAL_GUARD_MASK) !=
      PS_HW6_ADP5360_PROFILE_INITIAL_GUARD_MASK)
  {
    g_ps_hw6_adp5360_fuel_probe.skipped = 1U;
    g_ps_hw6_adp5360_fuel_probe.blocked_phase =
        PS_HW6_ADP5360_FUEL_PHASE_SAFETY;
    goto fuel_complete;
  }

  g_ps_hw6_adp5360_fuel_probe.phase = PS_HW6_ADP5360_FUEL_PHASE_SNAPSHOT;
  for (index = 0U; index < PS_HW6_ADP5360_FUEL_REGISTER_COUNT; index++)
  {
    value = 0xFFU;
    status = PS_HW6_ADP5360_ReadRegister(register_address[index], &value);
    g_ps_hw6_adp5360_fuel_probe.snapshot_status[index] = (uint8_t)status;
    g_ps_hw6_adp5360_fuel_probe.snapshot_error[index] = HAL_I2C_GetError(&hi2c3);
    if (status == HAL_OK)
    {
      g_ps_hw6_adp5360_fuel_probe.original_value[index] = value;
      g_ps_hw6_adp5360_fuel_probe.snapshot_ok_mask |= (1UL << index);
    }
  }
  if (g_ps_hw6_adp5360_fuel_probe.snapshot_ok_mask !=
      PS_HW6_ADP5360_FUEL_ALL_REGISTERS_MASK)
  {
    g_ps_hw6_adp5360_fuel_probe.skipped = 1U;
    g_ps_hw6_adp5360_fuel_probe.blocked_phase =
        PS_HW6_ADP5360_FUEL_PHASE_SNAPSHOT;
    goto fuel_complete;
  }

  value = 0xFFU;
  status = PS_HW6_ADP5360_ReadRegister(0x2FU, &value);
  g_ps_hw6_adp5360_fuel_probe.prewrite_pgood_status = (uint32_t)status;
  g_ps_hw6_adp5360_fuel_probe.prewrite_pgood_error = HAL_I2C_GetError(&hi2c3);
  if (status == HAL_OK)
  {
    g_ps_hw6_adp5360_fuel_probe.prewrite_pgood_value = value;
    if ((value & 0x08U) == 0U)
    {
      g_ps_hw6_adp5360_fuel_probe.guard_pass_mask |=
          PS_HW6_ADP5360_PROFILE_GUARD_VBUS_RECHECK;
    }
  }
  if ((g_ps_hw6_adp5360_fuel_probe.guard_pass_mask &
       PS_HW6_ADP5360_PROFILE_REQUIRED_GUARD_MASK) !=
      PS_HW6_ADP5360_PROFILE_REQUIRED_GUARD_MASK)
  {
    g_ps_hw6_adp5360_fuel_probe.skipped = 1U;
    g_ps_hw6_adp5360_fuel_probe.blocked_phase =
        PS_HW6_ADP5360_FUEL_PHASE_SAFETY;
    goto fuel_complete;
  }
  g_ps_hw6_adp5360_fuel_probe.phase = PS_HW6_ADP5360_FUEL_PHASE_CONFIGURE;
  for (index = 0U; index < PS_HW6_ADP5360_FUEL_REGISTER_COUNT; index++)
  {
    status = PS_HW6_ADP5360_WriteRegister(register_address[index],
                                           candidate_value[index]);
    g_ps_hw6_adp5360_fuel_probe.write_status[index] = (uint8_t)status;
    g_ps_hw6_adp5360_fuel_probe.write_error[index] = HAL_I2C_GetError(&hi2c3);
    if (status != HAL_OK)
    {
      g_ps_hw6_adp5360_fuel_probe.blocked_phase =
          PS_HW6_ADP5360_FUEL_PHASE_CONFIGURE;
      break;
    }
    g_ps_hw6_adp5360_fuel_probe.write_ok_mask |= (1UL << index);
  }

  value = 0xFFU;
  status = PS_HW6_ADP5360_ReadRegister(0x2FU, &value);
  g_ps_hw6_adp5360_fuel_probe.postwrite_pgood_status = (uint32_t)status;
  g_ps_hw6_adp5360_fuel_probe.postwrite_pgood_error = HAL_I2C_GetError(&hi2c3);
  if (status == HAL_OK)
  {
    g_ps_hw6_adp5360_fuel_probe.postwrite_pgood_value = value;
    g_ps_hw6_adp5360_fuel_probe.postwrite_vbus_absent =
        ((value & 0x08U) == 0U);
  }

  if ((g_ps_hw6_adp5360_fuel_probe.write_ok_mask ==
       PS_HW6_ADP5360_FUEL_ALL_REGISTERS_MASK) &&
      (g_ps_hw6_adp5360_fuel_probe.postwrite_vbus_absent != 0U))
  {
    for (index = 0U; index < PS_HW6_ADP5360_FUEL_REGISTER_COUNT; index++)
    {
      value = 0xFFU;
      status = PS_HW6_ADP5360_ReadRegister(register_address[index], &value);
      g_ps_hw6_adp5360_fuel_probe.verify_status[index] = (uint8_t)status;
      g_ps_hw6_adp5360_fuel_probe.verify_error[index] = HAL_I2C_GetError(&hi2c3);
      if (status == HAL_OK)
      {
        g_ps_hw6_adp5360_fuel_probe.candidate_readback[index] = value;
        g_ps_hw6_adp5360_fuel_probe.verify_ok_mask |= (1UL << index);
        if (value == candidate_value[index])
        {
          g_ps_hw6_adp5360_fuel_probe.candidate_match_mask |= (1UL << index);
        }
      }
    }
  }

  if ((g_ps_hw6_adp5360_fuel_probe.write_ok_mask !=
       PS_HW6_ADP5360_FUEL_ALL_REGISTERS_MASK) ||
      (g_ps_hw6_adp5360_fuel_probe.verify_ok_mask !=
       PS_HW6_ADP5360_FUEL_ALL_REGISTERS_MASK) ||
      (g_ps_hw6_adp5360_fuel_probe.candidate_match_mask !=
       PS_HW6_ADP5360_FUEL_ALL_REGISTERS_MASK) ||
      (g_ps_hw6_adp5360_fuel_probe.postwrite_vbus_absent == 0U))
  {
    if (g_ps_hw6_adp5360_fuel_probe.blocked_phase == 0U)
    {
      g_ps_hw6_adp5360_fuel_probe.blocked_phase =
          PS_HW6_ADP5360_FUEL_PHASE_CONFIGURE;
    }
    goto fuel_restore;
  }

  HAL_GPIO_WritePin(GPIOH, GPIO_PIN_1, GPIO_PIN_SET);
  g_ps_hw6_adp5360_fuel_probe.phase = PS_HW6_ADP5360_FUEL_PHASE_RESET_SOC;
  for (index = 0U; index < 2U; index++)
  {
    status = PS_HW6_ADP5360_WriteRegister(0x28U, reset_value[index]);
    g_ps_hw6_adp5360_fuel_probe.reset_status[index] = (uint8_t)status;
    g_ps_hw6_adp5360_fuel_probe.reset_error[index] = HAL_I2C_GetError(&hi2c3);
    if (status != HAL_OK)
    {
      g_ps_hw6_adp5360_fuel_probe.blocked_phase =
          PS_HW6_ADP5360_FUEL_PHASE_RESET_SOC;
      break;
    }
    g_ps_hw6_adp5360_fuel_probe.reset_write_ok_mask |= (1UL << index);
  }
  if (g_ps_hw6_adp5360_fuel_probe.reset_write_ok_mask != 0x03U)
  {
    goto fuel_restore;
  }

  g_ps_hw6_adp5360_fuel_probe.phase = PS_HW6_ADP5360_FUEL_PHASE_SAMPLE;
  sample_start_tick = HAL_GetTick();
  for (index = 0U; index < PS_HW6_ADP5360_FUEL_SAMPLE_COUNT; index++)
  {
    const uint32_t target_tick =
        sample_start_tick + (index * PS_HW6_ADP5360_FUEL_SAMPLE_PERIOD_MS);

    while ((int32_t)(HAL_GetTick() - target_tick) < 0)
    {
      __WFI();
    }
    g_ps_hw6_adp5360_fuel_probe.sample_tick[index] = HAL_GetTick();

    value = 0xFFU;
    status = PS_HW6_ADP5360_ReadRegister(0x21U, &value);
    if (status == HAL_OK)
    {
      g_ps_hw6_adp5360_fuel_probe.sample_soc[index] = value & 0x7FU;
      g_ps_hw6_adp5360_fuel_probe.sample_soc_ok_mask |= (1UL << index);
    }
    else if (g_ps_hw6_adp5360_fuel_probe.first_sample_failure_index ==
             0xFFFFFFFFUL)
    {
      g_ps_hw6_adp5360_fuel_probe.first_sample_failure_index = index;
      g_ps_hw6_adp5360_fuel_probe.first_sample_failure_register = 0x21U;
      g_ps_hw6_adp5360_fuel_probe.first_sample_failure_status =
          (uint32_t)status;
      g_ps_hw6_adp5360_fuel_probe.first_sample_failure_error =
          HAL_I2C_GetError(&hi2c3);
    }

    value = 0xFFU;
    status = PS_HW6_ADP5360_ReadRegister(0x25U, &value);
    if (status == HAL_OK)
    {
      g_ps_hw6_adp5360_fuel_probe.sample_vbat_h[index] = value;
      g_ps_hw6_adp5360_fuel_probe.sample_vbat_h_ok_mask |= (1UL << index);
    }
    else if (g_ps_hw6_adp5360_fuel_probe.first_sample_failure_index ==
             0xFFFFFFFFUL)
    {
      g_ps_hw6_adp5360_fuel_probe.first_sample_failure_index = index;
      g_ps_hw6_adp5360_fuel_probe.first_sample_failure_register = 0x25U;
      g_ps_hw6_adp5360_fuel_probe.first_sample_failure_status =
          (uint32_t)status;
      g_ps_hw6_adp5360_fuel_probe.first_sample_failure_error =
          HAL_I2C_GetError(&hi2c3);
    }

    value = 0xFFU;
    status = PS_HW6_ADP5360_ReadRegister(0x26U, &value);
    if (status == HAL_OK)
    {
      g_ps_hw6_adp5360_fuel_probe.sample_vbat_l[index] = value;
      g_ps_hw6_adp5360_fuel_probe.sample_vbat_l_ok_mask |= (1UL << index);
    }
    else if (g_ps_hw6_adp5360_fuel_probe.first_sample_failure_index ==
             0xFFFFFFFFUL)
    {
      g_ps_hw6_adp5360_fuel_probe.first_sample_failure_index = index;
      g_ps_hw6_adp5360_fuel_probe.first_sample_failure_register = 0x26U;
      g_ps_hw6_adp5360_fuel_probe.first_sample_failure_status =
          (uint32_t)status;
      g_ps_hw6_adp5360_fuel_probe.first_sample_failure_error =
          HAL_I2C_GetError(&hi2c3);
    }

    if (((g_ps_hw6_adp5360_fuel_probe.sample_vbat_h_ok_mask &
          (1UL << index)) != 0U) &&
        ((g_ps_hw6_adp5360_fuel_probe.sample_vbat_l_ok_mask &
          (1UL << index)) != 0U))
    {
      g_ps_hw6_adp5360_fuel_probe.sample_vbat_mv[index] =
          ((uint32_t)g_ps_hw6_adp5360_fuel_probe.sample_vbat_h[index] << 5U) |
          ((uint32_t)g_ps_hw6_adp5360_fuel_probe.sample_vbat_l[index] >> 3U);
    }
    g_ps_hw6_adp5360_fuel_probe.sample_count = index + 1U;
  }

  if ((g_ps_hw6_adp5360_fuel_probe.sample_soc_ok_mask !=
       PS_HW6_ADP5360_FUEL_ALL_SAMPLES_MASK) ||
      (g_ps_hw6_adp5360_fuel_probe.sample_vbat_h_ok_mask !=
       PS_HW6_ADP5360_FUEL_ALL_SAMPLES_MASK) ||
      (g_ps_hw6_adp5360_fuel_probe.sample_vbat_l_ok_mask !=
       PS_HW6_ADP5360_FUEL_ALL_SAMPLES_MASK))
  {
    g_ps_hw6_adp5360_fuel_probe.blocked_phase =
        PS_HW6_ADP5360_FUEL_PHASE_SAMPLE;
  }
fuel_restore:
  HAL_GPIO_WritePin(GPIOH, GPIO_PIN_1, GPIO_PIN_RESET);
  g_ps_hw6_adp5360_fuel_probe.phase = PS_HW6_ADP5360_FUEL_PHASE_RESTORE;
  g_ps_hw6_adp5360_fuel_probe.restore_attempted = 1U;
  for (index = PS_HW6_ADP5360_FUEL_REGISTER_COUNT; index > 0U; index--)
  {
    const uint32_t restore_index = index - 1U;

    status = PS_HW6_ADP5360_WriteRegister(
        register_address[restore_index],
        g_ps_hw6_adp5360_fuel_probe.original_value[restore_index]);
    g_ps_hw6_adp5360_fuel_probe.restore_status[restore_index] =
        (uint8_t)status;
    g_ps_hw6_adp5360_fuel_probe.restore_error[restore_index] =
        HAL_I2C_GetError(&hi2c3);
    if (status == HAL_OK)
    {
      g_ps_hw6_adp5360_fuel_probe.restore_ok_mask |=
          (1UL << restore_index);
    }
  }
  for (index = 0U; index < PS_HW6_ADP5360_FUEL_REGISTER_COUNT; index++)
  {
    value = 0xFFU;
    status = PS_HW6_ADP5360_ReadRegister(register_address[index], &value);
    g_ps_hw6_adp5360_fuel_probe.restore_verify_status[index] =
        (uint8_t)status;
    g_ps_hw6_adp5360_fuel_probe.restore_verify_error[index] =
        HAL_I2C_GetError(&hi2c3);
    if (status == HAL_OK)
    {
      g_ps_hw6_adp5360_fuel_probe.restored_readback[index] = value;
      g_ps_hw6_adp5360_fuel_probe.restore_verify_ok_mask |= (1UL << index);
      if (value == g_ps_hw6_adp5360_fuel_probe.original_value[index])
      {
        g_ps_hw6_adp5360_fuel_probe.restore_match_mask |= (1UL << index);
      }
    }
  }
  if ((g_ps_hw6_adp5360_fuel_probe.restore_ok_mask !=
       PS_HW6_ADP5360_FUEL_ALL_REGISTERS_MASK) ||
      (g_ps_hw6_adp5360_fuel_probe.restore_verify_ok_mask !=
       PS_HW6_ADP5360_FUEL_ALL_REGISTERS_MASK) ||
      (g_ps_hw6_adp5360_fuel_probe.restore_match_mask !=
       PS_HW6_ADP5360_FUEL_ALL_REGISTERS_MASK))
  {
    g_ps_hw6_adp5360_fuel_probe.blocked_phase =
        PS_HW6_ADP5360_FUEL_PHASE_RESTORE;
  }

  g_ps_hw6_adp5360_fuel_probe.phase =
      PS_HW6_ADP5360_FUEL_PHASE_FINAL_CHECK;
  value = 0xFFU;
  status = PS_HW6_ADP5360_ReadRegister(0x2EU, &value);
  g_ps_hw6_adp5360_fuel_probe.final_fault_status = (uint32_t)status;
  g_ps_hw6_adp5360_fuel_probe.final_fault_error = HAL_I2C_GetError(&hi2c3);
  if (status == HAL_OK)
  {
    g_ps_hw6_adp5360_fuel_probe.final_fault_value = value;
    g_ps_hw6_adp5360_fuel_probe.final_fault_clear = (value == 0x00U);
  }
  value = 0xFFU;
  status = PS_HW6_ADP5360_ReadRegister(0x2FU, &value);
  g_ps_hw6_adp5360_fuel_probe.final_pgood_status = (uint32_t)status;
  g_ps_hw6_adp5360_fuel_probe.final_pgood_error = HAL_I2C_GetError(&hi2c3);
  if (status == HAL_OK)
  {
    g_ps_hw6_adp5360_fuel_probe.final_pgood_value = value;
    g_ps_hw6_adp5360_fuel_probe.final_vbus_absent =
        ((value & 0x08U) == 0U);
  }
  if (((g_ps_hw6_adp5360_fuel_probe.final_fault_clear == 0U) ||
       (g_ps_hw6_adp5360_fuel_probe.final_vbus_absent == 0U)) &&
      (g_ps_hw6_adp5360_fuel_probe.blocked_phase == 0U))
  {
    g_ps_hw6_adp5360_fuel_probe.blocked_phase =
        PS_HW6_ADP5360_FUEL_PHASE_FINAL_CHECK;
  }

  g_ps_hw6_adp5360_fuel_probe.success =
      ((g_ps_hw6_adp5360_fuel_probe.guard_pass_mask &
        PS_HW6_ADP5360_PROFILE_REQUIRED_GUARD_MASK) ==
       PS_HW6_ADP5360_PROFILE_REQUIRED_GUARD_MASK) &&
      (g_ps_hw6_adp5360_fuel_probe.snapshot_ok_mask ==
       PS_HW6_ADP5360_FUEL_ALL_REGISTERS_MASK) &&
      (g_ps_hw6_adp5360_fuel_probe.write_ok_mask ==
       PS_HW6_ADP5360_FUEL_ALL_REGISTERS_MASK) &&
      (g_ps_hw6_adp5360_fuel_probe.candidate_match_mask ==
       PS_HW6_ADP5360_FUEL_ALL_REGISTERS_MASK) &&
      (g_ps_hw6_adp5360_fuel_probe.reset_write_ok_mask == 0x03U) &&
      (g_ps_hw6_adp5360_fuel_probe.sample_count ==
       PS_HW6_ADP5360_FUEL_SAMPLE_COUNT) &&
      (g_ps_hw6_adp5360_fuel_probe.sample_soc_ok_mask ==
       PS_HW6_ADP5360_FUEL_ALL_SAMPLES_MASK) &&
      (g_ps_hw6_adp5360_fuel_probe.sample_vbat_h_ok_mask ==
       PS_HW6_ADP5360_FUEL_ALL_SAMPLES_MASK) &&
      (g_ps_hw6_adp5360_fuel_probe.sample_vbat_l_ok_mask ==
       PS_HW6_ADP5360_FUEL_ALL_SAMPLES_MASK) &&
      (g_ps_hw6_adp5360_fuel_probe.restore_match_mask ==
       PS_HW6_ADP5360_FUEL_ALL_REGISTERS_MASK) &&
      (g_ps_hw6_adp5360_fuel_probe.final_fault_clear != 0U) &&
      (g_ps_hw6_adp5360_fuel_probe.final_vbus_absent != 0U) &&
      (g_ps_hw6_adp5360_fuel_probe.blocked_phase == 0U);

fuel_complete:
  HAL_GPIO_WritePin(GPIOH, GPIO_PIN_1, GPIO_PIN_RESET);
  g_ps_hw6_adp5360_fuel_probe.end_tick = HAL_GetTick();
  g_ps_hw6_adp5360_fuel_probe.duration_ticks =
      g_ps_hw6_adp5360_fuel_probe.end_tick -
      g_ps_hw6_adp5360_fuel_probe.start_tick;
  g_ps_hw6_adp5360_fuel_probe.complete = 1U;
  g_ps_hw6_adp5360_fuel_probe.phase = PS_HW6_ADP5360_FUEL_PHASE_COMPLETE;
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */
  g_ps_hw6_fw0_probe.magic = PS_HW6_FW0_PROBE_MAGIC;
  g_ps_hw6_fw0_probe.version = PS_HW6_FW0_PROBE_VERSION;
  g_ps_hw6_fw0_probe.phase = PS_HW6_FW0_PHASE_RESET;
  g_ps_hw6_fw0_probe.reset_flags = RCC->CSR;
  g_ps_hw6_fw0_probe.expected_output_mask = PS_HW6_FW0_EXPECTED_OUTPUT_MASK;
  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */
  g_ps_hw6_fw0_probe.device_id = HAL_GetDEVID();
  g_ps_hw6_fw0_probe.revision_id = HAL_GetREVID();
  g_ps_hw6_fw0_probe.phase = PS_HW6_FW0_PHASE_HAL_READY;
  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* Configure the peripherals common clocks */
  PeriphCommonClock_Config();

  /* USER CODE BEGIN SysInit */
  g_ps_hw6_fw0_probe.sysclk_hz = HAL_RCC_GetSysClockFreq();
  g_ps_hw6_fw0_probe.phase = PS_HW6_FW0_PHASE_CLOCK_READY;
  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_GPDMA1_Init();
  MX_LPDMA1_Init();
  MX_I2C3_Init();
  MX_LPUART1_UART_Init();
  MX_OCTOSPI1_Init();
  MX_RTC_Init();
  MX_SAI1_Init();
  MX_LPTIM1_Init();
  MX_SPI3_Init();
  MX_USB_OTG_FS_PCD_Init();
  /* USER CODE BEGIN 2 */
  /* Keep OTG FS IRQ masked until the storage owner explicitly exports MSC. */
  HAL_NVIC_DisableIRQ(OTG_FS_IRQn);
  NVIC_ClearPendingIRQ(OTG_FS_IRQn);

  g_ps_hw6_fw0_probe.output_mask = PS_HW6_FW0_ReadOutputMask();
  g_ps_hw6_fw0_probe.phase = PS_HW6_FW0_PHASE_GPIO_READY;
  if (g_ps_hw6_fw0_probe.output_mask !=
      g_ps_hw6_fw0_probe.expected_output_mask)
  {
    PS_HW6_FW0_RecordError(PS_HW6_FW0_PHASE_GPIO_READY,
                          PS_HW6_FW0_ERROR_OUTPUT_MISMATCH);
    Error_Handler();
  }
  PS_HW6_PeripheralProbe_Run();
  g_ps_hw6_fw0_probe.complete = 1U;
  g_ps_hw6_fw0_probe.phase = PS_HW6_FW0_PHASE_RUNNING;
  g_ps_hw6_fw0_probe.last_tick = HAL_GetTick();
  /* USER CODE END 2 */

  MX_ThreadX_Init();

  /* We should never get here as control is now taken by the scheduler */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  uint32_t heartbeat_tick = HAL_GetTick();
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    const uint32_t now = HAL_GetTick();
    if ((uint32_t)(now - heartbeat_tick) >= 250U)
    {
      heartbeat_tick = now;
      g_ps_hw6_fw0_probe.heartbeat++;
      g_ps_hw6_fw0_probe.last_tick = now;
      g_ps_hw6_fw0_probe.output_mask = PS_HW6_FW0_ReadOutputMask();
    }
    __WFI();
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
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI48|RCC_OSCILLATORTYPE_HSI
                              |RCC_OSCILLATORTYPE_LSE|RCC_OSCILLATORTYPE_MSI
                              |RCC_OSCILLATORTYPE_MSIK;
  RCC_OscInitStruct.LSEState = RCC_LSE_BYPASS;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSI48State = RCC_HSI48_ON;
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
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB3CLKDivider = RCC_HCLK_DIV8;

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
    HAL_NVIC_SetPriority(GPDMA1_Channel3_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(GPDMA1_Channel3_IRQn);
    HAL_NVIC_SetPriority(GPDMA1_Channel4_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(GPDMA1_Channel4_IRQn);
    HAL_NVIC_SetPriority(GPDMA1_Channel5_IRQn, 5, 0);
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
  hi2c3.Init.Timing = 0x00000E14;
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
    HAL_NVIC_SetPriority(LPDMA1_Channel0_IRQn, 5, 0);
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

  LPTIM_IC_ConfigTypeDef sConfig = {0};

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
  sConfig.ICInputSource = LPTIM_IC1SOURCE_COMP1;
  sConfig.ICPrescaler = LPTIM_ICPSC_DIV1;
  sConfig.ICPolarity = LPTIM_ICPOLARITY_RISING;
  sConfig.ICFilter = LPTIM_ICFLT_CLOCK_DIV1;
  if (HAL_LPTIM_IC_ConfigChannel(&hlptim1, &sConfig, LPTIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN LPTIM1_Init 2 */

  /* USER CODE END LPTIM1_Init 2 */

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
  * @brief RTC Initialization Function
  * @param None
  * @retval None
  */
static void MX_RTC_Init(void)
{

  /* USER CODE BEGIN RTC_Init 0 */

  /* USER CODE END RTC_Init 0 */

  RTC_PrivilegeStateTypeDef privilegeState = {0};
  RTC_TimeTypeDef sTime = {0};
  RTC_DateTypeDef sDate = {0};

  /* USER CODE BEGIN RTC_Init 1 */

  /* USER CODE END RTC_Init 1 */

  /** Initialize RTC Only
  */
  hrtc.Instance = RTC;
  hrtc.Init.HourFormat = RTC_HOURFORMAT_24;
  hrtc.Init.AsynchPrediv = 127;
  hrtc.Init.SynchPrediv = 255;
  hrtc.Init.OutPut = RTC_OUTPUT_DISABLE;
  hrtc.Init.OutPutRemap = RTC_OUTPUT_REMAP_NONE;
  hrtc.Init.OutPutPolarity = RTC_OUTPUT_POLARITY_HIGH;
  hrtc.Init.OutPutType = RTC_OUTPUT_TYPE_OPENDRAIN;
  hrtc.Init.OutPutPullUp = RTC_OUTPUT_PULLUP_NONE;
  hrtc.Init.BinMode = RTC_BINARY_NONE;
  if (HAL_RTC_Init(&hrtc) != HAL_OK)
  {
    Error_Handler();
  }
  privilegeState.rtcPrivilegeFull = RTC_PRIVILEGE_FULL_NO;
  privilegeState.backupRegisterPrivZone = RTC_PRIVILEGE_BKUP_ZONE_NONE;
  privilegeState.backupRegisterStartZone2 = RTC_BKP_DR0;
  privilegeState.backupRegisterStartZone3 = RTC_BKP_DR0;
  if (HAL_RTCEx_PrivilegeModeSet(&hrtc, &privilegeState) != HAL_OK)
  {
    Error_Handler();
  }

  /* USER CODE BEGIN Check_RTC_BKUP */

  /* USER CODE END Check_RTC_BKUP */

  /** Initialize RTC and set the Time and Date
  */
  sTime.Hours = 0x0;
  sTime.Minutes = 0x0;
  sTime.Seconds = 0x0;
  sTime.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
  sTime.StoreOperation = RTC_STOREOPERATION_RESET;
  if (HAL_RTC_SetTime(&hrtc, &sTime, RTC_FORMAT_BCD) != HAL_OK)
  {
    Error_Handler();
  }
  sDate.WeekDay = RTC_WEEKDAY_MONDAY;
  sDate.Month = RTC_MONTH_JANUARY;
  sDate.Date = 0x1;
  sDate.Year = 0x0;

  if (HAL_RTC_SetDate(&hrtc, &sDate, RTC_FORMAT_BCD) != HAL_OK)
  {
    Error_Handler();
  }

  /** Enable Calibration
  */
  if (HAL_RTCEx_SetCalibrationOutPut(&hrtc, RTC_CALIBOUTPUT_1HZ) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN RTC_Init 2 */

  /* USER CODE END RTC_Init 2 */

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
  hspi3.Init.Direction = SPI_DIRECTION_2LINES_TXONLY;
  hspi3.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi3.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi3.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi3.Init.NSS = SPI_NSS_HARD_OUTPUT;
  hspi3.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_2;
  hspi3.Init.FirstBit = SPI_FIRSTBIT_LSB;
  hspi3.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi3.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi3.Init.CRCPolynomial = 0x7;
  hspi3.Init.NSSPMode = SPI_NSS_PULSE_DISABLE;
  hspi3.Init.NSSPolarity = SPI_NSS_POLARITY_HIGH;
  hspi3.Init.FifoThreshold = SPI_FIFO_THRESHOLD_01DATA;
  hspi3.Init.MasterSSIdleness = SPI_MASTER_SS_IDLENESS_00CYCLE;
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
  HAL_SPI_AutonomousMode_Cfg_Struct.TriggerSelection = SPI_GRP2_LPDMA_CH0_TCF_TRG;
  HAL_SPI_AutonomousMode_Cfg_Struct.TriggerPolarity = SPI_TRIG_POLARITY_RISING;
  if (HAL_SPIEx_SetConfigAutonomousMode(&hspi3, &HAL_SPI_AutonomousMode_Cfg_Struct) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI3_Init 2 */

  /* USER CODE END SPI3_Init 2 */

}

/**
  * @brief USB_OTG_FS Initialization Function
  * @param None
  * @retval None
  */
static void MX_USB_OTG_FS_PCD_Init(void)
{

  /* USER CODE BEGIN USB_OTG_FS_Init 0 */

  /* USER CODE END USB_OTG_FS_Init 0 */

  /* USER CODE BEGIN USB_OTG_FS_Init 1 */

  /* USER CODE END USB_OTG_FS_Init 1 */
  hpcd_USB_OTG_FS.Instance = USB_OTG_FS;
  hpcd_USB_OTG_FS.Init.dev_endpoints = 6;
  hpcd_USB_OTG_FS.Init.speed = PCD_SPEED_FULL;
  hpcd_USB_OTG_FS.Init.phy_itface = PCD_PHY_EMBEDDED;
  hpcd_USB_OTG_FS.Init.Sof_enable = DISABLE;
  hpcd_USB_OTG_FS.Init.low_power_enable = DISABLE;
  hpcd_USB_OTG_FS.Init.lpm_enable = DISABLE;
  hpcd_USB_OTG_FS.Init.battery_charging_enable = DISABLE;
  hpcd_USB_OTG_FS.Init.use_dedicated_ep1 = DISABLE;
  hpcd_USB_OTG_FS.Init.vbus_sensing_enable = DISABLE;
  hpcd_USB_OTG_FS.Init.dma_enable = DISABLE;
  if (HAL_PCD_Init(&hpcd_USB_OTG_FS) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USB_OTG_FS_Init 2 */
  if (HAL_PCDEx_SetRxFiFo(&hpcd_USB_OTG_FS, 0x80U) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_PCDEx_SetTxFiFo(&hpcd_USB_OTG_FS, 0U, 0x40U) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_PCDEx_SetTxFiFo(&hpcd_USB_OTG_FS, 1U, 0x80U) != HAL_OK)
  {
    Error_Handler();
  }

  /* USER CODE END USB_OTG_FS_Init 2 */

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
  HAL_GPIO_WritePin(PWR_DBG_GPIO_Port, PWR_DBG_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, NINA_NRST_Pin|SD_MODE_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pins : PC15 PC2 PC3 PC4
                           PC5 PC7 PC8 */
  GPIO_InitStruct.Pin = GPIO_PIN_15|GPIO_PIN_2|GPIO_PIN_3|GPIO_PIN_4
                          |GPIO_PIN_5|GPIO_PIN_7|GPIO_PIN_8;
  GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pin : PH0 */
  GPIO_InitStruct.Pin = GPIO_PIN_0;
  GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOH, &GPIO_InitStruct);

  /*Configure GPIO pin : PWR_DBG_Pin */
  GPIO_InitStruct.Pin = PWR_DBG_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(PWR_DBG_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : PA1 PA5 */
  GPIO_InitStruct.Pin = GPIO_PIN_1|GPIO_PIN_5;
  GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : BTN_START_Pin */
  GPIO_InitStruct.Pin = BTN_START_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING_FALLING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(BTN_START_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : PB2 PB3 PB4 */
  GPIO_InitStruct.Pin = GPIO_PIN_2|GPIO_PIN_3|GPIO_PIN_4;
  GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pins : MPU_INT_Pin PMIC_INT_Pin BTN_A_Pin BTN_B_Pin
                           BTN_L_Pin BTN_R_Pin */
  GPIO_InitStruct.Pin = MPU_INT_Pin|PMIC_INT_Pin|BTN_A_Pin|BTN_B_Pin
                          |BTN_L_Pin|BTN_R_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING_FALLING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pins : NINA_NRST_Pin SD_MODE_Pin */
  GPIO_InitStruct.Pin = NINA_NRST_Pin|SD_MODE_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pin : JOY_INT_Pin */
  GPIO_InitStruct.Pin = JOY_INT_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING_FALLING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(JOY_INT_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : PD2 */
  GPIO_InitStruct.Pin = GPIO_PIN_2;
  GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

  /*Configure GPIO pin : BTN_BOOT_Pin */
  GPIO_InitStruct.Pin = BTN_BOOT_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING_FALLING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(BTN_BOOT_GPIO_Port, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI3_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI3_IRQn);

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

  HAL_NVIC_SetPriority(EXTI11_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI11_IRQn);

  HAL_NVIC_SetPriority(EXTI14_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI14_IRQn);

  HAL_NVIC_SetPriority(EXTI15_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI15_IRQn);

/* USER CODE BEGIN MX_GPIO_Init_2 */
/* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

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
  const uint32_t failing_phase = g_ps_hw6_fw0_probe.phase;
  __disable_irq();
  if (g_ps_hw6_fw0_probe.error_code == PS_HW6_FW0_ERROR_NONE)
  {
    PS_HW6_FW0_RecordError(failing_phase, PS_HW6_FW0_ERROR_HANDLER);
  }
  PS_HW6_FW0_DebugBreak();
  while (1)
  {
    __NOP();
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
  const uint32_t failing_phase = g_ps_hw6_fw0_probe.phase;
  __disable_irq();
  g_ps_hw6_fw0_probe.assert_count++;
  g_ps_hw6_fw0_probe.assert_line = line;
  PS_HW6_FW0_CopyAssertFile(file);
  g_ps_hw6_fw0_probe.error_count++;
  g_ps_hw6_fw0_probe.error_phase = failing_phase;
  g_ps_hw6_fw0_probe.error_code = PS_HW6_FW0_ERROR_ASSERT;
  g_ps_hw6_fw0_probe.phase = PS_HW6_FW0_PHASE_ASSERT;
  g_ps_hw6_fw0_probe.complete = 0U;
  PS_HW6_FW0_DebugBreak();
  while (1)
  {
    __NOP();
  }
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
