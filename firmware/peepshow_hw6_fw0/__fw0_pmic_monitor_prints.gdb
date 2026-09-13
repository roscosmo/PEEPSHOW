set pagination off
printf "--- HW6 PMIC read-group history ---\n"
if g_ps_hw6_owner_probe.power_driver_api_version != 11 || g_ps_hw6_pmic_monitor_probe.api_version != 1
  printf "Probe mismatch or PMIC not initialized. Requires driver API 11 and monitor API 1 with the matching flashed ELF.\n"
else
  set $pm = &g_ps_hw6_pmic_monitor_probe
  printf "sequence/requested/valid = %u / 0x%x / 0x%x\n", $pm->sequence, $pm->requested_mask, $pm->valid_mask
  set $pg = 0
  while $pg < 4
    printf "group %u attempts/successes/valid/status last-attempt/last-success tick last-good reads = %u / %u / %u / 0x%x / %u / %u / %u\n", $pg, $pm->groups[$pg].attempt_count, $pm->groups[$pg].success_count, $pm->groups[$pg].valid, $pm->groups[$pg].last_status, $pm->groups[$pg].last_attempt_tick, $pm->groups[$pg].last_success_tick, $pm->groups[$pg].last_good.read_count
    set $pg = $pg + 1
  end
  printf "Groups 0..3: SAFETY, EVENTS, SOC, CONFIG; last-good reads 7/2/1/14. Healthy full monitor: requested/valid=0xf/0xf, statuses=0.\n"
  printf "No scheduling change: full snapshots still run at the existing cadence. These are acquisition records, not freshness-qualified policy data.\n"
  printf "Failed attempts preserve last-good bytes but clear validity. Attempts alone do not prove successful reads. Kernel ticks do not measure age through STOP2.\n"
end
printf "--- end PMIC read-group history ---\n"
