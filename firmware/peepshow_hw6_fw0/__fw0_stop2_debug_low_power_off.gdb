set pagination off
set $dbgcr_before = *(uint32_t*)0xE0044004
set *(uint32_t*)0xE0044004 = ($dbgcr_before & 0xfffffff9)
set $dbgcr_after = *(uint32_t*)0xE0044004
printf "--- HW6 STOP2 debug-low-power OFF helper ---\n"
printf "DBGMCU CR before/after = 0x%x / 0x%x\n", $dbgcr_before, $dbgcr_after
printf "DBG_STOP/DBG_STANDBY after = %u / %u\n", (($dbgcr_after & 0x2) != 0), (($dbgcr_after & 0x4) != 0)
printf "If either after value is 1, the debugger/server re-enabled low-power debug.\n"
printf "Then source __fw0_stop2_controlled_entry_request.gdb and run the STOP2 wake test.\n"