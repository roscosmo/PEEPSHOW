set pagination off
printf "--- HW6 development V2 awake object scene ---\n"
if (g_ps_object_development_probe.api_version != 2) || (g_ps_scene_runtime_probe.api_version != 22)
  printf "NOT armed: this helper requires development/scene APIs 2/22. Check the flashed ELF.\n"
else
  if g_ps_object_development_probe.lease_fault != 0
    printf "NOT armed: a display timeout quarantined the test buffer. Reset before another launch.\n"
  else
    if (g_ps_hw6_rtos_probe.runtime_complete == 0) || ((g_ps_ui_router_probe.current_page != 1) && (g_ps_ui_router_probe.current_page != 2))
      printf "NOT armed: let boot finish, open HOME or the shell MENU, then halt.\n"
    else
      set g_ps_object_development_request = 1
      printf "Queued. Resume normally. No install or flash write is performed.\n"
      printf "Studio fixture: a small sprite at (80,40) alternates two frames every 500 ms. A moves the lower square right; B moves it left. Neither should restart the sprite.\n"
      printf "GUI scene-timer fixture: a hidden third square at (76,80) appears once after 2 seconds, regardless of A/B selection. It stays visible through later state changes.\n"
      printf "Alternate A then B during the first 2 seconds. This fixture has no state timer or L/R timer controls.\n"
      printf "B does NOT exit this fixture. START opens the shell; reset ends the development session.\n"
      printf "This is explicitly AWAKE ONLY: automatic STOP2 is blocked while the development scene is active, including shell suspension. Do not force manual STOP2.\n"
      printf "After observing animation and alternating A then B several times, halt and source __fw0_object_scene_awake_prints.gdb.\n"
    end
  end
end
