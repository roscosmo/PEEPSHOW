#ifndef PS_HW6_OWNER_STATE_MACHINES_H
#define PS_HW6_OWNER_STATE_MACHINES_H

#include <stdint.h>

#include "stm32u5xx_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PS_HW6_OWNER_SM_PROBE_MAGIC          (0x48364653UL)
#define PS_HW6_OWNER_SM_PROBE_VERSION        (13UL)
#define PS_HW6_OWNER_SM_COUNT                (10U)
#define PS_HW6_OWNER_SM_TRACE_DEPTH          (128U)
#define PS_HW6_OWNER_SM_PHYSICAL_OWNER_COUNT (7U)
#define PS_HW6_OWNER_SM_CYCLE_COUNT          (2U)
#define PS_HW6_OWNER_SM_CYCLE_DIRECTION_COUNT (2U)
#define PS_HW6_OWNER_SM_IMU_REGISTER_COUNT   (11U)
#define PS_HW6_OWNER_SM_NINA_COMMAND_COUNT   (7U)
#define PS_HW6_OWNER_SM_STATUS_NOT_RUN       (0xFFFFFFFFUL)

typedef enum
{
  PS_HW6_SM_POWER = 0,
  PS_HW6_SM_PMIC,
  PS_HW6_SM_DISPLAY,
  PS_HW6_SM_AUDIO,
  PS_HW6_SM_SPEAKER,
  PS_HW6_SM_JOYSTICK,
  PS_HW6_SM_IMU,
  PS_HW6_SM_STORAGE,
  PS_HW6_SM_FLASH,
  PS_HW6_SM_BLE
} PS_HW6_OwnerStateMachineId;

typedef enum
{
  PS_HW6_OWNER_SM_CYCLE_RESUME = 0,
  PS_HW6_OWNER_SM_CYCLE_QUIESCE
} PS_HW6_OwnerStateMachineCycleDirection;

typedef struct
{
  uint32_t tick;
  uint32_t state_machine_id;
  uint32_t from_state;
  uint32_t event;
  uint32_t to_state;
  uint32_t action_status;
} PS_HW6_OwnerStateTrace;

typedef struct
{
  uint32_t magic;
  uint32_t version;
  uint32_t phase;
  uint32_t complete;
  uint32_t success;
  uint32_t required_owner_mask;
  uint32_t completed_owner_mask;
  uint32_t success_owner_mask;
  uint32_t failure_owner_mask;
  uint32_t workflow_start_tick;
  uint32_t workflow_end_tick;

  uint32_t owner_command_send_status[PS_HW6_OWNER_SM_PHYSICAL_OWNER_COUNT];
  uint32_t owner_ack_wait_status[PS_HW6_OWNER_SM_PHYSICAL_OWNER_COUNT];
  uint32_t owner_ack_flags[PS_HW6_OWNER_SM_PHYSICAL_OWNER_COUNT];
  uint32_t owner_action_start_tick[PS_HW6_OWNER_SM_PHYSICAL_OWNER_COUNT];
  uint32_t owner_action_end_tick[PS_HW6_OWNER_SM_PHYSICAL_OWNER_COUNT];
  uint32_t owner_action_status[PS_HW6_OWNER_SM_PHYSICAL_OWNER_COUNT];

  uint32_t cycle_requested_count;
  uint32_t cycle_completed_count;
  uint32_t cycle_success;
  uint32_t cycle_start_tick[PS_HW6_OWNER_SM_CYCLE_COUNT];
  uint32_t cycle_active_tick[PS_HW6_OWNER_SM_CYCLE_COUNT];
  uint32_t cycle_end_tick[PS_HW6_OWNER_SM_CYCLE_COUNT];
  uint32_t cycle_resume_success_mask[PS_HW6_OWNER_SM_CYCLE_COUNT];
  uint32_t cycle_resume_failure_mask[PS_HW6_OWNER_SM_CYCLE_COUNT];
  uint32_t cycle_quiesce_success_mask[PS_HW6_OWNER_SM_CYCLE_COUNT];
  uint32_t cycle_quiesce_failure_mask[PS_HW6_OWNER_SM_CYCLE_COUNT];
  uint32_t cycle_active_state_match_mask[PS_HW6_OWNER_SM_CYCLE_COUNT];
  uint32_t cycle_inactive_state_match_mask[PS_HW6_OWNER_SM_CYCLE_COUNT];
  uint32_t cycle_command_send_status[PS_HW6_OWNER_SM_CYCLE_COUNT]
                                    [PS_HW6_OWNER_SM_CYCLE_DIRECTION_COUNT]
                                    [PS_HW6_OWNER_SM_PHYSICAL_OWNER_COUNT];
  uint32_t cycle_ack_wait_status[PS_HW6_OWNER_SM_CYCLE_COUNT]
                                [PS_HW6_OWNER_SM_CYCLE_DIRECTION_COUNT]
                                [PS_HW6_OWNER_SM_PHYSICAL_OWNER_COUNT];
  uint32_t cycle_ack_flags[PS_HW6_OWNER_SM_CYCLE_COUNT]
                          [PS_HW6_OWNER_SM_CYCLE_DIRECTION_COUNT]
                          [PS_HW6_OWNER_SM_PHYSICAL_OWNER_COUNT];
  uint32_t cycle_action_start_tick[PS_HW6_OWNER_SM_CYCLE_COUNT]
                                  [PS_HW6_OWNER_SM_CYCLE_DIRECTION_COUNT]
                                  [PS_HW6_OWNER_SM_PHYSICAL_OWNER_COUNT];
  uint32_t cycle_action_end_tick[PS_HW6_OWNER_SM_CYCLE_COUNT]
                                [PS_HW6_OWNER_SM_CYCLE_DIRECTION_COUNT]
                                [PS_HW6_OWNER_SM_PHYSICAL_OWNER_COUNT];
  uint32_t cycle_action_status[PS_HW6_OWNER_SM_CYCLE_COUNT]
                              [PS_HW6_OWNER_SM_CYCLE_DIRECTION_COUNT]
                              [PS_HW6_OWNER_SM_PHYSICAL_OWNER_COUNT];

  uint32_t current_state[PS_HW6_OWNER_SM_COUNT];
  uint32_t previous_state[PS_HW6_OWNER_SM_COUNT];
  uint32_t requested_state[PS_HW6_OWNER_SM_COUNT];
  uint32_t last_event[PS_HW6_OWNER_SM_COUNT];
  uint32_t transition_count[PS_HW6_OWNER_SM_COUNT];
  uint32_t rejected_transition_count[PS_HW6_OWNER_SM_COUNT];
  uint32_t last_action_status[PS_HW6_OWNER_SM_COUNT];
  uint32_t last_error[PS_HW6_OWNER_SM_COUNT];
  uint32_t last_transition_tick[PS_HW6_OWNER_SM_COUNT];

  uint32_t trace_count;
  uint32_t trace_write_index;
  PS_HW6_OwnerStateTrace trace[PS_HW6_OWNER_SM_TRACE_DEPTH];

  uint32_t joystick_driver_api_version;
  uint32_t joystick_driver_init_status;
  uint32_t joystick_driver_state;
  uint32_t joystick_driver_operation_count;
  uint32_t joystick_driver_last_status;
  uint32_t joystick_ready_status;
  uint32_t joystick_identity_status;
  uint32_t joystick_device_id;
  uint32_t joystick_manufacturer_lsb;
  uint32_t joystick_manufacturer_msb;
  uint32_t joystick_identity_match;
  uint32_t joystick_sensor_config1_before;
  uint32_t joystick_sensor_config1_after;
  uint32_t joystick_device_config2_before;
  uint32_t joystick_device_config2_after;
  uint32_t joystick_device_config2_sleep;
  uint32_t joystick_write_ok_mask;
  uint32_t joystick_verify_ok_mask;
  uint32_t joystick_sensor_config1_verify_status;
  uint32_t joystick_device_config2_verify_status;
  uint32_t joystick_sleep_write_status;
  uint32_t joystick_terminal_sleep_committed;
  uint32_t joystick_post_sleep_read_omitted;
  uint32_t joystick_i2c_state_after;
  uint32_t joystick_i2c_error_after;
  uint32_t joystick_cycle_wake_probe_status[PS_HW6_OWNER_SM_CYCLE_COUNT];
  uint32_t joystick_cycle_wake_retry_status[PS_HW6_OWNER_SM_CYCLE_COUNT];
  uint32_t joystick_cycle_active_sensor_config1[PS_HW6_OWNER_SM_CYCLE_COUNT];
  uint32_t joystick_cycle_active_device_config2[PS_HW6_OWNER_SM_CYCLE_COUNT];
  uint32_t joystick_cycle_active_status[PS_HW6_OWNER_SM_CYCLE_COUNT];
  uint32_t joystick_cycle_sleep_status[PS_HW6_OWNER_SM_CYCLE_COUNT];

  uint32_t imu_driver_api_version;
  uint32_t imu_driver_init_status;
  uint32_t imu_driver_state;
  uint32_t imu_driver_operation_count;
  uint32_t imu_driver_last_status;
  uint32_t imu_ready_status;
  uint32_t imu_whoami_status;
  uint32_t imu_whoami;
  uint32_t imu_identity_match;
  uint32_t imu_register_address[PS_HW6_OWNER_SM_IMU_REGISTER_COUNT];
  uint32_t imu_register_before[PS_HW6_OWNER_SM_IMU_REGISTER_COUNT];
  uint32_t imu_register_after[PS_HW6_OWNER_SM_IMU_REGISTER_COUNT];
  uint32_t imu_snapshot_ok_mask;
  uint32_t imu_write_ok_mask;
  uint32_t imu_verify_ok_mask;
  uint32_t imu_deep_power_down_value;
  uint32_t imu_deep_power_down_write_status;
  uint32_t imu_terminal_deep_power_down_committed;
  uint32_t imu_post_deep_power_down_read_omitted;
  uint32_t imu_i2c_state_after;
  uint32_t imu_i2c_error_after;
  uint32_t imu_cycle_wake_probe_status[PS_HW6_OWNER_SM_CYCLE_COUNT];
  uint32_t imu_cycle_wake_probe_error[PS_HW6_OWNER_SM_CYCLE_COUNT];
  uint32_t imu_cycle_wake_probe_accepted[PS_HW6_OWNER_SM_CYCLE_COUNT];
  uint32_t imu_cycle_whoami_status[PS_HW6_OWNER_SM_CYCLE_COUNT];
  uint32_t imu_cycle_whoami[PS_HW6_OWNER_SM_CYCLE_COUNT];
  uint32_t imu_cycle_active_ctrl5[PS_HW6_OWNER_SM_CYCLE_COUNT];
  uint32_t imu_cycle_active_status[PS_HW6_OWNER_SM_CYCLE_COUNT];
  uint32_t imu_cycle_sleep_status[PS_HW6_OWNER_SM_CYCLE_COUNT];

  uint32_t flash_driver_api_version;
  uint32_t flash_driver_init_status;
  uint32_t flash_driver_state;
  uint32_t flash_driver_operation_count;
  uint32_t flash_driver_last_status;
  uint32_t flash_jedec_status;
  uint32_t flash_jedec_id[3];
  uint32_t flash_identity_match;
  uint32_t flash_deep_power_down_status;
  uint32_t flash_scratch_status;
  uint32_t flash_scratch_address;
  uint32_t flash_scratch_length;
  uint32_t flash_scratch_status1_before;
  uint32_t flash_scratch_write_disable_status;
  uint32_t flash_scratch_write_disable_status1;
  uint32_t flash_scratch_erase_write_enable_status;
  uint32_t flash_scratch_erase_write_enable_status1;
  uint32_t flash_scratch_erase_status;
  uint32_t flash_scratch_erase_command_status1;
  uint32_t flash_scratch_erase_retry_count;
  uint32_t flash_scratch_erase_retry_write_disable_status;
  uint32_t flash_scratch_erase_retry_write_disable_status1;
  uint32_t flash_scratch_erase_retry_write_enable_status;
  uint32_t flash_scratch_erase_retry_write_enable_status1;
  uint32_t flash_scratch_erase_retry_status;
  uint32_t flash_scratch_erase_retry_status1;
  uint32_t flash_scratch_erase_wait_status;
  uint32_t flash_scratch_erase_poll_count;
  uint32_t flash_scratch_erase_blank_read_status;
  uint32_t flash_scratch_erase_blank_mismatch_count;
  uint32_t flash_scratch_erase_blank_first16[16];
  uint32_t flash_scratch_program_write_enable_status;
  uint32_t flash_scratch_program_write_enable_status1;
  uint32_t flash_scratch_program_status;
  uint32_t flash_scratch_program_wait_status;
  uint32_t flash_scratch_program_poll_count;
  uint32_t flash_scratch_program_read_status;
  uint32_t flash_scratch_program_mismatch_count;
  uint32_t flash_scratch_program_first16[16];
  uint32_t flash_scratch_dma_program_write_enable_status;
  uint32_t flash_scratch_dma_program_write_enable_status1;
  uint32_t flash_scratch_dma_program_status;
  uint32_t flash_scratch_dma_program_transfer_wait_status;
  uint32_t flash_scratch_dma_program_transfer_poll_count;
  uint32_t flash_scratch_dma_program_flash_wait_status;
  uint32_t flash_scratch_dma_program_flash_poll_count;
  uint32_t flash_scratch_dma_read_status;
  uint32_t flash_scratch_dma_read_transfer_wait_status;
  uint32_t flash_scratch_dma_read_transfer_poll_count;
  uint32_t flash_scratch_dma_verify_mismatch_count;
  uint32_t flash_scratch_dma_first16[16];
  uint32_t flash_scratch_dma_tx_state_after;
  uint32_t flash_scratch_dma_tx_error_after;
  uint32_t flash_scratch_dma_rx_state_after;
  uint32_t flash_scratch_dma_rx_error_after;
  uint32_t flash_scratch_cleanup_write_enable_status;
  uint32_t flash_scratch_cleanup_write_enable_status1;
  uint32_t flash_scratch_cleanup_erase_status;
  uint32_t flash_scratch_cleanup_wait_status;
  uint32_t flash_scratch_cleanup_poll_count;
  uint32_t flash_scratch_cleanup_blank_read_status;
  uint32_t flash_scratch_cleanup_blank_mismatch_count;
  uint32_t flash_scratch_cleanup_first16[16];
  uint32_t flash_scratch_ospi_state_after;
  uint32_t flash_scratch_ospi_error_after;
  uint32_t flash_block_api_version;
  uint32_t flash_block_init_status;
  uint32_t flash_block_operation_count;
  uint32_t flash_block_last_status;
  uint32_t flash_block_geometry_total_size;
  uint32_t flash_block_geometry_erase_size;
  uint32_t flash_block_geometry_page_size;
  uint32_t flash_block_geometry_count;
  uint32_t flash_block_test_status;
  uint32_t flash_block_test_address;
  uint32_t flash_block_test_index;
  uint32_t flash_block_test_length;
  uint32_t flash_block_erase_status;
  uint32_t flash_block_erase_poll_count;
  uint32_t flash_block_blank_read_status;
  uint32_t flash_block_blank_read_count;
  uint32_t flash_block_blank_mismatch_count;
  uint32_t flash_block_blank_first16[16];
  uint32_t flash_block_program_status;
  uint32_t flash_block_program_page_count;
  uint32_t flash_block_program_last_poll_count;
  uint32_t flash_block_verify_read_status;
  uint32_t flash_block_verify_read_count;
  uint32_t flash_block_verify_mismatch_count;
  uint32_t flash_block_verify_first16[16];
  uint32_t flash_block_cleanup_status;
  uint32_t flash_block_cleanup_poll_count;
  uint32_t flash_block_cleanup_read_status;
  uint32_t flash_block_cleanup_mismatch_count;
  uint32_t flash_block_cleanup_first16[16];
  uint32_t flash_block_ospi_state_after;
  uint32_t flash_block_ospi_error_after;
  uint32_t storage_layout_api_version;
  uint32_t storage_layout_validation_status;
  uint32_t storage_layout_region_count;
  uint32_t storage_layout_total_size;
  uint32_t storage_layout_erase_size;
  uint32_t storage_layout_end;
  uint32_t storage_layout_alignment_errors;
  uint32_t storage_layout_overlap_errors;
  uint32_t storage_layout_range_errors;
  uint32_t storage_layout_host_exposed_mask;
  uint32_t storage_layout_protected_mask;
  uint32_t storage_layout_scratch_index;
  uint32_t storage_layout_scratch_start;
  uint32_t storage_layout_scratch_length;
  uint32_t storage_fxlx_api_version;
  uint32_t storage_fxlx_status;
  uint32_t storage_fxlx_region_id;
  uint32_t storage_fxlx_region_start;
  uint32_t storage_fxlx_region_length;
  uint32_t storage_fxlx_test_start;
  uint32_t storage_fxlx_test_length;
  uint32_t storage_fxlx_erase_block_size;
  uint32_t storage_fxlx_sector_size;
  uint32_t storage_fxlx_sector_count;
  uint32_t storage_fxlx_lx_initialize_status;
  uint32_t storage_fxlx_lx_open_status;
  uint32_t storage_fxlx_fx_format_status;
  uint32_t storage_fxlx_fx_open_status;
  uint32_t storage_fxlx_file_create_status;
  uint32_t storage_fxlx_file_open_status;
  uint32_t storage_fxlx_file_write_status;
  uint32_t storage_fxlx_file_seek_status;
  uint32_t storage_fxlx_file_read_status;
  uint32_t storage_fxlx_file_close_status;
  uint32_t storage_fxlx_fx_flush_status;
  uint32_t storage_fxlx_fx_close_status;
  uint32_t storage_fxlx_lx_close_status;
  uint32_t storage_fxlx_bytes_written;
  uint32_t storage_fxlx_bytes_read;
  uint32_t storage_fxlx_verify_mismatch_count;
  uint32_t storage_fxlx_boot_read_first16[16];
  uint32_t storage_fxlx_boot_bytes_per_sector;
  uint32_t storage_fxlx_boot_sectors_per_cluster;
  uint32_t storage_fxlx_boot_reserved_sectors;
  uint32_t storage_fxlx_boot_number_of_fats;
  uint32_t storage_fxlx_boot_root_entries;
  uint32_t storage_fxlx_boot_total_sectors;
  uint32_t storage_fxlx_boot_sectors_per_fat;
  uint32_t storage_fxlx_boot_signature;
  uint32_t storage_fxlx_read_first16[16];
  uint32_t storage_fxlx_lx_driver_read_count;
  uint32_t storage_fxlx_lx_driver_write_count;
  uint32_t storage_fxlx_lx_driver_erase_count;
  uint32_t storage_fxlx_lx_driver_verify_count;
  uint32_t storage_fxlx_lx_driver_last_status;
  uint32_t storage_fxlx_fx_driver_read_count;
  uint32_t storage_fxlx_fx_driver_write_count;
  uint32_t storage_fxlx_fx_driver_flush_count;
  uint32_t storage_fxlx_fx_driver_abort_count;
  uint32_t storage_fxlx_fx_driver_init_count;
  uint32_t storage_fxlx_fx_driver_uninit_count;
  uint32_t storage_fxlx_fx_driver_release_count;
  uint32_t storage_fxlx_fx_driver_last_request;
  uint32_t storage_fxlx_fx_driver_last_status;
  uint32_t flash_ospi_state_after;
  uint32_t flash_ospi_error_after;
  uint32_t flash_cycle_release_status[PS_HW6_OWNER_SM_CYCLE_COUNT];
  uint32_t flash_cycle_jedec_status[PS_HW6_OWNER_SM_CYCLE_COUNT];
  uint32_t flash_cycle_identity_match[PS_HW6_OWNER_SM_CYCLE_COUNT];
  uint32_t flash_cycle_deep_power_down_status[PS_HW6_OWNER_SM_CYCLE_COUNT];

  uint32_t usb_vbus_present;
  uint32_t usb_pcd_state_before;
  uint32_t usb_clock_enabled_before;
  uint32_t usb_vddusb_enabled_before;
  uint32_t usb_deinit_attempted;
  uint32_t usb_deinit_status;
  uint32_t usb_pcd_state_after;
  uint32_t usb_clock_enabled_after;
  uint32_t usb_vddusb_enabled_after;
  uint32_t usb_parked;

  uint32_t ble_nrst_before;
  uint32_t ble_nrst_released;
  uint32_t ble_nrst_after;
  uint32_t ble_dsr_host_control_before;
  uint32_t ble_dsr_host_control_after;
  uint32_t ble_boot_rx_len;
  uint32_t ble_command_count;
  uint32_t ble_command_tx_status[PS_HW6_OWNER_SM_NINA_COMMAND_COUNT];
  uint32_t ble_command_rx_len[PS_HW6_OWNER_SM_NINA_COMMAND_COUNT];
  uint32_t ble_command_required_mask;
  uint32_t ble_command_attempted_mask;
  uint32_t ble_command_skipped_mask;
  uint32_t ble_command_ok_mask;
  uint32_t ble_command_error_mask;
  uint32_t ble_uart_deinit_status;
  uint32_t ble_uart_state_after;
  uint32_t ble_uart_error_after;
  uint32_t ble_fallback_reset_asserted;
  uint32_t ble_cycle_uart_init_status[PS_HW6_OWNER_SM_CYCLE_COUNT];
  uint32_t ble_cycle_wake_at_status[PS_HW6_OWNER_SM_CYCLE_COUNT];
  uint32_t ble_cycle_wake_rx_len[PS_HW6_OWNER_SM_CYCLE_COUNT];
  uint32_t ble_cycle_suspend_uart_status[PS_HW6_OWNER_SM_CYCLE_COUNT];
  uint32_t ble_cycle_dsr_after_resume[PS_HW6_OWNER_SM_CYCLE_COUNT];
  uint32_t ble_cycle_dsr_after_quiesce[PS_HW6_OWNER_SM_CYCLE_COUNT];
} PS_HW6_OwnerStateMachineProbe;

extern volatile PS_HW6_OwnerStateMachineProbe g_ps_hw6_owner_sm_probe;
extern volatile uint32_t g_ps_hw6_owner_sm_start_request;

void PS_HW6_OwnerStateMachines_Init(void);
void PS_HW6_OwnerStateMachines_BeginWorkflow(void);
HAL_StatusTypeDef PS_HW6_OwnerStateMachines_Stabilize(uint32_t owner_id);
void PS_HW6_OwnerStateMachines_BeginCycle(uint32_t cycle_index);
HAL_StatusTypeDef PS_HW6_OwnerStateMachines_Resume(uint32_t owner_id,
                                                    uint32_t cycle_index);
HAL_StatusTypeDef PS_HW6_OwnerStateMachines_Quiesce(uint32_t owner_id,
                                                     uint32_t cycle_index);
void PS_HW6_OwnerStateMachines_RecordCycleCommand(
  uint32_t cycle_index,
  uint32_t direction,
  uint32_t owner_id,
  uint32_t send_status,
  uint32_t ack_wait_status,
  uint32_t ack_flags);
void PS_HW6_OwnerStateMachines_RecordCycleActiveStates(uint32_t cycle_index);
void PS_HW6_OwnerStateMachines_EndCycle(uint32_t cycle_index);
void PS_HW6_OwnerStateMachines_RecordCommand(uint32_t owner_id,
                                              uint32_t send_status,
                                              uint32_t ack_wait_status,
                                              uint32_t ack_flags);
void PS_HW6_OwnerStateMachines_EndWorkflow(void);

#ifdef __cplusplus
}
#endif

#endif /* PS_HW6_OWNER_STATE_MACHINES_H */
