set pagination off

printf "=== HW6 FW0 ADP5360 READ-ONLY PROBE ===\n"
printf "magic                 = 0x%x\n", g_ps_hw6_adp5360_probe.magic
printf "version               = 0x%x\n", g_ps_hw6_adp5360_probe.version
printf "phase                 = 0x%x\n", g_ps_hw6_adp5360_probe.phase
printf "complete              = 0x%x\n", g_ps_hw6_adp5360_probe.complete
printf "success               = 0x%x\n", g_ps_hw6_adp5360_probe.success
printf "start_tick            = 0x%x\n", g_ps_hw6_adp5360_probe.start_tick
printf "end_tick              = 0x%x\n", g_ps_hw6_adp5360_probe.end_tick
printf "duration_ticks        = %u\n", g_ps_hw6_adp5360_probe.duration_ticks
printf "address_7bit          = 0x%x\n", g_ps_hw6_adp5360_probe.address_7bit
printf "address_hal           = 0x%x\n", g_ps_hw6_adp5360_probe.address_hal
printf "ready_status          = 0x%x\n", g_ps_hw6_adp5360_probe.ready_status
printf "ready_error           = 0x%x\n", g_ps_hw6_adp5360_probe.ready_error
printf "i2c_state_before      = 0x%x\n", g_ps_hw6_adp5360_probe.i2c_state_before
printf "i2c_error_before      = 0x%x\n", g_ps_hw6_adp5360_probe.i2c_error_before
printf "i2c_state_after       = 0x%x\n", g_ps_hw6_adp5360_probe.i2c_state_after
printf "i2c_error_after       = 0x%x\n", g_ps_hw6_adp5360_probe.i2c_error_after
printf "attempted_count       = %u\n", g_ps_hw6_adp5360_probe.attempted_count
printf "read_count            = %u\n", g_ps_hw6_adp5360_probe.read_count
printf "failure_count         = %u\n", g_ps_hw6_adp5360_probe.failure_count
printf "read_ok_mask          = 0x%x\n", g_ps_hw6_adp5360_probe.read_ok_mask

printf "\n--- non-destructive register reads ---\n"
set $i = 0
while $i < 14
  printf "reg 0x%02x = 0x%02x  status=0x%x  error=0x%x\n", g_ps_hw6_adp5360_probe.register_address[$i], g_ps_hw6_adp5360_probe.register_value[$i], g_ps_hw6_adp5360_probe.register_status[$i], g_ps_hw6_adp5360_probe.register_error[$i]
  set $i = $i + 1
end

printf "\n--- decoded ---\n"
printf "manufacturer_model_id = 0x%x\n", g_ps_hw6_adp5360_probe.manufacturer_model_id
printf "manufacturer_id       = 0x%x\n", g_ps_hw6_adp5360_probe.manufacturer_id
printf "model_id              = 0x%x\n", g_ps_hw6_adp5360_probe.model_id
printf "identity_match        = 0x%x\n", g_ps_hw6_adp5360_probe.identity_match
printf "silicon_revision      = 0x%x\n", g_ps_hw6_adp5360_probe.silicon_revision
printf "charger_status1       = 0x%x\n", g_ps_hw6_adp5360_probe.charger_status1
printf "charger_status2       = 0x%x\n", g_ps_hw6_adp5360_probe.charger_status2
printf "battery_soc_percent   = %u\n", g_ps_hw6_adp5360_probe.battery_soc_percent
printf "vbat_raw              = 0x%x\n", g_ps_hw6_adp5360_probe.vbat_raw
printf "vbat_mv               = %u\n", g_ps_hw6_adp5360_probe.vbat_mv
printf "buck_config           = 0x%x\n", g_ps_hw6_adp5360_probe.buck_config
printf "buck_vout             = 0x%x\n", g_ps_hw6_adp5360_probe.buck_vout
printf "buckboost_config      = 0x%x\n", g_ps_hw6_adp5360_probe.buckboost_config
printf "buckboost_vout        = 0x%x\n", g_ps_hw6_adp5360_probe.buckboost_vout
printf "supervisory           = 0x%x\n", g_ps_hw6_adp5360_probe.supervisory
printf "fault                 = 0x%x\n", g_ps_hw6_adp5360_probe.fault
printf "pgood                 = 0x%x\n", g_ps_hw6_adp5360_probe.pgood
printf "vout1_ok              = 0x%x\n", g_ps_hw6_adp5360_probe.vout1_ok
printf "vout2_ok              = 0x%x\n", g_ps_hw6_adp5360_probe.vout2_ok
printf "rails_ready           = 0x%x\n", g_ps_hw6_adp5360_probe.rails_ready
printf "vbus_ok               = 0x%x\n", g_ps_hw6_adp5360_probe.vbus_ok
printf "battery_ok            = 0x%x\n", g_ps_hw6_adp5360_probe.battery_ok
printf "mr_pressed            = 0x%x\n", g_ps_hw6_adp5360_probe.mr_pressed
printf "=== END HW6 FW0 ADP5360 PROBE ===\n"
