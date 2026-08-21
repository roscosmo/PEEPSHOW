set pagination off
set var g_display_renderer_waiting_test_variant = 6
printf "--- HW6 LPBAM admission fallback test enabled ---\n"
printf "variant = %u\n", g_display_renderer_waiting_test_variant
printf "Resume execution. The invalid five-phase element must be rejected without a guaranteed retry.\n"
printf "Expected: HELD_FRAME is selected and STOP2 still occurs without autonomous animation.\n"
printf "After STOP2 entry, source __fw0_lpbam_admission_fallback_test_prints.gdb.\n"
