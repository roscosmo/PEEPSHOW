set pagination off
printf "--- HW6 development V2 awake object scene ---\n"
if (g_ps_object_development_probe.api_version != 1) || (g_ps_scene_runtime_probe.api_version != 22)
  printf "NOT armed: this helper requires development/scene APIs 1/22. Check the flashed ELF.\n"
else
  if g_ps_object_development_probe.lease_fault != 0
    printf "NOT armed: a display timeout quarantined the test buffer. Reset before another launch.\n"
  else
    if (g_ps_hw6_rtos_probe.runtime_complete == 0) || ((g_ps_ui_router_probe.current_page != 1) && (g_ps_ui_router_probe.current_page != 2))
      printf "NOT armed: let boot finish, open HOME or the shell MENU, then halt.\n"
    else
      set g_ps_object_development_request = 1
      printf "Queued. Resume normally. No install or flash write is performed.\n"
      printf "Default fixture: a square moves around four corners every 250 ms. A moves the separate lower marker without restarting that animation. B returns to the shell.\n"
      printf "This is explicitly AWAKE ONLY: automatic STOP2 is blocked while the development scene is active, including shell suspension. Do not force manual STOP2.\n"
      printf "After observing animation and pressing A several times, halt and source __fw0_object_scene_awake_prints.gdb.\n"
    end
  end
end
