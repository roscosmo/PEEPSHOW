set pagination off
set var g_ps_package_source_override = 2
set var g_display_renderer_waiting_test_variant = 0
set var g_ps_scene_runtime_waiting_demo_enable = 0
set var g_ps_hw6_runtime_reactive_stub_request = 1
printf "--- HW6 STATE_SCENE proof requested ---\n"
printf "Continue until the source scene appears. Its center and left states show the cursor, right-edge marker, filled rectangle, diagonal line, outline rectangle, circle, and ellipse. The right state intentionally keeps only the two sprites. L/R must move one state per press.\n"
printf "Press A once: the target scene must appear with a center cursor plus left-edge marker. Press A again: the source scene must return at its entry state.\n"
printf "Let both scenes enter STOP2. Static primitives must remain composed and both sprite animations must continue. B must return HOME.\n"
printf "After at least two A presses, several L/R presses, and one STOP2 cycle in each scene, halt and source __fw0_state_scene_demo_prints.gdb.\n"
