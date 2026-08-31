set pagination off
set var g_ps_package_source_override = 2
set var g_display_renderer_waiting_test_variant = 0
set var g_ps_scene_runtime_waiting_demo_enable = 0
set var g_ps_hw6_runtime_reactive_stub_request = 1
printf "--- HW6 STATE sampled-SFX proof requested ---\n"
printf "Continue until the busy center STATE appears and press R once. Then leave it untouched long enough to enter STOP2 and press R once more; that second press wakes it and requests the post-STOP sound.\n"
printf "Expected: both sounds use the normal PLL2 SAI clock, complete cleanly, and sound alike.\n"
printf "Both sprite animations must continue while idle. No exact timing is required.\n"
printf "When finished, halt once and source __fw0_state_scene_sfx_test_prints.gdb.\n"
