set pagination off
if g_ps_hw6_rtos_probe.runtime_complete == 0
  printf "--- HW6 persistent runtime test NOT armed ---\n"
  printf "Firmware initialization is not complete. Continue until HOME is visible, halt, then source this helper again.\n"
else
  if g_ps_hw6_rtos_probe.runtime_current_class != 1
    printf "--- HW6 persistent runtime test NOT armed ---\n"
    printf "A package runtime is already active. This helper cannot replace it in place.\n"
    printf "Reset to boot the selected installed package, or return to the shell before using this helper.\n"
  else
    set var g_ps_package_source_override = 0
    set var g_ps_hw6_runtime_reactive_stub_request = 1
    printf "--- HW6 persistent installed egg runtime requested ---\n"
    printf "Resume from the shell. thRuntime will request thStorage to scan and copy the selected package before activation.\n"
    printf "Expected: the installed STATE scene launches, animates awake and in STOP2, and B returns HOME.\n"
    printf "Then halt and source __fw0_package_persistent_runtime_prints.gdb.\n"
  end
end
