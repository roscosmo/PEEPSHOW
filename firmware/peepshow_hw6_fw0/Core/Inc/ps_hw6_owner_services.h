#ifndef PS_HW6_OWNER_SERVICES_H
#define PS_HW6_OWNER_SERVICES_H

#include <stdint.h>

#include "stm32u5xx_hal.h"
#include "ps_dev_adp5360.h"
#include "tx_api.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PS_HW6_OWNER_PROBE_MAGIC                 (0x48364F57UL)
#define PS_HW6_OWNER_PROBE_VERSION               (17UL)
#define PS_HW6_OWNER_POWER_REGISTER_COUNT        (7U)
#define PS_HW6_OWNER_CHARGER_CONFIG_REGISTER_COUNT \
  PS_DEV_ADP5360_CHARGER_CONFIG_REGISTER_COUNT
#define PS_HW6_OWNER_INTERRUPT_REGISTER_COUNT \
  PS_DEV_ADP5360_INTERRUPT_REGISTER_COUNT
#define PS_HW6_OWNER_INTERRUPT_FLAG_REGISTER_COUNT \
  PS_DEV_ADP5360_INTERRUPT_FLAG_REGISTER_COUNT
#define PS_HW6_OWNER_STATUS_NOT_RUN              (0xFFFFFFFFUL)
#define PS_HW6_OWNER_STATUS_UNAVAILABLE          (0xFFFFFFFEUL)

typedef struct
{
  uint32_t magic;
  uint32_t version;
  uint32_t phase;
  uint32_t complete;
  uint32_t success;
  uint32_t services_init_status;
  uint32_t workflow_start_tick;
  uint32_t workflow_end_tick;
  uint32_t power_command_send_status;
  uint32_t display_command_send_status;
  uint32_t display_ack_wait_status;
  uint32_t display_ack_flags;
  uint32_t audio_command_send_status;
  uint32_t audio_ack_wait_status;
  uint32_t audio_ack_flags;

  uint32_t power_command_tick;
  uint32_t power_complete;
  uint32_t power_success;
  uint32_t power_driver_api_version;
  uint32_t power_driver_init_status;
  uint32_t power_driver_mr_shipping_mode_status;
  uint32_t power_driver_fuel_gauge_prepare_status;
  uint32_t power_driver_thermistor_config_status;
  uint32_t power_driver_charger_profile_status;
  uint32_t power_driver_interrupt_config_status;
  uint32_t power_driver_software_shipping_mode_status;
  uint32_t power_software_ship_request_count;
  uint32_t power_software_ship_request_tick;
  uint32_t power_driver_state;
  uint32_t power_driver_operation_count;
  uint32_t power_driver_last_status;
  uint32_t power_driver_function_ready_mask;
  uint32_t power_driver_read_ok_mask;
  uint32_t power_driver_expected_match_mask;
  uint32_t power_register_address[PS_HW6_OWNER_POWER_REGISTER_COUNT];
  uint32_t power_register_value[PS_HW6_OWNER_POWER_REGISTER_COUNT];
  uint32_t power_lease_get_status[PS_HW6_OWNER_POWER_REGISTER_COUNT];
  uint32_t power_transfer_status[PS_HW6_OWNER_POWER_REGISTER_COUNT];
  uint32_t power_transfer_error[PS_HW6_OWNER_POWER_REGISTER_COUNT];
  uint32_t power_lease_put_status[PS_HW6_OWNER_POWER_REGISTER_COUNT];
  uint32_t power_i2c_state_after;
  uint32_t power_i2c_error_after;
  uint32_t power_identity_match;
  uint32_t power_rails_ready;
  uint32_t power_fault_clear;
  uint32_t power_charger_status1;
  uint32_t power_charger_status2;
  uint32_t power_charger_status1_status;
  uint32_t power_charger_status2_status;
  uint32_t power_charger_status1_hal_status;
  uint32_t power_charger_status1_hal_error;
  uint32_t power_charger_status2_hal_status;
  uint32_t power_charger_status2_hal_error;
  uint32_t power_charger_thermistor_control;
  uint32_t power_charger_thermistor_control_status;
  uint32_t power_charger_thermistor_control_hal_status;
  uint32_t power_charger_thermistor_control_hal_error;
  uint32_t power_charger_monitor_read_ok_mask;
  uint32_t power_charger_config_read_ok_mask;
  uint32_t power_charger_config_address[PS_HW6_OWNER_CHARGER_CONFIG_REGISTER_COUNT];
  uint32_t power_charger_config_value[PS_HW6_OWNER_CHARGER_CONFIG_REGISTER_COUNT];
  uint32_t power_charger_config_status[PS_HW6_OWNER_CHARGER_CONFIG_REGISTER_COUNT];
  uint32_t power_charger_config_hal_status[PS_HW6_OWNER_CHARGER_CONFIG_REGISTER_COUNT];
  uint32_t power_charger_config_hal_error[PS_HW6_OWNER_CHARGER_CONFIG_REGISTER_COUNT];
  uint32_t power_interrupt_read_ok_mask;
  uint32_t power_interrupt_register_address[PS_HW6_OWNER_INTERRUPT_REGISTER_COUNT];
  uint32_t power_interrupt_register_value[PS_HW6_OWNER_INTERRUPT_REGISTER_COUNT];
  uint32_t power_interrupt_register_status[PS_HW6_OWNER_INTERRUPT_REGISTER_COUNT];
  uint32_t power_interrupt_register_hal_status[PS_HW6_OWNER_INTERRUPT_REGISTER_COUNT];
  uint32_t power_interrupt_register_hal_error[PS_HW6_OWNER_INTERRUPT_REGISTER_COUNT];
  uint32_t power_interrupt_clear_ok_mask;
  uint32_t power_interrupt_clear_address[PS_HW6_OWNER_INTERRUPT_FLAG_REGISTER_COUNT];
  uint32_t power_interrupt_clear_value[PS_HW6_OWNER_INTERRUPT_FLAG_REGISTER_COUNT];
  uint32_t power_interrupt_clear_status[PS_HW6_OWNER_INTERRUPT_FLAG_REGISTER_COUNT];
  uint32_t power_interrupt_clear_hal_status[PS_HW6_OWNER_INTERRUPT_FLAG_REGISTER_COUNT];
  uint32_t power_interrupt_clear_hal_error[PS_HW6_OWNER_INTERRUPT_FLAG_REGISTER_COUNT];
  uint32_t power_charger_mode;
  uint32_t power_charger_status;
  uint32_t power_charger_charge_type;
  uint32_t power_charger_health;
  uint32_t power_battery_status;
  uint32_t power_battery_thermal_status;
  uint32_t power_battery_present;
  uint32_t power_vbus_ok;
  uint32_t power_mcu_vbus_present;
  uint32_t power_vbus_agree;
  uint32_t power_vbus_disagree_count;
  uint32_t power_vbus_last_disagree_tick;
  uint32_t power_battery_ok;
  uint32_t power_charge_complete;
  uint32_t power_fuel_register_address[PS_DEV_ADP5360_FUEL_REGISTER_COUNT];
  uint32_t power_fuel_register_value[PS_DEV_ADP5360_FUEL_REGISTER_COUNT];
  uint32_t power_fuel_register_status[PS_DEV_ADP5360_FUEL_REGISTER_COUNT];
  uint32_t power_fuel_register_hal_status[PS_DEV_ADP5360_FUEL_REGISTER_COUNT];
  uint32_t power_fuel_register_hal_error[PS_DEV_ADP5360_FUEL_REGISTER_COUNT];
  uint32_t power_fuel_read_ok_mask;
  uint32_t power_fuel_soc_percent;
  uint32_t power_fuel_vbat_mv;
  uint32_t power_fuel_vbat_h;
  uint32_t power_fuel_vbat_l;
  uint32_t power_regulator_read_ok_mask;
  uint32_t power_regulator_buck_cfg;
  uint32_t power_regulator_buck_output;
  uint32_t power_regulator_buckbst_cfg;
  uint32_t power_regulator_buckbst_output;
  uint32_t power_regulator_vout1_ok;
  uint32_t power_regulator_vout2_ok;
  uint32_t power_regulator_battery_ok;

  uint32_t display_driver_api_version;
  uint32_t display_driver_init_status;
  uint32_t display_driver_state;
  uint32_t display_driver_operation_count;
  uint32_t display_driver_last_status;
  uint32_t display_command_tick;
  uint32_t display_complete;
  uint32_t display_success;
  uint32_t display_width;
  uint32_t display_height;
  uint32_t display_pattern_id;
  uint32_t display_framebuffer_hash;
  uint32_t display_black_pixels;
  uint32_t display_rtc_state;
  uint32_t display_rtc_cr;
  uint32_t display_spi_state_before;
  uint32_t display_init_status;
  uint32_t display_present_status;
  uint32_t display_dma_done;
  uint32_t display_spi_state_after;
  uint32_t display_spi_error_after;
  uint32_t display_dma_state_after;
  uint32_t display_dma_error_after;
  uint32_t display_ack_set_status;
  uint32_t display_ui_request_count;
  uint32_t display_ui_render_count;
  uint32_t display_ui_page;
  uint32_t display_ui_calibration_page;
  uint32_t display_ui_focus_index;
  uint32_t display_ui_shutdown_state;
  uint32_t display_ui_shutdown_countdown_seconds;
  uint32_t display_ui_status;
  uint32_t display_lpbam_ready;
  uint32_t display_lpbam_ready_page;
  uint32_t display_lpbam_ready_render_count;
  uint32_t display_lpbam_prepare_count;
  uint32_t display_lpbam_prepare_tick;
  uint32_t display_lpbam_prepare_status;
  uint32_t display_lpbam_debug_force_ready_count;
  uint32_t display_lpbam_abort_count;
  uint32_t display_lpbam_abort_tick;
  uint32_t display_lpbam_abort_status;
  uint32_t display_lpbam_clear_count;
  uint32_t display_lpbam_clear_reason;
  uint32_t display_lpbam_status;
  uint32_t display_lpbam_active;
  uint32_t display_lpbam_cursor_start_row;
  uint32_t display_lpbam_cursor_row_count;
  uint32_t display_lpbam_cursor_start_column;
  uint32_t display_lpbam_cursor_column_count;
  uint32_t display_lpbam_payload_frame_count;
  uint32_t display_lpbam_payload_chunk_count;
  uint32_t display_lpbam_payload_bytes;
  uint32_t display_lpbam_fill_status;
  uint32_t display_lpbam_clock_status;
  uint32_t display_lpbam_link_status;
  uint32_t display_lpbam_start_status;
  uint32_t display_lpbam_dma_start_status;
  uint32_t display_lpbam_lptim_init_status;
  uint32_t display_lpbam_lptim_oc_status;
  uint32_t display_lpbam_lptim_arr_status;
  uint32_t display_lpbam_lptim_cmp_status;
  uint32_t display_lpbam_lptim_start_status;
  uint32_t display_lpbam_lptim_restore_status;
  uint32_t display_lpbam_lptim_cr_after_config;
  uint32_t display_lpbam_lptim_cfgr_after_config;
  uint32_t display_lpbam_lptim_ccmr1_after_config;
  uint32_t display_lpbam_lptim_arr_after_config;
  uint32_t display_lpbam_lptim_cmp_after_config;
  uint32_t display_lpbam_rcc_srdamr_before;
  uint32_t display_lpbam_rcc_srdamr_after;
  uint32_t display_lpbam_spi_autocr_before;
  uint32_t display_lpbam_spi_autocr_after;
  uint32_t display_lpbam_dma_state_after_start;
  uint32_t display_lpbam_dma_error_after_start;
  uint32_t display_lpbam_queue_node_count;
  uint32_t display_lpbam_abort_lptim_status;
  uint32_t display_lpbam_abort_dma_status;
  uint32_t display_lpbam_abort_unlink_status;
  uint32_t display_lpbam_abort_spi_status;
  uint32_t display_lpbam_restore_status;

  uint32_t audio_driver_api_version;
  uint32_t audio_driver_init_status;
  uint32_t audio_driver_state;
  uint32_t audio_driver_operation_count;
  uint32_t audio_driver_last_status;
  uint32_t audio_command_tick;
  uint32_t audio_complete;
  uint32_t audio_success;
  uint32_t audio_sai_kernel_hz;
  uint32_t audio_sample_rate_hz;
  uint32_t audio_tone_hz;
  uint32_t audio_duration_ms;
  uint32_t audio_amplitude;
  uint32_t audio_buffer_halfwords;
  uint32_t audio_sd_state_before;
  uint32_t audio_sd_state_enabled;
  uint32_t audio_start_status;
  uint32_t audio_stop_status;
  uint32_t audio_sd_state_after;
  uint32_t audio_sai_state_after;
  uint32_t audio_sai_error_after;
  uint32_t audio_dma_state_after;
  uint32_t audio_dma_error_after;
  uint32_t audio_ack_set_status;
} PS_HW6_OwnerProbe;

extern volatile PS_HW6_OwnerProbe g_ps_hw6_owner_probe;

UINT PS_HW6_OwnerServices_Init(void);
HAL_StatusTypeDef PS_HW6_PowerOwner_EnableMrShippingMode(void);
HAL_StatusTypeDef PS_HW6_PowerOwner_PrepareFuelGauge(void);
HAL_StatusTypeDef PS_HW6_PowerOwner_ConfigureThermistor(void);
HAL_StatusTypeDef PS_HW6_PowerOwner_ConfigureChargerProfile(void);
HAL_StatusTypeDef PS_HW6_PowerOwner_ConfigurePmicInterrupts(void);
HAL_StatusTypeDef PS_HW6_PowerOwner_EnterSoftwareShipmentMode(void);
HAL_StatusTypeDef PS_HW6_PowerOwner_RunSnapshot(void);
HAL_StatusTypeDef PS_HW6_DisplayOwner_RunPattern(void);
HAL_StatusTypeDef PS_HW6_DisplayOwner_ClearBootHold(void);
HAL_StatusTypeDef PS_HW6_DisplayOwner_RenderUI(
  uint32_t page,
  uint32_t calibration_page,
  uint32_t focus_index,
  uint32_t shutdown_state,
  uint32_t shutdown_countdown_seconds);
HAL_StatusTypeDef PS_HW6_DisplayOwner_PrepareLpbamStop2(void);
HAL_StatusTypeDef PS_HW6_DisplayOwner_AbortLpbamStop2(void);
void PS_HW6_DisplayOwner_DebugForceNextLpbamReady(void);
HAL_StatusTypeDef PS_HW6_AudioOwner_RunTone(void);
HAL_StatusTypeDef PS_HW6_AudioOwner_VerifyIdle(void);
void PS_HW6_OwnerServices_MarkComplete(void);

#ifdef __cplusplus
}
#endif

#endif /* PS_HW6_OWNER_SERVICES_H */
