target extended-remote localhost:61234
monitor halt
info registers pc lr sp xpsr
p/x g_ps_phase0b_pmic_probe
x/55xb &g_ps_phase0b_pmic_probe.raw_reg
x/55xb &g_ps_phase0b_pmic_probe.raw_reg_status
p/d g_ps_phase0b_pmic_probe.vbat_mv
p/d g_ps_phase0b_pmic_probe.bat_soc_percent
p/d g_ps_phase0b_pmic_probe.buck_target_mv
p/d g_ps_phase0b_pmic_probe.buckboost_target_mv
p/x g_ps_phase0b_pmic_probe.fault
p/x g_ps_phase0b_pmic_probe.pgood_status
p/x &g_ps_phase0b_pmic_probe.vbat_mv
x/16xb &g_ps_phase0b_pmic_probe.vbat_mv
p/d g_ps_phase0b_pmic_probe.raw_reg[0x25]
p/d g_ps_phase0b_pmic_probe.raw_reg[0x26]
p/d (((unsigned int)g_ps_phase0b_pmic_probe.raw_reg[0x25] << 5) | (((unsigned int)g_ps_phase0b_pmic_probe.raw_reg[0x26] >> 3) & 0x1f))
info breakpoints
detach
quit
