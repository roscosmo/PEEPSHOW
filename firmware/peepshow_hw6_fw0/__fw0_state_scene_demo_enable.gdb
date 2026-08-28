set pagination off
set var g_ps_package_source_override = 2
set var g_display_renderer_waiting_test_variant = 0
set var g_ps_scene_runtime_waiting_demo_enable = 0
set var g_ps_hw6_runtime_reactive_stub_request = 1
printf "--- HW6 STATE_SCENE proof requested ---\n"
printf "No exact timing is required. Start on the busy center screen with several shapes.\n"
printf "1. On the opening busy center STATE, watch the right-edge marker move through its three vertical positions every 250 ms. Press R once: a short select sound plays, the sparse two-sprite screen appears, and that right-edge marker now holds each position for two ticks while the left cursor keeps blinking every tick.\n"
printf "2. Press R again: the busy left screen appears; its marker returns to one position per tick with an intentional timeline restart, its circle is missing, and the small static lower-right stripe pattern has changed.\n"
printf "3. Press R again: the busy center screen returns; the circle, edge marker, and lower-right pattern are restored.\n"
printf "4. Press A: a different sparse scene with a left-edge marker appears. Press A again: the original busy center screen returns.\n"
printf "5. Pause naturally on the sparse two-sprite screen and a busy shapes screen long enough for each to enter STOP2. The cursor and right-edge marker must continue animating, and static shapes must remain. B returns HOME.\n"
printf "Then halt and source __fw0_state_scene_demo_prints.gdb. Expected: one SFX cue dispatch completes, action errors=0, waiting-animation commits at least 2 with at least one rebase, scene replace count at least 2, replace failures=0.\n"
