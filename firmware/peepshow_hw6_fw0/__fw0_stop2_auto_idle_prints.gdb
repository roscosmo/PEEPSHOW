set pagination off
set $rt = &g_ps_hw6_rtos_probe
set $sm = &g_ps_hw6_owner_sm_probe
set $ow = &g_ps_hw6_owner_probe
set $ui = &g_ps_ui_router_probe
set $btn = &g_ps_input_buttons_probe
set $clk = &g_ps_hw6_clock_policy_probe
printf "--- HW6 automatic STOP2 idle admission scaffold ---\n"
printf "rtos api/runtime/boot/idlepark = %u / %u / %u / %u\n", $rt->version, $rt->runtime_complete, $rt->boot_power_done, $rt->boot_idle_peripheral_park_done
printf "boot idle park count/status/start/end = %u / 0x%x / %u / %u\n", $rt->boot_idle_peripheral_park_request_count, $rt->boot_idle_peripheral_park_last_status, $rt->boot_idle_peripheral_park_start_tick, $rt->boot_idle_peripheral_park_end_tick
printf "boot idle park BLE send/wait/ack = 0x%x / 0x%x / 0x%x\n", $rt->boot_idle_peripheral_park_ble_send_status, $rt->boot_idle_peripheral_park_ble_wait_status, $rt->boot_idle_peripheral_park_ble_ack_flags
printf "boot idle park IMU send/wait/ack = 0x%x / 0x%x / 0x%x\n", $rt->boot_idle_peripheral_park_imu_send_status, $rt->boot_idle_peripheral_park_imu_wait_status, $rt->boot_idle_peripheral_park_imu_ack_flags
printf "auto enabled/check/entry/skip = %u / %u / %u / %u\n", $rt->stop2_auto_enabled, $rt->stop2_auto_check_count, $rt->stop2_auto_entry_count, $rt->stop2_auto_skip_count
printf "auto force enable/count/tick = %u / %u / %u\n", $rt->stop2_auto_debug_force_enable, $rt->stop2_auto_debug_force_entry_count, $rt->stop2_auto_debug_force_entry_tick
printf "auto status/tick/next = 0x%x / %u / %u\n", $rt->stop2_auto_last_status, $rt->stop2_auto_last_tick, $rt->stop2_auto_next_tick
printf "auto idle start/live/required ticks = %u / %u / %u\n", $rt->stop2_auto_idle_start_tick, $rt->stop2_auto_idle_ticks, $rt->stop2_auto_required_idle_ticks
printf "auto block/pending/queue = 0x%x / 0x%x / 0x%x\n", $rt->stop2_auto_blocker_mask, $rt->stop2_auto_pending_mask, $rt->stop2_auto_queue_pending_mask
printf "auto elig/entry status = 0x%x / 0x%x\n", $rt->stop2_auto_eligibility_status, $rt->stop2_auto_entry_status
printf "elig ready/block/pending = %u / 0x%x / 0x%x\n", $rt->stop2_eligibility_ready, $rt->stop2_eligibility_blocker_mask, $rt->stop2_eligibility_pending_mask
printf "stop2 idle peripheral ready = %u\n", $rt->stop2_eligibility_idle_peripheral_park_ready
printf "power state/pmic/battery = %u / %u / %u\n", $rt->stop2_eligibility_power_state, $rt->stop2_eligibility_pmic_state, $rt->stop2_eligibility_battery_policy
printf "runtime class/exec/life/caps = %u / %u / %u / 0x%x\n", $rt->runtime_current_class, $rt->runtime_execution, $rt->runtime_lifecycle, $rt->runtime_active_capabilities
printf "ui page/nav/modal/pkg/shutdown/pending = %u / %u / %u / %u / %u / %u\n", $ui->current_page, $ui->nav_state, $ui->modal_state, $ui->package_state, $ui->shutdown_state, $ui->pending_action
printf "display ui req/render/page/status = %u / %u / %u / 0x%x\n", $ow->display_ui_request_count, $ow->display_ui_render_count, $ow->display_ui_page, $ow->display_ui_status
printf "display complete/success = %u / %u\n", $ow->display_complete, $ow->display_success
printf "display backend req/selected/status/held = %u / %u / 0x%x / %u\n", $rt->stop2_display_wait_backend_requested, $rt->stop2_display_wait_backend_selected, $rt->stop2_display_wait_backend_status, $rt->stop2_display_wait_backend_held_ready
printf "display lpbam ready/page/render/status = %u / %u / %u / 0x%x\n", $ow->display_lpbam_ready, $ow->display_lpbam_ready_page, $ow->display_lpbam_ready_render_count, $ow->display_lpbam_status
printf "display lpbam prep/clear/reason = %u / %u / %u\n", $ow->display_lpbam_prepare_count, $ow->display_lpbam_clear_count, $ow->display_lpbam_clear_reason
printf "display lpbam prep status/abort/status = 0x%x / %u / 0x%x\n", $ow->display_lpbam_prepare_status, $ow->display_lpbam_abort_count, $ow->display_lpbam_abort_status
printf "rtos lpbam prep count/send/wait/ack/owner/ready = %u / 0x%x / 0x%x / 0x%x / 0x%x / %u\n", $rt->stop2_lpbam_prepare_request_count, $rt->stop2_lpbam_prepare_send_status, $rt->stop2_lpbam_prepare_wait_status, $rt->stop2_lpbam_prepare_ack_flags, $rt->stop2_lpbam_prepare_owner_status, $rt->stop2_lpbam_prepare_ready_after
printf "rtos lpbam abort count/send/wait/ack/owner = %u / 0x%x / 0x%x / 0x%x / 0x%x\n", $rt->stop2_lpbam_abort_request_count, $rt->stop2_lpbam_abort_send_status, $rt->stop2_lpbam_abort_wait_status, $rt->stop2_lpbam_abort_ack_flags, $rt->stop2_lpbam_abort_owner_status
printf "input pending/start/logical/policy = 0x%x / %u / %u / %u\n", $btn->pending_mask, $btn->start_active, $btn->logical_event_count, $rt->input_policy_event_count
printf "clock caps/dom/readback/lpbam = 0x%x / 0x%x / 0x%x / %u\n", $clk->stop2_blocker_capabilities, $clk->stop2_blocker_domain_mask, $clk->readback_domain_mask, $clk->lpbam_stop2_ready
printf "owner stop2 count/start/wake/end = %u / %u / %u / %u\n", $sm->stop2_request_count, $sm->stop2_start_tick, $sm->stop2_wake_tick, $sm->stop2_end_tick
printf "wake expected/start/end IDR = 0x%x / 0x%x / 0x%x\n", $sm->stop2_expected_wake_pin, $sm->stop2_wake_start_idr, $sm->stop2_wake_end_idr
printf "wake class count/tick/source/primary = %u / %u / 0x%x / %u\n", $rt->stop2_wake_classify_count, $rt->stop2_wake_classify_tick, $rt->stop2_wake_source_mask, $rt->stop2_wake_primary_cause
printf "wake counts start/button/joy/sensor/pmic/unknown = %u / %u / %u / %u / %u / %u\n", $rt->stop2_wake_start_count, $rt->stop2_wake_button_count, $rt->stop2_wake_joystick_count, $rt->stop2_wake_sensor_count, $rt->stop2_wake_pmic_count, $rt->stop2_wake_unknown_count
printf "wake exti R/F/IMR = 0x%x / 0x%x / 0x%x\n", $rt->stop2_wake_exti_rising, $rt->stop2_wake_exti_falling, $rt->stop2_wake_exti_imr
printf "wake GPIOA before/after = 0x%x / 0x%x\n", $rt->stop2_wake_gpioa_before_idr, $rt->stop2_wake_gpioa_after_idr
printf "wake GPIOB before/after = 0x%x / 0x%x\n", $rt->stop2_wake_gpiob_before_idr, $rt->stop2_wake_gpiob_after_idr
printf "wake button edges before/after/last = %u / %u / %u / %u / %u\n", $rt->stop2_wake_button_edges_before, $rt->stop2_wake_button_edges_after, $btn->last_button_id, $btn->last_event, $btn->last_level
printf "storage power rel/jedec/match/dpd = 0x%x / 0x%x / %u / 0x%x\n", $sm->flash_power_release_status, $sm->flash_power_jedec_status, $sm->flash_power_identity_match, $sm->flash_power_deep_power_down_status
printf "storage OSPI park/restore count = %u / %u\n", $sm->storage_ospi_park_count, $sm->storage_ospi_restore_count
printf "storage OSPI park ENR1/2 after = 0x%x / 0x%x\n", $sm->storage_ospi_park_ahb2enr1_after, $sm->storage_ospi_park_ahb2enr2_after
printf "storage OSPI park SMEN1/2 after = 0x%x / 0x%x\n", $sm->storage_ospi_park_ahb2smenr1_after, $sm->storage_ospi_park_ahb2smenr2_after
printf "storage OSPI restore ENR1/2 after = 0x%x / 0x%x\n", $sm->storage_ospi_restore_ahb2enr1_after, $sm->storage_ospi_restore_ahb2enr2_after
printf "storage OSPI restore SMEN1/2 after = 0x%x / 0x%x\n", $sm->storage_ospi_restore_ahb2smenr1_after, $sm->storage_ospi_restore_ahb2smenr2_after
printf "blockers: BOOT=0x1 POWER=0x2 PMIC=0x4 BATT=0x8 CLOCK_CAP=0x10 CLOCK_READBACK=0x20 DISABLED=0x40 RUNTIME=0x80 UI=0x100 DISPLAY=0x200 STORAGE_USB=0x400 INPUT=0x800 QUEUE=0x1000 LPBAM=0x2000 IDLE_PARK=0x4000\n"
printf "pending: OWNER_QUIESCE=0x1 LPBAM=0x2 IDLE_WINDOW=0x4\n"
printf "display backends: NONE=0 HELD_FRAME=1 LPBAM=2\n"
printf "wake masks: START=0x1 BUTTON=0x2 JOY=0x4 SENSOR=0x8 PMIC=0x10 RTC=0x20 USB=0x40 FAULT=0x80 UNKNOWN=0x80000000\n"
printf "wake causes: NONE=0 START=1 BUTTON=2 JOY=3 SENSOR=4 PMIC=5 RTC=6 USB=7 FAULT=8 UNKNOWN=9\n"
printf "status: HAL_OK=0x0 HAL_ERROR=0x1 UNAVAILABLE=0xfffffffe NOT_RUN=0xffffffff\n"
