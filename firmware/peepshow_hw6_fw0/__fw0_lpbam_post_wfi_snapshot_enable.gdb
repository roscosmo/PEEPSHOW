set pagination off
set $dbgcr_before = *(uint32_t*)0xE0044004
set *(uint32_t*)0xE0044004 = ($dbgcr_before & 0xfffffff9)
set variable g_ps_hw6_power_stop2_display_backend_override = 2
set variable g_ps_hw6_power_stop2_lpbam_awake_hold_enable = 0
set variable g_ps_hw6_power_stop2_pre_wfi_hold_enable = 0
set variable g_ps_hw6_power_stop2_srdrun_test_enable = 0
set variable g_ps_hw6_power_stop2_apb3_div1_test_enable = 0
set variable g_ps_hw6_power_stop2_post_wfi_break_enable = 1
printf "--- HW6 LPBAM post-WFI snapshot armed ---\n"
printf "DBGMCU CR before/after = 0x%x / 0x%x\n", $dbgcr_before, *(uint32_t*)0xE0044004
printf "backend/post-WFI-break = %u / %u\n", g_ps_hw6_power_stop2_display_backend_override, g_ps_hw6_power_stop2_post_wfi_break_enable
printf "Resume and observe the STOP2 sweep. Press one wake button; firmware will snapshot and break before LPBAM cleanup.\n"
printf "At the automatic breakpoint, source __fw0_stop2_lpbam_prints.gdb.\n"
