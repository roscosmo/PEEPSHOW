set pagination off
set var g_display_renderer_waiting_test_variant = 3
printf "--- HW6 LPBAM deterministic three-step fallback test enabled ---\n"
printf "variant = %u\n", g_display_renderer_waiting_test_variant
printf "Resume execution. Preferred four-step full-panel compilation must overflow, then one global three-step compile must run in STOP2.\n"
printf "Expected STOP2 visuals: black, white, checker. Wake on checker; awake should continue with vertical stripes before returning to black.\n"
printf "After STOP2 entry, source __fw0_lpbam_guaranteed_fallback_test_prints.gdb.\n"
