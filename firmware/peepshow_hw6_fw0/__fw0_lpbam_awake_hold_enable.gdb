set pagination off
set $dbgcr_before = *(uint32_t*)0xE0044004
set *(uint32_t*)0xE0044004 = ($dbgcr_before & 0xfffffff9)
set variable g_ps_hw6_power_stop2_display_backend_override = 2
set variable g_ps_hw6_power_stop2_lpbam_awake_hold_enable = 1
printf "--- HW6 awake LPBAM-only hold enabled ---\n"
printf "DBGMCU CR before/after = 0x%x / 0x%x\n", $dbgcr_before, *(uint32_t*)0xE0044004
printf "backend/hold = %u / %u\n", g_ps_hw6_power_stop2_display_backend_override, g_ps_hw6_power_stop2_lpbam_awake_hold_enable
printf "Resume execution. CPU cursor blinking will stop at the next LPBAM handoff, but STOP2 entry will be skipped.\n"
printf "After observing the cursor, halt and source __fw0_stop2_lpbam_prints.gdb.\n"
