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
p/x hspi3.Init.Mode
p/x hspi3.Init.Direction
p/x hspi3.Init.DataSize
p/x hspi3.Init.CLKPolarity
p/x hspi3.Init.CLKPhase
p/x hspi3.Init.NSS
p/x hspi3.Init.NSSPolarity
p/x hspi3.Init.NSSPMode
p/x hspi3.Init.FirstBit
p/x hspi3.Init.BaudRatePrescaler
p/x hspi3.Instance->CR1
p/x hspi3.Instance->CR2
p/x hspi3.Instance->CFG1
p/x hspi3.Instance->CFG2
p/x hspi3.Instance->SR
p/x *(uint32_t *)0x42020000
p/x *(uint32_t *)0x42020024
p/x *(uint32_t *)0x42020014
p/x *(uint32_t *)0x42020010
p/x *(uint32_t *)0x42020c14
p/x *(uint32_t *)0x42020c10
info breakpoints
detach
quit
