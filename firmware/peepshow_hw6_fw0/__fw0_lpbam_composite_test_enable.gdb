set pagination off
set var g_display_renderer_waiting_test_variant = 1
printf "--- HW6 LPBAM composite animation test enabled ---\n"
printf "variant = %u\n", g_display_renderer_waiting_test_variant
printf "Resume execution. Wake once if LPBAM is already active so thDisplay rebuilds the program.\n"
printf "Expected visual: cursor and four-phase bar animate together every 250 ms without row or chunk sweeps.\n"
printf "After STOP2 entry, source __fw0_lpbam_composite_test_prints.gdb.\n"
