set pagination off
set var g_ps_hw6_storage_persistent_install_request = 1
printf "--- HW6 persistent embedded egg install requested ---\n"
printf "request flag = %lu\n", g_ps_hw6_storage_persistent_install_request
printf "Resume and wait one second. This writes the known-valid embedded egg to the inactive package slot and commits its index record last.\n"
printf "Then halt and source __fw0_package_persistent_install_prints.gdb.\n"
