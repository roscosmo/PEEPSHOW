set pagination off
set var g_ps_package_source_override = 2
set var g_display_renderer_waiting_test_variant = 0
set var g_ps_scene_runtime_waiting_demo_enable = 0
set var g_ps_hw6_runtime_reactive_stub_request = 1
printf "--- HW6 STATE sampled-SFX proof requested ---\n"
printf "Continue until the busy center STATE appears. Press R exactly once.\n"
printf "Expected: one short select sound plays and the sparse two-sprite state appears.\n"
printf "Then touch nothing for two seconds. The sound must end, STOP2 must resume, and both sprite animations must continue.\n"
printf "Halt once after that idle period and source __fw0_state_scene_sfx_test_prints.gdb.\n"
