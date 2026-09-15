set pagination off
set input-radix 10
printf "--- HW6 battery fault-wait bench result ---\n"
if g_ps_hw6_battery_fault_test_probe.api_version != 2 || g_ps_hw6_battery_fault_wait_probe.api_version != 1 || g_ps_hw6_battery_shutdown_probe.api_version != 2
  printf "Probe mismatch: use the matching normal Debug ELF.\n"
else
  set $ft = &g_ps_hw6_battery_fault_test_probe
  set $fw = &g_ps_hw6_battery_fault_wait_probe
  set $bs = &g_ps_hw6_battery_shutdown_probe
  printf "request/accepted/test active/status = %u / %u / %u / 0x%x\n", $ft->request, $ft->accepted, $ft->active, $ft->status
  printf "injected failures / last REAL preparation status = %u / 0x%x\n", $ft->injections, $ft->last_real_status
  printf "mode / owner pending/refusals/REAL status = %u / %u / %u / 0x%x\n", $ft->mode, $ft->owner_pending, $ft->owner_refusals, $ft->owner_real_status
  if $ft->mode == 2
    printf "owner refusal tick / next retry seen/tick = %u / %u / %u\n", $ft->owner_refusal_tick, $ft->owner_retry_seen, $ft->owner_retry_tick
    if $ft->owner_retry_seen != 0
      printf "refusal-to-retry ticks / intervening WFI / successful due reads = %u / %u / %u\n", (unsigned int)($ft->owner_retry_tick - $ft->owner_refusal_tick), (unsigned int)($ft->owner_wfi_at_retry - $ft->owner_wfi_at_refusal), (unsigned int)($ft->owner_reads_at_retry - $ft->owner_reads_at_refusal)
    end
    printf "Mode 2 expects ONE SENSOR refusal after real status=0. Retry spacing >=6000 ticks at 100 Hz, no intervening WFI, and successful battery reads during backoff.\n"
    printf "Source __fw0_battery_quiesce_timing_prints.gdb: FIRST FAILED barrier requires SENSOR ACK success/action=1, failure mask bit 0x10; later full quiesce must succeed before sleep.\n"
  end
  printf "preparation attempts/prepared/exhausted/status = %u / %u / %u / 0x%x\n", $bs->attempts, $bs->prepared, $bs->exhausted, $bs->last_status
  printf "fault active/sleep attempts/WFI returns/status/force read = %u / %u / %u / 0x%x / %u\n", $fw->active, $fw->attempts, $fw->wfi_returns, $fw->last_status, $fw->force_read
  printf "test WFI returns / battery RTC expiry / successful due-read deltas = %u / %u / %u\n", $fw->wfi_returns - $ft->wfi_baseline, g_ps_hw6_battery_wake_probe.rtc_expiries - $ft->expiry_baseline, g_ps_hw6_battery_wake_probe.due_successes - $ft->due_success_baseline
  printf "inspection active/count/deadline ticks / window ms / battery interval ms = %u / %u / %u / %u / %u\n", $ft->inspect_active, $ft->inspect_count, $ft->inspect_until, $ft->inspect_ms, $ft->wake_ms
  printf "battery mV/valid/status / recovery status = %u / %u / 0x%x / 0x%x\n", g_ps_hw6_owner_sm_probe.battery_policy_vbat_mv, g_ps_hw6_owner_sm_probe.battery_policy_fuel_ok, g_ps_hw6_owner_sm_probe.battery_policy_last_snapshot_status, $fw->recovery_status
  printf "actual shipment calls delta / pending = %u / %u\n", g_ps_hw6_owner_probe.power_software_ship_request_count - $ft->ship_baseline, $bs->ship_pending
  printf "quiesce required/ACK/success/failure masks = 0x%x / 0x%x / 0x%x / 0x%x\n", g_ps_hw6_owner_sm_probe.power_quiesce_required_mask, g_ps_hw6_owner_sm_probe.power_quiesce_ack_ok_mask, g_ps_hw6_owner_sm_probe.power_quiesce_success_mask, g_ps_hw6_owner_sm_probe.power_quiesce_failure_mask
  printf "STOP2 quiesce/clock prepare/clock restore/status = 0x%x / 0x%x / 0x%x / 0x%x\n", g_ps_hw6_owner_sm_probe.stop2_quiesce_status, g_ps_hw6_owner_sm_probe.stop2_clock_prepare_status, g_ps_hw6_owner_sm_probe.stop2_clock_restore_status, g_ps_hw6_owner_sm_probe.stop2_last_status
  printf "display LPBAM active / runtime lifecycle / power state = %u / %u / %u\n", g_ps_hw6_owner_probe.display_lpbam_active, g_ps_hw6_rtos_probe.runtime_lifecycle, g_ps_hw6_owner_sm_probe.current_state[0]
  printf "Expected before voltage recovery: injections=3, REAL status=0, attempts/exhausted=3/1, injected preparation status=1, fault active=1, shipment delta=0.\n"
  printf "Require positive WFI/RTC-expiry/due-success deltas AND measured low current before tapping A. A counter alone is not low-power proof.\n"
  printf "The first button wake opens inspection once. Resume to finish its running-time countdown; later buttons cannot extend or reopen it. Halting freezes this countdown.\n"
  printf "After inspection, raise the isolated supply to 3.8 V and resume. A valid reading and owner recovery should clear fault/test active; retained injection counts remain.\n"
  printf "This simulates preparation failure after working owners. It does not prove a broken owner can park or that a failed PMIC shipment write removes power.\n"
end
