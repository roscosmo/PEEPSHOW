set pagination off
set $rt = &g_ps_hw6_rtos_probe
set $cp = &g_ps_hw6_clock_policy_probe
printf "--- HW6 STOP2 eligibility dry-run ---\n"
printf "rtos api/count/status/tick = %u / %u / 0x%x / %u\n", $rt->version, $rt->stop2_eligibility_request_count, $rt->stop2_eligibility_last_status, $rt->stop2_eligibility_last_tick
printf "ready/block/pending     = %u / 0x%x / 0x%x\n", $rt->stop2_eligibility_ready, $rt->stop2_eligibility_blocker_mask, $rt->stop2_eligibility_pending_mask
printf "clock caps/dom/read/lpbam = 0x%x / 0x%x / 0x%x / %u\n", $rt->stop2_eligibility_clock_capabilities, $rt->stop2_eligibility_clock_domains, $rt->stop2_eligibility_readback_domains, $rt->stop2_eligibility_lpbam_ready
printf "clock requester active/agg = 0x%x / 0x%x\n", $cp->requester_active_mask, $cp->aggregated_capabilities
printf "req caps P/A/I/D/S/ST/C/UI/RT = 0x%x / 0x%x / 0x%x / 0x%x / 0x%x / 0x%x / 0x%x / 0x%x / 0x%x\n", $cp->requester_capabilities[0], $cp->requester_capabilities[1], $cp->requester_capabilities[2], $cp->requester_capabilities[3], $cp->requester_capabilities[4], $cp->requester_capabilities[5], $cp->requester_capabilities[6], $cp->requester_capabilities[7], $cp->requester_capabilities[8]
printf "power/pmic/battery     = %u / %u / %u\n", $rt->stop2_eligibility_power_state, $rt->stop2_eligibility_pmic_state, $rt->stop2_eligibility_battery_policy
printf "runtime class/exec/life = %u / %u / %u\n", $rt->stop2_eligibility_runtime_class, $rt->stop2_eligibility_runtime_execution, $rt->stop2_eligibility_runtime_lifecycle
printf "boot power/runtime     = %u / %u\n", $rt->boot_power_done, $rt->runtime_complete
printf "blockers: BOOT=0x1 POWER=0x2 PMIC=0x4 BATT=0x8 CLOCK_CAP=0x10 CLOCK_READBACK=0x20\n"
printf "pending: OWNER_QUIESCE=0x1 LPBAM_VALIDATION=0x2 (pending work before real STOP entry, not a current hard blocker)\n"
printf "power: ACTIVE_LP=2 ACTIVE_RT=3 SLEEP_PREP=4 STOP_RESIDENT=5 WAKE_RESUME=6 SHIP_PREP=8\n"
printf "pmic: MONITOR=3 CHARGING=4 DONE=5 LOW=6 CRITICAL=7 SHIP_PENDING=8\n"
printf "battery policy: BOOT_OK=1 OK=2 WARNING=3 CRITICAL=4 BOOT_BLOCK=5 CHARGE_RECOVERY=6 SHIP_REQ=7\n"
