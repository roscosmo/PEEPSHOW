set pagination off
set var g_ps_hw6_storage_persistent_install_request = 1
printf "--- HW6 persistent embedded egg install requested ---\n"
printf "request flag = %lu\n", g_ps_hw6_storage_persistent_install_request
printf "Resume normally. The screen will show PACKAGE / INSTALLING while thStorage owns flash.\n"
printf "It ends at INSTALLED / A PLAY on success or PKG ERROR / SEE GDB on failure.\n"
printf "Large eggs take longer than one second. Do not arm runtime replacement until the install print reports status=0 and stage=11.\n"
printf "Then halt and source __fw0_package_persistent_install_prints.gdb.\n"
