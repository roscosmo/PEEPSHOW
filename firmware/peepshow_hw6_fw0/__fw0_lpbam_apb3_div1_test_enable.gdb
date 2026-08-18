set pagination off
set $dbgcr_before = *(uint32_t*)0xE0044004
set *(uint32_t*)0xE0044004 = ($dbgcr_before & 0xfffffff9)
set variable g_ps_hw6_power_stop2_display_backend_override = 2
set variable g_ps_hw6_power_stop2_lpbam_awake_hold_enable = 0
set variable g_ps_hw6_power_stop2_pre_wfi_hold_enable = 0
set variable g_ps_hw6_power_stop2_srdrun_test_enable = 0
set variable g_ps_hw6_power_stop2_post_wfi_break_enable = 0
set variable g_ps_hw6_power_stop2_spi_autotrigger_test_enable = 0
set variable g_ps_hw6_power_stop2_apb3_div1_test_enable = 1
printf "--- HW6 LPBAM one-shot APB3 /1 STOP2 test armed ---\n"
printf "DBGMCU CR before/after = 0x%x / 0x%x\n", $dbgcr_before, *(uint32_t*)0xE0044004
printf "backend/awake/pre-WFI/SRDRUN/APB3-test = %u / %u / %u / %u / %u\n", g_ps_hw6_power_stop2_display_backend_override, g_ps_hw6_power_stop2_lpbam_awake_hold_enable, g_ps_hw6_power_stop2_pre_wfi_hold_enable, g_ps_hw6_power_stop2_srdrun_test_enable, g_ps_hw6_power_stop2_apb3_div1_test_enable
printf "Resume execution. The next eligible STOP2 entry will set APB3 /1 before LPBAM is armed, then restore the prior divider after wake.\n"
printf "Observe whether the whole cursor blinks together, then wake once and source __fw0_stop2_lpbam_prints.gdb.\n"
