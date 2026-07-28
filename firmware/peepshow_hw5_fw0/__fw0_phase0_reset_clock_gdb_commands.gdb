target extended-remote localhost:61234
monitor halt
info registers pc lr sp xpsr
p/x g_ps_phase0_reset_clock_probe
p/d g_ps_phase0_reset_clock_probe.system_core_clock
p/d g_ps_phase0_reset_clock_probe.sysclk_hz
p/d g_ps_phase0_reset_clock_probe.hclk_hz
p/d g_ps_phase0_reset_clock_probe.pclk1_hz
p/d g_ps_phase0_reset_clock_probe.pclk2_hz
p/d g_ps_phase0_reset_clock_probe.pclk3_hz
p/x g_ps_phase0_reset_clock_probe.rcc_cr
p/x g_ps_phase0_reset_clock_probe.rcc_csr
p/x g_ps_phase0_reset_clock_probe.rcc_cfgr1
p/x g_ps_phase0_reset_clock_probe.rcc_cfgr2
p/x g_ps_phase0_reset_clock_probe.rcc_cfgr3
p/x g_ps_phase0_reset_clock_probe.rcc_icscr1
p/x g_ps_phase0_reset_clock_probe.pwr_vosr
info breakpoints
detach
quit
