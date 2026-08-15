set var g_ps_hw6_joystick_sleep_audit_request = 1
printf "HW6 TMAG3001 sleep audit queued for thInput. Continue target briefly, then interrupt and source __fw0_joystick_tmag_sleep_audit_prints.gdb.\n"
printf "This configures TMAG INT_CONFIG_1 to raw 0x01, clears active magnetic channels, writes sleep last, and does not read TMAG after sleep.\n"