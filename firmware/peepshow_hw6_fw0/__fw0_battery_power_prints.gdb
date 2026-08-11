set pagination off

printf "--- HW6 battery power policy scaffold ---\n"
set $sm = &g_ps_hw6_owner_sm_probe
set $owner = &g_ps_hw6_owner_probe
set $rtos = &g_ps_hw6_rtos_probe
printf "api/state/event       = %u / %u / %u\n", $sm->version, $sm->battery_policy_state, $sm->battery_policy_last_event
printf "counts boot/monitor   = %u / %u\n", $sm->battery_policy_boot_check_count, $sm->battery_policy_monitor_count
printf "snapshot status/tick  = 0x%x / %u\n", $sm->battery_policy_last_snapshot_status, $sm->battery_policy_last_tick
printf "driver read/exp/rails/fault = 0x%x / 0x%x / %u / %u\n", $owner->power_driver_read_ok_mask, $owner->power_driver_expected_match_mask, $owner->power_rails_ready, $owner->power_fault_clear
printf "period/next tick      = %u / %u\n", $sm->battery_policy_period_ticks, $sm->battery_policy_next_tick
printf "thresholds warn/crit/restart mV = %u / %u / %u\n", $sm->battery_policy_warning_mv, $sm->battery_policy_critical_mv, $sm->battery_policy_restart_allow_mv
printf "policy vbat/fuel/vbus/batt = %u / %u / %u / %u\n", $sm->battery_policy_vbat_mv, $sm->battery_policy_fuel_ok, $sm->battery_policy_vbus_ok, $sm->battery_policy_battery_present
printf "owner fuel vbat/SOC/mask = %u / %u / 0x%x\n", $owner->power_fuel_vbat_mv, $owner->power_fuel_soc_percent, $owner->power_fuel_read_ok_mask
printf "owner fuel raw H/L    = 0x%x / 0x%x\n", $owner->power_fuel_vbat_h, $owner->power_fuel_vbat_l
printf "owner fuel cfg/mode   = 0x%x / 0x%x\n", $owner->power_fuel_register_value[0], $owner->power_fuel_register_value[4]
printf "charger raw/status    = 0x%x / 0x%x / 0x%x\n", $owner->power_charger_status1, $owner->power_charger_status2, $owner->power_charger_monitor_read_ok_mask
printf "charger profile/cfg mask = 0x%x / 0x%x\n", $owner->power_driver_charger_profile_status, $owner->power_charger_config_read_ok_mask
printf "charger cfg value   = 0x%x / 0x%x / 0x%x / 0x%x / 0x%x\n", $owner->power_charger_config_value[0], $owner->power_charger_config_value[1], $owner->power_charger_config_value[2], $owner->power_charger_config_value[3], $owner->power_charger_config_value[4]
printf "profile/therm status/reg = 0x%x / 0x%x / 0x%x\n", $owner->power_driver_charger_profile_status, $owner->power_charger_thermistor_control_status, $owner->power_charger_thermistor_control
printf "pmic irq cfg/status = 0x%x / 0x%x\n", $owner->power_driver_interrupt_config_status, $owner->power_interrupt_read_ok_mask
printf "pmic irq addr       = 0x%x / 0x%x / 0x%x / 0x%x\n", $owner->power_interrupt_register_address[0], $owner->power_interrupt_register_address[1], $owner->power_interrupt_register_address[2], $owner->power_interrupt_register_address[3]
printf "pmic irq value      = 0x%x / 0x%x / 0x%x / 0x%x\n", $owner->power_interrupt_register_value[0], $owner->power_interrupt_register_value[1], $owner->power_interrupt_register_value[2], $owner->power_interrupt_register_value[3]
printf "pmic irq read status = 0x%x / 0x%x / 0x%x / 0x%x\n", $owner->power_interrupt_register_status[0], $owner->power_interrupt_register_status[1], $owner->power_interrupt_register_status[2], $owner->power_interrupt_register_status[3]
printf "pmic irq clear     = 0x%x / 0x%x / 0x%x\n", $owner->power_interrupt_clear_ok_mask, $owner->power_interrupt_clear_value[0], $owner->power_interrupt_clear_value[1]
printf "pmic irq clr stat  = 0x%x / 0x%x\n", $owner->power_interrupt_clear_status[0], $owner->power_interrupt_clear_status[1]
printf "therm status bits    = %u\n", $owner->power_battery_thermal_status
printf "charger mode/stat/type/health = %u / %u / %u / %u\n", $owner->power_charger_mode, $owner->power_charger_status, $owner->power_charger_charge_type, $owner->power_charger_health
printf "vbus pmic/mcu/agree/disagree = %u / %u / %u / %u\n", $owner->power_vbus_ok, $owner->power_mcu_vbus_present, $owner->power_vbus_agree, $owner->power_vbus_disagree_count
printf "battery ok/present/full = %u / %u / %u\n", $owner->power_battery_ok, $owner->power_battery_present, $owner->power_charge_complete
printf "warn/crit/bootblock   = %u / %u / %u\n", $sm->battery_policy_warning_count, $sm->battery_policy_critical_count, $sm->battery_policy_boot_restart_block_count
printf "quiesce count/status/tick = %u / 0x%x / %u\n", $sm->battery_policy_quiesce_request_count, $sm->battery_policy_quiesce_last_status, $sm->battery_policy_quiesce_last_tick
printf "ship gates crit/boot  = %u / %u\n", $sm->battery_policy_critical_ship_enabled, $sm->battery_policy_boot_ship_enabled
printf "ship req/skip/status/tick = %u / %u / 0x%x / %u\n", $sm->battery_policy_software_ship_request_count, $sm->battery_policy_software_ship_skipped_count, $sm->battery_policy_software_ship_last_status, $sm->battery_policy_software_ship_last_tick
printf "power state/pmic state = %u / %u\n", $sm->current_state[0], $sm->current_state[1]
printf "pmic MR/fuel prep/sw ship status = 0x%x / 0x%x / 0x%x\n", $owner->power_driver_mr_shipping_mode_status, $owner->power_driver_fuel_gauge_prepare_status, $owner->power_driver_software_shipping_mode_status
printf "pmic sw ship status/count/tick = 0x%x / %u / %u\n", $owner->power_driver_software_shipping_mode_status, $owner->power_software_ship_request_count, $owner->power_software_ship_request_tick
printf "pmic sw ship request flag = %u\n", g_ps_hw6_pmic_software_ship_request
printf "pmic int irq/pending/cons = %u / %u / %u\n", $rtos->pmic_int_irq_count, $rtos->pmic_int_pending_count, $rtos->pmic_int_consumed_count
printf "pmic int pin/level/irq/consume = %u / %u / %u / %u\n", $rtos->pmic_int_last_pin, $rtos->pmic_int_last_level, $rtos->pmic_int_last_irq_tick, $rtos->pmic_int_last_consume_tick
printf "pmic int sm pending/snap/status = %u / %u / 0x%x\n", $sm->pmic_int_pending_count, $sm->pmic_int_snapshot_count, $sm->pmic_int_last_snapshot_status
printf "policy states: UNKNOWN=0 BOOT_OK=1 OK=2 WARNING=3 CRITICAL=4 BOOT_RESTART_BLOCKED=5 SHIP_REQUESTED=6\n"
printf "policy events: NONE=0 BOOT=1 MONITOR=2 WARNING=3 CRITICAL=4 BOOT_BLOCK=5 SHIP_REQ=6 SHIP_SKIP=7 SNAPSHOT_FAIL=8\n"
printf "charger status: NOT_CHARGING=0 CHARGING=1 FULL=2 DISCHARGING=3 LDO=4 TIMER=5 BAT_DETECT=6 UNKNOWN=7\n"
printf "power: PWR_ACTIVE_LP=2 PWR_ACTIVE_RT=3 PWR_FORCED_SLEEP=7 PWR_SHIP_PREP=8 PMIC_MONITOR=3 PMIC_CHARGING=4 PMIC_DONE=5 PMIC_LOW=6 PMIC_CRITICAL=7 PMIC_SHIP_PENDING=8\n"
