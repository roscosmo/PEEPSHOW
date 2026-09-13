set pagination off
printf "--- HW6 quiet battery wake ---\n"
if g_ps_hw6_battery_wake_probe.api_version != 1
  printf "Probe mismatch: use the matching flashed ELF.\n"
else
  set $bw = &g_ps_hw6_battery_wake_probe
  printf "configured/pending/tracking normal/warning/retry ticks = %u / %u / %u / %u / %u / %u\n", $bw->configured, $bw->pending, $bw->tracking, $bw->normal_ticks, $bw->warning_ticks, $bw->retry_ticks
  printf "attempts/successes/failures/last valid = %u / %u / %u / %u\n", $bw->attempts, $bw->successes, $bw->failures, $bw->last_sample_ok
  printf "RTC selections/expiries due wakes/checks/successes clock failures = %u / %u / %u / %u / %u / %u\n", $bw->rtc_selections, $bw->rtc_expiries, $bw->due_wakes, $bw->due_checks, $bw->due_successes, $bw->clock_failures
  printf "test arms/request ms / expiry delta / due-success delta = %u / %u / %u / %u\n", $bw->test_arms, g_ps_hw6_battery_wake_test_request_ms, $bw->rtc_expiries - $bw->test_rtc_expiries_before, $bw->due_successes - $bw->test_due_successes_before
  printf "deadline / last sleep remaining / elapsed ticks = %u / %u / %u\n", $bw->deadline_tick, $bw->sleep_remaining_ticks, $bw->last_elapsed_ticks
  printf "battery VBAT/status/policy critical-ship/boot-ship enabled = %u / 0x%x / %u / %u / %u\n", g_ps_hw6_owner_sm_probe.battery_policy_vbat_mv, g_ps_hw6_owner_sm_probe.battery_policy_last_snapshot_status, g_ps_hw6_owner_sm_probe.battery_policy_state, g_ps_hw6_owner_sm_probe.battery_policy_critical_ship_enabled, g_ps_hw6_owner_sm_probe.battery_policy_boot_ship_enabled
  printf "STOP2 entries / RTC wake classifications / shared RTC status = %u / %u / 0x%x\n", g_ps_hw6_rtos_probe.stop2_auto_entry_count, g_ps_hw6_rtos_probe.stop2_wake_rtc_count, g_ps_hw6_rtos_probe.runtime_interaction_rtc_arm_status
  printf "Test: test arms>0, request=0, expiry and due-success deltas>=1, failures/clock failures=0, last valid=1, snapshot status=0.\n"
  printf "RTC selection alone is not a wake or read. Require expiry plus a successful due check, unchanged visuals/input, and return to low-power residency.\n"
  printf "Battery deadlines use RTC elapsed time through STOP2; the raw deadline tick is rebased after each sleep attempt.\n"
  printf "Automatic low-battery shipment remains gated OFF pending controlled low-voltage and restart testing. This is not complete discharge-protection proof.\n"
end
