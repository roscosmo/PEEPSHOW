set pagination off

printf "--- HW6 charger/VBUS power scaffold ---\n"
set $sm = &g_ps_hw6_owner_sm_probe
set $owner = &g_ps_hw6_owner_probe
set $rtos = &g_ps_hw6_rtos_probe
printf "owner api/snapshot      = %u / %u / %u\n", $owner->version, $owner->power_complete, $owner->power_success
printf "power state/pmic state  = %u / %u\n", $sm->current_state[0], $sm->current_state[1]
printf "snapshot status/tick    = 0x%x / %u\n", $sm->battery_policy_last_snapshot_status, $sm->battery_policy_last_tick
printf "driver read/exp/rails/fault = 0x%x / 0x%x / %u / %u\n", $owner->power_driver_read_ok_mask, $owner->power_driver_expected_match_mask, $owner->power_rails_ready, $owner->power_fault_clear
printf "pmic vbus/mcu/agree     = %u / %u / %u\n", $owner->power_vbus_ok, $owner->power_mcu_vbus_present, $owner->power_vbus_agree
printf "vbus disagree count/tick = %u / %u\n", $owner->power_vbus_disagree_count, $owner->power_vbus_last_disagree_tick
printf "charger raw status1/2   = 0x%x / 0x%x\n", $owner->power_charger_status1, $owner->power_charger_status2
printf "charger read mask       = 0x%x\n", $owner->power_charger_monitor_read_ok_mask
printf "charger profile status = 0x%x\n", $owner->power_driver_charger_profile_status
printf "charger cfg mask      = 0x%x\n", $owner->power_charger_config_read_ok_mask
printf "charger cfg addr      = 0x%x / 0x%x / 0x%x / 0x%x / 0x%x\n", $owner->power_charger_config_address[0], $owner->power_charger_config_address[1], $owner->power_charger_config_address[2], $owner->power_charger_config_address[3], $owner->power_charger_config_address[4]
printf "charger cfg value     = 0x%x / 0x%x / 0x%x / 0x%x / 0x%x\n", $owner->power_charger_config_value[0], $owner->power_charger_config_value[1], $owner->power_charger_config_value[2], $owner->power_charger_config_value[3], $owner->power_charger_config_value[4]
printf "charger cfg status    = 0x%x / 0x%x / 0x%x / 0x%x / 0x%x\n", $owner->power_charger_config_status[0], $owner->power_charger_config_status[1], $owner->power_charger_config_status[2], $owner->power_charger_config_status[3], $owner->power_charger_config_status[4]
printf "profile/therm status/reg = 0x%x / 0x%x / 0x%x\n", $owner->power_driver_charger_profile_status, $owner->power_charger_thermistor_control_status, $owner->power_charger_thermistor_control
printf "therm status bits      = %u\n", $owner->power_battery_thermal_status
printf "charger mode/status/type/health = %u / %u / %u / %u\n", $owner->power_charger_mode, $owner->power_charger_status, $owner->power_charger_charge_type, $owner->power_charger_health
printf "battery ok/present/full = %u / %u / %u\n", $owner->power_battery_ok, $owner->power_battery_present, $owner->power_charge_complete
printf "fuel vbat/SOC/mask      = %u / %u / 0x%x\n", $owner->power_fuel_vbat_mv, $owner->power_fuel_soc_percent, $owner->power_fuel_read_ok_mask
printf "policy vbat/vbus/batt   = %u / %u / %u\n", $sm->battery_policy_vbat_mv, $sm->battery_policy_vbus_ok, $sm->battery_policy_battery_present
printf "policy state/event      = %u / %u\n", $sm->battery_policy_state, $sm->battery_policy_last_event
printf "pmic int irq/pending/cons = %u / %u / %u\n", $rtos->pmic_int_irq_count, $rtos->pmic_int_pending_count, $rtos->pmic_int_consumed_count
printf "pmic int pin/level/irq/consume = %u / %u / %u / %u\n", $rtos->pmic_int_last_pin, $rtos->pmic_int_last_level, $rtos->pmic_int_last_irq_tick, $rtos->pmic_int_last_consume_tick
printf "pmic int sm pending/snap/status = %u / %u / 0x%x\n", $sm->pmic_int_pending_count, $sm->pmic_int_snapshot_count, $sm->pmic_int_last_snapshot_status
printf "states: PWR_ACTIVE_LP=2 PWR_ACTIVE_RT=3 PWR_SHIP_PREP=8 PMIC_MONITOR=3 PMIC_CHARGING=4 PMIC_DONE=5 PMIC_LOW=6 PMIC_CRITICAL=7 PMIC_SHIP_PENDING=8\n"
printf "charger status: NOT_CHARGING=0 CHARGING=1 FULL=2 DISCHARGING=3 LDO=4 TIMER=5 BAT_DETECT=6 UNKNOWN=7\n"