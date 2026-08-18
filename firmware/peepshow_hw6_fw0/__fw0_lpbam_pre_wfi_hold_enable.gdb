set pagination off
set $dbgcr_before = *(uint32_t*)0xE0044004
set *(uint32_t*)0xE0044004 = ($dbgcr_before & 0xfffffff9)
set variable g_ps_hw6_power_stop2_display_backend_override = 2
set variable g_ps_hw6_power_stop2_lpbam_awake_hold_enable = 0
set variable g_ps_hw6_power_stop2_pre_wfi_hold_enable = 1
printf "--- HW6 LPBAM pre-WFI one-shot hold armed ---\n"
printf "DBGMCU CR before/after = 0x%x / 0x%x\n", $dbgcr_before, *(uint32_t*)0xE0044004
printf "backend/awake-hold/pre-WFI-hold = %u / %u / %u\n", g_ps_hw6_power_stop2_display_backend_override, g_ps_hw6_power_stop2_lpbam_awake_hold_enable, g_ps_hw6_power_stop2_pre_wfi_hold_enable
printf "Resume execution. The core will break after quiesce and GPIO parking, immediately before STOP2 WFI.\n"
printf "Observe the cursor while stopped, then source __fw0_stop2_lpbam_prints.gdb.\n"
