set var g_ps_hw6_power_stop2_display_backend_override = 2
set var g_ps_hw6_power_stop2_lpbam_awake_hold_enable = 0
set var g_ps_hw6_power_stop2_pre_wfi_hold_enable = 0
set var g_ps_hw6_power_stop2_srdrun_test_enable = 0
set var g_ps_hw6_power_stop2_apb3_div1_test_enable = 0
set var g_ps_hw6_power_stop2_post_wfi_break_enable = 0
set var g_ps_hw6_power_stop2_spi_autotrigger_test_enable = 0
printf "LPBAM STOP2 display backend override set to %u\n", g_ps_hw6_power_stop2_display_backend_override
printf "All LPBAM diagnostic overrides cleared; APB3 remains on the normal /8 policy.\n"
