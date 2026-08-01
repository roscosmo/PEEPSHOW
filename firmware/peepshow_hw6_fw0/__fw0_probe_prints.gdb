set pagination off

printf "=== HW6 FW0 BOOT PROBE ===\n"
printf "magic                = 0x%x\n", g_ps_hw6_fw0_probe.magic
printf "version              = 0x%x\n", g_ps_hw6_fw0_probe.version
printf "phase                = 0x%x\n", g_ps_hw6_fw0_probe.phase
printf "complete             = 0x%x\n", g_ps_hw6_fw0_probe.complete
printf "reset_flags          = 0x%x\n", g_ps_hw6_fw0_probe.reset_flags
printf "device_id            = 0x%x\n", g_ps_hw6_fw0_probe.device_id
printf "revision_id          = 0x%x\n", g_ps_hw6_fw0_probe.revision_id
printf "sysclk_hz            = %u\n", g_ps_hw6_fw0_probe.sysclk_hz
printf "expected_output_mask = 0x%x\n", g_ps_hw6_fw0_probe.expected_output_mask
printf "output_mask          = 0x%x\n", g_ps_hw6_fw0_probe.output_mask
printf "heartbeat            = 0x%x\n", g_ps_hw6_fw0_probe.heartbeat
printf "last_tick            = 0x%x\n", g_ps_hw6_fw0_probe.last_tick
printf "error_count          = 0x%x\n", g_ps_hw6_fw0_probe.error_count
printf "error_phase          = 0x%x\n", g_ps_hw6_fw0_probe.error_phase
printf "error_code           = 0x%x\n", g_ps_hw6_fw0_probe.error_code
printf "assert_count         = 0x%x\n", g_ps_hw6_fw0_probe.assert_count
printf "assert_line          = %u\n", g_ps_hw6_fw0_probe.assert_line
printf "assert_file          = %s\n", g_ps_hw6_fw0_probe.assert_file

printf "\n--- output mask bits ---\n"
printf "bit 0 PWR_DBG        = 0x%x\n", (g_ps_hw6_fw0_probe.output_mask >> 0) & 1
printf "bit 1 NINA_NRST      = 0x%x\n", (g_ps_hw6_fw0_probe.output_mask >> 1) & 1
printf "bit 2 SD_MODE        = 0x%x\n", (g_ps_hw6_fw0_probe.output_mask >> 2) & 1
printf "bit 3 FLASH_NCS      = 0x%x\n", (g_ps_hw6_fw0_probe.output_mask >> 3) & 1
printf "bit 4 DISPLAY_NSS    = 0x%x\n", (g_ps_hw6_fw0_probe.output_mask >> 4) & 1
printf "=== END HW6 FW0 BOOT PROBE ===\n"

