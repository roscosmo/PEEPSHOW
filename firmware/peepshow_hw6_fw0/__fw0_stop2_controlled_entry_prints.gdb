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
printf "display lpbam ready/page/render/status = %u / %u / %u / 0x%x\n", $ow->display_lpbam_ready, $ow->display_lpbam_ready_page, $ow->display_lpbam_ready_render_count, $ow->display_lpbam_status
printf "display lpbam prep/clear/reason = %u / %u / %u\n", $ow->display_lpbam_prepare_count, $ow->display_lpbam_clear_count, $ow->display_lpbam_clear_reason
printf "owner stop2 count/start/wake/end = %u / %u / %u / %u\n", $sm->stop2_request_count, $sm->stop2_start_tick, $sm->stop2_wake_tick, $sm->stop2_end_tick
printf "owner stop2 status q/enter/clk/recover/last = 0x%x / 0x%x / 0x%x / 0x%x / 0x%x\n", $sm->stop2_quiesce_status, $sm->stop2_enter_status, $sm->stop2_clock_restore_status, $sm->stop2_recover_status, $sm->stop2_last_status
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
printf "wake masks: START=0x1 BUTTON=0x2 JOY=0x4 SENSOR=0x8 PMIC=0x10 RTC=0x20 USB=0x40 FAULT=0x80 UNKNOWN=0x80000000\n"
printf "wake causes: NONE=0 START=1 BUTTON=2 JOY=3 SENSOR=4 PMIC=5 RTC=6 USB=7 FAULT=8 UNKNOWN=9\n"
printf "status: HAL_OK=0x0 HAL_ERROR=0x1 NOT_RUN=0xffffffff\n"
