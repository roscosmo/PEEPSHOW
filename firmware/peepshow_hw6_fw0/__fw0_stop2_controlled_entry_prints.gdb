set pagination off
set $rt = &g_ps_hw6_rtos_probe
set $sm = &g_ps_hw6_owner_sm_probe
set $ow = &g_ps_hw6_owner_probe
printf "--- HW6 controlled STOP2 entry scaffold ---\n"
printf "rtos api/control count/status/tick = %u / %u / 0x%x / %u\n", $rt->version, $rt->stop2_control_request_count, $rt->stop2_control_last_status, $rt->stop2_control_last_tick
printf "elig status/block/pending = 0x%x / 0x%x / 0x%x\n", $rt->stop2_control_eligibility_status, $rt->stop2_control_eligibility_blocker_mask, $rt->stop2_control_eligibility_pending_mask
printf "entry attempts/status     = %u / 0x%x\n", $rt->stop2_control_entry_attempt_count, $rt->stop2_control_entry_status
printf "stop2 count before/after  = %u / %u\n", $rt->stop2_control_stop2_count_before, $rt->stop2_control_stop2_count_after
printf "elig ready/block/pending  = %u / 0x%x / 0x%x\n", $rt->stop2_eligibility_ready, $rt->stop2_eligibility_blocker_mask, $rt->stop2_eligibility_pending_mask
printf "display backend req/selected/status/held = %u / %u / 0x%x / %u\n", $rt->stop2_display_wait_backend_requested, $rt->stop2_display_wait_backend_selected, $rt->stop2_display_wait_backend_status, $rt->stop2_display_wait_backend_held_ready
printf "display lpbam ready/page/render/status = %u / %u / %u / 0x%x\n", $ow->display_lpbam_ready, $ow->display_lpbam_ready_page, $ow->display_lpbam_ready_render_count, $ow->display_lpbam_status
printf "display lpbam prep/clear/reason = %u / %u / %u\n", $ow->display_lpbam_prepare_count, $ow->display_lpbam_clear_count, $ow->display_lpbam_clear_reason
printf "display lpbam prep status/abort/status = 0x%x / %u / 0x%x\n", $ow->display_lpbam_prepare_status, $ow->display_lpbam_abort_count, $ow->display_lpbam_abort_status
printf "rtos lpbam prep count/send/wait/ack/owner/ready = %u / 0x%x / 0x%x / 0x%x / 0x%x / %u\n", $rt->stop2_lpbam_prepare_request_count, $rt->stop2_lpbam_prepare_send_status, $rt->stop2_lpbam_prepare_wait_status, $rt->stop2_lpbam_prepare_ack_flags, $rt->stop2_lpbam_prepare_owner_status, $rt->stop2_lpbam_prepare_ready_after
printf "rtos lpbam abort count/send/wait/ack/owner = %u / 0x%x / 0x%x / 0x%x / 0x%x\n", $rt->stop2_lpbam_abort_request_count, $rt->stop2_lpbam_abort_send_status, $rt->stop2_lpbam_abort_wait_status, $rt->stop2_lpbam_abort_ack_flags, $rt->stop2_lpbam_abort_owner_status
printf "owner stop2 count/start/wake/end = %u / %u / %u / %u\n", $sm->stop2_request_count, $sm->stop2_start_tick, $sm->stop2_wake_tick, $sm->stop2_end_tick
printf "owner stop2 status q/enter/clk/recover/last = 0x%x / 0x%x / 0x%x / 0x%x / 0x%x\n", $sm->stop2_quiesce_status, $sm->stop2_enter_status, $sm->stop2_clock_restore_status, $sm->stop2_recover_status, $sm->stop2_last_status
printf "systick ctrl before/sleep/after = 0x%x / 0x%x / 0x%x\n", $sm->stop2_systick_ctrl_before, $sm->stop2_systick_ctrl_sleep, $sm->stop2_systick_ctrl_after
printf "systick icsr before/sleep/after = 0x%x / 0x%x / 0x%x\n", $sm->stop2_systick_icsr_before, $sm->stop2_systick_icsr_sleep, $sm->stop2_systick_icsr_after
printf "power quiesce count/reason/start/end/last = %u / %u / %u / %u / 0x%x\n", $sm->power_quiesce_request_count, $sm->power_quiesce_reason, $sm->power_quiesce_start_tick, $sm->power_quiesce_end_tick, $sm->power_quiesce_last_status
printf "power quiesce masks req/send/ack/ok/fail = 0x%x / 0x%x / 0x%x / 0x%x / 0x%x\n", $sm->power_quiesce_required_mask, $sm->power_quiesce_send_ok_mask, $sm->power_quiesce_ack_ok_mask, $sm->power_quiesce_success_mask, $sm->power_quiesce_failure_mask
printf "power quiesce owner stat A/I/D/S/ST/C = 0x%x / 0x%x / 0x%x / 0x%x / 0x%x / 0x%x\n", $sm->power_quiesce_owner_status[1], $sm->power_quiesce_owner_status[2], $sm->power_quiesce_owner_status[3], $sm->power_quiesce_owner_status[4], $sm->power_quiesce_owner_status[5], $sm->power_quiesce_owner_status[6]
printf "power quiesce send stat A/I/D/S/ST/C = 0x%x / 0x%x / 0x%x / 0x%x / 0x%x / 0x%x\n", $sm->power_quiesce_send_status[1], $sm->power_quiesce_send_status[2], $sm->power_quiesce_send_status[3], $sm->power_quiesce_send_status[4], $sm->power_quiesce_send_status[5], $sm->power_quiesce_send_status[6]
printf "power quiesce ack stat A/I/D/S/ST/C = 0x%x / 0x%x / 0x%x / 0x%x / 0x%x / 0x%x\n", $sm->power_quiesce_ack_status[1], $sm->power_quiesce_ack_status[2], $sm->power_quiesce_ack_status[3], $sm->power_quiesce_ack_status[4], $sm->power_quiesce_ack_status[5], $sm->power_quiesce_ack_status[6]
printf "power quiesce ack flags A/I/D/S/ST/C = 0x%x / 0x%x / 0x%x / 0x%x / 0x%x / 0x%x\n", $sm->power_quiesce_ack_flags[1], $sm->power_quiesce_ack_flags[2], $sm->power_quiesce_ack_flags[3], $sm->power_quiesce_ack_flags[4], $sm->power_quiesce_ack_flags[5], $sm->power_quiesce_ack_flags[6]
printf "audio sleep proof SD/sai/saierr/dma/dmaerr = %u / 0x%x / 0x%x / 0x%x / 0x%x\n", $ow->audio_sd_state_after, $ow->audio_sai_state_after, $ow->audio_sai_error_after, $ow->audio_dma_state_after, $ow->audio_dma_error_after
printf "joystick sleep proof status/committed/omit/i2c = 0x%x / %u / %u / 0x%x / 0x%x\n", $sm->joystick_sleep_write_status, $sm->joystick_terminal_sleep_committed, $sm->joystick_post_sleep_read_omitted, $sm->joystick_i2c_state_after, $sm->joystick_i2c_error_after
printf "joystick int cfg1 before/target/after = 0x%x / 0x%x / 0x%x\n", $sm->joystick_sleep_audit_int_config1_before, $sm->joystick_sleep_audit_int_config1_target, $sm->joystick_sleep_audit_int_config1_after
printf "joystick sleep write/verify masks = 0x%x / 0x%x\n", $sm->joystick_sleep_audit_write_ok_mask, $sm->joystick_sleep_audit_verify_ok_mask
printf "joystick cycle0 sleep/status/device2 = 0x%x / 0x%x / 0x%x\n", $sm->joystick_cycle_sleep_status[0], $sm->joystick_driver_last_status, $sm->joystick_device_config2_sleep
printf "imu sleep proof value/write/committed/omit/i2c = 0x%x / 0x%x / %u / %u / 0x%x / 0x%x\n", $sm->imu_deep_power_down_value, $sm->imu_deep_power_down_write_status, $sm->imu_terminal_deep_power_down_committed, $sm->imu_post_deep_power_down_read_omitted, $sm->imu_i2c_state_after, $sm->imu_i2c_error_after
printf "imu snapshot/write/verify masks = 0x%x / 0x%x / 0x%x\n", $sm->imu_snapshot_ok_mask, $sm->imu_write_ok_mask, $sm->imu_verify_ok_mask
printf "imu cycle0 sleep/status/whoami = 0x%x / 0x%x / 0x%x\n", $sm->imu_cycle_sleep_status[0], $sm->imu_driver_last_status, $sm->imu_whoami
printf "stop2 policy count/reason/tick = %u / %u / %u\n", $sm->stop2_policy_request_count, $sm->stop2_policy_reason, $sm->stop2_policy_last_tick
printf "stop2 policy BLE target/active/status = %u / %u / 0x%x\n", $sm->stop2_policy_ble_target_mode, $sm->stop2_policy_ble_active_mode, $sm->stop2_policy_ble_status
printf "stop2 policy IMU target/active/status = %u / %u / 0x%x\n", $sm->stop2_policy_imu_target_mode, $sm->stop2_policy_imu_active_mode, $sm->stop2_policy_imu_status
printf "gpio policy version/snapshots = %u / %u\n", $sm->stop2_gpio_policy_version, $sm->stop2_gpio_snapshot_count
printf "gpio used A/B/C/H = 0x%x / 0x%x / 0x%x / 0x%x\n", $sm->stop2_gpio_used_mask[0], $sm->stop2_gpio_used_mask[1], $sm->stop2_gpio_used_mask[2], $sm->stop2_gpio_used_mask[3]
printf "gpio wake A/B/C/H = 0x%x / 0x%x / 0x%x / 0x%x\n", $sm->stop2_gpio_wake_mask[0], $sm->stop2_gpio_wake_mask[1], $sm->stop2_gpio_wake_mask[2], $sm->stop2_gpio_wake_mask[3]
printf "gpio retain A/B/C/H = 0x%x / 0x%x / 0x%x / 0x%x\n", $sm->stop2_gpio_retain_mask[0], $sm->stop2_gpio_retain_mask[1], $sm->stop2_gpio_retain_mask[2], $sm->stop2_gpio_retain_mask[3]
printf "gpio park cand A/B/C/H = 0x%x / 0x%x / 0x%x / 0x%x\n", $sm->stop2_gpio_park_candidate_mask[0], $sm->stop2_gpio_park_candidate_mask[1], $sm->stop2_gpio_park_candidate_mask[2], $sm->stop2_gpio_park_candidate_mask[3]
printf "gpio park groups default/override/active = 0x%x / 0x%x / 0x%x\n", $sm->stop2_gpio_park_group_default_mask, $sm->stop2_gpio_park_group_override_mask, $sm->stop2_gpio_park_group_active_mask
printf "gpio park count/status restore count/status = %u / 0x%x / %u / 0x%x\n", $sm->stop2_gpio_park_count, $sm->stop2_gpio_park_status, $sm->stop2_gpio_restore_count, $sm->stop2_gpio_restore_status
printf "gpio park mask A/B/C/H = 0x%x / 0x%x / 0x%x / 0x%x\n", $sm->stop2_gpio_park_mask[0], $sm->stop2_gpio_park_mask[1], $sm->stop2_gpio_park_mask[2], $sm->stop2_gpio_park_mask[3]
printf "gpio MODER before A/B/C/H = 0x%x / 0x%x / 0x%x / 0x%x\n", $sm->stop2_gpio_moder_before[0], $sm->stop2_gpio_moder_before[1], $sm->stop2_gpio_moder_before[2], $sm->stop2_gpio_moder_before[3]
printf "gpio MODER sleep  A/B/C/H = 0x%x / 0x%x / 0x%x / 0x%x\n", $sm->stop2_gpio_moder_sleep[0], $sm->stop2_gpio_moder_sleep[1], $sm->stop2_gpio_moder_sleep[2], $sm->stop2_gpio_moder_sleep[3]
printf "gpio MODER after  A/B/C/H = 0x%x / 0x%x / 0x%x / 0x%x\n", $sm->stop2_gpio_moder_after[0], $sm->stop2_gpio_moder_after[1], $sm->stop2_gpio_moder_after[2], $sm->stop2_gpio_moder_after[3]
printf "gpio PUPDR before A/B/C/H = 0x%x / 0x%x / 0x%x / 0x%x\n", $sm->stop2_gpio_pupdr_before[0], $sm->stop2_gpio_pupdr_before[1], $sm->stop2_gpio_pupdr_before[2], $sm->stop2_gpio_pupdr_before[3]
printf "gpio PUPDR sleep  A/B/C/H = 0x%x / 0x%x / 0x%x / 0x%x\n", $sm->stop2_gpio_pupdr_sleep[0], $sm->stop2_gpio_pupdr_sleep[1], $sm->stop2_gpio_pupdr_sleep[2], $sm->stop2_gpio_pupdr_sleep[3]
printf "gpio PUPDR after  A/B/C/H = 0x%x / 0x%x / 0x%x / 0x%x\n", $sm->stop2_gpio_pupdr_after[0], $sm->stop2_gpio_pupdr_after[1], $sm->stop2_gpio_pupdr_after[2], $sm->stop2_gpio_pupdr_after[3]
printf "gpio ODR before A/B/C/H = 0x%x / 0x%x / 0x%x / 0x%x\n", $sm->stop2_gpio_odr_before[0], $sm->stop2_gpio_odr_before[1], $sm->stop2_gpio_odr_before[2], $sm->stop2_gpio_odr_before[3]
printf "gpio ODR sleep  A/B/C/H = 0x%x / 0x%x / 0x%x / 0x%x\n", $sm->stop2_gpio_odr_sleep[0], $sm->stop2_gpio_odr_sleep[1], $sm->stop2_gpio_odr_sleep[2], $sm->stop2_gpio_odr_sleep[3]
printf "gpio ODR after  A/B/C/H = 0x%x / 0x%x / 0x%x / 0x%x\n", $sm->stop2_gpio_odr_after[0], $sm->stop2_gpio_odr_after[1], $sm->stop2_gpio_odr_after[2], $sm->stop2_gpio_odr_after[3]
printf "gpio IDR before A/B/C/H = 0x%x / 0x%x / 0x%x / 0x%x\n", $sm->stop2_gpio_idr_before[0], $sm->stop2_gpio_idr_before[1], $sm->stop2_gpio_idr_before[2], $sm->stop2_gpio_idr_before[3]
printf "gpio IDR sleep  A/B/C/H = 0x%x / 0x%x / 0x%x / 0x%x\n", $sm->stop2_gpio_idr_sleep[0], $sm->stop2_gpio_idr_sleep[1], $sm->stop2_gpio_idr_sleep[2], $sm->stop2_gpio_idr_sleep[3]
printf "gpio IDR after  A/B/C/H = 0x%x / 0x%x / 0x%x / 0x%x\n", $sm->stop2_gpio_idr_after[0], $sm->stop2_gpio_idr_after[1], $sm->stop2_gpio_idr_after[2], $sm->stop2_gpio_idr_after[3]
printf "BLE physical nrst/dsr/uart/reset/dsr-highz = %u / %u / 0x%x / %u / %u\n", $sm->ble_nrst_after, $sm->ble_dsr_host_control_after, $sm->ble_uart_deinit_status, $sm->ble_shutdown_reset_asserted, $sm->ble_dsr_highz_configured
printf "BLE sleep dsr override/target/before/after = %u / %u / %u / %u\n", g_ps_hw6_ble_sleep_dsr_deasserted, $sm->ble_dsr_sleep_target_level, $sm->ble_dsr_before_sleep_level, $sm->ble_dsr_after_sleep_level
printf "BLE dsr ticks assert/deassert settle start/end/ticks = %u / %u / %u / %u / %u\n", $sm->ble_dsr_assert_tick, $sm->ble_dsr_deassert_tick, $sm->ble_stop_settle_start_tick, $sm->ble_stop_settle_end_tick, $sm->ble_stop_settle_ticks
printf "BLE identity status/rx len = 0x%x / %u\n", $sm->ble_identity_status, $sm->ble_identity_rx_len
printf "BLE identity response: "
x/s &$sm->ble_identity_response[0]
printf "BLE command req/attempt/ok/err = 0x%x / 0x%x / 0x%x / 0x%x\n", $sm->ble_command_required_mask, $sm->ble_command_attempted_mask, $sm->ble_command_ok_mask, $sm->ble_command_error_mask
printf "storage power rel/jedec/match/dpd = 0x%x / 0x%x / %u / 0x%x\n", $sm->flash_power_release_status, $sm->flash_power_jedec_status, $sm->flash_power_identity_match, $sm->flash_power_deep_power_down_status
printf "storage clock post/release/status = 0x%x / 0x%x / 0x%x\n", $rt->storage_clock_post_stop_resume_status, $rt->storage_clock_release_status, $rt->storage_clock_last_status
printf "storage OSPI park/restore count = %u / %u\n", $sm->storage_ospi_park_count, $sm->storage_ospi_restore_count
printf "storage OSPI park ENR1/2 before = 0x%x / 0x%x\n", $sm->storage_ospi_park_ahb2enr1_before, $sm->storage_ospi_park_ahb2enr2_before
printf "storage OSPI park ENR1/2 after  = 0x%x / 0x%x\n", $sm->storage_ospi_park_ahb2enr1_after, $sm->storage_ospi_park_ahb2enr2_after
printf "storage OSPI park SMEN1/2 before = 0x%x / 0x%x\n", $sm->storage_ospi_park_ahb2smenr1_before, $sm->storage_ospi_park_ahb2smenr2_before
printf "storage OSPI park SMEN1/2 after  = 0x%x / 0x%x\n", $sm->storage_ospi_park_ahb2smenr1_after, $sm->storage_ospi_park_ahb2smenr2_after
printf "storage OSPI restore ENR1/2 before/after = 0x%x / 0x%x / 0x%x / 0x%x\n", $sm->storage_ospi_restore_ahb2enr1_before, $sm->storage_ospi_restore_ahb2enr2_before, $sm->storage_ospi_restore_ahb2enr1_after, $sm->storage_ospi_restore_ahb2enr2_after
printf "storage OSPI restore SMEN1/2 before/after = 0x%x / 0x%x / 0x%x / 0x%x\n", $sm->storage_ospi_restore_ahb2smenr1_before, $sm->storage_ospi_restore_ahb2smenr2_before, $sm->storage_ospi_restore_ahb2smenr1_after, $sm->storage_ospi_restore_ahb2smenr2_after
printf "wake expected/start/end IDR = 0x%x / 0x%x / 0x%x\n", $sm->stop2_expected_wake_pin, $sm->stop2_wake_start_idr, $sm->stop2_wake_end_idr
printf "wake class count/tick/source/primary = %u / %u / 0x%x / %u\n", $rt->stop2_wake_classify_count, $rt->stop2_wake_classify_tick, $rt->stop2_wake_source_mask, $rt->stop2_wake_primary_cause
printf "wake counts start/button/joy/sensor/pmic/unknown = %u / %u / %u / %u / %u / %u\n", $rt->stop2_wake_start_count, $rt->stop2_wake_button_count, $rt->stop2_wake_joystick_count, $rt->stop2_wake_sensor_count, $rt->stop2_wake_pmic_count, $rt->stop2_wake_unknown_count
printf "wake exti R/F/IMR = 0x%x / 0x%x / 0x%x\n", $rt->stop2_wake_exti_rising, $rt->stop2_wake_exti_falling, $rt->stop2_wake_exti_imr
printf "wake GPIOA before/after = 0x%x / 0x%x\n", $rt->stop2_wake_gpioa_before_idr, $rt->stop2_wake_gpioa_after_idr
printf "wake GPIOB before/after = 0x%x / 0x%x\n", $rt->stop2_wake_gpiob_before_idr, $rt->stop2_wake_gpiob_after_idr
printf "wake GPIOC before/after = 0x%x / 0x%x\n", $rt->stop2_wake_gpioc_before_idr, $rt->stop2_wake_gpioc_after_idr
printf "wake button edges before/after/last = %u / %u / %u / %u / %u\n", $rt->stop2_wake_button_edges_before, $rt->stop2_wake_button_edges_after, g_ps_input_buttons_probe.last_button_id, g_ps_input_buttons_probe.last_event, g_ps_input_buttons_probe.last_level
printf "wake pmic edges before/after/last = %u / %u / %u / %u\n", $rt->stop2_wake_pmic_edges_before, $rt->stop2_wake_pmic_edges_after, $rt->pmic_int_last_pin, $rt->pmic_int_last_level
printf "wake DBGMCU CR before/after = 0x%x / 0x%x\n", $rt->stop2_wake_dbgmcu_cr_before, $rt->stop2_wake_dbgmcu_cr_after
printf "wake SCB ICSR/SCR/SHCSR before = 0x%x / 0x%x / 0x%x\n", $rt->stop2_wake_scb_icsr_before, $rt->stop2_wake_scb_scr_before, $rt->stop2_wake_scb_shcsr_before
printf "wake SCB ICSR/SCR/SHCSR after  = 0x%x / 0x%x / 0x%x\n", $rt->stop2_wake_scb_icsr_after, $rt->stop2_wake_scb_scr_after, $rt->stop2_wake_scb_shcsr_after
printf "wake PWR SR/WUSR before/after = 0x%x / 0x%x / 0x%x / 0x%x\n", $rt->stop2_wake_pwr_sr_before, $rt->stop2_wake_pwr_wusr_before, $rt->stop2_wake_pwr_sr_after, $rt->stop2_wake_pwr_wusr_after
printf "wake PWR WUCR1/2/3 = 0x%x / 0x%x / 0x%x\n", $rt->stop2_wake_pwr_wucr1, $rt->stop2_wake_pwr_wucr2, $rt->stop2_wake_pwr_wucr3
printf "wake NVIC ISPR before = 0x%x / 0x%x / 0x%x / 0x%x\n", $rt->stop2_wake_nvic_ispr0_before, $rt->stop2_wake_nvic_ispr1_before, $rt->stop2_wake_nvic_ispr2_before, $rt->stop2_wake_nvic_ispr3_before
printf "wake NVIC ISPR after  = 0x%x / 0x%x / 0x%x / 0x%x\n", $rt->stop2_wake_nvic_ispr0_after, $rt->stop2_wake_nvic_ispr1_after, $rt->stop2_wake_nvic_ispr2_after, $rt->stop2_wake_nvic_ispr3_after
printf "wake NVIC IABR after  = 0x%x / 0x%x / 0x%x / 0x%x\n", $rt->stop2_wake_nvic_iabr0_after, $rt->stop2_wake_nvic_iabr1_after, $rt->stop2_wake_nvic_iabr2_after, $rt->stop2_wake_nvic_iabr3_after
printf "wake NVIC ISER after  = 0x%x / 0x%x / 0x%x / 0x%x\n", $rt->stop2_wake_nvic_iser0_after, $rt->stop2_wake_nvic_iser1_after, $rt->stop2_wake_nvic_iser2_after, $rt->stop2_wake_nvic_iser3_after
printf "power state/pmic/battery   = %u / %u / %u\n", $rt->stop2_eligibility_power_state, $rt->stop2_eligibility_pmic_state, $rt->stop2_eligibility_battery_policy
printf "runtime class/exec/life   = %u / %u / %u\n", $rt->stop2_eligibility_runtime_class, $rt->stop2_eligibility_runtime_execution, $rt->stop2_eligibility_runtime_lifecycle
printf "blockers: BOOT=0x1 POWER=0x2 PMIC=0x4 BATT=0x8 CLOCK_CAP=0x10 CLOCK_READBACK=0x20\n"
printf "display backends: NONE=0 HELD_FRAME=1 LPBAM=2\n"
printf "BLE modes: RESET_HELD=0 SLEEP_SYSTEM_OFF=1 SEARCHING=2 PAIRING=3 CONNECTED=4\n"
printf "IMU modes: OFF=0 LOW_RATE=1 EVENT_ARMED=2 STEP_COUNTER=3 STREAMING=4\n"
printf "wake masks: START=0x1 BUTTON=0x2 JOY=0x4 SENSOR=0x8 PMIC=0x10 RTC=0x20 USB=0x40 FAULT=0x80 UNKNOWN=0x80000000\n"
printf "wake causes: NONE=0 START=1 BUTTON=2 JOY=3 SENSOR=4 PMIC=5 RTC=6 USB=7 FAULT=8 UNKNOWN=9\n"
printf "SysTick CTRL bits: ENABLE=0x1 TICKINT=0x2 CLKSOURCE=0x4\n"
printf "GPIO park groups: OSPI=0x1 SAI=0x2 USB=0x4 DISPLAY_SPI=0x8 I2C=0x10; override NOT_RUN=0xffffffff uses knob default\n"
printf "status: HAL_OK=0x0 HAL_ERROR=0x1 UNAVAILABLE=0xfffffffe NOT_RUN=0xffffffff\n"
