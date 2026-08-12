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
#define PS_HW6_IMU_ADDRESS                (0x18U)
#define PS_HW6_JOYSTICK_SWEEP_DURATION_TICKS (500UL)
#define PS_HW6_JOYSTICK_SWEEP_PERIOD_TICKS   (5UL)
#define PS_HW6_JOYSTICK_LIVE_DURATION_TICKS  (250UL)
#define PS_HW6_JOYSTICK_LIVE_PERIOD_TICKS    (2UL)
#define PS_HW6_JOYSTICK_CAL_MIN_DEADZONE     (3500)
#define PS_HW6_JOYSTICK_CAL_DEADZONE_PAD     (1500)
#define PS_HW6_JOYSTICK_CAL_MAX_DEADZONE     (12000)

#define PS_HW6_FLASH_WAKE_SETTLE_TICKS     (1UL)
#define PS_HW6_FLASH_SCRATCH_ADDRESS       (0x00FFF000UL)
#define PS_HW6_FLASH_SCRATCH_BLOCK_INDEX   (PS_HW6_FLASH_SCRATCH_ADDRESS / \
                                            PS_DEV_AT25SL128A_SECTOR_SIZE)

#define PS_HW6_NINA_DSR_HOST_CONTROL_PIN   (GPIO_PIN_8)
#define PS_HW6_NINA_DSR_HOST_CONTROL_PORT  (GPIOC)
#define PS_HW6_NINA_RESET_TICKS            (2UL)
#define PS_HW6_NINA_BOOT_TICKS             (75UL)
#define PS_HW6_NINA_BOOT_DRAIN_TICKS       (20UL)
#define PS_HW6_NINA_RX_WINDOW_TICKS        (100UL)
#define PS_HW6_NINA_RX_QUIET_TICKS         (5UL)
#define PS_HW6_NINA_STOP_SETTLE_TICKS      (110UL)
#define PS_HW6_NINA_WAKE_SETTLE_TICKS      (110UL)
#define PS_HW6_NINA_RX_BYTE_TIMEOUT_MS     (10U)
#define PS_HW6_NINA_TX_TIMEOUT_MS          (250U)

#define PS_HW6_NINA_RX_BUFFER_SIZE         (128U)
#define PS_HW6_NINA_REQUIRED_COMMAND_MASK  (0x79UL)
#define PS_HW6_NINA_UNSUPPORTED_COMMAND_MASK (0x06UL)

#define PS_HW6_ARRAY_COUNT(array) \
  (sizeof(array) / sizeof((array)[0]))

#define PS_HW6_BATTERY_FUEL_VBAT_OK_MASK  (0x0CUL)
#define PS_HW6_CHARGER_STATUS_CHARGING     (1UL)
#define PS_HW6_CHARGER_STATUS_FULL         (2UL)

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
volatile uint32_t g_ps_hw6_storage_usb_export_request;
volatile uint32_t g_ps_hw6_storage_usb_reclaim_request;
volatile uint32_t g_ps_hw6_joystick_sample_request;
volatile uint32_t g_ps_hw6_joystick_live_request;
volatile uint32_t g_ps_hw6_joystick_cardinal_request;
volatile uint32_t g_ps_hw6_joystick_calibration_capture_request;
volatile uint32_t g_ps_hw6_joystick_calibration_capture_page;

static uint32_t ps_start_power_return_state;
static uint32_t ps_power_battery_monitor_period_ticks;
static uint32_t ps_power_battery_owns_ship_prep;
static uint32_t ps_power_boot_restart_gate_pending;
static uint32_t ps_power_boot_restart_gate_blocked;
static PS_HW6_PowerQuiesceBarrierCallback ps_power_quiesce_barrier_callback;
static PS_HW6_PostStopResumeBarrierCallback ps_post_stop_resume_barrier_callback;
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

static HAL_StatusTypeDef PS_HW6_RequestPowerQuiesce(uint32_t reason)
{
  if (ps_power_quiesce_barrier_callback == NULL)
  {
    return HAL_ERROR;
  }
  return ps_power_quiesce_barrier_callback(reason);
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
  {BLE_IDLE, BLE_EV_DISABLE_REQUEST, BLE_SUSPENDING},
  {BLE_SUSPENDING, BLE_EV_QUIESCE, BLE_SUSPENDED},
  {BLE_SUSPENDED, BLE_EV_RESUME, BLE_BOOT_WAIT},
  {BLE_RESET_ASSERT, BLE_EV_FAULT, BLE_ERROR},
  {BLE_IDLE, BLE_EV_FAULT, BLE_ERROR},
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
static uint8_t ps_nina_rx_buffer[PS_HW6_NINA_RX_BUFFER_SIZE];
static ps_status_t PS_HW6_SM_EnsureFlashAwake(void);
static HAL_StatusTypeDef PS_HW6_SM_ParkUsb(void);
static void PS_HW6_SM_RecordUsbExportEntryState(void);
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

static void PS_HW6_SM_ResetJoystickRuntimeProbes(void)
{
  PS_HW6_SM_ResetJoystickInputProbe();
  PS_HW6_SM_ResetJoystickSampleProbe();
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

static HAL_StatusTypeDef PS_HW6_SM_StabilizeImu(void)
{
  ps_dev_lis2dux12_stabilize_result_t result;
  ps_status_t driver_status;
  HAL_StatusTypeDef status;
  uint32_t index;

  (void)PS_HW6_SM_Transition(PS_HW6_SM_IMU,
                            IMU_EV_ENABLE_REQUEST, HAL_OK);
  driver_status = ps_dev_lis2dux12_stabilize_suspended(
    &ps_imu_device,
    &result);
  status = PS_HW6_SM_StatusToHal(driver_status);

  g_ps_hw6_owner_sm_probe.imu_ready_status = result.whoami_hal_status;
  g_ps_hw6_owner_sm_probe.imu_whoami_status = result.whoami_hal_status;
  g_ps_hw6_owner_sm_probe.imu_whoami = result.whoami;
  g_ps_hw6_owner_sm_probe.imu_identity_match = result.identity_match;
  for (index = 0U; index < PS_HW6_OWNER_SM_IMU_REGISTER_COUNT; ++index)
  {
    g_ps_hw6_owner_sm_probe.imu_register_address[index] =
      result.register_address[index];
    g_ps_hw6_owner_sm_probe.imu_register_before[index] =
      result.register_before[index];
    g_ps_hw6_owner_sm_probe.imu_register_after[index] =
      result.register_after[index];
  }
  g_ps_hw6_owner_sm_probe.imu_snapshot_ok_mask = result.snapshot_ok_mask;
  g_ps_hw6_owner_sm_probe.imu_write_ok_mask = result.write_ok_mask;
  g_ps_hw6_owner_sm_probe.imu_verify_ok_mask = result.verify_ok_mask;
  g_ps_hw6_owner_sm_probe.imu_deep_power_down_value =
    result.deep_power_down_value;
  g_ps_hw6_owner_sm_probe.imu_deep_power_down_write_status =
    (uint32_t)result.deep_power_down_status;
  g_ps_hw6_owner_sm_probe.imu_terminal_deep_power_down_committed =
    result.terminal_deep_power_down_committed;
  g_ps_hw6_owner_sm_probe.imu_post_deep_power_down_read_omitted =
    result.post_deep_power_down_read_omitted;
  g_ps_hw6_owner_sm_probe.imu_i2c_state_after =
    (uint32_t)HAL_I2C_GetState(&hi2c3);
  g_ps_hw6_owner_sm_probe.imu_i2c_error_after =
    HAL_I2C_GetError(&hi2c3);
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
  if (g_ps_hw6_owner_sm_probe.usb_stage_rescan_pending == 0UL)
  {
    return HAL_OK;
  }

  g_ps_hw6_owner_sm_probe.usb_stage_rescan_status = (uint32_t)HAL_OK;
  g_ps_hw6_owner_sm_probe.usb_stage_rescan_package_scan_status =
    (uint32_t)PS_STATUS_UNSUPPORTED;
  g_ps_hw6_owner_sm_probe.usb_stage_rescan_pending = 0UL;
  return HAL_OK;
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

HAL_StatusTypeDef PS_HW6_OwnerStateMachines_ReclaimUsbExport(void)
{
  HAL_StatusTypeDef status;
  ps_status_t storage_status;
  UINT usb_status;

  g_ps_hw6_owner_sm_probe.usb_reclaim_request_count++;
  g_ps_hw6_owner_sm_probe.usb_reclaim_start_tick = (uint32_t)tx_time_get();
  g_ps_hw6_owner_sm_probe.usb_reclaim_dirty_seen =
    g_ps_storage_msc_bridge_probe.dirty;
  g_ps_hw6_owner_sm_probe.usb_stage_rescan_dirty_seen =
    g_ps_hw6_owner_sm_probe.usb_reclaim_dirty_seen;
  g_ps_hw6_owner_sm_probe.usb_stage_rescan_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.usb_stage_rescan_package_scan_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
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
    if (g_ps_hw6_owner_sm_probe.usb_reclaim_dirty_seen != 0UL)
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
    "AT&D4\r\n"
  };
  HAL_StatusTypeDef status = HAL_OK;
  uint32_t command_index;

  g_ps_hw6_owner_sm_probe.ble_command_required_mask =
    PS_HW6_NINA_REQUIRED_COMMAND_MASK;
  g_ps_hw6_owner_sm_probe.ble_command_skipped_mask =
    PS_HW6_NINA_UNSUPPORTED_COMMAND_MASK;

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
    status = PS_HW6_SM_NinaCommand(command_index,
                                   commands[command_index]);
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
  HAL_GPIO_WritePin(PS_HW6_NINA_DSR_HOST_CONTROL_PORT,
                    PS_HW6_NINA_DSR_HOST_CONTROL_PIN, GPIO_PIN_SET);
  tx_thread_sleep(PS_HW6_NINA_STOP_SETTLE_TICKS);
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
        (uint32_t)GPIO_PIN_SET) ||
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
  ps_dev_tmag3001_suspend_result_t result;
  ps_status_t driver_status;
  HAL_StatusTypeDef status;
  uint32_t i2c_state_after = 0UL;
  uint32_t i2c_error_after = 0UL;

  driver_status = ps_dev_tmag3001_suspend(&ps_joystick_device, &result);
  status = PS_HW6_SM_StatusToHal(driver_status);

  if (cycle_index < PS_HW6_OWNER_SM_CYCLE_COUNT)
  {
    g_ps_hw6_owner_sm_probe.joystick_cycle_sleep_status[cycle_index] =
      (uint32_t)result.sleep_status;
  }
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

  driver_status = ps_dev_at25sl128a_release_from_deep_power_down(
    &ps_flash_device,
    &command_result);
  status = PS_HW6_SM_StatusToHal(driver_status);
  if (cycle_index < PS_HW6_OWNER_SM_CYCLE_COUNT)
  {
    g_ps_hw6_owner_sm_probe.flash_cycle_release_status[cycle_index] =
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
  if (status == HAL_OK)
  {
    (void)PS_HW6_SM_Transition(PS_HW6_SM_FLASH,
                              FLASH_EV_REQUEST_DEEP_POWER_DOWN, status);
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

  HAL_GPIO_WritePin(PS_HW6_NINA_DSR_HOST_CONTROL_PORT,
                    PS_HW6_NINA_DSR_HOST_CONTROL_PIN, GPIO_PIN_SET);
  tx_thread_sleep(PS_HW6_NINA_STOP_SETTLE_TICKS);
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
  g_ps_hw6_storage_usb_export_request = 0UL;
  g_ps_hw6_storage_usb_reclaim_request = 0UL;
  g_ps_hw6_joystick_sample_request = 0UL;
  g_ps_hw6_joystick_live_request = 0UL;
  g_ps_hw6_joystick_cardinal_request = 0UL;
  g_ps_hw6_joystick_calibration_capture_request = 0UL;
  g_ps_hw6_joystick_calibration_capture_page = PS_UI_ROUTER_CAL_NONE;
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
  g_ps_hw6_owner_sm_probe.stop2_expected_wake_pin = BTN_START_Pin;
  g_ps_hw6_owner_sm_probe.stop2_wake_start_idr = 0UL;
  g_ps_hw6_owner_sm_probe.stop2_wake_end_idr = 0UL;
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
  g_ps_hw6_owner_sm_probe.usb_stage_rescan_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.usb_stage_rescan_package_scan_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.ble_uart_deinit_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
}

void PS_HW6_OwnerStateMachines_SetPowerQuiesceCallback(
  PS_HW6_PowerQuiesceBarrierCallback callback)
{
  ps_power_quiesce_barrier_callback = callback;
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
  UINT clock_restore_status = TX_NOT_DONE;

  g_ps_hw6_owner_sm_probe.stop2_request_count++;
  g_ps_hw6_owner_sm_probe.stop2_start_tick = (uint32_t)tx_time_get();
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
  g_ps_hw6_owner_sm_probe.stop2_expected_wake_pin = BTN_START_Pin;
  g_ps_hw6_owner_sm_probe.stop2_wake_start_idr = 0UL;
  g_ps_hw6_owner_sm_probe.stop2_wake_end_idr = 0UL;
  PS_HW6_TraceSleep(PS_HW6_TRACE_SLEEP_STAGE_PREP_START,
                    (uint32_t)PS_HW6_POWER_QUIESCE_REASON_SLEEP_PREP,
                    g_ps_hw6_owner_sm_probe.current_state[PS_HW6_SM_POWER],
                    (uint32_t)HAL_OK);
  status = PS_HW6_SM_Transition(PS_HW6_SM_POWER,
                                PWR_EV_SLEEP_REQUEST,
                                HAL_OK);
  if (status == HAL_OK)
  {
    quiesce_status = PS_HW6_RequestPowerQuiesce(
      (uint32_t)PS_HW6_POWER_QUIESCE_REASON_SLEEP_PREP);
    g_ps_hw6_owner_sm_probe.stop2_quiesce_status =
      (uint32_t)quiesce_status;
    if (quiesce_status == HAL_OK)
    {
      g_ps_hw6_owner_sm_probe.stop2_wake_start_idr =
        BTN_START_GPIO_Port->IDR;
      __HAL_GPIO_EXTI_CLEAR_IT(BTN_START_Pin);
      HAL_NVIC_ClearPendingIRQ(BTN_START_EXTI_IRQn);

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
        HAL_SuspendTick();
        __DSB();
        HAL_PWREx_EnterSTOP2Mode(PWR_STOPENTRY_WFI);
        __ISB();
        clock_restore_status = PS_HW6_ClockPolicy_RestoreBase();
        HAL_ResumeTick();

        g_ps_hw6_owner_sm_probe.stop2_wake_tick = (uint32_t)tx_time_get();
        g_ps_hw6_owner_sm_probe.stop2_wake_end_idr = BTN_START_GPIO_Port->IDR;
        g_ps_hw6_owner_sm_probe.stop2_clock_restore_status =
          (uint32_t)clock_restore_status;
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
      state = g_ps_hw6_owner_sm_probe.current_state[PS_HW6_SM_JOYSTICK];
      if ((state == (uint32_t)JOY_OFF) ||
          (state == (uint32_t)JOY_SUSPENDED))
      {
        status = HAL_OK;
      }
      else
      {
        status = PS_HW6_SM_QuiesceJoystick(
          PS_HW6_POWER_QUIESCE_CYCLE_INDEX);
      }
      break;

    case PS_HW6_RTOS_OWNER_DISPLAY:
      state = g_ps_hw6_owner_sm_probe.current_state[PS_HW6_SM_DISPLAY];
      status = ((state == (uint32_t)DISP_OFF) ||
                (state == (uint32_t)DISP_STATIC_HOLD)) ?
        HAL_OK : PS_HW6_SM_QuiesceDisplay();
      break;

    case PS_HW6_RTOS_OWNER_SENSOR:
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
      break;

    case PS_HW6_RTOS_OWNER_STORAGE:
      if ((g_ps_hw6_owner_sm_probe.current_state[PS_HW6_SM_STORAGE] ==
           (uint32_t)STORAGE_OFFLINE) ||
          (g_ps_hw6_owner_sm_probe.current_state[PS_HW6_SM_FLASH] ==
           (uint32_t)FLASH_OFF) ||
          (g_ps_hw6_owner_sm_probe.current_state[PS_HW6_SM_FLASH] ==
           (uint32_t)FLASH_DEEP_POWER_DOWN))
      {
        status = HAL_OK;
      }
      else
      {
        status = PS_HW6_SM_QuiesceStorage(
          PS_HW6_POWER_QUIESCE_CYCLE_INDEX);
      }
      break;

    case PS_HW6_RTOS_OWNER_COMM:
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
      if (state == (uint32_t)IMU_OFF)
      {
        status = HAL_OK;
      }
      else if (state == (uint32_t)IMU_SUSPENDED)
      {
        action_required = 1UL;
        status = PS_HW6_SM_ResumeImu(PS_HW6_POWER_QUIESCE_CYCLE_INDEX);
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
          (state == (uint32_t)BLE_IDLE))
      {
        status = HAL_OK;
      }
      else if (state == (uint32_t)BLE_SUSPENDED)
      {
        action_required = 1UL;
        status = PS_HW6_SM_ResumeBle(PS_HW6_POWER_QUIESCE_CYCLE_INDEX);
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
