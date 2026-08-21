set pagination off
set var g_display_renderer_waiting_test_variant = 3
printf "--- HW6 LPBAM admission fallback test enabled ---\n"
printf "variant = %u\n", g_display_renderer_waiting_test_variant
printf "Resume execution. The four-step full-panel program requires 24 fixed transactions and exceeds the permanent 18-transaction budget.\n"
printf "Expected: LPBAM admission rejects it, HELD_FRAME is selected, and STOP2 still occurs without autonomous animation.\n"
printf "After STOP2 entry, source __fw0_lpbam_admission_fallback_test_prints.gdb.\n"
