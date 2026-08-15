set var g_ps_hw6_joystick_xyz_capture_mode = 2
set var g_ps_hw6_joystick_xyz_capture_request = 1
printf "HW6 TMAG3001 SWEEP XYZ capture queued for thInput. Continue target now.\n"
printf "During capture: move the joystick through full travel, circles, edges, and corners. Expected runtime is about 10 seconds; hard timeout is about 15 seconds.\n"
printf "After the capture window, interrupt and source __fw0_joystick_xyz_prints.gdb, then __fw0_joystick_xyz_dump_csv.gdb.\n"
printf "CSV output: G:/PEEPSHOW/firmware/peepshow_hw6_fw0/__fw0_joystick_xyz_sweep_capture.csv\n"