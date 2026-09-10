set pagination off
printf "--- HW6 development V2 awake object scene ---\n"
if (g_ps_object_development_probe.api_version != 3) || (g_ps_scene_runtime_probe.api_version != 22) || (g_ps_audio_package_probe.api_version != 1)
  printf "NOT armed: this helper requires development/scene/audio-package APIs 3/22/1. Check the flashed ELF.\n"
else
  if (g_ps_object_development_probe.lease_fault != 0) || (g_ps_audio_package_probe.fault != 0)
    printf "NOT armed: a display or audio shutdown fault requires a reset before another launch.\n"
  else
    if (g_ps_hw6_rtos_probe.runtime_complete == 0) || ((g_ps_ui_router_probe.current_page != 1) && (g_ps_ui_router_probe.current_page != 2))
      printf "NOT armed: let boot finish, open HOME or the shell MENU, then halt.\n"
    else
      set g_ps_object_development_request = 1
      printf "Queued. Resume normally. No install or flash write is performed.\n"
      printf "Exact GUI four-frame timer fixture (de80153): large digits 1,2,3,4 loop at (72,40), 400 ms per frame. A moves the lower square right; B moves it left. Neither should reset the digits to 1.\n"
      printf "After 2 seconds the hidden square at (76,80) appears once; the digit should be 2 at expiry. Press A/B while 2,3 or 4 is visible and check that the sequence continues.\n"
      printf "This fixture has NO audio, L/R controls or exit action. HOLD START opens the shell; reset ends the development session.\n"
      printf "This is explicitly AWAKE ONLY: automatic STOP2 is blocked while the development scene is active, including shell suspension. Do not force manual STOP2.\n"
      printf "After observing the numbered sequence, A/B changes and timer reveal, halt and source __fw0_object_scene_awake_prints.gdb.\n"
    end
  end
end
