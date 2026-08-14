set pagination off
set $dbgcr_before = *(uint32_t*)0xE0044004
set *(uint32_t*)0xE0044004 = ($dbgcr_before | 0x6)
set $dbgcr_after = *(uint32_t*)0xE0044004
printf "--- HW6 STOP2 debug-low-power ON helper ---\n"
printf "DBGMCU CR before/after = 0x%x / 0x%x\n", $dbgcr_before, $dbgcr_after
printf "DBG_STOP/DBG_STANDBY after = %u / %u\n", (($dbgcr_after & 0x2) != 0), (($dbgcr_after & 0x4) != 0)
printf "Use this when you want SWD/debug visibility through STOP/standby-style tests again.\n"