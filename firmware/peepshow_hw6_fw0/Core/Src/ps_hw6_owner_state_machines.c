#include "ps_hw6_owner_state_machines.h"

#include <stddef.h>
#include <string.h>

#include "knobs_autogen.h"
#include "main.h"
#include "ps_hw6_clock_policy.h"
#include "ps_audio_events.h"
#include "ps_audio_state.h"
#include "ps_comm_events.h"
#include "ps_comm_state.h"
#include "ps_display_events.h"
#include "ps_display_state.h"
#include "ps_dev_at25sl128a.h"
#include "ps_dev_lis2dux12.h"
#include "ps_dev_tmag3001.h"
#include "ps_hw_i2c3.h"
#include "ps_hw6_owner_services.h"
#include "ps_hw6_rtos_probe.h"
#include "ps_hw6_trace.h"
#include "ps_hw6_usb_export.h"
#include "ps_input_buttons.h"
#include "ps_input_events.h"
#include "ps_input_joystick.h"
#include "ps_input_state.h"
#include "ps_ui_router.h"
#include "ps_power_events.h"
#include "ps_power_state.h"
#include "ps_sensor_events.h"
#include "ps_sensor_state.h"
#include "ps_storage_flash_block.h"
#include "ps_storage_events.h"
#include "ps_storage_filex_levelx.h"
#include "ps_storage_layout.h"
#include "ps_storage_msc_bridge.h"
#include "ps_storage_state.h"
#include "tx_api.h"

#define PS_HW6_SM_PHASE_INIT              (0x6800UL)
#define PS_HW6_SM_PHASE_RUNNING           (0x6810UL)
#define PS_HW6_SM_PHASE_COMPLETE          (0x68FFUL)
#define PS_HW6_SM_REQUIRED_OWNER_MASK     (0x7FUL)
#define PS_HW6_STOP2_ACTIVE_PREP_OWNER_MASK \
  (PS_HW6_SM_REQUIRED_OWNER_MASK & \
   ~(1UL << PS_HW6_RTOS_OWNER_STORAGE))
#define PS_HW6_POWER_QUIESCE_CYCLE_INDEX  (0xFFFFFFFFUL)

#define PS_HW6_SM_OSPI_TIMEOUT_MS         (100U)
#define PS_HW6_SM_ALL_STATE_MASK          \
  ((1UL << PS_HW6_OWNER_SM_COUNT) - 1UL)
#define PS_HW6_STOP2_ACTIVE_PREP_STATE_MASK \
  (PS_HW6_SM_ALL_STATE_MASK & \
   ~(1UL << PS_HW6_SM_STORAGE) & \
   ~(1UL << PS_HW6_SM_FLASH))

#define PS_HW6_TMAG_ADDRESS               (0x34U)
#define PS_HW6_TMAG_STOP2_INT_CONFIG1_TARGET (0x01U)
#define PS_HW6_IMU_ADDRESS                (0x18U)
#define PS_HW6_JOYSTICK_SWEEP_DURATION_TICKS (500UL)
#define PS_HW6_JOYSTICK_SWEEP_PERIOD_TICKS   (5UL)
#define PS_HW6_JOYSTICK_LIVE_DURATION_TICKS  (250UL)
#define PS_HW6_JOYSTICK_LIVE_PERIOD_TICKS    (2UL)
#define PS_HW6_JOYSTICK_XYZ_CAPTURE_CAPACITY \
  (KNOB_INPUT_JOYSTICK_XYZ_CAPTURE_SAMPLES)
#define PS_HW6_JOYSTICK_CAL_MIN_DEADZONE     (3500)
#define PS_HW6_JOYSTICK_CAL_DEADZONE_PAD     (1500)
#define PS_HW6_JOYSTICK_CAL_MAX_DEADZONE     (12000)

#define PS_HW6_FLASH_WAKE_SETTLE_TICKS     (1UL)
#define PS_HW6_FLASH_SCRATCH_ADDRESS       (0x00FFF000UL)
#define PS_HW6_FLASH_SCRATCH_BLOCK_INDEX   (PS_HW6_FLASH_SCRATCH_ADDRESS / \
                                            PS_DEV_AT25SL128A_SECTOR_SIZE)

#define PS_HW6_NINA_DSR_HOST_CONTROL_PIN   (GPIO_PIN_8)
#define PS_HW6_NINA_DSR_HOST_CONTROL_PORT  (GPIOC)
#define PS_HW6_NINA_RESET_TICKS            PS_HW6_SM_MsToTicks((uint32_t)KNOB_COMM_BLE_RESET_ASSERT_MS)
#define PS_HW6_NINA_BOOT_TICKS             PS_HW6_SM_MsToTicks((uint32_t)KNOB_COMM_BLE_BOOT_WAIT_MS)
#define PS_HW6_NINA_BOOT_DRAIN_TICKS       PS_HW6_SM_MsToTicks((uint32_t)KNOB_COMM_BLE_BOOT_DRAIN_MS)
#define PS_HW6_NINA_RX_WINDOW_TICKS        PS_HW6_SM_MsToTicks((uint32_t)KNOB_COMM_BLE_RX_WINDOW_MS)
#define PS_HW6_NINA_RX_QUIET_TICKS         PS_HW6_SM_MsToTicks((uint32_t)KNOB_COMM_BLE_RX_QUIET_MS)
#define PS_HW6_NINA_STOP_SETTLE_TICKS      PS_HW6_SM_MsToTicks((uint32_t)KNOB_COMM_BLE_STOP_SETTLE_MS)
#define PS_HW6_NINA_WAKE_SETTLE_TICKS      PS_HW6_SM_MsToTicks((uint32_t)KNOB_COMM_BLE_WAKE_SETTLE_MS)
#define PS_HW6_NINA_RX_BYTE_TIMEOUT_MS     ((uint32_t)KNOB_COMM_BLE_RX_BYTE_TIMEOUT_MS)
#define PS_HW6_NINA_TX_TIMEOUT_MS          ((uint32_t)KNOB_COMM_BLE_TX_TIMEOUT_MS)
#define PS_HW6_NINA_IDENTITY_COMMAND_INDEX  (7U)

#define PS_HW6_NINA_RX_BUFFER_SIZE         (KNOB_COMM_BLE_RX_BUFFER_BYTES)
#define PS_HW6_NINA_REQUIRED_COMMAND_MASK  (0x79UL)
#define PS_HW6_NINA_UNSUPPORTED_COMMAND_MASK (0x06UL)

#define PS_HW6_ARRAY_COUNT(array) \
  (sizeof(array) / sizeof((array)[0]))

#define PS_HW6_BATTERY_FUEL_VBAT_OK_MASK  (0x0CUL)
#define PS_HW6_CHARGER_STATUS_CHARGING     (1UL)
#define PS_HW6_CHARGER_STATUS_FULL         (2UL)

#define PS_HW6_STOP2_GPIO_POLICY_VERSION   (2UL)
#define PS_HW6_STOP2_GPIO_SNAPSHOT_BEFORE  (0UL)
#define PS_HW6_STOP2_GPIO_SNAPSHOT_SLEEP   (1UL)
#define PS_HW6_STOP2_GPIO_SNAPSHOT_AFTER   (2UL)
#define PS_HW6_STOP2_GPIO_PORT_A           (0U)
#define PS_HW6_STOP2_GPIO_PORT_B           (1U)
#define PS_HW6_STOP2_GPIO_PORT_C           (2U)
#define PS_HW6_STOP2_GPIO_PORT_H           (3U)
#define PS_HW6_STOP2_GPIO_GROUP_OSPI       (0U)
#define PS_HW6_STOP2_GPIO_GROUP_SAI        (1U)
#define PS_HW6_STOP2_GPIO_GROUP_USB        (2U)
#define PS_HW6_STOP2_GPIO_GROUP_DISPLAY    (3U)
#define PS_HW6_STOP2_GPIO_GROUP_I2C        (4U)
#define PS_HW6_STOP2_GPIO_GROUP_MASK_OSPI  (1UL << PS_HW6_STOP2_GPIO_GROUP_OSPI)
#define PS_HW6_STOP2_GPIO_GROUP_MASK_SAI   (1UL << PS_HW6_STOP2_GPIO_GROUP_SAI)
#define PS_HW6_STOP2_GPIO_GROUP_MASK_USB   (1UL << PS_HW6_STOP2_GPIO_GROUP_USB)
#define PS_HW6_STOP2_GPIO_GROUP_MASK_DISPLAY \
  (1UL << PS_HW6_STOP2_GPIO_GROUP_DISPLAY)
#define PS_HW6_STOP2_GPIO_GROUP_MASK_I2C   (1UL << PS_HW6_STOP2_GPIO_GROUP_I2C)
#define PS_HW6_STOP2_GPIO_GROUP_MASK_ALL   \
  (PS_HW6_STOP2_GPIO_GROUP_MASK_OSPI | \
   PS_HW6_STOP2_GPIO_GROUP_MASK_SAI | \
   PS_HW6_STOP2_GPIO_GROUP_MASK_USB | \
   PS_HW6_STOP2_GPIO_GROUP_MASK_DISPLAY | \
   PS_HW6_STOP2_GPIO_GROUP_MASK_I2C)
#define PS_HW6_STOP2_BUTTON_WAKE_EXTI_MASK ((uint32_t)(BTN_START_Pin | BTN_A_Pin | BTN_B_Pin | BTN_L_Pin | BTN_R_Pin))

extern I2C_HandleTypeDef hi2c3;
extern DMA_HandleTypeDef handle_GPDMA1_Channel4;
extern DMA_HandleTypeDef handle_GPDMA1_Channel5;
extern OSPI_HandleTypeDef hospi1;
extern PCD_HandleTypeDef hpcd_USB_OTG_FS;
extern UART_HandleTypeDef hlpuart1;

volatile PS_HW6_OwnerStateMachineProbe g_ps_hw6_owner_sm_probe;
volatile uint32_t g_ps_hw6_owner_sm_start_request;
volatile uint32_t g_ps_hw6_pmic_software_ship_request;
volatile uint32_t g_ps_hw6_power_sleep_prep_request;
volatile uint32_t g_ps_hw6_power_stop2_request;
volatile uint32_t g_ps_hw6_power_stop2_active_resume_request;
volatile uint32_t g_ps_hw6_power_stop2_active_prep_request;
volatile uint32_t g_ps_hw6_power_stop2_active_enter_request;
volatile uint32_t g_ps_hw6_power_stop2_pre_wfi_hold_enable;
volatile uint32_t g_ps_hw6_power_stop2_srdrun_test_enable;
volatile uint32_t g_ps_hw6_power_stop2_apb3_div1_test_enable;
volatile uint32_t g_ps_hw6_power_stop2_post_wfi_break_enable;
volatile uint32_t g_ps_hw6_power_stop2_spi_autotrigger_test_enable;
volatile uint32_t g_ps_hw6_storage_usb_export_request;
volatile uint32_t g_ps_hw6_storage_usb_reclaim_request;
volatile uint32_t g_ps_hw6_joystick_sample_request;
volatile uint32_t g_ps_hw6_joystick_live_request;
volatile uint32_t g_ps_hw6_joystick_cardinal_request;
volatile uint32_t g_ps_hw6_joystick_calibration_capture_request;
volatile uint32_t g_ps_hw6_joystick_calibration_capture_page;
volatile uint32_t g_ps_hw6_joystick_sleep_audit_request;
volatile uint32_t g_ps_hw6_joystick_xyz_capture_request;
volatile uint32_t g_ps_hw6_joystick_xyz_capture_mode;
volatile uint32_t g_ps_hw6_ble_sleep_dsr_deasserted = 1UL;
volatile uint32_t g_ps_hw6_stop2_gpio_park_group_mask_override =
  PS_HW6_OWNER_SM_STATUS_NOT_RUN;
volatile PS_HW6_JoystickXyzCaptureRecord
  g_ps_hw6_joystick_xyz_capture_buffer[KNOB_INPUT_JOYSTICK_XYZ_CAPTURE_SAMPLES];

static uint32_t ps_start_power_return_state;
static uint32_t ps_power_battery_monitor_period_ticks;
static uint32_t ps_power_battery_owns_ship_prep;
static uint32_t ps_power_boot_restart_gate_pending;
static uint32_t ps_power_boot_restart_gate_blocked;
static PS_HW6_PowerQuiesceBarrierCallback ps_power_quiesce_barrier_callback;
static PS_HW6_PowerAdmissionCallback ps_power_admission_callback;
static PS_HW6_PostStopResumeBarrierCallback ps_post_stop_resume_barrier_callback;

static GPIO_TypeDef * const
  ps_hw6_stop2_gpio_ports[PS_HW6_OWNER_SM_STOP2_GPIO_PORT_COUNT] =
{
  GPIOA,
  GPIOB,
  GPIOC,
  GPIOH
};

static const uint32_t
  ps_hw6_stop2_gpio_used_masks[PS_HW6_OWNER_SM_STOP2_GPIO_PORT_COUNT] =
{
  GPIO_PIN_0 | GPIO_PIN_2 | GPIO_PIN_3 | GPIO_PIN_4 |
    GPIO_PIN_6 | GPIO_PIN_7 | GPIO_PIN_8 | GPIO_PIN_9 |
    GPIO_PIN_10 | GPIO_PIN_11 | GPIO_PIN_12 | GPIO_PIN_13 |
    GPIO_PIN_14 | GPIO_PIN_15,
  GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_3 | GPIO_PIN_5 |
    GPIO_PIN_6 | GPIO_PIN_7 | GPIO_PIN_8 | GPIO_PIN_9 |
    GPIO_PIN_10 | GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14 |
    GPIO_PIN_15,
  GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_4 | GPIO_PIN_5 |
    GPIO_PIN_6 | GPIO_PIN_7 | GPIO_PIN_8 | GPIO_PIN_9 |
    GPIO_PIN_10 | GPIO_PIN_11 | GPIO_PIN_12 | GPIO_PIN_13 |
    GPIO_PIN_14,
  GPIO_PIN_1 | GPIO_PIN_3
};

static const uint32_t
  ps_hw6_stop2_gpio_wake_masks[PS_HW6_OWNER_SM_STOP2_GPIO_PORT_COUNT] =
{
  BTN_START_Pin,
  BTN_A_Pin | BTN_B_Pin | BTN_L_Pin | BTN_R_Pin |
    MPU_INT_Pin | PMIC_INT_Pin,
  JOY_INT_Pin,
  BTN_BOOT_Pin
};

static const uint32_t
  ps_hw6_stop2_gpio_retain_masks[PS_HW6_OWNER_SM_STOP2_GPIO_PORT_COUNT] =
{
  GPIO_PIN_13 | GPIO_PIN_14,
  GPIO_PIN_3,
  NINA_NRST_Pin | PS_HW6_NINA_DSR_HOST_CONTROL_PIN |
    SD_MODE_Pin | LCD_1HZ_Pin | GPIO_PIN_14,
  PWR_DBG_Pin
};

static const uint32_t
  ps_hw6_stop2_gpio_group_masks[PS_HW6_OWNER_SM_STOP2_GPIO_GROUP_COUNT]
                                [PS_HW6_OWNER_SM_STOP2_GPIO_PORT_COUNT] =
{
  {
    GPIO_PIN_0 | GPIO_PIN_6 | GPIO_PIN_7,
    GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_10,
    0UL,
    0UL
  },
  {
    GPIO_PIN_8 | GPIO_PIN_10,
    GPIO_PIN_9,
    0UL,
    0UL
  },
  {
    GPIO_PIN_9 | GPIO_PIN_11 | GPIO_PIN_12,
    0UL,
    0UL,
    0UL
  },
  {
    GPIO_PIN_15,
    0UL,
    GPIO_PIN_10 | GPIO_PIN_12,
    0UL
  },
  {
    0UL,
    0UL,
    GPIO_PIN_0 | GPIO_PIN_1,
    0UL
  }
};

static HAL_StatusTypeDef PS_HW6_SM_Transition(uint32_t state_machine_id,
                                               uint32_t event,
                                               HAL_StatusTypeDef action_status);

static uint32_t PS_HW6_SM_MsToTicks(uint32_t ms)
{
  uint64_t scaled;

  scaled = (((uint64_t)ms * (uint64_t)TX_TIMER_TICKS_PER_SECOND) +
            999ULL) / 1000ULL;
  if (scaled == 0ULL)
  {
    return 1UL;
  }
  if (scaled > 0xFFFFFFFFULL)
  {
    return 0xFFFFFFFFUL;
  }
  return (uint32_t)scaled;
}

static uint32_t PS_HW6_SM_GpioTwoBitMask(uint32_t pin_mask)
{
  uint32_t two_bit_mask = 0UL;
  uint32_t pin;

  for (pin = 0U; pin < 16U; ++pin)
  {
    if ((pin_mask & (1UL << pin)) != 0UL)
    {
      two_bit_mask |= (3UL << (pin * 2U));
    }
  }

  return two_bit_mask;
}

static uint32_t PS_HW6_SM_Stop2GpioDefaultParkGroupMask(void)
{
  return ((uint32_t)KNOB_POWER_STOP2_GPIO_PARK_GROUP_MASK) &
         PS_HW6_STOP2_GPIO_GROUP_MASK_ALL;
}

static uint32_t PS_HW6_SM_Stop2GpioActiveParkGroupMask(void)
{
  const uint32_t default_mask = PS_HW6_SM_Stop2GpioDefaultParkGroupMask();
  uint32_t active_mask = default_mask;
  const uint32_t override_mask =
    g_ps_hw6_stop2_gpio_park_group_mask_override;

  if (override_mask != PS_HW6_OWNER_SM_STATUS_NOT_RUN)
  {
    active_mask = override_mask & PS_HW6_STOP2_GPIO_GROUP_MASK_ALL;
  }

  if ((g_ps_hw6_owner_probe.display_lpbam_active != 0UL) ||
      ((g_ps_hw6_owner_probe.display_lpbam_ready != 0UL) &&
       (g_ps_hw6_owner_probe.display_lpbam_status == (uint32_t)HAL_OK)))
  {
    active_mask &= ~PS_HW6_STOP2_GPIO_GROUP_MASK_DISPLAY;
  }
  g_ps_hw6_owner_sm_probe.stop2_gpio_park_group_default_mask =
    default_mask;
  g_ps_hw6_owner_sm_probe.stop2_gpio_park_group_override_mask =
    override_mask;
  g_ps_hw6_owner_sm_probe.stop2_gpio_park_group_active_mask =
    active_mask;

  return active_mask;
}

static void PS_HW6_SM_RecalculateStop2GpioParkMasks(void)
{
  const uint32_t active_mask = PS_HW6_SM_Stop2GpioActiveParkGroupMask();
  uint32_t group;
  uint32_t port;

  for (port = 0U;
       port < PS_HW6_OWNER_SM_STOP2_GPIO_PORT_COUNT;
       ++port)
  {
    g_ps_hw6_owner_sm_probe.stop2_gpio_park_mask[port] = 0UL;
  }

  for (group = 0U;
       group < PS_HW6_OWNER_SM_STOP2_GPIO_GROUP_COUNT;
       ++group)
  {
    if ((active_mask & (1UL << group)) != 0UL)
    {
      for (port = 0U;
           port < PS_HW6_OWNER_SM_STOP2_GPIO_PORT_COUNT;
           ++port)
      {
        g_ps_hw6_owner_sm_probe.stop2_gpio_park_mask[port] |=
          ps_hw6_stop2_gpio_group_masks[group][port];
      }
    }
  }

  for (port = 0U;
       port < PS_HW6_OWNER_SM_STOP2_GPIO_PORT_COUNT;
       ++port)
  {
    g_ps_hw6_owner_sm_probe.stop2_gpio_park_mask[port] &=
      g_ps_hw6_owner_sm_probe.stop2_gpio_park_candidate_mask[port];
  }
}

static void PS_HW6_SM_ResetStop2GpioAudit(void)
{
  uint32_t index;

  g_ps_hw6_owner_sm_probe.stop2_gpio_policy_version =
    PS_HW6_STOP2_GPIO_POLICY_VERSION;
  g_ps_hw6_owner_sm_probe.stop2_gpio_snapshot_count = 0UL;
  g_ps_hw6_owner_sm_probe.stop2_gpio_park_count = 0UL;
  g_ps_hw6_owner_sm_probe.stop2_gpio_restore_count = 0UL;
  g_ps_hw6_owner_sm_probe.stop2_gpio_park_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.stop2_gpio_restore_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;

  for (index = 0U;
       index < PS_HW6_OWNER_SM_STOP2_GPIO_PORT_COUNT;
       ++index)
  {
    const uint32_t used_mask = ps_hw6_stop2_gpio_used_masks[index];
    const uint32_t wake_mask = ps_hw6_stop2_gpio_wake_masks[index];
    const uint32_t retain_mask = ps_hw6_stop2_gpio_retain_masks[index];

    g_ps_hw6_owner_sm_probe.stop2_gpio_used_mask[index] = used_mask;
    g_ps_hw6_owner_sm_probe.stop2_gpio_wake_mask[index] = wake_mask;
    g_ps_hw6_owner_sm_probe.stop2_gpio_retain_mask[index] = retain_mask;
    g_ps_hw6_owner_sm_probe.stop2_gpio_park_candidate_mask[index] =
      used_mask & ~(wake_mask | retain_mask);
    g_ps_hw6_owner_sm_probe.stop2_gpio_park_mask[index] = 0UL;
    g_ps_hw6_owner_sm_probe.stop2_gpio_moder_before[index] =
      PS_HW6_OWNER_SM_STATUS_NOT_RUN;
    g_ps_hw6_owner_sm_probe.stop2_gpio_pupdr_before[index] =
      PS_HW6_OWNER_SM_STATUS_NOT_RUN;
    g_ps_hw6_owner_sm_probe.stop2_gpio_odr_before[index] =
      PS_HW6_OWNER_SM_STATUS_NOT_RUN;
    g_ps_hw6_owner_sm_probe.stop2_gpio_idr_before[index] =
      PS_HW6_OWNER_SM_STATUS_NOT_RUN;
    g_ps_hw6_owner_sm_probe.stop2_gpio_moder_sleep[index] =
      PS_HW6_OWNER_SM_STATUS_NOT_RUN;
    g_ps_hw6_owner_sm_probe.stop2_gpio_pupdr_sleep[index] =
      PS_HW6_OWNER_SM_STATUS_NOT_RUN;
    g_ps_hw6_owner_sm_probe.stop2_gpio_odr_sleep[index] =
      PS_HW6_OWNER_SM_STATUS_NOT_RUN;
    g_ps_hw6_owner_sm_probe.stop2_gpio_idr_sleep[index] =
      PS_HW6_OWNER_SM_STATUS_NOT_RUN;
    g_ps_hw6_owner_sm_probe.stop2_gpio_moder_after[index] =
      PS_HW6_OWNER_SM_STATUS_NOT_RUN;
    g_ps_hw6_owner_sm_probe.stop2_gpio_pupdr_after[index] =
      PS_HW6_OWNER_SM_STATUS_NOT_RUN;
    g_ps_hw6_owner_sm_probe.stop2_gpio_odr_after[index] =
      PS_HW6_OWNER_SM_STATUS_NOT_RUN;
    g_ps_hw6_owner_sm_probe.stop2_gpio_idr_after[index] =
      PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  }

  PS_HW6_SM_RecalculateStop2GpioParkMasks();
}

static void PS_HW6_SM_RecordStop2GpioSnapshot(uint32_t snapshot)
{
  uint32_t index;

  for (index = 0U;
       index < PS_HW6_OWNER_SM_STOP2_GPIO_PORT_COUNT;
       ++index)
  {
    GPIO_TypeDef * const port = ps_hw6_stop2_gpio_ports[index];

    if (snapshot == PS_HW6_STOP2_GPIO_SNAPSHOT_BEFORE)
    {
      g_ps_hw6_owner_sm_probe.stop2_gpio_moder_before[index] = port->MODER;
      g_ps_hw6_owner_sm_probe.stop2_gpio_pupdr_before[index] = port->PUPDR;
      g_ps_hw6_owner_sm_probe.stop2_gpio_odr_before[index] = port->ODR;
      g_ps_hw6_owner_sm_probe.stop2_gpio_idr_before[index] = port->IDR;
    }
    else if (snapshot == PS_HW6_STOP2_GPIO_SNAPSHOT_SLEEP)
    {
      g_ps_hw6_owner_sm_probe.stop2_gpio_moder_sleep[index] = port->MODER;
      g_ps_hw6_owner_sm_probe.stop2_gpio_pupdr_sleep[index] = port->PUPDR;
      g_ps_hw6_owner_sm_probe.stop2_gpio_odr_sleep[index] = port->ODR;
      g_ps_hw6_owner_sm_probe.stop2_gpio_idr_sleep[index] = port->IDR;
    }
    else
    {
      g_ps_hw6_owner_sm_probe.stop2_gpio_moder_after[index] = port->MODER;
      g_ps_hw6_owner_sm_probe.stop2_gpio_pupdr_after[index] = port->PUPDR;
      g_ps_hw6_owner_sm_probe.stop2_gpio_odr_after[index] = port->ODR;
      g_ps_hw6_owner_sm_probe.stop2_gpio_idr_after[index] = port->IDR;
    }
  }

  g_ps_hw6_owner_sm_probe.stop2_gpio_snapshot_count++;
}

static HAL_StatusTypeDef PS_HW6_SM_ParkStop2GpioPins(void)
{
  uint32_t index;

  PS_HW6_SM_RecalculateStop2GpioParkMasks();
  g_ps_hw6_owner_sm_probe.stop2_gpio_park_count++;

  for (index = 0U;
       index < PS_HW6_OWNER_SM_STOP2_GPIO_PORT_COUNT;
       ++index)
  {
    GPIO_TypeDef * const port = ps_hw6_stop2_gpio_ports[index];
    const uint32_t pin_mask =
      g_ps_hw6_owner_sm_probe.stop2_gpio_park_mask[index];

    if (pin_mask != 0UL)
    {
      const uint32_t two_bit_mask = PS_HW6_SM_GpioTwoBitMask(pin_mask);

      port->PUPDR &= ~two_bit_mask;
      port->MODER = (port->MODER & ~two_bit_mask) | two_bit_mask;
    }
  }

  __DSB();
  g_ps_hw6_owner_sm_probe.stop2_gpio_park_status = (uint32_t)HAL_OK;
  return HAL_OK;
}

static void PS_HW6_SM_ClearStop2ButtonWakePending(void)
{
  __HAL_GPIO_EXTI_CLEAR_IT(PS_HW6_STOP2_BUTTON_WAKE_EXTI_MASK);
  HAL_NVIC_ClearPendingIRQ(BTN_START_EXTI_IRQn);
  HAL_NVIC_ClearPendingIRQ(BTN_A_EXTI_IRQn);
  HAL_NVIC_ClearPendingIRQ(BTN_B_EXTI_IRQn);
  HAL_NVIC_ClearPendingIRQ(BTN_L_EXTI_IRQn);
  HAL_NVIC_ClearPendingIRQ(BTN_R_EXTI_IRQn);
}

static HAL_StatusTypeDef PS_HW6_SM_RestoreStop2GpioPins(void)
{
  uint32_t index;

  g_ps_hw6_owner_sm_probe.stop2_gpio_restore_count++;

  for (index = 0U;
       index < PS_HW6_OWNER_SM_STOP2_GPIO_PORT_COUNT;
       ++index)
  {
    GPIO_TypeDef * const port = ps_hw6_stop2_gpio_ports[index];
    const uint32_t pin_mask =
      g_ps_hw6_owner_sm_probe.stop2_gpio_park_mask[index];

    if (pin_mask != 0UL)
    {
      const uint32_t two_bit_mask = PS_HW6_SM_GpioTwoBitMask(pin_mask);
      const uint32_t odr_before =
        g_ps_hw6_owner_sm_probe.stop2_gpio_odr_before[index];
      const uint32_t pupdr_before =
        g_ps_hw6_owner_sm_probe.stop2_gpio_pupdr_before[index];
      const uint32_t moder_before =
        g_ps_hw6_owner_sm_probe.stop2_gpio_moder_before[index];

      port->ODR = (port->ODR & ~pin_mask) | (odr_before & pin_mask);
      port->PUPDR = (port->PUPDR & ~two_bit_mask) |
                    (pupdr_before & two_bit_mask);
      port->MODER = (port->MODER & ~two_bit_mask) |
                    (moder_before & two_bit_mask);
    }
  }

  __DSB();
  g_ps_hw6_owner_sm_probe.stop2_gpio_restore_status = (uint32_t)HAL_OK;
  return HAL_OK;
}

static uint32_t PS_HW6_SM_SuspendThreadXSystick(void)
{
  const uint32_t ctrl_before = SysTick->CTRL;

  g_ps_hw6_owner_sm_probe.stop2_systick_ctrl_before = ctrl_before;
  g_ps_hw6_owner_sm_probe.stop2_systick_icsr_before = SCB->ICSR;
  SCB->ICSR = SCB_ICSR_PENDSTCLR_Msk;
  SysTick->CTRL = ctrl_before &
                  ~(SysTick_CTRL_ENABLE_Msk | SysTick_CTRL_TICKINT_Msk);
  g_ps_hw6_owner_sm_probe.stop2_systick_ctrl_sleep = SysTick->CTRL;
  g_ps_hw6_owner_sm_probe.stop2_systick_icsr_sleep = SCB->ICSR;
  __DSB();
  __ISB();
  return ctrl_before;
}

static void PS_HW6_SM_RecordStop2PreWfiState(void)
{
  g_ps_hw6_owner_sm_probe.stop2_pre_wfi_hold_count++;
  g_ps_hw6_owner_sm_probe.stop2_pre_wfi_hold_tick =
    (uint32_t)tx_time_get();
  g_ps_hw6_owner_sm_probe.stop2_pre_wfi_rcc_srdamr = RCC->SRDAMR;
  g_ps_hw6_owner_sm_probe.stop2_pre_wfi_spi_cr1 = SPI3->CR1;
  g_ps_hw6_owner_sm_probe.stop2_pre_wfi_spi_cfg1 = SPI3->CFG1;
  g_ps_hw6_owner_sm_probe.stop2_pre_wfi_spi_cfg2 = SPI3->CFG2;
  g_ps_hw6_owner_sm_probe.stop2_pre_wfi_spi_autocr = SPI3->AUTOCR;
  g_ps_hw6_owner_sm_probe.stop2_pre_wfi_spi_sr = SPI3->SR;
  g_ps_hw6_owner_sm_probe.stop2_pre_wfi_lptim_cr = LPTIM1->CR;
  g_ps_hw6_owner_sm_probe.stop2_pre_wfi_lptim_cfgr = LPTIM1->CFGR;
  g_ps_hw6_owner_sm_probe.stop2_pre_wfi_lptim_ccmr1 = LPTIM1->CCMR1;
  g_ps_hw6_owner_sm_probe.stop2_pre_wfi_lptim_arr = LPTIM1->ARR;
  g_ps_hw6_owner_sm_probe.stop2_pre_wfi_lptim_cmp = LPTIM1->CCR1;
}

static void PS_HW6_SM_RecordStop2PostWfiState(void)
{
  g_ps_hw6_owner_sm_probe.stop2_post_wfi_snapshot_count++;
  g_ps_hw6_owner_sm_probe.stop2_post_wfi_snapshot_tick =
    (uint32_t)tx_time_get();
  g_ps_hw6_owner_sm_probe.stop2_post_wfi_rcc_srdamr = RCC->SRDAMR;
  g_ps_hw6_owner_sm_probe.stop2_post_wfi_rcc_cfgr3 = RCC->CFGR3;
  g_ps_hw6_owner_sm_probe.stop2_post_wfi_pwr_cr2 = PWR->CR2;
  g_ps_hw6_owner_sm_probe.stop2_post_wfi_scb_icsr = SCB->ICSR;
  g_ps_hw6_owner_sm_probe.stop2_post_wfi_nvic_ispr0 = NVIC->ISPR[0];
  g_ps_hw6_owner_sm_probe.stop2_post_wfi_nvic_ispr1 = NVIC->ISPR[1];
  g_ps_hw6_owner_sm_probe.stop2_post_wfi_nvic_iabr0 = NVIC->IABR[0];
  g_ps_hw6_owner_sm_probe.stop2_post_wfi_nvic_iabr1 = NVIC->IABR[1];
  g_ps_hw6_owner_sm_probe.stop2_post_wfi_lpdma_misr = LPDMA1->MISR;
  g_ps_hw6_owner_sm_probe.stop2_post_wfi_dma_clbar = LPDMA1_Channel0->CLBAR;
  g_ps_hw6_owner_sm_probe.stop2_post_wfi_dma_csr = LPDMA1_Channel0->CSR;
  g_ps_hw6_owner_sm_probe.stop2_post_wfi_dma_ccr = LPDMA1_Channel0->CCR;
  g_ps_hw6_owner_sm_probe.stop2_post_wfi_dma_ctr1 = LPDMA1_Channel0->CTR1;
  g_ps_hw6_owner_sm_probe.stop2_post_wfi_dma_ctr2 = LPDMA1_Channel0->CTR2;
  g_ps_hw6_owner_sm_probe.stop2_post_wfi_dma_cbr1 = LPDMA1_Channel0->CBR1;
  g_ps_hw6_owner_sm_probe.stop2_post_wfi_dma_csar = LPDMA1_Channel0->CSAR;
  g_ps_hw6_owner_sm_probe.stop2_post_wfi_dma_cdar = LPDMA1_Channel0->CDAR;
  g_ps_hw6_owner_sm_probe.stop2_post_wfi_dma_cllr = LPDMA1_Channel0->CLLR;
  g_ps_hw6_owner_sm_probe.stop2_post_wfi_spi_cr1 = SPI3->CR1;
  g_ps_hw6_owner_sm_probe.stop2_post_wfi_spi_cr2 = SPI3->CR2;
  g_ps_hw6_owner_sm_probe.stop2_post_wfi_spi_cfg1 = SPI3->CFG1;
  g_ps_hw6_owner_sm_probe.stop2_post_wfi_spi_cfg2 = SPI3->CFG2;
  g_ps_hw6_owner_sm_probe.stop2_post_wfi_spi_ier = SPI3->IER;
  g_ps_hw6_owner_sm_probe.stop2_post_wfi_spi_sr = SPI3->SR;
  g_ps_hw6_owner_sm_probe.stop2_post_wfi_spi_autocr = SPI3->AUTOCR;
  g_ps_hw6_owner_sm_probe.stop2_post_wfi_lptim_isr = LPTIM1->ISR;
  g_ps_hw6_owner_sm_probe.stop2_post_wfi_lptim_dier = LPTIM1->DIER;
  g_ps_hw6_owner_sm_probe.stop2_post_wfi_lptim_cfgr = LPTIM1->CFGR;
  g_ps_hw6_owner_sm_probe.stop2_post_wfi_lptim_cr = LPTIM1->CR;
  g_ps_hw6_owner_sm_probe.stop2_post_wfi_lptim_ccmr1 = LPTIM1->CCMR1;
  g_ps_hw6_owner_sm_probe.stop2_post_wfi_lptim_arr = LPTIM1->ARR;
  g_ps_hw6_owner_sm_probe.stop2_post_wfi_lptim_cmp = LPTIM1->CCR1;
  g_ps_hw6_owner_sm_probe.stop2_post_wfi_lptim_cnt = LPTIM1->CNT;
}

static void PS_HW6_SM_RestoreThreadXSystick(uint32_t ctrl_before)
{
  SCB->ICSR = SCB_ICSR_PENDSTCLR_Msk;
  SysTick->CTRL = ctrl_before;
  g_ps_hw6_owner_sm_probe.stop2_systick_ctrl_after = SysTick->CTRL;
  g_ps_hw6_owner_sm_probe.stop2_systick_icsr_after = SCB->ICSR;
}

static HAL_StatusTypeDef PS_HW6_RequestPowerQuiesce(uint32_t reason)
{
  if (ps_power_quiesce_barrier_callback == NULL)
  {
    return HAL_ERROR;
  }
  return ps_power_quiesce_barrier_callback(reason);
}
static HAL_StatusTypeDef PS_HW6_RequestPowerAdmission(uint32_t reason)
{
  if (ps_power_admission_callback == NULL)
  {
    return HAL_ERROR;
  }
  return ps_power_admission_callback(reason);
}

static HAL_StatusTypeDef PS_HW6_RequestPostStopResume(void)
{
  if (ps_post_stop_resume_barrier_callback == NULL)
  {
    return HAL_ERROR;
  }
  return ps_post_stop_resume_barrier_callback();
}

static HAL_StatusTypeDef PS_HW6_BatteryPolicyPrepareForShipment(
  uint32_t reason)
{
  HAL_StatusTypeDef status;

  status = PS_HW6_RequestPowerAdmission(reason);
  if (status != HAL_OK)
  {
    return status;
  }

  g_ps_hw6_owner_sm_probe.battery_policy_quiesce_request_count++;
  g_ps_hw6_owner_sm_probe.battery_policy_quiesce_last_tick =
    (uint32_t)tx_time_get();
  status = PS_HW6_RequestPowerQuiesce(reason);
  g_ps_hw6_owner_sm_probe.battery_policy_quiesce_last_status =
    (uint32_t)status;
  return status;
}

static void PS_HW6_BatteryPolicyRequestSoftwareShipment(uint32_t boot_check)
{
  uint32_t enabled = (boot_check != 0UL) ?
    (uint32_t)KNOB_POWER_BOOT_LOW_BATTERY_SHIP_ENABLE :
    (uint32_t)KNOB_POWER_CRITICAL_SOFTWARE_SHIP_ENABLE;

  g_ps_hw6_owner_sm_probe.battery_policy_critical_ship_enabled =
    (uint32_t)KNOB_POWER_CRITICAL_SOFTWARE_SHIP_ENABLE;
  g_ps_hw6_owner_sm_probe.battery_policy_boot_ship_enabled =
    (uint32_t)KNOB_POWER_BOOT_LOW_BATTERY_SHIP_ENABLE;

  if (enabled != 0UL)
  {
    g_ps_hw6_owner_sm_probe.battery_policy_software_ship_request_count++;
    g_ps_hw6_owner_sm_probe.battery_policy_software_ship_last_tick =
      (uint32_t)tx_time_get();
    g_ps_hw6_owner_sm_probe.battery_policy_software_ship_last_status =
      (uint32_t)HAL_OK;
    g_ps_hw6_owner_sm_probe.battery_policy_state =
      PS_HW6_POWER_BATTERY_POLICY_SHIP_REQUESTED;
    g_ps_hw6_owner_sm_probe.battery_policy_last_event =
      PS_HW6_POWER_BATTERY_EVENT_SHIP_REQUEST;
    g_ps_hw6_pmic_software_ship_request = 1UL;
  }
  else
  {
    g_ps_hw6_owner_sm_probe.battery_policy_software_ship_skipped_count++;
    g_ps_hw6_owner_sm_probe.battery_policy_software_ship_last_status =
      PS_HW6_OWNER_SM_STATUS_NOT_RUN;
    g_ps_hw6_owner_sm_probe.battery_policy_last_event =
      PS_HW6_POWER_BATTERY_EVENT_SHIP_SKIPPED;
  }
}

static HAL_StatusTypeDef PS_HW6_SM_EvaluateBatteryPolicy(
  HAL_StatusTypeDef snapshot_status,
  uint32_t boot_check)
{
  uint32_t fuel_ok;
  uint32_t vbat_mv;
  uint32_t vbus_ok;
  uint32_t boot_gate_active;
  uint32_t now_tick = (uint32_t)tx_time_get();

  g_ps_hw6_owner_sm_probe.battery_policy_warning_mv =
    (uint32_t)KNOB_POWER_BATTERY_WARNING_MV;
  g_ps_hw6_owner_sm_probe.battery_policy_critical_mv =
    (uint32_t)KNOB_POWER_BATTERY_CRITICAL_SHIP_MV;
  g_ps_hw6_owner_sm_probe.battery_policy_restart_allow_mv =
    (uint32_t)KNOB_POWER_BATTERY_RESTART_ALLOW_MV;
  g_ps_hw6_owner_sm_probe.battery_policy_period_ticks =
    ps_power_battery_monitor_period_ticks;
  g_ps_hw6_owner_sm_probe.battery_policy_critical_ship_enabled =
    (uint32_t)KNOB_POWER_CRITICAL_SOFTWARE_SHIP_ENABLE;
  g_ps_hw6_owner_sm_probe.battery_policy_boot_ship_enabled =
    (uint32_t)KNOB_POWER_BOOT_LOW_BATTERY_SHIP_ENABLE;
  g_ps_hw6_owner_sm_probe.battery_policy_boot_restart_gate_pending =
    ps_power_boot_restart_gate_pending;
  g_ps_hw6_owner_sm_probe.battery_policy_boot_restart_gate_blocked =
    ps_power_boot_restart_gate_blocked;
  g_ps_hw6_owner_sm_probe.battery_policy_last_snapshot_status =
    (uint32_t)snapshot_status;
  g_ps_hw6_owner_sm_probe.battery_policy_last_tick = now_tick;
  g_ps_hw6_owner_sm_probe.battery_policy_next_tick =
    now_tick + ps_power_battery_monitor_period_ticks;

  if (boot_check != 0UL)
  {
    g_ps_hw6_owner_sm_probe.battery_policy_boot_check_count++;
    g_ps_hw6_owner_sm_probe.battery_policy_last_event =
      PS_HW6_POWER_BATTERY_EVENT_BOOT_CHECK;
  }
  else
  {
    g_ps_hw6_owner_sm_probe.battery_policy_monitor_count++;
    g_ps_hw6_owner_sm_probe.battery_policy_last_event =
      PS_HW6_POWER_BATTERY_EVENT_MONITOR_CHECK;
  }

  fuel_ok = ((g_ps_hw6_owner_probe.power_fuel_read_ok_mask &
              PS_HW6_BATTERY_FUEL_VBAT_OK_MASK) ==
             PS_HW6_BATTERY_FUEL_VBAT_OK_MASK) ? 1UL : 0UL;
  vbat_mv = g_ps_hw6_owner_probe.power_fuel_vbat_mv;
  if ((fuel_ok != 0UL) && (vbat_mv == 0UL))
  {
    fuel_ok = 0UL;
  }
  vbus_ok = g_ps_hw6_owner_probe.power_vbus_ok;

  g_ps_hw6_owner_sm_probe.battery_policy_fuel_ok = fuel_ok;
  g_ps_hw6_owner_sm_probe.battery_policy_vbat_mv = vbat_mv;
  g_ps_hw6_owner_sm_probe.battery_policy_vbus_ok = vbus_ok;
  g_ps_hw6_owner_sm_probe.battery_policy_battery_present =
    g_ps_hw6_owner_probe.power_battery_present;

  if ((snapshot_status != HAL_OK) || (fuel_ok == 0UL))
  {
    g_ps_hw6_owner_sm_probe.battery_policy_state =
      PS_HW6_POWER_BATTERY_POLICY_UNKNOWN;
    g_ps_hw6_owner_sm_probe.battery_policy_last_event =
      PS_HW6_POWER_BATTERY_EVENT_SNAPSHOT_FAIL;
    return HAL_ERROR;
  }

  boot_gate_active = ((boot_check != 0UL) ||
                      (ps_power_boot_restart_gate_pending != 0UL)) ?
    1UL : 0UL;
  if (boot_gate_active != 0UL)
  {
    if (vbat_mv >= (uint32_t)KNOB_POWER_BATTERY_RESTART_ALLOW_MV)
    {
      if (ps_power_boot_restart_gate_pending != 0UL)
      {
        g_ps_hw6_owner_sm_probe.battery_policy_boot_restart_gate_clear_count++;
      }
      ps_power_boot_restart_gate_pending = 0UL;
      ps_power_boot_restart_gate_blocked = 0UL;
      g_ps_hw6_owner_sm_probe.battery_policy_boot_restart_gate_pending = 0UL;
      g_ps_hw6_owner_sm_probe.battery_policy_boot_restart_gate_blocked = 0UL;
    }
    else if (vbus_ok != 0UL)
    {
      g_ps_hw6_owner_sm_probe.battery_policy_state =
        PS_HW6_POWER_BATTERY_POLICY_BOOT_CHARGE_RECOVERY;
      g_ps_hw6_owner_sm_probe.battery_policy_last_event =
        PS_HW6_POWER_BATTERY_EVENT_BOOT_CHARGE_RECOVERY;
      if (g_ps_hw6_owner_sm_probe.battery_policy_boot_charge_recovery_count ==
          0UL)
      {
        g_ps_hw6_owner_sm_probe.battery_policy_boot_charge_recovery_count++;
      }
      if ((ps_power_battery_owns_ship_prep != 0UL) &&
          (g_ps_hw6_owner_sm_probe.current_state[PS_HW6_SM_POWER] ==
           PWR_SHIP_PREP))
      {
        (void)PS_HW6_SM_Transition(PS_HW6_SM_POWER,
                                   PWR_EV_LP_REQUEST,
                                   HAL_OK);
        ps_power_battery_owns_ship_prep = 0UL;
      }
      ps_power_boot_restart_gate_blocked = 1UL;
      g_ps_hw6_owner_sm_probe.battery_policy_boot_restart_gate_pending =
        ps_power_boot_restart_gate_pending;
      g_ps_hw6_owner_sm_probe.battery_policy_boot_restart_gate_blocked =
        ps_power_boot_restart_gate_blocked;
      if ((g_ps_hw6_owner_probe.power_charge_complete != 0UL) ||
          (g_ps_hw6_owner_probe.power_charger_status ==
           PS_HW6_CHARGER_STATUS_FULL))
      {
        (void)PS_HW6_SM_Transition(PS_HW6_SM_PMIC,
                                   PMIC_EV_CHARGE_DONE,
                                   HAL_OK);
      }
      else if (g_ps_hw6_owner_probe.power_charger_status ==
               PS_HW6_CHARGER_STATUS_CHARGING)
      {
        (void)PS_HW6_SM_Transition(PS_HW6_SM_PMIC,
                                   PMIC_EV_CHARGING,
                                   HAL_OK);
      }
      else
      {
        (void)PS_HW6_SM_Transition(PS_HW6_SM_PMIC,
                                   PMIC_EV_LOW_BATTERY,
                                   HAL_OK);
      }
      return HAL_OK;
    }
    else
    {
      g_ps_hw6_owner_sm_probe.battery_policy_state =
        PS_HW6_POWER_BATTERY_POLICY_BOOT_RESTART_BLOCKED;
      g_ps_hw6_owner_sm_probe.battery_policy_last_event =
        PS_HW6_POWER_BATTERY_EVENT_BOOT_RESTART_BLOCK;
      if (g_ps_hw6_owner_sm_probe.battery_policy_boot_restart_block_count ==
          0UL)
      {
        g_ps_hw6_owner_sm_probe.battery_policy_boot_restart_block_count++;
        (void)PS_HW6_SM_Transition(PS_HW6_SM_POWER,
                                   PWR_EV_SHIP_REQUEST,
                                   HAL_OK);
        ps_power_battery_owns_ship_prep = 1UL;
        (void)PS_HW6_SM_Transition(PS_HW6_SM_PMIC,
                                   PMIC_EV_SHIP_REQUEST,
                                   HAL_OK);
        if (PS_HW6_BatteryPolicyPrepareForShipment(
              (uint32_t)PS_HW6_POWER_QUIESCE_REASON_BOOT_LOW_BATTERY) ==
            HAL_OK)
        {
          PS_HW6_BatteryPolicyRequestSoftwareShipment(1UL);
        }
      }
      ps_power_boot_restart_gate_blocked = 1UL;
      g_ps_hw6_owner_sm_probe.battery_policy_boot_restart_gate_pending =
        ps_power_boot_restart_gate_pending;
      g_ps_hw6_owner_sm_probe.battery_policy_boot_restart_gate_blocked =
        ps_power_boot_restart_gate_blocked;
      return HAL_OK;
    }
  }

  if (vbat_mv <= (uint32_t)KNOB_POWER_BATTERY_CRITICAL_SHIP_MV)
  {
    g_ps_hw6_owner_sm_probe.battery_policy_state =
      PS_HW6_POWER_BATTERY_POLICY_CRITICAL;
    g_ps_hw6_owner_sm_probe.battery_policy_last_event =
      PS_HW6_POWER_BATTERY_EVENT_CRITICAL;
    g_ps_hw6_owner_sm_probe.battery_policy_critical_count++;
    if (ps_power_battery_owns_ship_prep == 0UL)
    {
      (void)PS_HW6_SM_Transition(PS_HW6_SM_POWER,
                                 PWR_EV_SHIP_REQUEST,
                                 HAL_OK);
      ps_power_battery_owns_ship_prep = 1UL;
      (void)PS_HW6_SM_Transition(PS_HW6_SM_PMIC,
                                 PMIC_EV_CRITICAL_BATTERY,
                                 HAL_OK);
      if (PS_HW6_BatteryPolicyPrepareForShipment(
            (uint32_t)PS_HW6_POWER_QUIESCE_REASON_BATTERY_CRITICAL) ==
          HAL_OK)
      {
        (void)PS_HW6_SM_Transition(PS_HW6_SM_PMIC,
                                   PMIC_EV_SHIP_REQUEST,
                                   HAL_OK);
        PS_HW6_BatteryPolicyRequestSoftwareShipment(0UL);
      }
    }
    return HAL_OK;
  }

  if (vbat_mv <= (uint32_t)KNOB_POWER_BATTERY_WARNING_MV)
  {
    g_ps_hw6_owner_sm_probe.battery_policy_state =
      PS_HW6_POWER_BATTERY_POLICY_WARNING;
    g_ps_hw6_owner_sm_probe.battery_policy_last_event =
      PS_HW6_POWER_BATTERY_EVENT_WARNING;
    g_ps_hw6_owner_sm_probe.battery_policy_warning_count++;
    (void)PS_HW6_SM_Transition(PS_HW6_SM_PMIC,
                               PMIC_EV_LOW_BATTERY,
                               HAL_OK);
    return HAL_OK;
  }

  g_ps_hw6_owner_sm_probe.battery_policy_state =
    (boot_check != 0UL) ? PS_HW6_POWER_BATTERY_POLICY_BOOT_OK :
    PS_HW6_POWER_BATTERY_POLICY_OK;
  if ((ps_power_battery_owns_ship_prep != 0UL) &&
      (g_ps_hw6_owner_sm_probe.current_state[PS_HW6_SM_POWER] ==
       PWR_SHIP_PREP))
  {
    (void)PS_HW6_SM_Transition(PS_HW6_SM_POWER,
                               PWR_EV_LP_REQUEST,
                               HAL_OK);
    ps_power_battery_owns_ship_prep = 0UL;
  }

  if ((vbus_ok != 0UL) &&
      (g_ps_hw6_owner_sm_probe.current_state[PS_HW6_SM_PMIC] !=
       PMIC_SHIP_PENDING))
  {
    if ((g_ps_hw6_owner_probe.power_charge_complete != 0UL) ||
        (g_ps_hw6_owner_probe.power_charger_status ==
         PS_HW6_CHARGER_STATUS_FULL))
    {
      (void)PS_HW6_SM_Transition(PS_HW6_SM_PMIC,
                                 PMIC_EV_CHARGE_DONE,
                                 HAL_OK);
    }
    else if (g_ps_hw6_owner_probe.power_charger_status ==
             PS_HW6_CHARGER_STATUS_CHARGING)
    {
      (void)PS_HW6_SM_Transition(PS_HW6_SM_PMIC,
                                 PMIC_EV_CHARGING,
                                 HAL_OK);
    }
    else if ((g_ps_hw6_owner_sm_probe.current_state[PS_HW6_SM_PMIC] ==
              PMIC_CHARGING) ||
             (g_ps_hw6_owner_sm_probe.current_state[PS_HW6_SM_PMIC] ==
              PMIC_CHARGE_DONE))
    {
      (void)PS_HW6_SM_Transition(PS_HW6_SM_PMIC,
                                 PMIC_EV_RECOVER_OK,
                                 HAL_OK);
    }
  }
  else if ((g_ps_hw6_owner_sm_probe.current_state[PS_HW6_SM_PMIC] ==
            PMIC_LOW_BATT) ||
           (g_ps_hw6_owner_sm_probe.current_state[PS_HW6_SM_PMIC] ==
            PMIC_CRITICAL_BATT) ||
           (g_ps_hw6_owner_sm_probe.current_state[PS_HW6_SM_PMIC] ==
            PMIC_CHARGING) ||
           (g_ps_hw6_owner_sm_probe.current_state[PS_HW6_SM_PMIC] ==
            PMIC_CHARGE_DONE))
  {
    (void)PS_HW6_SM_Transition(PS_HW6_SM_PMIC,
                               PMIC_EV_RECOVER_OK,
                               HAL_OK);
  }
  return HAL_OK;
}

static HAL_StatusTypeDef PS_HW6_StartPowerPrepareForShipment(void)
{
  HAL_StatusTypeDef status;

  status = PS_HW6_RequestPowerAdmission(
    (uint32_t)PS_HW6_POWER_QUIESCE_REASON_START_SHUTDOWN);
  if (status != HAL_OK)
  {
    return status;
  }

  g_ps_hw6_owner_sm_probe.start_power_quiesce_request_count++;
  g_ps_hw6_owner_sm_probe.start_power_quiesce_last_tick =
    (uint32_t)tx_time_get();
  status = PS_HW6_RequestPowerQuiesce(
    (uint32_t)PS_HW6_POWER_QUIESCE_REASON_START_SHUTDOWN);
  g_ps_hw6_owner_sm_probe.start_power_quiesce_last_status =
    (uint32_t)status;
  return status;
}

static void PS_HW6_StartPowerRequestSoftwareShipment(void)
{
  g_ps_hw6_owner_sm_probe.start_power_software_ship_enabled =
    (uint32_t)KNOB_POWER_START_SOFTWARE_SHIP_ENABLE;
  if (KNOB_POWER_START_SOFTWARE_SHIP_ENABLE != 0UL)
  {
    g_ps_hw6_owner_sm_probe.start_power_software_ship_request_count++;
    g_ps_hw6_owner_sm_probe.start_power_software_ship_last_tick =
      (uint32_t)tx_time_get();
    g_ps_hw6_pmic_software_ship_request = 1UL;
    g_ps_hw6_owner_sm_probe.start_power_software_ship_last_status =
      (uint32_t)HAL_OK;
  }
  else
  {
    g_ps_hw6_owner_sm_probe.start_power_software_ship_skipped_count++;
    g_ps_hw6_owner_sm_probe.start_power_software_ship_last_status =
      PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  }
}

typedef struct
{
  uint32_t from_state;
  uint32_t event;
  uint32_t to_state;
} PS_HW6_StateTransition;

typedef struct
{
  const PS_HW6_StateTransition *transitions;
  uint32_t transition_count;
} PS_HW6_StateMachineDefinition;

static const PS_HW6_StateTransition ps_power_transitions[] =
{
  {PWR_BOOTING, PWR_EV_BOOT, PWR_RAIL_VALIDATE},
  {PWR_RAIL_VALIDATE, PWR_EV_RAILS_OK, PWR_ACTIVE_LP},
  {PWR_RAIL_VALIDATE, PWR_EV_RAILS_FAIL, PWR_FAULT},
  {PWR_ACTIVE_LP, PWR_EV_RT_REQUEST, PWR_ACTIVE_RT},
  {PWR_ACTIVE_RT, PWR_EV_LP_REQUEST, PWR_ACTIVE_LP},
  {PWR_ACTIVE_LP, PWR_EV_SLEEP_REQUEST, PWR_SLEEP_PREP},
  {PWR_ACTIVE_RT, PWR_EV_SLEEP_REQUEST, PWR_SLEEP_PREP},
  {PWR_SLEEP_PREP, PWR_EV_LP_REQUEST, PWR_ACTIVE_LP},
  {PWR_SLEEP_PREP, PWR_EV_STOP_ENTERED, PWR_STOP_RESIDENT},
  {PWR_STOP_RESIDENT, PWR_EV_WAKE, PWR_WAKE_RESUME},
  {PWR_WAKE_RESUME, PWR_EV_LP_REQUEST, PWR_ACTIVE_LP},
  {PWR_ACTIVE_LP, PWR_EV_LOW_BATTERY, PWR_FORCED_SLEEP},
  {PWR_ACTIVE_RT, PWR_EV_LOW_BATTERY, PWR_FORCED_SLEEP},
  {PWR_FORCED_SLEEP, PWR_EV_LOW_BATTERY, PWR_FORCED_SLEEP},
  {PWR_ACTIVE_LP, PWR_EV_SHIP_REQUEST, PWR_SHIP_PREP},
  {PWR_ACTIVE_RT, PWR_EV_SHIP_REQUEST, PWR_SHIP_PREP},
  {PWR_FORCED_SLEEP, PWR_EV_SHIP_REQUEST, PWR_SHIP_PREP},
  {PWR_SHIP_PREP, PWR_EV_SHIP_REQUEST, PWR_SHIP_PREP},
  {PWR_ACTIVE_LP, PWR_EV_START_SHIP_PREP, PWR_SHIP_PREP},
  {PWR_ACTIVE_RT, PWR_EV_START_SHIP_PREP, PWR_SHIP_PREP},
  {PWR_SHIP_PREP, PWR_EV_START_SHIP_WARNING, PWR_SHIP_PREP},
  {PWR_SHIP_PREP, PWR_EV_START_SHIP_IMMINENT, PWR_SHIP_PREP},
  {PWR_SHIP_PREP, PWR_EV_LP_REQUEST, PWR_ACTIVE_LP},
  {PWR_SHIP_PREP, PWR_EV_RT_REQUEST, PWR_ACTIVE_RT},
  {PWR_FAULT, PWR_EV_RECOVER_OK, PWR_RAIL_VALIDATE}
};

static const PS_HW6_StateTransition ps_pmic_transitions[] =
{
  {PMIC_OFFLINE, PMIC_EV_PROBE_REQUEST, PMIC_PROBE},
  {PMIC_PROBE, PMIC_EV_PROBE_OK, PMIC_MONITOR},
  {PMIC_PROBE, PMIC_EV_PROBE_FAIL, PMIC_ERROR},
  {PMIC_MONITOR, PMIC_EV_CHARGING, PMIC_CHARGING},
  {PMIC_CHARGING, PMIC_EV_CHARGING, PMIC_CHARGING},
  {PMIC_CHARGE_DONE, PMIC_EV_CHARGING, PMIC_CHARGING},
  {PMIC_MONITOR, PMIC_EV_CHARGE_DONE, PMIC_CHARGE_DONE},
  {PMIC_CHARGING, PMIC_EV_CHARGE_DONE, PMIC_CHARGE_DONE},
  {PMIC_CHARGE_DONE, PMIC_EV_CHARGE_DONE, PMIC_CHARGE_DONE},
  {PMIC_CHARGING, PMIC_EV_RECOVER_OK, PMIC_MONITOR},
  {PMIC_CHARGE_DONE, PMIC_EV_RECOVER_OK, PMIC_MONITOR},
  {PMIC_LOW_BATT, PMIC_EV_CHARGING, PMIC_CHARGING},
  {PMIC_CRITICAL_BATT, PMIC_EV_CHARGING, PMIC_CHARGING},
  {PMIC_LOW_BATT, PMIC_EV_CHARGE_DONE, PMIC_CHARGE_DONE},
  {PMIC_CRITICAL_BATT, PMIC_EV_CHARGE_DONE, PMIC_CHARGE_DONE},
  {PMIC_MONITOR, PMIC_EV_LOW_BATTERY, PMIC_LOW_BATT},
  {PMIC_CHARGING, PMIC_EV_LOW_BATTERY, PMIC_LOW_BATT},
  {PMIC_CHARGE_DONE, PMIC_EV_LOW_BATTERY, PMIC_LOW_BATT},
  {PMIC_LOW_BATT, PMIC_EV_LOW_BATTERY, PMIC_LOW_BATT},
  {PMIC_LOW_BATT, PMIC_EV_RECOVER_OK, PMIC_MONITOR},
  {PMIC_CRITICAL_BATT, PMIC_EV_RECOVER_OK, PMIC_MONITOR},
  {PMIC_MONITOR, PMIC_EV_CRITICAL_BATTERY, PMIC_CRITICAL_BATT},
  {PMIC_LOW_BATT, PMIC_EV_CRITICAL_BATTERY, PMIC_CRITICAL_BATT},
  {PMIC_CHARGING, PMIC_EV_CRITICAL_BATTERY, PMIC_CRITICAL_BATT},
  {PMIC_CHARGE_DONE, PMIC_EV_CRITICAL_BATTERY, PMIC_CRITICAL_BATT},
  {PMIC_CRITICAL_BATT, PMIC_EV_CRITICAL_BATTERY, PMIC_CRITICAL_BATT},
  {PMIC_SHIP_PENDING, PMIC_EV_CRITICAL_BATTERY, PMIC_SHIP_PENDING},
  {PMIC_MONITOR, PMIC_EV_SHIP_REQUEST, PMIC_SHIP_PENDING},
  {PMIC_CHARGING, PMIC_EV_SHIP_REQUEST, PMIC_SHIP_PENDING},
  {PMIC_CHARGE_DONE, PMIC_EV_SHIP_REQUEST, PMIC_SHIP_PENDING},
  {PMIC_CRITICAL_BATT, PMIC_EV_SHIP_REQUEST, PMIC_SHIP_PENDING},
  {PMIC_SHIP_PENDING, PMIC_EV_SHIP_REQUEST, PMIC_SHIP_PENDING},
  {PMIC_SHIP_PENDING, PMIC_EV_RECOVER_OK, PMIC_MONITOR},
  {PMIC_ERROR, PMIC_EV_RECOVER_OK, PMIC_PROBE}
};

static const PS_HW6_StateTransition ps_display_transitions[] =
{
  {DISP_OFF, DISP_EV_ENABLE_REQ, DISP_INIT},
  {DISP_INIT, DISP_EV_INIT_OK, DISP_STATIC_HOLD},
  {DISP_STATIC_HOLD, DISP_EV_RT_ENTER, DISP_RT_ACTIVE},
  {DISP_RT_ACTIVE, DISP_EV_STATIC_HOLD_REQ, DISP_STATIC_HOLD},
  {DISP_RT_ACTIVE, DISP_EV_FAULT, DISP_ERROR},
  {DISP_INIT, DISP_EV_FAULT, DISP_ERROR},
  {DISP_ERROR, DISP_EV_RECOVER_OK, DISP_INIT}
};

static const PS_HW6_StateTransition ps_audio_transitions[] =
{
  {AUDIO_OFF, AUDIO_EV_INIT_REQ, AUDIO_INIT},
  {AUDIO_INIT, AUDIO_EV_INIT_OK, AUDIO_IDLE},
  {AUDIO_IDLE, AUDIO_EV_TONE_REQ, AUDIO_ACTIVE},
  {AUDIO_ACTIVE, AUDIO_EV_PLAYBACK_DONE, AUDIO_IDLE},
  {AUDIO_INIT, AUDIO_EV_DMA_ERROR, AUDIO_ERROR},
  {AUDIO_ACTIVE, AUDIO_EV_DMA_ERROR, AUDIO_ERROR},
  {AUDIO_ERROR, AUDIO_EV_RECOVER_OK, AUDIO_INIT}
};

static const PS_HW6_StateTransition ps_speaker_transitions[] =
{
  {SPK_OFF, SPK_EV_ENABLE_REQUEST, SPK_ENABLE},
  {SPK_ENABLE, SPK_EV_ENABLED, SPK_IDLE},
  {SPK_IDLE, SPK_EV_PRELOAD, SPK_PRELOAD},
  {SPK_PRELOAD, SPK_EV_DMA_START_OK, SPK_PLAYING},
  {SPK_PLAYING, SPK_EV_PLAYBACK_DONE, SPK_DRAINING},
  {SPK_DRAINING, SPK_EV_DRAINED, SPK_OFF},
  {SPK_ENABLE, SPK_EV_FAULT, SPK_ERROR},
  {SPK_PRELOAD, SPK_EV_FAULT, SPK_ERROR},
  {SPK_PLAYING, SPK_EV_FAULT, SPK_ERROR},
  {SPK_ERROR, SPK_EV_RECOVER_OK, SPK_ENABLE}
};

static const PS_HW6_StateTransition ps_joystick_transitions[] =
{
  {JOY_OFF, JOY_EV_ENABLE_REQUEST, JOY_PROBE},
  {JOY_SUSPENDED, JOY_EV_RESUME, JOY_PROBE},
  {JOY_SUSPENDED, JOY_EV_THRESHOLD_ARM_REQUEST, JOY_THRESHOLD_ARMED},
  {JOY_SUSPENDED, JOY_EV_SLOW_POLL_REQUEST, JOY_SLOW_POLL},
  {JOY_SUSPENDED, JOY_EV_FAST_POLL_REQUEST, JOY_FAST_POLL},
  {JOY_PROBE, JOY_EV_PROBE_OK, JOY_CONFIG},
  {JOY_CONFIG, JOY_EV_CONFIG_OK, JOY_SUSPENDED},
  {JOY_CONFIG, JOY_EV_THRESHOLD_ARM_REQUEST, JOY_THRESHOLD_ARMED},
  {JOY_CONFIG, JOY_EV_SLOW_POLL_REQUEST, JOY_SLOW_POLL},
  {JOY_CONFIG, JOY_EV_FAST_POLL_REQUEST, JOY_FAST_POLL},
  {JOY_THRESHOLD_ARMED, JOY_EV_INTERRUPT, JOY_WAKE_PENDING},
  {JOY_THRESHOLD_ARMED, JOY_EV_SLOW_POLL_REQUEST, JOY_SLOW_POLL},
  {JOY_WAKE_PENDING, JOY_EV_SAMPLE_DONE, JOY_DIRECTION_SAMPLE},
  {JOY_DIRECTION_SAMPLE, JOY_EV_NORMALIZE_DONE, JOY_THRESHOLD_ARMED},
  {JOY_SLOW_POLL, JOY_EV_THRESHOLD_ARM_REQUEST, JOY_THRESHOLD_ARMED},
  {JOY_SLOW_POLL, JOY_EV_FAST_POLL_REQUEST, JOY_FAST_POLL},
  {JOY_SLOW_POLL, JOY_EV_QUIESCE, JOY_SUSPENDED},
  {JOY_FAST_POLL, JOY_EV_SLOW_POLL_REQUEST, JOY_SLOW_POLL},
  {JOY_FAST_POLL, JOY_EV_QUIESCE, JOY_SUSPENDED},
  {JOY_PROBE, JOY_EV_I2C_ERROR, JOY_ERROR},
  {JOY_CONFIG, JOY_EV_I2C_ERROR, JOY_ERROR},
  {JOY_THRESHOLD_ARMED, JOY_EV_I2C_ERROR, JOY_ERROR},
  {JOY_WAKE_PENDING, JOY_EV_I2C_ERROR, JOY_ERROR},
  {JOY_DIRECTION_SAMPLE, JOY_EV_I2C_ERROR, JOY_ERROR},
  {JOY_SLOW_POLL, JOY_EV_I2C_ERROR, JOY_ERROR},
  {JOY_FAST_POLL, JOY_EV_I2C_ERROR, JOY_ERROR},
  {JOY_ERROR, JOY_EV_RECOVER_OK, JOY_PROBE}
};
static const PS_HW6_StateTransition ps_imu_transitions[] =
{
  {IMU_OFF, IMU_EV_ENABLE_REQUEST, IMU_PROBE},
  {IMU_SUSPENDED, IMU_EV_RESUME, IMU_PROBE},
  {IMU_PROBE, IMU_EV_PROBE_OK, IMU_CONFIG},
  {IMU_CONFIG, IMU_EV_CONFIG_OK, IMU_SUSPENDED},
  {IMU_CONFIG, IMU_EV_LOW_RATE_SAMPLE_REQUEST, IMU_LOW_RATE_SAMPLE},
  {IMU_LOW_RATE_SAMPLE, IMU_EV_QUIESCE, IMU_SUSPENDED},
  {IMU_PROBE, IMU_EV_I2C_ERROR, IMU_ERROR},
  {IMU_CONFIG, IMU_EV_I2C_ERROR, IMU_ERROR},
  {IMU_LOW_RATE_SAMPLE, IMU_EV_I2C_ERROR, IMU_ERROR},
  {IMU_ERROR, IMU_EV_RECOVER_OK, IMU_PROBE}
};

static const PS_HW6_StateTransition ps_storage_transitions[] =
{
  {STORAGE_OFFLINE, STORAGE_EV_INIT, STORAGE_INIT},
  {STORAGE_INIT, STORAGE_EV_FLASH_READY, STORAGE_FLASH_READY},
  {STORAGE_FLASH_READY, STORAGE_EV_USB_VBUS_PRESENT, STORAGE_PREPARE_USB},
  {STORAGE_PREPARE_USB, STORAGE_EV_USB_MSC_ENTRY_ACCEPTED, STORAGE_USB_STAGING_EXPORTED},
  {STORAGE_USB_STAGING_EXPORTED, STORAGE_EV_USB_HOST_DIRTY, STORAGE_USB_STAGING_DIRTY},
  {STORAGE_USB_STAGING_EXPORTED, STORAGE_EV_USB_RELEASE_REQUEST, STORAGE_USB_RELEASE},
  {STORAGE_USB_STAGING_DIRTY, STORAGE_EV_USB_RELEASE_REQUEST, STORAGE_USB_RELEASE},
  {STORAGE_USB_RELEASE, STORAGE_EV_USB_RESCAN_OK, STORAGE_FLASH_READY},
  {STORAGE_PREPARE_USB, STORAGE_EV_FAULT, STORAGE_ERROR},
  {STORAGE_INIT, STORAGE_EV_FAULT, STORAGE_ERROR},
  {STORAGE_ERROR, STORAGE_EV_RECOVER_OK, STORAGE_RECOVERING},
  {STORAGE_RECOVERING, STORAGE_EV_FLASH_READY, STORAGE_FLASH_READY}
};

static const PS_HW6_StateTransition ps_flash_transitions[] =
{
  {FLASH_OFF, FLASH_EV_BOOT, FLASH_PROBE},
  {FLASH_PROBE, FLASH_EV_PROBE_OK, FLASH_CONFIG},
  {FLASH_PROBE, FLASH_EV_PROBE_FAIL, FLASH_ERROR},
  {FLASH_CONFIG, FLASH_EV_CONFIG_OK, FLASH_READY},
  {FLASH_READY, FLASH_EV_REQUEST_DEEP_POWER_DOWN, FLASH_DEEP_POWER_DOWN},
  {FLASH_DEEP_POWER_DOWN, FLASH_EV_WAKE_REVALIDATE, FLASH_PROBE},
  {FLASH_CONFIG, FLASH_EV_FAULT, FLASH_ERROR},
  {FLASH_READY, FLASH_EV_FAULT, FLASH_ERROR},
  {FLASH_ERROR, FLASH_EV_RECOVER_RETRY, FLASH_RECOVERING},
  {FLASH_RECOVERING, FLASH_EV_WAKE_REVALIDATE, FLASH_PROBE}
};

static const PS_HW6_StateTransition ps_ble_transitions[] =
{
  {BLE_OFF, BLE_EV_ENABLE_REQUEST, BLE_RESET_ASSERT},
  {BLE_RESET_ASSERT, BLE_EV_RESET_ASSERTED, BLE_BOOT_WAIT},
  {BLE_BOOT_WAIT, BLE_EV_BOOT_READY, BLE_CONFIG},
  {BLE_BOOT_WAIT, BLE_EV_BOOT_TIMEOUT, BLE_ERROR},
  {BLE_CONFIG, BLE_EV_CONFIG_OK, BLE_IDLE},
  {BLE_CONFIG, BLE_EV_CONFIG_FAIL, BLE_ERROR},
  {BLE_IDLE, BLE_EV_ADV_START_REQUEST, BLE_ADVERTISING},
  {BLE_ADVERTISING, BLE_EV_ADV_STOP_REQUEST, BLE_IDLE},
  {BLE_IDLE, BLE_EV_PAIRING_START, BLE_PAIRING},
  {BLE_ADVERTISING, BLE_EV_PAIRING_START, BLE_PAIRING},
  {BLE_PAIRING, BLE_EV_PAIRING_DONE, BLE_IDLE},
  {BLE_IDLE, BLE_EV_CONNECTED, BLE_CONNECTED},
  {BLE_ADVERTISING, BLE_EV_CONNECTED, BLE_CONNECTED},
  {BLE_PAIRING, BLE_EV_CONNECTED, BLE_CONNECTED},
  {BLE_CONNECTED, BLE_EV_DISCONNECTED, BLE_IDLE},
  {BLE_IDLE, BLE_EV_DISABLE_REQUEST, BLE_SUSPENDING},
  {BLE_ADVERTISING, BLE_EV_DISABLE_REQUEST, BLE_SUSPENDING},
  {BLE_PAIRING, BLE_EV_DISABLE_REQUEST, BLE_SUSPENDING},
  {BLE_CONNECTED, BLE_EV_DISABLE_REQUEST, BLE_SUSPENDING},
  {BLE_SUSPENDING, BLE_EV_QUIESCE, BLE_SUSPENDED},
  {BLE_SUSPENDING, BLE_EV_DISABLE_REQUEST, BLE_OFF},
  {BLE_SUSPENDED, BLE_EV_RESUME, BLE_BOOT_WAIT},
  {BLE_SUSPENDED, BLE_EV_DISABLE_REQUEST, BLE_OFF},
  {BLE_RESET_ASSERT, BLE_EV_DISABLE_REQUEST, BLE_OFF},
  {BLE_BOOT_WAIT, BLE_EV_DISABLE_REQUEST, BLE_OFF},
  {BLE_CONFIG, BLE_EV_DISABLE_REQUEST, BLE_OFF},
  {BLE_ERROR, BLE_EV_DISABLE_REQUEST, BLE_OFF},
  {BLE_RESET_ASSERT, BLE_EV_FAULT, BLE_ERROR},
  {BLE_IDLE, BLE_EV_FAULT, BLE_ERROR},
  {BLE_ADVERTISING, BLE_EV_FAULT, BLE_ERROR},
  {BLE_PAIRING, BLE_EV_FAULT, BLE_ERROR},
  {BLE_CONNECTED, BLE_EV_FAULT, BLE_ERROR},
  {BLE_SUSPENDING, BLE_EV_FAULT, BLE_ERROR},
  {BLE_ERROR, BLE_EV_RECOVER_OK, BLE_RESET_ASSERT}
};

static const PS_HW6_StateMachineDefinition ps_state_machines[PS_HW6_OWNER_SM_COUNT] =
{
  {ps_power_transitions, PS_HW6_ARRAY_COUNT(ps_power_transitions)},
  {ps_pmic_transitions, PS_HW6_ARRAY_COUNT(ps_pmic_transitions)},
  {ps_display_transitions, PS_HW6_ARRAY_COUNT(ps_display_transitions)},
  {ps_audio_transitions, PS_HW6_ARRAY_COUNT(ps_audio_transitions)},
  {ps_speaker_transitions, PS_HW6_ARRAY_COUNT(ps_speaker_transitions)},
  {ps_joystick_transitions, PS_HW6_ARRAY_COUNT(ps_joystick_transitions)},
  {ps_imu_transitions, PS_HW6_ARRAY_COUNT(ps_imu_transitions)},
  {ps_storage_transitions, PS_HW6_ARRAY_COUNT(ps_storage_transitions)},
  {ps_flash_transitions, PS_HW6_ARRAY_COUNT(ps_flash_transitions)},
  {ps_ble_transitions, PS_HW6_ARRAY_COUNT(ps_ble_transitions)}
};

static const uint32_t ps_initial_states[PS_HW6_OWNER_SM_COUNT] =
{
  PWR_BOOTING,
  PMIC_OFFLINE,
  DISP_OFF,
  AUDIO_OFF,
  SPK_OFF,
  JOY_OFF,
  IMU_OFF,
  STORAGE_OFFLINE,
  FLASH_OFF,
  BLE_OFF
};

static const uint32_t ps_cycle_active_states[PS_HW6_OWNER_SM_COUNT] =
{
  PWR_ACTIVE_RT,
  PMIC_MONITOR,
  DISP_STATIC_HOLD,
  AUDIO_IDLE,
  SPK_OFF,
  JOY_SLOW_POLL,
  IMU_LOW_RATE_SAMPLE,
  STORAGE_FLASH_READY,
  FLASH_READY,
  BLE_IDLE
};

static const uint32_t ps_cycle_inactive_states[PS_HW6_OWNER_SM_COUNT] =
{
  PWR_ACTIVE_LP,
  PMIC_MONITOR,
  DISP_STATIC_HOLD,
  AUDIO_IDLE,
  SPK_OFF,
  JOY_SUSPENDED,
  IMU_SUSPENDED,
  STORAGE_FLASH_READY,
  FLASH_DEEP_POWER_DOWN,
  BLE_SUSPENDED
};

static ps_dev_lis2dux12_t ps_imu_device;
static ps_dev_tmag3001_t ps_joystick_device;
static ps_input_joystick_state_t ps_joystick_input_state;
static const ps_input_joystick_calibration_t ps_joystick_hw6_default_calibration =
{
  -5616,
  -5536,
  -20960,
  27536,
  -27392,
  19520,
  3500,
  450,
  1UL
};
static ps_input_joystick_calibration_t ps_joystick_active_calibration =
{
  -5616,
  -5536,
  -20960,
  27536,
  -27392,
  19520,
  3500,
  450,
  1UL
};
static ps_dev_at25sl128a_t ps_flash_device;
static ps_storage_flash_block_t ps_flash_block;
static ps_dev_at25sl128a_jedec_result_t ps_flash_jedec_result;
static ps_dev_at25sl128a_command_result_t ps_flash_command_result;
static ps_dev_at25sl128a_scratch_result_t ps_flash_scratch_result;
static ps_storage_flash_block_test_result_t ps_flash_block_result;
static ps_storage_layout_validation_t ps_storage_layout_result;
static ps_storage_filex_levelx_smoke_result_t ps_storage_fxlx_result;
static ps_storage_filex_levelx_stage_scan_result_t ps_storage_stage_scan_result;
static ps_storage_filex_levelx_package_validate_result_t
  ps_storage_package_validate_result;
static uint8_t ps_nina_rx_buffer[PS_HW6_NINA_RX_BUFFER_SIZE];
static ps_status_t PS_HW6_SM_EnsureFlashAwake(void);
static void PS_HW6_SM_ParkOspiClocksForStop(void);
static void PS_HW6_SM_RestoreOspiClocksAfterStop(void);
static HAL_StatusTypeDef PS_HW6_SM_ParkUsb(void);
static void PS_HW6_SM_RecordUsbExportEntryState(void);
static void PS_HW6_SM_UpdateUsbHostAvailability(uint32_t event);
static HAL_StatusTypeDef PS_HW6_SM_PrepareStorageForFlashReady(
  uint32_t record_usb_export_entry);
static HAL_StatusTypeDef PS_HW6_SM_PrepareStorageForUsbExport(void);
static HAL_StatusTypeDef PS_HW6_SM_RunUsbStageRescanScaffold(void);


static void PS_HW6_SM_RecordTrace(uint32_t state_machine_id,
                                   uint32_t from_state,
                                   uint32_t event,
                                   uint32_t to_state,
                                   uint32_t action_status)
{
  uint32_t index = g_ps_hw6_owner_sm_probe.trace_write_index;

  g_ps_hw6_owner_sm_probe.trace[index].tick = (uint32_t)tx_time_get();
  g_ps_hw6_owner_sm_probe.trace[index].state_machine_id = state_machine_id;
  g_ps_hw6_owner_sm_probe.trace[index].from_state = from_state;
  g_ps_hw6_owner_sm_probe.trace[index].event = event;
  g_ps_hw6_owner_sm_probe.trace[index].to_state = to_state;
  g_ps_hw6_owner_sm_probe.trace[index].action_status = action_status;
  g_ps_hw6_owner_sm_probe.trace_write_index =
    (index + 1U) % PS_HW6_OWNER_SM_TRACE_DEPTH;
  if (g_ps_hw6_owner_sm_probe.trace_count < PS_HW6_OWNER_SM_TRACE_DEPTH)
  {
    g_ps_hw6_owner_sm_probe.trace_count++;
  }
}

static HAL_StatusTypeDef PS_HW6_SM_Transition(uint32_t state_machine_id,
                                               uint32_t event,
                                               HAL_StatusTypeDef action_status)
{
  const PS_HW6_StateMachineDefinition *definition;
  uint32_t current_state;
  uint32_t index;

  if (state_machine_id >= PS_HW6_OWNER_SM_COUNT)
  {
    return HAL_ERROR;
  }

  definition = &ps_state_machines[state_machine_id];
  current_state = g_ps_hw6_owner_sm_probe.current_state[state_machine_id];
  for (index = 0U; index < definition->transition_count; ++index)
  {
    const PS_HW6_StateTransition *transition =
      &definition->transitions[index];

    if ((transition->from_state == current_state) &&
        (transition->event == event))
    {
      g_ps_hw6_owner_sm_probe.previous_state[state_machine_id] = current_state;
      g_ps_hw6_owner_sm_probe.requested_state[state_machine_id] =
        transition->to_state;
      g_ps_hw6_owner_sm_probe.current_state[state_machine_id] =
        transition->to_state;
      g_ps_hw6_owner_sm_probe.last_event[state_machine_id] = event;
      g_ps_hw6_owner_sm_probe.transition_count[state_machine_id]++;
      g_ps_hw6_owner_sm_probe.last_action_status[state_machine_id] =
        (uint32_t)action_status;
      g_ps_hw6_owner_sm_probe.last_transition_tick[state_machine_id] =
        (uint32_t)tx_time_get();
      if (action_status != HAL_OK)
      {
        g_ps_hw6_owner_sm_probe.last_error[state_machine_id] =
          (uint32_t)action_status;
      }
      PS_HW6_SM_RecordTrace(state_machine_id, current_state, event,
                            transition->to_state, (uint32_t)action_status);
      PS_HW6_TraceOwnerState(state_machine_id,
                              current_state,
                              event,
                              transition->to_state);
      if ((state_machine_id == PS_HW6_SM_STORAGE) &&
          (transition->to_state == STORAGE_FLASH_READY) &&
          (action_status == HAL_OK))
      {
        PS_HW6_TraceSwoLifecycle(PS_HW6_TRACE_SWO_STORAGE_READY);
      }
      return HAL_OK;
    }
  }

  g_ps_hw6_owner_sm_probe.rejected_transition_count[state_machine_id]++;
  g_ps_hw6_owner_sm_probe.last_event[state_machine_id] = event;
  g_ps_hw6_owner_sm_probe.last_error[state_machine_id] = (uint32_t)HAL_ERROR;
  PS_HW6_SM_RecordTrace(state_machine_id, current_state, event,
                        current_state, (uint32_t)HAL_ERROR);
  PS_HW6_TraceOwnerReject(state_machine_id,
                          current_state,
                          event,
                          (uint32_t)HAL_ERROR);
  return HAL_ERROR;
}

static HAL_StatusTypeDef PS_HW6_SM_StatusToHal(ps_status_t status)
{
  return (status == PS_STATUS_OK) ? HAL_OK : HAL_ERROR;
}

static void PS_HW6_SM_UpdateImuDriverProbe(void)
{
  g_ps_hw6_owner_sm_probe.imu_driver_api_version =
    ps_imu_device.api_version;
  g_ps_hw6_owner_sm_probe.imu_driver_state = ps_imu_device.state;
  g_ps_hw6_owner_sm_probe.imu_driver_operation_count =
    ps_imu_device.operation_count;
  g_ps_hw6_owner_sm_probe.imu_driver_last_status =
    ps_imu_device.last_status;
}

static void PS_HW6_SM_UpdateJoystickDriverProbe(void)
{
  g_ps_hw6_owner_sm_probe.joystick_driver_api_version =
    ps_joystick_device.api_version;
  g_ps_hw6_owner_sm_probe.joystick_driver_state =
    ps_joystick_device.state;
  g_ps_hw6_owner_sm_probe.joystick_driver_operation_count =
    ps_joystick_device.operation_count;
  g_ps_hw6_owner_sm_probe.joystick_driver_last_status =
    ps_joystick_device.last_status;
}

static void PS_HW6_SM_UpdateJoystickInputProbe(void)
{
  g_ps_hw6_owner_sm_probe.joystick_input_api_version =
    ps_joystick_input_state.api_version;
  g_ps_hw6_owner_sm_probe.joystick_input_policy =
    ps_joystick_input_state.policy;
  g_ps_hw6_owner_sm_probe.joystick_input_calibration_valid =
    ps_joystick_input_state.calibration_valid;
  g_ps_hw6_owner_sm_probe.joystick_input_active =
    ps_joystick_input_state.active;
  g_ps_hw6_owner_sm_probe.joystick_input_direction_mask =
    ps_joystick_input_state.direction_mask;
  g_ps_hw6_owner_sm_probe.joystick_input_sample_tick =
    ps_joystick_input_state.sample_tick;
  g_ps_hw6_owner_sm_probe.joystick_input_sample_age_ticks =
    ps_joystick_input_state.sample_age_ticks;
  g_ps_hw6_owner_sm_probe.joystick_input_update_count =
    ps_joystick_input_state.update_count;
  g_ps_hw6_owner_sm_probe.joystick_input_fault_count =
    ps_joystick_input_state.fault_count;
  g_ps_hw6_owner_sm_probe.joystick_input_last_status =
    ps_joystick_input_state.last_status;
  g_ps_hw6_owner_sm_probe.joystick_input_raw_x =
    ps_joystick_input_state.raw_x;
  g_ps_hw6_owner_sm_probe.joystick_input_raw_y =
    ps_joystick_input_state.raw_y;
  g_ps_hw6_owner_sm_probe.joystick_input_raw_z =
    ps_joystick_input_state.raw_z;
  g_ps_hw6_owner_sm_probe.joystick_input_delta_x =
    ps_joystick_input_state.delta_x;
  g_ps_hw6_owner_sm_probe.joystick_input_delta_y =
    ps_joystick_input_state.delta_y;
  g_ps_hw6_owner_sm_probe.joystick_input_normalized_x =
    ps_joystick_input_state.normalized_x;
  g_ps_hw6_owner_sm_probe.joystick_input_normalized_y =
    ps_joystick_input_state.normalized_y;
  g_ps_hw6_owner_sm_probe.joystick_input_magnitude =
    ps_joystick_input_state.magnitude;
  g_ps_hw6_owner_sm_probe.joystick_input_conv_status =
    ps_joystick_input_state.conv_status;
}

static HAL_StatusTypeDef PS_HW6_SM_NormalizeJoystickSample(
  const ps_dev_tmag3001_raw_sample_t *sample,
  uint32_t policy)
{
  ps_input_joystick_raw_sample_t raw_sample;
  ps_status_t input_status;
  uint32_t now_tick;

  if (sample == (const ps_dev_tmag3001_raw_sample_t *)0)
  {
    return HAL_ERROR;
  }

  now_tick = (uint32_t)tx_time_get();
  raw_sample.raw_x = (int32_t)sample->x;
  raw_sample.raw_y = (int32_t)sample->y;
  raw_sample.raw_z = (int32_t)sample->z;
  raw_sample.conv_status = (uint32_t)sample->conv_status;
  raw_sample.sample_tick = now_tick;

  input_status = PS_InputJoystick_Normalize(
    &ps_joystick_active_calibration,
    &raw_sample,
    policy,
    now_tick,
    &ps_joystick_input_state);
  PS_HW6_SM_UpdateJoystickInputProbe();
  return PS_HW6_SM_StatusToHal(input_status);
}

static int32_t PS_HW6_SM_Abs32(int32_t value)
{
  return (value < 0) ? -value : value;
}

static int32_t PS_HW6_SM_ClampJoystickDeadzone(int32_t deadzone)
{
  if (deadzone < PS_HW6_JOYSTICK_CAL_MIN_DEADZONE)
  {
    return PS_HW6_JOYSTICK_CAL_MIN_DEADZONE;
  }
  if (deadzone > PS_HW6_JOYSTICK_CAL_MAX_DEADZONE)
  {
    return PS_HW6_JOYSTICK_CAL_MAX_DEADZONE;
  }
  return deadzone;
}

static void PS_HW6_SM_UpdateJoystickCalibrationProbe(void)
{
  g_ps_hw6_owner_sm_probe.joystick_calibration_active_valid =
    ps_joystick_active_calibration.valid;
  g_ps_hw6_owner_sm_probe.joystick_calibration_center_x =
    ps_joystick_active_calibration.center_x;
  g_ps_hw6_owner_sm_probe.joystick_calibration_center_y =
    ps_joystick_active_calibration.center_y;
  g_ps_hw6_owner_sm_probe.joystick_calibration_min_x =
    ps_joystick_active_calibration.min_x;
  g_ps_hw6_owner_sm_probe.joystick_calibration_max_x =
    ps_joystick_active_calibration.max_x;
  g_ps_hw6_owner_sm_probe.joystick_calibration_min_y =
    ps_joystick_active_calibration.min_y;
  g_ps_hw6_owner_sm_probe.joystick_calibration_max_y =
    ps_joystick_active_calibration.max_y;
  g_ps_hw6_owner_sm_probe.joystick_calibration_deadzone_counts =
    ps_joystick_active_calibration.deadzone_counts;
  g_ps_hw6_owner_sm_probe.joystick_calibration_direction_threshold =
    ps_joystick_active_calibration.direction_threshold;
}

static uint32_t PS_HW6_SM_JoystickCalibrationHasRange(void)
{
  if ((ps_joystick_active_calibration.min_x >=
       ps_joystick_active_calibration.center_x) ||
      (ps_joystick_active_calibration.max_x <=
       ps_joystick_active_calibration.center_x) ||
      (ps_joystick_active_calibration.min_y >=
       ps_joystick_active_calibration.center_y) ||
      (ps_joystick_active_calibration.max_y <=
       ps_joystick_active_calibration.center_y))
  {
    return 0UL;
  }
  return 1UL;
}

static void PS_HW6_SM_ResetJoystickInputProbe(void)
{
  PS_InputJoystick_InitState(&ps_joystick_input_state);
  PS_HW6_SM_UpdateJoystickInputProbe();
  g_ps_hw6_owner_sm_probe.joystick_live_request_count = 0UL;
  g_ps_hw6_owner_sm_probe.joystick_live_start_tick = 0UL;
  g_ps_hw6_owner_sm_probe.joystick_live_end_tick = 0UL;
  g_ps_hw6_owner_sm_probe.joystick_live_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.joystick_live_sample_count = 0UL;
  g_ps_hw6_owner_sm_probe.joystick_live_error_count = 0UL;
  g_ps_hw6_owner_sm_probe.joystick_cardinal_request_count = 0UL;
  g_ps_hw6_owner_sm_probe.joystick_cardinal_start_tick = 0UL;
  g_ps_hw6_owner_sm_probe.joystick_cardinal_end_tick = 0UL;
  g_ps_hw6_owner_sm_probe.joystick_cardinal_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.joystick_calibration_capture_request_count = 0UL;
  g_ps_hw6_owner_sm_probe.joystick_calibration_capture_start_tick = 0UL;
  g_ps_hw6_owner_sm_probe.joystick_calibration_capture_end_tick = 0UL;
  g_ps_hw6_owner_sm_probe.joystick_calibration_capture_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.joystick_calibration_capture_page =
    PS_UI_ROUTER_CAL_NONE;
  PS_HW6_SM_UpdateJoystickCalibrationProbe();
}

static void PS_HW6_SM_ResetJoystickSampleProbe(void)
{
  g_ps_hw6_owner_sm_probe.joystick_sample_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.joystick_sample_stabilize_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.joystick_sample_wake_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.joystick_sample_read_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.joystick_sample_sleep_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.joystick_sample_center_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.joystick_sample_center_conv_status = 0UL;
  g_ps_hw6_owner_sm_probe.joystick_sample_center_x = 0;
  g_ps_hw6_owner_sm_probe.joystick_sample_center_y = 0;
  g_ps_hw6_owner_sm_probe.joystick_sample_center_z = 0;
  g_ps_hw6_owner_sm_probe.joystick_sample_count = 0UL;
  g_ps_hw6_owner_sm_probe.joystick_sample_error_count = 0UL;
  g_ps_hw6_owner_sm_probe.joystick_sample_first_x = 0;
  g_ps_hw6_owner_sm_probe.joystick_sample_first_y = 0;
  g_ps_hw6_owner_sm_probe.joystick_sample_first_z = 0;
  g_ps_hw6_owner_sm_probe.joystick_sample_min_x = 0;
  g_ps_hw6_owner_sm_probe.joystick_sample_min_y = 0;
  g_ps_hw6_owner_sm_probe.joystick_sample_min_z = 0;
  g_ps_hw6_owner_sm_probe.joystick_sample_max_x = 0;
  g_ps_hw6_owner_sm_probe.joystick_sample_max_y = 0;
  g_ps_hw6_owner_sm_probe.joystick_sample_max_z = 0;
  g_ps_hw6_owner_sm_probe.joystick_sample_x = 0;
  g_ps_hw6_owner_sm_probe.joystick_sample_y = 0;
  g_ps_hw6_owner_sm_probe.joystick_sample_z = 0;
  g_ps_hw6_owner_sm_probe.joystick_sample_conv_status = 0UL;
}

static void PS_HW6_SM_ResetJoystickSleepAuditProbe(void)
{
  g_ps_hw6_owner_sm_probe.joystick_sleep_audit_start_tick = 0UL;
  g_ps_hw6_owner_sm_probe.joystick_sleep_audit_end_tick = 0UL;
  g_ps_hw6_owner_sm_probe.joystick_sleep_audit_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.joystick_sleep_audit_ready_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.joystick_sleep_audit_identity_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.joystick_sleep_audit_device_id = 0UL;
  g_ps_hw6_owner_sm_probe.joystick_sleep_audit_manufacturer_lsb = 0UL;
  g_ps_hw6_owner_sm_probe.joystick_sleep_audit_manufacturer_msb = 0UL;
  g_ps_hw6_owner_sm_probe.joystick_sleep_audit_identity_match = 0UL;
  g_ps_hw6_owner_sm_probe.joystick_sleep_audit_sensor_config1_before = 0UL;
  g_ps_hw6_owner_sm_probe.joystick_sleep_audit_sensor_config1_after = 0UL;
  g_ps_hw6_owner_sm_probe.joystick_sleep_audit_int_config1_before = 0UL;
  g_ps_hw6_owner_sm_probe.joystick_sleep_audit_int_config1_target = 0UL;
  g_ps_hw6_owner_sm_probe.joystick_sleep_audit_int_config1_after = 0UL;
  g_ps_hw6_owner_sm_probe.joystick_sleep_audit_device_config2_before = 0UL;
  g_ps_hw6_owner_sm_probe.joystick_sleep_audit_device_config2_after = 0UL;
  g_ps_hw6_owner_sm_probe.joystick_sleep_audit_device_config2_sleep = 0UL;
  g_ps_hw6_owner_sm_probe.joystick_sleep_audit_write_ok_mask = 0UL;
  g_ps_hw6_owner_sm_probe.joystick_sleep_audit_verify_ok_mask = 0UL;
  g_ps_hw6_owner_sm_probe.joystick_sleep_audit_sensor_config1_verify_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.joystick_sleep_audit_int_config1_verify_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.joystick_sleep_audit_device_config2_verify_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.joystick_sleep_audit_sleep_write_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.joystick_sleep_audit_terminal_sleep_committed =
    0UL;
  g_ps_hw6_owner_sm_probe.joystick_sleep_audit_post_sleep_read_omitted =
    0UL;
  g_ps_hw6_owner_sm_probe.joystick_sleep_audit_i2c_state_after = 0UL;
  g_ps_hw6_owner_sm_probe.joystick_sleep_audit_i2c_error_after = 0UL;
  g_ps_hw6_owner_sm_probe.joystick_sleep_audit_last_hal_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.joystick_sleep_audit_last_hal_error = 0UL;
}

static void PS_HW6_SM_ResetJoystickXyzCaptureProbe(void)
{
  g_ps_hw6_owner_sm_probe.joystick_xyz_capture_start_tick = 0UL;
  g_ps_hw6_owner_sm_probe.joystick_xyz_capture_end_tick = 0UL;
  g_ps_hw6_owner_sm_probe.joystick_xyz_capture_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.joystick_xyz_capture_mode =
    PS_HW6_JOYSTICK_XYZ_CAPTURE_NONE;
  g_ps_hw6_owner_sm_probe.joystick_xyz_capture_stabilize_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.joystick_xyz_capture_wake_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.joystick_xyz_capture_sleep_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.joystick_xyz_capture_sensor_config2_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.joystick_xyz_capture_sensor_config2_restore_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.joystick_xyz_capture_sensor_config2_before =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.joystick_xyz_capture_sensor_config2_active =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.joystick_xyz_capture_sensor_config2_restore =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.joystick_xyz_capture_range_override_mask = 0UL;
  g_ps_hw6_owner_sm_probe.joystick_xyz_capture_range_override_value = 0UL;
  g_ps_hw6_owner_sm_probe.joystick_xyz_capture_range_override_applied = 0UL;
  g_ps_hw6_owner_sm_probe.joystick_xyz_capture_period_ticks = 0UL;
  g_ps_hw6_owner_sm_probe.joystick_xyz_capture_timeout_ticks = 0UL;
  g_ps_hw6_owner_sm_probe.joystick_xyz_capture_requested_samples = 0UL;
  g_ps_hw6_owner_sm_probe.joystick_xyz_capture_capacity =
    (uint32_t)PS_HW6_JOYSTICK_XYZ_CAPTURE_CAPACITY;
  g_ps_hw6_owner_sm_probe.joystick_xyz_capture_count = 0UL;
  g_ps_hw6_owner_sm_probe.joystick_xyz_capture_success_count = 0UL;
  g_ps_hw6_owner_sm_probe.joystick_xyz_capture_error_count = 0UL;
  g_ps_hw6_owner_sm_probe.joystick_xyz_capture_timeout_count = 0UL;
  g_ps_hw6_owner_sm_probe.joystick_xyz_capture_first_x = 0;
  g_ps_hw6_owner_sm_probe.joystick_xyz_capture_first_y = 0;
  g_ps_hw6_owner_sm_probe.joystick_xyz_capture_first_z = 0;
  g_ps_hw6_owner_sm_probe.joystick_xyz_capture_min_x = 0;
  g_ps_hw6_owner_sm_probe.joystick_xyz_capture_min_y = 0;
  g_ps_hw6_owner_sm_probe.joystick_xyz_capture_min_z = 0;
  g_ps_hw6_owner_sm_probe.joystick_xyz_capture_max_x = 0;
  g_ps_hw6_owner_sm_probe.joystick_xyz_capture_max_y = 0;
  g_ps_hw6_owner_sm_probe.joystick_xyz_capture_max_z = 0;
  g_ps_hw6_owner_sm_probe.joystick_xyz_capture_last_x = 0;
  g_ps_hw6_owner_sm_probe.joystick_xyz_capture_last_y = 0;
  g_ps_hw6_owner_sm_probe.joystick_xyz_capture_last_z = 0;
  g_ps_hw6_owner_sm_probe.joystick_xyz_capture_max_abs_delta_z = 0UL;
  g_ps_hw6_owner_sm_probe.joystick_xyz_capture_last_read_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.joystick_xyz_capture_last_conv_status = 0UL;
}

static void PS_HW6_SM_ResetJoystickRuntimeProbes(void)
{
  PS_HW6_SM_ResetJoystickInputProbe();
  PS_HW6_SM_ResetJoystickSampleProbe();
  PS_HW6_SM_ResetJoystickSleepAuditProbe();
  PS_HW6_SM_ResetJoystickXyzCaptureProbe();
}
static void PS_HW6_SM_UpdateFlashBlockProbe(void)
{
  g_ps_hw6_owner_sm_probe.flash_block_api_version =
    ps_flash_block.api_version;
  g_ps_hw6_owner_sm_probe.flash_block_operation_count =
    ps_flash_block.operation_count;
  g_ps_hw6_owner_sm_probe.flash_block_last_status =
    ps_flash_block.last_status;
  g_ps_hw6_owner_sm_probe.flash_block_geometry_total_size =
    ps_flash_block.geometry.total_size;
  g_ps_hw6_owner_sm_probe.flash_block_geometry_erase_size =
    ps_flash_block.geometry.erase_block_size;
  g_ps_hw6_owner_sm_probe.flash_block_geometry_page_size =
    ps_flash_block.geometry.program_page_size;
  g_ps_hw6_owner_sm_probe.flash_block_geometry_count =
    ps_flash_block.geometry.logical_block_count;
}

static void PS_HW6_SM_UpdateFlashDriverProbe(void)
{
  g_ps_hw6_owner_sm_probe.flash_driver_api_version =
    ps_flash_device.api_version;
  g_ps_hw6_owner_sm_probe.flash_driver_state = ps_flash_device.state;
  g_ps_hw6_owner_sm_probe.flash_driver_operation_count =
    ps_flash_device.operation_count;
  g_ps_hw6_owner_sm_probe.flash_driver_last_status =
    ps_flash_device.last_status;
}
static void PS_HW6_SM_RecordFlashScratchResult(
  const ps_dev_at25sl128a_scratch_result_t *result)
{
  uint32_t index;

  if (result == NULL)
  {
    return;
  }

  g_ps_hw6_owner_sm_probe.flash_scratch_status =
    (uint32_t)result->status;
  g_ps_hw6_owner_sm_probe.flash_scratch_address =
    result->address;
  g_ps_hw6_owner_sm_probe.flash_scratch_length =
    result->length;
  g_ps_hw6_owner_sm_probe.flash_scratch_status1_before =
    result->status1_before;
  g_ps_hw6_owner_sm_probe.flash_scratch_write_disable_status =
    result->write_disable_hal_status;
  g_ps_hw6_owner_sm_probe.flash_scratch_write_disable_status1 =
    result->write_disable_status1;
  g_ps_hw6_owner_sm_probe.flash_scratch_erase_write_enable_status =
    result->erase_write_enable_hal_status;
  g_ps_hw6_owner_sm_probe.flash_scratch_erase_write_enable_status1 =
    result->erase_write_enable_status1;
  g_ps_hw6_owner_sm_probe.flash_scratch_erase_status =
    result->erase_hal_status;
  g_ps_hw6_owner_sm_probe.flash_scratch_erase_command_status1 =
    result->erase_command_status1;
  g_ps_hw6_owner_sm_probe.flash_scratch_erase_retry_count =
    result->erase_retry_count;
  g_ps_hw6_owner_sm_probe.flash_scratch_erase_retry_write_disable_status =
    result->erase_retry_write_disable_status;
  g_ps_hw6_owner_sm_probe.flash_scratch_erase_retry_write_disable_status1 =
    result->erase_retry_write_disable_status1;
  g_ps_hw6_owner_sm_probe.flash_scratch_erase_retry_write_enable_status =
    result->erase_retry_write_enable_status;
  g_ps_hw6_owner_sm_probe.flash_scratch_erase_retry_write_enable_status1 =
    result->erase_retry_write_enable_status1;
  g_ps_hw6_owner_sm_probe.flash_scratch_erase_retry_status =
    result->erase_retry_hal_status;
  g_ps_hw6_owner_sm_probe.flash_scratch_erase_retry_status1 =
    result->erase_retry_status1;
  g_ps_hw6_owner_sm_probe.flash_scratch_erase_wait_status =
    result->erase_wait_status;
  g_ps_hw6_owner_sm_probe.flash_scratch_erase_poll_count =
    result->erase_poll_count;
  g_ps_hw6_owner_sm_probe.flash_scratch_erase_blank_read_status =
    result->erase_blank_read_hal_status;
  g_ps_hw6_owner_sm_probe.flash_scratch_erase_blank_mismatch_count =
    result->erase_blank_mismatch_count;
  g_ps_hw6_owner_sm_probe.flash_scratch_program_write_enable_status =
    result->program_write_enable_hal_status;
  g_ps_hw6_owner_sm_probe.flash_scratch_program_write_enable_status1 =
    result->program_write_enable_status1;
  g_ps_hw6_owner_sm_probe.flash_scratch_program_status =
    result->program_hal_status;
  g_ps_hw6_owner_sm_probe.flash_scratch_program_wait_status =
    result->program_wait_status;
  g_ps_hw6_owner_sm_probe.flash_scratch_program_poll_count =
    result->program_poll_count;
  g_ps_hw6_owner_sm_probe.flash_scratch_program_read_status =
    result->program_read_hal_status;
  g_ps_hw6_owner_sm_probe.flash_scratch_program_mismatch_count =
    result->program_mismatch_count;
  g_ps_hw6_owner_sm_probe.flash_scratch_dma_program_write_enable_status =
    result->dma_program_write_enable_hal_status;
  g_ps_hw6_owner_sm_probe.flash_scratch_dma_program_write_enable_status1 =
    result->dma_program_write_enable_status1;
  g_ps_hw6_owner_sm_probe.flash_scratch_dma_program_status =
    result->dma_program_hal_status;
  g_ps_hw6_owner_sm_probe.flash_scratch_dma_program_transfer_wait_status =
    result->dma_program_transfer_wait_status;
  g_ps_hw6_owner_sm_probe.flash_scratch_dma_program_transfer_poll_count =
    result->dma_program_transfer_poll_count;
  g_ps_hw6_owner_sm_probe.flash_scratch_dma_program_flash_wait_status =
    result->dma_program_flash_wait_status;
  g_ps_hw6_owner_sm_probe.flash_scratch_dma_program_flash_poll_count =
    result->dma_program_flash_poll_count;
  g_ps_hw6_owner_sm_probe.flash_scratch_dma_read_status =
    result->dma_read_hal_status;
  g_ps_hw6_owner_sm_probe.flash_scratch_dma_read_transfer_wait_status =
    result->dma_read_transfer_wait_status;
  g_ps_hw6_owner_sm_probe.flash_scratch_dma_read_transfer_poll_count =
    result->dma_read_transfer_poll_count;
  g_ps_hw6_owner_sm_probe.flash_scratch_dma_verify_mismatch_count =
    result->dma_verify_mismatch_count;
  g_ps_hw6_owner_sm_probe.flash_scratch_dma_tx_state_after =
    result->dma_tx_state_after;
  g_ps_hw6_owner_sm_probe.flash_scratch_dma_tx_error_after =
    result->dma_tx_error_after;
  g_ps_hw6_owner_sm_probe.flash_scratch_dma_rx_state_after =
    result->dma_rx_state_after;
  g_ps_hw6_owner_sm_probe.flash_scratch_dma_rx_error_after =
    result->dma_rx_error_after;
  for (index = 0UL; index < 16UL; ++index)
  {
    g_ps_hw6_owner_sm_probe.flash_scratch_erase_blank_first16[index] =
      result->erase_blank_first16[index];
    g_ps_hw6_owner_sm_probe.flash_scratch_program_first16[index] =
      result->program_first16[index];
    g_ps_hw6_owner_sm_probe.flash_scratch_dma_first16[index] =
      result->dma_first16[index];
  }
  g_ps_hw6_owner_sm_probe.flash_scratch_cleanup_write_enable_status =
    result->cleanup_write_enable_hal_status;
  g_ps_hw6_owner_sm_probe.flash_scratch_cleanup_write_enable_status1 =
    result->cleanup_write_enable_status1;
  g_ps_hw6_owner_sm_probe.flash_scratch_cleanup_erase_status =
    result->cleanup_erase_hal_status;
  g_ps_hw6_owner_sm_probe.flash_scratch_cleanup_wait_status =
    result->cleanup_wait_status;
  g_ps_hw6_owner_sm_probe.flash_scratch_cleanup_poll_count =
    result->cleanup_poll_count;
  g_ps_hw6_owner_sm_probe.flash_scratch_cleanup_blank_read_status =
    result->cleanup_blank_read_hal_status;
  g_ps_hw6_owner_sm_probe.flash_scratch_cleanup_blank_mismatch_count =
    result->cleanup_blank_mismatch_count;
  for (index = 0UL; index < 16UL; ++index)
  {
    g_ps_hw6_owner_sm_probe.flash_scratch_cleanup_first16[index] =
      result->cleanup_first16[index];
  }
  g_ps_hw6_owner_sm_probe.flash_scratch_ospi_state_after =
    result->ospi_state_after;
  g_ps_hw6_owner_sm_probe.flash_scratch_ospi_error_after =
    result->ospi_error_after;
}

static void PS_HW6_SM_RecordFlashBlockResult(
  const ps_storage_flash_block_test_result_t *result)
{
  uint32_t index;

  if (result == NULL)
  {
    return;
  }

  g_ps_hw6_owner_sm_probe.flash_block_test_status =
    (uint32_t)result->status;
  g_ps_hw6_owner_sm_probe.flash_block_test_address = result->address;
  g_ps_hw6_owner_sm_probe.flash_block_test_index = result->block_index;
  g_ps_hw6_owner_sm_probe.flash_block_test_length = result->length;
  g_ps_hw6_owner_sm_probe.flash_block_geometry_total_size =
    result->geometry_total_size;
  g_ps_hw6_owner_sm_probe.flash_block_geometry_erase_size =
    result->geometry_erase_block_size;
  g_ps_hw6_owner_sm_probe.flash_block_geometry_page_size =
    result->geometry_program_page_size;
  g_ps_hw6_owner_sm_probe.flash_block_geometry_count =
    result->geometry_logical_block_count;
  g_ps_hw6_owner_sm_probe.flash_block_erase_status =
    result->erase_status;
  g_ps_hw6_owner_sm_probe.flash_block_erase_poll_count =
    result->erase_poll_count;
  g_ps_hw6_owner_sm_probe.flash_block_blank_read_status =
    result->blank_read_status;
  g_ps_hw6_owner_sm_probe.flash_block_blank_read_count =
    result->blank_read_count;
  g_ps_hw6_owner_sm_probe.flash_block_blank_mismatch_count =
    result->blank_mismatch_count;
  g_ps_hw6_owner_sm_probe.flash_block_program_status =
    result->program_status;
  g_ps_hw6_owner_sm_probe.flash_block_program_page_count =
    result->program_page_count;
  g_ps_hw6_owner_sm_probe.flash_block_program_last_poll_count =
    result->program_last_poll_count;
  g_ps_hw6_owner_sm_probe.flash_block_verify_read_status =
    result->verify_read_status;
  g_ps_hw6_owner_sm_probe.flash_block_verify_read_count =
    result->verify_read_count;
  g_ps_hw6_owner_sm_probe.flash_block_verify_mismatch_count =
    result->verify_mismatch_count;
  g_ps_hw6_owner_sm_probe.flash_block_cleanup_status =
    result->cleanup_status;
  g_ps_hw6_owner_sm_probe.flash_block_cleanup_poll_count =
    result->cleanup_poll_count;
  g_ps_hw6_owner_sm_probe.flash_block_cleanup_read_status =
    result->cleanup_read_status;
  g_ps_hw6_owner_sm_probe.flash_block_cleanup_mismatch_count =
    result->cleanup_mismatch_count;
  for (index = 0UL; index < 16UL; ++index)
  {
    g_ps_hw6_owner_sm_probe.flash_block_blank_first16[index] =
      result->blank_first16[index];
    g_ps_hw6_owner_sm_probe.flash_block_verify_first16[index] =
      result->verify_first16[index];
    g_ps_hw6_owner_sm_probe.flash_block_cleanup_first16[index] =
      result->cleanup_first16[index];
  }
  g_ps_hw6_owner_sm_probe.flash_block_ospi_state_after =
    result->ospi_state_after;
  g_ps_hw6_owner_sm_probe.flash_block_ospi_error_after =
    result->ospi_error_after;
}

static void PS_HW6_SM_RecordStorageLayoutResult(
  const ps_storage_layout_validation_t *result)
{
  if (result == NULL)
  {
    return;
  }

  g_ps_hw6_owner_sm_probe.storage_layout_api_version =
    result->api_version;
  g_ps_hw6_owner_sm_probe.storage_layout_validation_status =
    (uint32_t)result->status;
  g_ps_hw6_owner_sm_probe.storage_layout_region_count =
    result->region_count;
  g_ps_hw6_owner_sm_probe.storage_layout_total_size =
    result->total_size;
  g_ps_hw6_owner_sm_probe.storage_layout_erase_size =
    result->erase_block_size;
  g_ps_hw6_owner_sm_probe.storage_layout_end =
    result->layout_end;
  g_ps_hw6_owner_sm_probe.storage_layout_alignment_errors =
    result->alignment_error_count;
  g_ps_hw6_owner_sm_probe.storage_layout_overlap_errors =
    result->overlap_error_count;
  g_ps_hw6_owner_sm_probe.storage_layout_range_errors =
    result->range_error_count;
  g_ps_hw6_owner_sm_probe.storage_layout_host_exposed_mask =
    result->host_exposed_mask;
  g_ps_hw6_owner_sm_probe.storage_layout_protected_mask =
    result->protected_mask;
  g_ps_hw6_owner_sm_probe.storage_layout_scratch_index =
    result->scratch_region_index;
  g_ps_hw6_owner_sm_probe.storage_layout_scratch_start =
    result->scratch_start;
  g_ps_hw6_owner_sm_probe.storage_layout_scratch_length =
    result->scratch_length;
}

static void PS_HW6_SM_RecordStorageFxLxResult(
  const ps_storage_filex_levelx_smoke_result_t *result)
{
  uint32_t index;

  if (result == NULL)
  {
    return;
  }

  g_ps_hw6_owner_sm_probe.storage_fxlx_api_version = result->api_version;
  g_ps_hw6_owner_sm_probe.storage_fxlx_status = (uint32_t)result->status;
  g_ps_hw6_owner_sm_probe.storage_fxlx_region_id = result->region_id;
  g_ps_hw6_owner_sm_probe.storage_fxlx_region_start = result->region_start;
  g_ps_hw6_owner_sm_probe.storage_fxlx_region_length = result->region_length;
  g_ps_hw6_owner_sm_probe.storage_fxlx_test_start = result->test_start;
  g_ps_hw6_owner_sm_probe.storage_fxlx_test_length = result->test_length;
  g_ps_hw6_owner_sm_probe.storage_fxlx_erase_block_size =
    result->erase_block_size;
  g_ps_hw6_owner_sm_probe.storage_fxlx_sector_size =
    result->logical_sector_size;
  g_ps_hw6_owner_sm_probe.storage_fxlx_sector_count =
    result->logical_sector_count;
  g_ps_hw6_owner_sm_probe.storage_fxlx_preformat_erase_status =
    result->preformat_erase_status;
  g_ps_hw6_owner_sm_probe.storage_fxlx_preformat_erase_block_count =
    result->preformat_erase_block_count;
  g_ps_hw6_owner_sm_probe.storage_fxlx_preformat_erase_failed_block =
    result->preformat_erase_failed_block;
  g_ps_hw6_owner_sm_probe.storage_fxlx_preformat_erase_last_poll_count =
    result->preformat_erase_last_poll_count;
  g_ps_hw6_owner_sm_probe.storage_fxlx_lx_initialize_status =
    result->lx_initialize_status;
  g_ps_hw6_owner_sm_probe.storage_fxlx_lx_open_status =
    result->lx_open_status;
  g_ps_hw6_owner_sm_probe.storage_fxlx_fx_format_status =
    result->fx_format_status;
  g_ps_hw6_owner_sm_probe.storage_fxlx_fx_open_status =
    result->fx_open_status;
  g_ps_hw6_owner_sm_probe.storage_fxlx_file_create_status =
    result->file_create_status;
  g_ps_hw6_owner_sm_probe.storage_fxlx_file_open_status =
    result->file_open_status;
  g_ps_hw6_owner_sm_probe.storage_fxlx_file_write_status =
    result->file_write_status;
  g_ps_hw6_owner_sm_probe.storage_fxlx_file_seek_status =
    result->file_seek_status;
  g_ps_hw6_owner_sm_probe.storage_fxlx_file_read_status =
    result->file_read_status;
  g_ps_hw6_owner_sm_probe.storage_fxlx_file_close_status =
    result->file_close_status;
  g_ps_hw6_owner_sm_probe.storage_fxlx_fx_flush_status =
    result->fx_flush_status;
  g_ps_hw6_owner_sm_probe.storage_fxlx_fx_close_status =
    result->fx_close_status;
  g_ps_hw6_owner_sm_probe.storage_fxlx_lx_close_status =
    result->lx_close_status;
  g_ps_hw6_owner_sm_probe.storage_fxlx_bytes_written = result->bytes_written;
  g_ps_hw6_owner_sm_probe.storage_fxlx_bytes_read = result->bytes_read;
  g_ps_hw6_owner_sm_probe.storage_fxlx_verify_mismatch_count =
    result->verify_mismatch_count;
  for (index = 0UL; index < 16UL; ++index)
  {
    g_ps_hw6_owner_sm_probe.storage_fxlx_boot_read_first16[index] =
      result->boot_read_first16[index];
    g_ps_hw6_owner_sm_probe.storage_fxlx_read_first16[index] =
      result->read_first16[index];
  }
  g_ps_hw6_owner_sm_probe.storage_fxlx_boot_bytes_per_sector =
    result->boot_bytes_per_sector;
  g_ps_hw6_owner_sm_probe.storage_fxlx_boot_sectors_per_cluster =
    result->boot_sectors_per_cluster;
  g_ps_hw6_owner_sm_probe.storage_fxlx_boot_reserved_sectors =
    result->boot_reserved_sectors;
  g_ps_hw6_owner_sm_probe.storage_fxlx_boot_number_of_fats =
    result->boot_number_of_fats;
  g_ps_hw6_owner_sm_probe.storage_fxlx_boot_root_entries =
    result->boot_root_entries;
  g_ps_hw6_owner_sm_probe.storage_fxlx_boot_total_sectors =
    result->boot_total_sectors;
  g_ps_hw6_owner_sm_probe.storage_fxlx_boot_sectors_per_fat =
    result->boot_sectors_per_fat;
  g_ps_hw6_owner_sm_probe.storage_fxlx_boot_signature =
    result->boot_signature;
  g_ps_hw6_owner_sm_probe.storage_fxlx_lx_driver_read_count =
    result->lx_driver_read_count;
  g_ps_hw6_owner_sm_probe.storage_fxlx_lx_driver_write_count =
    result->lx_driver_write_count;
  g_ps_hw6_owner_sm_probe.storage_fxlx_lx_driver_erase_count =
    result->lx_driver_erase_count;
  g_ps_hw6_owner_sm_probe.storage_fxlx_lx_driver_verify_count =
    result->lx_driver_verify_count;
  g_ps_hw6_owner_sm_probe.storage_fxlx_lx_driver_last_status =
    result->lx_driver_last_status;
  g_ps_hw6_owner_sm_probe.storage_fxlx_fx_driver_read_count =
    result->fx_driver_read_count;
  g_ps_hw6_owner_sm_probe.storage_fxlx_fx_driver_write_count =
    result->fx_driver_write_count;
  g_ps_hw6_owner_sm_probe.storage_fxlx_fx_driver_flush_count =
    result->fx_driver_flush_count;
  g_ps_hw6_owner_sm_probe.storage_fxlx_fx_driver_abort_count =
    result->fx_driver_abort_count;
  g_ps_hw6_owner_sm_probe.storage_fxlx_fx_driver_init_count =
    result->fx_driver_init_count;
  g_ps_hw6_owner_sm_probe.storage_fxlx_fx_driver_uninit_count =
    result->fx_driver_uninit_count;
  g_ps_hw6_owner_sm_probe.storage_fxlx_fx_driver_release_count =
    result->fx_driver_release_count;
  g_ps_hw6_owner_sm_probe.storage_fxlx_fx_driver_last_request =
    result->fx_driver_last_request;
  g_ps_hw6_owner_sm_probe.storage_fxlx_fx_driver_last_status =
    result->fx_driver_last_status;
}

static const ps_storage_region_t *PS_HW6_SM_FindStorageRegion(
  ps_storage_region_id_t id)
{
  uint32_t count;
  uint32_t index;
  const ps_storage_region_t *regions = ps_storage_layout_regions(&count);

  for (index = 0UL; index < count; ++index)
  {
    if (regions[index].id == id)
    {
      return &regions[index];
    }
  }
  return NULL;
}

static HAL_StatusTypeDef PS_HW6_SM_StabilizePower(void)
{
  HAL_StatusTypeDef mr_shipping_status;
  HAL_StatusTypeDef charger_profile_status;
  HAL_StatusTypeDef interrupt_config_status;
  HAL_StatusTypeDef fuel_gauge_status;
  HAL_StatusTypeDef snapshot_status;
  HAL_StatusTypeDef status;

  (void)PS_HW6_SM_Transition(PS_HW6_SM_POWER, PWR_EV_BOOT, HAL_OK);
  (void)PS_HW6_SM_Transition(PS_HW6_SM_PMIC,
                            PMIC_EV_PROBE_REQUEST, HAL_OK);
  mr_shipping_status = PS_HW6_PowerOwner_EnableMrShippingMode();
  charger_profile_status = PS_HW6_PowerOwner_ConfigureChargerProfile();
  interrupt_config_status = PS_HW6_PowerOwner_ConfigurePmicInterrupts();
  fuel_gauge_status = PS_HW6_PowerOwner_PrepareFuelGauge();
  snapshot_status = PS_HW6_PowerOwner_RunSnapshot();
  PS_HW6_SM_UpdateUsbHostAvailability(
    (uint32_t)PS_HW6_USB_HOST_EVENT_POWER_SNAPSHOT);
  status = ((mr_shipping_status == HAL_OK) &&
            (charger_profile_status == HAL_OK) &&
            (interrupt_config_status == HAL_OK) &&
            (fuel_gauge_status == HAL_OK) &&
            (snapshot_status == HAL_OK)) ? HAL_OK : HAL_ERROR;
  if (status == HAL_OK)
  {
    (void)PS_HW6_SM_Transition(PS_HW6_SM_PMIC,
                              PMIC_EV_PROBE_OK, status);
    (void)PS_HW6_SM_Transition(PS_HW6_SM_POWER,
                              PWR_EV_RAILS_OK, status);
  }
  else
  {
    (void)PS_HW6_SM_Transition(PS_HW6_SM_PMIC,
                              PMIC_EV_PROBE_FAIL, status);
    (void)PS_HW6_SM_Transition(PS_HW6_SM_POWER,
                              PWR_EV_RAILS_FAIL, status);
  }
  (void)PS_HW6_SM_EvaluateBatteryPolicy(snapshot_status, 1UL);
  return status;
}

static HAL_StatusTypeDef PS_HW6_SM_StabilizeDisplay(void)
{
  HAL_StatusTypeDef status;

  (void)PS_HW6_SM_Transition(PS_HW6_SM_DISPLAY,
                            DISP_EV_ENABLE_REQ, HAL_OK);
  status = PS_HW6_DisplayOwner_RunPattern();
  if (status == HAL_OK)
  {
    (void)PS_HW6_SM_Transition(PS_HW6_SM_DISPLAY,
                              DISP_EV_INIT_OK, status);
  }
  else
  {
    (void)PS_HW6_SM_Transition(PS_HW6_SM_DISPLAY,
                              DISP_EV_FAULT, status);
  }
  return status;
}

static HAL_StatusTypeDef PS_HW6_SM_RunAudioTone(void)
{
  HAL_StatusTypeDef status;

  (void)PS_HW6_SM_Transition(PS_HW6_SM_SPEAKER,
                            SPK_EV_ENABLE_REQUEST, HAL_OK);
  (void)PS_HW6_SM_Transition(PS_HW6_SM_SPEAKER,
                            SPK_EV_ENABLED, HAL_OK);
  (void)PS_HW6_SM_Transition(PS_HW6_SM_SPEAKER,
                            SPK_EV_PRELOAD, HAL_OK);
  (void)PS_HW6_SM_Transition(PS_HW6_SM_AUDIO,
                            AUDIO_EV_TONE_REQ, HAL_OK);
  (void)PS_HW6_SM_Transition(PS_HW6_SM_SPEAKER,
                            SPK_EV_DMA_START_OK, HAL_OK);

  status = PS_HW6_AudioOwner_RunTone();
  if (status == HAL_OK)
  {
    (void)PS_HW6_SM_Transition(PS_HW6_SM_SPEAKER,
                              SPK_EV_PLAYBACK_DONE, status);
    (void)PS_HW6_SM_Transition(PS_HW6_SM_SPEAKER,
                              SPK_EV_DRAINED, status);
    (void)PS_HW6_SM_Transition(PS_HW6_SM_AUDIO,
                              AUDIO_EV_PLAYBACK_DONE, status);
  }
  else
  {
    (void)PS_HW6_SM_Transition(PS_HW6_SM_SPEAKER,
                              SPK_EV_FAULT, status);
    (void)PS_HW6_SM_Transition(PS_HW6_SM_AUDIO,
                              AUDIO_EV_DMA_ERROR, status);
  }
  return status;
}

static HAL_StatusTypeDef PS_HW6_SM_StabilizeAudio(void)
{
  uint32_t audio_state =
    g_ps_hw6_owner_sm_probe.current_state[PS_HW6_SM_AUDIO];

  if (audio_state == (uint32_t)AUDIO_OFF)
  {
    if (PS_HW6_SM_Transition(PS_HW6_SM_AUDIO,
                             AUDIO_EV_INIT_REQ, HAL_OK) != HAL_OK)
    {
      return HAL_ERROR;
    }
    if (PS_HW6_SM_Transition(PS_HW6_SM_AUDIO,
                             AUDIO_EV_INIT_OK, HAL_OK) != HAL_OK)
    {
      return HAL_ERROR;
    }
  }
  else if (audio_state != (uint32_t)AUDIO_IDLE)
  {
    return HAL_ERROR;
  }

  if (g_ps_hw6_owner_sm_probe.current_state[PS_HW6_SM_SPEAKER] !=
      (uint32_t)SPK_OFF)
  {
    return HAL_ERROR;
  }
  return PS_HW6_AudioOwner_VerifyIdle();
}

static HAL_StatusTypeDef PS_HW6_SM_StabilizeJoystick(void)
{
  ps_dev_tmag3001_stabilize_result_t result;
  ps_status_t driver_status;
  HAL_StatusTypeDef status;
  uint32_t i2c_state_after = 0UL;
  uint32_t i2c_error_after = 0UL;

  (void)PS_HW6_SM_Transition(PS_HW6_SM_JOYSTICK,
                            JOY_EV_ENABLE_REQUEST, HAL_OK);
  driver_status = ps_dev_tmag3001_stabilize_suspended(
    &ps_joystick_device,
    &result);
  status = PS_HW6_SM_StatusToHal(driver_status);

  g_ps_hw6_owner_sm_probe.joystick_ready_status =
    (uint32_t)result.ready_status;
  g_ps_hw6_owner_sm_probe.joystick_identity_status =
    (uint32_t)result.identity_status;
  g_ps_hw6_owner_sm_probe.joystick_device_id = result.device_id;
  g_ps_hw6_owner_sm_probe.joystick_manufacturer_lsb =
    result.manufacturer_lsb;
  g_ps_hw6_owner_sm_probe.joystick_manufacturer_msb =
    result.manufacturer_msb;
  g_ps_hw6_owner_sm_probe.joystick_identity_match =
    result.identity_match;
  g_ps_hw6_owner_sm_probe.joystick_sensor_config1_before =
    result.sensor_config1_before;
  g_ps_hw6_owner_sm_probe.joystick_sensor_config1_after =
    result.sensor_config1_after;
  g_ps_hw6_owner_sm_probe.joystick_device_config2_before =
    result.device_config2_before;
  g_ps_hw6_owner_sm_probe.joystick_device_config2_after =
    result.device_config2_after;
  g_ps_hw6_owner_sm_probe.joystick_device_config2_sleep =
    result.device_config2_sleep;
  g_ps_hw6_owner_sm_probe.joystick_write_ok_mask =
    result.write_ok_mask;
  g_ps_hw6_owner_sm_probe.joystick_verify_ok_mask =
    result.verify_ok_mask;
  g_ps_hw6_owner_sm_probe.joystick_sensor_config1_verify_status =
    (uint32_t)result.sensor_config1_verify_status;
  g_ps_hw6_owner_sm_probe.joystick_device_config2_verify_status =
    (uint32_t)result.device_config2_verify_status;
  g_ps_hw6_owner_sm_probe.joystick_sleep_write_status =
    (uint32_t)result.sleep_write_status;
  g_ps_hw6_owner_sm_probe.joystick_terminal_sleep_committed =
    result.terminal_sleep_committed;
  g_ps_hw6_owner_sm_probe.joystick_post_sleep_read_omitted =
    result.post_sleep_read_omitted;
  (void)ps_hw_i2c3_diagnostics(&i2c_state_after, &i2c_error_after);
  g_ps_hw6_owner_sm_probe.joystick_i2c_state_after = i2c_state_after;
  g_ps_hw6_owner_sm_probe.joystick_i2c_error_after = i2c_error_after;
  PS_HW6_SM_UpdateJoystickDriverProbe();
  if (status == HAL_OK)
  {
    (void)PS_HW6_SM_Transition(PS_HW6_SM_JOYSTICK,
                              JOY_EV_PROBE_OK, HAL_OK);
    (void)PS_HW6_SM_Transition(PS_HW6_SM_JOYSTICK,
                              JOY_EV_CONFIG_OK, status);
  }
  else
  {
    (void)PS_HW6_SM_Transition(PS_HW6_SM_JOYSTICK,
                              JOY_EV_I2C_ERROR, status);
  }

  return status;
}

static void PS_HW6_SM_RecordImuStabilizeResult(
  const ps_dev_lis2dux12_stabilize_result_t *result)
{
  uint32_t index;

  g_ps_hw6_owner_sm_probe.imu_ready_status = result->whoami_hal_status;
  g_ps_hw6_owner_sm_probe.imu_whoami_status = result->whoami_hal_status;
  g_ps_hw6_owner_sm_probe.imu_whoami = result->whoami;
  g_ps_hw6_owner_sm_probe.imu_identity_match = result->identity_match;
  for (index = 0U; index < PS_HW6_OWNER_SM_IMU_REGISTER_COUNT; ++index)
  {
    g_ps_hw6_owner_sm_probe.imu_register_address[index] =
      result->register_address[index];
    g_ps_hw6_owner_sm_probe.imu_register_before[index] =
      result->register_before[index];
    g_ps_hw6_owner_sm_probe.imu_register_after[index] =
      result->register_after[index];
  }
  g_ps_hw6_owner_sm_probe.imu_snapshot_ok_mask = result->snapshot_ok_mask;
  g_ps_hw6_owner_sm_probe.imu_write_ok_mask = result->write_ok_mask;
  g_ps_hw6_owner_sm_probe.imu_verify_ok_mask = result->verify_ok_mask;
  g_ps_hw6_owner_sm_probe.imu_deep_power_down_value =
    result->deep_power_down_value;
  g_ps_hw6_owner_sm_probe.imu_deep_power_down_write_status =
    (uint32_t)result->deep_power_down_status;
  g_ps_hw6_owner_sm_probe.imu_terminal_deep_power_down_committed =
    result->terminal_deep_power_down_committed;
  g_ps_hw6_owner_sm_probe.imu_post_deep_power_down_read_omitted =
    result->post_deep_power_down_read_omitted;
  g_ps_hw6_owner_sm_probe.imu_i2c_state_after =
    (uint32_t)HAL_I2C_GetState(&hi2c3);
  g_ps_hw6_owner_sm_probe.imu_i2c_error_after =
    HAL_I2C_GetError(&hi2c3);
}

static void PS_HW6_SM_ClearImuDeepPowerDownProof(void)
{
  g_ps_hw6_owner_sm_probe.imu_deep_power_down_write_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.imu_terminal_deep_power_down_committed = 0UL;
  g_ps_hw6_owner_sm_probe.imu_post_deep_power_down_read_omitted = 0UL;
}

static void PS_HW6_SM_ClearJoystickTerminalSleepProof(void)
{
  g_ps_hw6_owner_sm_probe.joystick_sleep_write_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.joystick_terminal_sleep_committed = 0UL;
  g_ps_hw6_owner_sm_probe.joystick_post_sleep_read_omitted = 0UL;
}

static uint32_t PS_HW6_SM_JoystickTerminalSleepProofValid(void)
{
  return ((g_ps_hw6_owner_sm_probe.joystick_sleep_write_status ==
           (uint32_t)PS_STATUS_OK) &&
          (g_ps_hw6_owner_sm_probe.joystick_terminal_sleep_committed !=
           0UL) &&
          (g_ps_hw6_owner_sm_probe.joystick_post_sleep_read_omitted !=
           0UL) &&
          (g_ps_hw6_owner_sm_probe.joystick_sleep_audit_int_config1_target ==
           PS_HW6_TMAG_STOP2_INT_CONFIG1_TARGET) &&
          (g_ps_hw6_owner_sm_probe.joystick_sleep_audit_int_config1_after ==
           PS_HW6_TMAG_STOP2_INT_CONFIG1_TARGET)) ? 1UL : 0UL;
}

static uint32_t PS_HW6_SM_ImuDeepPowerDownProofValid(void)
{
  return ((g_ps_hw6_owner_sm_probe.imu_deep_power_down_write_status ==
           (uint32_t)PS_STATUS_OK) &&
          (g_ps_hw6_owner_sm_probe.imu_terminal_deep_power_down_committed !=
           0UL) &&
          (g_ps_hw6_owner_sm_probe.imu_post_deep_power_down_read_omitted !=
           0UL)) ? 1UL : 0UL;
}

static void PS_HW6_SM_RecordImuSuspendResult(
  const ps_dev_lis2dux12_suspend_result_t *result)
{
  g_ps_hw6_owner_sm_probe.imu_deep_power_down_write_status =
    (uint32_t)result->deep_power_down_status;
  g_ps_hw6_owner_sm_probe.imu_terminal_deep_power_down_committed =
    result->terminal_deep_power_down_committed;
  g_ps_hw6_owner_sm_probe.imu_post_deep_power_down_read_omitted =
    result->post_deep_power_down_read_omitted;
  g_ps_hw6_owner_sm_probe.imu_i2c_state_after =
    (uint32_t)HAL_I2C_GetState(&hi2c3);
  g_ps_hw6_owner_sm_probe.imu_i2c_error_after =
    HAL_I2C_GetError(&hi2c3);
}

static HAL_StatusTypeDef PS_HW6_SM_StabilizeImu(void)
{
  ps_dev_lis2dux12_stabilize_result_t result;
  ps_status_t driver_status;
  HAL_StatusTypeDef status;

  (void)PS_HW6_SM_Transition(PS_HW6_SM_IMU,
                            IMU_EV_ENABLE_REQUEST, HAL_OK);
  driver_status = ps_dev_lis2dux12_stabilize_suspended(
    &ps_imu_device,
    &result);
  status = PS_HW6_SM_StatusToHal(driver_status);

  PS_HW6_SM_RecordImuStabilizeResult(&result);
  PS_HW6_SM_UpdateImuDriverProbe();

  if (result.identity_match != 0UL)
  {
    (void)PS_HW6_SM_Transition(PS_HW6_SM_IMU,
                              IMU_EV_PROBE_OK, HAL_OK);
  }
  if (status == HAL_OK)
  {
    (void)PS_HW6_SM_Transition(PS_HW6_SM_IMU,
                              IMU_EV_CONFIG_OK, status);
  }
  else
  {
    (void)PS_HW6_SM_Transition(PS_HW6_SM_IMU,
                              IMU_EV_I2C_ERROR, status);
  }
  return status;
}

static void PS_HW6_SM_RecordJoystickXyzCaptureSample(
  uint32_t sample_index,
  uint32_t capture_start_tick,
  uint32_t capture_mode,
  const ps_dev_tmag3001_raw_sample_t *sample)
{
  volatile PS_HW6_JoystickXyzCaptureRecord *record;
  int32_t delta_z;
  uint32_t abs_delta_z;
  uint32_t now_tick;

  if ((sample == (const ps_dev_tmag3001_raw_sample_t *)0) ||
      (sample_index >= (uint32_t)PS_HW6_JOYSTICK_XYZ_CAPTURE_CAPACITY))
  {
    return;
  }

  now_tick = (uint32_t)tx_time_get();
  record = &g_ps_hw6_joystick_xyz_capture_buffer[sample_index];
  record->index = sample_index;
  record->tick = now_tick;
  record->delta_tick = now_tick - capture_start_tick;
  record->mode = capture_mode;
  record->x = sample->x;
  record->y = sample->y;
  record->z = sample->z;
  record->reserved =
    (uint16_t)g_ps_hw6_owner_sm_probe.joystick_xyz_capture_sensor_config2_active;
  record->conv_status = (uint32_t)sample->conv_status;
  record->read_status = (uint32_t)sample->status;

  g_ps_hw6_owner_sm_probe.joystick_xyz_capture_count = sample_index + 1UL;
  g_ps_hw6_owner_sm_probe.joystick_xyz_capture_last_x = (int32_t)sample->x;
  g_ps_hw6_owner_sm_probe.joystick_xyz_capture_last_y = (int32_t)sample->y;
  g_ps_hw6_owner_sm_probe.joystick_xyz_capture_last_z = (int32_t)sample->z;
  g_ps_hw6_owner_sm_probe.joystick_xyz_capture_last_read_status =
    (uint32_t)sample->status;
  g_ps_hw6_owner_sm_probe.joystick_xyz_capture_last_conv_status =
    (uint32_t)sample->conv_status;

  if (sample->status != PS_STATUS_OK)
  {
    g_ps_hw6_owner_sm_probe.joystick_xyz_capture_error_count++;
    return;
  }

  if (g_ps_hw6_owner_sm_probe.joystick_xyz_capture_success_count == 0UL)
  {
    g_ps_hw6_owner_sm_probe.joystick_xyz_capture_first_x =
      (int32_t)sample->x;
    g_ps_hw6_owner_sm_probe.joystick_xyz_capture_first_y =
      (int32_t)sample->y;
    g_ps_hw6_owner_sm_probe.joystick_xyz_capture_first_z =
      (int32_t)sample->z;
    g_ps_hw6_owner_sm_probe.joystick_xyz_capture_min_x =
      (int32_t)sample->x;
    g_ps_hw6_owner_sm_probe.joystick_xyz_capture_min_y =
      (int32_t)sample->y;
    g_ps_hw6_owner_sm_probe.joystick_xyz_capture_min_z =
      (int32_t)sample->z;
    g_ps_hw6_owner_sm_probe.joystick_xyz_capture_max_x =
      (int32_t)sample->x;
    g_ps_hw6_owner_sm_probe.joystick_xyz_capture_max_y =
      (int32_t)sample->y;
    g_ps_hw6_owner_sm_probe.joystick_xyz_capture_max_z =
      (int32_t)sample->z;
  }
  else
  {
    if ((int32_t)sample->x <
        g_ps_hw6_owner_sm_probe.joystick_xyz_capture_min_x)
    {
      g_ps_hw6_owner_sm_probe.joystick_xyz_capture_min_x =
        (int32_t)sample->x;
    }
    if ((int32_t)sample->y <
        g_ps_hw6_owner_sm_probe.joystick_xyz_capture_min_y)
    {
      g_ps_hw6_owner_sm_probe.joystick_xyz_capture_min_y =
        (int32_t)sample->y;
    }
    if ((int32_t)sample->z <
        g_ps_hw6_owner_sm_probe.joystick_xyz_capture_min_z)
    {
      g_ps_hw6_owner_sm_probe.joystick_xyz_capture_min_z =
        (int32_t)sample->z;
    }
    if ((int32_t)sample->x >
        g_ps_hw6_owner_sm_probe.joystick_xyz_capture_max_x)
    {
      g_ps_hw6_owner_sm_probe.joystick_xyz_capture_max_x =
        (int32_t)sample->x;
    }
    if ((int32_t)sample->y >
        g_ps_hw6_owner_sm_probe.joystick_xyz_capture_max_y)
    {
      g_ps_hw6_owner_sm_probe.joystick_xyz_capture_max_y =
        (int32_t)sample->y;
    }
    if ((int32_t)sample->z >
        g_ps_hw6_owner_sm_probe.joystick_xyz_capture_max_z)
    {
      g_ps_hw6_owner_sm_probe.joystick_xyz_capture_max_z =
        (int32_t)sample->z;
    }
  }

  delta_z = (int32_t)sample->z -
    g_ps_hw6_owner_sm_probe.joystick_xyz_capture_first_z;
  abs_delta_z = (delta_z < 0) ? (uint32_t)(-delta_z) : (uint32_t)delta_z;
  if (abs_delta_z >
      g_ps_hw6_owner_sm_probe.joystick_xyz_capture_max_abs_delta_z)
  {
    g_ps_hw6_owner_sm_probe.joystick_xyz_capture_max_abs_delta_z =
      abs_delta_z;
  }
  g_ps_hw6_owner_sm_probe.joystick_xyz_capture_success_count++;
}

static uint32_t PS_HW6_SM_JoystickXyzRequestedSamples(uint32_t capture_mode)
{
  uint32_t requested_samples;

  if (capture_mode == PS_HW6_JOYSTICK_XYZ_CAPTURE_REST)
  {
    requested_samples = (uint32_t)KNOB_INPUT_JOYSTICK_XYZ_REST_SAMPLES;
  }
  else if ((capture_mode == PS_HW6_JOYSTICK_XYZ_CAPTURE_SWEEP) ||
           (capture_mode == PS_HW6_JOYSTICK_XYZ_CAPTURE_SWEEP_Z_HIGH))
  {
    requested_samples = (uint32_t)KNOB_INPUT_JOYSTICK_XYZ_SWEEP_SAMPLES;
  }
  else
  {
    requested_samples = 0UL;
  }

  if (requested_samples > (uint32_t)PS_HW6_JOYSTICK_XYZ_CAPTURE_CAPACITY)
  {
    requested_samples = (uint32_t)PS_HW6_JOYSTICK_XYZ_CAPTURE_CAPACITY;
  }
  if (requested_samples == 0UL)
  {
    requested_samples = 1UL;
  }

  return requested_samples;
}

HAL_StatusTypeDef PS_HW6_OwnerStateMachines_RunJoystickXyzCapture(
  uint32_t capture_mode)
{
  ps_dev_tmag3001_stabilize_result_t stabilize_result;
  ps_dev_tmag3001_wake_result_t wake_result;
  ps_dev_tmag3001_raw_sample_t sample;
  ps_dev_tmag3001_suspend_result_t suspend_result;
  ps_status_t driver_status;
  HAL_StatusTypeDef status;
  uint32_t capture_start_tick;
  uint32_t sample_index;
  uint32_t period_ticks;
  uint32_t requested_samples;
  uint32_t timeout_ticks;
  uint32_t elapsed_ticks;
  uint8_t range_override_mask = 0U;
  uint8_t range_override_value = 0U;
  uint8_t sensor_config2_restore = 0U;

  (void)memset(&wake_result, 0, sizeof(wake_result));
  g_ps_hw6_owner_sm_probe.joystick_xyz_capture_request_count++;
  PS_HW6_SM_ResetJoystickXyzCaptureProbe();
  g_ps_hw6_owner_sm_probe.joystick_xyz_capture_start_tick =
    (uint32_t)tx_time_get();
  g_ps_hw6_owner_sm_probe.joystick_xyz_capture_mode = capture_mode;
  requested_samples = PS_HW6_SM_JoystickXyzRequestedSamples(capture_mode);
  g_ps_hw6_owner_sm_probe.joystick_xyz_capture_requested_samples =
    requested_samples;
  g_ps_hw6_owner_sm_probe.joystick_xyz_capture_capacity =
    (uint32_t)PS_HW6_JOYSTICK_XYZ_CAPTURE_CAPACITY;
  period_ticks = PS_HW6_SM_MsToTicks(
    (uint32_t)KNOB_INPUT_JOYSTICK_XYZ_CAPTURE_PERIOD_MS);
  g_ps_hw6_owner_sm_probe.joystick_xyz_capture_period_ticks = period_ticks;
  timeout_ticks = PS_HW6_SM_MsToTicks(
    (uint32_t)KNOB_INPUT_JOYSTICK_XYZ_CAPTURE_TIMEOUT_MS);
  g_ps_hw6_owner_sm_probe.joystick_xyz_capture_timeout_ticks = timeout_ticks;
  if (capture_mode == PS_HW6_JOYSTICK_XYZ_CAPTURE_SWEEP_Z_HIGH)
  {
    range_override_mask = PS_DEV_TMAG3001_SENSOR_CONFIG2_Z_RANGE_MASK;
    range_override_value = PS_DEV_TMAG3001_SENSOR_CONFIG2_Z_HIGH_RANGE;
  }
  g_ps_hw6_owner_sm_probe.joystick_xyz_capture_range_override_mask =
    (uint32_t)range_override_mask;
  g_ps_hw6_owner_sm_probe.joystick_xyz_capture_range_override_value =
    (uint32_t)range_override_value;

  if ((capture_mode != PS_HW6_JOYSTICK_XYZ_CAPTURE_REST) &&
      (capture_mode != PS_HW6_JOYSTICK_XYZ_CAPTURE_SWEEP) &&
      (capture_mode != PS_HW6_JOYSTICK_XYZ_CAPTURE_SWEEP_Z_HIGH))
  {
    status = HAL_ERROR;
    g_ps_hw6_owner_sm_probe.joystick_xyz_capture_status =
      (uint32_t)status;
    g_ps_hw6_owner_sm_probe.joystick_xyz_capture_end_tick =
      (uint32_t)tx_time_get();
    return status;
  }

  status = HAL_OK;
  if (ps_joystick_device.state == PS_DEV_TMAG3001_STATE_READY)
  {
    driver_status = ps_dev_tmag3001_stabilize_suspended(
      &ps_joystick_device,
      &stabilize_result);
    g_ps_hw6_owner_sm_probe.joystick_xyz_capture_stabilize_status =
      (uint32_t)driver_status;
    status = PS_HW6_SM_StatusToHal(driver_status);
  }

  if (status == HAL_OK)
  {
    PS_HW6_SM_ClearJoystickTerminalSleepProof();
    driver_status = ps_dev_tmag3001_wake_continuous_with_range(
      &ps_joystick_device,
      range_override_mask,
      range_override_value,
      &wake_result);
    g_ps_hw6_owner_sm_probe.joystick_xyz_capture_wake_status =
      (uint32_t)driver_status;
    g_ps_hw6_owner_sm_probe.joystick_xyz_capture_sensor_config2_status =
      (uint32_t)wake_result.sensor_config2_status;
    g_ps_hw6_owner_sm_probe.joystick_xyz_capture_sensor_config2_before =
      (uint32_t)wake_result.sensor_config2_before;
    g_ps_hw6_owner_sm_probe.joystick_xyz_capture_sensor_config2_active =
      (uint32_t)wake_result.active_sensor_config2;
    g_ps_hw6_owner_sm_probe.joystick_xyz_capture_range_override_applied =
      wake_result.range_override_applied;
    status = PS_HW6_SM_StatusToHal(driver_status);
  }

  if (status == HAL_OK)
  {
    (void)PS_HW6_SM_Transition(PS_HW6_SM_JOYSTICK,
                               JOY_EV_FAST_POLL_REQUEST,
                               HAL_OK);
  }

  capture_start_tick = (uint32_t)tx_time_get();
  sample_index = 0UL;
  while ((status == HAL_OK) &&
         (sample_index < requested_samples))
  {
    if (timeout_ticks != 0UL)
    {
      elapsed_ticks = (uint32_t)((uint32_t)tx_time_get() -
                                capture_start_tick);
      if (elapsed_ticks >= timeout_ticks)
      {
        g_ps_hw6_owner_sm_probe.joystick_xyz_capture_timeout_count++;
        status = HAL_ERROR;
        break;
      }
    }

    driver_status = ps_dev_tmag3001_read_raw_sample(
      &ps_joystick_device,
      &sample);
    PS_HW6_SM_RecordJoystickXyzCaptureSample(sample_index,
                                             capture_start_tick,
                                             capture_mode,
                                             &sample);
    status = PS_HW6_SM_StatusToHal(driver_status);
    sample_index++;
    if ((status == HAL_OK) &&
        (sample_index < requested_samples))
    {
      tx_thread_sleep(period_ticks);
    }
  }

  if ((status == HAL_OK) &&
      (g_ps_hw6_owner_sm_probe.joystick_xyz_capture_success_count == 0UL))
  {
    status = HAL_ERROR;
  }

  if ((ps_joystick_device.state == PS_DEV_TMAG3001_STATE_ACTIVE) &&
      (wake_result.range_override_applied != 0UL))
  {
    driver_status = ps_dev_tmag3001_set_sensor_config2(
      &ps_joystick_device,
      wake_result.sensor_config2_before,
      &sensor_config2_restore);
    g_ps_hw6_owner_sm_probe.joystick_xyz_capture_sensor_config2_restore_status =
      (uint32_t)driver_status;
    g_ps_hw6_owner_sm_probe.joystick_xyz_capture_sensor_config2_restore =
      (uint32_t)sensor_config2_restore;
    if (status == HAL_OK)
    {
      status = PS_HW6_SM_StatusToHal(driver_status);
    }
  }

  if (ps_joystick_device.state == PS_DEV_TMAG3001_STATE_ACTIVE)
  {
    driver_status = ps_dev_tmag3001_suspend(
      &ps_joystick_device,
      &suspend_result);
    g_ps_hw6_owner_sm_probe.joystick_xyz_capture_sleep_status =
      (uint32_t)suspend_result.sleep_status;
    if (status == HAL_OK)
    {
      status = PS_HW6_SM_StatusToHal(driver_status);
    }
  }

  if (status == HAL_OK)
  {
    (void)PS_HW6_SM_Transition(PS_HW6_SM_JOYSTICK,
                               JOY_EV_QUIESCE,
                               HAL_OK);
  }
  else
  {
    (void)PS_HW6_SM_Transition(PS_HW6_SM_JOYSTICK,
                               JOY_EV_I2C_ERROR,
                               status);
  }

  g_ps_hw6_owner_sm_probe.joystick_xyz_capture_status = (uint32_t)status;
  g_ps_hw6_owner_sm_probe.joystick_xyz_capture_end_tick =
    (uint32_t)tx_time_get();
  PS_HW6_SM_UpdateJoystickDriverProbe();
  PS_HW6_SM_UpdateJoystickInputProbe();
  return status;
}

HAL_StatusTypeDef PS_HW6_OwnerStateMachines_RunJoystickSleepAudit(void)
{
  ps_dev_tmag3001_sleep_audit_result_t result;
  ps_status_t driver_status;
  HAL_StatusTypeDef status;
  uint32_t i2c_state_after = 0UL;
  uint32_t i2c_error_after = 0UL;

  g_ps_hw6_owner_sm_probe.joystick_sleep_audit_request_count++;
  PS_HW6_SM_ResetJoystickSleepAuditProbe();
  g_ps_hw6_owner_sm_probe.joystick_sleep_audit_start_tick =
    (uint32_t)tx_time_get();

  driver_status = ps_dev_tmag3001_prepare_sleep_audit(
    &ps_joystick_device,
    &result);
  status = PS_HW6_SM_StatusToHal(driver_status);

  g_ps_hw6_owner_sm_probe.joystick_sleep_audit_ready_status =
    (uint32_t)result.ready_status;
  g_ps_hw6_owner_sm_probe.joystick_sleep_audit_identity_status =
    (uint32_t)result.identity_status;
  g_ps_hw6_owner_sm_probe.joystick_sleep_audit_device_id = result.device_id;
  g_ps_hw6_owner_sm_probe.joystick_sleep_audit_manufacturer_lsb =
    result.manufacturer_lsb;
  g_ps_hw6_owner_sm_probe.joystick_sleep_audit_manufacturer_msb =
    result.manufacturer_msb;
  g_ps_hw6_owner_sm_probe.joystick_sleep_audit_identity_match =
    result.identity_match;
  g_ps_hw6_owner_sm_probe.joystick_sleep_audit_sensor_config1_before =
    result.sensor_config1_before;
  g_ps_hw6_owner_sm_probe.joystick_sleep_audit_sensor_config1_after =
    result.sensor_config1_after;
  g_ps_hw6_owner_sm_probe.joystick_sleep_audit_int_config1_before =
    result.int_config1_before;
  g_ps_hw6_owner_sm_probe.joystick_sleep_audit_int_config1_target =
    result.int_config1_target;
  g_ps_hw6_owner_sm_probe.joystick_sleep_audit_int_config1_after =
    result.int_config1_after;
  g_ps_hw6_owner_sm_probe.joystick_sleep_audit_device_config2_before =
    result.device_config2_before;
  g_ps_hw6_owner_sm_probe.joystick_sleep_audit_device_config2_after =
    result.device_config2_after;
  g_ps_hw6_owner_sm_probe.joystick_sleep_audit_device_config2_sleep =
    result.device_config2_sleep;
  g_ps_hw6_owner_sm_probe.joystick_sleep_audit_write_ok_mask =
    result.write_ok_mask;
  g_ps_hw6_owner_sm_probe.joystick_sleep_audit_verify_ok_mask =
    result.verify_ok_mask;
  g_ps_hw6_owner_sm_probe.joystick_sleep_audit_sensor_config1_verify_status =
    (uint32_t)result.sensor_config1_verify_status;
  g_ps_hw6_owner_sm_probe.joystick_sleep_audit_int_config1_verify_status =
    (uint32_t)result.int_config1_verify_status;
  g_ps_hw6_owner_sm_probe.joystick_sleep_audit_device_config2_verify_status =
    (uint32_t)result.device_config2_verify_status;
  g_ps_hw6_owner_sm_probe.joystick_sleep_audit_sleep_write_status =
    (uint32_t)result.sleep_write_status;
  g_ps_hw6_owner_sm_probe.joystick_sleep_audit_terminal_sleep_committed =
    result.terminal_sleep_committed;
  g_ps_hw6_owner_sm_probe.joystick_sleep_audit_post_sleep_read_omitted =
    result.post_sleep_read_omitted;
  g_ps_hw6_owner_sm_probe.joystick_sleep_audit_last_hal_status =
    result.last_hal_status;
  g_ps_hw6_owner_sm_probe.joystick_sleep_audit_last_hal_error =
    result.last_hal_error;
  (void)ps_hw_i2c3_diagnostics(&i2c_state_after, &i2c_error_after);
  g_ps_hw6_owner_sm_probe.joystick_sleep_audit_i2c_state_after =
    i2c_state_after;
  g_ps_hw6_owner_sm_probe.joystick_sleep_audit_i2c_error_after =
    i2c_error_after;
  g_ps_hw6_owner_sm_probe.joystick_sleep_audit_status = (uint32_t)status;
  g_ps_hw6_owner_sm_probe.joystick_sleep_audit_end_tick =
    (uint32_t)tx_time_get();
  PS_HW6_SM_UpdateJoystickDriverProbe();
  return status;
}

HAL_StatusTypeDef PS_HW6_OwnerStateMachines_RunJoystickSampleProbe(void)
{
  ps_dev_tmag3001_stabilize_result_t stabilize_result;
  ps_dev_tmag3001_wake_result_t wake_result;
  ps_dev_tmag3001_raw_sample_t sample;
  ps_dev_tmag3001_suspend_result_t suspend_result;
  ps_status_t driver_status;
  HAL_StatusTypeDef status;
  uint32_t sweep_start_tick;
  uint32_t first_sample;

  g_ps_hw6_owner_sm_probe.joystick_sample_request_count++;
  g_ps_hw6_owner_sm_probe.joystick_sample_start_tick =
    (uint32_t)tx_time_get();
  g_ps_hw6_owner_sm_probe.joystick_sample_end_tick = 0UL;
  PS_HW6_SM_ResetJoystickSampleProbe();

  status = HAL_OK;
  if (ps_joystick_device.state == PS_DEV_TMAG3001_STATE_READY)
  {
    driver_status = ps_dev_tmag3001_stabilize_suspended(
      &ps_joystick_device,
      &stabilize_result);
    g_ps_hw6_owner_sm_probe.joystick_sample_stabilize_status =
      (uint32_t)driver_status;
    status = PS_HW6_SM_StatusToHal(driver_status);
  }

  if (status == HAL_OK)
  {
    PS_HW6_SM_ClearJoystickTerminalSleepProof();
    driver_status = ps_dev_tmag3001_wake_continuous(
      &ps_joystick_device,
      &wake_result);
    g_ps_hw6_owner_sm_probe.joystick_sample_wake_status =
      (uint32_t)driver_status;
    status = PS_HW6_SM_StatusToHal(driver_status);
  }

  if (status == HAL_OK)
  {
    driver_status = ps_dev_tmag3001_read_raw_sample(
      &ps_joystick_device,
      &sample);
    g_ps_hw6_owner_sm_probe.joystick_sample_center_status =
      (uint32_t)sample.status;
    g_ps_hw6_owner_sm_probe.joystick_sample_center_conv_status =
      (uint32_t)sample.conv_status;
    g_ps_hw6_owner_sm_probe.joystick_sample_center_x =
      (int32_t)sample.x;
    g_ps_hw6_owner_sm_probe.joystick_sample_center_y =
      (int32_t)sample.y;
    g_ps_hw6_owner_sm_probe.joystick_sample_center_z =
      (int32_t)sample.z;
    g_ps_hw6_owner_sm_probe.joystick_sample_read_status =
      (uint32_t)sample.status;
    status = PS_HW6_SM_StatusToHal(driver_status);
  }

  first_sample = 1UL;
  sweep_start_tick = (uint32_t)tx_time_get();
  while ((status == HAL_OK) &&
         (((uint32_t)tx_time_get() - sweep_start_tick) <
          PS_HW6_JOYSTICK_SWEEP_DURATION_TICKS))
  {
    driver_status = ps_dev_tmag3001_read_raw_sample(
      &ps_joystick_device,
      &sample);
    g_ps_hw6_owner_sm_probe.joystick_sample_read_status =
      (uint32_t)sample.status;
    status = PS_HW6_SM_StatusToHal(driver_status);
    if (status == HAL_OK)
    {
      (void)PS_HW6_SM_NormalizeJoystickSample(
        &sample,
        PS_INPUT_JOYSTICK_POLICY_FAST_POLL);
      if (first_sample != 0UL)
      {
        g_ps_hw6_owner_sm_probe.joystick_sample_first_x =
          (int32_t)sample.x;
        g_ps_hw6_owner_sm_probe.joystick_sample_first_y =
          (int32_t)sample.y;
        g_ps_hw6_owner_sm_probe.joystick_sample_first_z =
          (int32_t)sample.z;
        g_ps_hw6_owner_sm_probe.joystick_sample_min_x =
          (int32_t)sample.x;
        g_ps_hw6_owner_sm_probe.joystick_sample_min_y =
          (int32_t)sample.y;
        g_ps_hw6_owner_sm_probe.joystick_sample_min_z =
          (int32_t)sample.z;
        g_ps_hw6_owner_sm_probe.joystick_sample_max_x =
          (int32_t)sample.x;
        g_ps_hw6_owner_sm_probe.joystick_sample_max_y =
          (int32_t)sample.y;
        g_ps_hw6_owner_sm_probe.joystick_sample_max_z =
          (int32_t)sample.z;
        first_sample = 0UL;
      }
      if ((int32_t)sample.x < g_ps_hw6_owner_sm_probe.joystick_sample_min_x)
      {
        g_ps_hw6_owner_sm_probe.joystick_sample_min_x = (int32_t)sample.x;
      }
      if ((int32_t)sample.y < g_ps_hw6_owner_sm_probe.joystick_sample_min_y)
      {
        g_ps_hw6_owner_sm_probe.joystick_sample_min_y = (int32_t)sample.y;
      }
      if ((int32_t)sample.z < g_ps_hw6_owner_sm_probe.joystick_sample_min_z)
      {
        g_ps_hw6_owner_sm_probe.joystick_sample_min_z = (int32_t)sample.z;
      }
      if ((int32_t)sample.x > g_ps_hw6_owner_sm_probe.joystick_sample_max_x)
      {
        g_ps_hw6_owner_sm_probe.joystick_sample_max_x = (int32_t)sample.x;
      }
      if ((int32_t)sample.y > g_ps_hw6_owner_sm_probe.joystick_sample_max_y)
      {
        g_ps_hw6_owner_sm_probe.joystick_sample_max_y = (int32_t)sample.y;
      }
      if ((int32_t)sample.z > g_ps_hw6_owner_sm_probe.joystick_sample_max_z)
      {
        g_ps_hw6_owner_sm_probe.joystick_sample_max_z = (int32_t)sample.z;
      }
      g_ps_hw6_owner_sm_probe.joystick_sample_x = (int32_t)sample.x;
      g_ps_hw6_owner_sm_probe.joystick_sample_y = (int32_t)sample.y;
      g_ps_hw6_owner_sm_probe.joystick_sample_z = (int32_t)sample.z;
      g_ps_hw6_owner_sm_probe.joystick_sample_conv_status =
        sample.conv_status;
      g_ps_hw6_owner_sm_probe.joystick_sample_count++;
    }
    else
    {
      g_ps_hw6_owner_sm_probe.joystick_sample_error_count++;
    }

    if (((uint32_t)tx_time_get() - sweep_start_tick) <
        PS_HW6_JOYSTICK_SWEEP_DURATION_TICKS)
    {
      tx_thread_sleep(PS_HW6_JOYSTICK_SWEEP_PERIOD_TICKS);
    }
  }

  if ((status == HAL_OK) &&
      (g_ps_hw6_owner_sm_probe.joystick_sample_count == 0UL))
  {
    status = HAL_ERROR;
  }

  if (ps_joystick_device.state == PS_DEV_TMAG3001_STATE_ACTIVE)
  {
    driver_status = ps_dev_tmag3001_suspend(
      &ps_joystick_device,
      &suspend_result);
    g_ps_hw6_owner_sm_probe.joystick_sample_sleep_status =
      (uint32_t)suspend_result.sleep_status;
    if (status == HAL_OK)
    {
      status = PS_HW6_SM_StatusToHal(driver_status);
    }
  }

  g_ps_hw6_owner_sm_probe.joystick_sample_status = (uint32_t)status;
  g_ps_hw6_owner_sm_probe.joystick_sample_end_tick =
    (uint32_t)tx_time_get();
  PS_HW6_SM_UpdateJoystickDriverProbe();
  PS_HW6_SM_UpdateJoystickInputProbe();
  return status;
}

HAL_StatusTypeDef PS_HW6_OwnerStateMachines_RunJoystickCalibrationCapture(
  uint32_t calibration_page)
{
  HAL_StatusTypeDef status;
  int32_t neutral_half_span_x;
  int32_t neutral_half_span_y;
  int32_t deadzone;

  g_ps_hw6_owner_sm_probe.joystick_calibration_capture_request_count++;
  g_ps_hw6_owner_sm_probe.joystick_calibration_capture_start_tick =
    (uint32_t)tx_time_get();
  g_ps_hw6_owner_sm_probe.joystick_calibration_capture_end_tick = 0UL;
  g_ps_hw6_owner_sm_probe.joystick_calibration_capture_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.joystick_calibration_capture_page =
    calibration_page;

  if ((calibration_page != PS_UI_ROUTER_CAL_JOYSTICK_NEUTRAL) &&
      (calibration_page != PS_UI_ROUTER_CAL_JOYSTICK_RIGHT) &&
      (calibration_page != PS_UI_ROUTER_CAL_JOYSTICK_CIRCLE))
  {
    g_ps_hw6_owner_sm_probe.joystick_calibration_capture_status =
      (uint32_t)HAL_ERROR;
    g_ps_hw6_owner_sm_probe.joystick_calibration_capture_end_tick =
      (uint32_t)tx_time_get();
    return HAL_ERROR;
  }

  status = PS_HW6_OwnerStateMachines_RunJoystickSampleProbe();
  if ((status == HAL_OK) &&
      (g_ps_hw6_owner_sm_probe.joystick_sample_count != 0UL))
  {
    if (calibration_page == PS_UI_ROUTER_CAL_JOYSTICK_NEUTRAL)
    {
      ps_joystick_active_calibration.center_x =
        (g_ps_hw6_owner_sm_probe.joystick_sample_min_x +
         g_ps_hw6_owner_sm_probe.joystick_sample_max_x) / 2;
      ps_joystick_active_calibration.center_y =
        (g_ps_hw6_owner_sm_probe.joystick_sample_min_y +
         g_ps_hw6_owner_sm_probe.joystick_sample_max_y) / 2;
      neutral_half_span_x = PS_HW6_SM_Abs32(
        g_ps_hw6_owner_sm_probe.joystick_sample_max_x -
        ps_joystick_active_calibration.center_x);
      neutral_half_span_y = PS_HW6_SM_Abs32(
        g_ps_hw6_owner_sm_probe.joystick_sample_max_y -
        ps_joystick_active_calibration.center_y);
      deadzone = ((neutral_half_span_x > neutral_half_span_y) ?
                  neutral_half_span_x : neutral_half_span_y) +
                 PS_HW6_JOYSTICK_CAL_DEADZONE_PAD;
      ps_joystick_active_calibration.deadzone_counts =
        PS_HW6_SM_ClampJoystickDeadzone(deadzone);
      ps_joystick_active_calibration.valid = 0UL;
    }
    else if (calibration_page == PS_UI_ROUTER_CAL_JOYSTICK_RIGHT)
    {
      ps_joystick_active_calibration.max_x =
        g_ps_hw6_owner_sm_probe.joystick_sample_max_x;
      ps_joystick_active_calibration.valid = 0UL;
    }
    else
    {
      ps_joystick_active_calibration.min_x =
        g_ps_hw6_owner_sm_probe.joystick_sample_min_x;
      if (g_ps_hw6_owner_sm_probe.joystick_sample_max_x >
          ps_joystick_active_calibration.max_x)
      {
        ps_joystick_active_calibration.max_x =
          g_ps_hw6_owner_sm_probe.joystick_sample_max_x;
      }
      ps_joystick_active_calibration.min_y =
        g_ps_hw6_owner_sm_probe.joystick_sample_min_y;
      ps_joystick_active_calibration.max_y =
        g_ps_hw6_owner_sm_probe.joystick_sample_max_y;
      ps_joystick_active_calibration.valid =
        PS_HW6_SM_JoystickCalibrationHasRange();
    }
  }
  else
  {
    status = HAL_ERROR;
  }

  PS_HW6_SM_UpdateJoystickCalibrationProbe();
  g_ps_hw6_owner_sm_probe.joystick_calibration_capture_status =
    (uint32_t)status;
  g_ps_hw6_owner_sm_probe.joystick_calibration_capture_end_tick =
    (uint32_t)tx_time_get();
  return status;
}

HAL_StatusTypeDef PS_HW6_OwnerStateMachines_RunJoystickLiveProbe(void)
{
  ps_dev_tmag3001_stabilize_result_t stabilize_result;
  ps_dev_tmag3001_wake_result_t wake_result;
  ps_dev_tmag3001_raw_sample_t sample;
  ps_dev_tmag3001_suspend_result_t suspend_result;
  ps_status_t driver_status;
  HAL_StatusTypeDef status;
  uint32_t live_start_tick;

  g_ps_hw6_owner_sm_probe.joystick_live_request_count++;
  g_ps_hw6_owner_sm_probe.joystick_live_start_tick =
    (uint32_t)tx_time_get();
  g_ps_hw6_owner_sm_probe.joystick_live_end_tick = 0UL;
  g_ps_hw6_owner_sm_probe.joystick_live_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.joystick_live_sample_count = 0UL;
  g_ps_hw6_owner_sm_probe.joystick_live_error_count = 0UL;

  status = HAL_OK;
  if (ps_joystick_device.state == PS_DEV_TMAG3001_STATE_READY)
  {
    driver_status = ps_dev_tmag3001_stabilize_suspended(
      &ps_joystick_device,
      &stabilize_result);
    status = PS_HW6_SM_StatusToHal(driver_status);
  }

  if (status == HAL_OK)
  {
    PS_HW6_SM_ClearJoystickTerminalSleepProof();
    driver_status = ps_dev_tmag3001_wake_continuous(
      &ps_joystick_device,
      &wake_result);
    status = PS_HW6_SM_StatusToHal(driver_status);
  }

  if (status == HAL_OK)
  {
    (void)PS_HW6_SM_Transition(PS_HW6_SM_JOYSTICK,
                               JOY_EV_FAST_POLL_REQUEST,
                               HAL_OK);
  }

  live_start_tick = (uint32_t)tx_time_get();
  while ((status == HAL_OK) &&
         (((uint32_t)tx_time_get() - live_start_tick) <
          PS_HW6_JOYSTICK_LIVE_DURATION_TICKS))
  {
    driver_status = ps_dev_tmag3001_read_raw_sample(
      &ps_joystick_device,
      &sample);
    status = PS_HW6_SM_StatusToHal(driver_status);
    if (status == HAL_OK)
    {
      status = PS_HW6_SM_NormalizeJoystickSample(
        &sample,
        PS_INPUT_JOYSTICK_POLICY_FAST_POLL);
      if (status == HAL_OK)
      {
        g_ps_hw6_owner_sm_probe.joystick_live_sample_count++;
      }
      else
      {
        g_ps_hw6_owner_sm_probe.joystick_live_error_count++;
      }
    }
    else
    {
      g_ps_hw6_owner_sm_probe.joystick_live_error_count++;
    }

    if (((uint32_t)tx_time_get() - live_start_tick) <
        PS_HW6_JOYSTICK_LIVE_DURATION_TICKS)
    {
      tx_thread_sleep(PS_HW6_JOYSTICK_LIVE_PERIOD_TICKS);
    }
  }

  if ((status == HAL_OK) &&
      (g_ps_hw6_owner_sm_probe.joystick_live_sample_count == 0UL))
  {
    status = HAL_ERROR;
  }

  if (ps_joystick_device.state == PS_DEV_TMAG3001_STATE_ACTIVE)
  {
    driver_status = ps_dev_tmag3001_suspend(
      &ps_joystick_device,
      &suspend_result);
    if (status == HAL_OK)
    {
      status = PS_HW6_SM_StatusToHal(driver_status);
    }
  }

  if (status == HAL_OK)
  {
    (void)PS_HW6_SM_Transition(PS_HW6_SM_JOYSTICK,
                               JOY_EV_QUIESCE,
                               HAL_OK);
  }
  else
  {
    (void)PS_HW6_SM_Transition(PS_HW6_SM_JOYSTICK,
                               JOY_EV_I2C_ERROR,
                               status);
  }

  PS_HW6_SM_UpdateJoystickDriverProbe();
  PS_HW6_SM_UpdateJoystickInputProbe();
  g_ps_hw6_owner_sm_probe.joystick_live_end_tick =
    (uint32_t)tx_time_get();
  g_ps_hw6_owner_sm_probe.joystick_live_status = (uint32_t)status;
  return status;
}

HAL_StatusTypeDef PS_HW6_OwnerStateMachines_RunJoystickCardinalProbe(void)
{
  ps_dev_tmag3001_stabilize_result_t stabilize_result;
  ps_dev_tmag3001_wake_result_t wake_result;
  ps_dev_tmag3001_raw_sample_t sample;
  ps_dev_tmag3001_suspend_result_t suspend_result;
  ps_status_t driver_status;
  HAL_StatusTypeDef status;

  g_ps_hw6_owner_sm_probe.joystick_cardinal_request_count++;
  g_ps_hw6_owner_sm_probe.joystick_cardinal_start_tick =
    (uint32_t)tx_time_get();
  g_ps_hw6_owner_sm_probe.joystick_cardinal_end_tick = 0UL;
  g_ps_hw6_owner_sm_probe.joystick_cardinal_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.joystick_calibration_capture_request_count = 0UL;
  g_ps_hw6_owner_sm_probe.joystick_calibration_capture_start_tick = 0UL;
  g_ps_hw6_owner_sm_probe.joystick_calibration_capture_end_tick = 0UL;
  g_ps_hw6_owner_sm_probe.joystick_calibration_capture_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.joystick_calibration_capture_page =
    PS_UI_ROUTER_CAL_NONE;
  PS_HW6_SM_UpdateJoystickCalibrationProbe();
  status = HAL_OK;
  if (ps_joystick_device.state == PS_DEV_TMAG3001_STATE_READY)
  {
    driver_status = ps_dev_tmag3001_stabilize_suspended(
      &ps_joystick_device,
      &stabilize_result);
    status = PS_HW6_SM_StatusToHal(driver_status);
  }

  if (status == HAL_OK)
  {
    (void)PS_HW6_SM_Transition(PS_HW6_SM_JOYSTICK,
                               JOY_EV_THRESHOLD_ARM_REQUEST,
                               HAL_OK);
    (void)PS_HW6_SM_Transition(PS_HW6_SM_JOYSTICK,
                               JOY_EV_INTERRUPT,
                               HAL_OK);
    PS_HW6_SM_ClearJoystickTerminalSleepProof();
    driver_status = ps_dev_tmag3001_wake_continuous(
      &ps_joystick_device,
      &wake_result);
    status = PS_HW6_SM_StatusToHal(driver_status);
  }

  if (status == HAL_OK)
  {
    driver_status = ps_dev_tmag3001_read_raw_sample(
      &ps_joystick_device,
      &sample);
    status = PS_HW6_SM_StatusToHal(driver_status);
  }

  if (status == HAL_OK)
  {
    (void)PS_HW6_SM_Transition(PS_HW6_SM_JOYSTICK,
                               JOY_EV_SAMPLE_DONE,
                               HAL_OK);
    status = PS_HW6_SM_NormalizeJoystickSample(
      &sample,
      PS_INPUT_JOYSTICK_POLICY_DIRECTION_SAMPLE);
  }

  if (ps_joystick_device.state == PS_DEV_TMAG3001_STATE_ACTIVE)
  {
    driver_status = ps_dev_tmag3001_suspend(
      &ps_joystick_device,
      &suspend_result);
    if (status == HAL_OK)
    {
      status = PS_HW6_SM_StatusToHal(driver_status);
    }
  }

  if (status == HAL_OK)
  {
    (void)PS_HW6_SM_Transition(PS_HW6_SM_JOYSTICK,
                               JOY_EV_NORMALIZE_DONE,
                               HAL_OK);
  }
  else
  {
    (void)PS_HW6_SM_Transition(PS_HW6_SM_JOYSTICK,
                               JOY_EV_I2C_ERROR,
                               status);
  }

  PS_HW6_SM_UpdateJoystickDriverProbe();
  PS_HW6_SM_UpdateJoystickInputProbe();
  g_ps_hw6_owner_sm_probe.joystick_cardinal_end_tick =
    (uint32_t)tx_time_get();
  g_ps_hw6_owner_sm_probe.joystick_cardinal_status = (uint32_t)status;
  return status;
}
static void PS_HW6_SM_RecordUsbExportEntryState(void)
{
  g_ps_hw6_owner_sm_probe.usb_vbus_present =
    (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_9) == GPIO_PIN_SET) ? 1UL : 0UL;
  g_ps_hw6_owner_sm_probe.usb_pcd_state_before =
    (uint32_t)HAL_PCD_GetState(&hpcd_USB_OTG_FS);
  g_ps_hw6_owner_sm_probe.usb_clock_enabled_before =
    (__HAL_RCC_USB_IS_CLK_ENABLED() != 0U) ? 1UL : 0UL;
  g_ps_hw6_owner_sm_probe.usb_vddusb_enabled_before =
    (READ_BIT(PWR->SVMCR, PWR_SVMCR_USV) != 0U) ? 1UL : 0UL;
  g_ps_hw6_owner_sm_probe.usb_deinit_attempted = 0UL;
  g_ps_hw6_owner_sm_probe.usb_deinit_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.usb_pcd_state_after =
    g_ps_hw6_owner_sm_probe.usb_pcd_state_before;
  g_ps_hw6_owner_sm_probe.usb_clock_enabled_after =
    g_ps_hw6_owner_sm_probe.usb_clock_enabled_before;
  g_ps_hw6_owner_sm_probe.usb_vddusb_enabled_after =
    g_ps_hw6_owner_sm_probe.usb_vddusb_enabled_before;
  g_ps_hw6_owner_sm_probe.usb_parked = 0UL;
}
static void PS_HW6_SM_UpdateUsbHostAvailability(uint32_t event)
{
  uint32_t storage_state =
    g_ps_hw6_owner_sm_probe.current_state[PS_HW6_SM_STORAGE];
  uint32_t external_power =
    (g_ps_hw6_owner_probe.power_vbus_ok != 0UL) ? 1UL : 0UL;
  uint32_t msc_active =
    ((g_ps_storage_msc_bridge_probe.export_enabled != 0UL) ||
     (storage_state == (uint32_t)STORAGE_USB_STAGING_EXPORTED) ||
     (storage_state == (uint32_t)STORAGE_USB_STAGING_DIRTY)) ? 1UL : 0UL;
  uint32_t data_seen =
    ((g_ps_storage_msc_bridge_probe.read_count != 0UL) ||
     (g_ps_storage_msc_bridge_probe.write_count != 0UL) ||
     (g_ps_storage_msc_bridge_probe.flush_count != 0UL) ||
     (g_ps_storage_msc_bridge_probe.status_count != 0UL)) ? 1UL : 0UL;
  uint32_t command_count =
    g_ps_storage_msc_bridge_probe.read_count +
    g_ps_storage_msc_bridge_probe.write_count +
    g_ps_storage_msc_bridge_probe.flush_count +
    g_ps_storage_msc_bridge_probe.status_count;
  uint32_t state;

  if (msc_active != 0UL)
  {
    state = (uint32_t)PS_HW6_USB_HOST_AVAILABILITY_MSC_ACTIVE;
  }
  else if (data_seen != 0UL)
  {
    state = (uint32_t)PS_HW6_USB_HOST_AVAILABILITY_DATA_HOST_SEEN;
  }
  else if (external_power != 0UL)
  {
    state = (uint32_t)PS_HW6_USB_HOST_AVAILABILITY_EXTERNAL_POWER;
  }
  else
  {
    state = (uint32_t)PS_HW6_USB_HOST_AVAILABILITY_NO_EXTERNAL_POWER;
  }

  g_ps_hw6_owner_sm_probe.usb_host_availability_state = state;
  g_ps_hw6_owner_sm_probe.usb_host_availability_event = event;
  g_ps_hw6_owner_sm_probe.usb_host_availability_update_count++;
  g_ps_hw6_owner_sm_probe.usb_host_availability_tick =
    (uint32_t)tx_time_get();
  g_ps_hw6_owner_sm_probe.usb_host_external_power_present =
    external_power;
  g_ps_hw6_owner_sm_probe.usb_host_data_seen = data_seen;
  g_ps_hw6_owner_sm_probe.usb_host_msc_active = msc_active;
  g_ps_hw6_owner_sm_probe.usb_host_msc_available =
    ((external_power != 0UL) &&
     (data_seen != 0UL) &&
     (msc_active == 0UL) &&
     (storage_state == (uint32_t)STORAGE_FLASH_READY)) ? 1UL : 0UL;
  g_ps_hw6_owner_sm_probe.usb_host_pmic_vbus =
    g_ps_hw6_owner_probe.power_vbus_ok;
  g_ps_hw6_owner_sm_probe.usb_host_mcu_vbus =
    g_ps_hw6_owner_probe.power_mcu_vbus_present;
  g_ps_hw6_owner_sm_probe.usb_host_power_agree =
    g_ps_hw6_owner_probe.power_vbus_agree;
  g_ps_hw6_owner_sm_probe.usb_host_bridge_activate_count =
    g_ps_storage_msc_bridge_probe.activate_count;
  g_ps_hw6_owner_sm_probe.usb_host_bridge_command_count =
    command_count;
}

static HAL_StatusTypeDef PS_HW6_SM_PrepareStorageForFlashReady(
  uint32_t record_usb_export_entry)
{
  ps_status_t driver_status;
  HAL_StatusTypeDef status;
  uint32_t storage_state;
  uint32_t flash_state;

  storage_state =
    g_ps_hw6_owner_sm_probe.current_state[PS_HW6_SM_STORAGE];
  if (storage_state == (uint32_t)STORAGE_FLASH_READY)
  {
    return HAL_OK;
  }

  if (storage_state == (uint32_t)STORAGE_OFFLINE)
  {
    (void)PS_HW6_SM_Transition(PS_HW6_SM_STORAGE,
                              STORAGE_EV_INIT,
                              HAL_OK);
  }
  else if ((storage_state != (uint32_t)STORAGE_INIT) &&
           (storage_state != (uint32_t)STORAGE_RECOVERING))
  {
    return HAL_ERROR;
  }

  if ((g_ps_hw6_owner_sm_probe.flash_driver_init_status !=
       (uint32_t)PS_STATUS_OK) ||
      (g_ps_hw6_owner_sm_probe.flash_block_init_status !=
       (uint32_t)PS_STATUS_OK))
  {
    (void)PS_HW6_SM_Transition(PS_HW6_SM_STORAGE,
                              STORAGE_EV_FAULT,
                              HAL_ERROR);
    return HAL_ERROR;
  }

  if (record_usb_export_entry != 0UL)
  {
    PS_HW6_SM_RecordUsbExportEntryState();
  }

  flash_state = g_ps_hw6_owner_sm_probe.current_state[PS_HW6_SM_FLASH];
  if (flash_state == (uint32_t)FLASH_OFF)
  {
    (void)PS_HW6_SM_Transition(PS_HW6_SM_FLASH,
                              FLASH_EV_BOOT,
                              HAL_OK);
  }
  else if (flash_state == (uint32_t)FLASH_DEEP_POWER_DOWN)
  {
    driver_status = PS_HW6_SM_EnsureFlashAwake();
    status = PS_HW6_SM_StatusToHal(driver_status);
    if (status != HAL_OK)
    {
      (void)PS_HW6_SM_Transition(PS_HW6_SM_FLASH,
                                FLASH_EV_FAULT,
                                status);
      (void)PS_HW6_SM_Transition(PS_HW6_SM_STORAGE,
                                STORAGE_EV_FAULT,
                                status);
      return status;
    }
    tx_thread_sleep(PS_HW6_FLASH_WAKE_SETTLE_TICKS);
    (void)PS_HW6_SM_Transition(PS_HW6_SM_FLASH,
                              FLASH_EV_WAKE_REVALIDATE,
                              status);
  }

  driver_status = ps_dev_at25sl128a_read_jedec(
    &ps_flash_device,
    &ps_flash_jedec_result);
  status = PS_HW6_SM_StatusToHal(driver_status);
  g_ps_hw6_owner_sm_probe.flash_jedec_status =
    ps_flash_jedec_result.hal_status;
  g_ps_hw6_owner_sm_probe.flash_jedec_id[0] =
    ps_flash_jedec_result.jedec_id[0];
  g_ps_hw6_owner_sm_probe.flash_jedec_id[1] =
    ps_flash_jedec_result.jedec_id[1];
  g_ps_hw6_owner_sm_probe.flash_jedec_id[2] =
    ps_flash_jedec_result.jedec_id[2];
  g_ps_hw6_owner_sm_probe.flash_identity_match =
    ps_flash_jedec_result.identity_match;
  PS_HW6_SM_UpdateFlashDriverProbe();

  if ((status != HAL_OK) ||
      (g_ps_hw6_owner_sm_probe.flash_identity_match == 0UL))
  {
    (void)PS_HW6_SM_Transition(PS_HW6_SM_FLASH,
                              FLASH_EV_PROBE_FAIL,
                              status);
    (void)PS_HW6_SM_Transition(PS_HW6_SM_STORAGE,
                              STORAGE_EV_FAULT,
                              status);
    return HAL_ERROR;
  }

  if (g_ps_hw6_owner_sm_probe.current_state[PS_HW6_SM_FLASH] ==
      (uint32_t)FLASH_PROBE)
  {
    (void)PS_HW6_SM_Transition(PS_HW6_SM_FLASH,
                              FLASH_EV_PROBE_OK,
                              HAL_OK);
  }
  if (g_ps_hw6_owner_sm_probe.current_state[PS_HW6_SM_FLASH] ==
      (uint32_t)FLASH_CONFIG)
  {
    (void)PS_HW6_SM_Transition(PS_HW6_SM_FLASH,
                              FLASH_EV_CONFIG_OK,
                              HAL_OK);
  }
  if (g_ps_hw6_owner_sm_probe.current_state[PS_HW6_SM_FLASH] !=
      (uint32_t)FLASH_READY)
  {
    (void)PS_HW6_SM_Transition(PS_HW6_SM_STORAGE,
                              STORAGE_EV_FAULT,
                              HAL_ERROR);
    return HAL_ERROR;
  }

  driver_status = ps_storage_layout_validate(
    &ps_flash_block.geometry,
    &ps_storage_layout_result);
  status = PS_HW6_SM_StatusToHal(driver_status);
  PS_HW6_SM_RecordStorageLayoutResult(&ps_storage_layout_result);
  if (status != HAL_OK)
  {
    (void)PS_HW6_SM_Transition(PS_HW6_SM_FLASH,
                              FLASH_EV_FAULT,
                              status);
    (void)PS_HW6_SM_Transition(PS_HW6_SM_STORAGE,
                              STORAGE_EV_FAULT,
                              status);
    return status;
  }

  PS_StorageMscBridge_SetPolicy(0UL, 1UL, 1UL);

  driver_status = ps_dev_at25sl128a_enter_deep_power_down(
    &ps_flash_device,
    &ps_flash_command_result);
  status = PS_HW6_SM_StatusToHal(driver_status);
  g_ps_hw6_owner_sm_probe.flash_deep_power_down_status =
    ps_flash_command_result.hal_status;
  if (status == HAL_OK)
  {
    (void)PS_HW6_SM_Transition(PS_HW6_SM_FLASH,
                              FLASH_EV_REQUEST_DEEP_POWER_DOWN,
                              status);
    PS_HW6_SM_ParkOspiClocksForStop();
    (void)PS_HW6_SM_Transition(PS_HW6_SM_STORAGE,
                              STORAGE_EV_FLASH_READY,
                              status);
  }
  else
  {
    (void)PS_HW6_SM_Transition(PS_HW6_SM_FLASH,
                              FLASH_EV_FAULT,
                              status);
    (void)PS_HW6_SM_Transition(PS_HW6_SM_STORAGE,
                              STORAGE_EV_FAULT,
                              status);
  }

  return status;
}

static HAL_StatusTypeDef PS_HW6_SM_PrepareStorageForUsbExport(void)
{
  return PS_HW6_SM_PrepareStorageForFlashReady(1UL);
}

static HAL_StatusTypeDef PS_HW6_SM_RunUsbStageRescanScaffold(void)
{
  ps_status_t scan_status;
  ps_status_t validate_status;
  uint32_t validate_candidate;
  uint32_t index;

  if (g_ps_hw6_owner_sm_probe.usb_stage_rescan_pending == 0UL)
  {
    return HAL_OK;
  }

  g_ps_hw6_owner_sm_probe.package_candidate_pending = 0UL;
  g_ps_hw6_owner_sm_probe.package_validate_start_tick = 0UL;
  g_ps_hw6_owner_sm_probe.package_validate_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.package_validate_reason =
    (uint32_t)PS_STORAGE_PACKAGE_VALIDATE_NOT_RUN;
  g_ps_hw6_owner_sm_probe.package_validate_candidate_count = 0UL;
  g_ps_hw6_owner_sm_probe.package_validate_unsupported_count = 0UL;
  g_ps_hw6_owner_sm_probe.package_validate_file_size = 0UL;
  g_ps_hw6_owner_sm_probe.package_validate_header_bytes = 0UL;
  g_ps_hw6_owner_sm_probe.package_validate_bytes_read = 0UL;
  g_ps_hw6_owner_sm_probe.package_validate_magic = 0UL;
  g_ps_hw6_owner_sm_probe.package_validate_magic_valid = 0UL;
  g_ps_hw6_owner_sm_probe.package_validate_minimum_envelope_valid = 0UL;
  g_ps_hw6_owner_sm_probe.package_validate_header_layout_supported = 0UL;
  g_ps_hw6_owner_sm_probe.package_validate_lx_open_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.package_validate_fx_open_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.package_validate_file_open_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.package_validate_file_read_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.package_validate_file_close_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.package_validate_fx_close_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.package_validate_lx_close_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  for (index = 0UL; index < 16UL; ++index)
  {
    g_ps_hw6_owner_sm_probe.package_validate_header_first16[index] = 0UL;
  }

  scan_status = ps_storage_filex_levelx_scan_usb_staging(
    &ps_flash_block,
    PS_HW6_SM_FindStorageRegion(PS_STORAGE_REGION_USB_STAGING),
    &ps_storage_stage_scan_result);

  g_ps_hw6_owner_sm_probe.usb_stage_rescan_status =
    (scan_status == PS_STATUS_OK) ? (uint32_t)HAL_OK : (uint32_t)HAL_ERROR;
  g_ps_hw6_owner_sm_probe.usb_stage_rescan_package_scan_status =
    ps_storage_stage_scan_result.package_scan_status;
  g_ps_hw6_owner_sm_probe.usb_stage_rescan_classification =
    ps_storage_stage_scan_result.classification;
  g_ps_hw6_owner_sm_probe.usb_stage_rescan_entry_count =
    ps_storage_stage_scan_result.entry_count;
  g_ps_hw6_owner_sm_probe.usb_stage_rescan_file_count =
    ps_storage_stage_scan_result.file_count;
  g_ps_hw6_owner_sm_probe.usb_stage_rescan_directory_count =
    ps_storage_stage_scan_result.directory_count;
  g_ps_hw6_owner_sm_probe.usb_stage_rescan_package_candidate_count =
    ps_storage_stage_scan_result.package_candidate_count;
  g_ps_hw6_owner_sm_probe.usb_stage_rescan_unsupported_count =
    ps_storage_stage_scan_result.unsupported_count;
  g_ps_hw6_owner_sm_probe.usb_stage_rescan_bounded =
    ps_storage_stage_scan_result.bounded;
  g_ps_hw6_owner_sm_probe.usb_stage_rescan_first_entry_status =
    ps_storage_stage_scan_result.first_entry_status;
  g_ps_hw6_owner_sm_probe.usb_stage_rescan_last_entry_status =
    ps_storage_stage_scan_result.last_entry_status;
  g_ps_hw6_owner_sm_probe.usb_stage_rescan_lx_open_status =
    ps_storage_stage_scan_result.lx_open_status;
  g_ps_hw6_owner_sm_probe.usb_stage_rescan_fx_open_status =
    ps_storage_stage_scan_result.fx_open_status;
  g_ps_hw6_owner_sm_probe.usb_stage_rescan_fx_close_status =
    ps_storage_stage_scan_result.fx_close_status;
  g_ps_hw6_owner_sm_probe.usb_stage_rescan_lx_close_status =
    ps_storage_stage_scan_result.lx_close_status;

  validate_candidate =
    ((scan_status == PS_STATUS_OK) &&
     (ps_storage_stage_scan_result.classification ==
      (uint32_t)PS_STORAGE_STAGE_SCAN_PACKAGE_CANDIDATE) &&
     (ps_storage_stage_scan_result.package_candidate_count == 1UL) &&
     (ps_storage_stage_scan_result.unsupported_count == 0UL) &&
     (ps_storage_stage_scan_result.bounded == 0UL)) ? 1UL : 0UL;

  if (validate_candidate != 0UL)
  {
    g_ps_hw6_owner_sm_probe.package_validate_request_count++;
    g_ps_hw6_owner_sm_probe.package_validate_start_tick =
      (uint32_t)tx_time_get();
    validate_status = ps_storage_filex_levelx_validate_usb_staging_package(
      &ps_flash_block,
      PS_HW6_SM_FindStorageRegion(PS_STORAGE_REGION_USB_STAGING),
      &ps_storage_package_validate_result);

    g_ps_hw6_owner_sm_probe.package_validate_status =
      (uint32_t)validate_status;
    g_ps_hw6_owner_sm_probe.package_validate_reason =
      ps_storage_package_validate_result.reason;
    g_ps_hw6_owner_sm_probe.package_validate_candidate_count =
      ps_storage_package_validate_result.package_candidate_count;
    g_ps_hw6_owner_sm_probe.package_validate_unsupported_count =
      ps_storage_package_validate_result.unsupported_count;
    g_ps_hw6_owner_sm_probe.package_validate_file_size =
      ps_storage_package_validate_result.package_size_bytes;
    g_ps_hw6_owner_sm_probe.package_validate_header_bytes =
      ps_storage_package_validate_result.header_probe_bytes;
    g_ps_hw6_owner_sm_probe.package_validate_bytes_read =
      ps_storage_package_validate_result.bytes_read;
    g_ps_hw6_owner_sm_probe.package_validate_magic =
      ps_storage_package_validate_result.magic;
    g_ps_hw6_owner_sm_probe.package_validate_magic_valid =
      ps_storage_package_validate_result.magic_valid;
    g_ps_hw6_owner_sm_probe.package_validate_minimum_envelope_valid =
      ps_storage_package_validate_result.minimum_envelope_valid;
    g_ps_hw6_owner_sm_probe.package_validate_header_layout_supported =
      ps_storage_package_validate_result.header_layout_supported;
    g_ps_hw6_owner_sm_probe.package_validate_lx_open_status =
      ps_storage_package_validate_result.lx_open_status;
    g_ps_hw6_owner_sm_probe.package_validate_fx_open_status =
      ps_storage_package_validate_result.fx_open_status;
    g_ps_hw6_owner_sm_probe.package_validate_file_open_status =
      ps_storage_package_validate_result.file_open_status;
    g_ps_hw6_owner_sm_probe.package_validate_file_read_status =
      ps_storage_package_validate_result.file_read_status;
    g_ps_hw6_owner_sm_probe.package_validate_file_close_status =
      ps_storage_package_validate_result.file_close_status;
    g_ps_hw6_owner_sm_probe.package_validate_fx_close_status =
      ps_storage_package_validate_result.fx_close_status;
    g_ps_hw6_owner_sm_probe.package_validate_lx_close_status =
      ps_storage_package_validate_result.lx_close_status;
    for (index = 0UL; index < 16UL; ++index)
    {
      g_ps_hw6_owner_sm_probe.package_validate_header_first16[index] =
        ps_storage_package_validate_result.header_first16[index];
    }

    g_ps_hw6_owner_sm_probe.package_candidate_pending =
      ((validate_status == PS_STATUS_OK) &&
       (ps_storage_package_validate_result.reason ==
        (uint32_t)PS_STORAGE_PACKAGE_VALIDATE_MINIMUM_ENVELOPE_OK) &&
       (ps_storage_package_validate_result.magic_valid != 0UL) &&
       (ps_storage_package_validate_result.minimum_envelope_valid != 0UL)) ?
      1UL : 0UL;
  }

  g_ps_hw6_owner_sm_probe.usb_stage_rescan_pending = 0UL;
  return (scan_status == PS_STATUS_OK) ? HAL_OK : HAL_ERROR;
}

HAL_StatusTypeDef PS_HW6_OwnerStateMachines_RunPackageInstallStub(void)
{
  HAL_StatusTypeDef status = HAL_ERROR;

  g_ps_hw6_owner_sm_probe.package_install_stub_request_count++;
  g_ps_hw6_owner_sm_probe.package_install_stub_start_tick =
    (uint32_t)tx_time_get();
  g_ps_hw6_owner_sm_probe.package_install_stub_candidate_classification =
    g_ps_hw6_owner_sm_probe.usb_stage_rescan_classification;
  g_ps_hw6_owner_sm_probe.package_install_stub_candidate_count =
    g_ps_hw6_owner_sm_probe.usb_stage_rescan_package_candidate_count;
  g_ps_hw6_owner_sm_probe.package_install_stub_unsupported_count =
    g_ps_hw6_owner_sm_probe.usb_stage_rescan_unsupported_count;

  if ((g_ps_hw6_owner_sm_probe.package_candidate_pending != 0UL) &&
      (g_ps_hw6_owner_sm_probe.usb_stage_rescan_status ==
       (uint32_t)HAL_OK) &&
      (g_ps_hw6_owner_sm_probe.usb_stage_rescan_classification ==
       (uint32_t)PS_STORAGE_STAGE_SCAN_PACKAGE_CANDIDATE) &&
      (g_ps_hw6_owner_sm_probe.usb_stage_rescan_package_candidate_count ==
       1UL) &&
      (g_ps_hw6_owner_sm_probe.usb_stage_rescan_unsupported_count == 0UL) &&
      (g_ps_hw6_owner_sm_probe.package_validate_status ==
       (uint32_t)PS_STATUS_OK) &&
      (g_ps_hw6_owner_sm_probe.package_validate_reason ==
       (uint32_t)PS_STORAGE_PACKAGE_VALIDATE_MINIMUM_ENVELOPE_OK) &&
      (g_ps_hw6_owner_sm_probe.package_validate_magic_valid != 0UL) &&
      (g_ps_hw6_owner_sm_probe.package_validate_minimum_envelope_valid !=
       0UL))
  {
    status = HAL_OK;
    g_ps_hw6_owner_sm_probe.package_candidate_pending = 0UL;
  }

  g_ps_hw6_owner_sm_probe.package_install_stub_last_status =
    (uint32_t)status;
  return status;
}

HAL_StatusTypeDef PS_HW6_OwnerStateMachines_AttachStorage(void)
{
  HAL_StatusTypeDef status;

  g_ps_hw6_owner_sm_probe.storage_attach_request_count++;
  g_ps_hw6_owner_sm_probe.storage_attach_start_tick =
    (uint32_t)tx_time_get();
  g_ps_hw6_owner_sm_probe.storage_attach_last_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;

  status = PS_HW6_SM_PrepareStorageForFlashReady(0UL);

  g_ps_hw6_owner_sm_probe.storage_attach_storage_state =
    g_ps_hw6_owner_sm_probe.current_state[PS_HW6_SM_STORAGE];
  g_ps_hw6_owner_sm_probe.storage_attach_flash_state =
    g_ps_hw6_owner_sm_probe.current_state[PS_HW6_SM_FLASH];
  g_ps_hw6_owner_sm_probe.storage_attach_last_status =
    (uint32_t)status;
  return status;
}

HAL_StatusTypeDef PS_HW6_OwnerStateMachines_InitializeFlash(void)
{
  HAL_StatusTypeDef status;
  ps_status_t driver_status;
  uint32_t storage_state;
  uint32_t recovery_required;

  g_ps_hw6_owner_sm_probe.storage_flash_init_request_count++;
  g_ps_hw6_owner_sm_probe.storage_flash_init_start_tick =
    (uint32_t)tx_time_get();
  g_ps_hw6_owner_sm_probe.storage_flash_init_wake_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.storage_flash_init_layout_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.storage_flash_init_fxlx_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.storage_flash_init_deep_power_down_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.storage_flash_init_last_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  PS_HW6_TraceSwoLifecycle(PS_HW6_TRACE_SWO_FLASH_INIT_REQUEST);

  storage_state = g_ps_hw6_owner_sm_probe.current_state[PS_HW6_SM_STORAGE];
  recovery_required =
    ((storage_state == (uint32_t)STORAGE_ERROR) &&
     (g_ps_storage_filex_levelx_msc_probe.invalid_media_detected != 0UL)) ?
    1UL : 0UL;

  if ((storage_state != (uint32_t)STORAGE_FLASH_READY) &&
      (recovery_required == 0UL))
  {
    status = PS_HW6_SM_PrepareStorageForFlashReady(0UL);
    if (status != HAL_OK)
    {
      g_ps_hw6_owner_sm_probe.storage_flash_init_last_status =
        (uint32_t)status;
      PS_HW6_TraceSwoLifecycle(PS_HW6_TRACE_SWO_ERROR);
      return status;
    }
    storage_state = g_ps_hw6_owner_sm_probe.current_state[PS_HW6_SM_STORAGE];
    if (storage_state != (uint32_t)STORAGE_FLASH_READY)
    {
      g_ps_hw6_owner_sm_probe.storage_flash_init_last_status =
        (uint32_t)HAL_ERROR;
      PS_HW6_TraceSwoLifecycle(PS_HW6_TRACE_SWO_ERROR);
      return HAL_ERROR;
    }
  }

  driver_status = PS_HW6_SM_EnsureFlashAwake();
  g_ps_hw6_owner_sm_probe.storage_flash_init_wake_status =
    (uint32_t)driver_status;
  status = PS_HW6_SM_StatusToHal(driver_status);
  if (status != HAL_OK)
  {
    (void)PS_HW6_SM_Transition(PS_HW6_SM_FLASH,
                              FLASH_EV_FAULT, status);
    (void)PS_HW6_SM_Transition(PS_HW6_SM_STORAGE,
                              STORAGE_EV_FAULT, status);
    g_ps_hw6_owner_sm_probe.storage_flash_init_last_status =
      (uint32_t)status;
    PS_HW6_TraceSwoLifecycle(PS_HW6_TRACE_SWO_ERROR);
    return status;
  }

  PS_HW6_TraceSwoLifecycle(PS_HW6_TRACE_SWO_FLASH_WAKE_OK);
  driver_status = ps_storage_layout_validate(
    &ps_flash_block.geometry,
    &ps_storage_layout_result);
  g_ps_hw6_owner_sm_probe.storage_flash_init_layout_status =
    (uint32_t)driver_status;
  status = PS_HW6_SM_StatusToHal(driver_status);
  PS_HW6_SM_RecordStorageLayoutResult(&ps_storage_layout_result);
  if (status != HAL_OK)
  {
    (void)PS_HW6_SM_Transition(PS_HW6_SM_FLASH,
                              FLASH_EV_FAULT, status);
    (void)PS_HW6_SM_Transition(PS_HW6_SM_STORAGE,
                              STORAGE_EV_FAULT, status);
    g_ps_hw6_owner_sm_probe.storage_flash_init_last_status =
      (uint32_t)status;
    PS_HW6_TraceSwoLifecycle(PS_HW6_TRACE_SWO_ERROR);
    return status;
  }

  PS_HW6_TraceSwoLifecycle(PS_HW6_TRACE_SWO_FLASH_LAYOUT_OK);
  driver_status = ps_storage_filex_levelx_initialize_usb_staging(
    &ps_flash_block,
    PS_HW6_SM_FindStorageRegion(PS_STORAGE_REGION_USB_STAGING),
    &ps_storage_fxlx_result);
  g_ps_hw6_owner_sm_probe.storage_flash_init_fxlx_status =
    (uint32_t)driver_status;
  status = PS_HW6_SM_StatusToHal(driver_status);
  PS_HW6_SM_RecordStorageFxLxResult(&ps_storage_fxlx_result);
  PS_HW6_SM_UpdateFlashBlockProbe();
  PS_HW6_SM_UpdateFlashDriverProbe();
  if (status != HAL_OK)
  {
    (void)PS_HW6_SM_Transition(PS_HW6_SM_FLASH,
                              FLASH_EV_FAULT, status);
    (void)PS_HW6_SM_Transition(PS_HW6_SM_STORAGE,
                              STORAGE_EV_FAULT, status);
    g_ps_hw6_owner_sm_probe.storage_flash_init_last_status =
      (uint32_t)status;
    PS_HW6_TraceSwoLifecycle(PS_HW6_TRACE_SWO_ERROR);
    return status;
  }

  PS_StorageMscBridge_SetPolicy(0UL, 1UL, 1UL);

  driver_status = ps_dev_at25sl128a_enter_deep_power_down(
    &ps_flash_device,
    &ps_flash_command_result);
  g_ps_hw6_owner_sm_probe.storage_flash_init_deep_power_down_status =
    (uint32_t)driver_status;
  g_ps_hw6_owner_sm_probe.flash_deep_power_down_status =
    ps_flash_command_result.hal_status;
  status = PS_HW6_SM_StatusToHal(driver_status);
  if (status != HAL_OK)
  {
    (void)PS_HW6_SM_Transition(PS_HW6_SM_FLASH,
                              FLASH_EV_FAULT, status);
    (void)PS_HW6_SM_Transition(PS_HW6_SM_STORAGE,
                              STORAGE_EV_FAULT, status);
    PS_HW6_TraceSwoLifecycle(PS_HW6_TRACE_SWO_ERROR);
  }
  else
  {
    (void)PS_HW6_SM_Transition(PS_HW6_SM_FLASH,
                              FLASH_EV_REQUEST_DEEP_POWER_DOWN,
                              status);
    PS_HW6_SM_ParkOspiClocksForStop();
    if (recovery_required != 0UL)
    {
      (void)PS_HW6_SM_Transition(PS_HW6_SM_STORAGE,
                                STORAGE_EV_RECOVER_OK,
                                status);
      (void)PS_HW6_SM_Transition(PS_HW6_SM_STORAGE,
                                STORAGE_EV_FLASH_READY,
                                status);
    }
    PS_HW6_TraceSwoLifecycle(PS_HW6_TRACE_SWO_FLASH_INIT_DONE);
  }

  g_ps_hw6_owner_sm_probe.flash_ospi_state_after =
    (uint32_t)HAL_OSPI_GetState(&hospi1);
  g_ps_hw6_owner_sm_probe.flash_ospi_error_after =
    HAL_OSPI_GetError(&hospi1);
  PS_HW6_SM_UpdateFlashDriverProbe();
  g_ps_hw6_owner_sm_probe.storage_flash_init_last_status =
    (uint32_t)status;
  return status;
}
HAL_StatusTypeDef PS_HW6_OwnerStateMachines_StartUsbExport(void)
{
  HAL_StatusTypeDef status = HAL_OK;
  ps_status_t storage_status;
  UINT usb_status;

  g_ps_hw6_owner_sm_probe.usb_export_request_count++;
  g_ps_hw6_owner_sm_probe.usb_export_start_tick = (uint32_t)tx_time_get();
  g_ps_hw6_owner_sm_probe.usb_export_vbus_present =
    (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_9) == GPIO_PIN_SET) ? 1UL : 0UL;
  g_ps_hw6_owner_sm_probe.usb_export_power_pmic_vbus_at_request =
    g_ps_hw6_owner_probe.power_vbus_ok;
  g_ps_hw6_owner_sm_probe.usb_export_power_mcu_vbus_at_request =
    g_ps_hw6_owner_probe.power_mcu_vbus_present;
  g_ps_hw6_owner_sm_probe.usb_export_power_vbus_agree_at_request =
    g_ps_hw6_owner_probe.power_vbus_agree;
  g_ps_hw6_owner_sm_probe.usb_export_policy_status = 0xFFFFFFFFUL;
  g_ps_hw6_owner_sm_probe.usb_export_flash_wake_status = 0xFFFFFFFFUL;
  g_ps_hw6_owner_sm_probe.usb_export_fxlx_open_status = 0xFFFFFFFFUL;
  g_ps_hw6_owner_sm_probe.usb_export_dcd_status = 0xFFFFFFFFUL;
  g_ps_hw6_owner_sm_probe.usb_export_pcd_init_status = 0xFFFFFFFFUL;
  g_ps_hw6_owner_sm_probe.usb_export_pcd_start_status = 0xFFFFFFFFUL;
  g_ps_hw6_owner_sm_probe.usb_export_irq_priority_before = 0xFFFFFFFFUL;
  g_ps_hw6_owner_sm_probe.usb_export_irq_priority_after = 0xFFFFFFFFUL;
  g_ps_hw6_owner_sm_probe.usb_export_devconnect_status = 0xFFFFFFFFUL;
  g_ps_hw6_owner_sm_probe.usb_export_started = 0UL;
  PS_HW6_SM_UpdateUsbHostAvailability(
    (uint32_t)PS_HW6_USB_HOST_EVENT_MSC_EXPORT_REQUEST);
  PS_HW6_TraceSwoLifecycle(PS_HW6_TRACE_SWO_MSC_EXPORT_START);

  status = PS_HW6_SM_PrepareStorageForUsbExport();
  if (status != HAL_OK)
  {
    g_ps_hw6_owner_sm_probe.usb_export_policy_status =
      PS_HW6_OWNER_SM_STATUS_NOT_RUN;
    PS_HW6_TraceSwoLifecycle(PS_HW6_TRACE_SWO_ERROR);
    return status;
  }

  if (g_ps_hw6_owner_sm_probe.current_state[PS_HW6_SM_STORAGE] !=
      STORAGE_FLASH_READY)
  {
    g_ps_hw6_owner_sm_probe.usb_export_policy_status =
      PS_HW6_OWNER_SM_STATUS_NOT_RUN;
    PS_HW6_TraceSwoLifecycle(PS_HW6_TRACE_SWO_ERROR);
    return HAL_ERROR;
  }

  (void)PS_HW6_SM_Transition(PS_HW6_SM_STORAGE,
                            STORAGE_EV_USB_VBUS_PRESENT, HAL_OK);
  storage_status = PS_HW6_SM_EnsureFlashAwake();
  g_ps_hw6_owner_sm_probe.usb_export_flash_wake_status =
    (uint32_t)storage_status;
  if (storage_status != PS_STATUS_OK)
  {
    (void)PS_HW6_SM_Transition(PS_HW6_SM_STORAGE,
                              STORAGE_EV_FAULT, HAL_ERROR);
    PS_HW6_TraceSwoLifecycle(PS_HW6_TRACE_SWO_ERROR);
    return HAL_ERROR;
  }

  storage_status = ps_storage_filex_levelx_msc_open(
    &ps_flash_block,
    PS_HW6_SM_FindStorageRegion(PS_STORAGE_REGION_USB_STAGING));
  g_ps_hw6_owner_sm_probe.usb_export_fxlx_open_status =
    (uint32_t)storage_status;
  if (storage_status != PS_STATUS_OK)
  {
    (void)PS_HW6_SM_Transition(PS_HW6_SM_STORAGE,
                              STORAGE_EV_FAULT, HAL_ERROR);
    if (storage_status == PS_STATUS_RECOVERY_REQUIRED)
    {
      PS_HW6_TraceSwoLifecycle(PS_HW6_TRACE_SWO_MSC_RECOVERY_REQUIRED);
    }
    else
    {
      PS_HW6_TraceSwoLifecycle(PS_HW6_TRACE_SWO_ERROR);
    }
    return HAL_ERROR;
  }

  PS_HW6_TraceSwoLifecycle(PS_HW6_TRACE_SWO_MSC_OPEN_OK);
  PS_StorageMscBridge_SetPolicy(1UL, 1UL, 1UL);
  g_ps_hw6_owner_sm_probe.usb_export_policy_status = 0UL;

  usb_status = PS_HW6_UsbExport_StartDevice();
  status = (usb_status == TX_SUCCESS) ? HAL_OK : HAL_ERROR;

  g_ps_hw6_owner_sm_probe.usb_export_pcd_state_after =
    (uint32_t)HAL_PCD_GetState(&hpcd_USB_OTG_FS);
  g_ps_hw6_owner_sm_probe.usb_export_clock_enabled_after =
    (__HAL_RCC_USB_IS_CLK_ENABLED() != 0U) ? 1UL : 0UL;
  g_ps_hw6_owner_sm_probe.usb_export_vddusb_enabled_after =
    (READ_BIT(PWR->SVMCR, PWR_SVMCR_USV) != 0U) ? 1UL : 0UL;

  if (status == HAL_OK)
  {
    g_ps_hw6_owner_sm_probe.usb_export_started = 1UL;
    g_ps_hw6_owner_sm_probe.usb_parked = 0UL;
    (void)PS_HW6_SM_Transition(PS_HW6_SM_STORAGE,
                              STORAGE_EV_USB_MSC_ENTRY_ACCEPTED,
                              status);
    PS_HW6_SM_UpdateUsbHostAvailability(
      (uint32_t)PS_HW6_USB_HOST_EVENT_MSC_ACTIVE);
  }
  else
  {
    PS_StorageMscBridge_SetPolicy(0UL, 1UL, 1UL);
    (void)ps_storage_filex_levelx_msc_close();
    (void)PS_HW6_SM_Transition(PS_HW6_SM_STORAGE,
                              STORAGE_EV_FAULT, status);
    PS_HW6_TraceSwoLifecycle(PS_HW6_TRACE_SWO_ERROR);
  }

  return status;
}

HAL_StatusTypeDef PS_HW6_OwnerStateMachines_ReclaimUsbExport(uint32_t force_stage_rescan)
{
  HAL_StatusTypeDef status;
  ps_status_t storage_status;
  UINT usb_status;
  uint32_t normalized_force_rescan;
  uint32_t rescan_required;

  g_ps_hw6_owner_sm_probe.usb_reclaim_request_count++;
  g_ps_hw6_owner_sm_probe.usb_reclaim_start_tick = (uint32_t)tx_time_get();
  normalized_force_rescan = (force_stage_rescan != 0UL) ? 1UL : 0UL;
  g_ps_hw6_owner_sm_probe.usb_reclaim_dirty_seen =
    g_ps_storage_msc_bridge_probe.dirty;
  g_ps_hw6_owner_sm_probe.usb_stage_rescan_dirty_seen =
    g_ps_hw6_owner_sm_probe.usb_reclaim_dirty_seen;
  g_ps_hw6_owner_sm_probe.usb_stage_rescan_forced =
    normalized_force_rescan;
  g_ps_hw6_owner_sm_probe.usb_stage_rescan_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.usb_stage_rescan_package_scan_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.usb_stage_rescan_classification =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.usb_stage_rescan_entry_count = 0UL;
  g_ps_hw6_owner_sm_probe.usb_stage_rescan_file_count = 0UL;
  g_ps_hw6_owner_sm_probe.usb_stage_rescan_directory_count = 0UL;
  g_ps_hw6_owner_sm_probe.usb_stage_rescan_package_candidate_count = 0UL;
  g_ps_hw6_owner_sm_probe.usb_stage_rescan_unsupported_count = 0UL;
  g_ps_hw6_owner_sm_probe.usb_stage_rescan_bounded = 0UL;
  g_ps_hw6_owner_sm_probe.usb_stage_rescan_first_entry_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.usb_stage_rescan_last_entry_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.usb_stage_rescan_lx_open_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.usb_stage_rescan_fx_open_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.usb_stage_rescan_fx_close_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.usb_stage_rescan_lx_close_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.package_candidate_pending = 0UL;
  g_ps_hw6_owner_sm_probe.usb_reclaim_fxlx_close_status =
    0xFFFFFFFFUL;
  g_ps_hw6_owner_sm_probe.usb_reclaim_devdisconnect_status =
    0xFFFFFFFFUL;
  g_ps_hw6_owner_sm_probe.usb_reclaim_disconnect_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.usb_reclaim_pcd_stop_status =
    0xFFFFFFFFUL;
  g_ps_hw6_owner_sm_probe.usb_reclaim_deinit_status =
    0xFFFFFFFFUL;
  PS_HW6_SM_UpdateUsbHostAvailability(
    (uint32_t)PS_HW6_USB_HOST_EVENT_MSC_RECLAIM);
  PS_HW6_TraceSwoLifecycle(PS_HW6_TRACE_SWO_MSC_RECLAIM_START);

  if (g_ps_hw6_owner_sm_probe.current_state[PS_HW6_SM_STORAGE] ==
      STORAGE_USB_STAGING_EXPORTED)
  {
    if (g_ps_storage_msc_bridge_probe.dirty != 0UL)
    {
      (void)PS_HW6_SM_Transition(PS_HW6_SM_STORAGE,
                                STORAGE_EV_USB_HOST_DIRTY,
                                HAL_OK);
    }
  }

  if ((g_ps_hw6_owner_sm_probe.current_state[PS_HW6_SM_STORAGE] !=
       STORAGE_USB_STAGING_EXPORTED) &&
      (g_ps_hw6_owner_sm_probe.current_state[PS_HW6_SM_STORAGE] !=
       STORAGE_USB_STAGING_DIRTY))
  {
    g_ps_hw6_owner_sm_probe.usb_reclaim_disconnect_status =
      PS_HW6_OWNER_SM_STATUS_NOT_RUN;
    PS_HW6_TraceSwoLifecycle(PS_HW6_TRACE_SWO_ERROR);
    return HAL_ERROR;
  }

  PS_StorageMscBridge_SetPolicy(0UL, 1UL, 1UL);
  (void)PS_HW6_SM_Transition(PS_HW6_SM_STORAGE,
                            STORAGE_EV_USB_RELEASE_REQUEST,
                            HAL_OK);

  usb_status = PS_HW6_UsbExport_StopDevice();
  status = (usb_status == TX_SUCCESS) ? HAL_OK : HAL_ERROR;

  storage_status = ps_storage_filex_levelx_msc_close();
  g_ps_hw6_owner_sm_probe.usb_reclaim_fxlx_close_status =
    (uint32_t)storage_status;
  if ((status == HAL_OK) && (storage_status != PS_STATUS_OK))
  {
    status = HAL_ERROR;
  }

  g_ps_hw6_owner_sm_probe.usb_reclaim_pcd_state_after =
    (uint32_t)HAL_PCD_GetState(&hpcd_USB_OTG_FS);
  g_ps_hw6_owner_sm_probe.usb_reclaim_clock_enabled_after =
    (__HAL_RCC_USB_IS_CLK_ENABLED() != 0U) ? 1UL : 0UL;
  g_ps_hw6_owner_sm_probe.usb_reclaim_vddusb_enabled_after =
    (READ_BIT(PWR->SVMCR, PWR_SVMCR_USV) != 0U) ? 1UL : 0UL;
  g_ps_hw6_owner_sm_probe.usb_reclaim_parked =
    ((status == HAL_OK) &&
     (g_ps_hw6_owner_sm_probe.usb_reclaim_clock_enabled_after == 0UL) &&
     (g_ps_hw6_owner_sm_probe.usb_reclaim_vddusb_enabled_after == 0UL)) ?
    1UL : 0UL;

  if (g_ps_hw6_owner_sm_probe.usb_reclaim_parked != 0UL)
  {
    g_ps_hw6_owner_sm_probe.usb_parked = 1UL;
    rescan_required =
      ((g_ps_hw6_owner_sm_probe.usb_reclaim_dirty_seen != 0UL) ||
       (normalized_force_rescan != 0UL)) ? 1UL : 0UL;
    if (rescan_required != 0UL)
    {
      g_ps_hw6_owner_sm_probe.usb_stage_rescan_request_count++;
      g_ps_hw6_owner_sm_probe.usb_stage_rescan_start_tick =
        (uint32_t)tx_time_get();
      g_ps_hw6_owner_sm_probe.usb_stage_rescan_pending = 1UL;
    }
    status = PS_HW6_SM_RunUsbStageRescanScaffold();
    if (status == HAL_OK)
    {
      (void)PS_HW6_SM_Transition(PS_HW6_SM_STORAGE,
                                STORAGE_EV_USB_RESCAN_OK,
                                HAL_OK);
      PS_HW6_TraceSwoLifecycle(PS_HW6_TRACE_SWO_MSC_RECLAIM_DONE);
    }
    else
    {
      (void)PS_HW6_SM_Transition(PS_HW6_SM_STORAGE,
                                STORAGE_EV_FAULT, status);
      PS_HW6_TraceSwoLifecycle(PS_HW6_TRACE_SWO_ERROR);
    }
  }
  else
  {
    (void)PS_HW6_SM_Transition(PS_HW6_SM_STORAGE,
                              STORAGE_EV_FAULT, status);
    PS_HW6_TraceSwoLifecycle(PS_HW6_TRACE_SWO_ERROR);
  }

  PS_HW6_SM_UpdateUsbHostAvailability(
    (uint32_t)PS_HW6_USB_HOST_EVENT_MSC_RECLAIM);

  return (g_ps_hw6_owner_sm_probe.usb_reclaim_parked != 0UL) ?
         HAL_OK : status;
}
static HAL_StatusTypeDef PS_HW6_SM_ParkUsb(void)
{
  HAL_StatusTypeDef status;
  UINT usb_status;

  g_ps_hw6_owner_sm_probe.usb_vbus_present =
    (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_9) == GPIO_PIN_SET) ? 1UL : 0UL;
  g_ps_hw6_owner_sm_probe.usb_pcd_state_before =
    (uint32_t)HAL_PCD_GetState(&hpcd_USB_OTG_FS);
  g_ps_hw6_owner_sm_probe.usb_clock_enabled_before =
    (__HAL_RCC_USB_IS_CLK_ENABLED() != 0U) ? 1UL : 0UL;
  g_ps_hw6_owner_sm_probe.usb_vddusb_enabled_before =
    (READ_BIT(PWR->SVMCR, PWR_SVMCR_USV) != 0U) ? 1UL : 0UL;

  g_ps_hw6_owner_sm_probe.usb_deinit_attempted = 1UL;
  usb_status = PS_HW6_UsbExport_StopDevice();
  g_ps_hw6_owner_sm_probe.usb_deinit_status = (uint32_t)usb_status;
  status = (usb_status == TX_SUCCESS) ? HAL_OK : HAL_ERROR;

  g_ps_hw6_owner_sm_probe.usb_pcd_state_after =
    (uint32_t)HAL_PCD_GetState(&hpcd_USB_OTG_FS);
  g_ps_hw6_owner_sm_probe.usb_clock_enabled_after =
    (__HAL_RCC_USB_IS_CLK_ENABLED() != 0U) ? 1UL : 0UL;
  g_ps_hw6_owner_sm_probe.usb_vddusb_enabled_after =
    (READ_BIT(PWR->SVMCR, PWR_SVMCR_USV) != 0U) ? 1UL : 0UL;
  g_ps_hw6_owner_sm_probe.usb_parked =
    ((status == HAL_OK) &&
     (g_ps_hw6_owner_sm_probe.usb_clock_enabled_after == 0UL) &&
     (g_ps_hw6_owner_sm_probe.usb_vddusb_enabled_after == 0UL)) ? 1UL : 0UL;

  PS_HW6_ClockPolicy_RecordHardwareSnapshot();
  return (g_ps_hw6_owner_sm_probe.usb_parked != 0UL) ? HAL_OK : status;
}

HAL_StatusTypeDef PS_HW6_OwnerStateMachines_ParkUsbForBoot(void)
{
  return PS_HW6_SM_ParkUsb();
}

static HAL_StatusTypeDef PS_HW6_SM_StabilizeStorage(void)
{
  ps_status_t driver_status;
  HAL_StatusTypeDef status;

  (void)PS_HW6_SM_Transition(PS_HW6_SM_STORAGE,
                            STORAGE_EV_INIT, HAL_OK);

  status = PS_HW6_SM_ParkUsb();
  if (status != HAL_OK)
  {
    (void)PS_HW6_SM_Transition(PS_HW6_SM_STORAGE,
                              STORAGE_EV_FAULT, status);
    goto storage_done;
  }

  (void)PS_HW6_SM_Transition(PS_HW6_SM_FLASH,
                            FLASH_EV_BOOT, HAL_OK);

  driver_status = ps_dev_at25sl128a_read_jedec(
    &ps_flash_device,
    &ps_flash_jedec_result);
  status = PS_HW6_SM_StatusToHal(driver_status);
  g_ps_hw6_owner_sm_probe.flash_jedec_status =
    ps_flash_jedec_result.hal_status;
  g_ps_hw6_owner_sm_probe.flash_jedec_id[0] =
    ps_flash_jedec_result.jedec_id[0];
  g_ps_hw6_owner_sm_probe.flash_jedec_id[1] =
    ps_flash_jedec_result.jedec_id[1];
  g_ps_hw6_owner_sm_probe.flash_jedec_id[2] =
    ps_flash_jedec_result.jedec_id[2];
  g_ps_hw6_owner_sm_probe.flash_identity_match =
    ps_flash_jedec_result.identity_match;
  PS_HW6_SM_UpdateFlashDriverProbe();

  if (g_ps_hw6_owner_sm_probe.flash_identity_match == 0UL)
  {
    (void)PS_HW6_SM_Transition(PS_HW6_SM_FLASH,
                              FLASH_EV_PROBE_FAIL, status);
    (void)PS_HW6_SM_Transition(PS_HW6_SM_STORAGE,
                              STORAGE_EV_FAULT, status);
    goto storage_done;
  }

  (void)PS_HW6_SM_Transition(PS_HW6_SM_FLASH,
                            FLASH_EV_PROBE_OK, HAL_OK);
  (void)PS_HW6_SM_Transition(PS_HW6_SM_FLASH,
                            FLASH_EV_CONFIG_OK, HAL_OK);

  driver_status = ps_dev_at25sl128a_run_scratch_test(
    &ps_flash_device,
    PS_HW6_FLASH_SCRATCH_ADDRESS,
    &ps_flash_scratch_result);
  status = PS_HW6_SM_StatusToHal(driver_status);
  PS_HW6_SM_RecordFlashScratchResult(&ps_flash_scratch_result);
  PS_HW6_SM_UpdateFlashDriverProbe();
  if (status != HAL_OK)
  {
    (void)PS_HW6_SM_Transition(PS_HW6_SM_FLASH,
                              FLASH_EV_FAULT, status);
    (void)PS_HW6_SM_Transition(PS_HW6_SM_STORAGE,
                              STORAGE_EV_FAULT, status);
    goto storage_done;
  }

  driver_status = ps_storage_flash_block_run_scratch_test(
    &ps_flash_block,
    PS_HW6_FLASH_SCRATCH_BLOCK_INDEX,
    &ps_flash_block_result);
  status = PS_HW6_SM_StatusToHal(driver_status);
  PS_HW6_SM_RecordFlashBlockResult(&ps_flash_block_result);
  PS_HW6_SM_UpdateFlashBlockProbe();
  PS_HW6_SM_UpdateFlashDriverProbe();
  if (status != HAL_OK)
  {
    (void)PS_HW6_SM_Transition(PS_HW6_SM_FLASH,
                              FLASH_EV_FAULT, status);
    (void)PS_HW6_SM_Transition(PS_HW6_SM_STORAGE,
                              STORAGE_EV_FAULT, status);
    goto storage_done;
  }

  driver_status = ps_storage_layout_validate(
    &ps_flash_block.geometry,
    &ps_storage_layout_result);
  status = PS_HW6_SM_StatusToHal(driver_status);
  PS_HW6_SM_RecordStorageLayoutResult(&ps_storage_layout_result);
  if (status != HAL_OK)
  {
    (void)PS_HW6_SM_Transition(PS_HW6_SM_FLASH,
                              FLASH_EV_FAULT, status);
    (void)PS_HW6_SM_Transition(PS_HW6_SM_STORAGE,
                              STORAGE_EV_FAULT, status);
    goto storage_done;
  }

  g_ps_hw6_owner_sm_probe.storage_fxlx_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  PS_StorageMscBridge_SetPolicy(0UL, 1UL, 1UL);

  driver_status = ps_dev_at25sl128a_enter_deep_power_down(
    &ps_flash_device,
    &ps_flash_command_result);
  status = PS_HW6_SM_StatusToHal(driver_status);
  g_ps_hw6_owner_sm_probe.flash_deep_power_down_status =
    ps_flash_command_result.hal_status;
  if (status == HAL_OK)
  {
    (void)PS_HW6_SM_Transition(PS_HW6_SM_FLASH,
                              FLASH_EV_REQUEST_DEEP_POWER_DOWN, status);
    PS_HW6_SM_ParkOspiClocksForStop();
    (void)PS_HW6_SM_Transition(PS_HW6_SM_STORAGE,
                              STORAGE_EV_FLASH_READY, status);
  }
  else
  {
    (void)PS_HW6_SM_Transition(PS_HW6_SM_FLASH,
                              FLASH_EV_FAULT, status);
    (void)PS_HW6_SM_Transition(PS_HW6_SM_STORAGE,
                              STORAGE_EV_FAULT, status);
  }

storage_done:
  g_ps_hw6_owner_sm_probe.flash_ospi_state_after =
    (uint32_t)HAL_OSPI_GetState(&hospi1);
  g_ps_hw6_owner_sm_probe.flash_ospi_error_after =
    HAL_OSPI_GetError(&hospi1);
  if ((status == HAL_OK) &&
      (g_ps_hw6_owner_sm_probe.flash_ospi_error_after != HAL_OSPI_ERROR_NONE))
  {
    status = HAL_ERROR;
  }
  PS_HW6_SM_UpdateFlashDriverProbe();
  return status;
}

static ps_status_t PS_HW6_SM_EnsureFlashAwake(void)
{
  ps_status_t status;

  if (ps_flash_device.state == PS_DEV_AT25SL128A_STATE_DEEP_POWER_DOWN)
  {
    PS_HW6_SM_RestoreOspiClocksAfterStop();
    status = ps_dev_at25sl128a_release_from_deep_power_down(
      &ps_flash_device,
      &ps_flash_command_result);
    PS_HW6_SM_UpdateFlashDriverProbe();
    return status;
  }
  return PS_STATUS_OK;
}

static ps_status_t PS_HW6_SM_HandleStorageMscRead(
  ps_storage_msc_request_t *request)
{
  ps_status_t status;

  if ((request == NULL) || (request->data == NULL) ||
      (request->block_count == 0UL))
  {
    return PS_STATUS_INVALID_ARGUMENT;
  }

  status = PS_HW6_SM_EnsureFlashAwake();
  if (status == PS_STATUS_OK)
  {
    status = ps_storage_filex_levelx_msc_read(request->lba,
                                              request->block_count,
                                              request->data);
  }
  PS_HW6_SM_UpdateFlashBlockProbe();
  PS_HW6_SM_UpdateFlashDriverProbe();
  g_ps_storage_msc_bridge_probe.read_count++;
  return status;
}

static ps_status_t PS_HW6_SM_HandleStorageMscWrite(
  ps_storage_msc_request_t *request)
{
  ps_status_t status;

  if ((request == NULL) || (request->data == NULL) ||
      (request->block_count == 0UL))
  {
    return PS_STATUS_INVALID_ARGUMENT;
  }

  status = PS_HW6_SM_EnsureFlashAwake();
  if (status == PS_STATUS_OK)
  {
    status = ps_storage_filex_levelx_msc_write(request->lba,
                                               request->block_count,
                                               request->data);
  }

  PS_HW6_SM_UpdateFlashBlockProbe();
  PS_HW6_SM_UpdateFlashDriverProbe();
  g_ps_storage_msc_bridge_probe.write_count++;
  return status;
}

void PS_HW6_OwnerStateMachines_HandleStorageMsc(uint32_t command)
{
  ps_storage_msc_request_t *request = PS_StorageMscBridge_CurrentRequest();
  ps_status_t status = PS_STATUS_OK;
  uint32_t storage_state =
    g_ps_hw6_owner_sm_probe.current_state[PS_HW6_SM_STORAGE];

  if ((request == NULL) ||
      ((storage_state != STORAGE_USB_STAGING_EXPORTED) &&
       (storage_state != STORAGE_USB_STAGING_DIRTY)))
  {
    status = PS_STATUS_INVALID_STATE;
  }
  else if (command == PS_HW6_RTOS_STORAGE_MSC_READ)
  {
    status = PS_HW6_SM_HandleStorageMscRead(request);
  }
  else if (command == PS_HW6_RTOS_STORAGE_MSC_WRITE)
  {
    status = PS_HW6_SM_HandleStorageMscWrite(request);
  }
  else if (command == PS_HW6_RTOS_STORAGE_MSC_FLUSH)
  {
    if (ps_storage_filex_levelx_msc_is_open() == 0UL)
    {
      status = PS_STATUS_INVALID_STATE;
    }
    g_ps_storage_msc_bridge_probe.flush_count++;
  }
  else if (command == PS_HW6_RTOS_STORAGE_MSC_STATUS)
  {
    if (ps_storage_filex_levelx_msc_is_open() == 0UL)
    {
      status = PS_STATUS_INVALID_STATE;
    }
    g_ps_storage_msc_bridge_probe.status_count++;
  }
  else
  {
    status = PS_STATUS_INVALID_ARGUMENT;
  }

  PS_StorageMscBridge_Complete((status == PS_STATUS_OK) ? UX_SUCCESS : UX_ERROR,
                               (status == PS_STATUS_OK) ? 0UL : 1UL,
                               (uint32_t)status);
  PS_HW6_SM_UpdateUsbHostAvailability(
    (uint32_t)PS_HW6_USB_HOST_EVENT_MSC_MEDIA_COMMAND);
}
static uint32_t PS_HW6_SM_BufferContains(const uint8_t *buffer,
                                         uint32_t length,
                                         const char *needle)
{
  uint32_t needle_length = (uint32_t)strlen(needle);
  uint32_t start;
  uint32_t index;

  if ((buffer == NULL) || (needle == NULL) ||
      (needle_length == 0U) || (needle_length > length))
  {
    return 0UL;
  }

  for (start = 0U; start <= (length - needle_length); ++start)
  {
    uint32_t match = 1UL;
    for (index = 0U; index < needle_length; ++index)
    {
      if (buffer[start + index] != (uint8_t)needle[index])
      {
        match = 0UL;
        break;
      }
    }
    if (match != 0UL)
    {
      return 1UL;
    }
  }
  return 0UL;
}

static uint32_t PS_HW6_SM_NinaReceiveUntilQuiet(uint8_t *buffer,
                                                uint32_t capacity,
                                                ULONG window_ticks,
                                                ULONG quiet_ticks)
{
  ULONG start_tick = tx_time_get();
  ULONG last_rx_tick = start_tick;
  uint32_t count = 0U;

  while (((tx_time_get() - start_tick) < window_ticks) &&
         (count < capacity))
  {
    uint8_t byte = 0U;
    if (HAL_UART_Receive(&hlpuart1, &byte, 1U,
                         PS_HW6_NINA_RX_BYTE_TIMEOUT_MS) == HAL_OK)
    {
      buffer[count++] = byte;
      last_rx_tick = tx_time_get();
    }
    else if ((count != 0U) &&
             ((tx_time_get() - last_rx_tick) >= quiet_ticks))
    {
      break;
    }
  }
  return count;
}

static void PS_HW6_SM_NinaRecordIdentityPreview(const uint8_t *buffer,
                                                uint32_t length)
{
  uint32_t copy_length;
  uint32_t index;

  if (buffer == NULL)
  {
    g_ps_hw6_owner_sm_probe.ble_identity_response[0] = '\0';
    return;
  }

  copy_length = length;
  if (copy_length >= PS_HW6_OWNER_SM_NINA_IDENTITY_BYTES)
  {
    copy_length = PS_HW6_OWNER_SM_NINA_IDENTITY_BYTES - 1U;
  }

  for (index = 0U; index < copy_length; ++index)
  {
    uint8_t value = buffer[index];
    if ((value == (uint8_t)'\r') || (value == (uint8_t)'\n') ||
        (value == (uint8_t)'\t'))
    {
      value = (uint8_t)' ';
    }
    else if ((value < (uint8_t)' ') || (value > (uint8_t)'~'))
    {
      value = (uint8_t)'.';
    }
    g_ps_hw6_owner_sm_probe.ble_identity_response[index] = (char)value;
  }
  g_ps_hw6_owner_sm_probe.ble_identity_response[copy_length] = '\0';
}

static HAL_StatusTypeDef PS_HW6_SM_NinaCommand(uint32_t command_index,
                                               const char *command)
{
  HAL_StatusTypeDef tx_status;
  uint32_t rx_length;
  uint32_t ok;
  uint32_t error;

  if ((command_index >= PS_HW6_OWNER_SM_NINA_COMMAND_COUNT) ||
      (command == NULL))
  {
    return HAL_ERROR;
  }

  (void)memset(ps_nina_rx_buffer, 0, sizeof(ps_nina_rx_buffer));
  tx_status = HAL_UART_Transmit(
    &hlpuart1, (const uint8_t *)command, (uint16_t)strlen(command),
    PS_HW6_NINA_TX_TIMEOUT_MS);
  g_ps_hw6_owner_sm_probe.ble_command_tx_status[command_index] =
    (uint32_t)tx_status;
  g_ps_hw6_owner_sm_probe.ble_command_attempted_mask |=
    1UL << command_index;
  g_ps_hw6_owner_sm_probe.ble_command_count++;
  if (tx_status != HAL_OK)
  {
    return tx_status;
  }

  rx_length = PS_HW6_SM_NinaReceiveUntilQuiet(
    ps_nina_rx_buffer, sizeof(ps_nina_rx_buffer),
    PS_HW6_NINA_RX_WINDOW_TICKS, PS_HW6_NINA_RX_QUIET_TICKS);
  g_ps_hw6_owner_sm_probe.ble_command_rx_len[command_index] = rx_length;
  if (command_index == PS_HW6_NINA_IDENTITY_COMMAND_INDEX)
  {
    g_ps_hw6_owner_sm_probe.ble_identity_rx_len = rx_length;
    PS_HW6_SM_NinaRecordIdentityPreview(ps_nina_rx_buffer, rx_length);
  }
  ok = PS_HW6_SM_BufferContains(ps_nina_rx_buffer, rx_length, "OK");
  error = PS_HW6_SM_BufferContains(ps_nina_rx_buffer, rx_length, "ERROR");
  if (ok != 0UL)
  {
    g_ps_hw6_owner_sm_probe.ble_command_ok_mask |= 1UL << command_index;
  }
  if (error != 0UL)
  {
    g_ps_hw6_owner_sm_probe.ble_command_error_mask |= 1UL << command_index;
  }
  return ((ok != 0UL) && (error == 0UL)) ? HAL_OK : HAL_ERROR;
}

static void PS_HW6_SM_ConfigureNinaDsrHostControl(GPIO_PinState state)
{
  GPIO_InitTypeDef gpio = {0};

  __HAL_RCC_GPIOC_CLK_ENABLE();
  HAL_GPIO_WritePin(PS_HW6_NINA_DSR_HOST_CONTROL_PORT,
                    PS_HW6_NINA_DSR_HOST_CONTROL_PIN, state);
  gpio.Pin = PS_HW6_NINA_DSR_HOST_CONTROL_PIN;
  gpio.Mode = GPIO_MODE_OUTPUT_PP;
  gpio.Pull = GPIO_NOPULL;
  gpio.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(PS_HW6_NINA_DSR_HOST_CONTROL_PORT, &gpio);
  g_ps_hw6_owner_sm_probe.ble_dsr_highz_configured = 0UL;
  if (state == GPIO_PIN_RESET)
  {
    g_ps_hw6_owner_sm_probe.ble_dsr_assert_tick =
      (uint32_t)tx_time_get();
  }
}
static void PS_HW6_SM_ReleaseNinaDsrHostControl(void)
{
  GPIO_InitTypeDef gpio = {0};

  __HAL_RCC_GPIOC_CLK_ENABLE();
  gpio.Pin = PS_HW6_NINA_DSR_HOST_CONTROL_PIN;
  gpio.Mode = GPIO_MODE_ANALOG;
  gpio.Pull = GPIO_NOPULL;
  gpio.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(PS_HW6_NINA_DSR_HOST_CONTROL_PORT, &gpio);
  g_ps_hw6_owner_sm_probe.ble_dsr_highz_configured = 1UL;
}

static GPIO_PinState PS_HW6_SM_NinaSleepDsrState(void)
{
  return (g_ps_hw6_ble_sleep_dsr_deasserted != 0UL) ?
    GPIO_PIN_SET : GPIO_PIN_RESET;
}

static void PS_HW6_SM_ApplyNinaSleepDsrState(void)
{
  GPIO_PinState target_state = PS_HW6_SM_NinaSleepDsrState();
  uint32_t start_tick;

  g_ps_hw6_owner_sm_probe.ble_sleep_dsr_deasserted =
    g_ps_hw6_ble_sleep_dsr_deasserted;
  g_ps_hw6_owner_sm_probe.ble_dsr_sleep_target_level =
    (uint32_t)target_state;
  g_ps_hw6_owner_sm_probe.ble_stop_settle_ticks =
    (uint32_t)PS_HW6_NINA_STOP_SETTLE_TICKS;
  g_ps_hw6_owner_sm_probe.ble_dsr_before_sleep_level =
    (uint32_t)HAL_GPIO_ReadPin(PS_HW6_NINA_DSR_HOST_CONTROL_PORT,
                              PS_HW6_NINA_DSR_HOST_CONTROL_PIN);

  start_tick = (uint32_t)tx_time_get();
  g_ps_hw6_owner_sm_probe.ble_stop_settle_start_tick = start_tick;
  HAL_GPIO_WritePin(PS_HW6_NINA_DSR_HOST_CONTROL_PORT,
                    PS_HW6_NINA_DSR_HOST_CONTROL_PIN, target_state);
  if (target_state == GPIO_PIN_SET)
  {
    g_ps_hw6_owner_sm_probe.ble_dsr_deassert_tick = start_tick;
  }
  else
  {
    g_ps_hw6_owner_sm_probe.ble_dsr_assert_tick = start_tick;
  }

  tx_thread_sleep(PS_HW6_NINA_STOP_SETTLE_TICKS);
  g_ps_hw6_owner_sm_probe.ble_stop_settle_end_tick =
    (uint32_t)tx_time_get();
  g_ps_hw6_owner_sm_probe.ble_dsr_after_sleep_level =
    (uint32_t)HAL_GPIO_ReadPin(PS_HW6_NINA_DSR_HOST_CONTROL_PORT,
                              PS_HW6_NINA_DSR_HOST_CONTROL_PIN);
}

static void PS_HW6_SM_NinaFail(HAL_StatusTypeDef status)
{
  uint32_t state = g_ps_hw6_owner_sm_probe.current_state[PS_HW6_SM_BLE];

  if (state == BLE_BOOT_WAIT)
  {
    (void)PS_HW6_SM_Transition(PS_HW6_SM_BLE,
                              BLE_EV_BOOT_TIMEOUT, status);
  }
  else if (state == BLE_CONFIG)
  {
    (void)PS_HW6_SM_Transition(PS_HW6_SM_BLE,
                              BLE_EV_CONFIG_FAIL, status);
  }
  else
  {
    (void)PS_HW6_SM_Transition(PS_HW6_SM_BLE,
                              BLE_EV_FAULT, status);
  }

  PS_HW6_SM_ConfigureNinaDsrHostControl(GPIO_PIN_RESET);
  HAL_GPIO_WritePin(NINA_NRST_GPIO_Port, NINA_NRST_Pin, GPIO_PIN_RESET);
  g_ps_hw6_owner_sm_probe.ble_fallback_reset_asserted = 1UL;
  g_ps_hw6_owner_sm_probe.ble_uart_deinit_status =
    (uint32_t)HAL_UART_DeInit(&hlpuart1);
}

static HAL_StatusTypeDef PS_HW6_SM_StabilizeBle(void)
{
  static const char *const commands[PS_HW6_OWNER_SM_NINA_COMMAND_COUNT] =
  {
    "AT\r\n",
    NULL,
    NULL,
    "AT+UBTDM=1\r\n",
    "AT+UBTCM=1\r\n",
    "AT+UBTPM=1\r\n",
    "AT&D4\r\n",
    "AT+UBTLEDIS?\r\n"
  };
  HAL_StatusTypeDef status = HAL_OK;
  HAL_StatusTypeDef command_status;
  uint32_t command_index;

  g_ps_hw6_owner_sm_probe.ble_command_required_mask =
    PS_HW6_NINA_REQUIRED_COMMAND_MASK;
  g_ps_hw6_owner_sm_probe.ble_command_skipped_mask =
    PS_HW6_NINA_UNSUPPORTED_COMMAND_MASK;
  g_ps_hw6_owner_sm_probe.ble_identity_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.ble_identity_rx_len = 0UL;
  g_ps_hw6_owner_sm_probe.ble_identity_response[0] = '\0';
  g_ps_hw6_owner_sm_probe.ble_sleep_dsr_deasserted =
    g_ps_hw6_ble_sleep_dsr_deasserted;
  g_ps_hw6_owner_sm_probe.ble_stop_settle_ticks =
    (uint32_t)PS_HW6_NINA_STOP_SETTLE_TICKS;

  (void)PS_HW6_SM_Transition(PS_HW6_SM_BLE,
                            BLE_EV_ENABLE_REQUEST, HAL_OK);
  g_ps_hw6_owner_sm_probe.ble_nrst_before =
    (uint32_t)HAL_GPIO_ReadPin(NINA_NRST_GPIO_Port, NINA_NRST_Pin);
  g_ps_hw6_owner_sm_probe.ble_dsr_host_control_before =
    (uint32_t)HAL_GPIO_ReadPin(PS_HW6_NINA_DSR_HOST_CONTROL_PORT,
                              PS_HW6_NINA_DSR_HOST_CONTROL_PIN);
  PS_HW6_SM_ConfigureNinaDsrHostControl(GPIO_PIN_RESET);

  HAL_GPIO_WritePin(NINA_NRST_GPIO_Port, NINA_NRST_Pin, GPIO_PIN_RESET);
  tx_thread_sleep(PS_HW6_NINA_RESET_TICKS);
  (void)PS_HW6_SM_Transition(PS_HW6_SM_BLE,
                            BLE_EV_RESET_ASSERTED, HAL_OK);
  HAL_GPIO_WritePin(NINA_NRST_GPIO_Port, NINA_NRST_Pin, GPIO_PIN_SET);
  g_ps_hw6_owner_sm_probe.ble_shutdown_reset_asserted = 0UL;
  g_ps_hw6_owner_sm_probe.ble_nrst_released =
    (uint32_t)HAL_GPIO_ReadPin(NINA_NRST_GPIO_Port, NINA_NRST_Pin);
  tx_thread_sleep(PS_HW6_NINA_BOOT_TICKS);

  (void)memset(ps_nina_rx_buffer, 0, sizeof(ps_nina_rx_buffer));
  g_ps_hw6_owner_sm_probe.ble_boot_rx_len =
    PS_HW6_SM_NinaReceiveUntilQuiet(
      ps_nina_rx_buffer, sizeof(ps_nina_rx_buffer),
      PS_HW6_NINA_BOOT_DRAIN_TICKS, PS_HW6_NINA_RX_QUIET_TICKS);

  status = PS_HW6_SM_NinaCommand(0U, commands[0]);
  if (status != HAL_OK)
  {
    PS_HW6_SM_NinaFail(status);
    goto ble_done;
  }
  (void)PS_HW6_SM_Transition(PS_HW6_SM_BLE,
                            BLE_EV_BOOT_READY, status);

  for (command_index = 1U;
       command_index < PS_HW6_OWNER_SM_NINA_COMMAND_COUNT;
       ++command_index)
  {
    if (commands[command_index] == NULL)
    {
      continue;
    }
    command_status = PS_HW6_SM_NinaCommand(command_index,
                                           commands[command_index]);
    if (command_index == PS_HW6_NINA_IDENTITY_COMMAND_INDEX)
    {
      g_ps_hw6_owner_sm_probe.ble_identity_status =
        (uint32_t)command_status;
      continue;
    }
    status = command_status;
    if (status != HAL_OK)
    {
      break;
    }
  }
  if ((status != HAL_OK) ||
      ((g_ps_hw6_owner_sm_probe.ble_command_attempted_mask &
        PS_HW6_NINA_REQUIRED_COMMAND_MASK) !=
       PS_HW6_NINA_REQUIRED_COMMAND_MASK) ||
      ((g_ps_hw6_owner_sm_probe.ble_command_ok_mask &
        PS_HW6_NINA_REQUIRED_COMMAND_MASK) !=
       PS_HW6_NINA_REQUIRED_COMMAND_MASK) ||
      ((g_ps_hw6_owner_sm_probe.ble_command_error_mask &
        PS_HW6_NINA_REQUIRED_COMMAND_MASK) != 0UL))
  {
    status = HAL_ERROR;
    PS_HW6_SM_NinaFail(status);
    goto ble_done;
  }

  (void)PS_HW6_SM_Transition(PS_HW6_SM_BLE,
                            BLE_EV_CONFIG_OK, status);
  (void)PS_HW6_SM_Transition(PS_HW6_SM_BLE,
                            BLE_EV_DISABLE_REQUEST, HAL_OK);
  PS_HW6_SM_ApplyNinaSleepDsrState();
  g_ps_hw6_owner_sm_probe.ble_uart_deinit_status =
    (uint32_t)HAL_UART_DeInit(&hlpuart1);
  if (g_ps_hw6_owner_sm_probe.ble_uart_deinit_status != (uint32_t)HAL_OK)
  {
    status = HAL_ERROR;
    PS_HW6_SM_NinaFail(status);
    goto ble_done;
  }
  (void)PS_HW6_SM_Transition(PS_HW6_SM_BLE,
                            BLE_EV_QUIESCE, HAL_OK);

ble_done:
  g_ps_hw6_owner_sm_probe.ble_nrst_after =
    (uint32_t)HAL_GPIO_ReadPin(NINA_NRST_GPIO_Port, NINA_NRST_Pin);
  g_ps_hw6_owner_sm_probe.ble_dsr_host_control_after =
    (uint32_t)HAL_GPIO_ReadPin(PS_HW6_NINA_DSR_HOST_CONTROL_PORT,
                              PS_HW6_NINA_DSR_HOST_CONTROL_PIN);
  g_ps_hw6_owner_sm_probe.ble_uart_state_after =
    (uint32_t)HAL_UART_GetState(&hlpuart1);
  g_ps_hw6_owner_sm_probe.ble_uart_error_after =
    HAL_UART_GetError(&hlpuart1);
  if ((status == HAL_OK) &&
      ((g_ps_hw6_owner_sm_probe.ble_nrst_after != (uint32_t)GPIO_PIN_SET) ||
       (g_ps_hw6_owner_sm_probe.ble_dsr_host_control_after !=
        g_ps_hw6_owner_sm_probe.ble_dsr_sleep_target_level) ||
       (g_ps_hw6_owner_sm_probe.current_state[PS_HW6_SM_BLE] !=
        (uint32_t)BLE_SUSPENDED)))
  {
    status = HAL_ERROR;
  }
  return status;
}

static uint32_t PS_HW6_SM_StateMatchMask(const uint32_t *expected_states)
{
  uint32_t match_mask = 0UL;
  uint32_t index;

  for (index = 0U; index < PS_HW6_OWNER_SM_COUNT; ++index)
  {
    if (g_ps_hw6_owner_sm_probe.current_state[index] ==
        expected_states[index])
    {
      match_mask |= 1UL << index;
    }
  }
  return match_mask;
}

static HAL_StatusTypeDef PS_HW6_SM_ResumePower(void)
{
  HAL_StatusTypeDef status = PS_HW6_PowerOwner_RunSnapshot();

  PS_HW6_SM_UpdateUsbHostAvailability(
    (uint32_t)PS_HW6_USB_HOST_EVENT_POWER_SNAPSHOT);

  (void)PS_HW6_SM_EvaluateBatteryPolicy(status, 0UL);
  if (status == HAL_OK)
  {
    if (PS_HW6_SM_Transition(PS_HW6_SM_POWER,
                             PWR_EV_RT_REQUEST, status) != HAL_OK)
    {
      status = HAL_ERROR;
    }
  }
  return status;
}

static HAL_StatusTypeDef PS_HW6_SM_QuiescePower(void)
{
  return PS_HW6_SM_Transition(PS_HW6_SM_POWER,
                              PWR_EV_LP_REQUEST, HAL_OK);
}

static HAL_StatusTypeDef PS_HW6_SM_ResumeDisplay(void)
{
  HAL_StatusTypeDef status;

  if (PS_HW6_SM_Transition(PS_HW6_SM_DISPLAY,
                           DISP_EV_RT_ENTER, HAL_OK) != HAL_OK)
  {
    return HAL_ERROR;
  }

  status = PS_HW6_DisplayOwner_RunPattern();
  if (status == HAL_OK)
  {
    (void)PS_HW6_SM_Transition(PS_HW6_SM_DISPLAY,
                              DISP_EV_STATIC_HOLD_REQ, status);
  }
  else
  {
    (void)PS_HW6_SM_Transition(PS_HW6_SM_DISPLAY,
                              DISP_EV_FAULT, status);
  }
  return status;
}

static HAL_StatusTypeDef PS_HW6_SM_QuiesceDisplay(void)
{
  return (g_ps_hw6_owner_sm_probe.current_state[PS_HW6_SM_DISPLAY] ==
          (uint32_t)DISP_STATIC_HOLD) ? HAL_OK : HAL_ERROR;
}

static HAL_StatusTypeDef PS_HW6_SM_ResumeAudio(void)
{
  if ((g_ps_hw6_owner_sm_probe.current_state[PS_HW6_SM_AUDIO] !=
       (uint32_t)AUDIO_IDLE) ||
      (g_ps_hw6_owner_sm_probe.current_state[PS_HW6_SM_SPEAKER] !=
       (uint32_t)SPK_OFF))
  {
    return HAL_ERROR;
  }
  return PS_HW6_AudioOwner_VerifyIdle();
}

static HAL_StatusTypeDef PS_HW6_SM_QuiesceAudio(void)
{
  if ((g_ps_hw6_owner_sm_probe.current_state[PS_HW6_SM_AUDIO] ==
       (uint32_t)AUDIO_IDLE) &&
      (g_ps_hw6_owner_sm_probe.current_state[PS_HW6_SM_SPEAKER] ==
       (uint32_t)SPK_OFF))
  {
    return PS_HW6_AudioOwner_VerifyIdle();
  }
  return HAL_ERROR;
}

static HAL_StatusTypeDef PS_HW6_SM_ResumeJoystick(uint32_t cycle_index)
{
  ps_dev_tmag3001_wake_result_t result;
  ps_status_t driver_status;
  HAL_StatusTypeDef status;

  if (PS_HW6_SM_Transition(PS_HW6_SM_JOYSTICK,
                           JOY_EV_RESUME, HAL_OK) != HAL_OK)
  {
    return HAL_ERROR;
  }

  PS_HW6_SM_ClearJoystickTerminalSleepProof();
  driver_status = ps_dev_tmag3001_wake_continuous(
    &ps_joystick_device,
    &result);
  status = PS_HW6_SM_StatusToHal(driver_status);

  if (cycle_index < PS_HW6_OWNER_SM_CYCLE_COUNT)
  {
    g_ps_hw6_owner_sm_probe.joystick_cycle_wake_probe_status[cycle_index] =
      (uint32_t)result.wake_probe_status;
    g_ps_hw6_owner_sm_probe.joystick_cycle_wake_retry_status[cycle_index] =
      (uint32_t)result.wake_retry_status;
    g_ps_hw6_owner_sm_probe.joystick_cycle_active_status[cycle_index] =
      (uint32_t)result.active_status;
    g_ps_hw6_owner_sm_probe
      .joystick_cycle_active_sensor_config1[cycle_index] =
      result.active_sensor_config1;
    g_ps_hw6_owner_sm_probe
      .joystick_cycle_active_device_config2[cycle_index] =
      result.active_device_config2;
  }
  PS_HW6_SM_UpdateJoystickDriverProbe();

  if (status == HAL_OK)
  {
    (void)PS_HW6_SM_Transition(PS_HW6_SM_JOYSTICK,
                              JOY_EV_PROBE_OK, HAL_OK);
    (void)PS_HW6_SM_Transition(PS_HW6_SM_JOYSTICK,
                              JOY_EV_SLOW_POLL_REQUEST, status);
  }
  else
  {
    (void)PS_HW6_SM_Transition(PS_HW6_SM_JOYSTICK,
                              JOY_EV_I2C_ERROR, status);
  }

  return status;
}

static HAL_StatusTypeDef PS_HW6_SM_QuiesceJoystick(uint32_t cycle_index)
{
  ps_dev_tmag3001_sleep_audit_result_t result;
  ps_status_t driver_status;
  HAL_StatusTypeDef status;
  uint32_t i2c_state_after = 0UL;
  uint32_t i2c_error_after = 0UL;
  uint32_t state = g_ps_hw6_owner_sm_probe.current_state[PS_HW6_SM_JOYSTICK];

  if (((state == (uint32_t)JOY_OFF) ||
       (state == (uint32_t)JOY_SUSPENDED)) &&
      (PS_HW6_SM_JoystickTerminalSleepProofValid() != 0UL))
  {
    (void)ps_hw_i2c3_diagnostics(&i2c_state_after, &i2c_error_after);
    g_ps_hw6_owner_sm_probe.joystick_i2c_state_after = i2c_state_after;
    g_ps_hw6_owner_sm_probe.joystick_i2c_error_after = i2c_error_after;
    if (cycle_index < PS_HW6_OWNER_SM_CYCLE_COUNT)
    {
      g_ps_hw6_owner_sm_probe.joystick_cycle_sleep_status[cycle_index] =
        (uint32_t)HAL_OK;
    }
    PS_HW6_SM_UpdateJoystickDriverProbe();
    return HAL_OK;
  }

  driver_status = ps_dev_tmag3001_prepare_sleep(
    &ps_joystick_device,
    PS_HW6_TMAG_STOP2_INT_CONFIG1_TARGET,
    &result);
  status = PS_HW6_SM_StatusToHal(driver_status);

  if (cycle_index < PS_HW6_OWNER_SM_CYCLE_COUNT)
  {
    g_ps_hw6_owner_sm_probe.joystick_cycle_sleep_status[cycle_index] =
      (uint32_t)result.sleep_write_status;
  }
  g_ps_hw6_owner_sm_probe.joystick_ready_status =
    (uint32_t)result.ready_status;
  g_ps_hw6_owner_sm_probe.joystick_identity_status =
    (uint32_t)result.identity_status;
  g_ps_hw6_owner_sm_probe.joystick_device_id = result.device_id;
  g_ps_hw6_owner_sm_probe.joystick_manufacturer_lsb =
    result.manufacturer_lsb;
  g_ps_hw6_owner_sm_probe.joystick_manufacturer_msb =
    result.manufacturer_msb;
  g_ps_hw6_owner_sm_probe.joystick_identity_match = result.identity_match;
  g_ps_hw6_owner_sm_probe.joystick_sensor_config1_before =
    result.sensor_config1_before;
  g_ps_hw6_owner_sm_probe.joystick_sensor_config1_after =
    result.sensor_config1_after;
  g_ps_hw6_owner_sm_probe.joystick_device_config2_before =
    result.device_config2_before;
  g_ps_hw6_owner_sm_probe.joystick_device_config2_after =
    result.device_config2_after;
  g_ps_hw6_owner_sm_probe.joystick_device_config2_sleep =
    result.device_config2_sleep;
  g_ps_hw6_owner_sm_probe.joystick_write_ok_mask = result.write_ok_mask;
  g_ps_hw6_owner_sm_probe.joystick_verify_ok_mask = result.verify_ok_mask;
  g_ps_hw6_owner_sm_probe.joystick_sensor_config1_verify_status =
    (uint32_t)result.sensor_config1_verify_status;
  g_ps_hw6_owner_sm_probe.joystick_device_config2_verify_status =
    (uint32_t)result.device_config2_verify_status;
  g_ps_hw6_owner_sm_probe.joystick_sleep_write_status =
    (uint32_t)result.sleep_write_status;
  g_ps_hw6_owner_sm_probe.joystick_terminal_sleep_committed =
    result.terminal_sleep_committed;
  g_ps_hw6_owner_sm_probe.joystick_post_sleep_read_omitted =
    result.post_sleep_read_omitted;
  g_ps_hw6_owner_sm_probe.joystick_sleep_audit_int_config1_before =
    result.int_config1_before;
  g_ps_hw6_owner_sm_probe.joystick_sleep_audit_int_config1_target =
    result.int_config1_target;
  g_ps_hw6_owner_sm_probe.joystick_sleep_audit_int_config1_after =
    result.int_config1_after;
  g_ps_hw6_owner_sm_probe.joystick_sleep_audit_write_ok_mask =
    result.write_ok_mask;
  g_ps_hw6_owner_sm_probe.joystick_sleep_audit_verify_ok_mask =
    result.verify_ok_mask;
  (void)ps_hw_i2c3_diagnostics(&i2c_state_after, &i2c_error_after);
  g_ps_hw6_owner_sm_probe.joystick_i2c_state_after = i2c_state_after;
  g_ps_hw6_owner_sm_probe.joystick_i2c_error_after = i2c_error_after;
  PS_HW6_SM_UpdateJoystickDriverProbe();

  if (status == HAL_OK)
  {
    (void)PS_HW6_SM_Transition(PS_HW6_SM_JOYSTICK,
                              JOY_EV_QUIESCE, status);
  }
  else
  {
    (void)PS_HW6_SM_Transition(PS_HW6_SM_JOYSTICK,
                              JOY_EV_I2C_ERROR, status);
  }
  return status;
}

static HAL_StatusTypeDef PS_HW6_SM_ResumeImu(uint32_t cycle_index)
{
  ps_dev_lis2dux12_wake_result_t result;
  ps_status_t driver_status;
  HAL_StatusTypeDef status;

  if (PS_HW6_SM_Transition(PS_HW6_SM_IMU,
                           IMU_EV_RESUME, HAL_OK) != HAL_OK)
  {
    return HAL_ERROR;
  }

  PS_HW6_SM_ClearImuDeepPowerDownProof();
  driver_status = ps_dev_lis2dux12_wake_low_rate(
    &ps_imu_device,
    &result);
  status = PS_HW6_SM_StatusToHal(driver_status);
  if (cycle_index < PS_HW6_OWNER_SM_CYCLE_COUNT)
  {
    g_ps_hw6_owner_sm_probe.imu_cycle_wake_probe_status[cycle_index] =
      result.wake_probe_hal_status;
    g_ps_hw6_owner_sm_probe.imu_cycle_wake_probe_error[cycle_index] =
      result.wake_probe_hal_error;
    g_ps_hw6_owner_sm_probe.imu_cycle_wake_probe_accepted[cycle_index] =
      result.wake_probe_accepted;
    g_ps_hw6_owner_sm_probe.imu_cycle_whoami_status[cycle_index] =
      (uint32_t)PS_HW6_SM_StatusToHal(result.whoami_status);
    g_ps_hw6_owner_sm_probe.imu_cycle_whoami[cycle_index] = result.whoami;
    g_ps_hw6_owner_sm_probe.imu_cycle_active_ctrl5[cycle_index] =
      result.active_ctrl5;
    g_ps_hw6_owner_sm_probe.imu_cycle_active_status[cycle_index] =
      (uint32_t)status;
  }
  PS_HW6_SM_UpdateImuDriverProbe();

  if ((result.whoami_status == PS_STATUS_OK) &&
      (result.whoami == 0x47U))
  {
    (void)PS_HW6_SM_Transition(PS_HW6_SM_IMU,
                              IMU_EV_PROBE_OK, HAL_OK);
  }
  if (status == HAL_OK)
  {
    if (PS_HW6_SM_Transition(PS_HW6_SM_IMU,
                             IMU_EV_LOW_RATE_SAMPLE_REQUEST,
                             status) != HAL_OK)
    {
      status = HAL_ERROR;
    }
  }
  else
  {
    (void)PS_HW6_SM_Transition(PS_HW6_SM_IMU,
                              IMU_EV_I2C_ERROR, status);
  }
  return status;
}

static HAL_StatusTypeDef PS_HW6_SM_QuiesceImu(uint32_t cycle_index)
{
  ps_dev_lis2dux12_suspend_result_t result;
  ps_status_t driver_status;
  HAL_StatusTypeDef status;

  driver_status = ps_dev_lis2dux12_suspend(&ps_imu_device, &result);
  status = PS_HW6_SM_StatusToHal(driver_status);
  PS_HW6_SM_RecordImuSuspendResult(&result);
  if (cycle_index < PS_HW6_OWNER_SM_CYCLE_COUNT)
  {
    g_ps_hw6_owner_sm_probe.imu_cycle_sleep_status[cycle_index] =
      (uint32_t)status;
  }
  PS_HW6_SM_UpdateImuDriverProbe();
  if (status == HAL_OK)
  {
    if (PS_HW6_SM_Transition(PS_HW6_SM_IMU,
                             IMU_EV_QUIESCE, status) != HAL_OK)
    {
      status = HAL_ERROR;
    }
  }
  else
  {
    (void)PS_HW6_SM_Transition(PS_HW6_SM_IMU,
                              IMU_EV_I2C_ERROR, status);
  }
  return status;
}

static void PS_HW6_SM_ParkOspiClocksForStop(void)
{
  g_ps_hw6_owner_sm_probe.storage_ospi_park_count++;
  g_ps_hw6_owner_sm_probe.storage_ospi_park_ahb2enr1_before = RCC->AHB2ENR1;
  g_ps_hw6_owner_sm_probe.storage_ospi_park_ahb2enr2_before = RCC->AHB2ENR2;
  g_ps_hw6_owner_sm_probe.storage_ospi_park_ahb2smenr1_before = RCC->AHB2SMENR1;
  g_ps_hw6_owner_sm_probe.storage_ospi_park_ahb2smenr2_before = RCC->AHB2SMENR2;

  __HAL_RCC_OSPI1_CLK_SLEEP_DISABLE();
  __HAL_RCC_OCTOSPIM_CLK_SLEEP_DISABLE();
  __HAL_RCC_OSPI1_CLK_DISABLE();
  __HAL_RCC_OSPIM_CLK_DISABLE();

  g_ps_hw6_owner_sm_probe.storage_ospi_park_ahb2enr1_after = RCC->AHB2ENR1;
  g_ps_hw6_owner_sm_probe.storage_ospi_park_ahb2enr2_after = RCC->AHB2ENR2;
  g_ps_hw6_owner_sm_probe.storage_ospi_park_ahb2smenr1_after = RCC->AHB2SMENR1;
  g_ps_hw6_owner_sm_probe.storage_ospi_park_ahb2smenr2_after = RCC->AHB2SMENR2;
}

static void PS_HW6_SM_RestoreOspiClocksAfterStop(void)
{
  g_ps_hw6_owner_sm_probe.storage_ospi_restore_count++;
  g_ps_hw6_owner_sm_probe.storage_ospi_restore_ahb2enr1_before = RCC->AHB2ENR1;
  g_ps_hw6_owner_sm_probe.storage_ospi_restore_ahb2enr2_before = RCC->AHB2ENR2;
  g_ps_hw6_owner_sm_probe.storage_ospi_restore_ahb2smenr1_before = RCC->AHB2SMENR1;
  g_ps_hw6_owner_sm_probe.storage_ospi_restore_ahb2smenr2_before = RCC->AHB2SMENR2;

  __HAL_RCC_OSPIM_CLK_ENABLE();
  __HAL_RCC_OSPI1_CLK_ENABLE();
  __HAL_RCC_OCTOSPIM_CLK_SLEEP_ENABLE();
  __HAL_RCC_OSPI1_CLK_SLEEP_ENABLE();

  g_ps_hw6_owner_sm_probe.storage_ospi_restore_ahb2enr1_after = RCC->AHB2ENR1;
  g_ps_hw6_owner_sm_probe.storage_ospi_restore_ahb2enr2_after = RCC->AHB2ENR2;
  g_ps_hw6_owner_sm_probe.storage_ospi_restore_ahb2smenr1_after = RCC->AHB2SMENR1;
  g_ps_hw6_owner_sm_probe.storage_ospi_restore_ahb2smenr2_after = RCC->AHB2SMENR2;
}

static HAL_StatusTypeDef PS_HW6_SM_ResumeStorage(uint32_t cycle_index)
{
  ps_dev_at25sl128a_command_result_t command_result;
  ps_dev_at25sl128a_jedec_result_t jedec_result;
  ps_status_t driver_status;
  HAL_StatusTypeDef status;
  uint32_t identity_match;

  if ((g_ps_hw6_owner_sm_probe.current_state[PS_HW6_SM_STORAGE] !=
       (uint32_t)STORAGE_FLASH_READY) ||
      (g_ps_hw6_owner_sm_probe.current_state[PS_HW6_SM_FLASH] !=
       (uint32_t)FLASH_DEEP_POWER_DOWN))
  {
    return HAL_ERROR;
  }

  PS_HW6_SM_RestoreOspiClocksAfterStop();
  driver_status = ps_dev_at25sl128a_release_from_deep_power_down(
    &ps_flash_device,
    &command_result);
  status = PS_HW6_SM_StatusToHal(driver_status);
  if (cycle_index < PS_HW6_OWNER_SM_CYCLE_COUNT)
  {
    g_ps_hw6_owner_sm_probe.flash_cycle_release_status[cycle_index] =
      command_result.hal_status;
  }
  if (cycle_index == PS_HW6_POWER_QUIESCE_CYCLE_INDEX)
  {
    g_ps_hw6_owner_sm_probe.flash_power_release_status =
      command_result.hal_status;
  }
  if (status == HAL_OK)
  {
    tx_thread_sleep(PS_HW6_FLASH_WAKE_SETTLE_TICKS);
    (void)PS_HW6_SM_Transition(PS_HW6_SM_FLASH,
                              FLASH_EV_WAKE_REVALIDATE, status);
    driver_status = ps_dev_at25sl128a_read_jedec(
      &ps_flash_device,
      &jedec_result);
    status = PS_HW6_SM_StatusToHal(driver_status);
  }
  else
  {
    (void)memset(&jedec_result, 0, sizeof(jedec_result));
    jedec_result.hal_status = command_result.hal_status;
  }
  identity_match = jedec_result.identity_match;
  if (cycle_index < PS_HW6_OWNER_SM_CYCLE_COUNT)
  {
    g_ps_hw6_owner_sm_probe.flash_cycle_jedec_status[cycle_index] =
      jedec_result.hal_status;
    g_ps_hw6_owner_sm_probe.flash_cycle_identity_match[cycle_index] =
      identity_match;
  }
  if (cycle_index == PS_HW6_POWER_QUIESCE_CYCLE_INDEX)
  {
    g_ps_hw6_owner_sm_probe.flash_power_jedec_status =
      jedec_result.hal_status;
    g_ps_hw6_owner_sm_probe.flash_power_identity_match = identity_match;
  }
  PS_HW6_SM_UpdateFlashDriverProbe();

  if (identity_match != 0UL)
  {
    (void)PS_HW6_SM_Transition(PS_HW6_SM_FLASH,
                              FLASH_EV_PROBE_OK, HAL_OK);
    (void)PS_HW6_SM_Transition(PS_HW6_SM_FLASH,
                              FLASH_EV_CONFIG_OK, HAL_OK);
    return HAL_OK;
  }

  status = HAL_ERROR;
  if (g_ps_hw6_owner_sm_probe.current_state[PS_HW6_SM_FLASH] ==
      (uint32_t)FLASH_PROBE)
  {
    (void)PS_HW6_SM_Transition(PS_HW6_SM_FLASH,
                              FLASH_EV_PROBE_FAIL, status);
  }
  PS_HW6_SM_UpdateFlashDriverProbe();
  return status;
}

static HAL_StatusTypeDef PS_HW6_SM_QuiesceStorage(uint32_t cycle_index)
{
  ps_dev_at25sl128a_command_result_t command_result;
  ps_status_t driver_status;
  HAL_StatusTypeDef status;

  if ((g_ps_hw6_owner_sm_probe.current_state[PS_HW6_SM_STORAGE] !=
       (uint32_t)STORAGE_FLASH_READY) ||
      (g_ps_hw6_owner_sm_probe.current_state[PS_HW6_SM_FLASH] !=
       (uint32_t)FLASH_READY))
  {
    return HAL_ERROR;
  }


  driver_status = ps_dev_at25sl128a_enter_deep_power_down(
    &ps_flash_device,
    &command_result);
  status = PS_HW6_SM_StatusToHal(driver_status);
  if (cycle_index < PS_HW6_OWNER_SM_CYCLE_COUNT)
  {
    g_ps_hw6_owner_sm_probe
      .flash_cycle_deep_power_down_status[cycle_index] =
      command_result.hal_status;
  }
  if (cycle_index == PS_HW6_POWER_QUIESCE_CYCLE_INDEX)
  {
    g_ps_hw6_owner_sm_probe.flash_power_deep_power_down_status =
      command_result.hal_status;
  }
  if (status == HAL_OK)
  {
    (void)PS_HW6_SM_Transition(PS_HW6_SM_FLASH,
                              FLASH_EV_REQUEST_DEEP_POWER_DOWN, status);
    PS_HW6_SM_ParkOspiClocksForStop();
  }
  else
  {
    (void)PS_HW6_SM_Transition(PS_HW6_SM_FLASH,
                              FLASH_EV_FAULT, status);
  }
  PS_HW6_SM_UpdateFlashDriverProbe();
  return status;
}

static HAL_StatusTypeDef PS_HW6_SM_InitNinaUart(void)
{
  HAL_StatusTypeDef status;

  hlpuart1.FifoMode = UART_FIFOMODE_DISABLE;
  status = HAL_UART_Init(&hlpuart1);
  if (status == HAL_OK)
  {
    status = HAL_UARTEx_SetTxFifoThreshold(
      &hlpuart1, UART_TXFIFO_THRESHOLD_1_8);
  }
  if (status == HAL_OK)
  {
    status = HAL_UARTEx_SetRxFifoThreshold(
      &hlpuart1, UART_RXFIFO_THRESHOLD_1_8);
  }
  if (status == HAL_OK)
  {
    status = HAL_UARTEx_DisableFifoMode(&hlpuart1);
  }
  return status;
}

static HAL_StatusTypeDef PS_HW6_SM_ResumeBle(uint32_t cycle_index)
{
  HAL_StatusTypeDef status;

  if (PS_HW6_SM_Transition(PS_HW6_SM_BLE,
                           BLE_EV_RESUME, HAL_OK) != HAL_OK)
  {
    return HAL_ERROR;
  }

  PS_HW6_SM_ConfigureNinaDsrHostControl(GPIO_PIN_RESET);
  status = PS_HW6_SM_InitNinaUart();
  if (cycle_index < PS_HW6_OWNER_SM_CYCLE_COUNT)
  {
    g_ps_hw6_owner_sm_probe.ble_cycle_uart_init_status[cycle_index] =
      (uint32_t)status;
  }
  if (status == HAL_OK)
  {
    tx_thread_sleep(PS_HW6_NINA_WAKE_SETTLE_TICKS);
    (void)memset(ps_nina_rx_buffer, 0, sizeof(ps_nina_rx_buffer));
    (void)PS_HW6_SM_NinaReceiveUntilQuiet(
      ps_nina_rx_buffer, sizeof(ps_nina_rx_buffer),
      PS_HW6_NINA_BOOT_DRAIN_TICKS, PS_HW6_NINA_RX_QUIET_TICKS);
    status = PS_HW6_SM_NinaCommand(0U, "AT\r\n");
  }
  if (cycle_index < PS_HW6_OWNER_SM_CYCLE_COUNT)
  {
    g_ps_hw6_owner_sm_probe.ble_cycle_wake_at_status[cycle_index] =
      (uint32_t)status;
    g_ps_hw6_owner_sm_probe.ble_cycle_wake_rx_len[cycle_index] =
      g_ps_hw6_owner_sm_probe.ble_command_rx_len[0];
    g_ps_hw6_owner_sm_probe.ble_cycle_dsr_after_resume[cycle_index] =
      (uint32_t)HAL_GPIO_ReadPin(PS_HW6_NINA_DSR_HOST_CONTROL_PORT,
                                PS_HW6_NINA_DSR_HOST_CONTROL_PIN);
  }
  if (status == HAL_OK)
  {
    (void)PS_HW6_SM_Transition(PS_HW6_SM_BLE,
                              BLE_EV_BOOT_READY, status);
    (void)PS_HW6_SM_Transition(PS_HW6_SM_BLE,
                              BLE_EV_CONFIG_OK, status);
  }
  else
  {
    PS_HW6_SM_NinaFail(status);
  }
  return status;
}

static HAL_StatusTypeDef PS_HW6_SM_QuiesceBle(uint32_t cycle_index)
{
  HAL_StatusTypeDef status;

  if (PS_HW6_SM_Transition(PS_HW6_SM_BLE,
                           BLE_EV_DISABLE_REQUEST, HAL_OK) != HAL_OK)
  {
    return HAL_ERROR;
  }

  PS_HW6_SM_ApplyNinaSleepDsrState();
  status = HAL_UART_DeInit(&hlpuart1);
  if (cycle_index < PS_HW6_OWNER_SM_CYCLE_COUNT)
  {
    g_ps_hw6_owner_sm_probe.ble_cycle_suspend_uart_status[cycle_index] =
      (uint32_t)status;
    g_ps_hw6_owner_sm_probe.ble_cycle_dsr_after_quiesce[cycle_index] =
      (uint32_t)HAL_GPIO_ReadPin(PS_HW6_NINA_DSR_HOST_CONTROL_PORT,
                                PS_HW6_NINA_DSR_HOST_CONTROL_PIN);
  }
  if (status == HAL_OK)
  {
    (void)PS_HW6_SM_Transition(PS_HW6_SM_BLE,
                              BLE_EV_QUIESCE, status);
  }
  else
  {
    PS_HW6_SM_NinaFail(status);
  }
  return status;
}

static HAL_StatusTypeDef PS_HW6_SM_HardShutdownBle(void)
{
  HAL_StatusTypeDef status = HAL_OK;
  uint32_t state = g_ps_hw6_owner_sm_probe.current_state[PS_HW6_SM_BLE];

  if ((state == (uint32_t)BLE_IDLE) ||
      (state == (uint32_t)BLE_ADVERTISING) ||
      (state == (uint32_t)BLE_PAIRING) ||
      (state == (uint32_t)BLE_CONNECTED))
  {
    status = PS_HW6_SM_QuiesceBle(PS_HW6_POWER_QUIESCE_CYCLE_INDEX);
    state = g_ps_hw6_owner_sm_probe.current_state[PS_HW6_SM_BLE];
  }

  g_ps_hw6_owner_sm_probe.ble_uart_deinit_status =
    (uint32_t)HAL_UART_DeInit(&hlpuart1);
  if (g_ps_hw6_owner_sm_probe.ble_uart_deinit_status != (uint32_t)HAL_OK)
  {
    status = HAL_ERROR;
  }

  HAL_GPIO_WritePin(NINA_NRST_GPIO_Port, NINA_NRST_Pin, GPIO_PIN_RESET);
  g_ps_hw6_owner_sm_probe.ble_shutdown_reset_asserted = 1UL;
  PS_HW6_SM_ReleaseNinaDsrHostControl();

  state = g_ps_hw6_owner_sm_probe.current_state[PS_HW6_SM_BLE];
  if (state != (uint32_t)BLE_OFF)
  {
    if (PS_HW6_SM_Transition(PS_HW6_SM_BLE,
                             BLE_EV_DISABLE_REQUEST,
                             status) != HAL_OK)
    {
      status = HAL_ERROR;
    }
  }

  g_ps_hw6_owner_sm_probe.ble_nrst_after =
    (uint32_t)HAL_GPIO_ReadPin(NINA_NRST_GPIO_Port, NINA_NRST_Pin);
  g_ps_hw6_owner_sm_probe.ble_dsr_host_control_after =
    (uint32_t)HAL_GPIO_ReadPin(PS_HW6_NINA_DSR_HOST_CONTROL_PORT,
                              PS_HW6_NINA_DSR_HOST_CONTROL_PIN);
  g_ps_hw6_owner_sm_probe.ble_uart_state_after =
    (uint32_t)HAL_UART_GetState(&hlpuart1);
  g_ps_hw6_owner_sm_probe.ble_uart_error_after = HAL_UART_GetError(&hlpuart1);
  return status;
}

static HAL_StatusTypeDef PS_HW6_SM_EnsureBleIdle(void)
{
  HAL_StatusTypeDef status = HAL_OK;
  uint32_t state = g_ps_hw6_owner_sm_probe.current_state[PS_HW6_SM_BLE];

  if (state == (uint32_t)BLE_OFF)
  {
    status = PS_HW6_SM_StabilizeBle();
    state = g_ps_hw6_owner_sm_probe.current_state[PS_HW6_SM_BLE];
  }
  if ((status == HAL_OK) && (state == (uint32_t)BLE_SUSPENDED))
  {
    status = PS_HW6_SM_ResumeBle(PS_HW6_POWER_QUIESCE_CYCLE_INDEX);
    state = g_ps_hw6_owner_sm_probe.current_state[PS_HW6_SM_BLE];
  }

  if ((status == HAL_OK) &&
      (state != (uint32_t)BLE_IDLE) &&
      (state != (uint32_t)BLE_ADVERTISING) &&
      (state != (uint32_t)BLE_PAIRING) &&
      (state != (uint32_t)BLE_CONNECTED))
  {
    status = HAL_ERROR;
  }
  return status;
}

HAL_StatusTypeDef PS_HW6_OwnerStateMachines_SetBleMode(uint32_t mode)
{
  HAL_StatusTypeDef status = HAL_OK;
  uint32_t state;

  g_ps_hw6_owner_sm_probe.ble_mode_request_count++;
  g_ps_hw6_owner_sm_probe.ble_mode_requested = mode;
  g_ps_hw6_owner_sm_probe.ble_mode_last_tick = (uint32_t)tx_time_get();
  g_ps_hw6_owner_sm_probe.ble_mode_placeholder = 0UL;
  g_ps_hw6_owner_sm_probe.ble_mode_last_status = (uint32_t)HAL_ERROR;

  switch (mode)
  {
    case PS_HW6_COMM_BLE_MODE_RESET_HELD:
      status = PS_HW6_SM_HardShutdownBle();
      break;

    case PS_HW6_COMM_BLE_MODE_SLEEP_SYSTEM_OFF:
      state = g_ps_hw6_owner_sm_probe.current_state[PS_HW6_SM_BLE];
      if (state == (uint32_t)BLE_OFF)
      {
        status = PS_HW6_SM_StabilizeBle();
      }
      else if (state == (uint32_t)BLE_SUSPENDED)
      {
        status = HAL_OK;
      }
      else if ((state == (uint32_t)BLE_IDLE) ||
               (state == (uint32_t)BLE_ADVERTISING) ||
               (state == (uint32_t)BLE_PAIRING) ||
               (state == (uint32_t)BLE_CONNECTED))
      {
        status = PS_HW6_SM_QuiesceBle(PS_HW6_POWER_QUIESCE_CYCLE_INDEX);
      }
      else
      {
        status = HAL_ERROR;
      }
      break;

    case PS_HW6_COMM_BLE_MODE_SEARCHING:
      g_ps_hw6_owner_sm_probe.ble_mode_placeholder = 1UL;
      status = PS_HW6_SM_EnsureBleIdle();
      state = g_ps_hw6_owner_sm_probe.current_state[PS_HW6_SM_BLE];
      if ((status == HAL_OK) && (state == (uint32_t)BLE_CONNECTED))
      {
        status = PS_HW6_SM_Transition(PS_HW6_SM_BLE,
                                      BLE_EV_DISCONNECTED,
                                      HAL_OK);
        state = g_ps_hw6_owner_sm_probe.current_state[PS_HW6_SM_BLE];
      }
      if ((status == HAL_OK) && (state == (uint32_t)BLE_PAIRING))
      {
        status = PS_HW6_SM_Transition(PS_HW6_SM_BLE,
                                      BLE_EV_PAIRING_DONE,
                                      HAL_OK);
        state = g_ps_hw6_owner_sm_probe.current_state[PS_HW6_SM_BLE];
      }
      if ((status == HAL_OK) && (state == (uint32_t)BLE_IDLE))
      {
        status = PS_HW6_SM_Transition(PS_HW6_SM_BLE,
                                      BLE_EV_ADV_START_REQUEST,
                                      HAL_OK);
      }
      break;

    case PS_HW6_COMM_BLE_MODE_PAIRING:
      g_ps_hw6_owner_sm_probe.ble_mode_placeholder = 1UL;
      status = PS_HW6_SM_EnsureBleIdle();
      state = g_ps_hw6_owner_sm_probe.current_state[PS_HW6_SM_BLE];
      if ((status == HAL_OK) && (state == (uint32_t)BLE_CONNECTED))
      {
        status = PS_HW6_SM_Transition(PS_HW6_SM_BLE,
                                      BLE_EV_DISCONNECTED,
                                      HAL_OK);
        state = g_ps_hw6_owner_sm_probe.current_state[PS_HW6_SM_BLE];
      }
      if ((status == HAL_OK) &&
          ((state == (uint32_t)BLE_IDLE) ||
           (state == (uint32_t)BLE_ADVERTISING)))
      {
        status = PS_HW6_SM_Transition(PS_HW6_SM_BLE,
                                      BLE_EV_PAIRING_START,
                                      HAL_OK);
      }
      break;

    case PS_HW6_COMM_BLE_MODE_CONNECTED:
      g_ps_hw6_owner_sm_probe.ble_mode_placeholder = 1UL;
      status = PS_HW6_SM_EnsureBleIdle();
      state = g_ps_hw6_owner_sm_probe.current_state[PS_HW6_SM_BLE];
      if ((status == HAL_OK) && (state != (uint32_t)BLE_CONNECTED))
      {
        status = PS_HW6_SM_Transition(PS_HW6_SM_BLE,
                                      BLE_EV_CONNECTED,
                                      HAL_OK);
      }
      break;

    default:
      status = HAL_ERROR;
      break;
  }

  g_ps_hw6_owner_sm_probe.ble_mode_last_status = (uint32_t)status;
  if (status == HAL_OK)
  {
    g_ps_hw6_owner_sm_probe.ble_mode_active = mode;
  }
  return status;
}

static HAL_StatusTypeDef PS_HW6_SM_EnsureImuSuspended(void)
{
  HAL_StatusTypeDef status = HAL_OK;
  uint32_t state = g_ps_hw6_owner_sm_probe.current_state[PS_HW6_SM_IMU];

  if (state == (uint32_t)IMU_OFF)
  {
    status = PS_HW6_SM_StabilizeImu();
    state = g_ps_hw6_owner_sm_probe.current_state[PS_HW6_SM_IMU];
  }
  if ((status == HAL_OK) && (state == (uint32_t)IMU_LOW_RATE_SAMPLE))
  {
    status = PS_HW6_SM_QuiesceImu(PS_HW6_POWER_QUIESCE_CYCLE_INDEX);
    state = g_ps_hw6_owner_sm_probe.current_state[PS_HW6_SM_IMU];
  }
  if ((status == HAL_OK) && (state != (uint32_t)IMU_SUSPENDED))
  {
    status = HAL_ERROR;
  }
  return status;
}

static HAL_StatusTypeDef PS_HW6_SM_RunImuDeepPowerDownForStop2(
  uint32_t cycle_index)
{
  ps_dev_lis2dux12_stabilize_result_t result;
  ps_status_t driver_status;
  HAL_StatusTypeDef status;

  driver_status = ps_dev_lis2dux12_stabilize_suspended(
    &ps_imu_device,
    &result);
  status = PS_HW6_SM_StatusToHal(driver_status);
  PS_HW6_SM_RecordImuStabilizeResult(&result);
  if (cycle_index < PS_HW6_OWNER_SM_CYCLE_COUNT)
  {
    g_ps_hw6_owner_sm_probe.imu_cycle_sleep_status[cycle_index] =
      (uint32_t)status;
  }
  PS_HW6_SM_UpdateImuDriverProbe();
  if (status != HAL_OK)
  {
    (void)PS_HW6_SM_Transition(PS_HW6_SM_IMU,
                              IMU_EV_I2C_ERROR, status);
  }
  return status;
}

static HAL_StatusTypeDef PS_HW6_SM_ParkImuDeepPowerDownForStop2(
  uint32_t cycle_index)
{
  HAL_StatusTypeDef status = HAL_OK;
  uint32_t state = g_ps_hw6_owner_sm_probe.current_state[PS_HW6_SM_IMU];

  if (PS_HW6_SM_ImuDeepPowerDownProofValid() != 0UL)
  {
    return HAL_OK;
  }
  if (state == (uint32_t)IMU_OFF)
  {
    status = PS_HW6_SM_StabilizeImu();
  }
  else if (state == (uint32_t)IMU_LOW_RATE_SAMPLE)
  {
    status = PS_HW6_SM_QuiesceImu(cycle_index);
  }
  else if (state == (uint32_t)IMU_SUSPENDED)
  {
    status = PS_HW6_SM_RunImuDeepPowerDownForStop2(cycle_index);
  }
  else
  {
    status = HAL_ERROR;
  }
  return status;
}

HAL_StatusTypeDef PS_HW6_OwnerStateMachines_SetImuMode(uint32_t mode)
{
  HAL_StatusTypeDef status = HAL_OK;
  uint32_t state;

  g_ps_hw6_owner_sm_probe.imu_mode_request_count++;
  g_ps_hw6_owner_sm_probe.imu_mode_requested = mode;
  g_ps_hw6_owner_sm_probe.imu_mode_last_tick = (uint32_t)tx_time_get();
  g_ps_hw6_owner_sm_probe.imu_mode_placeholder = 0UL;
  g_ps_hw6_owner_sm_probe.imu_mode_power_floor = 0UL;
  g_ps_hw6_owner_sm_probe.imu_mode_wake_source_enabled = 0UL;
  g_ps_hw6_owner_sm_probe.imu_mode_last_status = (uint32_t)HAL_ERROR;

  switch (mode)
  {
    case PS_HW6_IMU_MODE_OFF:
      status = PS_HW6_SM_EnsureImuSuspended();
      break;

    case PS_HW6_IMU_MODE_LOW_RATE_SAMPLE:
      state = g_ps_hw6_owner_sm_probe.current_state[PS_HW6_SM_IMU];
      if (state == (uint32_t)IMU_LOW_RATE_SAMPLE)
      {
        status = HAL_OK;
      }
      else
      {
        status = PS_HW6_SM_EnsureImuSuspended();
        if (status == HAL_OK)
        {
          status = PS_HW6_SM_ResumeImu(PS_HW6_POWER_QUIESCE_CYCLE_INDEX);
        }
      }
      g_ps_hw6_owner_sm_probe.imu_mode_power_floor = 1UL;
      break;

    case PS_HW6_IMU_MODE_EVENT_ARMED:
      g_ps_hw6_owner_sm_probe.imu_mode_placeholder = 1UL;
      g_ps_hw6_owner_sm_probe.imu_mode_power_floor = 1UL;
      g_ps_hw6_owner_sm_probe.imu_mode_wake_source_enabled = 1UL;
      g_ps_hw6_owner_sm_probe.imu_mode_last_status =
        PS_HW6_OWNER_SM_STATUS_UNAVAILABLE;
      return HAL_ERROR;

    case PS_HW6_IMU_MODE_STEP_COUNTER:
      g_ps_hw6_owner_sm_probe.imu_mode_placeholder = 1UL;
      g_ps_hw6_owner_sm_probe.imu_mode_power_floor = 1UL;
      g_ps_hw6_owner_sm_probe.imu_mode_wake_source_enabled = 0UL;
      g_ps_hw6_owner_sm_probe.imu_mode_last_status =
        PS_HW6_OWNER_SM_STATUS_UNAVAILABLE;
      return HAL_ERROR;

    case PS_HW6_IMU_MODE_STREAMING:
      g_ps_hw6_owner_sm_probe.imu_mode_placeholder = 1UL;
      g_ps_hw6_owner_sm_probe.imu_mode_power_floor = 1UL;
      g_ps_hw6_owner_sm_probe.imu_mode_wake_source_enabled = 0UL;
      g_ps_hw6_owner_sm_probe.imu_mode_last_status =
        PS_HW6_OWNER_SM_STATUS_UNAVAILABLE;
      return HAL_ERROR;

    default:
      status = HAL_ERROR;
      break;
  }

  g_ps_hw6_owner_sm_probe.imu_mode_last_status = (uint32_t)status;
  if (status == HAL_OK)
  {
    g_ps_hw6_owner_sm_probe.imu_mode_active = mode;
  }
  return status;
}
void PS_HW6_OwnerStateMachines_Init(void)
{
  ps_status_t imu_init_status;
  ps_status_t joystick_init_status;
  ps_status_t flash_init_status;
  ps_status_t flash_block_init_status;
  uint32_t cycle_index;
  uint32_t direction;
  uint32_t index;

  (void)memset((void *)&g_ps_hw6_owner_sm_probe, 0,
               sizeof(g_ps_hw6_owner_sm_probe));
  g_ps_hw6_owner_sm_start_request = 0UL;
  g_ps_hw6_power_sleep_prep_request = 0UL;
  g_ps_hw6_power_stop2_request = 0UL;
  g_ps_hw6_power_stop2_active_resume_request = 0UL;
  g_ps_hw6_power_stop2_active_prep_request = 0UL;
  g_ps_hw6_power_stop2_active_enter_request = 0UL;
  g_ps_hw6_power_stop2_pre_wfi_hold_enable = 0UL;
  g_ps_hw6_power_stop2_srdrun_test_enable = 0UL;
  g_ps_hw6_power_stop2_apb3_div1_test_enable = 0UL;
  g_ps_hw6_power_stop2_post_wfi_break_enable = 0UL;
  g_ps_hw6_power_stop2_spi_autotrigger_test_enable = 0UL;
  g_ps_hw6_storage_usb_export_request = 0UL;
  g_ps_hw6_storage_usb_reclaim_request = 0UL;
  g_ps_hw6_joystick_sample_request = 0UL;
  g_ps_hw6_joystick_live_request = 0UL;
  g_ps_hw6_joystick_cardinal_request = 0UL;
  g_ps_hw6_joystick_calibration_capture_request = 0UL;
  g_ps_hw6_joystick_calibration_capture_page = PS_UI_ROUTER_CAL_NONE;
  g_ps_hw6_joystick_sleep_audit_request = 0UL;
  g_ps_hw6_joystick_xyz_capture_request = 0UL;
  g_ps_hw6_joystick_xyz_capture_mode = PS_HW6_JOYSTICK_XYZ_CAPTURE_NONE;
  g_ps_hw6_ble_sleep_dsr_deasserted = 1UL;
  ps_joystick_active_calibration = ps_joystick_hw6_default_calibration;
  PS_HW6_UsbExport_Reset();
  PS_HW6_ClockPolicy_Reset();
  g_ps_hw6_owner_sm_probe.magic = PS_HW6_OWNER_SM_PROBE_MAGIC;
  g_ps_hw6_owner_sm_probe.version = PS_HW6_OWNER_SM_PROBE_VERSION;
  g_ps_hw6_owner_sm_probe.phase = PS_HW6_SM_PHASE_INIT;
  g_ps_hw6_owner_sm_probe.required_owner_mask =
    PS_HW6_SM_REQUIRED_OWNER_MASK;
  g_ps_hw6_owner_sm_probe.cycle_requested_count =
    PS_HW6_OWNER_SM_CYCLE_COUNT;
  g_ps_hw6_owner_sm_probe.ble_sleep_dsr_deasserted =
    g_ps_hw6_ble_sleep_dsr_deasserted;
  g_ps_hw6_owner_sm_probe.ble_stop_settle_ticks =
    (uint32_t)PS_HW6_NINA_STOP_SETTLE_TICKS;
  g_ps_hw6_owner_sm_probe.ble_identity_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.ble_identity_rx_len = 0UL;
  g_ps_hw6_owner_sm_probe.ble_identity_response[0] = '\0';
  PS_HW6_SM_ResetStop2GpioAudit();

  imu_init_status = ps_dev_lis2dux12_init(
    &ps_imu_device,
    PS_HW6_IMU_ADDRESS);
  g_ps_hw6_owner_sm_probe.imu_driver_init_status =
    (uint32_t)imu_init_status;
  PS_HW6_SM_UpdateImuDriverProbe();

  joystick_init_status = ps_dev_tmag3001_init(
    &ps_joystick_device,
    PS_HW6_TMAG_ADDRESS);
  g_ps_hw6_owner_sm_probe.joystick_driver_init_status =
    (uint32_t)joystick_init_status;
  PS_HW6_SM_UpdateJoystickDriverProbe();
  PS_HW6_SM_ResetJoystickRuntimeProbes();

  flash_init_status = ps_dev_at25sl128a_init(
    &ps_flash_device,
    &hospi1,
    &handle_GPDMA1_Channel4,
    &handle_GPDMA1_Channel5,
    PS_HW6_SM_OSPI_TIMEOUT_MS);
  g_ps_hw6_owner_sm_probe.flash_driver_init_status =
    (uint32_t)flash_init_status;
  PS_HW6_SM_UpdateFlashDriverProbe();

  flash_block_init_status = ps_storage_flash_block_init(
    &ps_flash_block,
    &ps_flash_device);
  g_ps_hw6_owner_sm_probe.flash_block_init_status =
    (uint32_t)flash_block_init_status;
  PS_HW6_SM_UpdateFlashBlockProbe();

  for (index = 0U; index < PS_HW6_OWNER_SM_PHYSICAL_OWNER_COUNT; ++index)
  {
    g_ps_hw6_owner_sm_probe.owner_command_send_status[index] =
      PS_HW6_OWNER_SM_STATUS_NOT_RUN;
    g_ps_hw6_owner_sm_probe.owner_ack_wait_status[index] =
      PS_HW6_OWNER_SM_STATUS_NOT_RUN;
    g_ps_hw6_owner_sm_probe.owner_action_status[index] =
      PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  }

  for (cycle_index = 0U;
       cycle_index < PS_HW6_OWNER_SM_CYCLE_COUNT;
       ++cycle_index)
  {
    for (direction = 0U;
         direction < PS_HW6_OWNER_SM_CYCLE_DIRECTION_COUNT;
         ++direction)
    {
      for (index = 0U;
           index < PS_HW6_OWNER_SM_PHYSICAL_OWNER_COUNT;
           ++index)
      {
        g_ps_hw6_owner_sm_probe
          .cycle_command_send_status[cycle_index][direction][index] =
          PS_HW6_OWNER_SM_STATUS_NOT_RUN;
        g_ps_hw6_owner_sm_probe
          .cycle_ack_wait_status[cycle_index][direction][index] =
          PS_HW6_OWNER_SM_STATUS_NOT_RUN;
        g_ps_hw6_owner_sm_probe
          .cycle_action_status[cycle_index][direction][index] =
          PS_HW6_OWNER_SM_STATUS_NOT_RUN;
      }
    }

    g_ps_hw6_owner_sm_probe
      .joystick_cycle_wake_probe_status[cycle_index] =
      PS_HW6_OWNER_SM_STATUS_NOT_RUN;
    g_ps_hw6_owner_sm_probe
      .joystick_cycle_wake_retry_status[cycle_index] =
      PS_HW6_OWNER_SM_STATUS_NOT_RUN;
    g_ps_hw6_owner_sm_probe
      .joystick_cycle_active_status[cycle_index] =
      PS_HW6_OWNER_SM_STATUS_NOT_RUN;
    g_ps_hw6_owner_sm_probe
      .joystick_cycle_sleep_status[cycle_index] =
      PS_HW6_OWNER_SM_STATUS_NOT_RUN;
    g_ps_hw6_owner_sm_probe.imu_cycle_wake_probe_status[cycle_index] =
      PS_HW6_OWNER_SM_STATUS_NOT_RUN;
    g_ps_hw6_owner_sm_probe.imu_cycle_wake_probe_error[cycle_index] =
      PS_HW6_OWNER_SM_STATUS_NOT_RUN;
    g_ps_hw6_owner_sm_probe.imu_cycle_wake_probe_accepted[cycle_index] =
      PS_HW6_OWNER_SM_STATUS_NOT_RUN;
    g_ps_hw6_owner_sm_probe.imu_cycle_whoami_status[cycle_index] =
      PS_HW6_OWNER_SM_STATUS_NOT_RUN;
    g_ps_hw6_owner_sm_probe.imu_cycle_active_status[cycle_index] =
      PS_HW6_OWNER_SM_STATUS_NOT_RUN;
    g_ps_hw6_owner_sm_probe.imu_cycle_sleep_status[cycle_index] =
      PS_HW6_OWNER_SM_STATUS_NOT_RUN;
    g_ps_hw6_owner_sm_probe.flash_cycle_release_status[cycle_index] =
      PS_HW6_OWNER_SM_STATUS_NOT_RUN;
    g_ps_hw6_owner_sm_probe.flash_cycle_jedec_status[cycle_index] =
      PS_HW6_OWNER_SM_STATUS_NOT_RUN;
    g_ps_hw6_owner_sm_probe.flash_cycle_identity_match[cycle_index] =
      PS_HW6_OWNER_SM_STATUS_NOT_RUN;
    g_ps_hw6_owner_sm_probe
      .flash_cycle_deep_power_down_status[cycle_index] =
      PS_HW6_OWNER_SM_STATUS_NOT_RUN;
    g_ps_hw6_owner_sm_probe.ble_cycle_uart_init_status[cycle_index] =
      PS_HW6_OWNER_SM_STATUS_NOT_RUN;
    g_ps_hw6_owner_sm_probe.ble_cycle_wake_at_status[cycle_index] =
      PS_HW6_OWNER_SM_STATUS_NOT_RUN;
    g_ps_hw6_owner_sm_probe
      .ble_cycle_suspend_uart_status[cycle_index] =
      PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  }

  for (index = 0U; index < PS_HW6_OWNER_SM_COUNT; ++index)
  {
    g_ps_hw6_owner_sm_probe.current_state[index] = ps_initial_states[index];
    g_ps_hw6_owner_sm_probe.previous_state[index] = ps_initial_states[index];
    g_ps_hw6_owner_sm_probe.requested_state[index] = ps_initial_states[index];
    g_ps_hw6_owner_sm_probe.last_action_status[index] =
      PS_HW6_OWNER_SM_STATUS_NOT_RUN;
    g_ps_hw6_owner_sm_probe.last_error[index] =
      PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  }

  for (index = 0U; index < PS_HW6_OWNER_SM_IMU_REGISTER_COUNT; ++index)
  {
    g_ps_hw6_owner_sm_probe.imu_register_address[index] =
      ps_dev_lis2dux12_diagnostic_register(index);
  }
  for (index = 0U; index < PS_HW6_OWNER_SM_NINA_COMMAND_COUNT; ++index)
  {
    g_ps_hw6_owner_sm_probe.ble_command_tx_status[index] =
      PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  }
  g_ps_hw6_owner_sm_probe.joystick_sensor_config1_verify_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.joystick_device_config2_verify_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.joystick_sleep_write_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.imu_deep_power_down_write_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;

  g_ps_hw6_owner_sm_probe.start_power_last_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.start_power_quiesce_last_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.start_power_software_ship_enabled =
    (uint32_t)KNOB_POWER_START_SOFTWARE_SHIP_ENABLE;
  g_ps_hw6_owner_sm_probe.start_power_software_ship_last_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  ps_start_power_return_state = PWR_ACTIVE_LP;
  g_ps_hw6_owner_sm_probe.start_power_return_state =
    ps_start_power_return_state;

  ps_power_battery_owns_ship_prep = 0UL;
  ps_power_boot_restart_gate_pending = 1UL;
  ps_power_boot_restart_gate_blocked = 0UL;
  ps_power_battery_monitor_period_ticks =
    PS_HW6_SM_MsToTicks((uint32_t)KNOB_POWER_BATTERY_MONITOR_PERIOD_MS);
  g_ps_hw6_owner_sm_probe.battery_policy_state =
    PS_HW6_POWER_BATTERY_POLICY_UNKNOWN;
  g_ps_hw6_owner_sm_probe.battery_policy_last_event =
    PS_HW6_POWER_BATTERY_EVENT_NONE;
  g_ps_hw6_owner_sm_probe.battery_policy_monitor_count = 0UL;
  g_ps_hw6_owner_sm_probe.battery_policy_boot_check_count = 0UL;
  g_ps_hw6_owner_sm_probe.battery_policy_last_snapshot_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.battery_policy_last_tick = 0UL;
  g_ps_hw6_owner_sm_probe.battery_policy_next_tick = 0UL;
  g_ps_hw6_owner_sm_probe.battery_policy_period_ticks =
    ps_power_battery_monitor_period_ticks;
  g_ps_hw6_owner_sm_probe.battery_policy_warning_mv =
    (uint32_t)KNOB_POWER_BATTERY_WARNING_MV;
  g_ps_hw6_owner_sm_probe.battery_policy_critical_mv =
    (uint32_t)KNOB_POWER_BATTERY_CRITICAL_SHIP_MV;
  g_ps_hw6_owner_sm_probe.battery_policy_restart_allow_mv =
    (uint32_t)KNOB_POWER_BATTERY_RESTART_ALLOW_MV;
  g_ps_hw6_owner_sm_probe.battery_policy_vbat_mv = 0UL;
  g_ps_hw6_owner_sm_probe.battery_policy_fuel_ok = 0UL;
  g_ps_hw6_owner_sm_probe.battery_policy_vbus_ok = 0UL;
  g_ps_hw6_owner_sm_probe.battery_policy_battery_present = 0UL;
  g_ps_hw6_owner_sm_probe.battery_policy_warning_count = 0UL;
  g_ps_hw6_owner_sm_probe.battery_policy_critical_count = 0UL;
  g_ps_hw6_owner_sm_probe.battery_policy_boot_restart_block_count = 0UL;
  g_ps_hw6_owner_sm_probe.battery_policy_boot_charge_recovery_count = 0UL;
  g_ps_hw6_owner_sm_probe.battery_policy_boot_restart_gate_pending =
    ps_power_boot_restart_gate_pending;
  g_ps_hw6_owner_sm_probe.battery_policy_boot_restart_gate_blocked =
    ps_power_boot_restart_gate_blocked;
  g_ps_hw6_owner_sm_probe.battery_policy_boot_restart_gate_clear_count = 0UL;
  g_ps_hw6_owner_sm_probe.battery_policy_quiesce_request_count = 0UL;
  g_ps_hw6_owner_sm_probe.battery_policy_quiesce_last_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.battery_policy_quiesce_last_tick = 0UL;
  g_ps_hw6_owner_sm_probe.battery_policy_critical_ship_enabled =
    (uint32_t)KNOB_POWER_CRITICAL_SOFTWARE_SHIP_ENABLE;
  g_ps_hw6_owner_sm_probe.battery_policy_boot_ship_enabled =
    (uint32_t)KNOB_POWER_BOOT_LOW_BATTERY_SHIP_ENABLE;
  g_ps_hw6_owner_sm_probe.battery_policy_software_ship_request_count = 0UL;
  g_ps_hw6_owner_sm_probe.battery_policy_software_ship_skipped_count = 0UL;
  g_ps_hw6_owner_sm_probe.battery_policy_software_ship_last_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.battery_policy_software_ship_last_tick = 0UL;
  g_ps_hw6_owner_sm_probe.power_quiesce_request_count = 0UL;
  g_ps_hw6_owner_sm_probe.power_quiesce_reason =
    (uint32_t)PS_HW6_POWER_QUIESCE_REASON_NONE;
  g_ps_hw6_owner_sm_probe.power_quiesce_start_tick = 0UL;
  g_ps_hw6_owner_sm_probe.power_quiesce_end_tick = 0UL;
  g_ps_hw6_owner_sm_probe.power_quiesce_required_mask = 0UL;
  g_ps_hw6_owner_sm_probe.power_quiesce_send_ok_mask = 0UL;
  g_ps_hw6_owner_sm_probe.power_quiesce_ack_ok_mask = 0UL;
  g_ps_hw6_owner_sm_probe.power_quiesce_success_mask = 0UL;
  g_ps_hw6_owner_sm_probe.power_quiesce_failure_mask = 0UL;
  g_ps_hw6_owner_sm_probe.power_quiesce_last_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.post_stop_resume_request_count = 0UL;
  g_ps_hw6_owner_sm_probe.post_stop_resume_start_tick = 0UL;
  g_ps_hw6_owner_sm_probe.post_stop_resume_end_tick = 0UL;
  g_ps_hw6_owner_sm_probe.post_stop_resume_required_mask = 0UL;
  g_ps_hw6_owner_sm_probe.post_stop_resume_send_ok_mask = 0UL;
  g_ps_hw6_owner_sm_probe.post_stop_resume_ack_ok_mask = 0UL;
  g_ps_hw6_owner_sm_probe.post_stop_resume_success_mask = 0UL;
  g_ps_hw6_owner_sm_probe.post_stop_resume_failure_mask = 0UL;
  g_ps_hw6_owner_sm_probe.post_stop_resume_noop_mask = 0UL;
  g_ps_hw6_owner_sm_probe.post_stop_resume_action_mask = 0UL;
  g_ps_hw6_owner_sm_probe.post_stop_resume_last_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.sleep_prep_request_count = 0UL;
  g_ps_hw6_owner_sm_probe.sleep_prep_start_tick = 0UL;
  g_ps_hw6_owner_sm_probe.sleep_prep_end_tick = 0UL;
  g_ps_hw6_owner_sm_probe.sleep_prep_last_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.sleep_prep_quiesce_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.sleep_prep_stop_entry_skipped = 0UL;
  g_ps_hw6_owner_sm_probe.sleep_prep_recover_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.stop2_request_count = 0UL;
  g_ps_hw6_owner_sm_probe.stop2_start_tick = 0UL;
  g_ps_hw6_owner_sm_probe.stop2_wfi_tick = 0UL;
  g_ps_hw6_owner_sm_probe.stop2_wake_tick = 0UL;
  g_ps_hw6_owner_sm_probe.stop2_end_tick = 0UL;
  g_ps_hw6_owner_sm_probe.stop2_last_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.stop2_quiesce_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.stop2_enter_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.stop2_clock_restore_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.stop2_recover_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.stop2_expected_wake_pin =
    PS_HW6_STOP2_BUTTON_WAKE_EXTI_MASK;
  g_ps_hw6_owner_sm_probe.stop2_wake_start_idr = 0UL;
  g_ps_hw6_owner_sm_probe.stop2_wake_end_idr = 0UL;
  g_ps_hw6_owner_sm_probe.stop2_systick_ctrl_before =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.stop2_systick_ctrl_sleep =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.stop2_systick_ctrl_after =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.stop2_systick_icsr_before =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.stop2_systick_icsr_sleep =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.stop2_systick_icsr_after =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.stop2_active_prep_request_count = 0UL;
  g_ps_hw6_owner_sm_probe.stop2_active_prep_start_tick = 0UL;
  g_ps_hw6_owner_sm_probe.stop2_active_prep_end_tick = 0UL;
  g_ps_hw6_owner_sm_probe.stop2_active_prep_cycle_index = 0UL;
  g_ps_hw6_owner_sm_probe.stop2_active_prep_last_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.stop2_active_prep_ready = 0UL;
  g_ps_hw6_owner_sm_probe.stop2_active_enter_request_count = 0UL;
  g_ps_hw6_owner_sm_probe.stop2_active_enter_gate_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.stop2_policy_request_count = 0UL;
  g_ps_hw6_owner_sm_probe.stop2_policy_reason =
    (uint32_t)PS_HW6_POWER_QUIESCE_REASON_NONE;
  g_ps_hw6_owner_sm_probe.stop2_policy_last_tick = 0UL;
  g_ps_hw6_owner_sm_probe.stop2_policy_ble_target_mode =
    (uint32_t)PS_HW6_COMM_BLE_MODE_RESET_HELD;
  g_ps_hw6_owner_sm_probe.stop2_policy_ble_active_mode =
    (uint32_t)PS_HW6_COMM_BLE_MODE_RESET_HELD;
  g_ps_hw6_owner_sm_probe.stop2_policy_ble_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.stop2_policy_imu_target_mode =
    (uint32_t)PS_HW6_IMU_MODE_OFF;
  g_ps_hw6_owner_sm_probe.stop2_policy_imu_active_mode =
    (uint32_t)PS_HW6_IMU_MODE_OFF;
  g_ps_hw6_owner_sm_probe.stop2_policy_imu_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  for (index = 0U; index < PS_HW6_OWNER_SM_PHYSICAL_OWNER_COUNT; ++index)
  {
    g_ps_hw6_owner_sm_probe.power_quiesce_send_status[index] =
      PS_HW6_OWNER_SM_STATUS_NOT_RUN;
    g_ps_hw6_owner_sm_probe.power_quiesce_ack_status[index] =
      PS_HW6_OWNER_SM_STATUS_NOT_RUN;
    g_ps_hw6_owner_sm_probe.power_quiesce_ack_flags[index] = 0UL;
    g_ps_hw6_owner_sm_probe.power_quiesce_owner_status[index] =
      PS_HW6_OWNER_SM_STATUS_NOT_RUN;
    g_ps_hw6_owner_sm_probe.post_stop_resume_send_status[index] =
      PS_HW6_OWNER_SM_STATUS_NOT_RUN;
    g_ps_hw6_owner_sm_probe.post_stop_resume_ack_status[index] =
      PS_HW6_OWNER_SM_STATUS_NOT_RUN;
    g_ps_hw6_owner_sm_probe.post_stop_resume_ack_flags[index] = 0UL;
    g_ps_hw6_owner_sm_probe.post_stop_resume_owner_status[index] =
      PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  }
  g_ps_hw6_owner_sm_probe.joystick_ready_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.joystick_identity_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.imu_ready_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.imu_whoami_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.flash_jedec_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.flash_deep_power_down_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.flash_scratch_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.flash_block_test_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.flash_block_erase_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.flash_block_blank_read_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.flash_block_program_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.flash_block_verify_read_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.flash_block_cleanup_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.flash_block_cleanup_read_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.storage_fxlx_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.storage_fxlx_preformat_erase_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.storage_fxlx_preformat_erase_block_count = 0UL;
  g_ps_hw6_owner_sm_probe.storage_fxlx_preformat_erase_failed_block =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.storage_fxlx_preformat_erase_last_poll_count = 0UL;
  g_ps_hw6_owner_sm_probe.storage_fxlx_lx_initialize_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.storage_fxlx_lx_open_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.storage_fxlx_fx_format_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.storage_fxlx_fx_open_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.storage_fxlx_file_create_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.storage_fxlx_file_open_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.storage_fxlx_file_write_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.storage_fxlx_file_seek_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.storage_fxlx_file_read_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.storage_fxlx_file_close_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.storage_fxlx_fx_flush_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.storage_fxlx_fx_close_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.storage_fxlx_lx_close_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.storage_flash_init_request_count = 0UL;
  g_ps_hw6_owner_sm_probe.storage_flash_init_start_tick = 0UL;
  g_ps_hw6_owner_sm_probe.storage_flash_init_wake_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.storage_flash_init_layout_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.storage_flash_init_fxlx_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.storage_flash_init_deep_power_down_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.storage_flash_init_last_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.storage_attach_request_count = 0UL;
  g_ps_hw6_owner_sm_probe.storage_attach_start_tick = 0UL;
  g_ps_hw6_owner_sm_probe.storage_attach_last_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.storage_attach_storage_state =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.storage_attach_flash_state =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.flash_power_release_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.flash_power_jedec_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.flash_power_identity_match =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.flash_power_deep_power_down_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.storage_ospi_park_count = 0UL;
  g_ps_hw6_owner_sm_probe.storage_ospi_restore_count = 0UL;
  g_ps_hw6_owner_sm_probe.storage_ospi_park_ahb2enr1_before =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.storage_ospi_park_ahb2enr1_after =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.storage_ospi_park_ahb2enr2_before =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.storage_ospi_park_ahb2enr2_after =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.storage_ospi_park_ahb2smenr1_before =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.storage_ospi_park_ahb2smenr1_after =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.storage_ospi_park_ahb2smenr2_before =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.storage_ospi_park_ahb2smenr2_after =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.storage_ospi_restore_ahb2enr1_before =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.storage_ospi_restore_ahb2enr1_after =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.storage_ospi_restore_ahb2enr2_before =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.storage_ospi_restore_ahb2enr2_after =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.storage_ospi_restore_ahb2smenr1_before =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.storage_ospi_restore_ahb2smenr1_after =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.storage_ospi_restore_ahb2smenr2_before =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.storage_ospi_restore_ahb2smenr2_after =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.flash_scratch_erase_write_enable_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.flash_scratch_erase_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.flash_scratch_erase_wait_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.flash_scratch_erase_blank_read_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.flash_scratch_program_write_enable_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.flash_scratch_program_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.flash_scratch_program_wait_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.flash_scratch_program_read_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.flash_scratch_dma_program_write_enable_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.flash_scratch_dma_program_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.flash_scratch_dma_program_transfer_wait_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.flash_scratch_dma_program_flash_wait_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.flash_scratch_dma_read_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.flash_scratch_dma_read_transfer_wait_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.flash_scratch_cleanup_write_enable_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.flash_scratch_cleanup_erase_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.flash_scratch_cleanup_wait_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.flash_scratch_cleanup_blank_read_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.usb_deinit_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.usb_export_vbus_present =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.usb_export_power_pmic_vbus_at_request =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.usb_export_power_mcu_vbus_at_request =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.usb_export_power_vbus_agree_at_request =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.usb_export_policy_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.usb_export_flash_wake_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.usb_export_fxlx_open_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.usb_export_dcd_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.usb_export_pcd_init_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.usb_export_pcd_start_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.usb_export_irq_priority_before =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.usb_export_irq_priority_after =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.usb_export_devconnect_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.usb_reclaim_devdisconnect_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.usb_reclaim_disconnect_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.usb_reclaim_pcd_stop_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.usb_reclaim_deinit_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.usb_reclaim_fxlx_close_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.usb_stage_rescan_forced = 0UL;
  g_ps_hw6_owner_sm_probe.usb_stage_rescan_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.usb_stage_rescan_package_scan_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.usb_stage_rescan_classification =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.usb_stage_rescan_entry_count = 0UL;
  g_ps_hw6_owner_sm_probe.usb_stage_rescan_file_count = 0UL;
  g_ps_hw6_owner_sm_probe.usb_stage_rescan_directory_count = 0UL;
  g_ps_hw6_owner_sm_probe.usb_stage_rescan_package_candidate_count = 0UL;
  g_ps_hw6_owner_sm_probe.usb_stage_rescan_unsupported_count = 0UL;
  g_ps_hw6_owner_sm_probe.usb_stage_rescan_bounded = 0UL;
  g_ps_hw6_owner_sm_probe.usb_stage_rescan_first_entry_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.usb_stage_rescan_last_entry_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.usb_stage_rescan_lx_open_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.usb_stage_rescan_fx_open_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.usb_stage_rescan_fx_close_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.usb_stage_rescan_lx_close_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.package_candidate_pending = 0UL;
  g_ps_hw6_owner_sm_probe.package_validate_request_count = 0UL;
  g_ps_hw6_owner_sm_probe.package_validate_start_tick = 0UL;
  g_ps_hw6_owner_sm_probe.package_validate_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.package_validate_reason =
    (uint32_t)PS_STORAGE_PACKAGE_VALIDATE_NOT_RUN;
  g_ps_hw6_owner_sm_probe.package_validate_candidate_count = 0UL;
  g_ps_hw6_owner_sm_probe.package_validate_unsupported_count = 0UL;
  g_ps_hw6_owner_sm_probe.package_validate_file_size = 0UL;
  g_ps_hw6_owner_sm_probe.package_validate_header_bytes = 0UL;
  g_ps_hw6_owner_sm_probe.package_validate_bytes_read = 0UL;
  g_ps_hw6_owner_sm_probe.package_validate_magic = 0UL;
  g_ps_hw6_owner_sm_probe.package_validate_magic_valid = 0UL;
  g_ps_hw6_owner_sm_probe.package_validate_minimum_envelope_valid = 0UL;
  g_ps_hw6_owner_sm_probe.package_validate_header_layout_supported = 0UL;
  g_ps_hw6_owner_sm_probe.package_validate_lx_open_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.package_validate_fx_open_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.package_validate_file_open_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.package_validate_file_read_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.package_validate_file_close_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.package_validate_fx_close_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.package_validate_lx_close_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  for (index = 0UL; index < 16UL; ++index)
  {
    g_ps_hw6_owner_sm_probe.package_validate_header_first16[index] = 0UL;
  }
  g_ps_hw6_owner_sm_probe.package_install_stub_request_count = 0UL;
  g_ps_hw6_owner_sm_probe.package_install_stub_start_tick = 0UL;
  g_ps_hw6_owner_sm_probe.package_install_stub_candidate_classification =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.package_install_stub_candidate_count = 0UL;
  g_ps_hw6_owner_sm_probe.package_install_stub_unsupported_count = 0UL;
  g_ps_hw6_owner_sm_probe.package_install_stub_last_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.ble_uart_deinit_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
}

void PS_HW6_OwnerStateMachines_SetPowerQuiesceCallback(
  PS_HW6_PowerQuiesceBarrierCallback callback)
{
  ps_power_quiesce_barrier_callback = callback;
}

void PS_HW6_OwnerStateMachines_SetPowerAdmissionCallback(
  PS_HW6_PowerAdmissionCallback callback)
{
  ps_power_admission_callback = callback;
}

void PS_HW6_OwnerStateMachines_SetPostStopResumeCallback(
  PS_HW6_PostStopResumeBarrierCallback callback)
{
  ps_post_stop_resume_barrier_callback = callback;
}

void PS_HW6_OwnerStateMachines_BeginWorkflow(void)
{
  g_ps_hw6_owner_sm_probe.phase = PS_HW6_SM_PHASE_RUNNING;
  g_ps_hw6_owner_sm_probe.workflow_start_tick = (uint32_t)tx_time_get();
}

HAL_StatusTypeDef PS_HW6_OwnerStateMachines_RunSleepPrepScaffold(void)
{
  HAL_StatusTypeDef status;
  HAL_StatusTypeDef quiesce_status = HAL_ERROR;
  HAL_StatusTypeDef recover_status = HAL_ERROR;

  g_ps_hw6_owner_sm_probe.sleep_prep_request_count++;
  g_ps_hw6_owner_sm_probe.sleep_prep_start_tick = (uint32_t)tx_time_get();
  g_ps_hw6_owner_sm_probe.sleep_prep_end_tick = 0UL;
  g_ps_hw6_owner_sm_probe.sleep_prep_last_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.sleep_prep_quiesce_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.sleep_prep_recover_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.sleep_prep_stop_entry_skipped = 1UL;

  status = PS_HW6_SM_Transition(PS_HW6_SM_POWER,
                                PWR_EV_SLEEP_REQUEST,
                                HAL_OK);
  if (status == HAL_OK)
  {
    quiesce_status = PS_HW6_RequestPowerQuiesce(
      (uint32_t)PS_HW6_POWER_QUIESCE_REASON_SLEEP_PREP);
    g_ps_hw6_owner_sm_probe.sleep_prep_quiesce_status =
      (uint32_t)quiesce_status;

    recover_status = PS_HW6_SM_Transition(PS_HW6_SM_POWER,
                                          PWR_EV_LP_REQUEST,
                                          HAL_OK);
    g_ps_hw6_owner_sm_probe.sleep_prep_recover_status =
      (uint32_t)recover_status;
    if (quiesce_status != HAL_OK)
    {
      status = quiesce_status;
    }
    else
    {
      status = recover_status;
    }
  }

  g_ps_hw6_owner_sm_probe.sleep_prep_end_tick = (uint32_t)tx_time_get();
  g_ps_hw6_owner_sm_probe.sleep_prep_last_status = (uint32_t)status;
  return status;
}

HAL_StatusTypeDef PS_HW6_OwnerStateMachines_RunStop2StartWakeScaffold(void)
{
  HAL_StatusTypeDef status;
  HAL_StatusTypeDef quiesce_status = HAL_ERROR;
  HAL_StatusTypeDef enter_transition_status = HAL_ERROR;
  HAL_StatusTypeDef wake_transition_status = HAL_ERROR;
  HAL_StatusTypeDef post_stop_resume_status = HAL_ERROR;
  HAL_StatusTypeDef recover_status = HAL_ERROR;
  HAL_StatusTypeDef gpio_park_status = HAL_ERROR;
  HAL_StatusTypeDef gpio_restore_status = HAL_ERROR;
  UINT clock_restore_status = TX_NOT_DONE;
  uint32_t systick_ctrl_before = 0UL;
  uint32_t stop2_entered = 0UL;
  uint32_t srdrun_cr2_before = 0UL;
  uint32_t srdrun_test_active = 0UL;
  uint32_t apb3_cfgr3_before = 0UL;
  uint32_t apb3_div1_test_active = 0UL;
  uint32_t spi_autocr_before = 0UL;
  uint32_t spi_autotrigger_test_active = 0UL;
  uint32_t final_input_primask = 0UL;
  uint32_t final_input_critical_active = 0UL;
  uint32_t final_input_veto = 0UL;

  g_ps_hw6_owner_sm_probe.stop2_request_count++;
  g_ps_hw6_owner_sm_probe.stop2_start_tick = (uint32_t)tx_time_get();
  g_ps_hw6_owner_sm_probe.stop2_wfi_tick = 0UL;
  g_ps_hw6_owner_sm_probe.stop2_wake_tick = 0UL;
  g_ps_hw6_owner_sm_probe.stop2_end_tick = 0UL;
  g_ps_hw6_owner_sm_probe.stop2_last_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.stop2_quiesce_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.stop2_enter_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.stop2_clock_restore_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.stop2_recover_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.stop2_expected_wake_pin =
    PS_HW6_STOP2_BUTTON_WAKE_EXTI_MASK;
  g_ps_hw6_owner_sm_probe.stop2_wake_start_idr = 0UL;
  g_ps_hw6_owner_sm_probe.stop2_wake_end_idr = 0UL;
  g_ps_hw6_owner_sm_probe.stop2_systick_ctrl_before =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.stop2_systick_ctrl_sleep =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.stop2_systick_ctrl_after =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.stop2_systick_icsr_before =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.stop2_systick_icsr_sleep =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.stop2_systick_icsr_after =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.stop2_pre_wfi_hold_enabled =
    (g_ps_hw6_power_stop2_pre_wfi_hold_enable != 0UL) ? 1UL : 0UL;
  g_ps_hw6_owner_sm_probe.stop2_pre_wfi_hold_active = 0UL;
  PS_HW6_SM_ResetStop2GpioAudit();
  PS_HW6_TraceSleep(PS_HW6_TRACE_SLEEP_STAGE_PREP_START,
                    (uint32_t)PS_HW6_POWER_QUIESCE_REASON_SLEEP_PREP,
                    g_ps_hw6_owner_sm_probe.current_state[PS_HW6_SM_POWER],
                    (uint32_t)HAL_OK);
  status = PS_HW6_SM_Transition(PS_HW6_SM_POWER,
                                PWR_EV_SLEEP_REQUEST,
                                HAL_OK);
  if (status == HAL_OK)
  {
    if ((g_ps_hw6_power_stop2_apb3_div1_test_enable != 0UL) &&
        (g_ps_hw6_power_stop2_pre_wfi_hold_enable == 0UL))
    {
      g_ps_hw6_power_stop2_apb3_div1_test_enable = 0UL;
      apb3_div1_test_active = 1UL;
      apb3_cfgr3_before = RCC->CFGR3;
      g_ps_hw6_owner_sm_probe.stop2_apb3_div1_test_active = 1UL;
      g_ps_hw6_owner_sm_probe.stop2_apb3_div1_test_count++;
      g_ps_hw6_owner_sm_probe.stop2_apb3_div1_test_tick =
        (uint32_t)tx_time_get();
      g_ps_hw6_owner_sm_probe.stop2_apb3_cfgr3_before =
        apb3_cfgr3_before;
      MODIFY_REG(RCC->CFGR3, RCC_CFGR3_PPRE3, RCC_HCLK_DIV1);
      __DSB();
      __ISB();
      g_ps_hw6_owner_sm_probe.stop2_apb3_cfgr3_forced = RCC->CFGR3;
    }
    quiesce_status = PS_HW6_RequestPowerQuiesce(
      (uint32_t)PS_HW6_POWER_QUIESCE_REASON_SLEEP_PREP);
    g_ps_hw6_owner_sm_probe.stop2_quiesce_status =
      (uint32_t)quiesce_status;
    if (quiesce_status == HAL_OK)
    {
      PS_HW6_RTOS_Stop2WakeClassifyBegin();
      g_ps_hw6_owner_sm_probe.stop2_wake_start_idr =
        BTN_START_GPIO_Port->IDR;
      PS_HW6_SM_ClearStop2ButtonWakePending();

      enter_transition_status = PS_HW6_SM_Transition(PS_HW6_SM_POWER,
                                                     PWR_EV_STOP_ENTERED,
                                                     HAL_OK);
      g_ps_hw6_owner_sm_probe.stop2_enter_status =
        (uint32_t)enter_transition_status;
      if (enter_transition_status == HAL_OK)
      {
        PS_HW6_TraceSleep(PS_HW6_TRACE_SLEEP_STAGE_ENTER_STOP2,
                          (uint32_t)PS_HW6_POWER_QUIESCE_REASON_SLEEP_PREP,
                          g_ps_hw6_owner_sm_probe.current_state[PS_HW6_SM_POWER],
                          (uint32_t)enter_transition_status);
        PS_HW6_SM_RecordStop2GpioSnapshot(PS_HW6_STOP2_GPIO_SNAPSHOT_BEFORE);
        gpio_park_status = PS_HW6_SM_ParkStop2GpioPins();
        PS_HW6_SM_RecordStop2GpioSnapshot(PS_HW6_STOP2_GPIO_SNAPSHOT_SLEEP);
        if (gpio_park_status == HAL_OK)
        {
          HAL_SuspendTick();
          systick_ctrl_before = PS_HW6_SM_SuspendThreadXSystick();
          if (g_ps_hw6_power_stop2_pre_wfi_hold_enable != 0UL)
          {
            PS_HW6_SM_RecordStop2PreWfiState();
            g_ps_hw6_power_stop2_pre_wfi_hold_enable = 0UL;
            g_ps_hw6_owner_sm_probe.stop2_pre_wfi_hold_active = 1UL;
            __DSB();
            __ISB();
            __BKPT(0);
            g_ps_hw6_owner_sm_probe.stop2_pre_wfi_hold_active = 0UL;
            g_ps_hw6_owner_sm_probe.stop2_pre_wfi_hold_skip_count++;
            clock_restore_status = TX_SUCCESS;
          }
          else
          {
            if (g_ps_hw6_power_stop2_srdrun_test_enable != 0UL)
            {
              g_ps_hw6_power_stop2_srdrun_test_enable = 0UL;
              srdrun_test_active = 1UL;
              srdrun_cr2_before = PWR->CR2;
              g_ps_hw6_owner_sm_probe.stop2_srdrun_test_active = 1UL;
              g_ps_hw6_owner_sm_probe.stop2_srdrun_test_count++;
              g_ps_hw6_owner_sm_probe.stop2_srdrun_test_tick =
                (uint32_t)tx_time_get();
              g_ps_hw6_owner_sm_probe.stop2_srdrun_cr2_before =
                srdrun_cr2_before;
              SET_BIT(PWR->CR2, PWR_CR2_SRDRUN);
              __DSB();
              __ISB();
              g_ps_hw6_owner_sm_probe.stop2_srdrun_cr2_forced = PWR->CR2;
            }
            if (g_ps_hw6_power_stop2_spi_autotrigger_test_enable != 0UL)
            {
              g_ps_hw6_power_stop2_spi_autotrigger_test_enable = 0UL;
              spi_autotrigger_test_active = 1UL;
              spi_autocr_before = SPI3->AUTOCR;
              g_ps_hw6_owner_sm_probe.stop2_spi_autotrigger_test_active = 1UL;
              g_ps_hw6_owner_sm_probe.stop2_spi_autotrigger_test_count++;
              g_ps_hw6_owner_sm_probe.stop2_spi_autotrigger_test_tick =
                (uint32_t)tx_time_get();
              g_ps_hw6_owner_sm_probe.stop2_spi_autocr_before =
                spi_autocr_before;
              CLEAR_BIT(SPI3->AUTOCR, SPI_AUTOCR_TRIGEN);
              __DSB();
              __ISB();
              g_ps_hw6_owner_sm_probe.stop2_spi_autocr_forced = SPI3->AUTOCR;
            }
            final_input_primask = __get_PRIMASK();
            __disable_irq();
            final_input_critical_active = 1UL;
            if (PS_HW6_RTOS_Stop2FinalInputReady() != 0UL)
            {
              __DSB();
              g_ps_hw6_owner_sm_probe.stop2_wfi_tick =
                (uint32_t)tx_time_get();
              HAL_PWREx_EnterSTOP2Mode(PWR_STOPENTRY_WFI);
              __ISB();
              stop2_entered = 1UL;
              PS_HW6_SM_RecordStop2PostWfiState();
              if (g_ps_hw6_power_stop2_post_wfi_break_enable != 0UL)
              {
                g_ps_hw6_power_stop2_post_wfi_break_enable = 0UL;
                g_ps_hw6_owner_sm_probe.stop2_post_wfi_break_count++;
                __DSB();
                __ISB();
                __BKPT(0);
              }
            }
            else
            {
              final_input_veto = 1UL;
            }
            if (spi_autotrigger_test_active != 0UL)
            {
              MODIFY_REG(SPI3->AUTOCR,
                         SPI_AUTOCR_TRIGEN,
                         spi_autocr_before & SPI_AUTOCR_TRIGEN);
              __DSB();
              __ISB();
              g_ps_hw6_owner_sm_probe.stop2_spi_autocr_after = SPI3->AUTOCR;
              g_ps_hw6_owner_sm_probe.stop2_spi_autotrigger_test_active = 0UL;
            }
            if (apb3_div1_test_active != 0UL)
            {
              MODIFY_REG(RCC->CFGR3,
                         RCC_CFGR3_PPRE3,
                         apb3_cfgr3_before & RCC_CFGR3_PPRE3);
              __DSB();
              __ISB();
              g_ps_hw6_owner_sm_probe.stop2_apb3_cfgr3_after = RCC->CFGR3;
              g_ps_hw6_owner_sm_probe.stop2_apb3_div1_test_active = 0UL;
            }
            if (srdrun_test_active != 0UL)
            {
              MODIFY_REG(PWR->CR2,
                         PWR_CR2_SRDRUN,
                         srdrun_cr2_before & PWR_CR2_SRDRUN);
              __DSB();
              __ISB();
              g_ps_hw6_owner_sm_probe.stop2_srdrun_cr2_after = PWR->CR2;
              g_ps_hw6_owner_sm_probe.stop2_srdrun_test_active = 0UL;
            }
            if (stop2_entered != 0UL)
            {
              clock_restore_status = PS_HW6_ClockPolicy_RestoreBase();
            }
            else
            {
              clock_restore_status = TX_SUCCESS;
            }
          }
          gpio_restore_status = PS_HW6_SM_RestoreStop2GpioPins();
          PS_HW6_SM_RestoreThreadXSystick(systick_ctrl_before);
          HAL_ResumeTick();
          if (final_input_critical_active != 0UL)
          {
            if (final_input_primask == 0UL)
            {
              __enable_irq();
            }
            final_input_critical_active = 0UL;
          }

          if (stop2_entered != 0UL)
          {
            g_ps_hw6_owner_sm_probe.stop2_wake_tick =
              (uint32_t)tx_time_get();
            g_ps_hw6_owner_sm_probe.stop2_wake_end_idr =
              BTN_START_GPIO_Port->IDR;
            PS_HW6_RTOS_Stop2WakeClassifyAfterWake();
          }
          PS_HW6_SM_RecordStop2GpioSnapshot(PS_HW6_STOP2_GPIO_SNAPSHOT_AFTER);
        }
        else
        {
          status = gpio_park_status;
        }
        g_ps_hw6_owner_sm_probe.stop2_clock_restore_status =
          (clock_restore_status == TX_SUCCESS) ?
          (uint32_t)gpio_restore_status : (uint32_t)clock_restore_status;
        PS_HW6_TraceSleep(PS_HW6_TRACE_SLEEP_STAGE_WAKE_STOP2,
                          (uint32_t)BTN_START_Pin,
                          g_ps_hw6_owner_sm_probe.current_state[PS_HW6_SM_POWER],
                          (uint32_t)clock_restore_status);

        wake_transition_status = PS_HW6_SM_Transition(PS_HW6_SM_POWER,
                                                      PWR_EV_WAKE,
                                                      HAL_OK);
        if (wake_transition_status == HAL_OK)
        {
          post_stop_resume_status = PS_HW6_RequestPostStopResume();
          recover_status = PS_HW6_SM_Transition(PS_HW6_SM_POWER,
                                                PWR_EV_LP_REQUEST,
                                                HAL_OK);
          if (post_stop_resume_status != HAL_OK)
          {
            status = post_stop_resume_status;
          }
          else
          {
            status = recover_status;
          }
        }
        else
        {
          status = wake_transition_status;
        }
        if (final_input_veto != 0UL)
        {
          status = HAL_ERROR;
        }
        g_ps_hw6_owner_sm_probe.stop2_recover_status =
          (uint32_t)recover_status;
      }
      else
      {
        recover_status = PS_HW6_SM_Transition(PS_HW6_SM_POWER,
                                              PWR_EV_LP_REQUEST,
                                              HAL_OK);
        g_ps_hw6_owner_sm_probe.stop2_recover_status =
          (uint32_t)recover_status;
        status = enter_transition_status;
      }
    }
    else
    {
      recover_status = PS_HW6_SM_Transition(PS_HW6_SM_POWER,
                                            PWR_EV_LP_REQUEST,
                                            HAL_OK);
      g_ps_hw6_owner_sm_probe.stop2_recover_status =
        (uint32_t)recover_status;
      status = quiesce_status;
    }
  }

  if (apb3_div1_test_active != 0UL)
  {
    MODIFY_REG(RCC->CFGR3,
               RCC_CFGR3_PPRE3,
               apb3_cfgr3_before & RCC_CFGR3_PPRE3);
    __DSB();
    __ISB();
    g_ps_hw6_owner_sm_probe.stop2_apb3_cfgr3_after = RCC->CFGR3;
    g_ps_hw6_owner_sm_probe.stop2_apb3_div1_test_active = 0UL;
  }

  g_ps_hw6_owner_sm_probe.stop2_end_tick = (uint32_t)tx_time_get();
  g_ps_hw6_owner_sm_probe.stop2_last_status = (uint32_t)status;
  PS_HW6_TraceSleep(PS_HW6_TRACE_SLEEP_STAGE_RECOVER,
                    (uint32_t)PS_HW6_POWER_QUIESCE_REASON_SLEEP_PREP,
                    g_ps_hw6_owner_sm_probe.current_state[PS_HW6_SM_POWER],
                    (uint32_t)status);
  return status;
}

void PS_HW6_OwnerStateMachines_BeginStop2ActivePrep(uint32_t cycle_index)
{
  uint32_t direction;
  uint32_t owner_id;

  g_ps_hw6_owner_sm_probe.stop2_active_prep_request_count++;
  g_ps_hw6_owner_sm_probe.stop2_active_prep_start_tick =
    (uint32_t)tx_time_get();
  g_ps_hw6_owner_sm_probe.stop2_active_prep_end_tick = 0UL;
  g_ps_hw6_owner_sm_probe.stop2_active_prep_cycle_index = cycle_index;
  g_ps_hw6_owner_sm_probe.stop2_active_prep_last_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.stop2_active_prep_ready = 0UL;
  g_ps_hw6_owner_sm_probe.stop2_active_enter_request_count = 0UL;
  g_ps_hw6_owner_sm_probe.stop2_active_enter_gate_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;

  if (cycle_index >= PS_HW6_OWNER_SM_CYCLE_COUNT)
  {
    g_ps_hw6_owner_sm_probe.stop2_active_prep_last_status =
      (uint32_t)HAL_ERROR;
    return;
  }

  g_ps_hw6_owner_sm_probe.cycle_start_tick[cycle_index] = 0UL;
  g_ps_hw6_owner_sm_probe.cycle_active_tick[cycle_index] = 0UL;
  g_ps_hw6_owner_sm_probe.cycle_end_tick[cycle_index] = 0UL;
  g_ps_hw6_owner_sm_probe.cycle_resume_success_mask[cycle_index] = 0UL;
  g_ps_hw6_owner_sm_probe.cycle_resume_failure_mask[cycle_index] = 0UL;
  g_ps_hw6_owner_sm_probe.cycle_quiesce_success_mask[cycle_index] = 0UL;
  g_ps_hw6_owner_sm_probe.cycle_quiesce_failure_mask[cycle_index] = 0UL;
  g_ps_hw6_owner_sm_probe.cycle_active_state_match_mask[cycle_index] = 0UL;
  g_ps_hw6_owner_sm_probe.cycle_inactive_state_match_mask[cycle_index] = 0UL;

  for (direction = 0U;
       direction < PS_HW6_OWNER_SM_CYCLE_DIRECTION_COUNT;
       ++direction)
  {
    for (owner_id = 0U;
         owner_id < PS_HW6_OWNER_SM_PHYSICAL_OWNER_COUNT;
         ++owner_id)
    {
      g_ps_hw6_owner_sm_probe
        .cycle_command_send_status[cycle_index][direction][owner_id] =
        PS_HW6_OWNER_SM_STATUS_NOT_RUN;
      g_ps_hw6_owner_sm_probe
        .cycle_ack_wait_status[cycle_index][direction][owner_id] =
        PS_HW6_OWNER_SM_STATUS_NOT_RUN;
      g_ps_hw6_owner_sm_probe
        .cycle_ack_flags[cycle_index][direction][owner_id] = 0UL;
      g_ps_hw6_owner_sm_probe
        .cycle_action_start_tick[cycle_index][direction][owner_id] = 0UL;
      g_ps_hw6_owner_sm_probe
        .cycle_action_end_tick[cycle_index][direction][owner_id] = 0UL;
      g_ps_hw6_owner_sm_probe
        .cycle_action_status[cycle_index][direction][owner_id] =
        PS_HW6_OWNER_SM_STATUS_NOT_RUN;
    }
  }
}

HAL_StatusTypeDef PS_HW6_OwnerStateMachines_EndStop2ActivePrep(
  uint32_t cycle_index)
{
  HAL_StatusTypeDef status;

  g_ps_hw6_owner_sm_probe.stop2_active_prep_end_tick =
    (uint32_t)tx_time_get();
  if (cycle_index >= PS_HW6_OWNER_SM_CYCLE_COUNT)
  {
    status = HAL_ERROR;
  }
  else
  {
    status = ((g_ps_hw6_owner_sm_probe.cycle_resume_success_mask
                 [cycle_index] == PS_HW6_STOP2_ACTIVE_PREP_OWNER_MASK) &&
              (g_ps_hw6_owner_sm_probe.cycle_resume_failure_mask
                 [cycle_index] == 0UL) &&
              (g_ps_hw6_owner_sm_probe.cycle_quiesce_success_mask
                 [cycle_index] == PS_HW6_STOP2_ACTIVE_PREP_OWNER_MASK) &&
              (g_ps_hw6_owner_sm_probe.cycle_quiesce_failure_mask
                 [cycle_index] == 0UL) &&
              (g_ps_hw6_owner_sm_probe.cycle_active_state_match_mask
                 [cycle_index] == PS_HW6_STOP2_ACTIVE_PREP_STATE_MASK) &&
              (g_ps_hw6_owner_sm_probe.cycle_inactive_state_match_mask
                 [cycle_index] == PS_HW6_STOP2_ACTIVE_PREP_STATE_MASK)) ?
      HAL_OK : HAL_ERROR;
  }
  g_ps_hw6_owner_sm_probe.stop2_active_prep_last_status =
    (uint32_t)status;
  g_ps_hw6_owner_sm_probe.stop2_active_prep_ready = (status == HAL_OK) ?
    1UL : 0UL;
  return status;
}

HAL_StatusTypeDef
PS_HW6_OwnerStateMachines_RunStop2AfterActivePrepScaffold(void)
{
  HAL_StatusTypeDef status;

  g_ps_hw6_owner_sm_probe.stop2_active_enter_request_count++;
  if ((g_ps_hw6_owner_sm_probe.stop2_active_prep_ready == 0UL) ||
      (g_ps_hw6_owner_sm_probe.stop2_active_prep_last_status !=
       (uint32_t)HAL_OK))
  {
    g_ps_hw6_owner_sm_probe.stop2_active_enter_gate_status =
      (uint32_t)HAL_ERROR;
    return HAL_ERROR;
  }

  g_ps_hw6_owner_sm_probe.stop2_active_enter_gate_status =
    (uint32_t)HAL_OK;
  status = PS_HW6_OwnerStateMachines_RunStop2StartWakeScaffold();
  if (status != HAL_OK)
  {
    g_ps_hw6_owner_sm_probe.stop2_active_prep_ready = 0UL;
  }
  return status;
}

HAL_StatusTypeDef PS_HW6_OwnerStateMachines_HandleStartShippingIntent(
  uint32_t start_event,
  uint32_t hold_ticks)
{
  HAL_StatusTypeDef status = HAL_ERROR;
  uint32_t power_event = 0UL;

  g_ps_hw6_owner_sm_probe.start_power_event_count++;
  g_ps_hw6_owner_sm_probe.start_power_last_event = start_event;
  g_ps_hw6_owner_sm_probe.start_power_last_hold_ticks = hold_ticks;
  g_ps_hw6_owner_sm_probe.start_power_last_tick = (uint32_t)tx_time_get();

  if (start_event ==
      (uint32_t)PS_INPUT_START_POWER_EVENT_SHIP_PREP)
  {
    power_event = PWR_EV_START_SHIP_PREP;
    ps_start_power_return_state =
      g_ps_hw6_owner_sm_probe.current_state[PS_HW6_SM_POWER];
    g_ps_hw6_owner_sm_probe.start_power_return_state =
      ps_start_power_return_state;
    g_ps_hw6_owner_sm_probe.start_power_ship_prep_count++;
  }
  else if (start_event ==
           (uint32_t)PS_INPUT_START_POWER_EVENT_SHIP_WARNING)
  {
    power_event = PWR_EV_START_SHIP_WARNING;
    g_ps_hw6_owner_sm_probe.start_power_ship_warning_count++;
  }
  else if (start_event ==
           (uint32_t)PS_INPUT_START_POWER_EVENT_SHIP_IMMINENT)
  {
    power_event = PWR_EV_START_SHIP_IMMINENT;
    g_ps_hw6_owner_sm_probe.start_power_ship_imminent_count++;
  }
  else if (start_event ==
           (uint32_t)PS_INPUT_START_POWER_EVENT_RELEASED_BEFORE_SHIP)
  {
    power_event = (ps_start_power_return_state == PWR_ACTIVE_RT) ?
      PWR_EV_RT_REQUEST : PWR_EV_LP_REQUEST;
    g_ps_hw6_owner_sm_probe.start_power_cancel_count++;
  }

  if (power_event != 0UL)
  {
    status = PS_HW6_SM_Transition(PS_HW6_SM_POWER, power_event, HAL_OK);
    if ((status == HAL_OK) &&
        (start_event ==
         (uint32_t)PS_INPUT_START_POWER_EVENT_SHIP_PREP))
    {
      status = PS_HW6_StartPowerPrepareForShipment();
      if (status == HAL_OK)
      {
        (void)PS_HW6_SM_Transition(PS_HW6_SM_PMIC,
                                  PMIC_EV_SHIP_REQUEST,
                                  HAL_OK);
      }
    }
    else if ((status == HAL_OK) &&
             (start_event ==
              (uint32_t)PS_INPUT_START_POWER_EVENT_SHIP_IMMINENT))
    {
      PS_HW6_StartPowerRequestSoftwareShipment();
    }
    else if ((status == HAL_OK) &&
             (start_event ==
              (uint32_t)PS_INPUT_START_POWER_EVENT_RELEASED_BEFORE_SHIP))
    {
      (void)PS_HW6_SM_Transition(PS_HW6_SM_PMIC,
                                PMIC_EV_RECOVER_OK,
                                HAL_OK);
    }
  }

  g_ps_hw6_owner_sm_probe.start_power_last_status = (uint32_t)status;
  PS_HW6_TracePowerStart(start_event,
                         hold_ticks,
                         (uint32_t)status,
                         power_event);
  return status;
}

HAL_StatusTypeDef PS_HW6_OwnerStateMachines_HandlePmicInterrupt(
  uint32_t now_tick,
  uint32_t pending_count,
  uint32_t irq_count,
  uint32_t gpio_pin,
  uint32_t level,
  uint32_t irq_tick)
{
  HAL_StatusTypeDef snapshot_status;

  g_ps_hw6_owner_sm_probe.pmic_int_irq_count = irq_count;
  g_ps_hw6_owner_sm_probe.pmic_int_pending_count = pending_count;
  g_ps_hw6_owner_sm_probe.pmic_int_last_pin = gpio_pin;
  g_ps_hw6_owner_sm_probe.pmic_int_last_level = level;
  g_ps_hw6_owner_sm_probe.pmic_int_last_irq_tick = irq_tick;
  g_ps_hw6_owner_sm_probe.pmic_int_snapshot_count++;
  g_ps_hw6_owner_sm_probe.pmic_int_last_snapshot_tick = now_tick;

  snapshot_status = PS_HW6_PowerOwner_RunSnapshot();
  PS_HW6_SM_UpdateUsbHostAvailability(
    (uint32_t)PS_HW6_USB_HOST_EVENT_POWER_SNAPSHOT);
  g_ps_hw6_owner_sm_probe.pmic_int_last_snapshot_status =
    (uint32_t)snapshot_status;
  PS_HW6_TracePmicInterrupt(pending_count,
                             irq_count,
                             level,
                             (uint32_t)snapshot_status);
  return PS_HW6_SM_EvaluateBatteryPolicy(snapshot_status, 0UL);
}

HAL_StatusTypeDef PS_HW6_OwnerStateMachines_RunBatteryMonitor(
  uint32_t now_tick)
{
  HAL_StatusTypeDef snapshot_status;
  uint32_t last_tick = g_ps_hw6_owner_sm_probe.battery_policy_last_tick;

  if (ps_power_battery_monitor_period_ticks == 0UL)
  {
    ps_power_battery_monitor_period_ticks =
      PS_HW6_SM_MsToTicks((uint32_t)KNOB_POWER_BATTERY_MONITOR_PERIOD_MS);
  }

  if ((last_tick != 0UL) &&
      ((now_tick - last_tick) < ps_power_battery_monitor_period_ticks))
  {
    g_ps_hw6_owner_sm_probe.battery_policy_next_tick =
      last_tick + ps_power_battery_monitor_period_ticks;
    return HAL_OK;
  }

  snapshot_status = PS_HW6_PowerOwner_RunSnapshot();
  PS_HW6_SM_UpdateUsbHostAvailability(
    (uint32_t)PS_HW6_USB_HOST_EVENT_POWER_SNAPSHOT);
  return PS_HW6_SM_EvaluateBatteryPolicy(snapshot_status, 0UL);
}

HAL_StatusTypeDef PS_HW6_OwnerStateMachines_Stabilize(uint32_t owner_id)
{
  HAL_StatusTypeDef status = HAL_ERROR;
  uint32_t owner_bit;

  if (owner_id >= PS_HW6_OWNER_SM_PHYSICAL_OWNER_COUNT)
  {
    return HAL_ERROR;
  }

  owner_bit = 1UL << owner_id;
  if ((g_ps_hw6_owner_sm_probe.completed_owner_mask & owner_bit) != 0UL)
  {
    return (HAL_StatusTypeDef)
      g_ps_hw6_owner_sm_probe.owner_action_status[owner_id];
  }

  g_ps_hw6_owner_sm_probe.owner_action_start_tick[owner_id] =
    (uint32_t)tx_time_get();
  switch (owner_id)
  {
    case PS_HW6_RTOS_OWNER_POWER:
      status = PS_HW6_SM_StabilizePower();
      break;
    case PS_HW6_RTOS_OWNER_AUDIO:
      status = PS_HW6_SM_StabilizeAudio();
      break;
    case PS_HW6_RTOS_OWNER_INPUT:
      status = PS_HW6_SM_StabilizeJoystick();
      break;
    case PS_HW6_RTOS_OWNER_DISPLAY:
      status = PS_HW6_SM_StabilizeDisplay();
      break;
    case PS_HW6_RTOS_OWNER_SENSOR:
      status = PS_HW6_SM_StabilizeImu();
      break;
    case PS_HW6_RTOS_OWNER_STORAGE:
      status = PS_HW6_SM_StabilizeStorage();
      break;
    case PS_HW6_RTOS_OWNER_COMM:
      status = PS_HW6_SM_StabilizeBle();
      break;
    default:
      status = HAL_ERROR;
      break;
  }

  g_ps_hw6_owner_sm_probe.owner_action_end_tick[owner_id] =
    (uint32_t)tx_time_get();
  g_ps_hw6_owner_sm_probe.owner_action_status[owner_id] = (uint32_t)status;
  g_ps_hw6_owner_sm_probe.completed_owner_mask |= owner_bit;
  if (status == HAL_OK)
  {
    g_ps_hw6_owner_sm_probe.success_owner_mask |= owner_bit;
  }
  else
  {
    g_ps_hw6_owner_sm_probe.failure_owner_mask |= owner_bit;
  }
  return status;
}

void PS_HW6_OwnerStateMachines_BeginCycle(uint32_t cycle_index)
{
  if (cycle_index >= PS_HW6_OWNER_SM_CYCLE_COUNT)
  {
    return;
  }
  g_ps_hw6_owner_sm_probe.cycle_start_tick[cycle_index] =
    (uint32_t)tx_time_get();
}

HAL_StatusTypeDef PS_HW6_OwnerStateMachines_Resume(uint32_t owner_id,
                                                    uint32_t cycle_index)
{
  HAL_StatusTypeDef status = HAL_ERROR;
  uint32_t owner_bit;

  if ((cycle_index >= PS_HW6_OWNER_SM_CYCLE_COUNT) ||
      (owner_id >= PS_HW6_OWNER_SM_PHYSICAL_OWNER_COUNT))
  {
    return HAL_ERROR;
  }
  if (g_ps_hw6_owner_sm_probe
        .cycle_action_status[cycle_index][PS_HW6_OWNER_SM_CYCLE_RESUME]
                            [owner_id] != PS_HW6_OWNER_SM_STATUS_NOT_RUN)
  {
    return HAL_ERROR;
  }

  owner_bit = 1UL << owner_id;
  g_ps_hw6_owner_sm_probe
    .cycle_action_start_tick[cycle_index][PS_HW6_OWNER_SM_CYCLE_RESUME]
                            [owner_id] = (uint32_t)tx_time_get();
  switch (owner_id)
  {
    case PS_HW6_RTOS_OWNER_POWER:
      status = PS_HW6_SM_ResumePower();
      break;
    case PS_HW6_RTOS_OWNER_AUDIO:
      status = PS_HW6_SM_ResumeAudio();
      break;
    case PS_HW6_RTOS_OWNER_INPUT:
      status = PS_HW6_SM_ResumeJoystick(cycle_index);
      break;
    case PS_HW6_RTOS_OWNER_DISPLAY:
      status = PS_HW6_SM_ResumeDisplay();
      break;
    case PS_HW6_RTOS_OWNER_SENSOR:
      status = PS_HW6_SM_ResumeImu(cycle_index);
      break;
    case PS_HW6_RTOS_OWNER_STORAGE:
      status = PS_HW6_SM_ResumeStorage(cycle_index);
      break;
    case PS_HW6_RTOS_OWNER_COMM:
      status = PS_HW6_SM_ResumeBle(cycle_index);
      break;
    default:
      status = HAL_ERROR;
      break;
  }

  g_ps_hw6_owner_sm_probe
    .cycle_action_end_tick[cycle_index][PS_HW6_OWNER_SM_CYCLE_RESUME]
                          [owner_id] = (uint32_t)tx_time_get();
  g_ps_hw6_owner_sm_probe
    .cycle_action_status[cycle_index][PS_HW6_OWNER_SM_CYCLE_RESUME]
                        [owner_id] = (uint32_t)status;
  if (status == HAL_OK)
  {
    g_ps_hw6_owner_sm_probe.cycle_resume_success_mask[cycle_index] |=
      owner_bit;
  }
  else
  {
    g_ps_hw6_owner_sm_probe.cycle_resume_failure_mask[cycle_index] |=
      owner_bit;
  }
  return status;
}

HAL_StatusTypeDef PS_HW6_OwnerStateMachines_Quiesce(uint32_t owner_id,
                                                     uint32_t cycle_index)
{
  HAL_StatusTypeDef status = HAL_ERROR;
  uint32_t owner_bit;

  if ((cycle_index >= PS_HW6_OWNER_SM_CYCLE_COUNT) ||
      (owner_id >= PS_HW6_OWNER_SM_PHYSICAL_OWNER_COUNT))
  {
    return HAL_ERROR;
  }
  if (g_ps_hw6_owner_sm_probe
        .cycle_action_status[cycle_index][PS_HW6_OWNER_SM_CYCLE_QUIESCE]
                            [owner_id] != PS_HW6_OWNER_SM_STATUS_NOT_RUN)
  {
    return HAL_ERROR;
  }

  owner_bit = 1UL << owner_id;
  g_ps_hw6_owner_sm_probe
    .cycle_action_start_tick[cycle_index][PS_HW6_OWNER_SM_CYCLE_QUIESCE]
                            [owner_id] = (uint32_t)tx_time_get();
  switch (owner_id)
  {
    case PS_HW6_RTOS_OWNER_POWER:
      status = PS_HW6_SM_QuiescePower();
      break;
    case PS_HW6_RTOS_OWNER_AUDIO:
      status = PS_HW6_SM_QuiesceAudio();
      break;
    case PS_HW6_RTOS_OWNER_INPUT:
      status = PS_HW6_SM_QuiesceJoystick(cycle_index);
      break;
    case PS_HW6_RTOS_OWNER_DISPLAY:
      status = PS_HW6_SM_QuiesceDisplay();
      break;
    case PS_HW6_RTOS_OWNER_SENSOR:
      status = PS_HW6_SM_QuiesceImu(cycle_index);
      break;
    case PS_HW6_RTOS_OWNER_STORAGE:
      status = PS_HW6_SM_QuiesceStorage(cycle_index);
      break;
    case PS_HW6_RTOS_OWNER_COMM:
      status = PS_HW6_SM_QuiesceBle(cycle_index);
      break;
    default:
      status = HAL_ERROR;
      break;
  }

  g_ps_hw6_owner_sm_probe
    .cycle_action_end_tick[cycle_index][PS_HW6_OWNER_SM_CYCLE_QUIESCE]
                          [owner_id] = (uint32_t)tx_time_get();
  g_ps_hw6_owner_sm_probe
    .cycle_action_status[cycle_index][PS_HW6_OWNER_SM_CYCLE_QUIESCE]
                        [owner_id] = (uint32_t)status;
  if (status == HAL_OK)
  {
    g_ps_hw6_owner_sm_probe.cycle_quiesce_success_mask[cycle_index] |=
      owner_bit;
  }
  else
  {
    g_ps_hw6_owner_sm_probe.cycle_quiesce_failure_mask[cycle_index] |=
      owner_bit;
  }
  return status;
}

static uint32_t PS_HW6_SM_Stop2TargetBleMode(void)
{
  uint32_t active_mode = g_ps_hw6_owner_sm_probe.ble_mode_active;

  if ((g_ps_hw6_owner_sm_probe.ble_mode_request_count > 0UL) &&
      (active_mode == (uint32_t)PS_HW6_COMM_BLE_MODE_RESET_HELD))
  {
    return (uint32_t)PS_HW6_COMM_BLE_MODE_RESET_HELD;
  }
  return (uint32_t)PS_HW6_COMM_BLE_MODE_SLEEP_SYSTEM_OFF;
}

static uint32_t PS_HW6_SM_Stop2BleResidentPolicyReady(void)
{
  uint32_t state = g_ps_hw6_owner_sm_probe.current_state[PS_HW6_SM_BLE];
  uint32_t target_mode = PS_HW6_SM_Stop2TargetBleMode();

  if (target_mode == (uint32_t)PS_HW6_COMM_BLE_MODE_RESET_HELD)
  {
    return (state == (uint32_t)BLE_OFF) ? 1UL : 0UL;
  }

  if (target_mode == (uint32_t)PS_HW6_COMM_BLE_MODE_SLEEP_SYSTEM_OFF)
  {
    return ((state == (uint32_t)BLE_SUSPENDED) &&
            (g_ps_hw6_owner_sm_probe.ble_mode_active ==
             (uint32_t)PS_HW6_COMM_BLE_MODE_SLEEP_SYSTEM_OFF) &&
            (g_ps_hw6_owner_sm_probe.ble_nrst_after ==
             (uint32_t)GPIO_PIN_SET) &&
            (g_ps_hw6_owner_sm_probe.ble_dsr_host_control_after ==
             g_ps_hw6_owner_sm_probe.ble_dsr_sleep_target_level)) ? 1UL : 0UL;
  }

  return 0UL;
}

static HAL_StatusTypeDef PS_HW6_SM_ApplyStop2BleResidentPolicy(void)
{
  HAL_StatusTypeDef status = HAL_ERROR;
  uint32_t state = g_ps_hw6_owner_sm_probe.current_state[PS_HW6_SM_BLE];
  uint32_t target_mode = PS_HW6_SM_Stop2TargetBleMode();

  g_ps_hw6_owner_sm_probe.stop2_policy_ble_target_mode = target_mode;
  g_ps_hw6_owner_sm_probe.stop2_policy_ble_active_mode =
    g_ps_hw6_owner_sm_probe.ble_mode_active;
  g_ps_hw6_owner_sm_probe.stop2_policy_ble_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;

  if (PS_HW6_SM_Stop2BleResidentPolicyReady() != 0UL)
  {
    status = HAL_OK;
  }
  else if (target_mode == (uint32_t)PS_HW6_COMM_BLE_MODE_RESET_HELD)
  {
    status = (state == (uint32_t)BLE_OFF) ? HAL_OK :
      PS_HW6_SM_HardShutdownBle();
  }
  else if (target_mode == (uint32_t)PS_HW6_COMM_BLE_MODE_SLEEP_SYSTEM_OFF)
  {
    if ((state == (uint32_t)BLE_IDLE) ||
        (state == (uint32_t)BLE_ADVERTISING) ||
        (state == (uint32_t)BLE_PAIRING) ||
        (state == (uint32_t)BLE_CONNECTED))
    {
      status = PS_HW6_SM_QuiesceBle(PS_HW6_POWER_QUIESCE_CYCLE_INDEX);
    }
    else
    {
      status = HAL_ERROR;
    }
  }
  else
  {
    status = HAL_ERROR;
  }

  g_ps_hw6_owner_sm_probe.stop2_policy_ble_active_mode =
    g_ps_hw6_owner_sm_probe.ble_mode_active;
  g_ps_hw6_owner_sm_probe.stop2_policy_ble_status = (uint32_t)status;
  return status;
}
static uint32_t PS_HW6_SM_Stop2TargetImuMode(void)
{
  uint32_t active_mode = g_ps_hw6_owner_sm_probe.imu_mode_active;

  if (active_mode == (uint32_t)PS_HW6_IMU_MODE_STEP_COUNTER)
  {
    return (uint32_t)PS_HW6_IMU_MODE_STEP_COUNTER;
  }
  return (uint32_t)PS_HW6_IMU_MODE_OFF;
}

static uint32_t PS_HW6_SM_Stop2ImuResidentPolicyReady(void)
{
  uint32_t state = g_ps_hw6_owner_sm_probe.current_state[PS_HW6_SM_IMU];
  uint32_t target_mode = PS_HW6_SM_Stop2TargetImuMode();

  if (target_mode == (uint32_t)PS_HW6_IMU_MODE_OFF)
  {
    return ((state == (uint32_t)IMU_SUSPENDED) &&
            (PS_HW6_SM_ImuDeepPowerDownProofValid() != 0UL)) ? 1UL : 0UL;
  }

  return 0UL;
}

uint32_t PS_HW6_OwnerStateMachines_Stop2IdlePeripheralsReady(void)
{
  return ((PS_HW6_SM_Stop2BleResidentPolicyReady() != 0UL) &&
          (PS_HW6_SM_Stop2ImuResidentPolicyReady() != 0UL)) ? 1UL : 0UL;
}

static HAL_StatusTypeDef PS_HW6_SM_ApplyStop2ImuResidentPolicy(void)
{
  HAL_StatusTypeDef status = HAL_ERROR;
  uint32_t state = g_ps_hw6_owner_sm_probe.current_state[PS_HW6_SM_IMU];
  uint32_t target_mode = PS_HW6_SM_Stop2TargetImuMode();

  g_ps_hw6_owner_sm_probe.stop2_policy_imu_target_mode = target_mode;
  g_ps_hw6_owner_sm_probe.stop2_policy_imu_active_mode =
    g_ps_hw6_owner_sm_probe.imu_mode_active;
  g_ps_hw6_owner_sm_probe.stop2_policy_imu_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;

  if (PS_HW6_SM_Stop2ImuResidentPolicyReady() != 0UL)
  {
    status = HAL_OK;
  }
  else if (target_mode == (uint32_t)PS_HW6_IMU_MODE_OFF)
  {
    if (state == (uint32_t)IMU_LOW_RATE_SAMPLE)
    {
      status = PS_HW6_SM_QuiesceImu(PS_HW6_POWER_QUIESCE_CYCLE_INDEX);
    }
    else
    {
      status = HAL_ERROR;
    }
    g_ps_hw6_owner_sm_probe.stop2_policy_imu_status = (uint32_t)status;
  }
  else
  {
    g_ps_hw6_owner_sm_probe.stop2_policy_imu_status =
      PS_HW6_OWNER_SM_STATUS_UNAVAILABLE;
    status = HAL_ERROR;
  }

  g_ps_hw6_owner_sm_probe.stop2_policy_imu_active_mode =
    g_ps_hw6_owner_sm_probe.imu_mode_active;
  return status;
}
void PS_HW6_OwnerStateMachines_BeginPowerQuiesce(uint32_t reason)
{
  uint32_t index;

  g_ps_hw6_owner_sm_probe.power_quiesce_request_count++;
  g_ps_hw6_owner_sm_probe.power_quiesce_reason = reason;
  g_ps_hw6_owner_sm_probe.power_quiesce_start_tick =
    (uint32_t)tx_time_get();
  g_ps_hw6_owner_sm_probe.power_quiesce_end_tick = 0UL;
  g_ps_hw6_owner_sm_probe.power_quiesce_required_mask =
    PS_HW6_SM_REQUIRED_OWNER_MASK & ~(1UL << PS_HW6_RTOS_OWNER_POWER);
  g_ps_hw6_owner_sm_probe.power_quiesce_send_ok_mask = 0UL;
  g_ps_hw6_owner_sm_probe.power_quiesce_ack_ok_mask = 0UL;
  g_ps_hw6_owner_sm_probe.power_quiesce_success_mask = 0UL;
  g_ps_hw6_owner_sm_probe.power_quiesce_failure_mask = 0UL;
  g_ps_hw6_owner_sm_probe.power_quiesce_last_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  if (reason == (uint32_t)PS_HW6_POWER_QUIESCE_REASON_SLEEP_PREP)
  {
    g_ps_hw6_owner_sm_probe.stop2_policy_request_count++;
    g_ps_hw6_owner_sm_probe.stop2_policy_reason = reason;
    g_ps_hw6_owner_sm_probe.stop2_policy_last_tick =
      (uint32_t)tx_time_get();
    g_ps_hw6_owner_sm_probe.stop2_policy_ble_target_mode =
      PS_HW6_SM_Stop2TargetBleMode();
    g_ps_hw6_owner_sm_probe.stop2_policy_ble_active_mode =
      g_ps_hw6_owner_sm_probe.ble_mode_active;
    g_ps_hw6_owner_sm_probe.stop2_policy_ble_status =
      PS_HW6_OWNER_SM_STATUS_NOT_RUN;
    g_ps_hw6_owner_sm_probe.stop2_policy_imu_target_mode =
      PS_HW6_SM_Stop2TargetImuMode();
    g_ps_hw6_owner_sm_probe.stop2_policy_imu_active_mode =
      g_ps_hw6_owner_sm_probe.imu_mode_active;
    g_ps_hw6_owner_sm_probe.stop2_policy_imu_status =
      PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  }
  for (index = 0U; index < PS_HW6_OWNER_SM_PHYSICAL_OWNER_COUNT; ++index)
  {
    g_ps_hw6_owner_sm_probe.power_quiesce_send_status[index] =
      PS_HW6_OWNER_SM_STATUS_NOT_RUN;
    g_ps_hw6_owner_sm_probe.power_quiesce_ack_status[index] =
      PS_HW6_OWNER_SM_STATUS_NOT_RUN;
    g_ps_hw6_owner_sm_probe.power_quiesce_ack_flags[index] = 0UL;
    g_ps_hw6_owner_sm_probe.power_quiesce_owner_status[index] =
      PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  }
}

HAL_StatusTypeDef PS_HW6_OwnerStateMachines_QuiesceForPowerBarrier(
  uint32_t owner_id)
{
  HAL_StatusTypeDef status = HAL_ERROR;
  uint32_t owner_bit;
  uint32_t state;

  if ((owner_id == PS_HW6_RTOS_OWNER_POWER) ||
      (owner_id >= PS_HW6_OWNER_SM_PHYSICAL_OWNER_COUNT))
  {
    return HAL_ERROR;
  }

  owner_bit = 1UL << owner_id;
  switch (owner_id)
  {
    case PS_HW6_RTOS_OWNER_AUDIO:
      state = g_ps_hw6_owner_sm_probe.current_state[PS_HW6_SM_AUDIO];
      if ((state == (uint32_t)AUDIO_OFF) ||
          ((state == (uint32_t)AUDIO_IDLE) &&
           (g_ps_hw6_owner_sm_probe.current_state[PS_HW6_SM_SPEAKER] ==
            (uint32_t)SPK_OFF)))
      {
        status = HAL_OK;
      }
      else
      {
        status = PS_HW6_SM_QuiesceAudio();
      }
      break;

    case PS_HW6_RTOS_OWNER_INPUT:
      status = PS_HW6_SM_QuiesceJoystick(
        PS_HW6_POWER_QUIESCE_CYCLE_INDEX);
      break;

    case PS_HW6_RTOS_OWNER_DISPLAY:
      state = g_ps_hw6_owner_sm_probe.current_state[PS_HW6_SM_DISPLAY];
      if ((g_ps_hw6_owner_sm_probe.power_quiesce_reason ==
           (uint32_t)PS_HW6_POWER_QUIESCE_REASON_SLEEP_PREP) &&
          (g_ps_hw6_owner_probe.display_lpbam_prearmed != 0UL) &&
          (g_ps_hw6_owner_probe.display_lpbam_ready != 0UL))
      {
        status = PS_HW6_DisplayOwner_CommitLpbamStop2();
      }
      else
      {
        status = ((state == (uint32_t)DISP_OFF) ||
                  (state == (uint32_t)DISP_STATIC_HOLD)) ?
          HAL_OK : PS_HW6_SM_QuiesceDisplay();
      }
      break;

    case PS_HW6_RTOS_OWNER_SENSOR:
      if (g_ps_hw6_owner_sm_probe.power_quiesce_reason ==
          (uint32_t)PS_HW6_POWER_QUIESCE_REASON_SLEEP_PREP)
      {
        status = PS_HW6_SM_ApplyStop2ImuResidentPolicy();
      }
      else
      {
        state = g_ps_hw6_owner_sm_probe.current_state[PS_HW6_SM_IMU];
        if ((state == (uint32_t)IMU_OFF) ||
            (state == (uint32_t)IMU_SUSPENDED))
        {
          status = HAL_OK;
        }
        else
        {
          status = PS_HW6_SM_QuiesceImu(PS_HW6_POWER_QUIESCE_CYCLE_INDEX);
        }
      }
      break;

    case PS_HW6_RTOS_OWNER_STORAGE:
      if ((g_ps_hw6_owner_sm_probe.current_state[PS_HW6_SM_STORAGE] ==
           (uint32_t)STORAGE_OFFLINE) ||
          (g_ps_hw6_owner_sm_probe.current_state[PS_HW6_SM_FLASH] ==
           (uint32_t)FLASH_OFF))
      {
        status = HAL_OK;
      }
      else if (g_ps_hw6_owner_sm_probe.current_state[PS_HW6_SM_FLASH] ==
               (uint32_t)FLASH_DEEP_POWER_DOWN)
      {
        PS_HW6_SM_ParkOspiClocksForStop();
        status = HAL_OK;
      }
      else
      {
        status = PS_HW6_SM_QuiesceStorage(
          PS_HW6_POWER_QUIESCE_CYCLE_INDEX);
      }
      break;

    case PS_HW6_RTOS_OWNER_COMM:
      if (g_ps_hw6_owner_sm_probe.power_quiesce_reason ==
          (uint32_t)PS_HW6_POWER_QUIESCE_REASON_SLEEP_PREP)
      {
        status = PS_HW6_SM_ApplyStop2BleResidentPolicy();
      }
      else
      {
        state = g_ps_hw6_owner_sm_probe.current_state[PS_HW6_SM_BLE];
        if ((state == (uint32_t)BLE_OFF) ||
            (state == (uint32_t)BLE_SUSPENDED))
        {
          status = HAL_OK;
        }
        else
        {
          status = PS_HW6_SM_QuiesceBle(PS_HW6_POWER_QUIESCE_CYCLE_INDEX);
        }
      }
      break;

    default:
      status = HAL_ERROR;
      break;
  }

  g_ps_hw6_owner_sm_probe.power_quiesce_owner_status[owner_id] =
    (uint32_t)status;
  if (status == HAL_OK)
  {
    g_ps_hw6_owner_sm_probe.power_quiesce_success_mask |= owner_bit;
  }
  else
  {
    g_ps_hw6_owner_sm_probe.power_quiesce_failure_mask |= owner_bit;
  }
  return status;
}

void PS_HW6_OwnerStateMachines_RecordPowerQuiesceCommand(
  uint32_t owner_id,
  uint32_t send_status,
  uint32_t ack_wait_status,
  uint32_t ack_flags)
{
  uint32_t owner_bit;

  if ((owner_id == PS_HW6_RTOS_OWNER_POWER) ||
      (owner_id >= PS_HW6_OWNER_SM_PHYSICAL_OWNER_COUNT))
  {
    return;
  }

  owner_bit = 1UL << owner_id;
  g_ps_hw6_owner_sm_probe.power_quiesce_send_status[owner_id] =
    send_status;
  g_ps_hw6_owner_sm_probe.power_quiesce_ack_status[owner_id] =
    ack_wait_status;
  g_ps_hw6_owner_sm_probe.power_quiesce_ack_flags[owner_id] = ack_flags;
  if (send_status == TX_SUCCESS)
  {
    g_ps_hw6_owner_sm_probe.power_quiesce_send_ok_mask |= owner_bit;
  }
  if ((ack_wait_status == TX_SUCCESS) &&
      ((ack_flags & owner_bit) != 0UL))
  {
    g_ps_hw6_owner_sm_probe.power_quiesce_ack_ok_mask |= owner_bit;
  }
}

HAL_StatusTypeDef PS_HW6_OwnerStateMachines_EndPowerQuiesce(void)
{
  uint32_t required_mask =
    g_ps_hw6_owner_sm_probe.power_quiesce_required_mask;
  HAL_StatusTypeDef status;

  g_ps_hw6_owner_sm_probe.power_quiesce_end_tick =
    (uint32_t)tx_time_get();
  status = (((g_ps_hw6_owner_sm_probe.power_quiesce_send_ok_mask &
              required_mask) == required_mask) &&
            ((g_ps_hw6_owner_sm_probe.power_quiesce_ack_ok_mask &
              required_mask) == required_mask) &&
            ((g_ps_hw6_owner_sm_probe.power_quiesce_success_mask &
              required_mask) == required_mask) &&
            ((g_ps_hw6_owner_sm_probe.power_quiesce_failure_mask &
              required_mask) == 0UL)) ? HAL_OK : HAL_ERROR;
  g_ps_hw6_owner_sm_probe.power_quiesce_last_status = (uint32_t)status;
  return status;
}


void PS_HW6_OwnerStateMachines_BeginPostStopResume(void)
{
  uint32_t index;

  g_ps_hw6_owner_sm_probe.post_stop_resume_request_count++;
  g_ps_hw6_owner_sm_probe.post_stop_resume_start_tick =
    (uint32_t)tx_time_get();
  g_ps_hw6_owner_sm_probe.post_stop_resume_end_tick = 0UL;
  g_ps_hw6_owner_sm_probe.post_stop_resume_required_mask =
    PS_HW6_SM_REQUIRED_OWNER_MASK & ~(1UL << PS_HW6_RTOS_OWNER_POWER);
  g_ps_hw6_owner_sm_probe.post_stop_resume_send_ok_mask = 0UL;
  g_ps_hw6_owner_sm_probe.post_stop_resume_ack_ok_mask = 0UL;
  g_ps_hw6_owner_sm_probe.post_stop_resume_success_mask = 0UL;
  g_ps_hw6_owner_sm_probe.post_stop_resume_failure_mask = 0UL;
  g_ps_hw6_owner_sm_probe.post_stop_resume_noop_mask = 0UL;
  g_ps_hw6_owner_sm_probe.post_stop_resume_action_mask = 0UL;
  g_ps_hw6_owner_sm_probe.post_stop_resume_last_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  for (index = 0U; index < PS_HW6_OWNER_SM_PHYSICAL_OWNER_COUNT; ++index)
  {
    g_ps_hw6_owner_sm_probe.post_stop_resume_send_status[index] =
      PS_HW6_OWNER_SM_STATUS_NOT_RUN;
    g_ps_hw6_owner_sm_probe.post_stop_resume_ack_status[index] =
      PS_HW6_OWNER_SM_STATUS_NOT_RUN;
    g_ps_hw6_owner_sm_probe.post_stop_resume_ack_flags[index] = 0UL;
    g_ps_hw6_owner_sm_probe.post_stop_resume_owner_status[index] =
      PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  }
}

HAL_StatusTypeDef PS_HW6_OwnerStateMachines_ResumeForPostStopBarrier(
  uint32_t owner_id)
{
  HAL_StatusTypeDef status = HAL_ERROR;
  uint32_t owner_bit;
  uint32_t state;
  uint32_t action_required = 0UL;

  if ((owner_id == PS_HW6_RTOS_OWNER_POWER) ||
      (owner_id >= PS_HW6_OWNER_SM_PHYSICAL_OWNER_COUNT))
  {
    return HAL_ERROR;
  }

  owner_bit = 1UL << owner_id;
  switch (owner_id)
  {
    case PS_HW6_RTOS_OWNER_AUDIO:
      state = g_ps_hw6_owner_sm_probe.current_state[PS_HW6_SM_AUDIO];
      if ((state == (uint32_t)AUDIO_OFF) ||
          ((state == (uint32_t)AUDIO_IDLE) &&
           (g_ps_hw6_owner_sm_probe.current_state[PS_HW6_SM_SPEAKER] ==
            (uint32_t)SPK_OFF)))
      {
        status = HAL_OK;
      }
      break;

    case PS_HW6_RTOS_OWNER_INPUT:
      state = g_ps_hw6_owner_sm_probe.current_state[PS_HW6_SM_JOYSTICK];
      if (state == (uint32_t)JOY_OFF)
      {
        status = HAL_OK;
      }
      else if (state == (uint32_t)JOY_SUSPENDED)
      {
        action_required = 1UL;
        status = PS_HW6_SM_ResumeJoystick(PS_HW6_POWER_QUIESCE_CYCLE_INDEX);
      }
      break;

    case PS_HW6_RTOS_OWNER_DISPLAY:
      state = g_ps_hw6_owner_sm_probe.current_state[PS_HW6_SM_DISPLAY];
      if ((state == (uint32_t)DISP_OFF) ||
          (state == (uint32_t)DISP_STATIC_HOLD))
      {
        status = HAL_OK;
      }
      break;

    case PS_HW6_RTOS_OWNER_SENSOR:
      state = g_ps_hw6_owner_sm_probe.current_state[PS_HW6_SM_IMU];
      if ((state == (uint32_t)IMU_OFF) ||
          (state == (uint32_t)IMU_LOW_RATE_SAMPLE))
      {
        status = HAL_OK;
      }
      else if (state == (uint32_t)IMU_SUSPENDED)
      {
        uint32_t mode = g_ps_hw6_owner_sm_probe.imu_mode_active;

        if (mode == (uint32_t)PS_HW6_IMU_MODE_LOW_RATE_SAMPLE)
        {
          action_required = 1UL;
          status = PS_HW6_SM_ResumeImu(PS_HW6_POWER_QUIESCE_CYCLE_INDEX);
        }
        else
        {
          status = HAL_OK;
        }
      }
      break;

    case PS_HW6_RTOS_OWNER_STORAGE:
      if ((g_ps_hw6_owner_sm_probe.current_state[PS_HW6_SM_STORAGE] ==
           (uint32_t)STORAGE_OFFLINE) ||
          (g_ps_hw6_owner_sm_probe.current_state[PS_HW6_SM_FLASH] ==
           (uint32_t)FLASH_OFF) ||
          ((g_ps_hw6_owner_sm_probe.current_state[PS_HW6_SM_STORAGE] ==
            (uint32_t)STORAGE_FLASH_READY) &&
           (g_ps_hw6_owner_sm_probe.current_state[PS_HW6_SM_FLASH] ==
            (uint32_t)FLASH_READY)))
      {
        status = HAL_OK;
      }
      else if ((g_ps_hw6_owner_sm_probe.current_state[PS_HW6_SM_STORAGE] ==
                (uint32_t)STORAGE_FLASH_READY) &&
               (g_ps_hw6_owner_sm_probe.current_state[PS_HW6_SM_FLASH] ==
                (uint32_t)FLASH_DEEP_POWER_DOWN))
      {
        action_required = 1UL;
        status = PS_HW6_SM_ResumeStorage(PS_HW6_POWER_QUIESCE_CYCLE_INDEX);
      }
      break;

    case PS_HW6_RTOS_OWNER_COMM:
      state = g_ps_hw6_owner_sm_probe.current_state[PS_HW6_SM_BLE];
      if ((state == (uint32_t)BLE_OFF) ||
          (state == (uint32_t)BLE_IDLE) ||
          (state == (uint32_t)BLE_ADVERTISING) ||
          (state == (uint32_t)BLE_PAIRING) ||
          (state == (uint32_t)BLE_CONNECTED))
      {
        status = HAL_OK;
      }
      else if (state == (uint32_t)BLE_SUSPENDED)
      {
        uint32_t mode = g_ps_hw6_owner_sm_probe.ble_mode_active;

        if ((mode == (uint32_t)PS_HW6_COMM_BLE_MODE_SEARCHING) ||
            (mode == (uint32_t)PS_HW6_COMM_BLE_MODE_PAIRING) ||
            (mode == (uint32_t)PS_HW6_COMM_BLE_MODE_CONNECTED))
        {
          action_required = 1UL;
          status = PS_HW6_OwnerStateMachines_SetBleMode(mode);
        }
        else
        {
          status = HAL_OK;
        }
      }
      break;

    default:
      status = HAL_ERROR;
      break;
  }

  g_ps_hw6_owner_sm_probe.post_stop_resume_owner_status[owner_id] =
    (uint32_t)status;
  if (status == HAL_OK)
  {
    g_ps_hw6_owner_sm_probe.post_stop_resume_success_mask |= owner_bit;
    if (action_required != 0UL)
    {
      g_ps_hw6_owner_sm_probe.post_stop_resume_action_mask |= owner_bit;
    }
    else
    {
      g_ps_hw6_owner_sm_probe.post_stop_resume_noop_mask |= owner_bit;
    }
  }
  else
  {
    g_ps_hw6_owner_sm_probe.post_stop_resume_failure_mask |= owner_bit;
  }
  return status;
}

void PS_HW6_OwnerStateMachines_RecordPostStopResumeCommand(
  uint32_t owner_id,
  uint32_t send_status,
  uint32_t ack_wait_status,
  uint32_t ack_flags)
{
  uint32_t owner_bit;

  if ((owner_id == PS_HW6_RTOS_OWNER_POWER) ||
      (owner_id >= PS_HW6_OWNER_SM_PHYSICAL_OWNER_COUNT))
  {
    return;
  }

  owner_bit = 1UL << owner_id;
  g_ps_hw6_owner_sm_probe.post_stop_resume_send_status[owner_id] =
    send_status;
  g_ps_hw6_owner_sm_probe.post_stop_resume_ack_status[owner_id] =
    ack_wait_status;
  g_ps_hw6_owner_sm_probe.post_stop_resume_ack_flags[owner_id] = ack_flags;
  if (send_status == TX_SUCCESS)
  {
    g_ps_hw6_owner_sm_probe.post_stop_resume_send_ok_mask |= owner_bit;
  }
  if ((ack_wait_status == TX_SUCCESS) &&
      ((ack_flags & owner_bit) != 0UL))
  {
    g_ps_hw6_owner_sm_probe.post_stop_resume_ack_ok_mask |= owner_bit;
  }
}

HAL_StatusTypeDef PS_HW6_OwnerStateMachines_EndPostStopResume(void)
{
  uint32_t required_mask =
    g_ps_hw6_owner_sm_probe.post_stop_resume_required_mask;
  HAL_StatusTypeDef status;

  g_ps_hw6_owner_sm_probe.post_stop_resume_end_tick =
    (uint32_t)tx_time_get();
  status = (((g_ps_hw6_owner_sm_probe.post_stop_resume_send_ok_mask &
              required_mask) == required_mask) &&
            ((g_ps_hw6_owner_sm_probe.post_stop_resume_ack_ok_mask &
              required_mask) == required_mask) &&
            ((g_ps_hw6_owner_sm_probe.post_stop_resume_success_mask &
              required_mask) == required_mask) &&
            ((g_ps_hw6_owner_sm_probe.post_stop_resume_failure_mask &
              required_mask) == 0UL)) ? HAL_OK : HAL_ERROR;
  g_ps_hw6_owner_sm_probe.post_stop_resume_last_status = (uint32_t)status;
  return status;
}

void PS_HW6_OwnerStateMachines_RecordCycleCommand(
  uint32_t cycle_index,
  uint32_t direction,
  uint32_t owner_id,
  uint32_t send_status,
  uint32_t ack_wait_status,
  uint32_t ack_flags)
{
  if ((cycle_index >= PS_HW6_OWNER_SM_CYCLE_COUNT) ||
      (direction >= PS_HW6_OWNER_SM_CYCLE_DIRECTION_COUNT) ||
      (owner_id >= PS_HW6_OWNER_SM_PHYSICAL_OWNER_COUNT))
  {
    return;
  }
  g_ps_hw6_owner_sm_probe
    .cycle_command_send_status[cycle_index][direction][owner_id] =
    send_status;
  g_ps_hw6_owner_sm_probe
    .cycle_ack_wait_status[cycle_index][direction][owner_id] =
    ack_wait_status;
  g_ps_hw6_owner_sm_probe
    .cycle_ack_flags[cycle_index][direction][owner_id] = ack_flags;
}

void PS_HW6_OwnerStateMachines_RecordCycleActiveStates(uint32_t cycle_index)
{
  if (cycle_index >= PS_HW6_OWNER_SM_CYCLE_COUNT)
  {
    return;
  }
  g_ps_hw6_owner_sm_probe.cycle_active_tick[cycle_index] =
    (uint32_t)tx_time_get();
  g_ps_hw6_owner_sm_probe.cycle_active_state_match_mask[cycle_index] =
    PS_HW6_SM_StateMatchMask(ps_cycle_active_states);
}

void PS_HW6_OwnerStateMachines_EndCycle(uint32_t cycle_index)
{
  uint32_t index;
  uint32_t success = 1UL;

  if (cycle_index >= PS_HW6_OWNER_SM_CYCLE_COUNT)
  {
    return;
  }
  g_ps_hw6_owner_sm_probe.cycle_end_tick[cycle_index] =
    (uint32_t)tx_time_get();
  g_ps_hw6_owner_sm_probe.cycle_inactive_state_match_mask[cycle_index] =
    PS_HW6_SM_StateMatchMask(ps_cycle_inactive_states);
  if (g_ps_hw6_owner_sm_probe.cycle_completed_count < (cycle_index + 1U))
  {
    g_ps_hw6_owner_sm_probe.cycle_completed_count = cycle_index + 1U;
  }

  if (g_ps_hw6_owner_sm_probe.cycle_completed_count !=
      PS_HW6_OWNER_SM_CYCLE_COUNT)
  {
    g_ps_hw6_owner_sm_probe.cycle_success = 0UL;
    return;
  }

  for (index = 0U; index < PS_HW6_OWNER_SM_CYCLE_COUNT; ++index)
  {
    if ((g_ps_hw6_owner_sm_probe.cycle_resume_success_mask[index] !=
         PS_HW6_SM_REQUIRED_OWNER_MASK) ||
        (g_ps_hw6_owner_sm_probe.cycle_resume_failure_mask[index] != 0UL) ||
        (g_ps_hw6_owner_sm_probe.cycle_quiesce_success_mask[index] !=
         PS_HW6_SM_REQUIRED_OWNER_MASK) ||
        (g_ps_hw6_owner_sm_probe.cycle_quiesce_failure_mask[index] != 0UL) ||
        (g_ps_hw6_owner_sm_probe.cycle_active_state_match_mask[index] !=
         PS_HW6_SM_ALL_STATE_MASK) ||
        (g_ps_hw6_owner_sm_probe.cycle_inactive_state_match_mask[index] !=
         PS_HW6_SM_ALL_STATE_MASK))
    {
      success = 0UL;
    }
  }
  g_ps_hw6_owner_sm_probe.cycle_success = success;
}

void PS_HW6_OwnerStateMachines_RecordCommand(uint32_t owner_id,
                                              uint32_t send_status,
                                              uint32_t ack_wait_status,
                                              uint32_t ack_flags)
{
  if (owner_id >= PS_HW6_OWNER_SM_PHYSICAL_OWNER_COUNT)
  {
    return;
  }
  g_ps_hw6_owner_sm_probe.owner_command_send_status[owner_id] = send_status;
  g_ps_hw6_owner_sm_probe.owner_ack_wait_status[owner_id] = ack_wait_status;
  g_ps_hw6_owner_sm_probe.owner_ack_flags[owner_id] = ack_flags;
}

void PS_HW6_OwnerStateMachines_EndWorkflow(void)
{
  g_ps_hw6_owner_sm_probe.workflow_end_tick = (uint32_t)tx_time_get();
  g_ps_hw6_owner_sm_probe.complete = 1UL;
  g_ps_hw6_owner_sm_probe.success =
    ((g_ps_hw6_owner_sm_probe.completed_owner_mask ==
      g_ps_hw6_owner_sm_probe.required_owner_mask) &&
     (g_ps_hw6_owner_sm_probe.success_owner_mask ==
      g_ps_hw6_owner_sm_probe.required_owner_mask) &&
     (g_ps_hw6_owner_sm_probe.failure_owner_mask == 0UL) &&
     (g_ps_hw6_owner_sm_probe.cycle_completed_count ==
      g_ps_hw6_owner_sm_probe.cycle_requested_count) &&
     (g_ps_hw6_owner_sm_probe.cycle_success != 0UL)) ? 1UL : 0UL;
  g_ps_hw6_owner_sm_probe.phase = PS_HW6_SM_PHASE_COMPLETE;
}
