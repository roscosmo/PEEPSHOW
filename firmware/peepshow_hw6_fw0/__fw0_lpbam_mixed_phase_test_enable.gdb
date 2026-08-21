set pagination off
set var g_display_renderer_waiting_test_variant = 5
printf "--- HW6 LPBAM mixed 2-phase/3-phase test enabled ---\n"
printf "variant = %u\n", g_display_renderer_waiting_test_variant
printf "Resume execution. The next STOP2 program should use the preferred six-step combined timeline.\n"
printf "After STOP2 entry, source __fw0_lpbam_mixed_phase_test_prints.gdb.\n"
