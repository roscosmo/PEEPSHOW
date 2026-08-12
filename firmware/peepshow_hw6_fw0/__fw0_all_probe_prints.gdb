set pagination off

printf "=== HW6 FW0 BOOT + PERIPHERAL + RTOS + OWNER STATE PROBE ===\n"
printf "boot magic/version/phase = 0x%x / 0x%x / 0x%x\n", g_ps_hw6_fw0_probe.magic, g_ps_hw6_fw0_probe.version, g_ps_hw6_fw0_probe.phase
printf "boot complete/pre-RTOS hb = %u / 0x%x\n", g_ps_hw6_fw0_probe.complete, g_ps_hw6_fw0_probe.heartbeat
printf "MCU device/revision      = 0x%x / 0x%x\n", g_ps_hw6_fw0_probe.device_id, g_ps_hw6_fw0_probe.revision_id
printf "SYSCLK Hz                = %u\n", g_ps_hw6_fw0_probe.sysclk_hz
printf "output expected/actual   = 0x%x / 0x%x\n", g_ps_hw6_fw0_probe.expected_output_mask, g_ps_hw6_fw0_probe.output_mask
printf "boot errors/asserts      = %u / %u\n", g_ps_hw6_fw0_probe.error_count, g_ps_hw6_fw0_probe.assert_count

printf "\n--- consolidated result ---\n"
printf "magic/version/phase      = 0x%x / 0x%x / 0x%x\n", g_ps_hw6_peripheral_probe.magic, g_ps_hw6_peripheral_probe.version, g_ps_hw6_peripheral_probe.phase
printf "complete/duration ticks  = %u / %u\n", g_ps_hw6_peripheral_probe.complete, g_ps_hw6_peripheral_probe.duration_ticks
printf "required/attempted       = 0x%x / 0x%x\n", g_ps_hw6_peripheral_probe.required_mask, g_ps_hw6_peripheral_probe.attempted_mask
printf "pass/failure/skipped     = 0x%x / 0x%x / 0x%x\n", g_ps_hw6_peripheral_probe.pass_mask, g_ps_hw6_peripheral_probe.failure_mask, g_ps_hw6_peripheral_probe.skipped_mask
printf "PMIC / IMU / joystick    = %u / %u / %u\n", (g_ps_hw6_peripheral_probe.pass_mask >> 0) & 1, (g_ps_hw6_peripheral_probe.pass_mask >> 1) & 1, (g_ps_hw6_peripheral_probe.pass_mask >> 2) & 1
printf "flash / NINA AT          = %u / %u\n", (g_ps_hw6_peripheral_probe.pass_mask >> 3) & 1, (g_ps_hw6_peripheral_probe.pass_mask >> 4) & 1
printf "pre-RTOS skipped 5..7    = display/audio/USB physical tests\n"

printf "\n--- I2C bus ---\n"
printf "state before/after       = 0x%x / 0x%x\n", g_ps_hw6_peripheral_probe.i2c_state_before, g_ps_hw6_peripheral_probe.i2c_state_after
printf "error before/after       = 0x%x / 0x%x\n", g_ps_hw6_peripheral_probe.i2c_error_before, g_ps_hw6_peripheral_probe.i2c_error_after

printf "\n--- ADP5360 @ 0x%x ---\n", g_ps_hw6_peripheral_probe.pmic_address_7bit
printf "ready/id/fault/pgood sts = 0x%x / 0x%x / 0x%x / 0x%x\n", g_ps_hw6_peripheral_probe.pmic_ready_status, g_ps_hw6_peripheral_probe.pmic_id_status, g_ps_hw6_peripheral_probe.pmic_fault_status, g_ps_hw6_peripheral_probe.pmic_pgood_status
printf "id/fault/pgood           = 0x%x / 0x%x / 0x%x\n", g_ps_hw6_peripheral_probe.pmic_id, g_ps_hw6_peripheral_probe.pmic_fault, g_ps_hw6_peripheral_probe.pmic_pgood
printf "identity/rails ready     = %u / %u\n", g_ps_hw6_peripheral_probe.pmic_identity_match, g_ps_hw6_peripheral_probe.pmic_rails_ready

printf "\n--- LIS2DUX12 address diagnosis ---\n"
set $imu = &g_ps_hw6_peripheral_probe
printf "0x%x ready/error          = 0x%x / 0x%x\n", $imu->imu_address_7bit, $imu->imu_ready_status, $imu->imu_ready_error
printf "0x%x read/error/WHO_AM_I  = 0x%x / 0x%x / 0x%x\n", $imu->imu_address_7bit, $imu->imu_whoami_status, $imu->imu_whoami_error, $imu->imu_whoami
printf "0x%x ready/error          = 0x%x / 0x%x\n", $imu->imu_alt_address_7bit, $imu->imu_alt_ready_status, $imu->imu_alt_ready_error
printf "0x%x read/error/WHO_AM_I  = 0x%x / 0x%x / 0x%x\n", $imu->imu_alt_address_7bit, $imu->imu_alt_whoami_status, $imu->imu_alt_whoami_error, $imu->imu_alt_whoami
printf "detected address/match    = 0x%x / %u\n", $imu->imu_detected_address_7bit, $imu->imu_identity_match

printf "\n--- TMAG3001 @ 0x%x ---\n", g_ps_hw6_peripheral_probe.joystick_address_7bit
printf "ready/id/mfr statuses    = 0x%x / 0x%x / 0x%x / 0x%x\n", g_ps_hw6_peripheral_probe.joystick_ready_status, g_ps_hw6_peripheral_probe.joystick_device_id_status, g_ps_hw6_peripheral_probe.joystick_manufacturer_lsb_status, g_ps_hw6_peripheral_probe.joystick_manufacturer_msb_status
printf "device/mfr LSB/MSB       = 0x%x / 0x%x / 0x%x\n", g_ps_hw6_peripheral_probe.joystick_device_id, g_ps_hw6_peripheral_probe.joystick_manufacturer_lsb, g_ps_hw6_peripheral_probe.joystick_manufacturer_msb
printf "identity match           = %u\n", g_ps_hw6_peripheral_probe.joystick_identity_match

printf "\n--- AT25SL128A ---\n"
printf "JEDEC/status reads       = 0x%x / 0x%x / 0x%x / 0x%x\n", g_ps_hw6_peripheral_probe.flash_jedec_status, g_ps_hw6_peripheral_probe.flash_status1_status, g_ps_hw6_peripheral_probe.flash_status2_status, g_ps_hw6_peripheral_probe.flash_status3_status
printf "JEDEC ID                 = %02x %02x %02x\n", g_ps_hw6_peripheral_probe.flash_jedec_id[0], g_ps_hw6_peripheral_probe.flash_jedec_id[1], g_ps_hw6_peripheral_probe.flash_jedec_id[2]
printf "status 1/2/3             = %02x %02x %02x\n", g_ps_hw6_peripheral_probe.flash_status[0], g_ps_hw6_peripheral_probe.flash_status[1], g_ps_hw6_peripheral_probe.flash_status[2]
printf "identity / OSPI error    = %u / 0x%x\n", g_ps_hw6_peripheral_probe.flash_identity_match, g_ps_hw6_peripheral_probe.flash_ospi_error_after

printf "\n--- NINA-B112 AT handshake ---\n"
printf "NRST before/up/after     = %u / %u / %u\n", g_ps_hw6_peripheral_probe.nina_nrst_before, g_ps_hw6_peripheral_probe.nina_nrst_released, g_ps_hw6_peripheral_probe.nina_nrst_after
printf "boot wait/rx             = %u ms / %u bytes\n", g_ps_hw6_peripheral_probe.nina_boot_wait_ms, g_ps_hw6_peripheral_probe.nina_boot_rx_len
printf "attempts/tx/rx           = %u / 0x%x / %u bytes\n", g_ps_hw6_peripheral_probe.nina_attempt_count, g_ps_hw6_peripheral_probe.nina_at_tx_status, g_ps_hw6_peripheral_probe.nina_at_rx_len
printf "OK / ERROR               = %u / %u\n", g_ps_hw6_peripheral_probe.nina_at_ok, g_ps_hw6_peripheral_probe.nina_at_error
printf "UART state/error         = 0x%x / 0x%x\n", g_ps_hw6_peripheral_probe.nina_uart_state_after, g_ps_hw6_peripheral_probe.nina_uart_error_after
printf "response                 = %s\n", g_ps_hw6_peripheral_probe.nina_at_rx

printf "\n--- pre-RTOS peripheral state ---\n"
printf "RTC / SAI                = 0x%x / 0x%x\n", g_ps_hw6_peripheral_probe.rtc_state, g_ps_hw6_peripheral_probe.audio_sai_state
printf "display SPI / LPTIM      = 0x%x / 0x%x\n", g_ps_hw6_peripheral_probe.display_spi_state, g_ps_hw6_peripheral_probe.display_lptim_state
printf "USB PCD                  = 0x%x\n", g_ps_hw6_peripheral_probe.usb_pcd_state

printf "\n--- ThreadX topology and startup self-test ---\n"
set $rtos = &g_ps_hw6_rtos_probe
printf "magic/version/phase       = 0x%x / 0x%x / 0x%x\n", $rtos->magic, $rtos->version, $rtos->phase
printf "init/runtime complete     = %u / %u\n", $rtos->init_complete, $rtos->runtime_complete
printf "boot power/display bootstrap = %u / %u\n", $rtos->boot_power_done, $rtos->boot_display_bootstrap_sent
printf "init status/error step/id = 0x%x / 0x%x / 0x%x\n", $rtos->init_status, $rtos->init_error_step, $rtos->init_error_index
printf "tick Hz/owners/queues/egs = %u / %u / %u / %u\n", $rtos->ticks_per_second, $rtos->owner_count, $rtos->queue_count, $rtos->event_group_count
printf "owner required/started    = 0x%x / 0x%x\n", $rtos->owner_required_mask, $rtos->owner_start_mask
printf "queue required/self-test  = 0x%x / 0x%x\n", $rtos->queue_required_mask, $rtos->queue_selftest_mask
printf "event required/self-test  = 0x%x / 0x%x\n", $rtos->event_required_mask, $rtos->event_selftest_mask
printf "pool info before/after    = 0x%x / 0x%x\n", $rtos->pool_info_before_status, $rtos->pool_info_after_status
printf "pool bytes before/after   = %u / %u\n", $rtos->pool_available_before, $rtos->pool_available_after
printf "pool fragments before/after = %u / %u\n", $rtos->pool_fragments_before, $rtos->pool_fragments_after

printf "\nowner/queue object status: stack qmem qcreate send thread\n"
printf "power   /qSysEvents       %x %x %x %x %x\n", $rtos->stack_alloc_status[0], $rtos->queue_alloc_status[0], $rtos->queue_create_status[0], $rtos->queue_selftest_send_status[0], $rtos->thread_create_status[0]
printf "audio   /qAudioCmd        %x %x %x %x %x\n", $rtos->stack_alloc_status[1], $rtos->queue_alloc_status[1], $rtos->queue_create_status[1], $rtos->queue_selftest_send_status[1], $rtos->thread_create_status[1]
printf "input   /qInputRaw        %x %x %x %x %x\n", $rtos->stack_alloc_status[2], $rtos->queue_alloc_status[2], $rtos->queue_create_status[2], $rtos->queue_selftest_send_status[2], $rtos->thread_create_status[2]
printf "display /qDisplayCmd      %x %x %x %x %x\n", $rtos->stack_alloc_status[3], $rtos->queue_alloc_status[3], $rtos->queue_create_status[3], $rtos->queue_selftest_send_status[3], $rtos->thread_create_status[3]
printf "sensor  /qSensorReq       %x %x %x %x %x\n", $rtos->stack_alloc_status[4], $rtos->queue_alloc_status[4], $rtos->queue_create_status[4], $rtos->queue_selftest_send_status[4], $rtos->thread_create_status[4]
printf "storage /qStorageReq      %x %x %x %x %x\n", $rtos->stack_alloc_status[5], $rtos->queue_alloc_status[5], $rtos->queue_create_status[5], $rtos->queue_selftest_send_status[5], $rtos->thread_create_status[5]
printf "comm    /qCommCmd         %x %x %x %x %x\n", $rtos->stack_alloc_status[6], $rtos->queue_alloc_status[6], $rtos->queue_create_status[6], $rtos->queue_selftest_send_status[6], $rtos->thread_create_status[6]
printf "UI      /qUIEvents        %x %x %x %x %x\n", $rtos->stack_alloc_status[7], $rtos->queue_alloc_status[7], $rtos->queue_create_status[7], $rtos->queue_selftest_send_status[7], $rtos->thread_create_status[7]
printf "runtime /qRuntimeEvents   %x %x %x %x %x\n", $rtos->stack_alloc_status[8], $rtos->queue_alloc_status[8], $rtos->queue_create_status[8], $rtos->queue_selftest_send_status[8], $rtos->thread_create_status[8]

printf "\nevent group status: create set get\n"
printf "egMode                    %x %x %x\n", $rtos->event_create_status[0], $rtos->event_set_status[0], $rtos->event_get_status[0]
printf "egPower                   %x %x %x\n", $rtos->event_create_status[1], $rtos->event_set_status[1], $rtos->event_get_status[1]
printf "egHealth                  %x %x %x\n", $rtos->event_create_status[2], $rtos->event_set_status[2], $rtos->event_get_status[2]
printf "egDebug                   %x %x %x\n", $rtos->event_create_status[3], $rtos->event_set_status[3], $rtos->event_get_status[3]

printf "\nowner runtime: heartbeat last_tick receives timeouts errors\n"
printf "power                     %u %u %u %u %u\n", $rtos->owner_heartbeat[0], $rtos->owner_last_tick[0], $rtos->queue_receive_count[0], $rtos->queue_timeout_count[0], $rtos->queue_message_error_count[0]
printf "audio                     %u %u %u %u %u\n", $rtos->owner_heartbeat[1], $rtos->owner_last_tick[1], $rtos->queue_receive_count[1], $rtos->queue_timeout_count[1], $rtos->queue_message_error_count[1]
printf "input                     %u %u %u %u %u\n", $rtos->owner_heartbeat[2], $rtos->owner_last_tick[2], $rtos->queue_receive_count[2], $rtos->queue_timeout_count[2], $rtos->queue_message_error_count[2]
printf "display                   %u %u %u %u %u\n", $rtos->owner_heartbeat[3], $rtos->owner_last_tick[3], $rtos->queue_receive_count[3], $rtos->queue_timeout_count[3], $rtos->queue_message_error_count[3]
printf "sensor                    %u %u %u %u %u\n", $rtos->owner_heartbeat[4], $rtos->owner_last_tick[4], $rtos->queue_receive_count[4], $rtos->queue_timeout_count[4], $rtos->queue_message_error_count[4]
printf "storage                   %u %u %u %u %u\n", $rtos->owner_heartbeat[5], $rtos->owner_last_tick[5], $rtos->queue_receive_count[5], $rtos->queue_timeout_count[5], $rtos->queue_message_error_count[5]
printf "comm                      %u %u %u %u %u\n", $rtos->owner_heartbeat[6], $rtos->owner_last_tick[6], $rtos->queue_receive_count[6], $rtos->queue_timeout_count[6], $rtos->queue_message_error_count[6]
printf "UI                        %u %u %u %u %u\n", $rtos->owner_heartbeat[7], $rtos->owner_last_tick[7], $rtos->queue_receive_count[7], $rtos->queue_timeout_count[7], $rtos->queue_message_error_count[7]
printf "runtime                   %u %u %u %u %u\n", $rtos->owner_heartbeat[8], $rtos->owner_last_tick[8], $rtos->queue_receive_count[8], $rtos->queue_timeout_count[8], $rtos->queue_message_error_count[8]

printf "\n--- normal boot owner services ---\n"
set $owner = &g_ps_hw6_owner_probe
set $sm = &g_ps_hw6_owner_sm_probe
printf "magic/version/phase       = 0x%x / 0x%x / 0x%x\n", $owner->magic, $owner->version, $owner->phase
printf "complete/success/init     = %u / %u / 0x%x\n", $owner->complete, $owner->success, $owner->services_init_status
printf "diagnostic workflow ticks = %u / %u / %u (0 means not requested)\n", $owner->workflow_start_tick, $owner->workflow_end_tick, $owner->workflow_end_tick - $owner->workflow_start_tick
printf "power command send/tick   = 0x%x / %u\n", $owner->power_command_send_status, $owner->power_command_tick
printf "display send/ack/flags    = 0x%x / 0x%x / 0x%x\n", $owner->display_command_send_status, $owner->display_ack_wait_status, $owner->display_ack_flags
printf "audio send/ack/flags      = 0x%x / 0x%x / 0x%x\n", $owner->audio_command_send_status, $owner->audio_ack_wait_status, $owner->audio_ack_flags

printf "\n  power owner: read-only ADP5360 snapshot\n"
printf "command/complete/success  = %u / %u / %u\n", $owner->power_command_tick, $owner->power_complete, $owner->power_success
printf "ADP driver API/init/MR/sw/state/ops/last = %u / %u / %u / %u / %u / %u / %u\n", $owner->power_driver_api_version, $owner->power_driver_init_status, $owner->power_driver_mr_shipping_mode_status, $owner->power_driver_software_shipping_mode_status, $owner->power_driver_state, $owner->power_driver_operation_count, $owner->power_driver_last_status
printf "ADP sw ship count/tick/request = %u / %u / %u\n", $owner->power_software_ship_request_count, $owner->power_software_ship_request_tick, g_ps_hw6_pmic_software_ship_request
printf "START quiesce count/status/tick = %u / 0x%x / %u\n", $sm->start_power_quiesce_request_count, $sm->start_power_quiesce_last_status, $sm->start_power_quiesce_last_tick
printf "START sw ship en/req/skip/status/tick = %u / %u / %u / 0x%x / %u\n", $sm->start_power_software_ship_enabled, $sm->start_power_software_ship_request_count, $sm->start_power_software_ship_skipped_count, $sm->start_power_software_ship_last_status, $sm->start_power_software_ship_last_tick
printf "ADP functions/read/match = 0x%x / 0x%x / 0x%x\n", $owner->power_driver_function_ready_mask, $owner->power_driver_read_ok_mask, $owner->power_driver_expected_match_mask
printf "register addresses        = %02x %02x %02x %02x %02x %02x %02x\n", $owner->power_register_address[0], $owner->power_register_address[1], $owner->power_register_address[2], $owner->power_register_address[3], $owner->power_register_address[4], $owner->power_register_address[5], $owner->power_register_address[6]
printf "register values           = %02x %02x %02x %02x %02x %02x %02x\n", $owner->power_register_value[0], $owner->power_register_value[1], $owner->power_register_value[2], $owner->power_register_value[3], $owner->power_register_value[4], $owner->power_register_value[5], $owner->power_register_value[6]
printf "lease get status          = %x %x %x %x %x %x %x\n", $owner->power_lease_get_status[0], $owner->power_lease_get_status[1], $owner->power_lease_get_status[2], $owner->power_lease_get_status[3], $owner->power_lease_get_status[4], $owner->power_lease_get_status[5], $owner->power_lease_get_status[6]
printf "HAL transfer status       = %x %x %x %x %x %x %x\n", $owner->power_transfer_status[0], $owner->power_transfer_status[1], $owner->power_transfer_status[2], $owner->power_transfer_status[3], $owner->power_transfer_status[4], $owner->power_transfer_status[5], $owner->power_transfer_status[6]
printf "HAL transfer errors       = %x %x %x %x %x %x %x\n", $owner->power_transfer_error[0], $owner->power_transfer_error[1], $owner->power_transfer_error[2], $owner->power_transfer_error[3], $owner->power_transfer_error[4], $owner->power_transfer_error[5], $owner->power_transfer_error[6]
printf "lease put status          = %x %x %x %x %x %x %x\n", $owner->power_lease_put_status[0], $owner->power_lease_put_status[1], $owner->power_lease_put_status[2], $owner->power_lease_put_status[3], $owner->power_lease_put_status[4], $owner->power_lease_put_status[5], $owner->power_lease_put_status[6]
printf "I2C state/error           = 0x%x / 0x%x\n", $owner->power_i2c_state_after, $owner->power_i2c_error_after
printf "identity/rails/fault OK   = %u / %u / %u\n", $owner->power_identity_match, $owner->power_rails_ready, $owner->power_fault_clear
printf "ADP charger status regs  = %02x / %02x\n", $owner->power_charger_status1, $owner->power_charger_status2
printf "ADP charger read sts     = 0x%x / 0x%x mask 0x%x\n", $owner->power_charger_status1_status, $owner->power_charger_status2_status, $owner->power_charger_monitor_read_ok_mask
printf "ADP charger HAL sts/err  = 0x%x/0x%x 0x%x/0x%x\n", $owner->power_charger_status1_hal_status, $owner->power_charger_status1_hal_error, $owner->power_charger_status2_hal_status, $owner->power_charger_status2_hal_error
printf "ADP mode/status/type/health = %u / %u / %u / %u\n", $owner->power_charger_mode, $owner->power_charger_status, $owner->power_charger_charge_type, $owner->power_charger_health
printf "ADP battery stat/therm/present = %u / %u / %u\n", $owner->power_battery_status, $owner->power_battery_thermal_status, $owner->power_battery_present
printf "ADP PGOOD vbus/bat/full = %u / %u / %u\n", $owner->power_vbus_ok, $owner->power_battery_ok, $owner->power_charge_complete
printf "ADP fuel regs            = %02x %02x %02x %02x %02x\n", $owner->power_fuel_register_address[0], $owner->power_fuel_register_address[1], $owner->power_fuel_register_address[2], $owner->power_fuel_register_address[3], $owner->power_fuel_register_address[4]
printf "ADP fuel values          = %02x %02x %02x %02x %02x\n", $owner->power_fuel_register_value[0], $owner->power_fuel_register_value[1], $owner->power_fuel_register_value[2], $owner->power_fuel_register_value[3], $owner->power_fuel_register_value[4]
printf "ADP fuel read mask       = 0x%x\n", $owner->power_fuel_read_ok_mask
printf "ADP fuel raw sts         = %x %x %x %x %x\n", $owner->power_fuel_register_status[0], $owner->power_fuel_register_status[1], $owner->power_fuel_register_status[2], $owner->power_fuel_register_status[3], $owner->power_fuel_register_status[4]
printf "ADP fuel SOC/VBAT        = %u %% / %u mV (raw %02x %02x)\n", $owner->power_fuel_soc_percent, $owner->power_fuel_vbat_mv, $owner->power_fuel_vbat_h, $owner->power_fuel_vbat_l
printf "ADP regulator read mask  = 0x%x\n", $owner->power_regulator_read_ok_mask
printf "ADP regulator cfg/out    = buck %02x/%02x buckbst %02x/%02x\n", $owner->power_regulator_buck_cfg, $owner->power_regulator_buck_output, $owner->power_regulator_buckbst_cfg, $owner->power_regulator_buckbst_output
printf "ADP regulator ok bits    = vout1 %u vout2 %u bat %u\n", $owner->power_regulator_vout1_ok, $owner->power_regulator_vout2_ok, $owner->power_regulator_battery_ok

printf "\n  display owner: boot clear hold or current UI framebuffer\n"
printf "command/complete/success  = %u / %u / %u\n", $owner->display_command_tick, $owner->display_complete, $owner->display_success
printf "display driver API/init/state/ops/last = %u / 0x%x / %u / %u / 0x%x\n", $owner->display_driver_api_version, $owner->display_driver_init_status, $owner->display_driver_state, $owner->display_driver_operation_count, $owner->display_driver_last_status
printf "size/pattern/hash/black   = %ux%u / 0x%x / 0x%x / %u\n", $owner->display_width, $owner->display_height, $owner->display_pattern_id, $owner->display_framebuffer_hash, $owner->display_black_pixels
printf "hash/black note          = clear hold is all-white; UI pages are content dependent\n"
printf "RTC state/CR              = 0x%x / 0x%x\n", $owner->display_rtc_state, $owner->display_rtc_cr
printf "SPI before/init/present   = 0x%x / 0x%x / 0x%x\n", $owner->display_spi_state_before, $owner->display_init_status, $owner->display_present_status
printf "DMA done/state/error      = %u / 0x%x / 0x%x\n", $owner->display_dma_done, $owner->display_dma_state_after, $owner->display_dma_error_after
printf "SPI state/error after     = 0x%x / 0x%x\n", $owner->display_spi_state_after, $owner->display_spi_error_after
printf "ack set status            = 0x%x\n", $owner->display_ack_set_status
printf "expected visual           = early blank hold, then current UI page\n"
printf "display UI page/cal/focus = %u / %u / %u\n", $owner->display_ui_page, $owner->display_ui_calibration_page, $owner->display_ui_focus_index
printf "display shutdown/cd       = %u / %u\n", $owner->display_ui_shutdown_state, $owner->display_ui_shutdown_countdown_seconds
printf "ui shutdown: NONE=0 PREP=1 WARNING=2 IMMINENT=3 CANCELLED=4 LOW_BOOT=5 LOW_CHARGE=6 FLASH_INIT=7 FLASH_DONE=8 FLASH_ERROR=9 MSC_EXPORT=10 MSC_ACTIVE=11 MSC_RECLAIM=12 MSC_DONE=13 MSC_ERROR=14 MSC_RECOVERY=15\n"
printf "expected normal boot      = display bootstrap sent, then HOME render after power\n"

printf "\n  audio owner: idle unless diagnostic workflow requested\n"
printf "command/complete/success  = %u / %u / %u\n", $owner->audio_command_tick, $owner->audio_complete, $owner->audio_success
printf "audio driver API/init/state/ops/last = %u / 0x%x / %u / %u / 0x%x\n", $owner->audio_driver_api_version, $owner->audio_driver_init_status, $owner->audio_driver_state, $owner->audio_driver_operation_count, $owner->audio_driver_last_status
printf "kernel/sample/tone Hz     = %u / %u / %u\n", $owner->audio_sai_kernel_hz, $owner->audio_sample_rate_hz, $owner->audio_tone_hz
printf "duration/amplitude/buffer = %u ms / %u / %u halfwords\n", $owner->audio_duration_ms, $owner->audio_amplitude, $owner->audio_buffer_halfwords
printf "SD before/enabled/after   = %u / %u / %u\n", $owner->audio_sd_state_before, $owner->audio_sd_state_enabled, $owner->audio_sd_state_after
printf "start/stop status         = 0x%x / 0x%x\n", $owner->audio_start_status, $owner->audio_stop_status
printf "SAI state/error after     = 0x%x / 0x%x\n", $owner->audio_sai_state_after, $owner->audio_sai_error_after
printf "DMA state/error after     = 0x%x / 0x%x\n", $owner->audio_dma_state_after, $owner->audio_dma_error_after
printf "ack set status            = 0x%x\n", $owner->audio_ack_set_status
printf "expected normal boot      = silent; tone fields are diagnostic-only\n"

printf "\n--- normal boot lifecycle state-machine status ---\n"
set $sm = &g_ps_hw6_owner_sm_probe
printf "start request              = %u\n", g_ps_hw6_owner_sm_start_request
printf "magic/version/phase        = 0x%x / 0x%x / 0x%x\n", $sm->magic, $sm->version, $sm->phase
printf "complete/success           = %u / %u\n", $sm->complete, $sm->success
printf "required/completed         = 0x%x / 0x%x\n", $sm->required_owner_mask, $sm->completed_owner_mask
printf "normal boot note         = power owner completes; storage may show USB boot-park transport\n"
printf "success/failure owners     = 0x%x / 0x%x\n", $sm->success_owner_mask, $sm->failure_owner_mask
printf "diagnostic workflow ticks  = %u / %u / %u (0 means not requested)\n", $sm->workflow_start_tick, $sm->workflow_end_tick, $sm->workflow_end_tick - $sm->workflow_start_tick

printf "START overlay state/active/hold ticks = %u / %u / %u\n", g_ps_input_buttons_probe.start_state, g_ps_input_buttons_probe.start_active, g_ps_input_buttons_probe.start_hold_ticks
printf "START armed/live/next/check/synth = %u / %u / %u / %u / %u\n", g_ps_input_buttons_probe.start_armed, g_ps_input_buttons_probe.start_live_level, g_ps_input_buttons_probe.start_next_check_tick, g_ps_input_buttons_probe.start_checkpoint_count, g_ps_input_buttons_probe.start_synth_release_count
printf "START prep/warn/imm/release/drop = %u / %u / %u / %u / %u\n", g_ps_input_buttons_probe.start_ship_prep_count, g_ps_input_buttons_probe.start_ship_warning_count, g_ps_input_buttons_probe.start_ship_imminent_count, g_ps_input_buttons_probe.start_release_before_ship_count, g_ps_input_buttons_probe.start_pending_drop_count
printf "START power count/event/status/hold ticks = %u / %u / 0x%x / %u\n", $sm->start_power_event_count, $sm->start_power_last_event, $sm->start_power_last_status, $sm->start_power_last_hold_ticks
printf "START power return state = %u\n", $sm->start_power_return_state
printf "START power prep/warn/imm/cancel = %u / %u / %u / %u\n", $sm->start_power_ship_prep_count, $sm->start_power_ship_warning_count, $sm->start_power_ship_imminent_count, $sm->start_power_cancel_count
printf "START power quiesce count/status/tick = %u / 0x%x / %u\n", $sm->start_power_quiesce_request_count, $sm->start_power_quiesce_last_status, $sm->start_power_quiesce_last_tick
printf "START power sw ship en/req/skip/status/tick = %u / %u / %u / 0x%x / %u\n", $sm->start_power_software_ship_enabled, $sm->start_power_software_ship_request_count, $sm->start_power_software_ship_skipped_count, $sm->start_power_software_ship_last_status, $sm->start_power_software_ship_last_tick
printf "\nowner lifecycle transport: send wait ack action start end\n"
printf "power                      %x %x %x %x %u %u\n", $sm->owner_command_send_status[0], $sm->owner_ack_wait_status[0], $sm->owner_ack_flags[0], $sm->owner_action_status[0], $sm->owner_action_start_tick[0], $sm->owner_action_end_tick[0]
printf "audio                      %x %x %x %x %u %u\n", $sm->owner_command_send_status[1], $sm->owner_ack_wait_status[1], $sm->owner_ack_flags[1], $sm->owner_action_status[1], $sm->owner_action_start_tick[1], $sm->owner_action_end_tick[1]
printf "input                      %x %x %x %x %u %u\n", $sm->owner_command_send_status[2], $sm->owner_ack_wait_status[2], $sm->owner_ack_flags[2], $sm->owner_action_status[2], $sm->owner_action_start_tick[2], $sm->owner_action_end_tick[2]
printf "display                    %x %x %x %x %u %u\n", $sm->owner_command_send_status[3], $sm->owner_ack_wait_status[3], $sm->owner_ack_flags[3], $sm->owner_action_status[3], $sm->owner_action_start_tick[3], $sm->owner_action_end_tick[3]
printf "sensor                     %x %x %x %x %u %u\n", $sm->owner_command_send_status[4], $sm->owner_ack_wait_status[4], $sm->owner_ack_flags[4], $sm->owner_action_status[4], $sm->owner_action_start_tick[4], $sm->owner_action_end_tick[4]
printf "storage                    %x %x %x %x %u %u\n", $sm->owner_command_send_status[5], $sm->owner_ack_wait_status[5], $sm->owner_ack_flags[5], $sm->owner_action_status[5], $sm->owner_action_start_tick[5], $sm->owner_action_end_tick[5]
printf "comm                       %x %x %x %x %u %u\n", $sm->owner_command_send_status[6], $sm->owner_ack_wait_status[6], $sm->owner_ack_flags[6], $sm->owner_action_status[6], $sm->owner_action_start_tick[6], $sm->owner_action_end_tick[6]

printf "\n--- diagnostic inactive-active-inactive owner cycles (not normal boot) ---\n"
printf "requested/completed/success = %u / %u / %u\n", $sm->cycle_requested_count, $sm->cycle_completed_count, $sm->cycle_success
printf "required owner/state masks  = 0x7f / 0x3ff\n"
printf "owner map: 0=power 1=audio 2=input 3=display 4=sensor 5=storage 6=comm\n"
set $cycle = 0
while $cycle < 2
  printf "\ncycle %u start/active/end     = %u / %u / %u ticks\n", $cycle, $sm->cycle_start_tick[$cycle], $sm->cycle_active_tick[$cycle], $sm->cycle_end_tick[$cycle]
  printf "resume success/failure      = 0x%x / 0x%x\n", $sm->cycle_resume_success_mask[$cycle], $sm->cycle_resume_failure_mask[$cycle]
  printf "quiesce success/failure     = 0x%x / 0x%x\n", $sm->cycle_quiesce_success_mask[$cycle], $sm->cycle_quiesce_failure_mask[$cycle]
  printf "active/inactive state match = 0x%x / 0x%x\n", $sm->cycle_active_state_match_mask[$cycle], $sm->cycle_inactive_state_match_mask[$cycle]
  set $direction = 0
  while $direction < 2
    if $direction == 0
      printf "  resume transport/action: owner send wait ack action start end\n"
    else
      printf "  quiesce transport/action: owner send wait ack action start end\n"
    end
    set $physical_owner = 0
    while $physical_owner < 7
      printf "  %u %x %x %x %x %u %u\n", $physical_owner, $sm->cycle_command_send_status[$cycle][$direction][$physical_owner], $sm->cycle_ack_wait_status[$cycle][$direction][$physical_owner], $sm->cycle_ack_flags[$cycle][$direction][$physical_owner], $sm->cycle_action_status[$cycle][$direction][$physical_owner], $sm->cycle_action_start_tick[$cycle][$direction][$physical_owner], $sm->cycle_action_end_tick[$cycle][$direction][$physical_owner]
      set $physical_owner = $physical_owner + 1
    end
    set $direction = $direction + 1
  end

  printf "  TMAG wake/retry/active/sleep = %x %x %x %x\n", $sm->joystick_cycle_wake_probe_status[$cycle], $sm->joystick_cycle_wake_retry_status[$cycle], $sm->joystick_cycle_active_status[$cycle], $sm->joystick_cycle_sleep_status[$cycle]
  printf "  TMAG active config1/config2  = %02x / %02x\n", $sm->joystick_cycle_active_sensor_config1[$cycle], $sm->joystick_cycle_active_device_config2[$cycle]
  printf "  TMAG sample status/conv     = %x / %02x\n", $sm->joystick_cycle_sample_status[$cycle], $sm->joystick_cycle_sample_conv_status[$cycle]
  printf "  TMAG raw X/Y/Z              = %d / %d / %d\n", $sm->joystick_cycle_sample_x[$cycle], $sm->joystick_cycle_sample_y[$cycle], $sm->joystick_cycle_sample_z[$cycle]
  printf "  IMU wake status/error/accepted = %x %x %u\n", $sm->imu_cycle_wake_probe_status[$cycle], $sm->imu_cycle_wake_probe_error[$cycle], $sm->imu_cycle_wake_probe_accepted[$cycle]
  printf "  IMU WHO/active/sleep         = %x %x %x\n", $sm->imu_cycle_whoami_status[$cycle], $sm->imu_cycle_active_status[$cycle], $sm->imu_cycle_sleep_status[$cycle]
  printf "  IMU WHO/active CTRL5         = %02x / %02x (expected 47 / 10)\n", $sm->imu_cycle_whoami[$cycle], $sm->imu_cycle_active_ctrl5[$cycle]
  printf "  flash release/JEDEC/match/DPD = %x %x %u %x\n", $sm->flash_cycle_release_status[$cycle], $sm->flash_cycle_jedec_status[$cycle], $sm->flash_cycle_identity_match[$cycle], $sm->flash_cycle_deep_power_down_status[$cycle]
  printf "  BLE UART/AT/RX/suspend       = %x %x %u %x\n", $sm->ble_cycle_uart_init_status[$cycle], $sm->ble_cycle_wake_at_status[$cycle], $sm->ble_cycle_wake_rx_len[$cycle], $sm->ble_cycle_suspend_uart_status[$cycle]
  printf "  BLE DSR resume/quiesce       = %u / %u\n", $sm->ble_cycle_dsr_after_resume[$cycle], $sm->ble_cycle_dsr_after_quiesce[$cycle]
  set $cycle = $cycle + 1
end

printf "\nFSM final state: current expected transitions rejected last_event last_status last_error\n"
printf "power                      %u 2  %u %u %u %x %x\n", $sm->current_state[0], $sm->transition_count[0], $sm->rejected_transition_count[0], $sm->last_event[0], $sm->last_action_status[0], $sm->last_error[0]
printf "PMIC                       %u 3  %u %u %u %x %x\n", $sm->current_state[1], $sm->transition_count[1], $sm->rejected_transition_count[1], $sm->last_event[1], $sm->last_action_status[1], $sm->last_error[1]
printf "display                    %u 2  %u %u %u %x %x\n", $sm->current_state[2], $sm->transition_count[2], $sm->rejected_transition_count[2], $sm->last_event[2], $sm->last_action_status[2], $sm->last_error[2]
printf "audio                      %u 2  %u %u %u %x %x\n", $sm->current_state[3], $sm->transition_count[3], $sm->rejected_transition_count[3], $sm->last_event[3], $sm->last_action_status[3], $sm->last_error[3]
printf "speaker                    %u 0  %u %u %u %x %x\n", $sm->current_state[4], $sm->transition_count[4], $sm->rejected_transition_count[4], $sm->last_event[4], $sm->last_action_status[4], $sm->last_error[4]
printf "joystick                   %u 12 %u %u %u %x %x\n", $sm->current_state[5], $sm->transition_count[5], $sm->rejected_transition_count[5], $sm->last_event[5], $sm->last_action_status[5], $sm->last_error[5]
printf "IMU                        %u 8  %u %u %u %x %x\n", $sm->current_state[6], $sm->transition_count[6], $sm->rejected_transition_count[6], $sm->last_event[6], $sm->last_action_status[6], $sm->last_error[6]
printf "storage                    %u 2  %u %u %u %x %x\n", $sm->current_state[7], $sm->transition_count[7], $sm->rejected_transition_count[7], $sm->last_event[7], $sm->last_action_status[7], $sm->last_error[7]
printf "flash                      %u 8  %u %u %u %x %x\n", $sm->current_state[8], $sm->transition_count[8], $sm->rejected_transition_count[8], $sm->last_event[8], $sm->last_action_status[8], $sm->last_error[8]
printf "BLE                        %u 10 %u %u %u %x %x\n", $sm->current_state[9], $sm->transition_count[9], $sm->rejected_transition_count[9], $sm->last_event[9], $sm->last_action_status[9], $sm->last_error[9]

printf "\n  TMAG3001 verified suspended baseline\n"
printf "driver API/init/state/ops/last = %u / %u / %u / %u / %u\n", $sm->joystick_driver_api_version, $sm->joystick_driver_init_status, $sm->joystick_driver_state, $sm->joystick_driver_operation_count, $sm->joystick_driver_last_status
printf "ready/identity/match       = 0x%x / 0x%x / %u\n", $sm->joystick_ready_status, $sm->joystick_identity_status, $sm->joystick_identity_match
printf "device/mfr LSB/MSB         = %02x / %02x / %02x\n", $sm->joystick_device_id, $sm->joystick_manufacturer_lsb, $sm->joystick_manufacturer_msb
printf "SENSOR_CONFIG1 before/verified = %02x / %02x\n", $sm->joystick_sensor_config1_before, $sm->joystick_sensor_config1_after
printf "DEVICE_CONFIG2 before/verified/sleep = %02x / %02x / %02x\n", $sm->joystick_device_config2_before, $sm->joystick_device_config2_after, $sm->joystick_device_config2_sleep
printf "pre-terminal verify status = 0x%x / 0x%x\n", $sm->joystick_sensor_config1_verify_status, $sm->joystick_device_config2_verify_status
printf "write/verify masks         = 0x%x / 0x%x (expected 0x7 / 0x3)\n", $sm->joystick_write_ok_mask, $sm->joystick_verify_ok_mask
printf "terminal sleep status/committed = 0x%x / %u\n", $sm->joystick_sleep_write_status, $sm->joystick_terminal_sleep_committed
printf "post-sleep read omitted    = %u (expected 1; any address probe wakes TMAG)\n", $sm->joystick_post_sleep_read_omitted
printf "I2C state/error            = 0x%x / 0x%x\n", $sm->joystick_i2c_state_after, $sm->joystick_i2c_error_after

printf "\n  LIS2DUX12 verified deep-power-down baseline\n"
printf "driver API/init/state/ops/last = %u / %u / %u / %u / %u\n", $sm->imu_driver_api_version, $sm->imu_driver_init_status, $sm->imu_driver_state, $sm->imu_driver_operation_count, $sm->imu_driver_last_status
printf "ready/WHO status/value/match = 0x%x / 0x%x / %02x / %u\n", $sm->imu_ready_status, $sm->imu_whoami_status, $sm->imu_whoami, $sm->imu_identity_match
printf "register addresses         ="
set $i = 0
while $i < 11
  printf " %02x", $sm->imu_register_address[$i]
  set $i = $i + 1
end
printf "\nregister before            ="
set $i = 0
while $i < 11
  printf " %02x", $sm->imu_register_before[$i]
  set $i = $i + 1
end
printf "\nverified before terminal   ="
set $i = 0
while $i < 10
  printf " %02x", $sm->imu_register_after[$i]
  set $i = $i + 1
end
printf "\nsnapshot/write/verify     = 0x%x / 0x%x / 0x%x (expected 0x7ff / 0x7ff / 0x3ff)\n", $sm->imu_snapshot_ok_mask, $sm->imu_write_ok_mask, $sm->imu_verify_ok_mask
printf "deep-PD value/status/committed = %02x / 0x%x / %u\n", $sm->imu_deep_power_down_value, $sm->imu_deep_power_down_write_status, $sm->imu_terminal_deep_power_down_committed
printf "post-deep-PD read omitted  = %u (expected 1; registers are inaccessible)\n", $sm->imu_post_deep_power_down_read_omitted
printf "I2C state/error            = 0x%x / 0x%x\n", $sm->imu_i2c_state_after, $sm->imu_i2c_error_after

printf "\n  AT25SL128A idle baseline\n"
printf "driver API/init/state/ops/last = %u / %u / %u / %u / %u\n", $sm->flash_driver_api_version, $sm->flash_driver_init_status, $sm->flash_driver_state, $sm->flash_driver_operation_count, $sm->flash_driver_last_status
printf "JEDEC status/ID/match      = 0x%x / %02x %02x %02x / %u\n", $sm->flash_jedec_status, $sm->flash_jedec_id[0], $sm->flash_jedec_id[1], $sm->flash_jedec_id[2], $sm->flash_identity_match
printf "deep-power-down status     = 0x%x\n", $sm->flash_deep_power_down_status
printf "scratch status/address/length = %u / 0x%08x / %u\n", $sm->flash_scratch_status, $sm->flash_scratch_address, $sm->flash_scratch_length
printf "scratch status1 before        = 0x%02x\n", $sm->flash_scratch_status1_before
printf "scratch WRDI/status1       = 0x%x / 0x%02x\n", $sm->flash_scratch_write_disable_status, $sm->flash_scratch_write_disable_status1
printf "erase WREN/cmd/wait/polls     = 0x%x / 0x%x / %u / %u\n", $sm->flash_scratch_erase_write_enable_status, $sm->flash_scratch_erase_status, $sm->flash_scratch_erase_wait_status, $sm->flash_scratch_erase_poll_count
printf "erase status1 after WREN/cmd  = 0x%02x / 0x%02x\n", $sm->flash_scratch_erase_write_enable_status1, $sm->flash_scratch_erase_command_status1
printf "erase retry count/WRDI/WREN/cmd/status1 = %u / 0x%x 0x%02x / 0x%x 0x%02x / 0x%x 0x%02x\n", $sm->flash_scratch_erase_retry_count, $sm->flash_scratch_erase_retry_write_disable_status, $sm->flash_scratch_erase_retry_write_disable_status1, $sm->flash_scratch_erase_retry_write_enable_status, $sm->flash_scratch_erase_retry_write_enable_status1, $sm->flash_scratch_erase_retry_status, $sm->flash_scratch_erase_retry_status1
printf "erase blank read/mismatches   = 0x%x / %u\n", $sm->flash_scratch_erase_blank_read_status, $sm->flash_scratch_erase_blank_mismatch_count
printf "erase blank first16           ="
set $i = 0
while $i < 16
  printf " %02x", $sm->flash_scratch_erase_blank_first16[$i]
  set $i = $i + 1
end
printf "\n"
printf "program WREN/cmd/wait/polls   = 0x%x / 0x%x / %u / %u\n", $sm->flash_scratch_program_write_enable_status, $sm->flash_scratch_program_status, $sm->flash_scratch_program_wait_status, $sm->flash_scratch_program_poll_count
printf "program status1 after WREN    = 0x%02x\n", $sm->flash_scratch_program_write_enable_status1
printf "program read/mismatches       = 0x%x / %u\n", $sm->flash_scratch_program_read_status, $sm->flash_scratch_program_mismatch_count
printf "program first16              ="
set $i = 0
while $i < 16
  printf " %02x", $sm->flash_scratch_program_first16[$i]
  set $i = $i + 1
end
printf "\nDMA program WREN/cmd/xfer wait/polls = 0x%x / 0x%x / %u / %u\n", $sm->flash_scratch_dma_program_write_enable_status, $sm->flash_scratch_dma_program_status, $sm->flash_scratch_dma_program_transfer_wait_status, $sm->flash_scratch_dma_program_transfer_poll_count
printf "DMA program status1 after WREN = 0x%02x\n", $sm->flash_scratch_dma_program_write_enable_status1
printf "DMA program flash wait/polls = %u / %u\n", $sm->flash_scratch_dma_program_flash_wait_status, $sm->flash_scratch_dma_program_flash_poll_count
printf "DMA read cmd/xfer wait/polls = 0x%x / %u / %u\n", $sm->flash_scratch_dma_read_status, $sm->flash_scratch_dma_read_transfer_wait_status, $sm->flash_scratch_dma_read_transfer_poll_count
printf "DMA read mismatches          = %u\n", $sm->flash_scratch_dma_verify_mismatch_count
printf "DMA first16                 ="
set $i = 0
while $i < 16
  printf " %02x", $sm->flash_scratch_dma_first16[$i]
  set $i = $i + 1
end
printf "\nDMA tx state/error           = 0x%x / 0x%x\n", $sm->flash_scratch_dma_tx_state_after, $sm->flash_scratch_dma_tx_error_after
printf "DMA rx state/error           = 0x%x / 0x%x\n", $sm->flash_scratch_dma_rx_state_after, $sm->flash_scratch_dma_rx_error_after
printf "cleanup WREN/cmd/wait/polls = 0x%x / 0x%x / %u / %u\n", $sm->flash_scratch_cleanup_write_enable_status, $sm->flash_scratch_cleanup_erase_status, $sm->flash_scratch_cleanup_wait_status, $sm->flash_scratch_cleanup_poll_count
printf "cleanup status1 after WREN    = 0x%02x\n", $sm->flash_scratch_cleanup_write_enable_status1
printf "cleanup blank read/mismatches = 0x%x / %u\n", $sm->flash_scratch_cleanup_blank_read_status, $sm->flash_scratch_cleanup_blank_mismatch_count
printf "cleanup first16              ="
set $i = 0
while $i < 16
  printf " %02x", $sm->flash_scratch_cleanup_first16[$i]
  set $i = $i + 1
end
printf "\nscratch OSPI state/error    = 0x%x / 0x%x\n", $sm->flash_scratch_ospi_state_after, $sm->flash_scratch_ospi_error_after
printf "OSPI state/error           = 0x%x / 0x%x\n", $sm->flash_ospi_state_after, $sm->flash_ospi_error_after
printf "\n  raw flash block adapter scratch sector\n"
printf "block API/init/ops/last     = %u / %u / %u / %u\n", $sm->flash_block_api_version, $sm->flash_block_init_status, $sm->flash_block_operation_count, $sm->flash_block_last_status
printf "geometry total/erase/page/count = %u / %u / %u / %u\n", $sm->flash_block_geometry_total_size, $sm->flash_block_geometry_erase_size, $sm->flash_block_geometry_page_size, $sm->flash_block_geometry_count
printf "test status/index/address/len = %u / %u / 0x%08x / %u\n", $sm->flash_block_test_status, $sm->flash_block_test_index, $sm->flash_block_test_address, $sm->flash_block_test_length
printf "erase status/polls        = %u / %u\n", $sm->flash_block_erase_status, $sm->flash_block_erase_poll_count
printf "blank read count/status/mismatch = %u / %u / %u\n", $sm->flash_block_blank_read_count, $sm->flash_block_blank_read_status, $sm->flash_block_blank_mismatch_count
printf "blank first16             ="
set $i = 0
while $i < 16
  printf " %02x", $sm->flash_block_blank_first16[$i]
  set $i = $i + 1
end
printf "\nprogram status/pages/last polls = %u / %u / %u\n", $sm->flash_block_program_status, $sm->flash_block_program_page_count, $sm->flash_block_program_last_poll_count
printf "verify read count/status/mismatch = %u / %u / %u\n", $sm->flash_block_verify_read_count, $sm->flash_block_verify_read_status, $sm->flash_block_verify_mismatch_count
printf "verify first16            ="
set $i = 0
while $i < 16
  printf " %02x", $sm->flash_block_verify_first16[$i]
  set $i = $i + 1
end
printf "\ncleanup status/read/mismatch = %u / %u / %u\n", $sm->flash_block_cleanup_status, $sm->flash_block_cleanup_read_status, $sm->flash_block_cleanup_mismatch_count
printf "cleanup first16           ="
set $i = 0
while $i < 16
  printf " %02x", $sm->flash_block_cleanup_first16[$i]
  set $i = $i + 1
end
printf "\nblock OSPI state/error    = 0x%x / 0x%x\n", $sm->flash_block_ospi_state_after, $sm->flash_block_ospi_error_after
printf "\n  storage flash region layout\n"
printf "layout API/status/count    = %u / %u / %u\n", $sm->storage_layout_api_version, $sm->storage_layout_validation_status, $sm->storage_layout_region_count
printf "layout total/erase/end     = %u / %u / 0x%08x\n", $sm->storage_layout_total_size, $sm->storage_layout_erase_size, $sm->storage_layout_end
printf "layout errors align/overlap/range = %u / %u / %u\n", $sm->storage_layout_alignment_errors, $sm->storage_layout_overlap_errors, $sm->storage_layout_range_errors
printf "layout host/protected masks = 0x%x / 0x%x\n", $sm->storage_layout_host_exposed_mask, $sm->storage_layout_protected_mask
printf "layout scratch index/start/len = %u / 0x%08x / %u\n", $sm->storage_layout_scratch_index, $sm->storage_layout_scratch_start, $sm->storage_layout_scratch_length
printf "\n  FileX + LevelX explicit init/provision result\n"
printf "fxlx API/status/region      = %u / %u / %u\n", $sm->storage_fxlx_api_version, $sm->storage_fxlx_status, $sm->storage_fxlx_region_id
printf "region start/len           = 0x%08x / %u\n", $sm->storage_fxlx_region_start, $sm->storage_fxlx_region_length
printf "test start/len             = 0x%08x / %u\n", $sm->storage_fxlx_test_start, $sm->storage_fxlx_test_length
printf "erase/sector/count         = %u / %u / %u\n", $sm->storage_fxlx_erase_block_size, $sm->storage_fxlx_sector_size, $sm->storage_fxlx_sector_count
printf "preformat erase sts/blocks/fail/polls = %u / %u / %u / %u\n", $sm->storage_fxlx_preformat_erase_status, $sm->storage_fxlx_preformat_erase_block_count, $sm->storage_fxlx_preformat_erase_failed_block, $sm->storage_fxlx_preformat_erase_last_poll_count
printf "LX init/open/close         = 0x%x / 0x%x / 0x%x\n", $sm->storage_fxlx_lx_initialize_status, $sm->storage_fxlx_lx_open_status, $sm->storage_fxlx_lx_close_status
printf "FX format/open/flush/close = 0x%x / 0x%x / 0x%x / 0x%x\n", $sm->storage_fxlx_fx_format_status, $sm->storage_fxlx_fx_open_status, $sm->storage_fxlx_fx_flush_status, $sm->storage_fxlx_fx_close_status
printf "file create/open/write     = 0x%x / 0x%x / 0x%x\n", $sm->storage_fxlx_file_create_status, $sm->storage_fxlx_file_open_status, $sm->storage_fxlx_file_write_status
printf "file seek/read/close       = 0x%x / 0x%x / 0x%x\n", $sm->storage_fxlx_file_seek_status, $sm->storage_fxlx_file_read_status, $sm->storage_fxlx_file_close_status
printf "bytes written/read/mismatch = %u / %u / %u\n", $sm->storage_fxlx_bytes_written, $sm->storage_fxlx_bytes_read, $sm->storage_fxlx_verify_mismatch_count
printf "boot read first16          ="
set $i = 0
while $i < 16
  printf " %02x", $sm->storage_fxlx_boot_read_first16[$i]
  set $i = $i + 1
end
printf "\nboot bps/spc/res/fats      = %u / %u / %u / %u\n", $sm->storage_fxlx_boot_bytes_per_sector, $sm->storage_fxlx_boot_sectors_per_cluster, $sm->storage_fxlx_boot_reserved_sectors, $sm->storage_fxlx_boot_number_of_fats
printf "boot root/total/spf/sig    = %u / %u / %u / 0x%04x\n", $sm->storage_fxlx_boot_root_entries, $sm->storage_fxlx_boot_total_sectors, $sm->storage_fxlx_boot_sectors_per_fat, $sm->storage_fxlx_boot_signature
printf "file read first16          ="
set $i = 0
while $i < 16
  printf " %02x", $sm->storage_fxlx_read_first16[$i]
  set $i = $i + 1
end
printf "\nLX driver rd/wr/erase/verify = %u / %u / %u / %u\n", $sm->storage_fxlx_lx_driver_read_count, $sm->storage_fxlx_lx_driver_write_count, $sm->storage_fxlx_lx_driver_erase_count, $sm->storage_fxlx_lx_driver_verify_count
printf "LX driver last status      = 0x%x\n", $sm->storage_fxlx_lx_driver_last_status
printf "FX driver rd/wr/flush/abort = %u / %u / %u / %u\n", $sm->storage_fxlx_fx_driver_read_count, $sm->storage_fxlx_fx_driver_write_count, $sm->storage_fxlx_fx_driver_flush_count, $sm->storage_fxlx_fx_driver_abort_count
printf "FX driver init/uninit/release = %u / %u / %u\n", $sm->storage_fxlx_fx_driver_init_count, $sm->storage_fxlx_fx_driver_uninit_count, $sm->storage_fxlx_fx_driver_release_count
printf "FX driver last req/status  = 0x%x / 0x%x\n", $sm->storage_fxlx_fx_driver_last_request, $sm->storage_fxlx_fx_driver_last_status
printf "flash init count/tick/status = %u / %u / 0x%x\n", $sm->storage_flash_init_request_count, $sm->storage_flash_init_start_tick, $sm->storage_flash_init_last_status
printf "flash init wake/layout/fxlx/dpd = 0x%x / 0x%x / 0x%x / 0x%x\n", $sm->storage_flash_init_wake_status, $sm->storage_flash_init_layout_status, $sm->storage_flash_init_fxlx_status, $sm->storage_flash_init_deep_power_down_status
printf "\n  USB device detached baseline (owned by storage)\n"
printf "VBUS present               = %u (expected 0)\n", $sm->usb_vbus_present
printf "PCD state before/after     = 0x%x / 0x%x\n", $sm->usb_pcd_state_before, $sm->usb_pcd_state_after
printf "clock before/after         = %u / %u\n", $sm->usb_clock_enabled_before, $sm->usb_clock_enabled_after
printf "VDDUSB before/after        = %u / %u\n", $sm->usb_vddusb_enabled_before, $sm->usb_vddusb_enabled_after
printf "deinit attempted/status    = %u / 0x%x\n", $sm->usb_deinit_attempted, $sm->usb_deinit_status
printf "USB parked                 = %u (expected 1)\n", $sm->usb_parked
printf "\n  clock policy\n"
set $cp = &g_ps_hw6_clock_policy_probe
printf "clock api/apply/status     = %u / %u / 0x%x\n", $cp->api_version, $cp->apply_count, $cp->last_status
printf "clock req/current/caps     = %u / %u / 0x%x\n", $cp->selected_profile, $cp->current_profile, $cp->active_capabilities
printf "clock domains req/managed/read = 0x%x / 0x%x / 0x%x\n", $cp->required_domain_mask, $cp->managed_domain_mask, $cp->readback_domain_mask
printf "PLL2 gate en/skip/on/off/status = %u / %u / %u / %u / 0x%x\n", $cp->pll2_autogate_enabled, $cp->pll2_autogate_skip_count, $cp->pll2_domain_on_count, $cp->pll2_domain_off_count, $cp->pll2_domain_last_status
printf "PLL2 output req/read       = 0x%x / 0x%x\n", $cp->pll2_required_output_mask, $cp->pll2_output_enabled_mask
printf "PLL2 ready sai/ospi Hz     = %u / %u / %u\n", $cp->pll2_ready, $cp->sai1_kernel_hz, $cp->ospi_kernel_hz
printf "USB export req/tick/mcu_vbus_at_req = %u / %u / %u\n", $sm->usb_export_request_count, $sm->usb_export_start_tick, $sm->usb_export_vbus_present
printf "USB export power pmic/mcu/agree at req = %u / %u / %u\n", $sm->usb_export_power_pmic_vbus_at_request, $sm->usb_export_power_mcu_vbus_at_request, $sm->usb_export_power_vbus_agree_at_request
printf "USB availability state/event/update/tick = %u / %u / %u / %u\n", $sm->usb_host_availability_state, $sm->usb_host_availability_event, $sm->usb_host_availability_update_count, $sm->usb_host_availability_tick
printf "USB availability ext/data/avail/active = %u / %u / %u / %u\n", $sm->usb_host_external_power_present, $sm->usb_host_data_seen, $sm->usb_host_msc_available, $sm->usb_host_msc_active
printf "USB availability pmic/mcu/agree bridge/cmd = %u / %u / %u / %u / %u\n", $sm->usb_host_pmic_vbus, $sm->usb_host_mcu_vbus, $sm->usb_host_power_agree, $sm->usb_host_bridge_activate_count, $sm->usb_host_bridge_command_count
printf "USB availability states: UNKNOWN=0 NO_POWER=1 EXTERNAL_POWER=2 DATA_HOST=3 MSC_ACTIVE=4\n"
printf "USB availability events: NONE=0 POWER=1 EXPORT_REQ=2 MSC_ACTIVE=3 MEDIA_CMD=4 RECLAIM=5\n"
set $usb_data_host_seen = ((g_usbx_scsi_cbw_count != 0) || (g_ps_storage_msc_bridge_probe.read_count != 0) || (g_ps_storage_msc_bridge_probe.write_count != 0) || (g_ps_storage_msc_bridge_probe.flush_count != 0) || (g_ps_storage_msc_bridge_probe.status_count != 0))
printf "USB data host seen/cbw/media rd/wr/stat = %u / %u / %u / %u / %u\n", $usb_data_host_seen, g_usbx_scsi_cbw_count, g_ps_storage_msc_bridge_probe.read_count, g_ps_storage_msc_bridge_probe.write_count, g_ps_storage_msc_bridge_probe.status_count
printf "USB export policy/dcd/init/start = 0x%x / 0x%x / 0x%x / 0x%x\n", $sm->usb_export_policy_status, $sm->usb_export_dcd_status, $sm->usb_export_pcd_init_status, $sm->usb_export_pcd_start_status
printf "USB export flash/fxlx      = 0x%x / 0x%x\n", $sm->usb_export_flash_wake_status, $sm->usb_export_fxlx_open_status
set $fxlx_msc = &g_ps_storage_filex_levelx_msc_probe
printf "USB export fxlx msc api/status/stage = %u / 0x%x / %u\n", $fxlx_msc->api_version, $fxlx_msc->status, $fxlx_msc->last_stage
printf "USB export fxlx msc validate/lx open/driver = 0x%x / 0x%x / 0x%x\n", $fxlx_msc->validate_status, $fxlx_msc->lx_open_status, $fxlx_msc->lx_driver_last_status
printf "USB export fxlx recovery invalid/count lx/driver = %u / %u / 0x%x / 0x%x\n", $fxlx_msc->invalid_media_detected, $fxlx_msc->recovery_required_count, $fxlx_msc->recovery_lx_open_status, $fxlx_msc->recovery_driver_status
printf "USB export fxlx msc rd/wr/erase/verify = %u / %u / %u / %u\n", $fxlx_msc->lx_driver_read_count, $fxlx_msc->lx_driver_write_count, $fxlx_msc->lx_driver_erase_count, $fxlx_msc->lx_driver_verify_count
printf "USB export fxlx msc block/flash/nor/ospi = 0x%x / %u / 0x%x / %u / 0x%x\n", $fxlx_msc->block_last_status, $fxlx_msc->flash_state, $fxlx_msc->flash_last_status, $fxlx_msc->nor_state, $fxlx_msc->ospi_error_after
printf "USBX pool/init/stage/error/dcd irqguard = 0x%x / 0x%x / %u / 0x%x / 0x%x / %u\n", g_ps_hw6_usbx_byte_pool_create_status, g_ps_hw6_usbx_device_init_status, g_ps_hw6_usbx_init_stage, g_ps_hw6_usbx_init_error_code, g_ps_hw6_usbx_dcd_status, g_ps_hw6_usb_irq_guard_drop_count
printf "RTOS low-power enter/usb-skip = %u / %u\n", g_ps_hw6_rtos_probe.low_power_enter_count, g_ps_hw6_rtos_low_power_usb_skip_count
printf "USB export irq/devconn     = %u -> %u / 0x%x\n", $sm->usb_export_irq_priority_before, $sm->usb_export_irq_priority_after, $sm->usb_export_devconnect_status
printf "USB export pcd/clk/vdd/started = 0x%x / %u / %u / %u\n", $sm->usb_export_pcd_state_after, $sm->usb_export_clock_enabled_after, $sm->usb_export_vddusb_enabled_after, $sm->usb_export_started
printf "USB export PCD cfg bc/vbus = %u / %u (expected 0 / 0)\n", hpcd_USB_OTG_FS.Init.battery_charging_enable, hpcd_USB_OTG_FS.Init.vbus_sensing_enable
printf "USB reclaim req/tick/dirty = %u / %u / %u\n", $sm->usb_reclaim_request_count, $sm->usb_reclaim_start_tick, $sm->usb_reclaim_dirty_seen
printf "USB reclaim devdisc/uxdisc-not-used/stop/deinit = 0x%x / 0x%x / 0x%x / 0x%x\n", $sm->usb_reclaim_devdisconnect_status, $sm->usb_reclaim_disconnect_status, $sm->usb_reclaim_pcd_stop_status, $sm->usb_reclaim_deinit_status
printf "USB reclaim fxlx close     = 0x%x\n", $sm->usb_reclaim_fxlx_close_status
printf "USB reclaim pcd/clk/vdd/parked = 0x%x / %u / %u / %u\n", $sm->usb_reclaim_pcd_state_after, $sm->usb_reclaim_clock_enabled_after, $sm->usb_reclaim_vddusb_enabled_after, $sm->usb_reclaim_parked
printf "USB staging rescan count/tick/dirty/pending = %u / %u / %u / %u\n", $sm->usb_stage_rescan_request_count, $sm->usb_stage_rescan_start_tick, $sm->usb_stage_rescan_dirty_seen, $sm->usb_stage_rescan_pending
printf "USB staging rescan status/package = 0x%x / 0x%x\n", $sm->usb_stage_rescan_status, $sm->usb_stage_rescan_package_scan_status
printf "USB staging rescan status legend: OK=0x0 PACKAGE_UNSUPPORTED=0xb NOT_RUN=0xffffffff\n"
printf "USBX pool/init status      = 0x%x / 0x%x (baseline may be nonzero before MSC bridge)\n", g_ps_hw6_usbx_byte_pool_create_status, g_ps_hw6_usbx_device_init_status
printf "USBX stage statuses       = alloc 0x%x system 0x%x devstack 0x%x class 0x%x appstack 0x%x thread 0x%x\n", g_ps_hw6_usbx_stack_alloc_status, g_ps_hw6_usbx_system_init_status, g_ps_hw6_usbx_device_stack_status, g_ps_hw6_usbx_class_register_status, g_ps_hw6_usbx_thread_stack_status, g_ps_hw6_usbx_thread_create_status
printf "USBX desc lengths         = HS %u FS %u string %u lang %u\n", g_ps_hw6_usbx_framework_hs_length, g_ps_hw6_usbx_framework_fs_length, g_ps_hw6_usbx_string_framework_length, g_ps_hw6_usbx_language_framework_length
printf "USBX MSC cfg/if/lba/blk   = %u / %u / %u / %u\n", g_ps_hw6_usbx_storage_configuration_number, g_ps_hw6_usbx_storage_interface_number, g_ps_hw6_usbx_storage_last_lba, g_ps_hw6_usbx_storage_block_length
printf "MSC bridge api/init      = %u / %u\n", g_ps_storage_msc_bridge_probe.api_version, g_ps_storage_msc_bridge_probe.initialized
printf "MSC bridge policy        = export %u media %u write %u dirty %u\n", g_ps_storage_msc_bridge_probe.export_enabled, g_ps_storage_msc_bridge_probe.media_present, g_ps_storage_msc_bridge_probe.write_enabled, g_ps_storage_msc_bridge_probe.dirty
printf "MSC bridge activate/deactivate = %u / %u\n", g_ps_storage_msc_bridge_probe.activate_count, g_ps_storage_msc_bridge_probe.deactivate_count
printf "MSC bridge denied total/read/write/status = %u / %u / %u / %u\n", g_ps_storage_msc_bridge_probe.denied_count, g_ps_storage_msc_bridge_probe.denied_read_count, g_ps_storage_msc_bridge_probe.denied_write_count, g_ps_storage_msc_bridge_probe.denied_status_count
printf "MSC bridge submit/done/timeout/busy = %u / %u / %u / %u\n", g_ps_storage_msc_bridge_probe.submit_count, g_ps_storage_msc_bridge_probe.completed_count, g_ps_storage_msc_bridge_probe.timeout_count, g_ps_storage_msc_bridge_probe.busy_count
printf "MSC bridge rd/wr/fl/status/fast_status = %u / %u / %u / %u / %u\n", g_ps_storage_msc_bridge_probe.read_count, g_ps_storage_msc_bridge_probe.write_count, g_ps_storage_msc_bridge_probe.flush_count, g_ps_storage_msc_bridge_probe.status_count, g_ps_storage_msc_bridge_probe.fast_status_count
printf "MSC bridge last cmd/lba/blocks = 0x%x / %u / %u\n", g_ps_storage_msc_bridge_probe.last_command, g_ps_storage_msc_bridge_probe.last_lba, g_ps_storage_msc_bridge_probe.last_block_count
printf "MSC bridge tx/owner/ux/media/ps = 0x%x / 0x%x / 0x%x / 0x%x / 0x%x\n", g_ps_storage_msc_bridge_probe.last_tx_status, g_ps_storage_msc_bridge_probe.last_owner_status, g_ps_storage_msc_bridge_probe.last_ux_status, g_ps_storage_msc_bridge_probe.last_media_status, g_ps_storage_msc_bridge_probe.last_ps_status

printf "\n  NINA-B112 AT-controlled STOP baseline\n"
printf "NRST before/released/after = %u / %u / %u\n", $sm->ble_nrst_before, $sm->ble_nrst_released, $sm->ble_nrst_after
printf "NINA_DSR host-DTR before/after = %u / %u\n", $sm->ble_dsr_host_control_before, $sm->ble_dsr_host_control_after
printf "boot RX / commands         = %u bytes / %u\n", $sm->ble_boot_rx_len, $sm->ble_command_count
printf "slots 0..6                 = AT / UPWRMNG(skip) / UPWRMNG(skip) / UBTDM / UBTCM / UBTPM / &D4\n"
printf "command TX statuses        = %x %x %x %x %x %x %x\n", $sm->ble_command_tx_status[0], $sm->ble_command_tx_status[1], $sm->ble_command_tx_status[2], $sm->ble_command_tx_status[3], $sm->ble_command_tx_status[4], $sm->ble_command_tx_status[5], $sm->ble_command_tx_status[6]
printf "command RX lengths         = %u %u %u %u %u %u %u\n", $sm->ble_command_rx_len[0], $sm->ble_command_rx_len[1], $sm->ble_command_rx_len[2], $sm->ble_command_rx_len[3], $sm->ble_command_rx_len[4], $sm->ble_command_rx_len[5], $sm->ble_command_rx_len[6]
printf "required/attempted/skipped = 0x%x / 0x%x / 0x%x (expected 0x79 / 0x79 / 0x6)\n", $sm->ble_command_required_mask, $sm->ble_command_attempted_mask, $sm->ble_command_skipped_mask
printf "OK/error masks             = 0x%x / 0x%x (expected 0x79 / 0)\n", $sm->ble_command_ok_mask, $sm->ble_command_error_mask
printf "UART deinit/state/error    = 0x%x / 0x%x / 0x%x\n", $sm->ble_uart_deinit_status, $sm->ble_uart_state_after, $sm->ble_uart_error_after
printf "fallback reset asserted    = %u (expected 0)\n", $sm->ble_fallback_reset_asserted

printf "\n  bounded transition trace: tick fsm from event to action\n"
set $trace_count = $sm->trace_count
set $trace_index = ($sm->trace_write_index + 128 - $trace_count) % 128
set $i = 0
while $i < $trace_count
  printf "%u %u %u %u %u %x\n", $sm->trace[$trace_index].tick, $sm->trace[$trace_index].state_machine_id, $sm->trace[$trace_index].from_state, $sm->trace[$trace_index].event, $sm->trace[$trace_index].to_state, $sm->trace[$trace_index].action_status
  set $trace_index = ($trace_index + 1) % 128
  set $i = $i + 1
end

printf "\n--- scheduler idle and workflow marker ---\n"
printf "WFI setup/enter/exit/adjust = %u / %u / %u / %u\n", $rtos->low_power_setup_count, $rtos->low_power_enter_count, $rtos->low_power_exit_count, $rtos->low_power_adjust_count
printf "next scheduler timeout ticks = %u\n", $rtos->low_power_next_ticks
printf "PWR_DBG state/toggles/last = %u / %u / %u\n", $rtos->pwr_dbg_state, $rtos->pwr_dbg_toggle_count, $rtos->pwr_dbg_last_toggle_tick
printf "=== END HW6 FW0 BOOT + PERIPHERAL + RTOS + OWNER STATE PROBE ===\n"
