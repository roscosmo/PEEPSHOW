set pagination off
set $rt = &g_ps_hw6_rtos_probe
set $sm = &g_ps_hw6_owner_sm_probe
printf "--- HW6 controlled STOP2 entry scaffold ---\n"
printf "rtos api/control count/status/tick = %u / %u / 0x%x / %u\n", $rt->version, $rt->stop2_control_request_count, $rt->stop2_control_last_status, $rt->stop2_control_last_tick
printf "elig status/block/pending = 0x%x / 0x%x / 0x%x\n", $rt->stop2_control_eligibility_status, $rt->stop2_control_eligibility_blocker_mask, $rt->stop2_control_eligibility_pending_mask
printf "entry attempts/status     = %u / 0x%x\n", $rt->stop2_control_entry_attempt_count, $rt->stop2_control_entry_status
printf "stop2 count before/after  = %u / %u\n", $rt->stop2_control_stop2_count_before, $rt->stop2_control_stop2_count_after
printf "elig ready/block/pending  = %u / 0x%x / 0x%x\n", $rt->stop2_eligibility_ready, $rt->stop2_eligibility_blocker_mask, $rt->stop2_eligibility_pending_mask
printf "owner stop2 count/start/wake/end = %u / %u / %u / %u\n", $sm->stop2_request_count, $sm->stop2_start_tick, $sm->stop2_wake_tick, $sm->stop2_end_tick
printf "owner stop2 status q/enter/clk/recover/last = 0x%x / 0x%x / 0x%x / 0x%x / 0x%x\n", $sm->stop2_quiesce_status, $sm->stop2_enter_status, $sm->stop2_clock_restore_status, $sm->stop2_recover_status, $sm->stop2_last_status
printf "wake expected/start/end IDR = 0x%x / 0x%x / 0x%x\n", $sm->stop2_expected_wake_pin, $sm->stop2_wake_start_idr, $sm->stop2_wake_end_idr
printf "power state/pmic/battery   = %u / %u / %u\n", $rt->stop2_eligibility_power_state, $rt->stop2_eligibility_pmic_state, $rt->stop2_eligibility_battery_policy
printf "runtime class/exec/life   = %u / %u / %u\n", $rt->stop2_eligibility_runtime_class, $rt->stop2_eligibility_runtime_execution, $rt->stop2_eligibility_runtime_lifecycle
printf "blockers: BOOT=0x1 POWER=0x2 PMIC=0x4 BATT=0x8 CLOCK_CAP=0x10 CLOCK_READBACK=0x20\n"
printf "status: HAL_OK=0x0 HAL_ERROR=0x1 NOT_RUN=0xffffffff\n"