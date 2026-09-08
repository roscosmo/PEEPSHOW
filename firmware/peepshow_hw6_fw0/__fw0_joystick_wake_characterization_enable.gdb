set pagination off

set $sm = &g_ps_hw6_owner_sm_probe
set $rt = &g_ps_hw6_rtos_probe
set $ui = &g_ps_ui_router_probe

if ($sm->magic != 0x48364653) || ($sm->version != 85)
  printf "HW6 wake characterization NOT armed: running firmware does not match owner probe API 85. Reflash this build first.\n"
else
  if $rt->runtime_complete == 0
    printf "HW6 wake characterization NOT armed: firmware startup is not complete.\n"
  else
    if $sm->joystick_calibration_active_valid == 0
      printf "HW6 wake characterization NOT armed: save a valid joystick calibration first.\n"
    else
      if !(($rt->runtime_current_class == 1 && $rt->runtime_lifecycle == 2) || ($rt->runtime_lifecycle == 3))
        printf "HW6 wake characterization NOT armed: enter the system menu first so the package is suspended.\n"
      else
        if g_ps_ui_router_request != 0
          printf "HW6 wake characterization NOT armed: a UI transition is still pending. Resume briefly, halt, and source this helper again.\n"
        else
          set var g_ps_hw6_joystick_wake_characterization_start_request = 1
          printf "--- HW6 guided joystick wake characterization armed ---\n"
          printf "Resume normally. WAKE SCAN guides CENTER, then alternates each cardinal direction with RELEASE CENTER.\n"
          printf "At CENTER, wait for the stick to physically settle and press A once. At each cardinal, hold full travel and press A once.\n"
          printf "SCANNING appears during each bounded capture; do not press A again until the next prompt.\n"
          printf "At HALT + PRINT, halt once and source __fw0_joystick_wake_characterization_prints.gdb.\n"
          printf "This diagnostic does not change or save calibration and does not change the production wake threshold.\n"
        end
      end
    end
  end
end
