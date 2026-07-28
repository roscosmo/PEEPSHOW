/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    app_threadx.c
  * @author  MCD Application Team
  * @brief   ThreadX applicative file
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

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "ADP5360.h"
#include "LIS2DUX12.h"
#include "lpbam_lpbamap1.h"
#include "ps_lpbam_display_buffers.h"
#include "stm32u5xx_hal_spi_ex.h"
#include "stm32u5xx_ll_spi.h"
#include "main.h"
#include "ps_display_renderer.h"
#include "tmag3001.h"

extern void SystemClock_Config(void);

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef struct
{
  ULONG magic;
  ULONG phase;
  ULONG init_time;
  ULONG pool_available_bytes;
  ULONG pool_fragments;
  ULONG create_status[7];
  ULONG start_mask;
  ULONG heartbeat[7];
  ULONG last_time[7];
  ULONG thread_resumptions;
  ULONG thread_suspensions;
  ULONG thread_solicited_preemptions;
  ULONG thread_interrupt_preemptions;
  ULONG thread_priority_inversions;
  ULONG thread_time_slices;
  ULONG thread_relinquishes;
  ULONG thread_timeouts;
  ULONG thread_wait_aborts;
  ULONG thread_non_idle_returns;
  ULONG thread_idle_returns;
  ULONG power_action_count;
  ULONG power_action_error_count;
  ULONG power_pmic_init_status;
  ULONG power_rail_config_status;
  ULONG power_read_before_status;
  ULONG power_read_after_status;
  ULONG power_buck_cfg_before;
  ULONG power_buck_vout_before;
  ULONG power_buckboost_cfg_before;
  ULONG power_buckboost_vout_before;
  ULONG power_pgood_before;
  ULONG power_fault_before;
  ULONG power_buck_cfg_after;
  ULONG power_buck_vout_after;
  ULONG power_buckboost_cfg_after;
  ULONG power_buckboost_vout_after;
  ULONG power_pgood_after;
  ULONG power_fault_after;
  ULONG power_vbat_mv;
  ULONG power_vbat_raw;
  ULONG power_i2c_error_after;
  ULONG power_rails_ready;
  ULONG power_pgood_poll_count;
  ULONG power_adp_id;
  ULONG power_adp_revision;
  ULONG power_charger_function;
  ULONG power_charger_status1;
  ULONG power_charger_status2;
  ULONG power_supervisory;
  ULONG power_pgood1_mask;
  ULONG power_pgood2_mask;
  ULONG power_int_enable1;
  ULONG power_int_enable2;
  ULONG power_shipmode;
  ULONG power_buck_vout_mv;
  ULONG power_buck_vout_delay_us;
  ULONG power_buckboost_vout_mv;
  ULONG power_buckboost_vout_delay_us;
  ULONG power_buck_enable_decoded;
  ULONG power_buck_ilim_ma_decoded;
  ULONG power_buckboost_enable_decoded;
  ULONG power_buckboost_ilim_ma_decoded;
  ULONG input_queue_create_status;
  ULONG input_queue_ready;
  ULONG input_queue_sent;
  ULONG input_queue_send_fail;
  ULONG input_queue_not_ready_drop;
  ULONG input_queue_received;
  ULONG input_queue_receive_timeouts;
  ULONG input_last_send_status;
  ULONG input_last_button_id;
  ULONG input_last_active_level;
  ULONG input_last_event_tick;
  ULONG input_last_sequence;
  ULONG input_button_edges[5];
  ULONG input_button_presses[5];
  ULONG input_button_releases[5];
  ULONG input_debounce_ticks;
  ULONG input_debounce_rejected[5];
  ULONG input_duplicate_level_ignored[5];
  ULONG input_debounced_edges[5];
  ULONG input_debounced_presses[5];
  ULONG input_debounced_releases[5];
  ULONG input_debounced_mask;
  ULONG input_last_debounced_button_id;
  ULONG input_last_debounced_active_level;
  ULONG input_last_debounced_tick;
  ULONG input_chord_mask;
  ULONG input_chord_events;
  ULONG ui_queue_create_status;
  ULONG ui_queue_ready;
  ULONG ui_queue_sent;
  ULONG ui_queue_send_fail;
  ULONG ui_queue_received;
  ULONG ui_queue_receive_timeouts;
  ULONG ui_last_send_status;
  ULONG ui_last_sequence;
  ULONG ui_last_event_type;
  ULONG ui_last_button_id;
  ULONG ui_last_active_level;
  ULONG ui_last_mask;
  ULONG ui_last_event_tick;
  ULONG ui_button_events[5];
  ULONG ui_button_presses[5];
  ULONG ui_button_releases[5];
  ULONG ui_chord_events;
  ULONG display_queue_create_status;
  ULONG display_queue_ready;
  ULONG display_queue_sent;
  ULONG display_queue_send_fail;
  ULONG display_queue_received;
  ULONG display_queue_receive_timeouts;
  ULONG display_last_send_status;
  ULONG display_last_sequence;
  ULONG display_last_cmd_type;
  ULONG display_last_source_event;
  ULONG display_last_button_id;
  ULONG display_last_mask;
  ULONG display_last_event_tick;
  ULONG display_activity_hints;
  ULONG display_action_count;
  ULONG display_action_error_count;
  ULONG display_init_status;
  ULONG display_renderer_init_status;
  ULONG display_fill_status;
  ULONG display_present_status;
  ULONG display_last_hal_error;
  ULONG display_last_fill_value;
  ULONG display_last_action_mode;
  ULONG display_vlt_lcd_state;
  ULONG display_spi_state_after;
  ULONG display_spi_error_after;
  ULONG display_dma_done_after;
  ULONG display_power_not_ready_skips;
  ULONG storage_queue_create_status;
  ULONG storage_queue_ready;
  ULONG storage_queue_sent;
  ULONG storage_queue_send_fail;
  ULONG storage_queue_received;
  ULONG storage_queue_receive_timeouts;
  ULONG storage_last_send_status;
  ULONG storage_last_sequence;
  ULONG storage_last_cmd_type;
  ULONG storage_last_source_event;
  ULONG storage_last_button_id;
  ULONG storage_last_mask;
  ULONG storage_last_event_tick;
  ULONG storage_activity_hints;
  ULONG comms_queue_create_status;
  ULONG comms_queue_ready;
  ULONG comms_queue_sent;
  ULONG comms_queue_send_fail;
  ULONG comms_queue_received;
  ULONG comms_queue_receive_timeouts;
  ULONG comms_last_send_status;
  ULONG comms_last_sequence;
  ULONG comms_last_cmd_type;
  ULONG comms_last_source_event;
  ULONG comms_last_button_id;
  ULONG comms_last_mask;
  ULONG comms_last_event_tick;
  ULONG comms_activity_hints;
  ULONG audio_queue_create_status;
  ULONG audio_queue_ready;
  ULONG audio_queue_sent;
  ULONG audio_queue_send_fail;
  ULONG audio_queue_received;
  ULONG audio_queue_receive_timeouts;
  ULONG audio_last_send_status;
  ULONG audio_last_sequence;
  ULONG audio_last_cmd_type;
  ULONG audio_last_source_event;
  ULONG audio_last_button_id;
  ULONG audio_last_mask;
  ULONG audio_last_event_tick;
  ULONG audio_activity_hints;
  ULONG audio_action_count;
  ULONG audio_action_error_count;
  ULONG audio_bbb_start_status;
  ULONG audio_bbb_stop_status;
  ULONG audio_bbb_kernel_hz;
  ULONG audio_bbb_period;
  ULONG audio_bbb_pulse;
  ULONG audio_bbb_cue_hz;
  ULONG audio_bbb_cue_ticks;
  ULONG audio_lptim_state_after;
  ULONG audio_power_not_ready_skips;
  ULONG audio_speaker_action_count;
  ULONG audio_speaker_action_error_count;
  ULONG audio_speaker_start_status;
  ULONG audio_speaker_stop_status;
  ULONG audio_speaker_kernel_hz;
  ULONG audio_speaker_sample_rate_hz;
  ULONG audio_speaker_tone_hz;
  ULONG audio_speaker_amplitude;
  ULONG audio_speaker_buffer_halfwords;
  ULONG audio_speaker_cue_ticks;
  ULONG audio_speaker_sai_state_after;
  ULONG audio_speaker_sai_error_after;
  ULONG audio_speaker_sai_sr_after;
  ULONG audio_speaker_sd_mode_state_after;
  ULONG phase6_quiesce_request;
  ULONG phase6_quiesce_tick;
  ULONG phase6_quiesce_ack_mask;
  ULONG phase6_sleep_trigger_requested;
  ULONG phase6_sleep_trigger_button_id;
  ULONG phase6_sleep_trigger_tick;
  ULONG phase6_resume_cycle_count;
  ULONG phase6_resume_i2c_init_status;
  ULONG phase6_resume_trigger_cleared;
  ULONG phase6_stage;
  ULONG phase6_stage_tick;
  ULONG phase6_stage_hold_ticks;
  ULONG phase6_display_vlt_lcd_state;
  ULONG phase6_audio_sd_mode_state;
  ULONG phase6_audio_lptim_stop_status;
  ULONG phase6_audio_sai_stop_status;
  ULONG phase6_peripheral_deinit_done;
  ULONG phase6_external_sleep_done;
  ULONG phase6_flash_dpd_status;
  ULONG phase6_lis_mode_get_status;
  ULONG phase6_lis_mode_set_status;
  ULONG phase6_lis_deep_pd_status;
  ULONG phase6_lis_whoami_status;
  ULONG phase6_lis_ctrl1_before;
  ULONG phase6_lis_ctrl2_before;
  ULONG phase6_lis_ctrl3_before;
  ULONG phase6_lis_ctrl4_before;
  ULONG phase6_lis_ctrl5_before;
  ULONG phase6_lis_fifo_ctrl_before;
  ULONG phase6_lis_interrupt_cfg_before;
  ULONG phase6_lis_md1_cfg_before;
  ULONG phase6_lis_md2_cfg_before;
  ULONG phase6_lis_sleep_before;
  ULONG phase6_lis_fifo_bypass_status;
  ULONG phase6_lis_interrupt_clear_status;
  ULONG phase6_lis_temp_disable_status;
  ULONG phase6_lis_embedded_disable_status;
  ULONG phase6_lis_ctrl1_after;
  ULONG phase6_lis_ctrl2_after;
  ULONG phase6_lis_ctrl3_after;
  ULONG phase6_lis_ctrl4_after;
  ULONG phase6_lis_ctrl5_after;
  ULONG phase6_lis_fifo_ctrl_after;
  ULONG phase6_lis_interrupt_cfg_after;
  ULONG phase6_lis_md1_cfg_after;
  ULONG phase6_lis_md2_cfg_after;
  ULONG phase6_lis_sleep_after;
  ULONG phase6_lis_readback_status;
  ULONG phase6_tmag_ready_status;
  ULONG phase6_tmag_sensor_cfg1_before;
  ULONG phase6_tmag_sensor_cfg1_after;
  ULONG phase6_tmag_device_cfg2_before;
  ULONG phase6_tmag_device_cfg2_after;
  ULONG phase6_tmag_sensor_sleep_status;
  ULONG phase6_nina_nrst_state;
  ULONG phase6_nina_boot_rx_len;
  ULONG phase6_nina_boot_rx_word0;
  ULONG phase6_nina_boot_rx_word1;
  ULONG phase6_nina_at_tx_status;
  ULONG phase6_nina_at_rx_len;
  ULONG phase6_nina_at_ok;
  ULONG phase6_nina_at_error;
  ULONG phase6_nina_at_rx_word0;
  ULONG phase6_nina_at_rx_word1;
  ULONG phase6_nina_poweroff_tx_status;
  ULONG phase6_nina_poweroff_rx_len;
  ULONG phase6_nina_poweroff_ok;
  ULONG phase6_nina_poweroff_error;
  ULONG phase6_nina_poweroff_rx_word0;
  ULONG phase6_nina_poweroff_rx_word1;
  ULONG phase6_nina_poweroff_rx_word2;
  ULONG phase6_nina_poweroff_rx_word3;
  ULONG phase6_nina_pin_park_done;
  ULONG phase6_nina_gpioa_moder_after;
  ULONG phase6_nina_gpiob_moder_after;
  ULONG phase6_nina_gpioc_moder_after;
  ULONG phase6_phot_en_state;
  ULONG phase6_buck_pulsestop_get_status;
  ULONG phase6_buck_pulsestop_set_status;
  ULONG phase6_buck_pulsestop_cfg_after;
  ULONG phase6_buckboost_pulsestop_get_status;
  ULONG phase6_buckboost_pulsestop_set_status;
  ULONG phase6_buckboost_pulsestop_cfg_after;
  ULONG phase6_diag_cut_3v3_enabled;
  ULONG phase6_diag_cut_3v3_tick;
  ULONG phase6_diag_vlt_lcd_state;
  ULONG phase6_diag_buckboost_get_status;
  ULONG phase6_diag_buckboost_set_status;
  ULONG phase6_diag_buckboost_cfg_after;
  ULONG phase6_diag_pgood_after_3v3_cut;
  ULONG phase6_diag_buckboost_cfg_after_hold;
  ULONG phase6_diag_pgood_after_3v3_hold;
  ULONG phase6_adc_deinit_status;
  ULONG phase6_ospi_deinit_status;
  ULONG phase6_spi_deinit_status;
  ULONG phase6_lptim_deinit_status;
  ULONG phase6_sai_deinit_status;
  ULONG phase6_uart_deinit_status;
  ULONG phase6_i2c_deinit_status;
  ULONG phase6_pll2_disabled;
  ULONG phase6_hsi_disabled;
  ULONG phase6_msik_disabled;
  ULONG phase6_quiesce_ack_wait_ticks;
  ULONG phase6_quiesce_ack_wait_complete;
  ULONG phase6_stop2_attempted;
  ULONG phase6_stop2_entry_tick;
  ULONG phase6_stop2_returned;
  ULONG phase6_stop2_return_count;
  ULONG phase6_stop2_return_tick;
  ULONG phase6_stop2_clock_restore_attempted;
  ULONG phase6_stop2_clock_restore_done;
  ULONG phase6_systick_ctrl_after_restore;
  ULONG phase6_pwr_sr_before_stop2;
  ULONG phase6_pwr_wusr_before_stop2;
  ULONG phase6_pwr_sr_after_stop2;
  ULONG phase6_pwr_wusr_after_stop2;
  ULONG phase6_scb_scr_before_stop2;
  ULONG phase6_scb_scr_after_stop2;
  ULONG phase6_dbgmcu_cr_before_stop2;
  ULONG phase6_dbgmcu_cr_after_stop2;
  ULONG phase6_systick_ctrl_before_stop2;
  ULONG phase6_systick_ctrl_after_disable;
  ULONG phase6_nvic_icer0_after_disable;
  ULONG phase6_nvic_icer1_after_disable;
  ULONG phase6_button_wake_rearm_done;
  ULONG phase6_exti_imr1_before_stop;
  ULONG phase6_exti_rtsr1_before_stop;
  ULONG phase6_exti_ftsr1_before_stop;
  ULONG phase6_exti_rpr1_before_stop;
  ULONG phase6_exti_fpr1_before_stop;
  ULONG phase6_exti_imr1_after_wake;
  ULONG phase6_exti_rpr1_after_wake;
  ULONG phase6_exti_fpr1_after_wake;
  ULONG phase6_nvic_iser0_before_stop;
  ULONG phase6_nvic_ispr0_before_stop;
  ULONG phase6_nvic_ispr0_after_wake;
  ULONG phase6_exti_callback_count;
  ULONG phase6_exti_callback_pin;
  ULONG phase6_exti_callback_button_id;
  ULONG phase6_exti_callback_active_level;
  ULONG phase6_exti_callback_tick;
  ULONG phase6_exti_callback_stage;
  ULONG phase6_exti_callback_gpioa_idr;
  ULONG phase6_exti_callback_gpiob_idr;
  ULONG phase6_wake_reason;
  ULONG phase6_wake_button_id;
  ULONG phase6_wake_pin;
  ULONG phase6_wake_active_level;
  ULONG phase6_wake_tick;
  ULONG phase6_wake_stage;
  ULONG phase6_wake_exti_rpr1;
  ULONG phase6_wake_exti_fpr1;
  ULONG phase6_lpbam_display_enabled;
  ULONG phase6_lpbam_display_rows;
  ULONG phase6_lpbam_rebuild_count;
  ULONG phase6_lpbam_compile_variant;
  ULONG phase6_lpbam_prearm_status;
  ULONG phase6_lpbam_prearm_tick;
  ULONG phase6_lpbam_start_request_tick;
  ULONG phase6_lpbam_dma_started_tick;
  ULONG phase6_lpbam_lptim_started_tick;
  ULONG phase6_lpbam_prepare_status;
  ULONG phase6_lpbam_fill_status;
  ULONG phase6_lpbam_start_status;
  ULONG phase6_lpbam_dma_mode_after_link;
  ULONG phase6_lpbam_dma_state_after_link;
  ULONG phase6_lpbam_dma_error_after_link;
  ULONG phase6_lpbam_queue_head;
  ULONG phase6_lpbam_queue_first_circular;
  ULONG phase6_lpbam_queue_node_count;
  ULONG phase6_lpbam_queue_state_after_link;
  ULONG phase6_lpbam_queue_error_after_link;
  ULONG phase6_lpbam_queue_type_after_link;
  ULONG phase6_lpbam_dma_state_after_start;
  ULONG phase6_lpbam_dma_error_after_start;
  ULONG phase6_lpbam_queue_state_after_start;
  ULONG phase6_lpbam_queue_error_after_start;
  ULONG phase6_lpbam_dma_start_status;
  ULONG phase6_lpbam_dma_ccr_after_start;
  ULONG phase6_lpbam_dma_csr_after_start;
  ULONG phase6_lpbam_dma_cllr_after_start;
  ULONG phase6_lpbam_spi_cr1_after_dma_start;
  ULONG phase6_lpbam_spi_autocr_after_dma_start;
  ULONG phase6_lpbam_dma_state_before_stop;
  ULONG phase6_lpbam_dma_state_after_stop;
  ULONG phase6_lpbam_rcc_srdamr_before;
  ULONG phase6_lpbam_rcc_srdamr_after;
  ULONG phase6_lpbam_spi_autocr_before;
  ULONG phase6_lpbam_spi_autocr_after;
  ULONG phase6_lpbam_dma_csr_before_stop;
  ULONG phase6_lpbam_dma_csr_after_stop;
  ULONG phase6_lpbam_spi_sr_before_stop;
  ULONG phase6_lpbam_spi_sr_after_stop;
  ULONG phase6_lpbam_spi_state_before_stop;
  ULONG phase6_lpbam_spi_state_after_stop;
  ULONG phase6_lpbam_spi_error_after_stop;
  ULONG phase6_lpbam_lptim_start_status;
  ULONG phase6_lpbam_lptim_cr_before_stop;
  ULONG phase6_lpbam_lptim_cr_after_stop;
  ULONG phase6_lpbam_lptim_cfgr_before_stop;
  ULONG phase6_lpbam_lptim_cfgr_after_stop;
  ULONG phase6_lpbam_lptim_arr_before_stop;
  ULONG phase6_lpbam_lptim_arr_after_stop;
  ULONG phase6_lpbam_lptim_cmp_before_stop;
  ULONG phase6_lpbam_lptim_cmp_after_stop;
  ULONG phase6_lpbam_lptim_cnt_before_stop;
  ULONG phase6_lpbam_lptim_cnt_after_stop;
  ULONG phase6_lpbam_lptim_ccmr1_before_stop;
  ULONG phase6_lpbam_lptim_ccmr1_after_stop;
  ULONG phase6_lpbam_lptim_isr_before_stop;
  ULONG phase6_lpbam_lptim_isr_after_stop;
  ULONG phase6_lpbam_lptim_arr_wait_status;
  ULONG phase6_lpbam_lptim_cmp_wait_status;
  ULONG phase6_lpbam_abort_attempted;
  ULONG phase6_lpbam_lptim_stop_status;
  ULONG phase6_lpbam_dma_abort_status;
  ULONG phase6_lpbam_dma_unlink_status;
  ULONG phase6_lpbam_spi_abort_status;
  ULONG phase6_lpbam_dma_state_after_abort;
  ULONG phase6_lpbam_dma_error_after_abort;
  ULONG phase6_lpbam_dma_csr_after_abort;
  ULONG phase6_lpbam_spi_state_after_abort;
  ULONG phase6_lpbam_spi_error_after_abort;
  ULONG phase6_lpbam_spi_sr_after_abort;
  ULONG phase6_lpbam_lptim_cr_after_abort;
  ULONG phase6_lpbam_post_wake_marker_status;
  ULONG complete;
} PsPhase5ThreadXProbe;

typedef struct
{
  ULONG magic;
  ULONG phase;
  ULONG boot_rx_len;
  ULONG at_tx_status;
  ULONG at_rx_len;
  ULONG at_ok;
  ULONG upm_query_tx_status;
  ULONG upm_query_rx_len;
  ULONG upm_query_ok;
  ULONG upm_test_tx_status;
  ULONG upm_test_rx_len;
  ULONG upm_test_ok;
  ULONG command_list_tx_status;
  ULONG command_list_rx_len;
  ULONG command_list_ok;
  ULONG at_rx_word0;
  ULONG at_rx_word1;
  ULONG upm_query_rx_word0;
  ULONG upm_query_rx_word1;
  ULONG upm_test_rx_word0;
  ULONG upm_test_rx_word1;
  ULONG final_nrst_state;
  ULONG final_nrst_moder;
  ULONG final_nrst_odr;
  ULONG final_nrst_idr;
  ULONG complete;
  uint8_t command_list_rx[1024];
} PsNinaPowerProbe;

typedef struct
{
  ULONG magic;
  ULONG phase;
  ULONG tick_start;
  ULONG tick_peer;
  ULONG tick_data_done;
  ULONG peer_wait_loops;
  ULONG data_wait_loops;
  ULONG boot_rx_len;
  ULONG at_tx_status;
  ULONG at_rx_len;
  ULONG at_ok;
  ULONG setup_tx_status[9];
  ULONG setup_rx_len[9];
  ULONG setup_ok[9];
  ULONG phone_rx_len;
  ULONG phone_detected;
  ULONG phone_sps_detected;
  ULONG data_mode_tx_status;
  ULONG data_mode_rx_len;
  ULONG data_mode_ok;
  ULONG hello_tx_status;
  ULONG phone_data_rx_len;
  ULONG phone_echo_tx_count;
  ULONG phone_alive_tx_count;
  ULONG uart_flow_diag_mode;
  ULONG uart_reinit_status;
  ULONG flow_gpioa_moder[4];
  ULONG flow_gpiob_moder[4];
  ULONG flow_gpiob_odr[4];
  ULONG flow_gpiob_idr[4];
  ULONG flow_gpioc_moder[4];
  ULONG flow_gpioc_idr[4];
  ULONG flow_uart_cr1[4];
  ULONG flow_uart_cr2[4];
  ULONG flow_uart_cr3[4];
  ULONG flow_uart_isr[4];
  ULONG uart_state_after;
  ULONG uart_error_after;
  ULONG nrst_state_after;
  ULONG dtr_state_after;
  ULONG dsr_state_after;
  ULONG complete;
  uint8_t boot_rx[64];
  uint8_t at_rx[64];
  uint8_t setup_rx[9][96];
  uint8_t phone_rx[256];
  uint8_t data_mode_rx[64];
  uint8_t phone_data_rx[256];
} PsNinaSpsProbe;
typedef struct
{
  ULONG magic;
  ULONG phase;
  ULONG tick_start;
  ULONG tick_read_event;
  ULONG boot_rx_len;
  ULONG at_tx_status;
  ULONG at_rx_len;
  ULONG at_ok;
  ULONG enable_query_tx_status;
  ULONG enable_query_rx_len;
  ULONG enable_query_ok;
  ULONG uri_set_tx_status;
  ULONG uri_set_rx_len;
  ULONG uri_set_ok;
  ULONG uri_query_tx_status;
  ULONG uri_query_rx_len;
  ULONG uri_query_ok;
  ULONG enable_uri_tx_status;
  ULONG enable_uri_rx_len;
  ULONG enable_uri_ok;
  ULONG enable_verify_tx_status;
  ULONG enable_verify_rx_len;
  ULONG enable_verify_ok;
  ULONG read_event_rx_len;
  ULONG read_event_detected;
  ULONG read_event_wait_loops;
  ULONG uart_state_after;
  ULONG uart_error_after;
  ULONG nrst_state_after;
  ULONG complete;
  uint8_t boot_rx[64];
  uint8_t at_rx[64];
  uint8_t enable_query_rx[96];
  uint8_t uri_set_rx[96];
  uint8_t uri_query_rx[128];
  uint8_t enable_uri_rx[96];
  uint8_t enable_verify_rx[96];
  uint8_t read_event_rx[128];
} PsNinaNfcProbe;

typedef struct
{
  ULONG magic;
  ULONG phase;
  ULONG tick_start;
  ULONG tick_ustop_enter;
  ULONG tick_ustop_done;
  ULONG tick_dtr_stop_enter;
  ULONG tick_dtr_stop_done;
  ULONG boot_rx_len;
  ULONG at_tx_status;
  ULONG at_rx_len;
  ULONG at_ok;
  ULONG upwrreg_query_tx_status;
  ULONG upwrreg_query_rx_len;
  ULONG upwrreg_query_ok;
  ULONG pwrmng_min_tx_status;
  ULONG pwrmng_min_rx_len;
  ULONG pwrmng_min_ok;
  ULONG pwrmng_max_tx_status;
  ULONG pwrmng_max_rx_len;
  ULONG pwrmng_max_ok;
  ULONG bt_discoverable_off_tx_status;
  ULONG bt_discoverable_off_rx_len;
  ULONG bt_discoverable_off_ok;
  ULONG bt_connectable_off_tx_status;
  ULONG bt_connectable_off_rx_len;
  ULONG bt_connectable_off_ok;
  ULONG bt_pairing_off_tx_status;
  ULONG bt_pairing_off_rx_len;
  ULONG bt_pairing_off_ok;
  ULONG ustop_tx_status;
  ULONG ustop_rx_len;
  ULONG ustop_ok;
  ULONG ustop_startup_seen;
  ULONG post_ustop_at_tx_status;
  ULONG post_ustop_at_rx_len;
  ULONG post_ustop_at_ok;
  ULONG dtr_asserted_before_set_state;
  ULONG dtr_uartoff_set_tx_status;
  ULONG dtr_uartoff_set_rx_len;
  ULONG dtr_uartoff_set_ok;
  ULONG dtr3_deasserted_state;
  ULONG dtr3_at_while_deasserted_tx_status;
  ULONG dtr3_at_while_deasserted_rx_len;
  ULONG dtr3_at_while_deasserted_ok;
  ULONG dtr3_wake_asserted_state;
  ULONG dtr3_post_wake_at_tx_status;
  ULONG dtr3_post_wake_at_rx_len;
  ULONG dtr3_post_wake_at_ok;
  ULONG dtr_set_tx_status;
  ULONG dtr_set_rx_len;
  ULONG dtr_set_ok;
  ULONG dtr_stop_hold_active;
  ULONG dtr_stop_hold_loops;
  ULONG dtr_deasserted_state;
  ULONG dtr_wake_asserted_state;
  ULONG dtr_wake_rx_len;
  ULONG dtr_wake_startup_seen;
  ULONG reset_attrib_hold_active;
  ULONG reset_attrib_hold_loops;
  ULONG reset_attrib_tick_enter;
  ULONG reset_attrib_tick_done;
  ULONG reset_attrib_nrst_state;
  ULONG post_dtr_at_tx_status;
  ULONG post_dtr_at_rx_len;
  ULONG post_dtr_at_ok;
  ULONG gpioa_moder_after;
  ULONG gpiob_moder_after;
  ULONG gpioc_moder_after;
  ULONG gpioc_odr_after;
  ULONG gpioc_idr_after;
  ULONG uart_state_after;
  ULONG uart_error_after;
  ULONG nrst_state_after;
  ULONG dtr_state_after;
  ULONG dsr_state_after;
  ULONG complete;
  uint8_t boot_rx[64];
  uint8_t at_rx[64];
  uint8_t upwrreg_query_rx[96];
  uint8_t pwrmng_min_rx[64];
  uint8_t pwrmng_max_rx[64];
  uint8_t bt_discoverable_off_rx[64];
  uint8_t bt_connectable_off_rx[64];
  uint8_t bt_pairing_off_rx[64];
  uint8_t ustop_rx[128];
  uint8_t post_ustop_at_rx[64];
  uint8_t dtr_uartoff_set_rx[64];
  uint8_t dtr3_at_while_deasserted_rx[64];
  uint8_t dtr3_post_wake_at_rx[64];
  uint8_t dtr_set_rx[64];
  uint8_t dtr_wake_rx[128];
  uint8_t post_dtr_at_rx[64];
} PsNinaSleepProbe;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define PS_THREADX_OWNER_COUNT          7U
#define PS_THREADX_OWNER_STACK_BYTES    1024U
#define PS_THREADX_OWNER_STACK_WORDS    (PS_THREADX_OWNER_STACK_BYTES / sizeof(ULONG))

#define PS_THREADX_OWNER_POWER          0U
#define PS_THREADX_OWNER_DISPLAY        1U
#define PS_THREADX_OWNER_STORAGE        2U
#define PS_THREADX_OWNER_INPUT          3U
#define PS_THREADX_OWNER_COMMS          4U
#define PS_THREADX_OWNER_UI             5U
#define PS_THREADX_OWNER_AUDIO          6U

#define PS_INPUT_RAW_QUEUE_DEPTH        16U
#define PS_INPUT_RAW_EVENT_WORDS        4U
#define PS_INPUT_DEBOUNCE_TICKS         20U

#define PS_UI_EVENT_QUEUE_DEPTH         16U
#define PS_UI_EVENT_WORDS               5U
#define PS_UI_EVENT_BUTTON_EDGE         1U
#define PS_UI_EVENT_BUTTON_CHORD        2U

#define PS_DISPLAY_CMD_QUEUE_DEPTH      16U
#define PS_DISPLAY_CMD_WORDS            5U
#define PS_DISPLAY_CMD_ACTIVITY_HINT    1U
#define PS_DISPLAY_CMD_DIAG_FILL        2U
#define PS_DISPLAY_DMA_TIMEOUT_MS       1000U
#define PS_DISPLAY_QUEUE_TIMEOUT_TICKS  100U

#define PS_STORAGE_CMD_QUEUE_DEPTH      16U
#define PS_STORAGE_CMD_WORDS            5U
#define PS_STORAGE_CMD_ACTIVITY_HINT    1U

#define PS_COMMS_CMD_QUEUE_DEPTH        16U
#define PS_COMMS_CMD_WORDS              5U
#define PS_COMMS_CMD_ACTIVITY_HINT      1U

#define PS_AUDIO_CMD_QUEUE_DEPTH        16U
#define PS_AUDIO_CMD_WORDS              5U
#define PS_AUDIO_CMD_ACTIVITY_HINT      1U
#define PS_AUDIO_BBB_CUE_HZ             2000U
#define PS_AUDIO_BBB_CUE_TICKS          5U
#define PS_AUDIO_SPEAKER_SAMPLE_RATE_HZ 16000U
#define PS_AUDIO_SPEAKER_CUE_HZ         1000U
#define PS_AUDIO_SPEAKER_CUE_AMPLITUDE  3000
#define PS_AUDIO_SPEAKER_CUE_TICKS      20U
#define PS_AUDIO_SPEAKER_AMP_SETTLE_TICKS 2U
#define PS_AUDIO_SPEAKER_BUFFER_FRAMES  256U
#define PS_AUDIO_SPEAKER_BUFFER_HALFWORDS (PS_AUDIO_SPEAKER_BUFFER_FRAMES * 2U)

#define PS_PHASE6_QUIESCE_DELAY_TICKS   200U
#define PS_PHASE6_QUIESCE_ACK_TIMEOUT_TICKS 500U
#define PS_PHASE6_STAGE_HOLD_TICKS      500U
#define PS_PHASE6_STAGE_TRIGGERED       1U
#define PS_PHASE6_STAGE_OWNERS_QUIESCED 2U
#define PS_PHASE6_STAGE_EXTERNAL_SLEEP  3U
#define PS_PHASE6_STAGE_PERIPHERALS_OFF 4U
#define PS_PHASE6_STAGE_STOP2           5U
#define PS_PHASE6_STAGE_DIAG_3V3_OFF    6U
#define PS_PHASE6_DIAG_CUT_3V3_RAIL     0U
#define PS_PHASE6_LPBAM_DISPLAY_EXPERIMENT 1U
#define PS_PHASE6_LPBAM_START_AUTONOMOUS 1U
#define PS_PHASE6_LPBAM_LATENCY_ONLY 1U
#define PS_PHASE6_DEBUG_IN_STOP         1U
#define PS_PHASE6_LPBAM_DISPLAY_START_ROW  1U
#define PS_PHASE6_LPBAM_DISPLAY_ROWS       DISPLAY_HEIGHT
#define PS_PHASE6_LPBAM_LPTIM_250MS_ARR    7812U
#define PS_PHASE6_LPBAM_LPTIM_250MS_CMP    3906U
#define PS_PHASE6_BUTTON_EXTI_LINES       (BTN_A_Pin | BTN_B_Pin | BTN_L_Pin | BTN_R_Pin)
#define PS_PHASE6_WAKE_REASON_NONE        0U
#define PS_PHASE6_WAKE_REASON_INPUT       1U
#define PS_PHASE6_WAKE_REASON_UNKNOWN     0xFFFFFFFFUL
#define PS_PHASE6_NINA_RESET_ASSERT_TICKS 2U
#define PS_PHASE6_NINA_BOOT_WAIT_TICKS  75U
#define PS_PHASE6_NINA_BOOT_DRAIN_TICKS 500U
#define PS_PHASE6_NINA_AT_RX_TICKS      100U
#define PS_PHASE6_NINA_RX_QUIET_TICKS   10U
#define PS_PHASE6_NINA_POWEROFF_WAIT_TICKS 100U
#define PS_PHASE6_OWNER_ACK_MASK(owner_id) (1UL << (owner_id))
#define PS_PHASE6_ALL_OWNER_ACK_MASK    ((1UL << PS_THREADX_OWNER_COUNT) - 1UL)
#define PS_PHASE6_I2C_TIMEOUT_MS        25U
#define PS_PHASE6_OSPI_TIMEOUT_MS       100U
#define PS_PHASE6_LIS2DUX12_ADDR        (0x18U << 1)
#define PS_PHASE6_TMAG3001_ADDR         (0x34U << 1)
#define PS_PHASE6_AT25_CMD_DPD          0xB9U
#define PS_NINA_POWER_PROBE_MAGIC        0x504E5057UL
#define PS_NINA_POWER_PROBE_PHASE        0x00007100UL
#define PS_NINA_SPS_PROBE_MAGIC          0x504E5350UL
#define PS_NINA_SPS_PROBE_PHASE          0x00007200UL
#define PS_NINA_NFC_PROBE_MAGIC          0x504E4643UL
#define PS_NINA_NFC_PROBE_PHASE          0x00007300UL
#define PS_NINA_SLEEP_PROBE_MAGIC        0x504E534CUL
#define PS_NINA_SLEEP_PROBE_PHASE        0x00007400UL
#define PS_NINA_SPS_CONNECT_WAIT_TICKS    30000U
#define PS_NINA_SPS_DATA_WAIT_TICKS       30000U
#define PS_NINA_SPS_POST_ATO_DRAIN_TICKS  50U
#define PS_NINA_SPS_FLOW_DIAG_MODE        0U
#define PS_NINA_NFC_READ_WAIT_TICKS       300000U
#define PS_NINA_SLEEP_USTOP_TIMEOUT_MS    5000U
#define PS_NINA_SLEEP_DTR_STOP_HOLD_TICKS 30000U
#define PS_NINA_SLEEP_RESET_ATTRIB_HOLD_TICKS 20000U
#define PS_NINA_SLEEP_BOOT_QUIET_TICKS 100U
#define PS_NINA_SLEEP_DISPLAY_MARK_DTR_STOP   0xD400UL
#define PS_NINA_SLEEP_DISPLAY_MARK_RESET_HELD 0xE500UL
#define PS_NINA_SLEEP_DISPLAY_MARK_DONE       0x0D0EUL
#define PS_NINA_CAPABILITY_BOOT_WAIT_TICKS 250U
#define PS_NINA_CAPABILITY_LIST_RX_TICKS  500U

#define PS_INPUT_BUTTON_START           1U
#define PS_INPUT_BUTTON_A               2U
#define PS_INPUT_BUTTON_B               3U
#define PS_INPUT_BUTTON_L               4U
#define PS_INPUT_BUTTON_R               5U

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */
volatile PsPhase5ThreadXProbe g_ps_phase5_threadx_probe;
volatile PsNinaPowerProbe g_ps_nina_power_probe;
volatile PsNinaSpsProbe g_ps_nina_sps_probe;
volatile PsNinaNfcProbe g_ps_nina_nfc_probe;
volatile PsNinaSleepProbe g_ps_nina_sleep_probe;

static TX_THREAD ps_threadx_owner_threads[PS_THREADX_OWNER_COUNT];
static ULONG ps_threadx_owner_stacks[PS_THREADX_OWNER_COUNT][PS_THREADX_OWNER_STACK_WORDS];
static LS013B7DH05 ps_threadx_lcd;
static PsDisplayRenderer ps_threadx_display_renderer;
static TX_QUEUE ps_input_raw_queue;
static ULONG ps_input_raw_queue_storage[PS_INPUT_RAW_QUEUE_DEPTH * PS_INPUT_RAW_EVENT_WORDS];
static TX_QUEUE ps_ui_event_queue;
static ULONG ps_ui_event_queue_storage[PS_UI_EVENT_QUEUE_DEPTH * PS_UI_EVENT_WORDS];
static TX_QUEUE ps_display_cmd_queue;
static ULONG ps_display_cmd_queue_storage[PS_DISPLAY_CMD_QUEUE_DEPTH * PS_DISPLAY_CMD_WORDS];
static TX_QUEUE ps_storage_cmd_queue;
static ULONG ps_storage_cmd_queue_storage[PS_STORAGE_CMD_QUEUE_DEPTH * PS_STORAGE_CMD_WORDS];
static TX_QUEUE ps_comms_cmd_queue;
static ULONG ps_comms_cmd_queue_storage[PS_COMMS_CMD_QUEUE_DEPTH * PS_COMMS_CMD_WORDS];
static TX_QUEUE ps_audio_cmd_queue;
static ULONG ps_audio_cmd_queue_storage[PS_AUDIO_CMD_QUEUE_DEPTH * PS_AUDIO_CMD_WORDS];
static ULONG ps_input_stable_level[5];
static ULONG ps_input_last_accept_tick[5];
static uint16_t ps_audio_speaker_dma_buffer[PS_AUDIO_SPEAKER_BUFFER_HALFWORDS];
static volatile ULONG ps_phase6_sleep_trigger_requested;
static ULONG ps_phase6_lpbam_rebuild_count;
static uint8_t ps_phase6_lpbam_prearmed;
static volatile ULONG ps_phase6_quiesce_requested;
static uint8_t ps_display_renderer_ready;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */
static void PS_ThreadXOwnerEntry(ULONG owner_id);
static void PS_PowerOwnerEntry(void);
static void PS_InputOwnerEntry(void);
static void PS_UIOwnerEntry(void);
static void PS_DisplayOwnerEntry(void);
static HAL_StatusTypeDef PS_DisplayOwnerRunActivityHint(void);
static HAL_StatusTypeDef PS_DisplayOwnerEnsureReady(void);
static HAL_StatusTypeDef PS_DisplayOwnerFill(uint8_t fill_value);
static HAL_StatusTypeDef PS_DisplayOwnerPresentAuthoredFrameA(void);
static HAL_StatusTypeDef PS_DisplayOwnerBlankForSleep(void);
static void PS_StorageOwnerEntry(void);
static void PS_CommsOwnerEntry(void);
static void PS_CommsRunNinaPowerCapabilityProbe(void);
static void PS_CommsRunNinaSpsProbe(void);
static void PS_CommsRunNinaNfcProbe(void);
static void PS_CommsRunNinaSleepProbe(void);
static void PS_CommsSampleNinaSpsFlow(volatile PsNinaSpsProbe *probe, uint32_t index);
static HAL_StatusTypeDef PS_CommsConfigureNinaSpsFlowDiag(volatile PsNinaSpsProbe *probe);
static void PS_AudioOwnerEntry(void);
static HAL_StatusTypeDef PS_AudioOwnerRunActivityHint(void);
static void PS_AudioOwnerPrepareSpeakerBuffer(int16_t amplitude, uint32_t frequency_hz);
static void PS_InputPublishUIEvent(ULONG event_type,
                                   ULONG button_id,
                                   ULONG active_level,
                                   ULONG mask,
                                   ULONG event_tick);
static void PS_UIPublishDisplayCmd(ULONG source_event,
                                   ULONG button_id,
                                   ULONG mask,
                                   ULONG event_tick);
static void PS_PublishDisplayDiagFill(uint8_t fill_value, ULONG marker);
static void PS_UIPublishStorageCmd(ULONG source_event,
                                   ULONG button_id,
                                   ULONG mask,
                                   ULONG event_tick);
static void PS_UIPublishCommsCmd(ULONG source_event,
                                 ULONG button_id,
                                 ULONG mask,
                                 ULONG event_tick);
static void PS_UIPublishAudioCmd(ULONG source_event,
                                 ULONG button_id,
                                 ULONG mask,
                                 ULONG event_tick);

static HAL_StatusTypeDef PS_PowerOwnerConfigureRails(void);
static HAL_StatusTypeDef PS_PowerOwnerReadRailStatus(ULONG before);
static void PS_Phase6_PrepareExternalDevicesForSleep(void);
static HAL_StatusTypeDef PS_Phase6_AT25CommandOnly(uint8_t instruction);
static int32_t PS_Phase6_LIS2DUX12_ReadReg(void *handle, uint8_t reg, uint8_t *data, uint16_t len);
static int32_t PS_Phase6_LIS2DUX12_WriteReg(void *handle, uint8_t reg, const uint8_t *data, uint16_t len);
static HAL_StatusTypeDef PS_Phase6_TMAG3001_ReadReg(uint8_t reg, uint8_t *data);
static HAL_StatusTypeDef PS_Phase6_TMAG3001_WriteReg(uint8_t reg, uint8_t data);
static void PS_Phase6_PrepareNinaForSleep(void);
static uint16_t PS_Phase6_NinaReceiveUntilQuiet(uint8_t *buffer,
                                                uint16_t capacity,
                                                ULONG max_ticks,
                                                ULONG quiet_ticks);
static ULONG PS_Phase6_PackBytes(const uint8_t *data, uint16_t len, uint16_t offset);
static ULONG PS_Phase6_BufferContainsToken(const uint8_t *data, uint16_t len, const char *token);
static void PS_Phase6_EnablePmicPulseStopForSleep(void);
static void PS_Phase6_DiagnosticCut3V3Rail(void);
#if PS_PHASE6_LPBAM_START_AUTONOMOUS
static HAL_StatusTypeDef PS_Phase6_PrearmLpbamDisplayStopTransfer(void);
static HAL_StatusTypeDef PS_Phase6_PrepareLpbamDisplayStopTransfer(void);
static HAL_StatusTypeDef PS_Phase6_WaitLptimFlag(uint32_t flag);
#endif
#if PS_PHASE6_LPBAM_START_AUTONOMOUS
static HAL_StatusTypeDef PS_Phase6_EnableLpbamDisplayAutonomousClocks(void);
#endif
static void PS_Phase6_RecordLpbamDisplayStopState(uint8_t after_stop);
static void PS_Phase6_StopLpbamDisplayAfterWake(void);
static void PS_Phase6_ResumeAfterStop2Wake(void);
static HAL_StatusTypeDef PS_Phase6_RestoreDisplaySpiAfterLpbam(void);
static void PS_Phase6_RearmButtonWakeInputs(void);
static void PS_Phase6_ParkNinaInterfacePins(void);
static void PS_Phase6_RecordStage(ULONG stage);
static void PS_Phase6_HoldStage(ULONG ticks);
static void PS_Phase6_WaitForOwnerQuiesceAcks(void);
static void PS_Phase6_DeInitIdlePeripherals(void);
static void PS_Phase6_EnterStop2ForCurrentTest(void);
static void PS_ThreadXRecordPerformance(void);
static ULONG PS_InputButtonIdFromPin(uint16_t gpio_pin);

extern ADC_HandleTypeDef hadc1;
extern LPTIM_HandleTypeDef hlptim1;
extern UART_HandleTypeDef hlpuart1;
extern OSPI_HandleTypeDef hospi1;
extern SAI_HandleTypeDef hsai_BlockA1;
extern SPI_HandleTypeDef hspi3;
extern DMA_HandleTypeDef handle_LPDMA1_Channel0;
extern DMA_QListTypeDef Queue1_Q;

/* USER CODE END PFP */

/**
  * @brief  Application ThreadX Initialization.
  * @param memory_ptr: memory pointer
  * @retval int
  */
UINT App_ThreadX_Init(VOID *memory_ptr)
{
  UINT ret = TX_SUCCESS;

  /* USER CODE BEGIN App_ThreadX_MEM_POOL */
  if (memory_ptr != TX_NULL)
  {
    CHAR *pool_name = TX_NULL;
    TX_THREAD *first_suspended = TX_NULL;
    TX_BYTE_POOL *next_pool = TX_NULL;
    ULONG suspended_count = 0U;

    (void)tx_byte_pool_info_get((TX_BYTE_POOL *)memory_ptr,
                                &pool_name,
                                (ULONG *)&g_ps_phase5_threadx_probe.pool_available_bytes,
                                (ULONG *)&g_ps_phase5_threadx_probe.pool_fragments,
                                &first_suspended,
                                &suspended_count,
                                &next_pool);
    (void)pool_name;
    (void)first_suspended;
    (void)suspended_count;
    (void)next_pool;
  }

  /* USER CODE END App_ThreadX_MEM_POOL */

  /* USER CODE BEGIN App_ThreadX_Init */
  g_ps_phase5_threadx_probe.magic = 0x50355458U;
  g_ps_phase5_threadx_probe.phase = 0x5100U;
  g_ps_phase5_threadx_probe.init_time = tx_time_get();

  g_ps_phase5_threadx_probe.input_queue_create_status =
      tx_queue_create(&ps_input_raw_queue,
                      "qInputRaw",
                      PS_INPUT_RAW_EVENT_WORDS,
                      ps_input_raw_queue_storage,
                      sizeof(ps_input_raw_queue_storage));
  g_ps_phase5_threadx_probe.input_queue_ready =
      (g_ps_phase5_threadx_probe.input_queue_create_status == TX_SUCCESS) ? 1U : 0U;
  g_ps_phase5_threadx_probe.input_debounce_ticks = PS_INPUT_DEBOUNCE_TICKS;

  g_ps_phase5_threadx_probe.ui_queue_create_status =
      tx_queue_create(&ps_ui_event_queue,
                      "qUIEvents",
                      PS_UI_EVENT_WORDS,
                      ps_ui_event_queue_storage,
                      sizeof(ps_ui_event_queue_storage));
  g_ps_phase5_threadx_probe.ui_queue_ready =
      (g_ps_phase5_threadx_probe.ui_queue_create_status == TX_SUCCESS) ? 1U : 0U;

  g_ps_phase5_threadx_probe.display_queue_create_status =
      tx_queue_create(&ps_display_cmd_queue,
                      "qDisplayCmd",
                      PS_DISPLAY_CMD_WORDS,
                      ps_display_cmd_queue_storage,
                      sizeof(ps_display_cmd_queue_storage));
  g_ps_phase5_threadx_probe.display_queue_ready =
      (g_ps_phase5_threadx_probe.display_queue_create_status == TX_SUCCESS) ? 1U : 0U;
  g_ps_phase5_threadx_probe.storage_queue_create_status =
      tx_queue_create(&ps_storage_cmd_queue,
                      "qStorageCmd",
                      PS_STORAGE_CMD_WORDS,
                      ps_storage_cmd_queue_storage,
                      sizeof(ps_storage_cmd_queue_storage));
  g_ps_phase5_threadx_probe.storage_queue_ready =
      (g_ps_phase5_threadx_probe.storage_queue_create_status == TX_SUCCESS) ? 1U : 0U;

  g_ps_phase5_threadx_probe.comms_queue_create_status =
      tx_queue_create(&ps_comms_cmd_queue,
                      "qCommsCmd",
                      PS_COMMS_CMD_WORDS,
                      ps_comms_cmd_queue_storage,
                      sizeof(ps_comms_cmd_queue_storage));
  g_ps_phase5_threadx_probe.comms_queue_ready =
      (g_ps_phase5_threadx_probe.comms_queue_create_status == TX_SUCCESS) ? 1U : 0U;

  g_ps_phase5_threadx_probe.audio_queue_create_status =
      tx_queue_create(&ps_audio_cmd_queue,
                      "qAudioCmd",
                      PS_AUDIO_CMD_WORDS,
                      ps_audio_cmd_queue_storage,
                      sizeof(ps_audio_cmd_queue_storage));
  g_ps_phase5_threadx_probe.audio_queue_ready =
      (g_ps_phase5_threadx_probe.audio_queue_create_status == TX_SUCCESS) ? 1U : 0U;

  g_ps_phase5_threadx_probe.create_status[PS_THREADX_OWNER_POWER] =
      tx_thread_create(&ps_threadx_owner_threads[PS_THREADX_OWNER_POWER],
                       "ps_power_owner",
                       PS_ThreadXOwnerEntry,
                       PS_THREADX_OWNER_POWER,
                       ps_threadx_owner_stacks[PS_THREADX_OWNER_POWER],
                       PS_THREADX_OWNER_STACK_BYTES,
                       5U,
                       5U,
                       TX_NO_TIME_SLICE,
                       TX_AUTO_START);

  g_ps_phase5_threadx_probe.create_status[PS_THREADX_OWNER_DISPLAY] =
      tx_thread_create(&ps_threadx_owner_threads[PS_THREADX_OWNER_DISPLAY],
                       "ps_display_owner",
                       PS_ThreadXOwnerEntry,
                       PS_THREADX_OWNER_DISPLAY,
                       ps_threadx_owner_stacks[PS_THREADX_OWNER_DISPLAY],
                       PS_THREADX_OWNER_STACK_BYTES,
                       8U,
                       8U,
                       TX_NO_TIME_SLICE,
                       TX_AUTO_START);

  g_ps_phase5_threadx_probe.create_status[PS_THREADX_OWNER_STORAGE] =
      tx_thread_create(&ps_threadx_owner_threads[PS_THREADX_OWNER_STORAGE],
                       "ps_storage_owner",
                       PS_ThreadXOwnerEntry,
                       PS_THREADX_OWNER_STORAGE,
                       ps_threadx_owner_stacks[PS_THREADX_OWNER_STORAGE],
                       PS_THREADX_OWNER_STACK_BYTES,
                       9U,
                       9U,
                       TX_NO_TIME_SLICE,
                       TX_AUTO_START);

  g_ps_phase5_threadx_probe.create_status[PS_THREADX_OWNER_INPUT] =
      tx_thread_create(&ps_threadx_owner_threads[PS_THREADX_OWNER_INPUT],
                       "ps_input_owner",
                       PS_ThreadXOwnerEntry,
                       PS_THREADX_OWNER_INPUT,
                       ps_threadx_owner_stacks[PS_THREADX_OWNER_INPUT],
                       PS_THREADX_OWNER_STACK_BYTES,
                       7U,
                       7U,
                       TX_NO_TIME_SLICE,
                       TX_AUTO_START);

  g_ps_phase5_threadx_probe.create_status[PS_THREADX_OWNER_COMMS] =
      tx_thread_create(&ps_threadx_owner_threads[PS_THREADX_OWNER_COMMS],
                       "ps_comms_owner",
                       PS_ThreadXOwnerEntry,
                       PS_THREADX_OWNER_COMMS,
                       ps_threadx_owner_stacks[PS_THREADX_OWNER_COMMS],
                       PS_THREADX_OWNER_STACK_BYTES,
                       10U,
                       10U,
                       TX_NO_TIME_SLICE,
                       TX_AUTO_START);

  g_ps_phase5_threadx_probe.create_status[PS_THREADX_OWNER_UI] =
      tx_thread_create(&ps_threadx_owner_threads[PS_THREADX_OWNER_UI],
                       "ps_ui_owner",
                       PS_ThreadXOwnerEntry,
                       PS_THREADX_OWNER_UI,
                       ps_threadx_owner_stacks[PS_THREADX_OWNER_UI],
                       PS_THREADX_OWNER_STACK_BYTES,
                       11U,
                       11U,
                       TX_NO_TIME_SLICE,
                       TX_AUTO_START);

  g_ps_phase5_threadx_probe.create_status[PS_THREADX_OWNER_AUDIO] =
      tx_thread_create(&ps_threadx_owner_threads[PS_THREADX_OWNER_AUDIO],
                       "ps_audio_owner",
                       PS_ThreadXOwnerEntry,
                       PS_THREADX_OWNER_AUDIO,
                       ps_threadx_owner_stacks[PS_THREADX_OWNER_AUDIO],
                       PS_THREADX_OWNER_STACK_BYTES,
                       6U,
                       6U,
                       TX_NO_TIME_SLICE,
                       TX_AUTO_START);

  for (UINT i = 0U; i < PS_THREADX_OWNER_COUNT; ++i)
  {
    if (g_ps_phase5_threadx_probe.create_status[i] != TX_SUCCESS)
    {
      ret = g_ps_phase5_threadx_probe.create_status[i];
      break;
    }
  }
  if (g_ps_phase5_threadx_probe.input_queue_create_status != TX_SUCCESS)
  {
    ret = g_ps_phase5_threadx_probe.input_queue_create_status;
  }
  if (g_ps_phase5_threadx_probe.ui_queue_create_status != TX_SUCCESS)
  {
    ret = g_ps_phase5_threadx_probe.ui_queue_create_status;
  }
  if (g_ps_phase5_threadx_probe.display_queue_create_status != TX_SUCCESS)
  {
    ret = g_ps_phase5_threadx_probe.display_queue_create_status;
  }
  if (g_ps_phase5_threadx_probe.storage_queue_create_status != TX_SUCCESS)
  {
    ret = g_ps_phase5_threadx_probe.storage_queue_create_status;
  }
  if (g_ps_phase5_threadx_probe.comms_queue_create_status != TX_SUCCESS)
  {
    ret = g_ps_phase5_threadx_probe.comms_queue_create_status;
  }
  if (g_ps_phase5_threadx_probe.audio_queue_create_status != TX_SUCCESS)
  {
    ret = g_ps_phase5_threadx_probe.audio_queue_create_status;
  }

  g_ps_phase5_threadx_probe.complete = (ret == TX_SUCCESS) ? 1U : 0U;
  /* USER CODE END App_ThreadX_Init */

  return ret;
}

  /**
  * @brief  Function that implements the kernel's initialization.
  * @param  None
  * @retval None
  */
void MX_ThreadX_Init(void)
{
  /* USER CODE BEGIN  Before_Kernel_Start */

  /* USER CODE END  Before_Kernel_Start */

  tx_kernel_enter();

  /* USER CODE BEGIN  Kernel_Start_Error */

  /* USER CODE END  Kernel_Start_Error */
}

/* USER CODE BEGIN 1 */
UINT PS_ThreadXPostButtonEdgeFromISR(uint16_t gpio_pin, uint8_t active_level)
{
  ULONG button_id = PS_InputButtonIdFromPin(gpio_pin);
  ULONG event[PS_INPUT_RAW_EVENT_WORDS];
  UINT status;

  g_ps_phase5_threadx_probe.phase6_exti_callback_count++;
  g_ps_phase5_threadx_probe.phase6_exti_callback_pin = (ULONG)gpio_pin;
  g_ps_phase5_threadx_probe.phase6_exti_callback_button_id = button_id;
  g_ps_phase5_threadx_probe.phase6_exti_callback_active_level =
      (active_level != 0U) ? 1U : 0U;
  g_ps_phase5_threadx_probe.phase6_exti_callback_tick = tx_time_get();
  g_ps_phase5_threadx_probe.phase6_exti_callback_stage =
      g_ps_phase5_threadx_probe.phase6_stage;
  g_ps_phase5_threadx_probe.phase6_exti_callback_gpioa_idr = GPIOA->IDR;
  g_ps_phase5_threadx_probe.phase6_exti_callback_gpiob_idr = GPIOB->IDR;

  if ((g_ps_phase5_threadx_probe.phase6_stage == PS_PHASE6_STAGE_STOP2) &&
      (g_ps_phase5_threadx_probe.phase6_wake_reason == PS_PHASE6_WAKE_REASON_NONE))
  {
    g_ps_phase5_threadx_probe.phase6_wake_reason =
        (button_id != 0U) ? PS_PHASE6_WAKE_REASON_INPUT : PS_PHASE6_WAKE_REASON_UNKNOWN;
    g_ps_phase5_threadx_probe.phase6_wake_button_id = button_id;
    g_ps_phase5_threadx_probe.phase6_wake_pin = (ULONG)gpio_pin;
    g_ps_phase5_threadx_probe.phase6_wake_active_level = (active_level != 0U) ? 1U : 0U;
    g_ps_phase5_threadx_probe.phase6_wake_tick = g_ps_phase5_threadx_probe.phase6_exti_callback_tick;
    g_ps_phase5_threadx_probe.phase6_wake_stage = g_ps_phase5_threadx_probe.phase6_stage;
    g_ps_phase5_threadx_probe.phase6_wake_exti_rpr1 = EXTI->RPR1;
    g_ps_phase5_threadx_probe.phase6_wake_exti_fpr1 = EXTI->FPR1;
  }

  if ((button_id == 0U) || (g_ps_phase5_threadx_probe.input_queue_ready == 0U))
  {
    g_ps_phase5_threadx_probe.input_queue_not_ready_drop++;
    return TX_QUEUE_ERROR;
  }

  event[0] = g_ps_phase5_threadx_probe.input_queue_sent + 1U;
  event[1] = button_id;
  event[2] = (active_level != 0U) ? 1U : 0U;
  event[3] = tx_time_get();

  status = tx_queue_send(&ps_input_raw_queue, event, TX_NO_WAIT);
  g_ps_phase5_threadx_probe.input_last_send_status = status;

  if (status == TX_SUCCESS)
  {
    g_ps_phase5_threadx_probe.input_queue_sent++;
  }
  else
  {
    g_ps_phase5_threadx_probe.input_queue_send_fail++;
  }

  return status;
}

static void PS_ThreadXOwnerEntry(ULONG owner_id)
{
  if (owner_id >= PS_THREADX_OWNER_COUNT)
  {
    tx_thread_suspend(tx_thread_identify());
  }

  g_ps_phase5_threadx_probe.start_mask |= (1UL << owner_id);

  if (owner_id == PS_THREADX_OWNER_INPUT)
  {
    PS_InputOwnerEntry();
  }
  if (owner_id == PS_THREADX_OWNER_POWER)
  {
    PS_PowerOwnerEntry();
  }
  if (owner_id == PS_THREADX_OWNER_DISPLAY)
  {
    PS_DisplayOwnerEntry();
  }
  if (owner_id == PS_THREADX_OWNER_STORAGE)
  {
    PS_StorageOwnerEntry();
  }
  if (owner_id == PS_THREADX_OWNER_UI)
  {
    PS_UIOwnerEntry();
  }
  if (owner_id == PS_THREADX_OWNER_COMMS)
  {
    PS_CommsOwnerEntry();
  }
  if (owner_id == PS_THREADX_OWNER_AUDIO)
  {
    PS_AudioOwnerEntry();
  }

  for (;;)
  {
    g_ps_phase5_threadx_probe.heartbeat[owner_id]++;
    g_ps_phase5_threadx_probe.last_time[owner_id] = tx_time_get();

    if (owner_id == PS_THREADX_OWNER_POWER)
    {
      PS_ThreadXRecordPerformance();
    }

    tx_thread_sleep(100U + (owner_id * 25U));
  }
}

static void PS_PowerOwnerEntry(void)
{
  HAL_StatusTypeDef status;

  g_ps_phase5_threadx_probe.power_action_count++;

  status = ADP5360_init();
  g_ps_phase5_threadx_probe.power_pmic_init_status = (ULONG)status;
  if (status == HAL_OK)
  {
    status = PS_PowerOwnerReadRailStatus(1U);
    g_ps_phase5_threadx_probe.power_read_before_status = (ULONG)status;
  }

  if (status == HAL_OK)
  {
    status = PS_PowerOwnerConfigureRails();
    g_ps_phase5_threadx_probe.power_rail_config_status = (ULONG)status;
  }

  tx_thread_sleep(20U);

  if (status == HAL_OK)
  {
    status = PS_PowerOwnerReadRailStatus(0U);
    g_ps_phase5_threadx_probe.power_read_after_status = (ULONG)status;
  }

  if (status != HAL_OK)
  {
    g_ps_phase5_threadx_probe.power_action_error_count++;
    g_ps_phase5_threadx_probe.power_i2c_error_after = HAL_I2C_GetError(&hi2c3);
  }

  g_ps_phase5_threadx_probe.power_rails_ready =
      ((g_ps_phase5_threadx_probe.power_pgood_after &
        (ADP5360_PG_VOUT1OK | ADP5360_PG_VOUT2OK)) ==
       (ADP5360_PG_VOUT1OK | ADP5360_PG_VOUT2OK)) ? 1U : 0U;

#if PS_PHASE6_LPBAM_DISPLAY_EXPERIMENT && PS_PHASE6_LPBAM_START_AUTONOMOUS
  if (g_ps_phase5_threadx_probe.power_rails_ready != 0U)
  {
    g_ps_phase5_threadx_probe.phase6_lpbam_prearm_status =
        (ULONG)PS_Phase6_PrearmLpbamDisplayStopTransfer();
  }
#endif

phase6_wait_for_sleep_trigger:
  for (;;)
  {
    g_ps_phase5_threadx_probe.heartbeat[PS_THREADX_OWNER_POWER]++;
    g_ps_phase5_threadx_probe.last_time[PS_THREADX_OWNER_POWER] = tx_time_get();
    PS_ThreadXRecordPerformance();

    if (ps_phase6_sleep_trigger_requested != 0U)
    {
      break;
    }

    if (g_ps_phase5_threadx_probe.power_rails_ready != 0U)
    {
      g_ps_phase5_threadx_probe.power_pgood_poll_count++;
    }
    else if (PS_PowerOwnerReadRailStatus(0U) == HAL_OK)
    {
      g_ps_phase5_threadx_probe.power_pgood_poll_count++;
      g_ps_phase5_threadx_probe.power_rails_ready =
          ((g_ps_phase5_threadx_probe.power_pgood_after &
            (ADP5360_PG_VOUT1OK | ADP5360_PG_VOUT2OK)) ==
           (ADP5360_PG_VOUT1OK | ADP5360_PG_VOUT2OK)) ? 1U : 0U;
    }
    else
    {
      g_ps_phase5_threadx_probe.power_action_error_count++;
      g_ps_phase5_threadx_probe.power_i2c_error_after = HAL_I2C_GetError(&hi2c3);
      g_ps_phase5_threadx_probe.power_rails_ready = 0U;
    }

#if PS_PHASE6_LPBAM_DISPLAY_EXPERIMENT && PS_PHASE6_LPBAM_START_AUTONOMOUS && PS_PHASE6_LPBAM_LATENCY_ONLY
    tx_thread_sleep(1U);
#else
    tx_thread_sleep(100U);
#endif
  }

  PS_Phase6_RecordStage(PS_PHASE6_STAGE_TRIGGERED);
  ps_phase6_quiesce_requested = 1U;
  g_ps_phase5_threadx_probe.phase6_quiesce_request = 1U;
  g_ps_phase5_threadx_probe.phase6_quiesce_tick = tx_time_get();
  g_ps_phase5_threadx_probe.phase6_quiesce_ack_mask |=
      PS_PHASE6_OWNER_ACK_MASK(PS_THREADX_OWNER_POWER);
#if PS_PHASE6_LPBAM_DISPLAY_EXPERIMENT && PS_PHASE6_LPBAM_START_AUTONOMOUS && PS_PHASE6_LPBAM_LATENCY_ONLY
  g_ps_phase5_threadx_probe.phase6_quiesce_ack_wait_ticks = 0U;
  g_ps_phase5_threadx_probe.phase6_quiesce_ack_wait_complete = 1U;
#else
  PS_Phase6_WaitForOwnerQuiesceAcks();
#endif

  PS_Phase6_RecordStage(PS_PHASE6_STAGE_OWNERS_QUIESCED);
#if !(PS_PHASE6_LPBAM_DISPLAY_EXPERIMENT && PS_PHASE6_LPBAM_START_AUTONOMOUS && PS_PHASE6_LPBAM_LATENCY_ONLY)
  PS_Phase6_PrepareExternalDevicesForSleep();
  PS_Phase6_EnablePmicPulseStopForSleep();
#endif
  PS_Phase6_RecordStage(PS_PHASE6_STAGE_EXTERNAL_SLEEP);
#if !(PS_PHASE6_LPBAM_DISPLAY_EXPERIMENT && PS_PHASE6_LPBAM_START_AUTONOMOUS && PS_PHASE6_LPBAM_LATENCY_ONLY)
  PS_Phase6_HoldStage(PS_PHASE6_STAGE_HOLD_TICKS);
#endif

  PS_Phase6_DiagnosticCut3V3Rail();

#if !(PS_PHASE6_LPBAM_DISPLAY_EXPERIMENT && PS_PHASE6_LPBAM_START_AUTONOMOUS && PS_PHASE6_LPBAM_LATENCY_ONLY)
  PS_Phase6_DeInitIdlePeripherals();
#endif

  PS_Phase6_RecordStage(PS_PHASE6_STAGE_PERIPHERALS_OFF);
#if !(PS_PHASE6_LPBAM_DISPLAY_EXPERIMENT && PS_PHASE6_LPBAM_START_AUTONOMOUS && PS_PHASE6_LPBAM_LATENCY_ONLY)
  PS_Phase6_HoldStage(PS_PHASE6_STAGE_HOLD_TICKS);
#endif

#if PS_PHASE6_LPBAM_DISPLAY_EXPERIMENT
#if PS_PHASE6_LPBAM_START_AUTONOMOUS
  (void)PS_Phase6_PrepareLpbamDisplayStopTransfer();
#else
  HAL_GPIO_WritePin(VLT_LCD_GPIO_Port, VLT_LCD_Pin, GPIO_PIN_SET);
  g_ps_phase5_threadx_probe.phase6_lpbam_display_enabled = PS_PHASE6_LPBAM_DISPLAY_EXPERIMENT;
  g_ps_phase5_threadx_probe.phase6_lpbam_display_rows = 0U;
  g_ps_phase5_threadx_probe.phase6_lpbam_start_status = 0xFFFFFFFEUL;
#endif
#endif
  PS_Phase6_RecordStage(PS_PHASE6_STAGE_STOP2);
  PS_Phase6_EnterStop2ForCurrentTest();
  PS_Phase6_ResumeAfterStop2Wake();
  goto phase6_wait_for_sleep_trigger;


  for (;;)
  {
    g_ps_phase5_threadx_probe.heartbeat[PS_THREADX_OWNER_POWER]++;
    g_ps_phase5_threadx_probe.last_time[PS_THREADX_OWNER_POWER] = tx_time_get();
    g_ps_phase5_threadx_probe.phase6_quiesce_ack_mask |=
        PS_PHASE6_OWNER_ACK_MASK(PS_THREADX_OWNER_POWER);
    PS_ThreadXRecordPerformance();

    if (g_ps_phase5_threadx_probe.phase6_i2c_deinit_status != (ULONG)HAL_OK)
    {
      g_ps_phase5_threadx_probe.power_rails_ready = 0U;
    }
    else if (g_ps_phase5_threadx_probe.power_rails_ready != 0U)
    {
      g_ps_phase5_threadx_probe.power_pgood_poll_count++;
    }
    else if (PS_PowerOwnerReadRailStatus(0U) == HAL_OK)
    {
      g_ps_phase5_threadx_probe.power_pgood_poll_count++;
      g_ps_phase5_threadx_probe.power_rails_ready =
          ((g_ps_phase5_threadx_probe.power_pgood_after &
            (ADP5360_PG_VOUT1OK | ADP5360_PG_VOUT2OK)) ==
           (ADP5360_PG_VOUT1OK | ADP5360_PG_VOUT2OK)) ? 1U : 0U;
    }
    else
    {
      g_ps_phase5_threadx_probe.power_action_error_count++;
      g_ps_phase5_threadx_probe.power_i2c_error_after = HAL_I2C_GetError(&hi2c3);
      g_ps_phase5_threadx_probe.power_rails_ready = 0U;
    }

    tx_thread_sleep(1000U);
  }
}

static HAL_StatusTypeDef PS_PowerOwnerConfigureRails(void)
{
  HAL_StatusTypeDef status;

  status = ADP5360_set_buck_vout(ADP_cfg.buck_vout.vout_mV,
                                 ADP_cfg.buck_vout.dly_us);
  if (status != HAL_OK)
  {
    return status;
  }

  status = ADP5360_set_buck(&ADP_cfg.buck_cfg);
  if (status != HAL_OK)
  {
    return status;
  }

  status = ADP5360_set_buckboost_vout(ADP_cfg.buckbst_vout.vout_mV,
                                      ADP_cfg.buckbst_vout.dly_us);
  if (status != HAL_OK)
  {
    return status;
  }

  return ADP5360_set_buckboost(&ADP_cfg.buckbst_cfg);
}

static HAL_StatusTypeDef PS_PowerOwnerReadRailStatus(ULONG before)
{
  HAL_StatusTypeDef status;
  uint8_t value = 0U;
  uint16_t vbat_mv = 0U;
  uint16_t vbat_raw = 0U;
  uint16_t vout_mv = 0U;
  uint16_t delay_us = 0U;
  ADP5360_buck_cfg_t buck_cfg = {0};
  ADP5360_buckbst_cfg_t buckboost_cfg = {0};

  status = ADP5360_read_u8(ADP5360_REG_BUCK_CONFIGURE, &value);
  if (status != HAL_OK)
  {
    return status;
  }
  if (before != 0U)
  {
    g_ps_phase5_threadx_probe.power_buck_cfg_before = value;
  }
  else
  {
    g_ps_phase5_threadx_probe.power_buck_cfg_after = value;
  }

  status = ADP5360_read_u8(ADP5360_REG_BUCK_OUTPUT_VOLTAGE, &value);
  if (status != HAL_OK)
  {
    return status;
  }
  if (before != 0U)
  {
    g_ps_phase5_threadx_probe.power_buck_vout_before = value;
  }
  else
  {
    g_ps_phase5_threadx_probe.power_buck_vout_after = value;
  }

  status = ADP5360_read_u8(ADP5360_REG_BUCKBST_CONFIGURE, &value);
  if (status != HAL_OK)
  {
    return status;
  }
  if (before != 0U)
  {
    g_ps_phase5_threadx_probe.power_buckboost_cfg_before = value;
  }
  else
  {
    g_ps_phase5_threadx_probe.power_buckboost_cfg_after = value;
  }

  status = ADP5360_read_u8(ADP5360_REG_BUCKBST_OUTPUT_VOLTAGE, &value);
  if (status != HAL_OK)
  {
    return status;
  }
  if (before != 0U)
  {
    g_ps_phase5_threadx_probe.power_buckboost_vout_before = value;
  }
  else
  {
    g_ps_phase5_threadx_probe.power_buckboost_vout_after = value;
  }

  status = ADP5360_read_u8(ADP5360_REG_PGOOD_STATUS, &value);
  if (status != HAL_OK)
  {
    return status;
  }
  if (before != 0U)
  {
    g_ps_phase5_threadx_probe.power_pgood_before = value;
  }
  else
  {
    g_ps_phase5_threadx_probe.power_pgood_after = value;
  }

  status = ADP5360_read_u8(ADP5360_REG_FAULT, &value);
  if (status != HAL_OK)
  {
    return status;
  }
  if (before != 0U)
  {
    g_ps_phase5_threadx_probe.power_fault_before = value;
  }
  else
  {
    g_ps_phase5_threadx_probe.power_fault_after = value;
  }

  status = ADP5360_get_vbat(&vbat_mv, &vbat_raw);
  if (status == HAL_OK)
  {
    g_ps_phase5_threadx_probe.power_vbat_mv = vbat_mv;
    g_ps_phase5_threadx_probe.power_vbat_raw = vbat_raw;
  }
  if (status != HAL_OK)
  {
    return status;
  }

  status = ADP5360_read_u8(ADP5360_REG_MANUF_MODEL_ID, &value);
  if (status != HAL_OK)
  {
    return status;
  }
  g_ps_phase5_threadx_probe.power_adp_id = value;

  status = ADP5360_read_u8(ADP5360_REG_SILICON_REV, &value);
  if (status != HAL_OK)
  {
    return status;
  }
  g_ps_phase5_threadx_probe.power_adp_revision = value;

  status = ADP5360_read_u8(ADP5360_REG_CHARGER_FUNCTION_SETTING, &value);
  if (status != HAL_OK)
  {
    return status;
  }
  g_ps_phase5_threadx_probe.power_charger_function = value;

  status = ADP5360_read_u8(ADP5360_REG_CHARGER_STATUS1, &value);
  if (status != HAL_OK)
  {
    return status;
  }
  g_ps_phase5_threadx_probe.power_charger_status1 = value;

  status = ADP5360_read_u8(ADP5360_REG_CHARGER_STATUS2, &value);
  if (status != HAL_OK)
  {
    return status;
  }
  g_ps_phase5_threadx_probe.power_charger_status2 = value;

  status = ADP5360_read_u8(ADP5360_REG_SUPERVISORY_SETTING, &value);
  if (status != HAL_OK)
  {
    return status;
  }
  g_ps_phase5_threadx_probe.power_supervisory = value;

  status = ADP5360_read_u8(ADP5360_REG_PGOOD1_MASK, &value);
  if (status != HAL_OK)
  {
    return status;
  }
  g_ps_phase5_threadx_probe.power_pgood1_mask = value;

  status = ADP5360_read_u8(ADP5360_REG_PGOOD2_MASK, &value);
  if (status != HAL_OK)
  {
    return status;
  }
  g_ps_phase5_threadx_probe.power_pgood2_mask = value;

  status = ADP5360_read_u8(ADP5360_REG_INT_ENABLE1, &value);
  if (status != HAL_OK)
  {
    return status;
  }
  g_ps_phase5_threadx_probe.power_int_enable1 = value;

  status = ADP5360_read_u8(ADP5360_REG_INT_ENABLE2, &value);
  if (status != HAL_OK)
  {
    return status;
  }
  g_ps_phase5_threadx_probe.power_int_enable2 = value;

  status = ADP5360_read_u8(ADP5360_REG_SHIPMODE, &value);
  if (status != HAL_OK)
  {
    return status;
  }
  g_ps_phase5_threadx_probe.power_shipmode = value;

  status = ADP5360_get_buck_vout(&vout_mv, &delay_us);
  if (status != HAL_OK)
  {
    return status;
  }
  g_ps_phase5_threadx_probe.power_buck_vout_mv = vout_mv;
  g_ps_phase5_threadx_probe.power_buck_vout_delay_us = delay_us;

  status = ADP5360_get_buckboost_vout(&vout_mv, &delay_us);
  if (status != HAL_OK)
  {
    return status;
  }
  g_ps_phase5_threadx_probe.power_buckboost_vout_mv = vout_mv;
  g_ps_phase5_threadx_probe.power_buckboost_vout_delay_us = delay_us;

  status = ADP5360_get_buck(&buck_cfg);
  if (status != HAL_OK)
  {
    return status;
  }
  g_ps_phase5_threadx_probe.power_buck_enable_decoded = buck_cfg.enable;
  g_ps_phase5_threadx_probe.power_buck_ilim_ma_decoded = buck_cfg.ilim_mA;

  status = ADP5360_get_buckboost(&buckboost_cfg);
  if (status != HAL_OK)
  {
    return status;
  }
  g_ps_phase5_threadx_probe.power_buckboost_enable_decoded = buckboost_cfg.enable;
  g_ps_phase5_threadx_probe.power_buckboost_ilim_ma_decoded = buckboost_cfg.ilim_mA;

  return status;
}

static void PS_Phase6_PrepareExternalDevicesForSleep(void)
{
  HAL_StatusTypeDef status;
  stmdev_ctx_t lis_ctx = {0};
  lis2dux12_md_t lis_mode = {0};
  uint8_t value = 0U;

  if (g_ps_phase5_threadx_probe.phase6_external_sleep_done != 0U)
  {
    return;
  }

  HAL_GPIO_WritePin(PHOT_EN_GPIO_Port, PHOT_EN_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(SD_MODE_GPIO_Port, SD_MODE_Pin, GPIO_PIN_RESET);
  g_ps_phase5_threadx_probe.phase6_phot_en_state =
      (ULONG)HAL_GPIO_ReadPin(PHOT_EN_GPIO_Port, PHOT_EN_Pin);

  /* NINA capability discovery and power-state ownership live in thComms. */
  g_ps_phase5_threadx_probe.phase6_nina_nrst_state =
      (ULONG)HAL_GPIO_ReadPin(NINA_NRST_GPIO_Port, NINA_NRST_Pin);

  g_ps_phase5_threadx_probe.phase6_flash_dpd_status =
      (ULONG)PS_Phase6_AT25CommandOnly(PS_PHASE6_AT25_CMD_DPD);

  lis_ctx.read_reg = PS_Phase6_LIS2DUX12_ReadReg;
  lis_ctx.write_reg = PS_Phase6_LIS2DUX12_WriteReg;
  lis_ctx.handle = (void *)(uintptr_t)PS_PHASE6_LIS2DUX12_ADDR;

  g_ps_phase5_threadx_probe.phase6_lis_whoami_status =
      (ULONG)lis2dux12_read_reg(&lis_ctx, LIS2DUX12_WHO_AM_I, &value, 1U);

  if (g_ps_phase5_threadx_probe.phase6_lis_whoami_status == 0U)
  {
    g_ps_phase5_threadx_probe.phase6_lis_readback_status = 0U;

    if (lis2dux12_read_reg(&lis_ctx, LIS2DUX12_CTRL1, &value, 1U) == 0)
    {
      g_ps_phase5_threadx_probe.phase6_lis_ctrl1_before = value;
    }
    else
    {
      g_ps_phase5_threadx_probe.phase6_lis_readback_status = 0xFFFFFFFFUL;
    }
    if (lis2dux12_read_reg(&lis_ctx, LIS2DUX12_CTRL2, &value, 1U) == 0)
    {
      g_ps_phase5_threadx_probe.phase6_lis_ctrl2_before = value;
    }
    else
    {
      g_ps_phase5_threadx_probe.phase6_lis_readback_status = 0xFFFFFFFFUL;
    }
    if (lis2dux12_read_reg(&lis_ctx, LIS2DUX12_CTRL3, &value, 1U) == 0)
    {
      g_ps_phase5_threadx_probe.phase6_lis_ctrl3_before = value;
    }
    else
    {
      g_ps_phase5_threadx_probe.phase6_lis_readback_status = 0xFFFFFFFFUL;
    }
    if (lis2dux12_read_reg(&lis_ctx, LIS2DUX12_CTRL4, &value, 1U) == 0)
    {
      g_ps_phase5_threadx_probe.phase6_lis_ctrl4_before = value;
    }
    else
    {
      g_ps_phase5_threadx_probe.phase6_lis_readback_status = 0xFFFFFFFFUL;
    }
    if (lis2dux12_read_reg(&lis_ctx, LIS2DUX12_CTRL5, &value, 1U) == 0)
    {
      g_ps_phase5_threadx_probe.phase6_lis_ctrl5_before = value;
    }
    else
    {
      g_ps_phase5_threadx_probe.phase6_lis_readback_status = 0xFFFFFFFFUL;
    }
    if (lis2dux12_read_reg(&lis_ctx, LIS2DUX12_FIFO_CTRL, &value, 1U) == 0)
    {
      g_ps_phase5_threadx_probe.phase6_lis_fifo_ctrl_before = value;
    }
    else
    {
      g_ps_phase5_threadx_probe.phase6_lis_readback_status = 0xFFFFFFFFUL;
    }
    if (lis2dux12_read_reg(&lis_ctx, LIS2DUX12_INTERRUPT_CFG, &value, 1U) == 0)
    {
      g_ps_phase5_threadx_probe.phase6_lis_interrupt_cfg_before = value;
    }
    else
    {
      g_ps_phase5_threadx_probe.phase6_lis_readback_status = 0xFFFFFFFFUL;
    }
    if (lis2dux12_read_reg(&lis_ctx, LIS2DUX12_MD1_CFG, &value, 1U) == 0)
    {
      g_ps_phase5_threadx_probe.phase6_lis_md1_cfg_before = value;
    }
    else
    {
      g_ps_phase5_threadx_probe.phase6_lis_readback_status = 0xFFFFFFFFUL;
    }
    if (lis2dux12_read_reg(&lis_ctx, LIS2DUX12_MD2_CFG, &value, 1U) == 0)
    {
      g_ps_phase5_threadx_probe.phase6_lis_md2_cfg_before = value;
    }
    else
    {
      g_ps_phase5_threadx_probe.phase6_lis_readback_status = 0xFFFFFFFFUL;
    }
    if (lis2dux12_read_reg(&lis_ctx, LIS2DUX12_SLEEP, &value, 1U) == 0)
    {
      g_ps_phase5_threadx_probe.phase6_lis_sleep_before = value;
    }
    else
    {
      g_ps_phase5_threadx_probe.phase6_lis_readback_status = 0xFFFFFFFFUL;
    }

    value = 0U;
    g_ps_phase5_threadx_probe.phase6_lis_fifo_bypass_status =
        (ULONG)lis2dux12_write_reg(&lis_ctx, LIS2DUX12_FIFO_CTRL, &value, 1U);

    g_ps_phase5_threadx_probe.phase6_lis_interrupt_clear_status = 0U;
    value = 0U;
    if (lis2dux12_write_reg(&lis_ctx, LIS2DUX12_CTRL2, &value, 1U) != 0)
    {
      g_ps_phase5_threadx_probe.phase6_lis_interrupt_clear_status = 0xFFFFFFFFUL;
    }
    value = 0U;
    if (lis2dux12_write_reg(&lis_ctx, LIS2DUX12_CTRL3, &value, 1U) != 0)
    {
      g_ps_phase5_threadx_probe.phase6_lis_interrupt_clear_status = 0xFFFFFFFFUL;
    }
    value = 0U;
    if (lis2dux12_write_reg(&lis_ctx, LIS2DUX12_INTERRUPT_CFG, &value, 1U) != 0)
    {
      g_ps_phase5_threadx_probe.phase6_lis_interrupt_clear_status = 0xFFFFFFFFUL;
    }
    value = 0U;
    if (lis2dux12_write_reg(&lis_ctx, LIS2DUX12_MD1_CFG, &value, 1U) != 0)
    {
      g_ps_phase5_threadx_probe.phase6_lis_interrupt_clear_status = 0xFFFFFFFFUL;
    }
    value = 0U;
    if (lis2dux12_write_reg(&lis_ctx, LIS2DUX12_MD2_CFG, &value, 1U) != 0)
    {
      g_ps_phase5_threadx_probe.phase6_lis_interrupt_clear_status = 0xFFFFFFFFUL;
    }

    g_ps_phase5_threadx_probe.phase6_lis_temp_disable_status =
        (ULONG)lis2dux12_temp_disable_set(&lis_ctx, PROPERTY_ENABLE);
    g_ps_phase5_threadx_probe.phase6_lis_embedded_disable_status =
        (ULONG)lis2dux12_embedded_state_set(&lis_ctx, PROPERTY_DISABLE);

    if (lis2dux12_read_reg(&lis_ctx, LIS2DUX12_CTRL4, &value, 1U) == 0)
    {
      value = (uint8_t)(value & ~(0x08U | 0x10U));
      if (lis2dux12_write_reg(&lis_ctx, LIS2DUX12_CTRL4, &value, 1U) != 0)
      {
        g_ps_phase5_threadx_probe.phase6_lis_embedded_disable_status = 0xFFFFFFFFUL;
      }
    }
    else
    {
      g_ps_phase5_threadx_probe.phase6_lis_embedded_disable_status = 0xFFFFFFFFUL;
    }

    g_ps_phase5_threadx_probe.phase6_lis_mode_get_status =
        (ULONG)lis2dux12_mode_get(&lis_ctx, &lis_mode);
    if (g_ps_phase5_threadx_probe.phase6_lis_mode_get_status == 0U)
    {
      lis_mode.odr = LIS2DUX12_OFF;
      g_ps_phase5_threadx_probe.phase6_lis_mode_set_status =
          (ULONG)lis2dux12_mode_set(&lis_ctx, &lis_mode);
    }
    else
    {
      g_ps_phase5_threadx_probe.phase6_lis_mode_set_status = 0xFFFFFFFFUL;
    }

    if (lis2dux12_read_reg(&lis_ctx, LIS2DUX12_CTRL1, &value, 1U) == 0)
    {
      value = (uint8_t)(value & 0x10U);
      (void)lis2dux12_write_reg(&lis_ctx, LIS2DUX12_CTRL1, &value, 1U);
    }

    g_ps_phase5_threadx_probe.phase6_lis_deep_pd_status =
        (ULONG)lis2dux12_enter_deep_power_down(&lis_ctx, 1U);

    if (lis2dux12_read_reg(&lis_ctx, LIS2DUX12_CTRL1, &value, 1U) == 0)
    {
      g_ps_phase5_threadx_probe.phase6_lis_ctrl1_after = value;
    }
    if (lis2dux12_read_reg(&lis_ctx, LIS2DUX12_CTRL2, &value, 1U) == 0)
    {
      g_ps_phase5_threadx_probe.phase6_lis_ctrl2_after = value;
    }
    if (lis2dux12_read_reg(&lis_ctx, LIS2DUX12_CTRL3, &value, 1U) == 0)
    {
      g_ps_phase5_threadx_probe.phase6_lis_ctrl3_after = value;
    }
    if (lis2dux12_read_reg(&lis_ctx, LIS2DUX12_CTRL4, &value, 1U) == 0)
    {
      g_ps_phase5_threadx_probe.phase6_lis_ctrl4_after = value;
    }
    if (lis2dux12_read_reg(&lis_ctx, LIS2DUX12_CTRL5, &value, 1U) == 0)
    {
      g_ps_phase5_threadx_probe.phase6_lis_ctrl5_after = value;
    }
    if (lis2dux12_read_reg(&lis_ctx, LIS2DUX12_FIFO_CTRL, &value, 1U) == 0)
    {
      g_ps_phase5_threadx_probe.phase6_lis_fifo_ctrl_after = value;
    }
    if (lis2dux12_read_reg(&lis_ctx, LIS2DUX12_INTERRUPT_CFG, &value, 1U) == 0)
    {
      g_ps_phase5_threadx_probe.phase6_lis_interrupt_cfg_after = value;
    }
    if (lis2dux12_read_reg(&lis_ctx, LIS2DUX12_MD1_CFG, &value, 1U) == 0)
    {
      g_ps_phase5_threadx_probe.phase6_lis_md1_cfg_after = value;
    }
    if (lis2dux12_read_reg(&lis_ctx, LIS2DUX12_MD2_CFG, &value, 1U) == 0)
    {
      g_ps_phase5_threadx_probe.phase6_lis_md2_cfg_after = value;
    }
    if (lis2dux12_read_reg(&lis_ctx, LIS2DUX12_SLEEP, &value, 1U) == 0)
    {
      g_ps_phase5_threadx_probe.phase6_lis_sleep_after = value;
    }
  }
  else
  {
    g_ps_phase5_threadx_probe.phase6_lis_mode_get_status = 0xFFFFFFFFUL;
    g_ps_phase5_threadx_probe.phase6_lis_mode_set_status = 0xFFFFFFFFUL;
    g_ps_phase5_threadx_probe.phase6_lis_deep_pd_status = 0xFFFFFFFFUL;
    g_ps_phase5_threadx_probe.phase6_lis_readback_status = 0xFFFFFFFFUL;
  }

  status = HAL_I2C_IsDeviceReady(&hi2c3,
                                 PS_PHASE6_TMAG3001_ADDR,
                                 2U,
                                 PS_PHASE6_I2C_TIMEOUT_MS);
  g_ps_phase5_threadx_probe.phase6_tmag_ready_status = (ULONG)status;
  if (status == HAL_OK)
  {
    status = PS_Phase6_TMAG3001_ReadReg(SENSOR_CONFIG_1_ADDRESS, &value);
    if (status == HAL_OK)
    {
      g_ps_phase5_threadx_probe.phase6_tmag_sensor_cfg1_before = value;
      value = (uint8_t)((value & ~SENSOR_CONFIG_1_MAG_CH_EN_MASK) |
                        SENSOR_CONFIG_1_MAG_CH_EN_Off);
      status = PS_Phase6_TMAG3001_WriteReg(SENSOR_CONFIG_1_ADDRESS, value);
    }
    if (status == HAL_OK)
    {
      status = PS_Phase6_TMAG3001_ReadReg(SENSOR_CONFIG_1_ADDRESS, &value);
      if (status == HAL_OK)
      {
        g_ps_phase5_threadx_probe.phase6_tmag_sensor_cfg1_after = value;
      }
    }

    if (status == HAL_OK)
    {
      status = PS_Phase6_TMAG3001_ReadReg(DEVICE_CONFIG_2_ADDRESS, &value);
      if (status == HAL_OK)
      {
        g_ps_phase5_threadx_probe.phase6_tmag_device_cfg2_before = value;
        value = (uint8_t)((value & ~(DEVICE_CONFIG_2_LP_LN_MASK |
                                     DEVICE_CONFIG_2_OPERATING_MODE_MASK)) |
                          DEVICE_CONFIG_2_LP_LN_LowCurrent |
                          DEVICE_CONFIG_2_OPERATING_MODE_Sleep);
        status = PS_Phase6_TMAG3001_WriteReg(DEVICE_CONFIG_2_ADDRESS, value);
      }
    }
    if (status == HAL_OK)
    {
      status = PS_Phase6_TMAG3001_ReadReg(DEVICE_CONFIG_2_ADDRESS, &value);
      if (status == HAL_OK)
      {
        g_ps_phase5_threadx_probe.phase6_tmag_device_cfg2_after = value;
      }
    }
    g_ps_phase5_threadx_probe.phase6_tmag_sensor_sleep_status = (ULONG)status;
  }
  else
  {
    g_ps_phase5_threadx_probe.phase6_tmag_sensor_sleep_status = (ULONG)status;
  }

  g_ps_phase5_threadx_probe.phase6_external_sleep_done = 1U;
}

static void __attribute__((unused)) PS_Phase6_PrepareNinaForSleep(void)
{
  static const uint8_t at_cmd[] = "AT\r\n";
  static const uint8_t poweroff_cmd[] = "AT+CPWROFF\r\n";
  uint8_t rx[64] = {0};
  uint16_t rx_len;
  HAL_StatusTypeDef status;

  HAL_GPIO_WritePin(NINA_NRST_GPIO_Port, NINA_NRST_Pin, GPIO_PIN_RESET);
  tx_thread_sleep(PS_PHASE6_NINA_RESET_ASSERT_TICKS);
  HAL_GPIO_WritePin(NINA_NRST_GPIO_Port, NINA_NRST_Pin, GPIO_PIN_SET);
  tx_thread_sleep(PS_NINA_CAPABILITY_BOOT_WAIT_TICKS);

  g_ps_phase5_threadx_probe.phase6_nina_boot_rx_len =
      PS_Phase6_NinaReceiveUntilQuiet(rx,
                                      sizeof(rx),
                                      PS_PHASE6_NINA_BOOT_DRAIN_TICKS,
                                      PS_PHASE6_NINA_RX_QUIET_TICKS);
  g_ps_phase5_threadx_probe.phase6_nina_boot_rx_word0 =
      PS_Phase6_PackBytes(rx,
                          (uint16_t)g_ps_phase5_threadx_probe.phase6_nina_boot_rx_len,
                          0U);
  g_ps_phase5_threadx_probe.phase6_nina_boot_rx_word1 =
      PS_Phase6_PackBytes(rx,
                          (uint16_t)g_ps_phase5_threadx_probe.phase6_nina_boot_rx_len,
                          4U);

  for (uint16_t i = 0U; i < sizeof(rx); i++)
  {
    rx[i] = 0U;
  }

  status = HAL_UART_Transmit(&hlpuart1,
                             at_cmd,
                             (uint16_t)(sizeof(at_cmd) - 1U),
                             250U);
  g_ps_phase5_threadx_probe.phase6_nina_at_tx_status = (ULONG)status;

  rx_len = PS_Phase6_NinaReceiveUntilQuiet(rx,
                                           sizeof(rx),
                                           PS_PHASE6_NINA_AT_RX_TICKS,
                                           PS_PHASE6_NINA_RX_QUIET_TICKS);
  g_ps_phase5_threadx_probe.phase6_nina_at_rx_len = rx_len;
  g_ps_phase5_threadx_probe.phase6_nina_at_ok =
      PS_Phase6_BufferContainsToken(rx, rx_len, "OK");
  g_ps_phase5_threadx_probe.phase6_nina_at_error =
      PS_Phase6_BufferContainsToken(rx, rx_len, "ERROR");
  g_ps_phase5_threadx_probe.phase6_nina_at_rx_word0 =
      PS_Phase6_PackBytes(rx, rx_len, 0U);
  g_ps_phase5_threadx_probe.phase6_nina_at_rx_word1 =
      PS_Phase6_PackBytes(rx, rx_len, 4U);

  if (g_ps_phase5_threadx_probe.phase6_nina_at_ok == 0U)
  {
    g_ps_phase5_threadx_probe.phase6_nina_poweroff_tx_status = 0xFFFFFFFFUL;
    HAL_GPIO_WritePin(NINA_NRST_GPIO_Port, NINA_NRST_Pin, GPIO_PIN_RESET);
    g_ps_phase5_threadx_probe.phase6_nina_nrst_state =
        (ULONG)HAL_GPIO_ReadPin(NINA_NRST_GPIO_Port, NINA_NRST_Pin);
    return;
  }

  for (uint16_t i = 0U; i < sizeof(rx); i++)
  {
    rx[i] = 0U;
  }

  status = HAL_UART_Transmit(&hlpuart1,
                             poweroff_cmd,
                             (uint16_t)(sizeof(poweroff_cmd) - 1U),
                             250U);
  g_ps_phase5_threadx_probe.phase6_nina_poweroff_tx_status = (ULONG)status;

  rx_len = PS_Phase6_NinaReceiveUntilQuiet(rx,
                                           sizeof(rx),
                                           500U,
                                           PS_PHASE6_NINA_RX_QUIET_TICKS);
  g_ps_phase5_threadx_probe.phase6_nina_poweroff_rx_len = rx_len;
  g_ps_phase5_threadx_probe.phase6_nina_poweroff_ok =
      PS_Phase6_BufferContainsToken(rx, rx_len, "OK");
  g_ps_phase5_threadx_probe.phase6_nina_poweroff_error =
      PS_Phase6_BufferContainsToken(rx, rx_len, "ERROR");
  g_ps_phase5_threadx_probe.phase6_nina_poweroff_rx_word0 =
      PS_Phase6_PackBytes(rx, rx_len, 0U);
  g_ps_phase5_threadx_probe.phase6_nina_poweroff_rx_word1 =
      PS_Phase6_PackBytes(rx, rx_len, 4U);
  g_ps_phase5_threadx_probe.phase6_nina_poweroff_rx_word2 =
      PS_Phase6_PackBytes(rx, rx_len, 8U);
  g_ps_phase5_threadx_probe.phase6_nina_poweroff_rx_word3 =
      PS_Phase6_PackBytes(rx, rx_len, 12U);

  tx_thread_sleep(PS_PHASE6_NINA_POWEROFF_WAIT_TICKS);

  if (g_ps_phase5_threadx_probe.phase6_nina_poweroff_ok == 0U)
  {
    HAL_GPIO_WritePin(NINA_NRST_GPIO_Port, NINA_NRST_Pin, GPIO_PIN_RESET);
  }

  g_ps_phase5_threadx_probe.phase6_nina_nrst_state =
      (ULONG)HAL_GPIO_ReadPin(NINA_NRST_GPIO_Port, NINA_NRST_Pin);
}

static uint16_t PS_Phase6_NinaReceiveUntilQuiet(uint8_t *buffer,
                                                uint16_t capacity,
                                                ULONG max_ticks,
                                                ULONG quiet_ticks)
{
  uint16_t len = 0U;
  ULONG start = tx_time_get();
  ULONG last_rx = start;

  while (((tx_time_get() - start) < max_ticks) && (len < capacity))
  {
    uint8_t byte = 0U;
    if (HAL_UART_Receive(&hlpuart1, &byte, 1U, 10U) == HAL_OK)
    {
      buffer[len++] = byte;
      last_rx = tx_time_get();
    }
    else if ((len != 0U) && ((tx_time_get() - last_rx) >= quiet_ticks))
    {
      break;
    }
  }

  return len;
}

static ULONG PS_Phase6_PackBytes(const uint8_t *data, uint16_t len, uint16_t offset)
{
  ULONG word = 0U;

  for (uint16_t i = 0U; i < 4U; i++)
  {
    if ((uint16_t)(offset + i) < len)
    {
      word |= ((ULONG)data[offset + i]) << (i * 8U);
    }
  }

  return word;
}

static ULONG PS_Phase6_BufferContainsToken(const uint8_t *data, uint16_t len, const char *token)
{
  uint16_t token_len = 0U;

  while (token[token_len] != '\0')
  {
    token_len++;
  }

  if ((token_len == 0U) || (len < token_len))
  {
    return 0U;
  }

  for (uint16_t i = 0U; i <= (uint16_t)(len - token_len); i++)
  {
    uint16_t matched = 0U;

    while ((matched < token_len) && (data[i + matched] == (uint8_t)token[matched]))
    {
      matched++;
    }

    if (matched == token_len)
    {
      return 1U;
    }
  }

  return 0U;
}

static void PS_Phase6_EnablePmicPulseStopForSleep(void)
{
  HAL_StatusTypeDef status;
  ADP5360_buck_cfg_t buck_cfg = {0};
  ADP5360_buckbst_cfg_t buckboost_cfg = {0};
  uint8_t value = 0U;

  status = ADP5360_get_buck(&buck_cfg);
  g_ps_phase5_threadx_probe.phase6_buck_pulsestop_get_status = (ULONG)status;
  if (status == HAL_OK)
  {
    buck_cfg.stop_enable = 1U;
    status = ADP5360_set_buck(&buck_cfg);
  }
  g_ps_phase5_threadx_probe.phase6_buck_pulsestop_set_status = (ULONG)status;
  if (ADP5360_read_u8(ADP5360_REG_BUCK_CONFIGURE, &value) == HAL_OK)
  {
    g_ps_phase5_threadx_probe.phase6_buck_pulsestop_cfg_after = value;
  }

  status = ADP5360_get_buckboost(&buckboost_cfg);
  g_ps_phase5_threadx_probe.phase6_buckboost_pulsestop_get_status = (ULONG)status;
  if (status == HAL_OK)
  {
    buckboost_cfg.stop_enable = 1U;
    status = ADP5360_set_buckboost(&buckboost_cfg);
  }
  g_ps_phase5_threadx_probe.phase6_buckboost_pulsestop_set_status = (ULONG)status;
  if (ADP5360_read_u8(ADP5360_REG_BUCKBST_CONFIGURE, &value) == HAL_OK)
  {
    g_ps_phase5_threadx_probe.phase6_buckboost_pulsestop_cfg_after = value;
  }
}

static void PS_Phase6_DiagnosticCut3V3Rail(void)
{
  HAL_StatusTypeDef status;
  ADP5360_buckbst_cfg_t buckboost_cfg = {0};
  uint8_t value = 0U;

  g_ps_phase5_threadx_probe.phase6_diag_cut_3v3_enabled =
      PS_PHASE6_DIAG_CUT_3V3_RAIL;

  if (PS_PHASE6_DIAG_CUT_3V3_RAIL == 0U)
  {
    return;
  }

  HAL_GPIO_WritePin(VLT_LCD_GPIO_Port, VLT_LCD_Pin, GPIO_PIN_RESET);
  g_ps_phase5_threadx_probe.phase6_diag_vlt_lcd_state =
      (ULONG)HAL_GPIO_ReadPin(VLT_LCD_GPIO_Port, VLT_LCD_Pin);

  status = ADP5360_get_buckboost(&buckboost_cfg);
  g_ps_phase5_threadx_probe.phase6_diag_buckboost_get_status = (ULONG)status;
  if (status == HAL_OK)
  {
    buckboost_cfg.enable = 0U;
    buckboost_cfg.discharge_en = 0U;
    status = ADP5360_set_buckboost(&buckboost_cfg);
  }
  g_ps_phase5_threadx_probe.phase6_diag_buckboost_set_status = (ULONG)status;

  if (ADP5360_read_u8(ADP5360_REG_BUCKBST_CONFIGURE, &value) == HAL_OK)
  {
    g_ps_phase5_threadx_probe.phase6_diag_buckboost_cfg_after = value;
  }

  if (ADP5360_read_u8(ADP5360_REG_PGOOD_STATUS, &value) == HAL_OK)
  {
    g_ps_phase5_threadx_probe.phase6_diag_pgood_after_3v3_cut = value;
  }

  PS_Phase6_RecordStage(PS_PHASE6_STAGE_DIAG_3V3_OFF);
  g_ps_phase5_threadx_probe.phase6_diag_cut_3v3_tick = tx_time_get();
  PS_Phase6_HoldStage(PS_PHASE6_STAGE_HOLD_TICKS);

  if (ADP5360_read_u8(ADP5360_REG_BUCKBST_CONFIGURE, &value) == HAL_OK)
  {
    g_ps_phase5_threadx_probe.phase6_diag_buckboost_cfg_after_hold = value;
  }

  if (ADP5360_read_u8(ADP5360_REG_PGOOD_STATUS, &value) == HAL_OK)
  {
    g_ps_phase5_threadx_probe.phase6_diag_pgood_after_3v3_hold = value;
  }
}

#if PS_PHASE6_LPBAM_START_AUTONOMOUS
static HAL_StatusTypeDef PS_Phase6_EnableLpbamDisplayAutonomousClocks(void)
{
  SPI_AutonomousModeConfTypeDef autonomous = {0};

  g_ps_phase5_threadx_probe.phase6_lpbam_rcc_srdamr_before = RCC->SRDAMR;

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

  if (ADV_LPBAM_SPI_EnableDMARequests(SPI3) != LPBAM_OK)
  {
    return HAL_ERROR;
  }

  g_ps_phase5_threadx_probe.phase6_lpbam_rcc_srdamr_after = RCC->SRDAMR;
  return HAL_OK;
}

#endif
static void PS_Phase6_RearmButtonWakeInputs(void)
{
  GPIO_InitTypeDef gpio = {0};

  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  gpio.Pin = BTN_START_Pin;
  gpio.Mode = GPIO_MODE_INPUT;
  gpio.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(BTN_START_GPIO_Port, &gpio);

  gpio.Pin = BTN_A_Pin | BTN_B_Pin | BTN_L_Pin | BTN_R_Pin;
  gpio.Mode = GPIO_MODE_IT_RISING_FALLING;
  gpio.Pull = GPIO_PULLDOWN;
  HAL_GPIO_Init(GPIOB, &gpio);

  CLEAR_BIT(EXTI->IMR1, BTN_START_Pin);
  CLEAR_BIT(EXTI->RTSR1, BTN_START_Pin);
  CLEAR_BIT(EXTI->FTSR1, BTN_START_Pin);
  WRITE_REG(EXTI->RPR1, BTN_START_Pin);
  WRITE_REG(EXTI->FPR1, BTN_START_Pin);

  SET_BIT(EXTI->IMR1, PS_PHASE6_BUTTON_EXTI_LINES);
  SET_BIT(EXTI->RTSR1, PS_PHASE6_BUTTON_EXTI_LINES);
  SET_BIT(EXTI->FTSR1, PS_PHASE6_BUTTON_EXTI_LINES);

  WRITE_REG(EXTI->RPR1, PS_PHASE6_BUTTON_EXTI_LINES);
  WRITE_REG(EXTI->FPR1, PS_PHASE6_BUTTON_EXTI_LINES);

  NVIC_DisableIRQ(EXTI4_IRQn);
  NVIC_ClearPendingIRQ(EXTI4_IRQn);
  NVIC_ClearPendingIRQ(EXTI5_IRQn);
  NVIC_ClearPendingIRQ(EXTI6_IRQn);
  NVIC_ClearPendingIRQ(EXTI7_IRQn);
  NVIC_ClearPendingIRQ(EXTI8_IRQn);
  NVIC_EnableIRQ(EXTI5_IRQn);
  NVIC_EnableIRQ(EXTI6_IRQn);
  NVIC_EnableIRQ(EXTI7_IRQn);
  NVIC_EnableIRQ(EXTI8_IRQn);

  g_ps_phase5_threadx_probe.phase6_button_wake_rearm_done = 1U;
}

static void PS_Phase6_RecordLpbamDisplayStopState(uint8_t after_stop)
{
  ULONG dma_state = 0xFFFFFFFFUL;
  ULONG dma_csr = 0xFFFFFFFFUL;
  ULONG spi_sr = 0xFFFFFFFFUL;
  ULONG spi_state = 0xFFFFFFFFUL;
  ULONG spi_error = 0xFFFFFFFFUL;
  ULONG lptim_cr = 0xFFFFFFFFUL;
  ULONG lptim_cfgr = 0xFFFFFFFFUL;
  ULONG lptim_arr = 0xFFFFFFFFUL;
  ULONG lptim_cmp = 0xFFFFFFFFUL;
  ULONG lptim_cnt = 0xFFFFFFFFUL;
  ULONG lptim_ccmr1 = 0xFFFFFFFFUL;
  ULONG lptim_isr = 0xFFFFFFFFUL;

  if (handle_LPDMA1_Channel0.Instance != NULL)
  {
    dma_state = (ULONG)handle_LPDMA1_Channel0.State;
    dma_csr = handle_LPDMA1_Channel0.Instance->CSR;
  }

  if (hspi3.Instance != NULL)
  {
    spi_sr = hspi3.Instance->SR;
    spi_state = (ULONG)HAL_SPI_GetState(&hspi3);
    spi_error = (ULONG)HAL_SPI_GetError(&hspi3);
  }

  if (hlptim1.Instance != NULL)
  {
    lptim_cr = hlptim1.Instance->CR;
    lptim_cfgr = hlptim1.Instance->CFGR;
    lptim_arr = hlptim1.Instance->ARR;
    lptim_cmp = hlptim1.Instance->CCR1;
    lptim_cnt = hlptim1.Instance->CNT;
    lptim_ccmr1 = hlptim1.Instance->CCMR1;
    lptim_isr = hlptim1.Instance->ISR;
  }

  if (after_stop == 0U)
  {
    g_ps_phase5_threadx_probe.phase6_lpbam_dma_state_before_stop = dma_state;
    g_ps_phase5_threadx_probe.phase6_lpbam_dma_csr_before_stop = dma_csr;
    g_ps_phase5_threadx_probe.phase6_lpbam_spi_sr_before_stop = spi_sr;
    g_ps_phase5_threadx_probe.phase6_lpbam_spi_state_before_stop = spi_state;
    g_ps_phase5_threadx_probe.phase6_lpbam_lptim_cr_before_stop = lptim_cr;
    g_ps_phase5_threadx_probe.phase6_lpbam_lptim_cfgr_before_stop = lptim_cfgr;
    g_ps_phase5_threadx_probe.phase6_lpbam_lptim_arr_before_stop = lptim_arr;
    g_ps_phase5_threadx_probe.phase6_lpbam_lptim_cmp_before_stop = lptim_cmp;
    g_ps_phase5_threadx_probe.phase6_lpbam_lptim_cnt_before_stop = lptim_cnt;
    g_ps_phase5_threadx_probe.phase6_lpbam_lptim_ccmr1_before_stop = lptim_ccmr1;
    g_ps_phase5_threadx_probe.phase6_lpbam_lptim_isr_before_stop = lptim_isr;
  }
  else
  {
    g_ps_phase5_threadx_probe.phase6_lpbam_dma_state_after_stop = dma_state;
    g_ps_phase5_threadx_probe.phase6_lpbam_dma_csr_after_stop = dma_csr;
    g_ps_phase5_threadx_probe.phase6_lpbam_spi_sr_after_stop = spi_sr;
    g_ps_phase5_threadx_probe.phase6_lpbam_spi_state_after_stop = spi_state;
    g_ps_phase5_threadx_probe.phase6_lpbam_spi_error_after_stop = spi_error;
    g_ps_phase5_threadx_probe.phase6_lpbam_lptim_cr_after_stop = lptim_cr;
    g_ps_phase5_threadx_probe.phase6_lpbam_lptim_cfgr_after_stop = lptim_cfgr;
    g_ps_phase5_threadx_probe.phase6_lpbam_lptim_arr_after_stop = lptim_arr;
    g_ps_phase5_threadx_probe.phase6_lpbam_lptim_cmp_after_stop = lptim_cmp;
    g_ps_phase5_threadx_probe.phase6_lpbam_lptim_cnt_after_stop = lptim_cnt;
    g_ps_phase5_threadx_probe.phase6_lpbam_lptim_ccmr1_after_stop = lptim_ccmr1;
    g_ps_phase5_threadx_probe.phase6_lpbam_lptim_isr_after_stop = lptim_isr;
  }
}

static HAL_StatusTypeDef PS_Phase6_RestoreDisplaySpiAfterLpbam(void)
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
static void PS_Phase6_ResumeAfterStop2Wake(void)
{
  g_ps_phase5_threadx_probe.phase6_resume_cycle_count++;

  ps_phase6_sleep_trigger_requested = 0U;
  ps_phase6_quiesce_requested = 0U;
  g_ps_phase5_threadx_probe.phase6_sleep_trigger_requested = 0U;
  g_ps_phase5_threadx_probe.phase6_quiesce_request = 0U;
  g_ps_phase5_threadx_probe.phase6_quiesce_ack_mask = 0U;
  g_ps_phase5_threadx_probe.phase6_quiesce_ack_wait_complete = 0U;
  g_ps_phase5_threadx_probe.phase6_resume_trigger_cleared = 1U;

  g_ps_phase5_threadx_probe.phase6_resume_i2c_init_status = (ULONG)HAL_I2C_Init(&hi2c3);
  if (g_ps_phase5_threadx_probe.phase6_resume_i2c_init_status == (ULONG)HAL_OK)
  {
    g_ps_phase5_threadx_probe.phase6_i2c_deinit_status = 0xFFFFFFFEUL;
  }

  PS_Phase6_RecordStage(PS_PHASE6_STAGE_TRIGGERED - 1U);
}
static void PS_Phase6_StopLpbamDisplayAfterWake(void)
{
  HAL_StatusTypeDef status;

  g_ps_phase5_threadx_probe.phase6_lpbam_abort_attempted = 1U;

  status = HAL_LPTIM_PWM_Stop(&hlptim1, LPTIM_CHANNEL_1);
  g_ps_phase5_threadx_probe.phase6_lpbam_lptim_stop_status = (ULONG)status;
  __HAL_LPTIM_DISABLE(&hlptim1);

  status = HAL_DMA_Abort(&handle_LPDMA1_Channel0);
  g_ps_phase5_threadx_probe.phase6_lpbam_dma_abort_status = (ULONG)status;

  status = HAL_DMAEx_List_UnLinkQ(&handle_LPDMA1_Channel0);
  g_ps_phase5_threadx_probe.phase6_lpbam_dma_unlink_status = (ULONG)status;
  ps_phase6_lpbam_prearmed = 0U;

  status = HAL_SPI_Abort(&hspi3);
  g_ps_phase5_threadx_probe.phase6_lpbam_spi_abort_status = (ULONG)status;

  if (handle_LPDMA1_Channel0.Instance != NULL)
  {
    g_ps_phase5_threadx_probe.phase6_lpbam_dma_state_after_abort =
        (ULONG)handle_LPDMA1_Channel0.State;
    g_ps_phase5_threadx_probe.phase6_lpbam_dma_error_after_abort =
        (ULONG)handle_LPDMA1_Channel0.ErrorCode;
    g_ps_phase5_threadx_probe.phase6_lpbam_dma_csr_after_abort =
        handle_LPDMA1_Channel0.Instance->CSR;
  }

  if (hspi3.Instance != NULL)
  {
    g_ps_phase5_threadx_probe.phase6_lpbam_spi_state_after_abort =
        (ULONG)HAL_SPI_GetState(&hspi3);
    g_ps_phase5_threadx_probe.phase6_lpbam_spi_error_after_abort =
        (ULONG)HAL_SPI_GetError(&hspi3);
    g_ps_phase5_threadx_probe.phase6_lpbam_spi_sr_after_abort = hspi3.Instance->SR;
  }

  if (hlptim1.Instance != NULL)
  {
    g_ps_phase5_threadx_probe.phase6_lpbam_lptim_cr_after_abort = hlptim1.Instance->CR;
  }

  g_ps_phase5_threadx_probe.phase6_lpbam_post_wake_marker_status =
      (ULONG)PS_Phase6_RestoreDisplaySpiAfterLpbam();
  if (g_ps_phase5_threadx_probe.phase6_lpbam_post_wake_marker_status == (ULONG)HAL_OK)
  {
    g_ps_phase5_threadx_probe.phase6_lpbam_post_wake_marker_status =
        (ULONG)PS_DisplayOwnerPresentAuthoredFrameA();
  }
}
#if PS_PHASE6_LPBAM_START_AUTONOMOUS
static HAL_StatusTypeDef PS_Phase6_WaitLptimFlag(uint32_t flag)
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

static HAL_StatusTypeDef PS_Phase6_PrearmLpbamDisplayStopTransfer(void)
{
  HAL_StatusTypeDef status;

  if (ps_phase6_lpbam_prearmed != 0U)
  {
    return HAL_OK;
  }

  g_ps_phase5_threadx_probe.phase6_lpbam_display_enabled = PS_PHASE6_LPBAM_DISPLAY_EXPERIMENT;
  g_ps_phase5_threadx_probe.phase6_lpbam_display_rows = PS_PHASE6_LPBAM_DISPLAY_ROWS;
  if (hspi3.Instance != NULL)
  {
    g_ps_phase5_threadx_probe.phase6_lpbam_spi_autocr_before = hspi3.Instance->AUTOCR;
  }

  ps_phase6_lpbam_rebuild_count++;
  g_ps_phase5_threadx_probe.phase6_lpbam_rebuild_count = ps_phase6_lpbam_rebuild_count;
  g_ps_phase5_threadx_probe.phase6_lpbam_compile_variant = 0x6CUL;
  PS_LpbamDisplay_SetExperimentVariant(0U);

  status = PS_DisplayOwnerEnsureReady();
  g_ps_phase5_threadx_probe.phase6_lpbam_prepare_status = (ULONG)status;
  if (status != HAL_OK)
  {
    return status;
  }

#if PS_PHASE6_LPBAM_DISPLAY_EXPERIMENT && PS_PHASE6_LPBAM_START_AUTONOMOUS && PS_PHASE6_LPBAM_LATENCY_ONLY
  status = PS_DisplayOwnerPresentAuthoredFrameA();
  if (status != HAL_OK)
  {
    g_ps_phase5_threadx_probe.phase6_lpbam_start_status = (ULONG)status;
    return status;
  }
#endif

  status = PS_LpbamDisplay_BuildPatternBuffers(PS_PHASE6_LPBAM_DISPLAY_START_ROW,
                                               PS_PHASE6_LPBAM_DISPLAY_ROWS);
  g_ps_phase5_threadx_probe.phase6_lpbam_fill_status = (ULONG)status;
  if (status != HAL_OK)
  {
    return status;
  }

  HAL_GPIO_WritePin(VLT_LCD_GPIO_Port, VLT_LCD_Pin, GPIO_PIN_SET);

  status = PS_Phase6_EnableLpbamDisplayAutonomousClocks();
  if (status != HAL_OK)
  {
    g_ps_phase5_threadx_probe.phase6_lpbam_start_status = (ULONG)status;
    return status;
  }

  __HAL_LPTIM_DISABLE(&hlptim1);
  MODIFY_REG(hlptim1.Instance->CFGR, LPTIM_CFGR_PRESC, LPTIM_PRESCALER_DIV128);
  __HAL_LPTIM_ENABLE(&hlptim1);

  __HAL_LPTIM_CLEAR_FLAG(&hlptim1, LPTIM_FLAG_ARROK);
  __HAL_LPTIM_AUTORELOAD_SET(&hlptim1, PS_PHASE6_LPBAM_LPTIM_250MS_ARR);
  status = PS_Phase6_WaitLptimFlag(LPTIM_FLAG_ARROK);
  g_ps_phase5_threadx_probe.phase6_lpbam_lptim_arr_wait_status = (ULONG)status;
  if (status != HAL_OK)
  {
    g_ps_phase5_threadx_probe.phase6_lpbam_start_status = (ULONG)status;
    return status;
  }

  __HAL_LPTIM_CLEAR_FLAG(&hlptim1, LPTIM_FLAG_CMP1OK);
  __HAL_LPTIM_COMPARE_SET(&hlptim1, LPTIM_CHANNEL_1, PS_PHASE6_LPBAM_LPTIM_250MS_CMP);
  status = PS_Phase6_WaitLptimFlag(LPTIM_FLAG_CMP1OK);
  g_ps_phase5_threadx_probe.phase6_lpbam_lptim_cmp_wait_status = (ULONG)status;
  if (status != HAL_OK)
  {
    g_ps_phase5_threadx_probe.phase6_lpbam_start_status = (ULONG)status;
    return status;
  }

  MX_LpbamAp1_Scenario_Build();
  MX_LpbamAp1_Scenario_Link(&handle_LPDMA1_Channel0);
  g_ps_phase5_threadx_probe.phase6_lpbam_dma_mode_after_link = handle_LPDMA1_Channel0.Mode;
  g_ps_phase5_threadx_probe.phase6_lpbam_dma_state_after_link = handle_LPDMA1_Channel0.State;
  g_ps_phase5_threadx_probe.phase6_lpbam_dma_error_after_link = handle_LPDMA1_Channel0.ErrorCode;
  g_ps_phase5_threadx_probe.phase6_lpbam_queue_head = (ULONG)(uintptr_t)Queue1_Q.Head;
  g_ps_phase5_threadx_probe.phase6_lpbam_queue_first_circular = (ULONG)(uintptr_t)Queue1_Q.FirstCircularNode;
  g_ps_phase5_threadx_probe.phase6_lpbam_queue_node_count = Queue1_Q.NodeNumber;
  g_ps_phase5_threadx_probe.phase6_lpbam_queue_state_after_link = Queue1_Q.State;
  g_ps_phase5_threadx_probe.phase6_lpbam_queue_error_after_link = Queue1_Q.ErrorCode;
  g_ps_phase5_threadx_probe.phase6_lpbam_queue_type_after_link = Queue1_Q.Type;

  ps_phase6_lpbam_prearmed = 1U;
  g_ps_phase5_threadx_probe.phase6_lpbam_prearm_tick = tx_time_get();
  g_ps_phase5_threadx_probe.phase6_lpbam_prearm_status = (ULONG)HAL_OK;
  return HAL_OK;
}

static HAL_StatusTypeDef PS_Phase6_PrepareLpbamDisplayStopTransfer(void)
{
  HAL_StatusTypeDef status;

  g_ps_phase5_threadx_probe.phase6_lpbam_display_enabled = PS_PHASE6_LPBAM_DISPLAY_EXPERIMENT;
  g_ps_phase5_threadx_probe.phase6_lpbam_display_rows = PS_PHASE6_LPBAM_DISPLAY_ROWS;
  g_ps_phase5_threadx_probe.phase6_lpbam_compile_variant = 0x6CUL;
  g_ps_phase5_threadx_probe.phase6_lpbam_start_request_tick = tx_time_get();

  status = PS_Phase6_PrearmLpbamDisplayStopTransfer();
  if (status != HAL_OK)
  {
    g_ps_phase5_threadx_probe.phase6_lpbam_start_status = (ULONG)status;
    return status;
  }

  status = HAL_DMAEx_List_Start(&handle_LPDMA1_Channel0);
  g_ps_phase5_threadx_probe.phase6_lpbam_dma_start_status = (ULONG)status;
  g_ps_phase5_threadx_probe.phase6_lpbam_dma_state_after_start = handle_LPDMA1_Channel0.State;
  g_ps_phase5_threadx_probe.phase6_lpbam_dma_error_after_start = handle_LPDMA1_Channel0.ErrorCode;
  g_ps_phase5_threadx_probe.phase6_lpbam_queue_state_after_start = Queue1_Q.State;
  g_ps_phase5_threadx_probe.phase6_lpbam_queue_error_after_start = Queue1_Q.ErrorCode;
  g_ps_phase5_threadx_probe.phase6_lpbam_dma_ccr_after_start = handle_LPDMA1_Channel0.Instance->CCR;
  g_ps_phase5_threadx_probe.phase6_lpbam_dma_csr_after_start = handle_LPDMA1_Channel0.Instance->CSR;
  g_ps_phase5_threadx_probe.phase6_lpbam_dma_cllr_after_start = handle_LPDMA1_Channel0.Instance->CLLR;
  if (hspi3.Instance != NULL)
  {
    g_ps_phase5_threadx_probe.phase6_lpbam_spi_cr1_after_dma_start = hspi3.Instance->CR1;
    g_ps_phase5_threadx_probe.phase6_lpbam_spi_autocr_after_dma_start = hspi3.Instance->AUTOCR;
  }
  if (status != HAL_OK)
  {
    g_ps_phase5_threadx_probe.phase6_lpbam_start_status = (ULONG)status;
    return status;
  }

  g_ps_phase5_threadx_probe.phase6_lpbam_dma_started_tick = tx_time_get();

  __HAL_LPTIM_RESET_COUNTER(&hlptim1);
  status = HAL_LPTIM_PWM_Start(&hlptim1, LPTIM_CHANNEL_1);
  g_ps_phase5_threadx_probe.phase6_lpbam_lptim_start_status = (ULONG)status;
  if (status != HAL_OK)
  {
    g_ps_phase5_threadx_probe.phase6_lpbam_start_status = (ULONG)status;
    return status;
  }

  g_ps_phase5_threadx_probe.phase6_lpbam_lptim_started_tick = tx_time_get();
g_ps_phase5_threadx_probe.phase6_lpbam_start_status = (ULONG)HAL_OK;
  if (hspi3.Instance != NULL)
  {
    g_ps_phase5_threadx_probe.phase6_lpbam_spi_autocr_after = hspi3.Instance->AUTOCR;
  }
  PS_Phase6_RecordLpbamDisplayStopState(0U);

  return HAL_OK;
}
#endif
static void PS_Phase6_ParkNinaInterfacePins(void)
{
  GPIO_InitTypeDef gpio = {0};

  gpio.Mode = GPIO_MODE_ANALOG;
  gpio.Pull = GPIO_NOPULL;

  gpio.Pin = GPIO_PIN_2 | GPIO_PIN_3;
  HAL_GPIO_Init(GPIOA, &gpio);

  gpio.Pin = GPIO_PIN_12 | GPIO_PIN_13;
  HAL_GPIO_Init(GPIOB, &gpio);

  gpio.Pin = NINA_SW1_Pin | NINA_SW2_Pin | NINA_DTR_Pin | NINA_DSR_Pin;
  HAL_GPIO_Init(GPIOC, &gpio);

  g_ps_phase5_threadx_probe.phase6_nina_pin_park_done = 1U;
  g_ps_phase5_threadx_probe.phase6_nina_gpioa_moder_after = GPIOA->MODER;
  g_ps_phase5_threadx_probe.phase6_nina_gpiob_moder_after = GPIOB->MODER;
  g_ps_phase5_threadx_probe.phase6_nina_gpioc_moder_after = GPIOC->MODER;
}

static HAL_StatusTypeDef PS_Phase6_AT25CommandOnly(uint8_t instruction)
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

  return HAL_OSPI_Command(&hospi1, &command, PS_PHASE6_OSPI_TIMEOUT_MS);
}

static int32_t PS_Phase6_LIS2DUX12_ReadReg(void *handle, uint8_t reg, uint8_t *data, uint16_t len)
{
  uint16_t address = (uint16_t)(uintptr_t)handle;
  HAL_StatusTypeDef status = HAL_I2C_Mem_Read(&hi2c3,
                                              address,
                                              reg,
                                              I2C_MEMADD_SIZE_8BIT,
                                              data,
                                              len,
                                              PS_PHASE6_I2C_TIMEOUT_MS);

  return (status == HAL_OK) ? 0 : -1;
}

static int32_t PS_Phase6_LIS2DUX12_WriteReg(void *handle,
                                            uint8_t reg,
                                            const uint8_t *data,
                                            uint16_t len)
{
  uint16_t address = (uint16_t)(uintptr_t)handle;
  HAL_StatusTypeDef status = HAL_I2C_Mem_Write(&hi2c3,
                                               address,
                                               reg,
                                               I2C_MEMADD_SIZE_8BIT,
                                               (uint8_t *)data,
                                               len,
                                               PS_PHASE6_I2C_TIMEOUT_MS);

  return (status == HAL_OK) ? 0 : -1;
}

static HAL_StatusTypeDef PS_Phase6_TMAG3001_ReadReg(uint8_t reg, uint8_t *data)
{
  return HAL_I2C_Mem_Read(&hi2c3,
                          PS_PHASE6_TMAG3001_ADDR,
                          reg,
                          I2C_MEMADD_SIZE_8BIT,
                          data,
                          1U,
                          PS_PHASE6_I2C_TIMEOUT_MS);
}

static HAL_StatusTypeDef PS_Phase6_TMAG3001_WriteReg(uint8_t reg, uint8_t data)
{
  return HAL_I2C_Mem_Write(&hi2c3,
                           PS_PHASE6_TMAG3001_ADDR,
                           reg,
                           I2C_MEMADD_SIZE_8BIT,
                           &data,
                           1U,
                           PS_PHASE6_I2C_TIMEOUT_MS);
}

static void PS_Phase6_WaitForOwnerQuiesceAcks(void)
{
  ULONG start_tick = tx_time_get();

  while (((g_ps_phase5_threadx_probe.phase6_quiesce_ack_mask &
           PS_PHASE6_ALL_OWNER_ACK_MASK) != PS_PHASE6_ALL_OWNER_ACK_MASK) &&
         ((tx_time_get() - start_tick) < PS_PHASE6_QUIESCE_ACK_TIMEOUT_TICKS))
  {
    tx_thread_sleep(1U);
  }

  g_ps_phase5_threadx_probe.phase6_quiesce_ack_wait_ticks =
      tx_time_get() - start_tick;
  g_ps_phase5_threadx_probe.phase6_quiesce_ack_wait_complete =
      ((g_ps_phase5_threadx_probe.phase6_quiesce_ack_mask &
        PS_PHASE6_ALL_OWNER_ACK_MASK) == PS_PHASE6_ALL_OWNER_ACK_MASK) ? 1U : 0U;
}

static void PS_Phase6_RecordStage(ULONG stage)
{
  g_ps_phase5_threadx_probe.phase6_stage = stage;
  g_ps_phase5_threadx_probe.phase6_stage_tick = tx_time_get();
}

static void PS_Phase6_HoldStage(ULONG ticks)
{
  ULONG start_tick = tx_time_get();

  g_ps_phase5_threadx_probe.phase6_stage_hold_ticks = ticks;

  while ((tx_time_get() - start_tick) < ticks)
  {
    g_ps_phase5_threadx_probe.heartbeat[PS_THREADX_OWNER_POWER]++;
    g_ps_phase5_threadx_probe.last_time[PS_THREADX_OWNER_POWER] = tx_time_get();
    PS_ThreadXRecordPerformance();
    tx_thread_sleep(10U);
  }
}

static void PS_Phase6_DeInitIdlePeripherals(void)
{
  if (g_ps_phase5_threadx_probe.phase6_peripheral_deinit_done != 0U)
  {
    return;
  }

  PS_Phase6_PrepareExternalDevicesForSleep();
  PS_Phase6_EnablePmicPulseStopForSleep();

  g_ps_phase5_threadx_probe.phase6_audio_lptim_stop_status =
      (ULONG)HAL_LPTIM_PWM_Stop(&hlptim1, LPTIM_CHANNEL_1);
  g_ps_phase5_threadx_probe.phase6_audio_sai_stop_status =
      (ULONG)HAL_SAI_DMAStop(&hsai_BlockA1);

  g_ps_phase5_threadx_probe.phase6_adc_deinit_status =
      (ULONG)HAL_ADC_DeInit(&hadc1);
  g_ps_phase5_threadx_probe.phase6_ospi_deinit_status =
      (ULONG)HAL_OSPI_DeInit(&hospi1);
#if PS_PHASE6_LPBAM_DISPLAY_EXPERIMENT
  g_ps_phase5_threadx_probe.phase6_spi_deinit_status = 0xFFFFFFFEUL;
  g_ps_phase5_threadx_probe.phase6_lptim_deinit_status = 0xFFFFFFFDUL;
#else
  g_ps_phase5_threadx_probe.phase6_spi_deinit_status =
      (ULONG)HAL_SPI_DeInit(&hspi3);
  g_ps_phase5_threadx_probe.phase6_lptim_deinit_status =
      (ULONG)HAL_LPTIM_DeInit(&hlptim1);
#endif
  g_ps_phase5_threadx_probe.phase6_sai_deinit_status =
      (ULONG)HAL_SAI_DeInit(&hsai_BlockA1);
  g_ps_phase5_threadx_probe.phase6_uart_deinit_status =
      (ULONG)HAL_UART_DeInit(&hlpuart1);
  PS_Phase6_ParkNinaInterfacePins();
  g_ps_phase5_threadx_probe.phase6_nina_nrst_state =
      (ULONG)HAL_GPIO_ReadPin(NINA_NRST_GPIO_Port, NINA_NRST_Pin);

  __HAL_RCC_PLL2_DISABLE();
  g_ps_phase5_threadx_probe.phase6_pll2_disabled = 1U;
  __HAL_RCC_HSI_DISABLE();
  g_ps_phase5_threadx_probe.phase6_hsi_disabled = 1U;
#if PS_PHASE6_LPBAM_DISPLAY_EXPERIMENT
  __HAL_RCC_MSIK_ENABLE();
  __HAL_RCC_MSIKSTOP_ENABLE();
  g_ps_phase5_threadx_probe.phase6_msik_disabled = 0U;
#else
  __HAL_RCC_MSIKSTOP_DISABLE();
  __HAL_RCC_MSIK_DISABLE();
  g_ps_phase5_threadx_probe.phase6_msik_disabled = 1U;
#endif

  g_ps_phase5_threadx_probe.phase6_i2c_deinit_status =
      (ULONG)HAL_I2C_DeInit(&hi2c3);
  g_ps_phase5_threadx_probe.phase6_display_vlt_lcd_state =
      (ULONG)HAL_GPIO_ReadPin(VLT_LCD_GPIO_Port, VLT_LCD_Pin);
  g_ps_phase5_threadx_probe.phase6_audio_sd_mode_state =
      (ULONG)HAL_GPIO_ReadPin(SD_MODE_GPIO_Port, SD_MODE_Pin);
  g_ps_phase5_threadx_probe.phase6_peripheral_deinit_done = 1U;
}

static void PS_Phase6_EnterStop2ForCurrentTest(void)
{
#if PS_PHASE6_DEBUG_IN_STOP
  HAL_DBGMCU_EnableDBGStopMode();
#else
  HAL_DBGMCU_DisableDBGStopMode();
  HAL_DBGMCU_DisableDBGStandbyMode();
  CLEAR_BIT(DBGMCU->CR, DBGMCU_CR_DBG_STOP | DBGMCU_CR_DBG_STANDBY);
#endif
  HAL_PWREx_EnableUltraLowPowerMode();
  PS_Phase6_RearmButtonWakeInputs();
  HAL_SuspendTick();
  g_ps_phase5_threadx_probe.phase6_systick_ctrl_before_stop2 = SysTick->CTRL;
  SysTick->CTRL = 0U;
  g_ps_phase5_threadx_probe.phase6_systick_ctrl_after_disable = SysTick->CTRL;

#if PS_PHASE6_LPBAM_DISPLAY_EXPERIMENT
  g_ps_phase5_threadx_probe.phase6_nvic_icer0_after_disable = NVIC->ICER[0];
  g_ps_phase5_threadx_probe.phase6_nvic_icer1_after_disable = NVIC->ICER[1];
#else
  NVIC->ICER[0] = 0xFFFFFFFFUL;
  NVIC->ICER[1] = 0xFFFFFFFFUL;
  g_ps_phase5_threadx_probe.phase6_nvic_icer0_after_disable = NVIC->ICER[0];
  g_ps_phase5_threadx_probe.phase6_nvic_icer1_after_disable = NVIC->ICER[1];
  __disable_irq();
#endif

  for (;;)
  {
    g_ps_phase5_threadx_probe.phase6_wake_reason = PS_PHASE6_WAKE_REASON_NONE;
    g_ps_phase5_threadx_probe.phase6_wake_button_id = 0U;
    g_ps_phase5_threadx_probe.phase6_wake_pin = 0U;
    g_ps_phase5_threadx_probe.phase6_wake_active_level = 0U;
    g_ps_phase5_threadx_probe.phase6_wake_tick = 0U;
    g_ps_phase5_threadx_probe.phase6_wake_stage = 0U;
    g_ps_phase5_threadx_probe.phase6_wake_exti_rpr1 = 0U;
    g_ps_phase5_threadx_probe.phase6_wake_exti_fpr1 = 0U;
    g_ps_phase5_threadx_probe.phase6_stop2_attempted = 1U;
    g_ps_phase5_threadx_probe.phase6_stop2_entry_tick = tx_time_get();
    g_ps_phase5_threadx_probe.phase6_pwr_sr_before_stop2 = PWR->SR;
    g_ps_phase5_threadx_probe.phase6_pwr_wusr_before_stop2 = PWR->WUSR;
    g_ps_phase5_threadx_probe.phase6_scb_scr_before_stop2 = SCB->SCR;
    g_ps_phase5_threadx_probe.phase6_dbgmcu_cr_before_stop2 = DBGMCU->CR;
    g_ps_phase5_threadx_probe.phase6_exti_imr1_before_stop = EXTI->IMR1;
    g_ps_phase5_threadx_probe.phase6_exti_rtsr1_before_stop = EXTI->RTSR1;
    g_ps_phase5_threadx_probe.phase6_exti_ftsr1_before_stop = EXTI->FTSR1;
    g_ps_phase5_threadx_probe.phase6_exti_rpr1_before_stop = EXTI->RPR1;
    g_ps_phase5_threadx_probe.phase6_exti_fpr1_before_stop = EXTI->FPR1;
    g_ps_phase5_threadx_probe.phase6_nvic_iser0_before_stop = NVIC->ISER[0];
    g_ps_phase5_threadx_probe.phase6_nvic_ispr0_before_stop = NVIC->ISPR[0];
#if PS_PHASE6_LPBAM_DISPLAY_EXPERIMENT
    PS_Phase6_RecordLpbamDisplayStopState(0U);
#endif

    __HAL_PWR_CLEAR_FLAG(PWR_FLAG_STOPF);
    SET_BIT(PWR->WUSCR, PWR_WUSCR_CWUF);

    __DSB();
    __ISB();
    HAL_PWREx_EnterSTOP2Mode(PWR_STOPENTRY_WFI);

    if (g_ps_phase5_threadx_probe.phase6_wake_reason == PS_PHASE6_WAKE_REASON_NONE)
    {
      g_ps_phase5_threadx_probe.phase6_wake_reason = PS_PHASE6_WAKE_REASON_UNKNOWN;
      g_ps_phase5_threadx_probe.phase6_wake_tick = tx_time_get();
      g_ps_phase5_threadx_probe.phase6_wake_stage = g_ps_phase5_threadx_probe.phase6_stage;
      g_ps_phase5_threadx_probe.phase6_wake_exti_rpr1 = EXTI->RPR1;
      g_ps_phase5_threadx_probe.phase6_wake_exti_fpr1 = EXTI->FPR1;
    }
    g_ps_phase5_threadx_probe.phase6_stop2_returned = 1U;
    g_ps_phase5_threadx_probe.phase6_stop2_return_count++;
    g_ps_phase5_threadx_probe.phase6_stop2_return_tick = tx_time_get();
    g_ps_phase5_threadx_probe.phase6_pwr_sr_after_stop2 = PWR->SR;
    g_ps_phase5_threadx_probe.phase6_pwr_wusr_after_stop2 = PWR->WUSR;
    g_ps_phase5_threadx_probe.phase6_scb_scr_after_stop2 = SCB->SCR;
    g_ps_phase5_threadx_probe.phase6_dbgmcu_cr_after_stop2 = DBGMCU->CR;
    g_ps_phase5_threadx_probe.phase6_exti_imr1_after_wake = EXTI->IMR1;
    g_ps_phase5_threadx_probe.phase6_exti_rpr1_after_wake = EXTI->RPR1;
    g_ps_phase5_threadx_probe.phase6_exti_fpr1_after_wake = EXTI->FPR1;
    g_ps_phase5_threadx_probe.phase6_nvic_ispr0_after_wake = NVIC->ISPR[0];
#if PS_PHASE6_LPBAM_DISPLAY_EXPERIMENT
    PS_Phase6_RecordLpbamDisplayStopState(1U);

    g_ps_phase5_threadx_probe.phase6_stop2_clock_restore_attempted = 1U;
    SystemClock_Config();
    SysTick->CTRL = g_ps_phase5_threadx_probe.phase6_systick_ctrl_before_stop2;
    HAL_ResumeTick();
    g_ps_phase5_threadx_probe.phase6_systick_ctrl_after_restore = SysTick->CTRL;
    g_ps_phase5_threadx_probe.phase6_stop2_clock_restore_done = 1U;
    PS_Phase6_StopLpbamDisplayAfterWake();
    return;
#endif
  }
}

static void PS_InputOwnerEntry(void)
{
  ULONG event[PS_INPUT_RAW_EVENT_WORDS];

  for (;;)
  {
    UINT status = tx_queue_receive(&ps_input_raw_queue, event, 100U);

    g_ps_phase5_threadx_probe.heartbeat[PS_THREADX_OWNER_INPUT]++;
    g_ps_phase5_threadx_probe.last_time[PS_THREADX_OWNER_INPUT] = tx_time_get();
    if (ps_phase6_quiesce_requested != 0U)
    {
      g_ps_phase5_threadx_probe.phase6_quiesce_ack_mask |=
          PS_PHASE6_OWNER_ACK_MASK(PS_THREADX_OWNER_INPUT);
    }

    if (status == TX_SUCCESS)
    {
      ULONG button_id = event[1];
      ULONG active_level = event[2];

      g_ps_phase5_threadx_probe.input_queue_received++;
      g_ps_phase5_threadx_probe.input_last_sequence = event[0];
      g_ps_phase5_threadx_probe.input_last_button_id = button_id;
      g_ps_phase5_threadx_probe.input_last_active_level = active_level;
      g_ps_phase5_threadx_probe.input_last_event_tick = event[3];

      if ((button_id >= PS_INPUT_BUTTON_START) && (button_id <= PS_INPUT_BUTTON_R))
      {
        ULONG index = button_id - 1U;
        ULONG previous_level = ps_input_stable_level[index];
        ULONG debounce_elapsed = event[3] - ps_input_last_accept_tick[index];

        g_ps_phase5_threadx_probe.input_button_edges[index]++;
        if (active_level != 0U)
        {
          g_ps_phase5_threadx_probe.input_button_presses[index]++;
        }
        else
        {
          g_ps_phase5_threadx_probe.input_button_releases[index]++;
        }

        if ((debounce_elapsed < PS_INPUT_DEBOUNCE_TICKS) &&
            (ps_input_last_accept_tick[index] != 0U))
        {
          g_ps_phase5_threadx_probe.input_debounce_rejected[index]++;
        }
        else if (active_level == previous_level)
        {
          g_ps_phase5_threadx_probe.input_duplicate_level_ignored[index]++;
          ps_input_last_accept_tick[index] = event[3];
        }
        else
        {
          ps_input_stable_level[index] = active_level;
          ps_input_last_accept_tick[index] = event[3];
          g_ps_phase5_threadx_probe.input_debounced_edges[index]++;
          g_ps_phase5_threadx_probe.input_last_debounced_button_id = button_id;
          g_ps_phase5_threadx_probe.input_last_debounced_active_level = active_level;
          g_ps_phase5_threadx_probe.input_last_debounced_tick = event[3];

          if (active_level != 0U)
          {
            g_ps_phase5_threadx_probe.input_debounced_presses[index]++;
            g_ps_phase5_threadx_probe.input_debounced_mask |= (1UL << index);
          }
          else
          {
            g_ps_phase5_threadx_probe.input_debounced_releases[index]++;
            g_ps_phase5_threadx_probe.input_debounced_mask &= ~(1UL << index);
          }

          g_ps_phase5_threadx_probe.input_chord_mask =
          g_ps_phase5_threadx_probe.input_debounced_mask;
          if ((button_id == PS_INPUT_BUTTON_START) && (active_level != 0U))
          {
            ps_phase6_sleep_trigger_requested = 1U;
            g_ps_phase5_threadx_probe.phase6_sleep_trigger_requested = 1U;
            g_ps_phase5_threadx_probe.phase6_sleep_trigger_button_id = button_id;
            g_ps_phase5_threadx_probe.phase6_sleep_trigger_tick = event[3];
          }
          PS_InputPublishUIEvent(PS_UI_EVENT_BUTTON_EDGE,
                                 button_id,
                                 active_level,
                                 g_ps_phase5_threadx_probe.input_debounced_mask,
                                 event[3]);
          if ((active_level != 0U) &&
              ((g_ps_phase5_threadx_probe.input_debounced_mask &
                (g_ps_phase5_threadx_probe.input_debounced_mask - 1U)) != 0U))
          {
            g_ps_phase5_threadx_probe.input_chord_events++;
            PS_InputPublishUIEvent(PS_UI_EVENT_BUTTON_CHORD,
                                   button_id,
                                   active_level,
                                   g_ps_phase5_threadx_probe.input_debounced_mask,
                                   event[3]);
          }
        }
      }
    }
    else if (status == TX_QUEUE_EMPTY)
    {
      g_ps_phase5_threadx_probe.input_queue_receive_timeouts++;
    }
  }
}

static void PS_UIOwnerEntry(void)
{
  ULONG event[PS_UI_EVENT_WORDS];

  for (;;)
  {
    UINT status = tx_queue_receive(&ps_ui_event_queue, event, 100U);

    g_ps_phase5_threadx_probe.heartbeat[PS_THREADX_OWNER_UI]++;
    g_ps_phase5_threadx_probe.last_time[PS_THREADX_OWNER_UI] = tx_time_get();
    if (ps_phase6_quiesce_requested != 0U)
    {
      g_ps_phase5_threadx_probe.phase6_quiesce_ack_mask |=
          PS_PHASE6_OWNER_ACK_MASK(PS_THREADX_OWNER_UI);
    }

    if (status == TX_SUCCESS)
    {
      ULONG event_type = event[1];
      ULONG button_id = event[2];
      ULONG active_level = event[3] & 0xFFU;
      ULONG mask = event[3] >> 8;

      g_ps_phase5_threadx_probe.ui_queue_received++;
      g_ps_phase5_threadx_probe.ui_last_sequence = event[0];
      g_ps_phase5_threadx_probe.ui_last_event_type = event_type;
      g_ps_phase5_threadx_probe.ui_last_button_id = button_id;
      g_ps_phase5_threadx_probe.ui_last_active_level = active_level;
      g_ps_phase5_threadx_probe.ui_last_mask = mask;
      g_ps_phase5_threadx_probe.ui_last_event_tick = event[4];

      if (ps_phase6_quiesce_requested != 0U)
      {
        continue;
      }

      if ((event_type == PS_UI_EVENT_BUTTON_CHORD) || (active_level != 0U))
      {
        PS_UIPublishDisplayCmd(event_type, button_id, mask, event[4]);
        PS_UIPublishAudioCmd(event_type, button_id, mask, event[4]);
      }
      PS_UIPublishStorageCmd(event_type, button_id, mask, event[4]);
      PS_UIPublishCommsCmd(event_type, button_id, mask, event[4]);

      if ((button_id >= PS_INPUT_BUTTON_START) && (button_id <= PS_INPUT_BUTTON_R))
      {
        ULONG index = button_id - 1U;

        g_ps_phase5_threadx_probe.ui_button_events[index]++;
        if (event_type == PS_UI_EVENT_BUTTON_CHORD)
        {
          g_ps_phase5_threadx_probe.ui_chord_events++;
        }
        else if (active_level != 0U)
        {
          g_ps_phase5_threadx_probe.ui_button_presses[index]++;
        }
        else
        {
          g_ps_phase5_threadx_probe.ui_button_releases[index]++;
        }
      }
    }
    else if (status == TX_QUEUE_EMPTY)
    {
      g_ps_phase5_threadx_probe.ui_queue_receive_timeouts++;
    }
  }
}

static void PS_DisplayOwnerEntry(void)
{
  ULONG cmd[PS_DISPLAY_CMD_WORDS];
  uint8_t quiesced = 0U;

  for (;;)
  {
    UINT status = tx_queue_receive(&ps_display_cmd_queue,
                                   cmd,
                                   PS_DISPLAY_QUEUE_TIMEOUT_TICKS);

    g_ps_phase5_threadx_probe.heartbeat[PS_THREADX_OWNER_DISPLAY]++;
    g_ps_phase5_threadx_probe.last_time[PS_THREADX_OWNER_DISPLAY] = tx_time_get();
    if (ps_phase6_quiesce_requested != 0U)
    {
      if (quiesced == 0U)
      {
#if !(PS_PHASE6_LPBAM_DISPLAY_EXPERIMENT && PS_PHASE6_LPBAM_START_AUTONOMOUS)
        (void)PS_DisplayOwnerBlankForSleep();
#endif
        HAL_GPIO_WritePin(VLT_LCD_GPIO_Port, VLT_LCD_Pin, GPIO_PIN_SET);
        g_ps_phase5_threadx_probe.phase6_display_vlt_lcd_state =
            (ULONG)HAL_GPIO_ReadPin(VLT_LCD_GPIO_Port, VLT_LCD_Pin);
        quiesced = 1U;
      }
      g_ps_phase5_threadx_probe.phase6_quiesce_ack_mask |=
          PS_PHASE6_OWNER_ACK_MASK(PS_THREADX_OWNER_DISPLAY);
    }

    if (status == TX_SUCCESS)
    {
      g_ps_phase5_threadx_probe.display_queue_received++;
      g_ps_phase5_threadx_probe.display_last_sequence = cmd[0];
      g_ps_phase5_threadx_probe.display_last_cmd_type = cmd[1];
      g_ps_phase5_threadx_probe.display_last_source_event = cmd[2];
      g_ps_phase5_threadx_probe.display_last_button_id = cmd[3] & 0xFFU;
      g_ps_phase5_threadx_probe.display_last_mask = cmd[3] >> 8;
      g_ps_phase5_threadx_probe.display_last_event_tick = cmd[4];

      if (ps_phase6_quiesce_requested != 0U)
      {
        continue;
      }

      if (cmd[1] == PS_DISPLAY_CMD_ACTIVITY_HINT)
      {
        g_ps_phase5_threadx_probe.display_activity_hints++;
        if (g_ps_phase5_threadx_probe.power_rails_ready != 0U)
        {
          (void)PS_DisplayOwnerRunActivityHint();
        }
        else
        {
          g_ps_phase5_threadx_probe.display_power_not_ready_skips++;
        }
      }
      else if (cmd[1] == PS_DISPLAY_CMD_DIAG_FILL)
      {
        g_ps_phase5_threadx_probe.display_activity_hints++;
        if (g_ps_phase5_threadx_probe.power_rails_ready != 0U)
        {
          (void)PS_DisplayOwnerFill((uint8_t)(cmd[3] & 0xFFU));
        }
        else
        {
          g_ps_phase5_threadx_probe.display_power_not_ready_skips++;
        }
      }
    }
    else if (status == TX_QUEUE_EMPTY)
    {
      g_ps_phase5_threadx_probe.display_queue_receive_timeouts++;
    }
  }
}

static HAL_StatusTypeDef PS_DisplayOwnerRunActivityHint(void)
{
  HAL_StatusTypeDef status = PS_DisplayOwnerPresentAuthoredFrameA();

  if (status == HAL_OK)
  {
    g_ps_phase5_threadx_probe.display_last_action_mode = 0xA0U;
    g_ps_phase5_threadx_probe.display_action_count++;
  }

  return status;
}

static HAL_StatusTypeDef PS_DisplayOwnerEnsureReady(void)
{
  HAL_StatusTypeDef status;

  if (ps_display_renderer_ready != 0U)
  {
    return HAL_OK;
  }

  HAL_GPIO_WritePin(VLT_LCD_GPIO_Port, VLT_LCD_Pin, GPIO_PIN_SET);
  tx_thread_sleep(5U);

  status = LCD_Init(&ps_threadx_lcd, &hspi3);
  g_ps_phase5_threadx_probe.display_init_status = (ULONG)status;
  if (status != HAL_OK)
  {
    g_ps_phase5_threadx_probe.display_action_error_count++;
    g_ps_phase5_threadx_probe.display_last_hal_error = HAL_SPI_GetError(&hspi3);
    return status;
  }

  status = PS_DisplayRenderer_Init(&ps_threadx_display_renderer,
                                   &ps_threadx_lcd,
                                   PS_DISPLAY_DMA_TIMEOUT_MS);
  g_ps_phase5_threadx_probe.display_renderer_init_status = (ULONG)status;
  if (status != HAL_OK)
  {
    g_ps_phase5_threadx_probe.display_action_error_count++;
    g_ps_phase5_threadx_probe.display_last_hal_error = HAL_SPI_GetError(&hspi3);
    return status;
  }

  ps_display_renderer_ready = 1U;
  return HAL_OK;
}

static HAL_StatusTypeDef PS_DisplayOwnerFill(uint8_t fill_value)
{
  HAL_StatusTypeDef status = PS_DisplayOwnerEnsureReady();

  if (status != HAL_OK)
  {
    return status;
  }

  status = PS_DisplayRenderer_Fill(&ps_threadx_display_renderer, fill_value);
  g_ps_phase5_threadx_probe.display_last_fill_value = (ULONG)fill_value;
  g_ps_phase5_threadx_probe.display_fill_status = (ULONG)status;
  if (status != HAL_OK)
  {
    g_ps_phase5_threadx_probe.display_action_error_count++;
    g_ps_phase5_threadx_probe.display_last_hal_error = HAL_SPI_GetError(&hspi3);
    return status;
  }

  status = PS_DisplayRenderer_Present(&ps_threadx_display_renderer);
  g_ps_phase5_threadx_probe.display_present_status = (ULONG)status;
  g_ps_phase5_threadx_probe.display_last_hal_error = HAL_SPI_GetError(&hspi3);
  g_ps_phase5_threadx_probe.display_vlt_lcd_state =
      (ULONG)HAL_GPIO_ReadPin(VLT_LCD_GPIO_Port, VLT_LCD_Pin);
  g_ps_phase5_threadx_probe.display_spi_state_after = (ULONG)HAL_SPI_GetState(&hspi3);
  g_ps_phase5_threadx_probe.display_spi_error_after = HAL_SPI_GetError(&hspi3);
  g_ps_phase5_threadx_probe.display_dma_done_after = LCD_FlushDMA_IsDone() ? 1UL : 0UL;
  if (status != HAL_OK)
  {
    g_ps_phase5_threadx_probe.display_action_error_count++;
  }

  return status;
}
static HAL_StatusTypeDef PS_DisplayOwnerPresentAuthoredFrameA(void)
{
  HAL_StatusTypeDef status = PS_DisplayOwnerEnsureReady();

  if (status != HAL_OK)
  {
    return status;
  }

  PS_LpbamDisplay_ComposeExperimentFrames();
  HAL_GPIO_WritePin(VLT_LCD_GPIO_Port, VLT_LCD_Pin, GPIO_PIN_SET);

  status = LCD_PresentFull_DMA(&ps_threadx_lcd,
                               (const uint8_t *)ps_lpbam_display_frame_a,
                               PS_DISPLAY_DMA_TIMEOUT_MS);
  g_ps_phase5_threadx_probe.display_fill_status = (ULONG)HAL_OK;
  g_ps_phase5_threadx_probe.display_present_status = (ULONG)status;
  g_ps_phase5_threadx_probe.display_last_fill_value = 0xA0U;
  g_ps_phase5_threadx_probe.display_last_hal_error = HAL_SPI_GetError(&hspi3);
  g_ps_phase5_threadx_probe.display_vlt_lcd_state =
      (ULONG)HAL_GPIO_ReadPin(VLT_LCD_GPIO_Port, VLT_LCD_Pin);
  g_ps_phase5_threadx_probe.display_spi_state_after = (ULONG)HAL_SPI_GetState(&hspi3);
  g_ps_phase5_threadx_probe.display_spi_error_after = HAL_SPI_GetError(&hspi3);
  g_ps_phase5_threadx_probe.display_dma_done_after = LCD_FlushDMA_IsDone() ? 1UL : 0UL;
  if (status != HAL_OK)
  {
    g_ps_phase5_threadx_probe.display_action_error_count++;
  }

  return status;
}
static HAL_StatusTypeDef PS_DisplayOwnerBlankForSleep(void)
{
  HAL_StatusTypeDef status;

  if (ps_display_renderer_ready == 0U)
  {
    return HAL_OK;
  }

  HAL_GPIO_WritePin(VLT_LCD_GPIO_Port, VLT_LCD_Pin, GPIO_PIN_SET);
  status = PS_DisplayRenderer_Fill(&ps_threadx_display_renderer, 0xFFU);
  g_ps_phase5_threadx_probe.display_fill_status = (ULONG)status;
  g_ps_phase5_threadx_probe.display_last_fill_value = 0xFFU;
  if (status != HAL_OK)
  {
    g_ps_phase5_threadx_probe.display_action_error_count++;
    g_ps_phase5_threadx_probe.display_last_hal_error = HAL_SPI_GetError(&hspi3);
    return status;
  }

  status = PS_DisplayRenderer_Present(&ps_threadx_display_renderer);
  g_ps_phase5_threadx_probe.display_present_status = (ULONG)status;
  g_ps_phase5_threadx_probe.display_last_hal_error = HAL_SPI_GetError(&hspi3);
  if (status != HAL_OK)
  {
    g_ps_phase5_threadx_probe.display_action_error_count++;
    return status;
  }

  return HAL_OK;
}

static void PS_StorageOwnerEntry(void)
{
  ULONG cmd[PS_STORAGE_CMD_WORDS];

  for (;;)
  {
    UINT status = tx_queue_receive(&ps_storage_cmd_queue, cmd, 100U);

    g_ps_phase5_threadx_probe.heartbeat[PS_THREADX_OWNER_STORAGE]++;
    g_ps_phase5_threadx_probe.last_time[PS_THREADX_OWNER_STORAGE] = tx_time_get();
    if (ps_phase6_quiesce_requested != 0U)
    {
      g_ps_phase5_threadx_probe.phase6_quiesce_ack_mask |=
          PS_PHASE6_OWNER_ACK_MASK(PS_THREADX_OWNER_STORAGE);
    }

    if (status == TX_SUCCESS)
    {
      g_ps_phase5_threadx_probe.storage_queue_received++;
      g_ps_phase5_threadx_probe.storage_last_sequence = cmd[0];
      g_ps_phase5_threadx_probe.storage_last_cmd_type = cmd[1];
      g_ps_phase5_threadx_probe.storage_last_source_event = cmd[2];
      g_ps_phase5_threadx_probe.storage_last_button_id = cmd[3] & 0xFFU;
      g_ps_phase5_threadx_probe.storage_last_mask = cmd[3] >> 8;
      g_ps_phase5_threadx_probe.storage_last_event_tick = cmd[4];

      if (cmd[1] == PS_STORAGE_CMD_ACTIVITY_HINT)
      {
        g_ps_phase5_threadx_probe.storage_activity_hints++;
      }
    }
    else if (status == TX_QUEUE_EMPTY)
    {
      g_ps_phase5_threadx_probe.storage_queue_receive_timeouts++;
    }
  }
}

static void __attribute__((unused)) PS_CommsRunNinaPowerCapabilityProbe(void)
{
  static const uint8_t at_cmd[] = "AT\r\n";
  static const uint8_t upm_query_cmd[] = "AT+UPM?\r\n";
  static const uint8_t upm_test_cmd[] = "AT+UPM=?\r\n";
  static const uint8_t command_list_cmd[] = "AT+CLAC\r\n";
  uint8_t rx[128] = {0};
  uint16_t rx_len;

  g_ps_nina_power_probe.magic = PS_NINA_POWER_PROBE_MAGIC;
  g_ps_nina_power_probe.phase = PS_NINA_POWER_PROBE_PHASE;

  HAL_GPIO_WritePin(NINA_NRST_GPIO_Port, NINA_NRST_Pin, GPIO_PIN_RESET);
  tx_thread_sleep(PS_PHASE6_NINA_RESET_ASSERT_TICKS);
  HAL_GPIO_WritePin(NINA_NRST_GPIO_Port, NINA_NRST_Pin, GPIO_PIN_SET);
  tx_thread_sleep(PS_NINA_CAPABILITY_BOOT_WAIT_TICKS);
  g_ps_nina_power_probe.boot_rx_len =
      PS_Phase6_NinaReceiveUntilQuiet(rx, sizeof(rx),
                                      PS_PHASE6_NINA_BOOT_DRAIN_TICKS,
                                      PS_PHASE6_NINA_RX_QUIET_TICKS);

  for (uint16_t i = 0U; i < sizeof(rx); i++) { rx[i] = 0U; }
  g_ps_nina_power_probe.at_tx_status = (ULONG)HAL_UART_Transmit(
      &hlpuart1, at_cmd, sizeof(at_cmd) - 1U, 250U);
  rx_len = PS_Phase6_NinaReceiveUntilQuiet(rx, sizeof(rx),
                                            PS_PHASE6_NINA_AT_RX_TICKS,
                                            PS_PHASE6_NINA_RX_QUIET_TICKS);
  g_ps_nina_power_probe.at_rx_len = rx_len;
  g_ps_nina_power_probe.at_ok = PS_Phase6_BufferContainsToken(rx, rx_len, "OK");
  g_ps_nina_power_probe.at_rx_word0 = PS_Phase6_PackBytes(rx, rx_len, 0U);
  g_ps_nina_power_probe.at_rx_word1 = PS_Phase6_PackBytes(rx, rx_len, 4U);

  for (uint16_t i = 0U; i < sizeof(rx); i++) { rx[i] = 0U; }
  g_ps_nina_power_probe.upm_query_tx_status = (ULONG)HAL_UART_Transmit(
      &hlpuart1, upm_query_cmd, sizeof(upm_query_cmd) - 1U, 250U);
  rx_len = PS_Phase6_NinaReceiveUntilQuiet(rx, sizeof(rx),
                                            PS_PHASE6_NINA_AT_RX_TICKS,
                                            PS_PHASE6_NINA_RX_QUIET_TICKS);
  g_ps_nina_power_probe.upm_query_rx_len = rx_len;
  g_ps_nina_power_probe.upm_query_ok = PS_Phase6_BufferContainsToken(rx, rx_len, "OK");
  g_ps_nina_power_probe.upm_query_rx_word0 = PS_Phase6_PackBytes(rx, rx_len, 0U);
  g_ps_nina_power_probe.upm_query_rx_word1 = PS_Phase6_PackBytes(rx, rx_len, 4U);

  for (uint16_t i = 0U; i < sizeof(rx); i++) { rx[i] = 0U; }
  g_ps_nina_power_probe.upm_test_tx_status = (ULONG)HAL_UART_Transmit(
      &hlpuart1, upm_test_cmd, sizeof(upm_test_cmd) - 1U, 250U);
  rx_len = PS_Phase6_NinaReceiveUntilQuiet(rx, sizeof(rx),
                                            PS_PHASE6_NINA_AT_RX_TICKS,
                                            PS_PHASE6_NINA_RX_QUIET_TICKS);
  g_ps_nina_power_probe.upm_test_rx_len = rx_len;
  g_ps_nina_power_probe.upm_test_ok = PS_Phase6_BufferContainsToken(rx, rx_len, "OK");
  g_ps_nina_power_probe.upm_test_rx_word0 = PS_Phase6_PackBytes(rx, rx_len, 0U);
  g_ps_nina_power_probe.upm_test_rx_word1 = PS_Phase6_PackBytes(rx, rx_len, 4U);

  for (uint16_t i = 0U; i < sizeof(g_ps_nina_power_probe.command_list_rx); i++)
  {
    g_ps_nina_power_probe.command_list_rx[i] = 0U;
  }
  g_ps_nina_power_probe.command_list_tx_status = (ULONG)HAL_UART_Transmit(
      &hlpuart1, command_list_cmd, sizeof(command_list_cmd) - 1U, 250U);
  g_ps_nina_power_probe.command_list_rx_len = PS_Phase6_NinaReceiveUntilQuiet(
      (uint8_t *)g_ps_nina_power_probe.command_list_rx,
      sizeof(g_ps_nina_power_probe.command_list_rx),
      PS_NINA_CAPABILITY_LIST_RX_TICKS,
      PS_PHASE6_NINA_RX_QUIET_TICKS);
  g_ps_nina_power_probe.command_list_ok = PS_Phase6_BufferContainsToken(
      (const uint8_t *)g_ps_nina_power_probe.command_list_rx,
      (uint16_t)g_ps_nina_power_probe.command_list_rx_len,
      "OK");

  HAL_GPIO_WritePin(NINA_NRST_GPIO_Port, NINA_NRST_Pin, GPIO_PIN_RESET);
  g_ps_nina_power_probe.final_nrst_state =
      (ULONG)HAL_GPIO_ReadPin(NINA_NRST_GPIO_Port, NINA_NRST_Pin);
  g_ps_nina_power_probe.final_nrst_moder = GPIOC->MODER;
  g_ps_nina_power_probe.final_nrst_odr = GPIOC->ODR;
  g_ps_nina_power_probe.final_nrst_idr = GPIOC->IDR;
  g_ps_nina_power_probe.complete = 1U;
}


static void PS_CommsSampleNinaSpsFlow(volatile PsNinaSpsProbe *probe, uint32_t index)
{
  if (index >= 4U)
  {
    return;
  }

  probe->flow_gpioa_moder[index] = GPIOA->MODER;
  probe->flow_gpiob_moder[index] = GPIOB->MODER;
  probe->flow_gpiob_odr[index] = GPIOB->ODR;
  probe->flow_gpiob_idr[index] = GPIOB->IDR;
  probe->flow_gpioc_moder[index] = GPIOC->MODER;
  probe->flow_gpioc_idr[index] = GPIOC->IDR;
  probe->flow_uart_cr1[index] = LPUART1->CR1;
  probe->flow_uart_cr2[index] = LPUART1->CR2;
  probe->flow_uart_cr3[index] = LPUART1->CR3;
  probe->flow_uart_isr[index] = LPUART1->ISR;
}

static HAL_StatusTypeDef PS_CommsConfigureNinaSpsFlowDiag(volatile PsNinaSpsProbe *probe)
{
  GPIO_InitTypeDef gpio = {0};
  HAL_StatusTypeDef status;

  probe->uart_flow_diag_mode = PS_NINA_SPS_FLOW_DIAG_MODE;
  if (PS_NINA_SPS_FLOW_DIAG_MODE == 0U)
  {
    probe->uart_reinit_status = 0U;
    return HAL_OK;
  }

  status = HAL_UART_DeInit(&hlpuart1);
  if (status != HAL_OK)
  {
    probe->uart_reinit_status = (ULONG)status;
    return status;
  }

  hlpuart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  status = HAL_UART_Init(&hlpuart1);
  probe->uart_reinit_status = (ULONG)status;
  if (status != HAL_OK)
  {
    return status;
  }

  __HAL_RCC_GPIOB_CLK_ENABLE();
  gpio.Pin = GPIO_PIN_12;
  gpio.Mode = GPIO_MODE_OUTPUT_PP;
  gpio.Pull = GPIO_NOPULL;
  gpio.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &gpio);
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12,
                    (PS_NINA_SPS_FLOW_DIAG_MODE == 1U) ? GPIO_PIN_SET : GPIO_PIN_RESET);

  gpio.Pin = GPIO_PIN_13;
  gpio.Mode = GPIO_MODE_ANALOG;
  gpio.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOB, &gpio);

  return HAL_OK;
}

static void __attribute__((unused)) PS_CommsRunNinaSpsProbe(void)
{
  static const uint8_t at_cmd[] = "AT\r\n";
  static const uint8_t setup_cmds[9][16] = {
      "AT+UMRS?\r\n",
      "ATI9\r\n",
      "AT+UBTLN?\r\n",
      "AT+UBTLE?\r\n",
      "AT+UDSC=0\r\n",
      "AT+UDSF=0\r\n",
      "AT+UBTDM=3\r\n",
      "AT+UBTCM=2\r\n",
      "AT+UBTPM=2\r\n",
  };
  static const uint8_t setup_lens[9] = {
      (uint8_t)(sizeof("AT+UMRS?\r\n") - 1U),
      (uint8_t)(sizeof("ATI9\r\n") - 1U),
      (uint8_t)(sizeof("AT+UBTLN?\r\n") - 1U),
      (uint8_t)(sizeof("AT+UBTLE?\r\n") - 1U),
      (uint8_t)(sizeof("AT+UDSC=0\r\n") - 1U),
      (uint8_t)(sizeof("AT+UDSF=0\r\n") - 1U),
      (uint8_t)(sizeof("AT+UBTDM=3\r\n") - 1U),
      (uint8_t)(sizeof("AT+UBTCM=2\r\n") - 1U),
      (uint8_t)(sizeof("AT+UBTPM=2\r\n") - 1U),
  };
  static const uint8_t data_mode_cmd[] = "ATO1\r\n";
  static const uint8_t hello[] = "\r\npeepshow nina sps hello\r\ntype text and press send\r\n";
  static const uint8_t alive[] = "peepshow nina still listening\r\n";
  volatile PsNinaSpsProbe *probe = &g_ps_nina_sps_probe;
  uint16_t rx_len;

  probe->magic = PS_NINA_SPS_PROBE_MAGIC;
  probe->phase = PS_NINA_SPS_PROBE_PHASE;
  probe->tick_start = tx_time_get();
  probe->data_mode_tx_status = 0xFFFFFFFFUL;
  probe->hello_tx_status = 0xFFFFFFFFUL;
  probe->uart_flow_diag_mode = PS_NINA_SPS_FLOW_DIAG_MODE;
  probe->uart_reinit_status = 0xFFFFFFFFUL;
  (void)PS_CommsConfigureNinaSpsFlowDiag(probe);
  PS_CommsSampleNinaSpsFlow(probe, 0U);

  HAL_GPIO_WritePin(NINA_NRST_GPIO_Port, NINA_NRST_Pin, GPIO_PIN_RESET);
  tx_thread_sleep(PS_PHASE6_NINA_RESET_ASSERT_TICKS);
  HAL_GPIO_WritePin(NINA_NRST_GPIO_Port, NINA_NRST_Pin, GPIO_PIN_SET);
  tx_thread_sleep(PS_NINA_CAPABILITY_BOOT_WAIT_TICKS);

  probe->boot_rx_len = PS_Phase6_NinaReceiveUntilQuiet((uint8_t *)probe->boot_rx,
                                                        sizeof(probe->boot_rx),
                                                        PS_PHASE6_NINA_BOOT_DRAIN_TICKS,
                                                        PS_PHASE6_NINA_RX_QUIET_TICKS);

  probe->at_tx_status = (ULONG)HAL_UART_Transmit(&hlpuart1,
                                                 at_cmd,
                                                 (uint16_t)(sizeof(at_cmd) - 1U),
                                                 250U);
  probe->at_rx_len = PS_Phase6_NinaReceiveUntilQuiet((uint8_t *)probe->at_rx,
                                                     sizeof(probe->at_rx),
                                                     PS_PHASE6_NINA_AT_RX_TICKS,
                                                     PS_PHASE6_NINA_RX_QUIET_TICKS);
  probe->at_ok = PS_Phase6_BufferContainsToken((const uint8_t *)probe->at_rx,
                                               (uint16_t)probe->at_rx_len,
                                               "OK");

  for (uint32_t i = 0U; i < 9U; i++)
  {
    for (uint32_t j = 0U; j < sizeof(probe->setup_rx[i]); j++)
    {
      probe->setup_rx[i][j] = 0U;
    }

    probe->setup_tx_status[i] = (ULONG)HAL_UART_Transmit(&hlpuart1,
                                                         setup_cmds[i],
                                                         setup_lens[i],
                                                         250U);
    rx_len = PS_Phase6_NinaReceiveUntilQuiet((uint8_t *)probe->setup_rx[i],
                                             sizeof(probe->setup_rx[i]),
                                             PS_PHASE6_NINA_AT_RX_TICKS,
                                             PS_PHASE6_NINA_RX_QUIET_TICKS);
    probe->setup_rx_len[i] = rx_len;
    probe->setup_ok[i] = PS_Phase6_BufferContainsToken((const uint8_t *)probe->setup_rx[i],
                                                       rx_len,
                                                       "OK");
    if (PS_Phase6_BufferContainsToken((const uint8_t *)probe->setup_rx[i], rx_len, "+UUBTACLC:") != 0U)
    {
      probe->phone_detected = 1U;
    }
    if (PS_Phase6_BufferContainsToken((const uint8_t *)probe->setup_rx[i], rx_len, "+UUDPC:") != 0U)
    {
      probe->phone_sps_detected = 1U;
      probe->tick_peer = tx_time_get();
    }
  }
  PS_CommsSampleNinaSpsFlow(probe, 1U);

  if (probe->phone_sps_detected == 0U)
  {
    ULONG start = tx_time_get();
    uint16_t count = 0U;
    while (((tx_time_get() - start) < PS_NINA_SPS_CONNECT_WAIT_TICKS) &&
           (count < sizeof(probe->phone_rx)))
    {
      probe->peer_wait_loops++;
      uint8_t byte = 0U;
      if (HAL_UART_Receive(&hlpuart1, &byte, 1U, 10U) == HAL_OK)
      {
        probe->phone_rx[count++] = byte;
        probe->phone_rx_len = count;
        if (PS_Phase6_BufferContainsToken((const uint8_t *)probe->phone_rx, count, "+UUBTACLC:") != 0U)
        {
          probe->phone_detected = 1U;
        }
        if (PS_Phase6_BufferContainsToken((const uint8_t *)probe->phone_rx, count, "+UUDPC:") != 0U)
        {
          probe->phone_sps_detected = 1U;
          probe->tick_peer = tx_time_get();
          if (count < sizeof(probe->phone_rx))
          {
            rx_len = PS_Phase6_NinaReceiveUntilQuiet((uint8_t *)&probe->phone_rx[count],
                                                     (uint16_t)(sizeof(probe->phone_rx) - count),
                                                     PS_PHASE6_NINA_AT_RX_TICKS,
                                                     PS_PHASE6_NINA_RX_QUIET_TICKS);
            count = (uint16_t)(count + rx_len);
            probe->phone_rx_len = count;
          }
          break;
        }
      }
    }
  }
  PS_CommsSampleNinaSpsFlow(probe, 2U);

  if (probe->phone_sps_detected != 0U)
  {
    probe->data_mode_tx_status = (ULONG)HAL_UART_Transmit(&hlpuart1,
                                                          data_mode_cmd,
                                                          (uint16_t)(sizeof(data_mode_cmd) - 1U),
                                                          250U);
    probe->data_mode_rx_len = PS_Phase6_NinaReceiveUntilQuiet((uint8_t *)probe->data_mode_rx,
                                                              sizeof(probe->data_mode_rx),
                                                              PS_NINA_SPS_POST_ATO_DRAIN_TICKS,
                                                              PS_PHASE6_NINA_RX_QUIET_TICKS);
    probe->data_mode_ok = PS_Phase6_BufferContainsToken((const uint8_t *)probe->data_mode_rx,
                                                        (uint16_t)probe->data_mode_rx_len,
                                                        "OK");
    tx_thread_sleep(5U);
    probe->hello_tx_status = (ULONG)HAL_UART_Transmit(&hlpuart1,
                                                      hello,
                                                      (uint16_t)(sizeof(hello) - 1U),
                                                      250U);

    ULONG start = tx_time_get();
    ULONG last_alive = start;
    uint16_t count = 0U;
    while (((tx_time_get() - start) < PS_NINA_SPS_DATA_WAIT_TICKS) &&
           (count < sizeof(probe->phone_data_rx)))
    {
      probe->data_wait_loops++;
      uint8_t byte = 0U;
      if ((tx_time_get() - last_alive) >= 200U)
      {
        if (HAL_UART_Transmit(&hlpuart1, alive, (uint16_t)(sizeof(alive) - 1U), 250U) == HAL_OK)
        {
          probe->phone_alive_tx_count++;
        }
        last_alive = tx_time_get();
      }
      if (HAL_UART_Receive(&hlpuart1, &byte, 1U, 10U) == HAL_OK)
      {
        probe->phone_data_rx[count++] = byte;
        probe->phone_data_rx_len = count;
        if (HAL_UART_Transmit(&hlpuart1, &byte, 1U, 250U) == HAL_OK)
        {
          probe->phone_echo_tx_count++;
        }
      }
    }
  }

  PS_CommsSampleNinaSpsFlow(probe, 3U);
  probe->tick_data_done = tx_time_get();
  probe->uart_state_after = (ULONG)HAL_UART_GetState(&hlpuart1);
  probe->uart_error_after = (ULONG)HAL_UART_GetError(&hlpuart1);
  probe->nrst_state_after = (ULONG)HAL_GPIO_ReadPin(NINA_NRST_GPIO_Port, NINA_NRST_Pin);
  probe->dtr_state_after = (ULONG)HAL_GPIO_ReadPin(NINA_DTR_GPIO_Port, NINA_DTR_Pin);
  probe->dsr_state_after = (ULONG)HAL_GPIO_ReadPin(NINA_DSR_GPIO_Port, NINA_DSR_Pin);
  probe->complete = 1U;
}

static void __attribute__((unused)) PS_CommsRunNinaNfcProbe(void)
{
  static const uint8_t at_cmd[] = "AT\r\n";
  static const uint8_t enable_query_cmd[] = "AT+UNFCEN?\r\n";
  static const uint8_t uri_set_cmd[] = "AT+UNFCURI=1,\"https://example.com/peepshow-nfc\"\r\n";
  static const uint8_t uri_query_cmd[] = "AT+UNFCURI?\r\n";
  static const uint8_t enable_uri_cmd[] = "AT+UNFCEN=2\r\n";
  volatile PsNinaNfcProbe *probe = &g_ps_nina_nfc_probe;

  probe->magic = PS_NINA_NFC_PROBE_MAGIC;
  probe->phase = PS_NINA_NFC_PROBE_PHASE;
  probe->tick_start = tx_time_get();
  probe->at_tx_status = 0xFFFFFFFFUL;
  probe->enable_query_tx_status = 0xFFFFFFFFUL;
  probe->uri_set_tx_status = 0xFFFFFFFFUL;
  probe->uri_query_tx_status = 0xFFFFFFFFUL;
  probe->enable_uri_tx_status = 0xFFFFFFFFUL;
  probe->enable_verify_tx_status = 0xFFFFFFFFUL;

  HAL_GPIO_WritePin(NINA_NRST_GPIO_Port, NINA_NRST_Pin, GPIO_PIN_RESET);
  tx_thread_sleep(PS_PHASE6_NINA_RESET_ASSERT_TICKS);
  HAL_GPIO_WritePin(NINA_NRST_GPIO_Port, NINA_NRST_Pin, GPIO_PIN_SET);
  tx_thread_sleep(PS_NINA_CAPABILITY_BOOT_WAIT_TICKS);

  probe->boot_rx_len = PS_Phase6_NinaReceiveUntilQuiet((uint8_t *)probe->boot_rx,
                                                        sizeof(probe->boot_rx),
                                                        PS_PHASE6_NINA_BOOT_DRAIN_TICKS,
                                                        PS_PHASE6_NINA_RX_QUIET_TICKS);

  probe->at_tx_status = (ULONG)HAL_UART_Transmit(&hlpuart1,
                                                 at_cmd,
                                                 (uint16_t)(sizeof(at_cmd) - 1U),
                                                 250U);
  probe->at_rx_len = PS_Phase6_NinaReceiveUntilQuiet((uint8_t *)probe->at_rx,
                                                     sizeof(probe->at_rx),
                                                     PS_PHASE6_NINA_AT_RX_TICKS,
                                                     PS_PHASE6_NINA_RX_QUIET_TICKS);
  probe->at_ok = PS_Phase6_BufferContainsToken((const uint8_t *)probe->at_rx,
                                               (uint16_t)probe->at_rx_len,
                                               "OK");

  probe->enable_query_tx_status = (ULONG)HAL_UART_Transmit(&hlpuart1,
                                                           enable_query_cmd,
                                                           (uint16_t)(sizeof(enable_query_cmd) - 1U),
                                                           250U);
  probe->enable_query_rx_len = PS_Phase6_NinaReceiveUntilQuiet((uint8_t *)probe->enable_query_rx,
                                                               sizeof(probe->enable_query_rx),
                                                               PS_PHASE6_NINA_AT_RX_TICKS,
                                                               PS_PHASE6_NINA_RX_QUIET_TICKS);
  probe->enable_query_ok = PS_Phase6_BufferContainsToken((const uint8_t *)probe->enable_query_rx,
                                                         (uint16_t)probe->enable_query_rx_len,
                                                         "OK");

  probe->uri_set_tx_status = (ULONG)HAL_UART_Transmit(&hlpuart1,
                                                      uri_set_cmd,
                                                      (uint16_t)(sizeof(uri_set_cmd) - 1U),
                                                      250U);
  probe->uri_set_rx_len = PS_Phase6_NinaReceiveUntilQuiet((uint8_t *)probe->uri_set_rx,
                                                          sizeof(probe->uri_set_rx),
                                                          PS_PHASE6_NINA_AT_RX_TICKS,
                                                          PS_PHASE6_NINA_RX_QUIET_TICKS);
  probe->uri_set_ok = PS_Phase6_BufferContainsToken((const uint8_t *)probe->uri_set_rx,
                                                    (uint16_t)probe->uri_set_rx_len,
                                                    "OK");

  probe->uri_query_tx_status = (ULONG)HAL_UART_Transmit(&hlpuart1,
                                                        uri_query_cmd,
                                                        (uint16_t)(sizeof(uri_query_cmd) - 1U),
                                                        250U);
  probe->uri_query_rx_len = PS_Phase6_NinaReceiveUntilQuiet((uint8_t *)probe->uri_query_rx,
                                                            sizeof(probe->uri_query_rx),
                                                            PS_PHASE6_NINA_AT_RX_TICKS,
                                                            PS_PHASE6_NINA_RX_QUIET_TICKS);
  probe->uri_query_ok = PS_Phase6_BufferContainsToken((const uint8_t *)probe->uri_query_rx,
                                                      (uint16_t)probe->uri_query_rx_len,
                                                      "OK");

  probe->enable_uri_tx_status = (ULONG)HAL_UART_Transmit(&hlpuart1,
                                                         enable_uri_cmd,
                                                         (uint16_t)(sizeof(enable_uri_cmd) - 1U),
                                                         250U);
  probe->enable_uri_rx_len = PS_Phase6_NinaReceiveUntilQuiet((uint8_t *)probe->enable_uri_rx,
                                                             sizeof(probe->enable_uri_rx),
                                                             PS_PHASE6_NINA_AT_RX_TICKS,
                                                             PS_PHASE6_NINA_RX_QUIET_TICKS);
  probe->enable_uri_ok = PS_Phase6_BufferContainsToken((const uint8_t *)probe->enable_uri_rx,
                                                       (uint16_t)probe->enable_uri_rx_len,
                                                       "OK");

  probe->enable_verify_tx_status = (ULONG)HAL_UART_Transmit(&hlpuart1,
                                                            enable_query_cmd,
                                                            (uint16_t)(sizeof(enable_query_cmd) - 1U),
                                                            250U);
  probe->enable_verify_rx_len = PS_Phase6_NinaReceiveUntilQuiet((uint8_t *)probe->enable_verify_rx,
                                                                sizeof(probe->enable_verify_rx),
                                                                PS_PHASE6_NINA_AT_RX_TICKS,
                                                                PS_PHASE6_NINA_RX_QUIET_TICKS);
  probe->enable_verify_ok = PS_Phase6_BufferContainsToken((const uint8_t *)probe->enable_verify_rx,
                                                          (uint16_t)probe->enable_verify_rx_len,
                                                          "OK");

  ULONG start = tx_time_get();
  uint16_t count = 0U;
  while (((tx_time_get() - start) < PS_NINA_NFC_READ_WAIT_TICKS) &&
         (count < sizeof(probe->read_event_rx)))
  {
    probe->read_event_wait_loops++;
    uint8_t byte = 0U;
    if (HAL_UART_Receive(&hlpuart1, &byte, 1U, 10U) == HAL_OK)
    {
      probe->read_event_rx[count++] = byte;
      probe->read_event_rx_len = count;
      if (PS_Phase6_BufferContainsToken((const uint8_t *)probe->read_event_rx,
                                        count,
                                        "+UUNFCRD") != 0U)
      {
        probe->read_event_detected = 1U;
        probe->tick_read_event = tx_time_get();
        break;
      }
    }
  }

  probe->uart_state_after = (ULONG)HAL_UART_GetState(&hlpuart1);
  probe->uart_error_after = (ULONG)HAL_UART_GetError(&hlpuart1);
  probe->nrst_state_after = (ULONG)HAL_GPIO_ReadPin(NINA_NRST_GPIO_Port, NINA_NRST_Pin);
  probe->complete = 1U;
}

static void PS_CommsRunNinaSleepProbe(void)
{
  static const uint8_t at_cmd[] = "AT\r\n";
  static const uint8_t upwrreg_query_cmd[] = "AT+UPWRREG?\r\n";
  static const uint8_t pwrmng_min_cmd[] = "AT+UPWRMNG=1,10\r\n";
  static const uint8_t pwrmng_max_cmd[] = "AT+UPWRMNG=2,80\r\n";
  static const uint8_t bt_discoverable_off_cmd[] = "AT+UBTDM=1\r\n";
  static const uint8_t bt_connectable_off_cmd[] = "AT+UBTCM=1\r\n";
  static const uint8_t bt_pairing_off_cmd[] = "AT+UBTPM=1\r\n";
  static const uint8_t ustop_cmd[] = "AT+USTOP=1,5000\r\n";
  static const uint8_t dtr_uartoff_cmd[] = "AT&D3\r\n";
  static const uint8_t dtr_stop_cmd[] = "AT&D4\r\n";
  GPIO_InitTypeDef gpio = {0};
  volatile PsNinaSleepProbe *probe = &g_ps_nina_sleep_probe;

  probe->magic = PS_NINA_SLEEP_PROBE_MAGIC;
  probe->phase = PS_NINA_SLEEP_PROBE_PHASE;
  probe->tick_start = tx_time_get();
  probe->at_tx_status = 0xFFFFFFFFUL;
  probe->upwrreg_query_tx_status = 0xFFFFFFFFUL;
  probe->pwrmng_min_tx_status = 0xFFFFFFFFUL;
  probe->pwrmng_max_tx_status = 0xFFFFFFFFUL;
  probe->bt_discoverable_off_tx_status = 0xFFFFFFFFUL;
  probe->bt_connectable_off_tx_status = 0xFFFFFFFFUL;
  probe->bt_pairing_off_tx_status = 0xFFFFFFFFUL;
  probe->ustop_tx_status = 0xFFFFFFFFUL;
  probe->post_ustop_at_tx_status = 0xFFFFFFFFUL;
  probe->dtr_uartoff_set_tx_status = 0xFFFFFFFFUL;
  probe->dtr3_at_while_deasserted_tx_status = 0xFFFFFFFFUL;
  probe->dtr3_post_wake_at_tx_status = 0xFFFFFFFFUL;
  probe->dtr_set_tx_status = 0xFFFFFFFFUL;
  probe->post_dtr_at_tx_status = 0xFFFFFFFFUL;

  __HAL_RCC_GPIOC_CLK_ENABLE();
  HAL_GPIO_WritePin(NINA_NRST_GPIO_Port, NINA_NRST_Pin, GPIO_PIN_RESET);
  gpio.Pin = NINA_NRST_Pin;
  gpio.Mode = GPIO_MODE_OUTPUT_PP;
  gpio.Pull = GPIO_NOPULL;
  gpio.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(NINA_NRST_GPIO_Port, &gpio);
  tx_thread_sleep(PS_PHASE6_NINA_RESET_ASSERT_TICKS);
  HAL_GPIO_WritePin(NINA_NRST_GPIO_Port, NINA_NRST_Pin, GPIO_PIN_SET);
  tx_thread_sleep(PS_NINA_CAPABILITY_BOOT_WAIT_TICKS);

  probe->boot_rx_len = PS_Phase6_NinaReceiveUntilQuiet((uint8_t *)probe->boot_rx,
                                                        sizeof(probe->boot_rx),
                                                        PS_PHASE6_NINA_BOOT_DRAIN_TICKS,
                                                        PS_NINA_SLEEP_BOOT_QUIET_TICKS);

  probe->at_tx_status = (ULONG)HAL_UART_Transmit(&hlpuart1,
                                                 at_cmd,
                                                 (uint16_t)(sizeof(at_cmd) - 1U),
                                                 250U);
  probe->at_rx_len = PS_Phase6_NinaReceiveUntilQuiet((uint8_t *)probe->at_rx,
                                                     sizeof(probe->at_rx),
                                                     PS_PHASE6_NINA_AT_RX_TICKS,
                                                     PS_PHASE6_NINA_RX_QUIET_TICKS);
  probe->at_ok = PS_Phase6_BufferContainsToken((const uint8_t *)probe->at_rx,
                                               (uint16_t)probe->at_rx_len,
                                               "OK");

  probe->upwrreg_query_tx_status = (ULONG)HAL_UART_Transmit(&hlpuart1,
                                                            upwrreg_query_cmd,
                                                            (uint16_t)(sizeof(upwrreg_query_cmd) - 1U),
                                                            250U);
  probe->upwrreg_query_rx_len = PS_Phase6_NinaReceiveUntilQuiet((uint8_t *)probe->upwrreg_query_rx,
                                                                sizeof(probe->upwrreg_query_rx),
                                                                PS_PHASE6_NINA_AT_RX_TICKS,
                                                                PS_PHASE6_NINA_RX_QUIET_TICKS);
  probe->upwrreg_query_ok = PS_Phase6_BufferContainsToken((const uint8_t *)probe->upwrreg_query_rx,
                                                          (uint16_t)probe->upwrreg_query_rx_len,
                                                          "OK");

  probe->pwrmng_min_tx_status = (ULONG)HAL_UART_Transmit(&hlpuart1,
                                                         pwrmng_min_cmd,
                                                         (uint16_t)(sizeof(pwrmng_min_cmd) - 1U),
                                                         250U);
  probe->pwrmng_min_rx_len = PS_Phase6_NinaReceiveUntilQuiet((uint8_t *)probe->pwrmng_min_rx,
                                                             sizeof(probe->pwrmng_min_rx),
                                                             PS_PHASE6_NINA_AT_RX_TICKS,
                                                             PS_PHASE6_NINA_RX_QUIET_TICKS);
  probe->pwrmng_min_ok = PS_Phase6_BufferContainsToken((const uint8_t *)probe->pwrmng_min_rx,
                                                       (uint16_t)probe->pwrmng_min_rx_len,
                                                       "OK");

  probe->pwrmng_max_tx_status = (ULONG)HAL_UART_Transmit(&hlpuart1,
                                                         pwrmng_max_cmd,
                                                         (uint16_t)(sizeof(pwrmng_max_cmd) - 1U),
                                                         250U);
  probe->pwrmng_max_rx_len = PS_Phase6_NinaReceiveUntilQuiet((uint8_t *)probe->pwrmng_max_rx,
                                                             sizeof(probe->pwrmng_max_rx),
                                                             PS_PHASE6_NINA_AT_RX_TICKS,
                                                             PS_PHASE6_NINA_RX_QUIET_TICKS);
  probe->pwrmng_max_ok = PS_Phase6_BufferContainsToken((const uint8_t *)probe->pwrmng_max_rx,
                                                       (uint16_t)probe->pwrmng_max_rx_len,
                                                       "OK");

  probe->bt_discoverable_off_tx_status = (ULONG)HAL_UART_Transmit(&hlpuart1,
                                                                  bt_discoverable_off_cmd,
                                                                  (uint16_t)(sizeof(bt_discoverable_off_cmd) - 1U),
                                                                  250U);
  probe->bt_discoverable_off_rx_len = PS_Phase6_NinaReceiveUntilQuiet((uint8_t *)probe->bt_discoverable_off_rx,
                                                                      sizeof(probe->bt_discoverable_off_rx),
                                                                      PS_PHASE6_NINA_AT_RX_TICKS,
                                                                      PS_PHASE6_NINA_RX_QUIET_TICKS);
  probe->bt_discoverable_off_ok = PS_Phase6_BufferContainsToken((const uint8_t *)probe->bt_discoverable_off_rx,
                                                                (uint16_t)probe->bt_discoverable_off_rx_len,
                                                                "OK");

  probe->bt_connectable_off_tx_status = (ULONG)HAL_UART_Transmit(&hlpuart1,
                                                                 bt_connectable_off_cmd,
                                                                 (uint16_t)(sizeof(bt_connectable_off_cmd) - 1U),
                                                                 250U);
  probe->bt_connectable_off_rx_len = PS_Phase6_NinaReceiveUntilQuiet((uint8_t *)probe->bt_connectable_off_rx,
                                                                     sizeof(probe->bt_connectable_off_rx),
                                                                     PS_PHASE6_NINA_AT_RX_TICKS,
                                                                     PS_PHASE6_NINA_RX_QUIET_TICKS);
  probe->bt_connectable_off_ok = PS_Phase6_BufferContainsToken((const uint8_t *)probe->bt_connectable_off_rx,
                                                               (uint16_t)probe->bt_connectable_off_rx_len,
                                                               "OK");

  probe->bt_pairing_off_tx_status = (ULONG)HAL_UART_Transmit(&hlpuart1,
                                                             bt_pairing_off_cmd,
                                                             (uint16_t)(sizeof(bt_pairing_off_cmd) - 1U),
                                                             250U);
  probe->bt_pairing_off_rx_len = PS_Phase6_NinaReceiveUntilQuiet((uint8_t *)probe->bt_pairing_off_rx,
                                                                 sizeof(probe->bt_pairing_off_rx),
                                                                 PS_PHASE6_NINA_AT_RX_TICKS,
                                                                 PS_PHASE6_NINA_RX_QUIET_TICKS);
  probe->bt_pairing_off_ok = PS_Phase6_BufferContainsToken((const uint8_t *)probe->bt_pairing_off_rx,
                                                           (uint16_t)probe->bt_pairing_off_rx_len,
                                                           "OK");

  probe->tick_ustop_enter = tx_time_get();
  probe->ustop_tx_status = (ULONG)HAL_UART_Transmit(&hlpuart1,
                                                    ustop_cmd,
                                                    (uint16_t)(sizeof(ustop_cmd) - 1U),
                                                    250U);
  probe->ustop_rx_len = PS_Phase6_NinaReceiveUntilQuiet((uint8_t *)probe->ustop_rx,
                                                        sizeof(probe->ustop_rx),
                                                        (PS_NINA_SLEEP_USTOP_TIMEOUT_MS / 10U) + 200U,
                                                        PS_PHASE6_NINA_RX_QUIET_TICKS);
  probe->tick_ustop_done = tx_time_get();
  probe->ustop_ok = PS_Phase6_BufferContainsToken((const uint8_t *)probe->ustop_rx,
                                                  (uint16_t)probe->ustop_rx_len,
                                                  "OK");
  probe->ustop_startup_seen = PS_Phase6_BufferContainsToken((const uint8_t *)probe->ustop_rx,
                                                            (uint16_t)probe->ustop_rx_len,
                                                            "+STARTUP");

  tx_thread_sleep(50U);
  probe->post_ustop_at_tx_status = (ULONG)HAL_UART_Transmit(&hlpuart1,
                                                            at_cmd,
                                                            (uint16_t)(sizeof(at_cmd) - 1U),
                                                            250U);
  probe->post_ustop_at_rx_len = PS_Phase6_NinaReceiveUntilQuiet((uint8_t *)probe->post_ustop_at_rx,
                                                                sizeof(probe->post_ustop_at_rx),
                                                                PS_PHASE6_NINA_AT_RX_TICKS,
                                                                PS_PHASE6_NINA_RX_QUIET_TICKS);
  probe->post_ustop_at_ok = PS_Phase6_BufferContainsToken((const uint8_t *)probe->post_ustop_at_rx,
                                                          (uint16_t)probe->post_ustop_at_rx_len,
                                                          "OK");

  __HAL_RCC_GPIOC_CLK_ENABLE();
  gpio.Pin = NINA_DSR_Pin;
  gpio.Mode = GPIO_MODE_OUTPUT_PP;
  gpio.Pull = GPIO_NOPULL;
  gpio.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(NINA_DSR_GPIO_Port, &gpio);

  HAL_GPIO_WritePin(NINA_DSR_GPIO_Port, NINA_DSR_Pin, GPIO_PIN_RESET);
  tx_thread_sleep(2U);
  probe->dtr_asserted_before_set_state = (ULONG)HAL_GPIO_ReadPin(NINA_DSR_GPIO_Port, NINA_DSR_Pin);

  probe->dtr_uartoff_set_tx_status = (ULONG)HAL_UART_Transmit(&hlpuart1,
                                                              dtr_uartoff_cmd,
                                                              (uint16_t)(sizeof(dtr_uartoff_cmd) - 1U),
                                                              250U);
  probe->dtr_uartoff_set_rx_len = PS_Phase6_NinaReceiveUntilQuiet((uint8_t *)probe->dtr_uartoff_set_rx,
                                                                  sizeof(probe->dtr_uartoff_set_rx),
                                                                  PS_PHASE6_NINA_AT_RX_TICKS,
                                                                  PS_PHASE6_NINA_RX_QUIET_TICKS);
  probe->dtr_uartoff_set_ok = PS_Phase6_BufferContainsToken((const uint8_t *)probe->dtr_uartoff_set_rx,
                                                            (uint16_t)probe->dtr_uartoff_set_rx_len,
                                                            "OK");

  HAL_GPIO_WritePin(NINA_DSR_GPIO_Port, NINA_DSR_Pin, GPIO_PIN_SET);
  tx_thread_sleep(1200U);
  probe->dtr3_deasserted_state = (ULONG)HAL_GPIO_ReadPin(NINA_DSR_GPIO_Port, NINA_DSR_Pin);
  probe->dtr3_at_while_deasserted_tx_status = (ULONG)HAL_UART_Transmit(&hlpuart1,
                                                                       at_cmd,
                                                                       (uint16_t)(sizeof(at_cmd) - 1U),
                                                                       250U);
  probe->dtr3_at_while_deasserted_rx_len = PS_Phase6_NinaReceiveUntilQuiet((uint8_t *)probe->dtr3_at_while_deasserted_rx,
                                                                           sizeof(probe->dtr3_at_while_deasserted_rx),
                                                                           PS_PHASE6_NINA_AT_RX_TICKS,
                                                                           PS_PHASE6_NINA_RX_QUIET_TICKS);
  probe->dtr3_at_while_deasserted_ok = PS_Phase6_BufferContainsToken((const uint8_t *)probe->dtr3_at_while_deasserted_rx,
                                                                     (uint16_t)probe->dtr3_at_while_deasserted_rx_len,
                                                                     "OK");

  HAL_GPIO_WritePin(NINA_DSR_GPIO_Port, NINA_DSR_Pin, GPIO_PIN_RESET);
  tx_thread_sleep(1200U);
  probe->dtr3_wake_asserted_state = (ULONG)HAL_GPIO_ReadPin(NINA_DSR_GPIO_Port, NINA_DSR_Pin);
  probe->dtr3_post_wake_at_tx_status = (ULONG)HAL_UART_Transmit(&hlpuart1,
                                                                at_cmd,
                                                                (uint16_t)(sizeof(at_cmd) - 1U),
                                                                250U);
  probe->dtr3_post_wake_at_rx_len = PS_Phase6_NinaReceiveUntilQuiet((uint8_t *)probe->dtr3_post_wake_at_rx,
                                                                    sizeof(probe->dtr3_post_wake_at_rx),
                                                                    PS_PHASE6_NINA_AT_RX_TICKS,
                                                                    PS_PHASE6_NINA_RX_QUIET_TICKS);
  probe->dtr3_post_wake_at_ok = PS_Phase6_BufferContainsToken((const uint8_t *)probe->dtr3_post_wake_at_rx,
                                                              (uint16_t)probe->dtr3_post_wake_at_rx_len,
                                                              "OK");

  probe->dtr_set_tx_status = (ULONG)HAL_UART_Transmit(&hlpuart1,
                                                       dtr_stop_cmd,
                                                       (uint16_t)(sizeof(dtr_stop_cmd) - 1U),
                                                       250U);
  probe->dtr_set_rx_len = PS_Phase6_NinaReceiveUntilQuiet((uint8_t *)probe->dtr_set_rx,
                                                          sizeof(probe->dtr_set_rx),
                                                          PS_PHASE6_NINA_AT_RX_TICKS,
                                                          PS_PHASE6_NINA_RX_QUIET_TICKS);
  probe->dtr_set_ok = PS_Phase6_BufferContainsToken((const uint8_t *)probe->dtr_set_rx,
                                                    (uint16_t)probe->dtr_set_rx_len,
                                                    "OK");

  PS_PublishDisplayDiagFill(0x00U, PS_NINA_SLEEP_DISPLAY_MARK_DTR_STOP);
  tx_thread_sleep(20U);

  HAL_GPIO_WritePin(NINA_DSR_GPIO_Port, NINA_DSR_Pin, GPIO_PIN_SET);
  probe->tick_dtr_stop_enter = tx_time_get();
  probe->dtr_deasserted_state = (ULONG)HAL_GPIO_ReadPin(NINA_DSR_GPIO_Port, NINA_DSR_Pin);
  probe->dtr_stop_hold_active = 1U;
  while ((tx_time_get() - probe->tick_dtr_stop_enter) < PS_NINA_SLEEP_DTR_STOP_HOLD_TICKS)
  {
    probe->dtr_stop_hold_loops++;
    tx_thread_sleep(100U);
  }
  probe->dtr_stop_hold_active = 0U;

  PS_PublishDisplayDiagFill(0xFFU, PS_NINA_SLEEP_DISPLAY_MARK_RESET_HELD);
  tx_thread_sleep(20U);

  HAL_GPIO_WritePin(NINA_NRST_GPIO_Port, NINA_NRST_Pin, GPIO_PIN_RESET);
  gpio.Pin = NINA_NRST_Pin;
  gpio.Mode = GPIO_MODE_OUTPUT_PP;
  gpio.Pull = GPIO_NOPULL;
  gpio.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(NINA_NRST_GPIO_Port, &gpio);
  HAL_GPIO_WritePin(NINA_NRST_GPIO_Port, NINA_NRST_Pin, GPIO_PIN_RESET);
  probe->reset_attrib_tick_enter = tx_time_get();
  probe->reset_attrib_nrst_state = (ULONG)HAL_GPIO_ReadPin(NINA_NRST_GPIO_Port, NINA_NRST_Pin);
  probe->reset_attrib_hold_active = 1U;
  while ((tx_time_get() - probe->reset_attrib_tick_enter) < PS_NINA_SLEEP_RESET_ATTRIB_HOLD_TICKS)
  {
    probe->reset_attrib_hold_loops++;
    tx_thread_sleep(100U);
  }
  probe->reset_attrib_hold_active = 0U;
  probe->reset_attrib_tick_done = tx_time_get();

  HAL_GPIO_WritePin(NINA_NRST_GPIO_Port, NINA_NRST_Pin, GPIO_PIN_SET);
  tx_thread_sleep(PS_NINA_CAPABILITY_BOOT_WAIT_TICKS);

  HAL_GPIO_WritePin(NINA_DSR_GPIO_Port, NINA_DSR_Pin, GPIO_PIN_RESET);
  probe->dtr_wake_asserted_state = (ULONG)HAL_GPIO_ReadPin(NINA_DSR_GPIO_Port, NINA_DSR_Pin);
  tx_thread_sleep(100U);
  probe->dtr_wake_rx_len = PS_Phase6_NinaReceiveUntilQuiet((uint8_t *)probe->dtr_wake_rx,
                                                           sizeof(probe->dtr_wake_rx),
                                                           PS_PHASE6_NINA_BOOT_DRAIN_TICKS,
                                                           PS_PHASE6_NINA_RX_QUIET_TICKS);
  probe->tick_dtr_stop_done = tx_time_get();
  probe->dtr_wake_startup_seen = PS_Phase6_BufferContainsToken((const uint8_t *)probe->dtr_wake_rx,
                                                               (uint16_t)probe->dtr_wake_rx_len,
                                                               "+STARTUP");

  probe->post_dtr_at_tx_status = (ULONG)HAL_UART_Transmit(&hlpuart1,
                                                          at_cmd,
                                                          (uint16_t)(sizeof(at_cmd) - 1U),
                                                          250U);
  probe->post_dtr_at_rx_len = PS_Phase6_NinaReceiveUntilQuiet((uint8_t *)probe->post_dtr_at_rx,
                                                              sizeof(probe->post_dtr_at_rx),
                                                              PS_PHASE6_NINA_AT_RX_TICKS,
                                                              PS_PHASE6_NINA_RX_QUIET_TICKS);
  probe->post_dtr_at_ok = PS_Phase6_BufferContainsToken((const uint8_t *)probe->post_dtr_at_rx,
                                                        (uint16_t)probe->post_dtr_at_rx_len,
                                                        "OK");

  PS_PublishDisplayDiagFill(0xAAU, PS_NINA_SLEEP_DISPLAY_MARK_DONE);
  tx_thread_sleep(20U);

  gpio.Pin = NINA_DSR_Pin;
  gpio.Mode = GPIO_MODE_ANALOG;
  gpio.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(NINA_DSR_GPIO_Port, &gpio);

  probe->gpioa_moder_after = GPIOA->MODER;
  probe->gpiob_moder_after = GPIOB->MODER;
  probe->gpioc_moder_after = GPIOC->MODER;
  probe->gpioc_odr_after = GPIOC->ODR;
  probe->gpioc_idr_after = GPIOC->IDR;
  probe->uart_state_after = (ULONG)HAL_UART_GetState(&hlpuart1);
  probe->uart_error_after = (ULONG)HAL_UART_GetError(&hlpuart1);
  probe->nrst_state_after = (ULONG)HAL_GPIO_ReadPin(NINA_NRST_GPIO_Port, NINA_NRST_Pin);
  probe->dtr_state_after = (ULONG)HAL_GPIO_ReadPin(NINA_DTR_GPIO_Port, NINA_DTR_Pin);
  probe->dsr_state_after = (ULONG)HAL_GPIO_ReadPin(NINA_DSR_GPIO_Port, NINA_DSR_Pin);
  probe->complete = 1U;
}
static void PS_CommsOwnerEntry(void)
{
  ULONG cmd[PS_COMMS_CMD_WORDS];
#if !(PS_PHASE6_LPBAM_DISPLAY_EXPERIMENT && PS_PHASE6_LPBAM_START_AUTONOMOUS && PS_PHASE6_LPBAM_LATENCY_ONLY)
  static uint8_t nina_sleep_probe_ran;

  if (nina_sleep_probe_ran == 0U)
  {
    nina_sleep_probe_ran = 1U;
    PS_CommsRunNinaSleepProbe();
  }
#endif

  for (;;)
  {
    UINT status = tx_queue_receive(&ps_comms_cmd_queue, cmd, 100U);

    g_ps_phase5_threadx_probe.heartbeat[PS_THREADX_OWNER_COMMS]++;
    g_ps_phase5_threadx_probe.last_time[PS_THREADX_OWNER_COMMS] = tx_time_get();
    if (ps_phase6_quiesce_requested != 0U)
    {
      g_ps_phase5_threadx_probe.phase6_quiesce_ack_mask |=
          PS_PHASE6_OWNER_ACK_MASK(PS_THREADX_OWNER_COMMS);
    }

    if (status == TX_SUCCESS)
    {
      g_ps_phase5_threadx_probe.comms_queue_received++;
      g_ps_phase5_threadx_probe.comms_last_sequence = cmd[0];
      g_ps_phase5_threadx_probe.comms_last_cmd_type = cmd[1];
      g_ps_phase5_threadx_probe.comms_last_source_event = cmd[2];
      g_ps_phase5_threadx_probe.comms_last_button_id = cmd[3] & 0xFFU;
      g_ps_phase5_threadx_probe.comms_last_mask = cmd[3] >> 8;
      g_ps_phase5_threadx_probe.comms_last_event_tick = cmd[4];

      if (cmd[1] == PS_COMMS_CMD_ACTIVITY_HINT)
      {
        g_ps_phase5_threadx_probe.comms_activity_hints++;
      }
    }
    else if (status == TX_QUEUE_EMPTY)
    {
      g_ps_phase5_threadx_probe.comms_queue_receive_timeouts++;
    }
  }
}

static void PS_AudioOwnerEntry(void)
{
  ULONG cmd[PS_AUDIO_CMD_WORDS];
  uint8_t quiesced = 0U;

  for (;;)
  {
    UINT status = tx_queue_receive(&ps_audio_cmd_queue, cmd, 100U);

    g_ps_phase5_threadx_probe.heartbeat[PS_THREADX_OWNER_AUDIO]++;
    g_ps_phase5_threadx_probe.last_time[PS_THREADX_OWNER_AUDIO] = tx_time_get();
    if (ps_phase6_quiesce_requested != 0U)
    {
      if (quiesced == 0U)
      {
        g_ps_phase5_threadx_probe.phase6_audio_lptim_stop_status =
            (ULONG)HAL_LPTIM_PWM_Stop(&hlptim1, LPTIM_CHANNEL_1);
        g_ps_phase5_threadx_probe.phase6_audio_sai_stop_status =
            (ULONG)HAL_SAI_DMAStop(&hsai_BlockA1);
        HAL_GPIO_WritePin(SD_MODE_GPIO_Port, SD_MODE_Pin, GPIO_PIN_RESET);
        g_ps_phase5_threadx_probe.phase6_audio_sd_mode_state =
            (ULONG)HAL_GPIO_ReadPin(SD_MODE_GPIO_Port, SD_MODE_Pin);
        quiesced = 1U;
      }
      g_ps_phase5_threadx_probe.phase6_quiesce_ack_mask |=
          PS_PHASE6_OWNER_ACK_MASK(PS_THREADX_OWNER_AUDIO);
    }

    if (status == TX_SUCCESS)
    {
      g_ps_phase5_threadx_probe.audio_queue_received++;
      g_ps_phase5_threadx_probe.audio_last_sequence = cmd[0];
      g_ps_phase5_threadx_probe.audio_last_cmd_type = cmd[1];
      g_ps_phase5_threadx_probe.audio_last_source_event = cmd[2];
      g_ps_phase5_threadx_probe.audio_last_button_id = cmd[3] & 0xFFU;
      g_ps_phase5_threadx_probe.audio_last_mask = cmd[3] >> 8;
      g_ps_phase5_threadx_probe.audio_last_event_tick = cmd[4];

      if (ps_phase6_quiesce_requested != 0U)
      {
        continue;
      }

      if (cmd[1] == PS_AUDIO_CMD_ACTIVITY_HINT)
      {
        g_ps_phase5_threadx_probe.audio_activity_hints++;
        if (g_ps_phase5_threadx_probe.power_rails_ready != 0U)
        {
          (void)PS_AudioOwnerRunActivityHint();
        }
        else
        {
          g_ps_phase5_threadx_probe.audio_power_not_ready_skips++;
        }
      }
    }
    else if (status == TX_QUEUE_EMPTY)
    {
      g_ps_phase5_threadx_probe.audio_queue_receive_timeouts++;
    }
  }
}

static HAL_StatusTypeDef PS_AudioOwnerRunActivityHint(void)
{
  HAL_StatusTypeDef status;

  PS_AudioOwnerPrepareSpeakerBuffer(PS_AUDIO_SPEAKER_CUE_AMPLITUDE,
                                    PS_AUDIO_SPEAKER_CUE_HZ);

  g_ps_phase5_threadx_probe.audio_speaker_kernel_hz =
      HAL_RCCEx_GetPeriphCLKFreq(RCC_PERIPHCLK_SAI1);
  g_ps_phase5_threadx_probe.audio_speaker_sample_rate_hz =
      PS_AUDIO_SPEAKER_SAMPLE_RATE_HZ;
  g_ps_phase5_threadx_probe.audio_speaker_tone_hz =
      PS_AUDIO_SPEAKER_CUE_HZ;
  g_ps_phase5_threadx_probe.audio_speaker_amplitude =
      (ULONG)(uint16_t)PS_AUDIO_SPEAKER_CUE_AMPLITUDE;
  g_ps_phase5_threadx_probe.audio_speaker_buffer_halfwords =
      PS_AUDIO_SPEAKER_BUFFER_HALFWORDS;
  g_ps_phase5_threadx_probe.audio_speaker_cue_ticks =
      PS_AUDIO_SPEAKER_CUE_TICKS;

  (void)HAL_LPTIM_PWM_Stop(&hlptim1, LPTIM_CHANNEL_1);
  (void)HAL_SAI_DMAStop(&hsai_BlockA1);
  HAL_GPIO_WritePin(SD_MODE_GPIO_Port, SD_MODE_Pin, GPIO_PIN_SET);
  tx_thread_sleep(PS_AUDIO_SPEAKER_AMP_SETTLE_TICKS);

  status = HAL_SAI_Transmit_DMA(&hsai_BlockA1,
                                (uint8_t *)ps_audio_speaker_dma_buffer,
                                PS_AUDIO_SPEAKER_BUFFER_HALFWORDS);
  g_ps_phase5_threadx_probe.audio_speaker_start_status = (ULONG)status;
  if (status != HAL_OK)
  {
    g_ps_phase5_threadx_probe.audio_action_error_count++;
    g_ps_phase5_threadx_probe.audio_speaker_action_error_count++;
    HAL_GPIO_WritePin(SD_MODE_GPIO_Port, SD_MODE_Pin, GPIO_PIN_RESET);
    g_ps_phase5_threadx_probe.audio_speaker_sai_state_after =
        (ULONG)HAL_SAI_GetState(&hsai_BlockA1);
    g_ps_phase5_threadx_probe.audio_speaker_sai_error_after =
        HAL_SAI_GetError(&hsai_BlockA1);
    g_ps_phase5_threadx_probe.audio_speaker_sai_sr_after = SAI1_Block_A->SR;
    g_ps_phase5_threadx_probe.audio_speaker_sd_mode_state_after =
        (ULONG)HAL_GPIO_ReadPin(SD_MODE_GPIO_Port, SD_MODE_Pin);
    return status;
  }

  tx_thread_sleep(PS_AUDIO_SPEAKER_CUE_TICKS);

  status = HAL_SAI_DMAStop(&hsai_BlockA1);
  g_ps_phase5_threadx_probe.audio_speaker_stop_status = (ULONG)status;
  HAL_GPIO_WritePin(SD_MODE_GPIO_Port, SD_MODE_Pin, GPIO_PIN_RESET);
  g_ps_phase5_threadx_probe.audio_speaker_sai_state_after =
      (ULONG)HAL_SAI_GetState(&hsai_BlockA1);
  g_ps_phase5_threadx_probe.audio_speaker_sai_error_after =
      HAL_SAI_GetError(&hsai_BlockA1);
  g_ps_phase5_threadx_probe.audio_speaker_sai_sr_after = SAI1_Block_A->SR;
  g_ps_phase5_threadx_probe.audio_speaker_sd_mode_state_after =
      (ULONG)HAL_GPIO_ReadPin(SD_MODE_GPIO_Port, SD_MODE_Pin);
  if (status != HAL_OK)
  {
    g_ps_phase5_threadx_probe.audio_action_error_count++;
    g_ps_phase5_threadx_probe.audio_speaker_action_error_count++;
    return status;
  }

  g_ps_phase5_threadx_probe.audio_action_count++;
  g_ps_phase5_threadx_probe.audio_speaker_action_count++;
  return HAL_OK;
}

static void PS_AudioOwnerPrepareSpeakerBuffer(int16_t amplitude, uint32_t frequency_hz)
{
  uint32_t half_period_frames;

  if (frequency_hz == 0U)
  {
    frequency_hz = 1U;
  }

  half_period_frames = PS_AUDIO_SPEAKER_SAMPLE_RATE_HZ / (frequency_hz * 2U);
  if (half_period_frames == 0U)
  {
    half_period_frames = 1U;
  }

  for (uint32_t frame = 0U; frame < PS_AUDIO_SPEAKER_BUFFER_FRAMES; frame++)
  {
    int16_t sample = (((frame / half_period_frames) & 1U) == 0U) ?
        amplitude : (int16_t)(-amplitude);

    ps_audio_speaker_dma_buffer[(frame * 2U) + 0U] = (uint16_t)sample;
    ps_audio_speaker_dma_buffer[(frame * 2U) + 1U] = (uint16_t)sample;
  }
}

static void PS_InputPublishUIEvent(ULONG event_type,
                                   ULONG button_id,
                                   ULONG active_level,
                                   ULONG mask,
                                   ULONG event_tick)
{
  ULONG event[PS_UI_EVENT_WORDS];
  UINT status;

  if (g_ps_phase5_threadx_probe.ui_queue_ready == 0U)
  {
    g_ps_phase5_threadx_probe.ui_queue_send_fail++;
    g_ps_phase5_threadx_probe.ui_last_send_status = TX_QUEUE_ERROR;
    return;
  }

  event[0] = g_ps_phase5_threadx_probe.ui_queue_sent + 1U;
  event[1] = event_type;
  event[2] = button_id;
  event[3] = ((mask & 0xFFFFFFUL) << 8) | ((active_level != 0U) ? 1UL : 0UL);
  event[4] = event_tick;

  status = tx_queue_send(&ps_ui_event_queue, event, TX_NO_WAIT);
  g_ps_phase5_threadx_probe.ui_last_send_status = status;

  if (status == TX_SUCCESS)
  {
    g_ps_phase5_threadx_probe.ui_queue_sent++;
  }
  else
  {
    g_ps_phase5_threadx_probe.ui_queue_send_fail++;
  }
}

static void PS_UIPublishDisplayCmd(ULONG source_event,
                                   ULONG button_id,
                                   ULONG mask,
                                   ULONG event_tick)
{
  ULONG cmd[PS_DISPLAY_CMD_WORDS];
  UINT status;

#if PS_PHASE6_LPBAM_DISPLAY_EXPERIMENT && PS_PHASE6_LPBAM_START_AUTONOMOUS
  (void)source_event;
  (void)button_id;
  (void)mask;
  (void)event_tick;
  return;
#endif

  if (g_ps_phase5_threadx_probe.display_queue_ready == 0U)
  {
    g_ps_phase5_threadx_probe.display_queue_send_fail++;
    g_ps_phase5_threadx_probe.display_last_send_status = TX_QUEUE_ERROR;
    return;
  }

  cmd[0] = g_ps_phase5_threadx_probe.display_queue_sent + 1U;
  cmd[1] = PS_DISPLAY_CMD_ACTIVITY_HINT;
  cmd[2] = source_event;
  cmd[3] = ((mask & 0xFFFFFFUL) << 8) | (button_id & 0xFFUL);
  cmd[4] = event_tick;

  status = tx_queue_send(&ps_display_cmd_queue, cmd, TX_NO_WAIT);
  g_ps_phase5_threadx_probe.display_last_send_status = status;

  if (status == TX_SUCCESS)
  {
    g_ps_phase5_threadx_probe.display_queue_sent++;
  }
  else
  {
    g_ps_phase5_threadx_probe.display_queue_send_fail++;
  }
}

static void PS_PublishDisplayDiagFill(uint8_t fill_value, ULONG marker)
{
  ULONG cmd[PS_DISPLAY_CMD_WORDS];
  UINT status;

  if (g_ps_phase5_threadx_probe.display_queue_ready == 0U)
  {
    g_ps_phase5_threadx_probe.display_queue_send_fail++;
    g_ps_phase5_threadx_probe.display_last_send_status = TX_QUEUE_ERROR;
    return;
  }

  cmd[0] = g_ps_phase5_threadx_probe.display_queue_sent + 1U;
  cmd[1] = PS_DISPLAY_CMD_DIAG_FILL;
  cmd[2] = marker;
  cmd[3] = (ULONG)fill_value;
  cmd[4] = tx_time_get();

  status = tx_queue_send(&ps_display_cmd_queue, cmd, TX_NO_WAIT);
  g_ps_phase5_threadx_probe.display_last_send_status = status;

  if (status == TX_SUCCESS)
  {
    g_ps_phase5_threadx_probe.display_queue_sent++;
  }
  else
  {
    g_ps_phase5_threadx_probe.display_queue_send_fail++;
  }
}
static void PS_UIPublishStorageCmd(ULONG source_event,
                                   ULONG button_id,
                                   ULONG mask,
                                   ULONG event_tick)
{
  ULONG cmd[PS_STORAGE_CMD_WORDS];
  UINT status;

  if (g_ps_phase5_threadx_probe.storage_queue_ready == 0U)
  {
    g_ps_phase5_threadx_probe.storage_queue_send_fail++;
    g_ps_phase5_threadx_probe.storage_last_send_status = TX_QUEUE_ERROR;
    return;
  }

  cmd[0] = g_ps_phase5_threadx_probe.storage_queue_sent + 1U;
  cmd[1] = PS_STORAGE_CMD_ACTIVITY_HINT;
  cmd[2] = source_event;
  cmd[3] = ((mask & 0xFFFFFFUL) << 8) | (button_id & 0xFFUL);
  cmd[4] = event_tick;

  status = tx_queue_send(&ps_storage_cmd_queue, cmd, TX_NO_WAIT);
  g_ps_phase5_threadx_probe.storage_last_send_status = status;

  if (status == TX_SUCCESS)
  {
    g_ps_phase5_threadx_probe.storage_queue_sent++;
  }
  else
  {
    g_ps_phase5_threadx_probe.storage_queue_send_fail++;
  }
}

static void PS_UIPublishCommsCmd(ULONG source_event,
                                 ULONG button_id,
                                 ULONG mask,
                                 ULONG event_tick)
{
  ULONG cmd[PS_COMMS_CMD_WORDS];
  UINT status;

  if (g_ps_phase5_threadx_probe.comms_queue_ready == 0U)
  {
    g_ps_phase5_threadx_probe.comms_queue_send_fail++;
    g_ps_phase5_threadx_probe.comms_last_send_status = TX_QUEUE_ERROR;
    return;
  }

  cmd[0] = g_ps_phase5_threadx_probe.comms_queue_sent + 1U;
  cmd[1] = PS_COMMS_CMD_ACTIVITY_HINT;
  cmd[2] = source_event;
  cmd[3] = ((mask & 0xFFFFFFUL) << 8) | (button_id & 0xFFUL);
  cmd[4] = event_tick;

  status = tx_queue_send(&ps_comms_cmd_queue, cmd, TX_NO_WAIT);
  g_ps_phase5_threadx_probe.comms_last_send_status = status;

  if (status == TX_SUCCESS)
  {
    g_ps_phase5_threadx_probe.comms_queue_sent++;
  }
  else
  {
    g_ps_phase5_threadx_probe.comms_queue_send_fail++;
  }
}

static void PS_UIPublishAudioCmd(ULONG source_event,
                                 ULONG button_id,
                                 ULONG mask,
                                 ULONG event_tick)
{
  ULONG cmd[PS_AUDIO_CMD_WORDS];
  UINT status;

  if (g_ps_phase5_threadx_probe.audio_queue_ready == 0U)
  {
    g_ps_phase5_threadx_probe.audio_queue_send_fail++;
    g_ps_phase5_threadx_probe.audio_last_send_status = TX_QUEUE_ERROR;
    return;
  }

  cmd[0] = g_ps_phase5_threadx_probe.audio_queue_sent + 1U;
  cmd[1] = PS_AUDIO_CMD_ACTIVITY_HINT;
  cmd[2] = source_event;
  cmd[3] = ((mask & 0xFFFFFFUL) << 8) | (button_id & 0xFFUL);
  cmd[4] = event_tick;

  status = tx_queue_send(&ps_audio_cmd_queue, cmd, TX_NO_WAIT);
  g_ps_phase5_threadx_probe.audio_last_send_status = status;

  if (status == TX_SUCCESS)
  {
    g_ps_phase5_threadx_probe.audio_queue_sent++;
  }
  else
  {
    g_ps_phase5_threadx_probe.audio_queue_send_fail++;
  }
}

static void PS_ThreadXRecordPerformance(void)
{
  (void)tx_thread_performance_system_info_get(
      (ULONG *)&g_ps_phase5_threadx_probe.thread_resumptions,
      (ULONG *)&g_ps_phase5_threadx_probe.thread_suspensions,
      (ULONG *)&g_ps_phase5_threadx_probe.thread_solicited_preemptions,
      (ULONG *)&g_ps_phase5_threadx_probe.thread_interrupt_preemptions,
      (ULONG *)&g_ps_phase5_threadx_probe.thread_priority_inversions,
      (ULONG *)&g_ps_phase5_threadx_probe.thread_time_slices,
      (ULONG *)&g_ps_phase5_threadx_probe.thread_relinquishes,
      (ULONG *)&g_ps_phase5_threadx_probe.thread_timeouts,
      (ULONG *)&g_ps_phase5_threadx_probe.thread_wait_aborts,
      (ULONG *)&g_ps_phase5_threadx_probe.thread_non_idle_returns,
      (ULONG *)&g_ps_phase5_threadx_probe.thread_idle_returns);
}

static ULONG PS_InputButtonIdFromPin(uint16_t gpio_pin)
{
  if (gpio_pin == BTN_START_Pin)
  {
    return PS_INPUT_BUTTON_START;
  }
  if (gpio_pin == BTN_A_Pin)
  {
    return PS_INPUT_BUTTON_A;
  }
  if (gpio_pin == BTN_B_Pin)
  {
    return PS_INPUT_BUTTON_B;
  }
  if (gpio_pin == BTN_L_Pin)
  {
    return PS_INPUT_BUTTON_L;
  }
  if (gpio_pin == BTN_R_Pin)
  {
    return PS_INPUT_BUTTON_R;
  }

  return 0U;
}

/* USER CODE END 1 */


















