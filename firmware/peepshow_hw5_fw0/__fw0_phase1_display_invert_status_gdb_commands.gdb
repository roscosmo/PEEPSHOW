target extended-remote localhost:61234
monitor halt
info registers pc lr sp xpsr
p/x g_ps_phase1_display_baseline_probe
p/x g_ps_phase1_display_invert_probe
p/d g_ps_phase1_display_invert_probe.frame_counter
p/d g_ps_phase1_display_invert_probe.tx_ok_count
p/d g_ps_phase1_display_invert_probe.tx_error_count
p/x g_ps_phase1_display_invert_probe.last_tx_status
p/x g_ps_phase1_display_invert_probe.last_hal_error
p/x g_ps_phase1_display_invert_probe.last_fill_value
p/x g_ps_phase1_display_invert_probe.vlt_lcd_pin_state
p/x hspi3.State
p/x hspi3.ErrorCode
p/x hspi3.Instance->CR1
p/x hspi3.Instance->CR2
p/x hspi3.Instance->CFG1
p/x hspi3.Instance->CFG2
p/x hspi3.Instance->SR
info breakpoints
detach
quit
