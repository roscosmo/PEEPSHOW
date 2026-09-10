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
      printf "OS sound variant: a square orbits four corners at (80,40), one frame every 250 ms. A moves the lower marker right; B moves it left. Each matched A/B transition plays a short tone without restarting animation.\n"
      printf "After 2 seconds the hidden square at (76,80) appears and a 6-second tone starts. Test tones are now twice the source amplitude. Local A/B changes must not stop the long tone.\n"
      printf "L starts a short and long tone together. Immediately HOLD START to open the shell: the long tone must stop early. Resume returns silently. L can start fresh sounds afterward. R exits to shell and discards sounds.\n"
      printf "This is explicitly AWAKE ONLY: automatic STOP2 is blocked while the development scene is active, including shell suspension. Do not force manual STOP2.\n"
      printf "Do not halt during playback. Once quiet, halt and source __fw0_object_scene_awake_prints.gdb.\n"
    end
  end
end
