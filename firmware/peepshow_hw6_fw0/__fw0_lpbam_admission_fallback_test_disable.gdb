set pagination off
set var g_display_renderer_waiting_test_variant = 0
printf "--- HW6 LPBAM admission fallback test disabled ---\n"
printf "variant = %u\n", g_display_renderer_waiting_test_variant
printf "Resume execution. A normal display render clears the fallback latch and the HOME cursor returns to LPBAM.\n"
