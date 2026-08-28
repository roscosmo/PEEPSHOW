set pagination off
set var g_ps_package_source_override = 2
set var g_display_renderer_waiting_test_variant = 0
set var g_ps_scene_runtime_waiting_demo_enable = 0
set var g_ps_hw6_runtime_reactive_stub_request = 1
printf "--- HW6 STATE_SCENE proof requested ---\n"
printf "No exact timing or ordered script is required.\n"
printf "On the source scene, L/R and joystick L/R move through the busy/sparse visual states; R from center still plays the short select SFX. The cursor and marker should continue animating in STOP2.\n"
printf "Press A from the source scene to enter INPUT PROOF. The screen has labeled rows: BTN, JOY, and DIAG.\n"
printf "On INPUT PROOF, use controls naturally: B press/release, START short press, hold L, hold R, joystick left/right, hold joystick up/down, and diagonals. The marker should move to the matching labeled position. A returns to the source scene.\n"
printf "When done, halt and source __fw0_state_scene_demo_prints.gdb.\n"
