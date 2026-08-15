set $xyz_count = g_ps_hw6_owner_sm_probe.joystick_xyz_capture_count
set $xyz_capacity = g_ps_hw6_owner_sm_probe.joystick_xyz_capture_capacity
set $xyz_mode = g_ps_hw6_owner_sm_probe.joystick_xyz_capture_mode
if $xyz_count > $xyz_capacity
  set $xyz_count = $xyz_capacity
end
if $xyz_mode == 1
  set logging file G:/PEEPSHOW/firmware/peepshow_hw6_fw0/__fw0_joystick_xyz_rest_capture.csv
else
  if $xyz_mode == 2
    set logging file G:/PEEPSHOW/firmware/peepshow_hw6_fw0/__fw0_joystick_xyz_sweep_capture.csv
  else
    if $xyz_mode == 3
      set logging file G:/PEEPSHOW/firmware/peepshow_hw6_fw0/__fw0_joystick_xyz_sweep_zrange_capture.csv
    else
      set logging file G:/PEEPSHOW/firmware/peepshow_hw6_fw0/__fw0_joystick_xyz_capture.csv
    end
  end
end
set logging overwrite on
set logging redirect on
set logging enabled on
printf "index,tick,delta_tick,mode,x,y,z,sensor_config2,conv_status,read_status\n"
set $i = 0
while $i < $xyz_count
  printf "%lu,%lu,%lu,%lu,%d,%d,%d,0x%x,%lu,%lu\n", g_ps_hw6_joystick_xyz_capture_buffer[$i].index, g_ps_hw6_joystick_xyz_capture_buffer[$i].tick, g_ps_hw6_joystick_xyz_capture_buffer[$i].delta_tick, g_ps_hw6_joystick_xyz_capture_buffer[$i].mode, g_ps_hw6_joystick_xyz_capture_buffer[$i].x, g_ps_hw6_joystick_xyz_capture_buffer[$i].y, g_ps_hw6_joystick_xyz_capture_buffer[$i].z, g_ps_hw6_joystick_xyz_capture_buffer[$i].reserved, g_ps_hw6_joystick_xyz_capture_buffer[$i].conv_status, g_ps_hw6_joystick_xyz_capture_buffer[$i].read_status
  set $i = $i + 1
end
set logging enabled off
if $xyz_mode == 1
  printf "Dumped %lu TMAG3001 REST XYZ rows to G:/PEEPSHOW/firmware/peepshow_hw6_fw0/__fw0_joystick_xyz_rest_capture.csv\n", $xyz_count
else
  if $xyz_mode == 2
    printf "Dumped %lu TMAG3001 SWEEP XYZ rows to G:/PEEPSHOW/firmware/peepshow_hw6_fw0/__fw0_joystick_xyz_sweep_capture.csv\n", $xyz_count
  else
    if $xyz_mode == 3
      printf "Dumped %lu TMAG3001 SWEEP Z-HIGH XYZ rows to G:/PEEPSHOW/firmware/peepshow_hw6_fw0/__fw0_joystick_xyz_sweep_zrange_capture.csv\n", $xyz_count
    else
      printf "Dumped %lu TMAG3001 XYZ rows to G:/PEEPSHOW/firmware/peepshow_hw6_fw0/__fw0_joystick_xyz_capture.csv\n", $xyz_count
    end
  end
end