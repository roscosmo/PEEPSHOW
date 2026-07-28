target extended-remote localhost:61234
monitor halt
info registers pc lr sp xpsr
p/x g_ps_phase0b_pmic_probe
p/x g_ps_phase0b_pmic_probe.driver_init_status
x/55xb &g_ps_phase0b_pmic_probe.raw_reg
x/55xb &g_ps_phase0b_pmic_probe.raw_reg_status
p/d g_ps_phase0b_pmic_probe.vbat_mv
p/d g_ps_phase0b_pmic_probe.bat_soc_percent
p/d g_ps_phase0b_pmic_probe.buck_target_mv
p/d g_ps_phase0b_pmic_probe.buckboost_target_mv
p/x g_ps_phase0b_pmic_probe.fault
p/x g_ps_phase0b_pmic_probe.pgood_status
info breakpoints
detach
quit
