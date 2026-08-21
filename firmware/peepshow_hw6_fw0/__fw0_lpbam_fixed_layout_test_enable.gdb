set pagination off
set var g_display_renderer_waiting_test_variant = 4
printf "--- HW6 LPBAM permanent SRAM4 layout test enabled ---\n"
printf "variant = %u\n", g_display_renderer_waiting_test_variant
printf "Resume execution. Wake once if LPBAM is already active so thDisplay compiles the test program.\n"
printf "Expected visual: full-screen black, white, and checker states change coherently every 250 ms through six equal 28-row transport bands without a row sweep.\n"
printf "This diagnostic is intended to prove one STOP2 playback cycle; wake after observing it, then source __fw0_lpbam_fixed_layout_test_prints.gdb.\n"
