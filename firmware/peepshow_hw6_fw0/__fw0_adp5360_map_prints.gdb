set pagination off

printf "=== HW6 FW0 ADP5360 CONFIGURATION MAP ===\n"
printf "magic                     = 0x%x\n", g_ps_hw6_adp5360_map_probe.magic
printf "version                   = 0x%x\n", g_ps_hw6_adp5360_map_probe.version
printf "phase                     = 0x%x\n", g_ps_hw6_adp5360_map_probe.phase
printf "complete                  = 0x%x\n", g_ps_hw6_adp5360_map_probe.complete
printf "success                   = 0x%x\n", g_ps_hw6_adp5360_map_probe.success
printf "duration_ticks            = %u\n", g_ps_hw6_adp5360_map_probe.duration_ticks
printf "attempted_count           = %u\n", g_ps_hw6_adp5360_map_probe.attempted_count
printf "read_count                = %u\n", g_ps_hw6_adp5360_map_probe.read_count
printf "failure_count             = %u\n", g_ps_hw6_adp5360_map_probe.failure_count
printf "identity_match            = 0x%x\n", g_ps_hw6_adp5360_map_probe.identity_match

printf "\n--- decoded configuration anchors ---\n"
printf "charger_function          = 0x%x\n", g_ps_hw6_adp5360_map_probe.charger_function
printf "battery_thermistor_ctrl   = 0x%x\n", g_ps_hw6_adp5360_map_probe.battery_thermistor_control
printf "battery_protection_ctrl   = 0x%x\n", g_ps_hw6_adp5360_map_probe.battery_protection_control
printf "battery_capacity_code     = 0x%x\n", g_ps_hw6_adp5360_map_probe.battery_capacity_code
printf "battery_capacity_mah      = %u\n", g_ps_hw6_adp5360_map_probe.battery_capacity_mah
printf "fuel_gauge_mode           = 0x%x\n", g_ps_hw6_adp5360_map_probe.fuel_gauge_mode
printf "fuel_gauge_enabled        = 0x%x\n", g_ps_hw6_adp5360_map_probe.fuel_gauge_enabled
printf "fuel_gauge_sleep_mode     = 0x%x\n", g_ps_hw6_adp5360_map_probe.fuel_gauge_sleep_mode
printf "buck_config               = 0x%x\n", g_ps_hw6_adp5360_map_probe.buck_config
printf "buck_target_mv            = %u\n", g_ps_hw6_adp5360_map_probe.buck_target_mv
printf "buckboost_config          = 0x%x\n", g_ps_hw6_adp5360_map_probe.buckboost_config
printf "buckboost_target_mv       = %u\n", g_ps_hw6_adp5360_map_probe.buckboost_target_mv
printf "supervisory               = 0x%x\n", g_ps_hw6_adp5360_map_probe.supervisory
printf "fault                     = 0x%x\n", g_ps_hw6_adp5360_map_probe.fault
printf "pgood                     = 0x%x\n", g_ps_hw6_adp5360_map_probe.pgood
printf "interrupt_enable1         = 0x%x\n", g_ps_hw6_adp5360_map_probe.interrupt_enable1
printf "interrupt_enable2         = 0x%x\n", g_ps_hw6_adp5360_map_probe.interrupt_enable2
printf "interrupt_flag1           = 0x%x\n", g_ps_hw6_adp5360_map_probe.interrupt_flag1
printf "interrupt_flag2           = 0x%x\n", g_ps_hw6_adp5360_map_probe.interrupt_flag2
printf "shipmode                  = 0x%x\n", g_ps_hw6_adp5360_map_probe.shipmode

printf "\n--- ID, charger, thermistor, protection: 0x00..0x15 ---\n"
set $reg = 0x00
while $reg <= 0x15
  printf "reg 0x%02x = 0x%02x  status=0x%x  error=0x%x\n", $reg, g_ps_hw6_adp5360_map_probe.register_value[$reg], g_ps_hw6_adp5360_map_probe.register_status[$reg], g_ps_hw6_adp5360_map_probe.register_error[$reg]
  set $reg = $reg + 1
end

printf "\n--- fuel gauge: 0x16..0x28 ---\n"
set $reg = 0x16
while $reg <= 0x28
  printf "reg 0x%02x = 0x%02x  status=0x%x  error=0x%x\n", $reg, g_ps_hw6_adp5360_map_probe.register_value[$reg], g_ps_hw6_adp5360_map_probe.register_status[$reg], g_ps_hw6_adp5360_map_probe.register_error[$reg]
  set $reg = $reg + 1
end

printf "\n--- regulators, supervisor, IRQ, ship mode: 0x29..0x36 ---\n"
set $reg = 0x29
while $reg <= 0x36
  printf "reg 0x%02x = 0x%02x  status=0x%x  error=0x%x\n", $reg, g_ps_hw6_adp5360_map_probe.register_value[$reg], g_ps_hw6_adp5360_map_probe.register_status[$reg], g_ps_hw6_adp5360_map_probe.register_error[$reg]
  set $reg = $reg + 1
end
printf "=== END HW6 FW0 ADP5360 CONFIGURATION MAP ===\n"
