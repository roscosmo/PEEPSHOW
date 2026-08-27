set pagination off
set var g_ps_package_source_override = 2
set var g_display_renderer_waiting_test_variant = 0
set var g_ps_scene_runtime_waiting_demo_enable = 0
set var g_ps_hw6_runtime_interaction_test_continuous = 1
set var g_ps_hw6_runtime_reactive_stub_request = 1
printf "--- HW6 deterministic manual interaction test enabled ---\n"
printf "Continue from HOME and wait for the STATE scene. Automatic inactivity is disabled for this test.\n"
printf "Whenever ready, hold START until PRESS START appears, then release. This proves manual INACTIVE without a timing race.\n"
printf "Whenever ready, tap START to run the eye activation. The scene then remains ACTIVE until another long START hold.\n"
printf "After confirming STOP2 once, halt and source __fw0_interaction_policy_prints.gdb, __fw0_input_stop2_reliability_prints.gdb, and __fw0_stop2_power_prints.gdb.\n"
