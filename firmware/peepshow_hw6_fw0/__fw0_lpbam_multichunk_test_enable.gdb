set pagination off
set var g_display_renderer_waiting_test_variant = 2
printf "--- HW6 LPBAM bounded-transaction full-frame test enabled ---\n"
printf "variant = %u\n", g_display_renderer_waiting_test_variant
printf "Resume execution. Wake once if LPBAM is already active so thDisplay rebuilds the program.\n"
printf "Expected visual: the entire display alternates black/white every 250 ms; each complete frame must change without a row sweep.\n"
printf "After STOP2 entry, source __fw0_lpbam_multichunk_test_prints.gdb.\n"
