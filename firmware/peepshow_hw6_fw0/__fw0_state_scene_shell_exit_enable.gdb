set pagination off
set var g_ps_package_source_override = 2
set var g_display_renderer_waiting_test_variant = 0
set var g_ps_scene_runtime_waiting_demo_enable = 0
printf "--- HW6 authored STATE shell exit requested ---\n"
printf "This helper may be sourced at the temporary main breakpoint; normal boot launches the selected package entry scene.\n"
printf "Continue until the source scene appears. Press A once to enter INPUT PROOF, then press START once whenever ready.\n"
printf "Expected: the package ends and the PeepOS shell menu appears. START is authored by this diagnostic package; no system-owned shell gesture is defined.\n"
printf "Then halt and source __fw0_state_scene_shell_exit_prints.gdb.\n"
