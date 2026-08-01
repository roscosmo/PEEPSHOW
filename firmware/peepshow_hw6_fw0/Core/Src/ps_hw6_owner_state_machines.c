#include "ps_hw6_owner_state_machines.h"

#include <stddef.h>
#include <string.h>

#include "main.h"
#include "ps_audio_events.h"
#include "ps_audio_state.h"
#include "ps_comm_events.h"
#include "ps_comm_state.h"
#include "ps_display_events.h"
#include "ps_display_state.h"
#include "ps_dev_lis2dux12.h"
#include "ps_hw_i2c3.h"
#include "ps_hw6_owner_services.h"
#include "ps_hw6_rtos_probe.h"
#include "ps_input_events.h"
#include "ps_input_state.h"
#include "ps_power_events.h"
#include "ps_power_state.h"
#include "ps_sensor_events.h"
#include "ps_sensor_state.h"
#include "ps_storage_events.h"
#include "ps_storage_state.h"
#include "tx_api.h"

#define PS_HW6_SM_PHASE_INIT              (0x6800UL)
#define PS_HW6_SM_PHASE_RUNNING           (0x6810UL)
#define PS_HW6_SM_PHASE_COMPLETE          (0x68FFUL)
#define PS_HW6_SM_REQUIRED_OWNER_MASK     (0x7FUL)
#define PS_HW6_SM_I2C_LEASE_TICKS         (20UL)
#define PS_HW6_SM_I2C_TIMEOUT_MS          (50U)
#define PS_HW6_SM_OSPI_TIMEOUT_MS         (100U)
#define PS_HW6_SM_ALL_STATE_MASK          \
  ((1UL << PS_HW6_OWNER_SM_COUNT) - 1UL)

#define PS_HW6_TMAG_ADDRESS               (0x34U)
#define PS_HW6_TMAG_REG_DEVICE_CONFIG2    (0x01U)
#define PS_HW6_TMAG_REG_SENSOR_CONFIG1    (0x02U)
#define PS_HW6_TMAG_REG_DEVICE_ID         (0x0DU)
#define PS_HW6_TMAG_REG_MANUFACTURER_LSB  (0x0EU)
#define PS_HW6_TMAG_REG_MANUFACTURER_MSB  (0x0FU)
#define PS_HW6_TMAG_MAG_CHANNEL_MASK      (0xF0U)
#define PS_HW6_TMAG_LOW_NOISE_MASK        (0x10U)
#define PS_HW6_TMAG_OPERATING_MODE_MASK   (0x03U)
#define PS_HW6_TMAG_OPERATING_MODE_STANDBY (0x00U)
#define PS_HW6_TMAG_OPERATING_MODE_SLEEP  (0x01U)
#define PS_HW6_TMAG_OPERATING_MODE_CONTINUOUS (0x02U)
#define PS_HW6_TMAG_ACTIVE_CHANNELS       (0x70U)
#define PS_HW6_TMAG_WAKE_SETTLE_TICKS     (1UL)
#define PS_HW6_TMAG_WRITE_REQUIRED_MASK   (0x07UL)
#define PS_HW6_TMAG_VERIFY_REQUIRED_MASK  (0x03UL)

#define PS_HW6_IMU_ADDRESS                (0x18U)

#define PS_HW6_FLASH_CMD_JEDEC_ID          (0x9FU)
#define PS_HW6_FLASH_CMD_RELEASE_POWER_DOWN (0xABU)
#define PS_HW6_FLASH_CMD_DEEP_POWER_DOWN   (0xB9U)
#define PS_HW6_FLASH_WAKE_SETTLE_TICKS     (1UL)
#define PS_HW6_FLASH_ID0                   (0x1FU)
#define PS_HW6_FLASH_ID1                   (0x42U)
#define PS_HW6_FLASH_ID2                   (0x18U)

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

extern I2C_HandleTypeDef hi2c3;
extern OSPI_HandleTypeDef hospi1;
extern PCD_HandleTypeDef hpcd_USB_OTG_FS;
extern SAI_HandleTypeDef hsai_BlockA1;
extern UART_HandleTypeDef hlpuart1;

volatile PS_HW6_OwnerStateMachineProbe g_ps_hw6_owner_sm_probe;
volatile uint32_t g_ps_hw6_owner_sm_start_request;

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
  {PWR_FAULT, PWR_EV_RECOVER_OK, PWR_RAIL_VALIDATE}
};

static const PS_HW6_StateTransition ps_pmic_transitions[] =
{
  {PMIC_OFFLINE, PMIC_EV_PROBE_REQUEST, PMIC_PROBE},
  {PMIC_PROBE, PMIC_EV_PROBE_OK, PMIC_MONITOR},
  {PMIC_PROBE, PMIC_EV_PROBE_FAIL, PMIC_ERROR},
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
  {JOY_PROBE, JOY_EV_PROBE_OK, JOY_CONFIG},
  {JOY_CONFIG, JOY_EV_CONFIG_OK, JOY_SUSPENDED},
  {JOY_CONFIG, JOY_EV_SLOW_POLL_REQUEST, JOY_SLOW_POLL},
  {JOY_SLOW_POLL, JOY_EV_QUIESCE, JOY_SUSPENDED},
  {JOY_PROBE, JOY_EV_I2C_ERROR, JOY_ERROR},
  {JOY_CONFIG, JOY_EV_I2C_ERROR, JOY_ERROR},
  {JOY_SLOW_POLL, JOY_EV_I2C_ERROR, JOY_ERROR},
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
static uint8_t ps_nina_rx_buffer[PS_HW6_NINA_RX_BUFFER_SIZE];

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
      return HAL_OK;
    }
  }

  g_ps_hw6_owner_sm_probe.rejected_transition_count[state_machine_id]++;
  g_ps_hw6_owner_sm_probe.last_event[state_machine_id] = event;
  g_ps_hw6_owner_sm_probe.last_error[state_machine_id] = (uint32_t)HAL_ERROR;
  PS_HW6_SM_RecordTrace(state_machine_id, current_state, event,
                        current_state, (uint32_t)HAL_ERROR);
  return HAL_ERROR;
}

static uint32_t PS_HW6_SM_I2CResultIsOk(PS_HW_I2C3_Result result)
{
  return ((result.acquire_status == TX_SUCCESS) &&
          (result.transfer_status == HAL_OK) &&
          (result.release_status == TX_SUCCESS)) ? 1UL : 0UL;
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

static HAL_StatusTypeDef PS_HW6_SM_ReadI2C(uint8_t address,
                                           uint8_t reg,
                                           uint8_t *value)
{
  PS_HW_I2C3_Result result = PS_HW_I2C3_ReadRegister(
    address, reg, value, PS_HW6_SM_I2C_LEASE_TICKS,
    PS_HW6_SM_I2C_TIMEOUT_MS);

  return (PS_HW6_SM_I2CResultIsOk(result) != 0UL) ? HAL_OK : HAL_ERROR;
}

static HAL_StatusTypeDef PS_HW6_SM_WriteI2C(uint8_t address,
                                            uint8_t reg,
                                            uint8_t value)
{
  PS_HW_I2C3_Result result = PS_HW_I2C3_WriteRegister(
    address, reg, value, PS_HW6_SM_I2C_LEASE_TICKS,
    PS_HW6_SM_I2C_TIMEOUT_MS);

  return (PS_HW6_SM_I2CResultIsOk(result) != 0UL) ? HAL_OK : HAL_ERROR;
}

static HAL_StatusTypeDef PS_HW6_SM_StabilizePower(void)
{
  HAL_StatusTypeDef status;

  (void)PS_HW6_SM_Transition(PS_HW6_SM_POWER, PWR_EV_BOOT, HAL_OK);
  (void)PS_HW6_SM_Transition(PS_HW6_SM_PMIC,
                            PMIC_EV_PROBE_REQUEST, HAL_OK);
  status = PS_HW6_PowerOwner_RunSnapshot();
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
  (void)PS_HW6_SM_Transition(PS_HW6_SM_AUDIO,
                            AUDIO_EV_INIT_REQ, HAL_OK);
  (void)PS_HW6_SM_Transition(PS_HW6_SM_AUDIO,
                            AUDIO_EV_INIT_OK, HAL_OK);
  return PS_HW6_SM_RunAudioTone();
}

static HAL_StatusTypeDef PS_HW6_SM_StabilizeJoystick(void)
{
  HAL_StatusTypeDef status = HAL_OK;
  uint8_t value = 0U;
  uint8_t sensor_config1;
  uint8_t device_config2;
  uint8_t sleep_device_config2;

  (void)PS_HW6_SM_Transition(PS_HW6_SM_JOYSTICK,
                            JOY_EV_ENABLE_REQUEST, HAL_OK);

  status = PS_HW6_SM_ReadI2C(PS_HW6_TMAG_ADDRESS,
                             PS_HW6_TMAG_REG_DEVICE_ID, &value);
  g_ps_hw6_owner_sm_probe.joystick_ready_status = (uint32_t)status;
  g_ps_hw6_owner_sm_probe.joystick_device_id = value;
  if (status == HAL_OK)
  {
    status = PS_HW6_SM_ReadI2C(PS_HW6_TMAG_ADDRESS,
                               PS_HW6_TMAG_REG_MANUFACTURER_LSB, &value);
    g_ps_hw6_owner_sm_probe.joystick_manufacturer_lsb = value;
  }
  if (status == HAL_OK)
  {
    status = PS_HW6_SM_ReadI2C(PS_HW6_TMAG_ADDRESS,
                               PS_HW6_TMAG_REG_MANUFACTURER_MSB, &value);
    g_ps_hw6_owner_sm_probe.joystick_manufacturer_msb = value;
  }
  g_ps_hw6_owner_sm_probe.joystick_identity_status = (uint32_t)status;
  g_ps_hw6_owner_sm_probe.joystick_identity_match =
    ((g_ps_hw6_owner_sm_probe.joystick_manufacturer_lsb == 0x49U) &&
     (g_ps_hw6_owner_sm_probe.joystick_manufacturer_msb == 0x54U)) ?
    1UL : 0UL;
  if ((status != HAL_OK) ||
      (g_ps_hw6_owner_sm_probe.joystick_identity_match == 0UL))
  {
    status = HAL_ERROR;
    (void)PS_HW6_SM_Transition(PS_HW6_SM_JOYSTICK,
                              JOY_EV_I2C_ERROR, status);
    goto joystick_done;
  }

  (void)PS_HW6_SM_Transition(PS_HW6_SM_JOYSTICK,
                            JOY_EV_PROBE_OK, HAL_OK);
  status = PS_HW6_SM_ReadI2C(PS_HW6_TMAG_ADDRESS,
                             PS_HW6_TMAG_REG_SENSOR_CONFIG1,
                             &sensor_config1);
  if (status == HAL_OK)
  {
    g_ps_hw6_owner_sm_probe.joystick_sensor_config1_before = sensor_config1;
    sensor_config1 &= (uint8_t)~PS_HW6_TMAG_MAG_CHANNEL_MASK;
    status = PS_HW6_SM_WriteI2C(PS_HW6_TMAG_ADDRESS,
                                PS_HW6_TMAG_REG_SENSOR_CONFIG1,
                                sensor_config1);
    if (status == HAL_OK)
    {
      g_ps_hw6_owner_sm_probe.joystick_write_ok_mask |= 1UL << 0U;
    }
  }
  if (status == HAL_OK)
  {
    status = PS_HW6_SM_ReadI2C(PS_HW6_TMAG_ADDRESS,
                               PS_HW6_TMAG_REG_SENSOR_CONFIG1, &value);
    g_ps_hw6_owner_sm_probe.joystick_sensor_config1_verify_status =
      (uint32_t)status;
    if ((status == HAL_OK) && (value == sensor_config1))
    {
      g_ps_hw6_owner_sm_probe.joystick_sensor_config1_after = value;
      g_ps_hw6_owner_sm_probe.joystick_verify_ok_mask |= 1UL << 0U;
    }
    else
    {
      status = HAL_ERROR;
    }
  }
  if (status == HAL_OK)
  {
    status = PS_HW6_SM_ReadI2C(PS_HW6_TMAG_ADDRESS,
                               PS_HW6_TMAG_REG_DEVICE_CONFIG2,
                               &device_config2);
  }
  if (status == HAL_OK)
  {
    g_ps_hw6_owner_sm_probe.joystick_device_config2_before = device_config2;
    device_config2 =
      (uint8_t)((device_config2 &
                 (uint8_t)~(PS_HW6_TMAG_LOW_NOISE_MASK |
                            PS_HW6_TMAG_OPERATING_MODE_MASK)) |
                PS_HW6_TMAG_OPERATING_MODE_STANDBY);
    status = PS_HW6_SM_WriteI2C(PS_HW6_TMAG_ADDRESS,
                                PS_HW6_TMAG_REG_DEVICE_CONFIG2,
                                device_config2);
    if (status == HAL_OK)
    {
      g_ps_hw6_owner_sm_probe.joystick_write_ok_mask |= 1UL << 1U;
    }
  }
  if (status == HAL_OK)
  {
    status = PS_HW6_SM_ReadI2C(PS_HW6_TMAG_ADDRESS,
                               PS_HW6_TMAG_REG_DEVICE_CONFIG2, &value);
    g_ps_hw6_owner_sm_probe.joystick_device_config2_verify_status =
      (uint32_t)status;
    if ((status == HAL_OK) && (value == device_config2))
    {
      g_ps_hw6_owner_sm_probe.joystick_device_config2_after = value;
      g_ps_hw6_owner_sm_probe.joystick_verify_ok_mask |= 1UL << 1U;
    }
    else
    {
      status = HAL_ERROR;
    }
  }
  if (status == HAL_OK)
  {
    sleep_device_config2 =
      (uint8_t)(device_config2 | PS_HW6_TMAG_OPERATING_MODE_SLEEP);
    g_ps_hw6_owner_sm_probe.joystick_device_config2_sleep =
      sleep_device_config2;
    g_ps_hw6_owner_sm_probe.joystick_post_sleep_read_omitted = 1UL;
    status = PS_HW6_SM_WriteI2C(PS_HW6_TMAG_ADDRESS,
                                PS_HW6_TMAG_REG_DEVICE_CONFIG2,
                                sleep_device_config2);
    g_ps_hw6_owner_sm_probe.joystick_sleep_write_status =
      (uint32_t)status;
    if (status == HAL_OK)
    {
      g_ps_hw6_owner_sm_probe.joystick_write_ok_mask |= 1UL << 2U;
      g_ps_hw6_owner_sm_probe.joystick_terminal_sleep_committed = 1UL;
    }
  }

  if ((status == HAL_OK) &&
      (g_ps_hw6_owner_sm_probe.joystick_write_ok_mask ==
       PS_HW6_TMAG_WRITE_REQUIRED_MASK) &&
      (g_ps_hw6_owner_sm_probe.joystick_verify_ok_mask ==
       PS_HW6_TMAG_VERIFY_REQUIRED_MASK) &&
      (g_ps_hw6_owner_sm_probe.joystick_terminal_sleep_committed != 0UL))
  {
    (void)PS_HW6_SM_Transition(PS_HW6_SM_JOYSTICK,
                              JOY_EV_CONFIG_OK, status);
  }
  else
  {
    status = HAL_ERROR;
    (void)PS_HW6_SM_Transition(PS_HW6_SM_JOYSTICK,
                              JOY_EV_I2C_ERROR, status);
  }

joystick_done:
  g_ps_hw6_owner_sm_probe.joystick_i2c_state_after =
    (uint32_t)HAL_I2C_GetState(&hi2c3);
  g_ps_hw6_owner_sm_probe.joystick_i2c_error_after =
    HAL_I2C_GetError(&hi2c3);
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

static void PS_HW6_SM_PrepareOspiCommand(OSPI_RegularCmdTypeDef *command,
                                         uint8_t instruction,
                                         uint32_t data_mode,
                                         uint32_t length)
{
  (void)memset(command, 0, sizeof(*command));
  command->OperationType = HAL_OSPI_OPTYPE_COMMON_CFG;
  command->FlashId = HAL_OSPI_FLASH_ID_1;
  command->Instruction = instruction;
  command->InstructionMode = HAL_OSPI_INSTRUCTION_1_LINE;
  command->InstructionSize = HAL_OSPI_INSTRUCTION_8_BITS;
  command->InstructionDtrMode = HAL_OSPI_INSTRUCTION_DTR_DISABLE;
  command->AddressMode = HAL_OSPI_ADDRESS_NONE;
  command->AddressDtrMode = HAL_OSPI_ADDRESS_DTR_DISABLE;
  command->AlternateBytesMode = HAL_OSPI_ALTERNATE_BYTES_NONE;
  command->AlternateBytesDtrMode = HAL_OSPI_ALTERNATE_BYTES_DTR_DISABLE;
  command->DataMode = data_mode;
  command->NbData = length;
  command->DataDtrMode = HAL_OSPI_DATA_DTR_DISABLE;
  command->DummyCycles = 0U;
  command->DQSMode = HAL_OSPI_DQS_DISABLE;
  command->SIOOMode = HAL_OSPI_SIOO_INST_EVERY_CMD;
}

static HAL_StatusTypeDef PS_HW6_SM_ReadFlash(uint8_t instruction,
                                             uint8_t *data,
                                             uint32_t length)
{
  OSPI_RegularCmdTypeDef command;
  HAL_StatusTypeDef status;

  if ((data == NULL) || (length == 0U))
  {
    return HAL_ERROR;
  }

  PS_HW6_SM_PrepareOspiCommand(&command, instruction,
                               HAL_OSPI_DATA_1_LINE, length);
  status = HAL_OSPI_Command(&hospi1, &command, PS_HW6_SM_OSPI_TIMEOUT_MS);
  if (status == HAL_OK)
  {
    status = HAL_OSPI_Receive(&hospi1, data, PS_HW6_SM_OSPI_TIMEOUT_MS);
  }
  return status;
}

static HAL_StatusTypeDef PS_HW6_SM_SendFlashCommand(uint8_t instruction)
{
  OSPI_RegularCmdTypeDef command;

  PS_HW6_SM_PrepareOspiCommand(&command, instruction,
                               HAL_OSPI_DATA_NONE, 0U);
  return HAL_OSPI_Command(&hospi1, &command, PS_HW6_SM_OSPI_TIMEOUT_MS);
}

static HAL_StatusTypeDef PS_HW6_SM_ParkUsb(void)
{
  HAL_StatusTypeDef status = HAL_OK;
  uint32_t pwr_clock_was_disabled;

  g_ps_hw6_owner_sm_probe.usb_vbus_present =
    (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_9) == GPIO_PIN_SET) ? 1UL : 0UL;
  g_ps_hw6_owner_sm_probe.usb_pcd_state_before =
    (uint32_t)HAL_PCD_GetState(&hpcd_USB_OTG_FS);
  g_ps_hw6_owner_sm_probe.usb_clock_enabled_before =
    (__HAL_RCC_USB_IS_CLK_ENABLED() != 0U) ? 1UL : 0UL;
  g_ps_hw6_owner_sm_probe.usb_vddusb_enabled_before =
    (READ_BIT(PWR->SVMCR, PWR_SVMCR_USV) != 0U) ? 1UL : 0UL;

  if (g_ps_hw6_owner_sm_probe.usb_vbus_present != 0UL)
  {
    status = HAL_BUSY;
  }
  else
  {
    g_ps_hw6_owner_sm_probe.usb_deinit_attempted = 1UL;
    status = HAL_PCD_DeInit(&hpcd_USB_OTG_FS);
    g_ps_hw6_owner_sm_probe.usb_deinit_status = (uint32_t)status;
    if (status == HAL_OK)
    {
      pwr_clock_was_disabled =
        (__HAL_RCC_PWR_IS_CLK_DISABLED() != 0U) ? 1UL : 0UL;
      if (pwr_clock_was_disabled != 0UL)
      {
        __HAL_RCC_PWR_CLK_ENABLE();
      }
      HAL_PWREx_DisableVddUSB();
      if (pwr_clock_was_disabled != 0UL)
      {
        __HAL_RCC_PWR_CLK_DISABLE();
      }
    }
  }

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

  return (g_ps_hw6_owner_sm_probe.usb_parked != 0UL) ? HAL_OK : status;
}

static HAL_StatusTypeDef PS_HW6_SM_StabilizeStorage(void)
{
  HAL_StatusTypeDef status;
  uint8_t id[3] = {0U, 0U, 0U};

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

  status = PS_HW6_SM_ReadFlash(PS_HW6_FLASH_CMD_JEDEC_ID,
                               id, sizeof(id));
  g_ps_hw6_owner_sm_probe.flash_jedec_status = (uint32_t)status;
  g_ps_hw6_owner_sm_probe.flash_jedec_id[0] = id[0];
  g_ps_hw6_owner_sm_probe.flash_jedec_id[1] = id[1];
  g_ps_hw6_owner_sm_probe.flash_jedec_id[2] = id[2];
  g_ps_hw6_owner_sm_probe.flash_identity_match =
    ((status == HAL_OK) &&
     (id[0] == PS_HW6_FLASH_ID0) &&
     (id[1] == PS_HW6_FLASH_ID1) &&
     (id[2] == PS_HW6_FLASH_ID2)) ? 1UL : 0UL;

  if (g_ps_hw6_owner_sm_probe.flash_identity_match == 0UL)
  {
    status = HAL_ERROR;
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
  status = PS_HW6_SM_SendFlashCommand(PS_HW6_FLASH_CMD_DEEP_POWER_DOWN);
  g_ps_hw6_owner_sm_probe.flash_deep_power_down_status = (uint32_t)status;
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
  return status;
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
  return PS_HW6_SM_RunAudioTone();
}

static HAL_StatusTypeDef PS_HW6_SM_QuiesceAudio(void)
{
  if ((g_ps_hw6_owner_sm_probe.current_state[PS_HW6_SM_AUDIO] ==
       (uint32_t)AUDIO_IDLE) &&
      (g_ps_hw6_owner_sm_probe.current_state[PS_HW6_SM_SPEAKER] ==
       (uint32_t)SPK_OFF) &&
      (HAL_GPIO_ReadPin(SD_MODE_GPIO_Port, SD_MODE_Pin) == GPIO_PIN_RESET) &&
      (HAL_SAI_GetState(&hsai_BlockA1) == HAL_SAI_STATE_READY) &&
      (HAL_SAI_GetError(&hsai_BlockA1) == HAL_SAI_ERROR_NONE))
  {
    return HAL_OK;
  }
  return HAL_ERROR;
}

static HAL_StatusTypeDef PS_HW6_SM_ResumeJoystick(uint32_t cycle_index)
{
  HAL_StatusTypeDef status;
  uint8_t value = 0U;
  uint8_t manufacturer_lsb = 0U;
  uint8_t manufacturer_msb = 0U;
  uint8_t sensor_config1 = 0U;
  uint8_t device_config2 = 0U;

  if (PS_HW6_SM_Transition(PS_HW6_SM_JOYSTICK,
                           JOY_EV_RESUME, HAL_OK) != HAL_OK)
  {
    return HAL_ERROR;
  }

  status = PS_HW6_SM_ReadI2C(PS_HW6_TMAG_ADDRESS,
                             PS_HW6_TMAG_REG_DEVICE_ID, &value);
  g_ps_hw6_owner_sm_probe.joystick_cycle_wake_probe_status[cycle_index] =
    (uint32_t)status;
  tx_thread_sleep(PS_HW6_TMAG_WAKE_SETTLE_TICKS);

  status = PS_HW6_SM_ReadI2C(PS_HW6_TMAG_ADDRESS,
                             PS_HW6_TMAG_REG_DEVICE_ID, &value);
  g_ps_hw6_owner_sm_probe.joystick_cycle_wake_retry_status[cycle_index] =
    (uint32_t)status;
  if (status == HAL_OK)
  {
    status = PS_HW6_SM_ReadI2C(PS_HW6_TMAG_ADDRESS,
                               PS_HW6_TMAG_REG_MANUFACTURER_LSB,
                               &manufacturer_lsb);
  }
  if (status == HAL_OK)
  {
    status = PS_HW6_SM_ReadI2C(PS_HW6_TMAG_ADDRESS,
                               PS_HW6_TMAG_REG_MANUFACTURER_MSB,
                               &manufacturer_msb);
  }
  if ((status != HAL_OK) || (manufacturer_lsb != 0x49U) ||
      (manufacturer_msb != 0x54U))
  {
    status = HAL_ERROR;
    (void)PS_HW6_SM_Transition(PS_HW6_SM_JOYSTICK,
                              JOY_EV_I2C_ERROR, status);
    goto joystick_resume_done;
  }

  (void)PS_HW6_SM_Transition(PS_HW6_SM_JOYSTICK,
                            JOY_EV_PROBE_OK, HAL_OK);
  status = PS_HW6_SM_ReadI2C(PS_HW6_TMAG_ADDRESS,
                             PS_HW6_TMAG_REG_SENSOR_CONFIG1,
                             &sensor_config1);
  if (status == HAL_OK)
  {
    sensor_config1 =
      (uint8_t)((sensor_config1 & (uint8_t)~PS_HW6_TMAG_MAG_CHANNEL_MASK) |
                PS_HW6_TMAG_ACTIVE_CHANNELS);
    status = PS_HW6_SM_WriteI2C(PS_HW6_TMAG_ADDRESS,
                                PS_HW6_TMAG_REG_SENSOR_CONFIG1,
                                sensor_config1);
  }
  if (status == HAL_OK)
  {
    status = PS_HW6_SM_ReadI2C(PS_HW6_TMAG_ADDRESS,
                               PS_HW6_TMAG_REG_DEVICE_CONFIG2,
                               &device_config2);
  }
  if (status == HAL_OK)
  {
    device_config2 =
      (uint8_t)((device_config2 &
                 (uint8_t)~(PS_HW6_TMAG_LOW_NOISE_MASK |
                            PS_HW6_TMAG_OPERATING_MODE_MASK)) |
                PS_HW6_TMAG_OPERATING_MODE_CONTINUOUS);
    status = PS_HW6_SM_WriteI2C(PS_HW6_TMAG_ADDRESS,
                                PS_HW6_TMAG_REG_DEVICE_CONFIG2,
                                device_config2);
  }
  if (status == HAL_OK)
  {
    status = PS_HW6_SM_ReadI2C(PS_HW6_TMAG_ADDRESS,
                               PS_HW6_TMAG_REG_SENSOR_CONFIG1, &value);
    if ((status != HAL_OK) || (value != sensor_config1))
    {
      status = HAL_ERROR;
    }
  }
  if (status == HAL_OK)
  {
    g_ps_hw6_owner_sm_probe
      .joystick_cycle_active_sensor_config1[cycle_index] = value;
    status = PS_HW6_SM_ReadI2C(PS_HW6_TMAG_ADDRESS,
                               PS_HW6_TMAG_REG_DEVICE_CONFIG2, &value);
    if ((status != HAL_OK) || (value != device_config2))
    {
      status = HAL_ERROR;
    }
  }
  if (status == HAL_OK)
  {
    g_ps_hw6_owner_sm_probe
      .joystick_cycle_active_device_config2[cycle_index] = value;
    (void)PS_HW6_SM_Transition(PS_HW6_SM_JOYSTICK,
                              JOY_EV_SLOW_POLL_REQUEST, status);
  }
  else
  {
    (void)PS_HW6_SM_Transition(PS_HW6_SM_JOYSTICK,
                              JOY_EV_I2C_ERROR, status);
  }

joystick_resume_done:
  g_ps_hw6_owner_sm_probe.joystick_cycle_active_status[cycle_index] =
    (uint32_t)status;
  return status;
}

static HAL_StatusTypeDef PS_HW6_SM_QuiesceJoystick(uint32_t cycle_index)
{
  HAL_StatusTypeDef status;
  uint8_t value = 0U;
  uint8_t sensor_config1 = 0U;
  uint8_t device_config2 = 0U;

  status = PS_HW6_SM_ReadI2C(PS_HW6_TMAG_ADDRESS,
                             PS_HW6_TMAG_REG_SENSOR_CONFIG1,
                             &sensor_config1);
  if (status == HAL_OK)
  {
    sensor_config1 &= (uint8_t)~PS_HW6_TMAG_MAG_CHANNEL_MASK;
    status = PS_HW6_SM_WriteI2C(PS_HW6_TMAG_ADDRESS,
                                PS_HW6_TMAG_REG_SENSOR_CONFIG1,
                                sensor_config1);
  }
  if (status == HAL_OK)
  {
    status = PS_HW6_SM_ReadI2C(PS_HW6_TMAG_ADDRESS,
                               PS_HW6_TMAG_REG_SENSOR_CONFIG1, &value);
    if ((status != HAL_OK) || (value != sensor_config1))
    {
      status = HAL_ERROR;
    }
  }
  if (status == HAL_OK)
  {
    status = PS_HW6_SM_ReadI2C(PS_HW6_TMAG_ADDRESS,
                               PS_HW6_TMAG_REG_DEVICE_CONFIG2,
                               &device_config2);
  }
  if (status == HAL_OK)
  {
    device_config2 =
      (uint8_t)(device_config2 &
                (uint8_t)~(PS_HW6_TMAG_LOW_NOISE_MASK |
                           PS_HW6_TMAG_OPERATING_MODE_MASK));
    status = PS_HW6_SM_WriteI2C(PS_HW6_TMAG_ADDRESS,
                                PS_HW6_TMAG_REG_DEVICE_CONFIG2,
                                device_config2);
  }
  if (status == HAL_OK)
  {
    status = PS_HW6_SM_ReadI2C(PS_HW6_TMAG_ADDRESS,
                               PS_HW6_TMAG_REG_DEVICE_CONFIG2, &value);
    if ((status != HAL_OK) || (value != device_config2))
    {
      status = HAL_ERROR;
    }
  }
  if (status == HAL_OK)
  {
    status = PS_HW6_SM_WriteI2C(
      PS_HW6_TMAG_ADDRESS, PS_HW6_TMAG_REG_DEVICE_CONFIG2,
      (uint8_t)(device_config2 | PS_HW6_TMAG_OPERATING_MODE_SLEEP));
  }
  g_ps_hw6_owner_sm_probe.joystick_cycle_sleep_status[cycle_index] =
    (uint32_t)status;
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
  g_ps_hw6_owner_sm_probe.imu_cycle_sleep_status[cycle_index] =
    (uint32_t)status;
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
  HAL_StatusTypeDef status;
  uint8_t id[3] = {0U, 0U, 0U};

  if ((g_ps_hw6_owner_sm_probe.current_state[PS_HW6_SM_STORAGE] !=
       (uint32_t)STORAGE_FLASH_READY) ||
      (g_ps_hw6_owner_sm_probe.current_state[PS_HW6_SM_FLASH] !=
       (uint32_t)FLASH_DEEP_POWER_DOWN))
  {
    return HAL_ERROR;
  }

  status = PS_HW6_SM_SendFlashCommand(
    PS_HW6_FLASH_CMD_RELEASE_POWER_DOWN);
  g_ps_hw6_owner_sm_probe.flash_cycle_release_status[cycle_index] =
    (uint32_t)status;
  if (status == HAL_OK)
  {
    tx_thread_sleep(PS_HW6_FLASH_WAKE_SETTLE_TICKS);
    (void)PS_HW6_SM_Transition(PS_HW6_SM_FLASH,
                              FLASH_EV_WAKE_REVALIDATE, status);
    status = PS_HW6_SM_ReadFlash(PS_HW6_FLASH_CMD_JEDEC_ID,
                                 id, sizeof(id));
  }
  g_ps_hw6_owner_sm_probe.flash_cycle_jedec_status[cycle_index] =
    (uint32_t)status;
  g_ps_hw6_owner_sm_probe.flash_cycle_identity_match[cycle_index] =
    ((status == HAL_OK) &&
     (id[0] == PS_HW6_FLASH_ID0) &&
     (id[1] == PS_HW6_FLASH_ID1) &&
     (id[2] == PS_HW6_FLASH_ID2)) ? 1UL : 0UL;
  if (g_ps_hw6_owner_sm_probe.flash_cycle_identity_match[cycle_index] != 0UL)
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
  return status;
}

static HAL_StatusTypeDef PS_HW6_SM_QuiesceStorage(uint32_t cycle_index)
{
  HAL_StatusTypeDef status;

  if ((g_ps_hw6_owner_sm_probe.current_state[PS_HW6_SM_STORAGE] !=
       (uint32_t)STORAGE_FLASH_READY) ||
      (g_ps_hw6_owner_sm_probe.current_state[PS_HW6_SM_FLASH] !=
       (uint32_t)FLASH_READY))
  {
    return HAL_ERROR;
  }

  status = PS_HW6_SM_SendFlashCommand(PS_HW6_FLASH_CMD_DEEP_POWER_DOWN);
  g_ps_hw6_owner_sm_probe
    .flash_cycle_deep_power_down_status[cycle_index] = (uint32_t)status;
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
  g_ps_hw6_owner_sm_probe.ble_cycle_uart_init_status[cycle_index] =
    (uint32_t)status;
  if (status == HAL_OK)
  {
    tx_thread_sleep(PS_HW6_NINA_WAKE_SETTLE_TICKS);
    (void)memset(ps_nina_rx_buffer, 0, sizeof(ps_nina_rx_buffer));
    (void)PS_HW6_SM_NinaReceiveUntilQuiet(
      ps_nina_rx_buffer, sizeof(ps_nina_rx_buffer),
      PS_HW6_NINA_BOOT_DRAIN_TICKS, PS_HW6_NINA_RX_QUIET_TICKS);
    status = PS_HW6_SM_NinaCommand(0U, "AT\r\n");
  }
  g_ps_hw6_owner_sm_probe.ble_cycle_wake_at_status[cycle_index] =
    (uint32_t)status;
  g_ps_hw6_owner_sm_probe.ble_cycle_wake_rx_len[cycle_index] =
    g_ps_hw6_owner_sm_probe.ble_command_rx_len[0];
  g_ps_hw6_owner_sm_probe.ble_cycle_dsr_after_resume[cycle_index] =
    (uint32_t)HAL_GPIO_ReadPin(PS_HW6_NINA_DSR_HOST_CONTROL_PORT,
                              PS_HW6_NINA_DSR_HOST_CONTROL_PIN);
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
  g_ps_hw6_owner_sm_probe.ble_cycle_suspend_uart_status[cycle_index] =
    (uint32_t)status;
  g_ps_hw6_owner_sm_probe.ble_cycle_dsr_after_quiesce[cycle_index] =
    (uint32_t)HAL_GPIO_ReadPin(PS_HW6_NINA_DSR_HOST_CONTROL_PORT,
                              PS_HW6_NINA_DSR_HOST_CONTROL_PIN);
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
  uint32_t cycle_index;
  uint32_t direction;
  uint32_t index;

  (void)memset((void *)&g_ps_hw6_owner_sm_probe, 0,
               sizeof(g_ps_hw6_owner_sm_probe));
  g_ps_hw6_owner_sm_start_request = 0UL;
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
  g_ps_hw6_owner_sm_probe.usb_deinit_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
  g_ps_hw6_owner_sm_probe.ble_uart_deinit_status =
    PS_HW6_OWNER_SM_STATUS_NOT_RUN;
}

void PS_HW6_OwnerStateMachines_BeginWorkflow(void)
{
  g_ps_hw6_owner_sm_probe.phase = PS_HW6_SM_PHASE_RUNNING;
  g_ps_hw6_owner_sm_probe.workflow_start_tick = (uint32_t)tx_time_get();
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
