set pagination off
if g_ps_hw6_rtos_probe.runtime_complete == 0
  printf "--- HW6 persistent runtime replacement NOT armed ---\n"
  printf "Firmware initialization is not complete. Continue until HOME or a package scene is visible, halt, then source this helper again.\n"
else
  if ((g_ps_hw6_rtos_probe.runtime_current_class != 1) && (g_ps_hw6_rtos_probe.runtime_current_class != 2))
    printf "--- HW6 persistent runtime replacement NOT armed ---\n"
    printf "The runtime is neither the shell nor an active STATE scene. Return to one of those states before using this helper.\n"
  else
    set var g_ps_package_source_override = 0
    set var g_ps_hw6_runtime_persistent_replace_request = 1
    printf "--- HW6 persistent installed egg replacement requested ---\n"
    printf "Resume normally. thRuntime reloads the selected installed package; an active SFX is allowed to finish before its source bytes change.\n"
    printf "The old scene is replaced in place. A load or render failure returns to the shell error page.\n"
    printf "When the new scene or error page is visible, halt and source __fw0_package_persistent_runtime_prints.gdb.\n"
  end
end
