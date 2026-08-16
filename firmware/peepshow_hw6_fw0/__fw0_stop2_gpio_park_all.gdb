set variable g_ps_hw6_stop2_gpio_park_group_mask_override = 0x1f
printf "HW6 STOP2 GPIO parking override set to diagnostic ALL groups (0x1f): OSPI, SAI, USB, display SPI, and I2C. OSPI analog parking measured higher STOP2 current; use __fw0_stop2_gpio_park_all_except_ospi.gdb for the validated default. Now run the controlled STOP2 test.\n"
