set pagination off
set var g_display_renderer_waiting_test_variant = 0
set var g_ps_scene_runtime_waiting_demo_enable = 0
set var g_ps_hw6_runtime_reactive_stub_request = 1
printf "--- HW6 STATE_SCENE proof requested ---\n"
printf "Resume until STATE SCENE appears. L/R must move one state per press, B must return HOME, and the cursor plus marker must continue awake and in STOP2.\n"
printf "After several L/R presses and one STOP2 cycle, source __fw0_state_scene_demo_prints.gdb.\n"
