set pagination off
printf "=== FW0 PHASE6 LIS2DUX12 IMU SLEEP ===\n"
printf "stage                       = 0x%lx\n", g_ps_phase5_threadx_probe.phase6_stage
printf "external_sleep_done         = 0x%lx\n", g_ps_phase5_threadx_probe.phase6_external_sleep_done
printf "lis_whoami_status           = 0x%lx\n", g_ps_phase5_threadx_probe.phase6_lis_whoami_status
printf "lis_readback_status         = 0x%lx\n", g_ps_phase5_threadx_probe.phase6_lis_readback_status
printf "lis_ctrl1_before            = 0x%lx\n", g_ps_phase5_threadx_probe.phase6_lis_ctrl1_before
printf "lis_ctrl2_before            = 0x%lx\n", g_ps_phase5_threadx_probe.phase6_lis_ctrl2_before
printf "lis_ctrl3_before            = 0x%lx\n", g_ps_phase5_threadx_probe.phase6_lis_ctrl3_before
printf "lis_ctrl4_before            = 0x%lx\n", g_ps_phase5_threadx_probe.phase6_lis_ctrl4_before
printf "lis_ctrl5_before            = 0x%lx\n", g_ps_phase5_threadx_probe.phase6_lis_ctrl5_before
printf "lis_fifo_before             = 0x%lx\n", g_ps_phase5_threadx_probe.phase6_lis_fifo_ctrl_before
printf "lis_intcfg_before           = 0x%lx\n", g_ps_phase5_threadx_probe.phase6_lis_interrupt_cfg_before
printf "lis_md1_before              = 0x%lx\n", g_ps_phase5_threadx_probe.phase6_lis_md1_cfg_before
printf "lis_md2_before              = 0x%lx\n", g_ps_phase5_threadx_probe.phase6_lis_md2_cfg_before
printf "lis_sleep_before            = 0x%lx\n", g_ps_phase5_threadx_probe.phase6_lis_sleep_before
printf "lis_fifo_bypass_status      = 0x%lx\n", g_ps_phase5_threadx_probe.phase6_lis_fifo_bypass_status
printf "lis_int_clear_status        = 0x%lx\n", g_ps_phase5_threadx_probe.phase6_lis_interrupt_clear_status
printf "lis_temp_disable_status     = 0x%lx\n", g_ps_phase5_threadx_probe.phase6_lis_temp_disable_status
printf "lis_emb_disable_status      = 0x%lx\n", g_ps_phase5_threadx_probe.phase6_lis_embedded_disable_status
printf "lis_mode_get_status         = 0x%lx\n", g_ps_phase5_threadx_probe.phase6_lis_mode_get_status
printf "lis_mode_set_status         = 0x%lx\n", g_ps_phase5_threadx_probe.phase6_lis_mode_set_status
printf "lis_deep_pd_status          = 0x%lx\n", g_ps_phase5_threadx_probe.phase6_lis_deep_pd_status
printf "lis_ctrl1_after             = 0x%lx\n", g_ps_phase5_threadx_probe.phase6_lis_ctrl1_after
printf "lis_ctrl2_after             = 0x%lx\n", g_ps_phase5_threadx_probe.phase6_lis_ctrl2_after
printf "lis_ctrl3_after             = 0x%lx\n", g_ps_phase5_threadx_probe.phase6_lis_ctrl3_after
printf "lis_ctrl4_after             = 0x%lx\n", g_ps_phase5_threadx_probe.phase6_lis_ctrl4_after
printf "lis_ctrl5_after             = 0x%lx\n", g_ps_phase5_threadx_probe.phase6_lis_ctrl5_after
printf "lis_fifo_after              = 0x%lx\n", g_ps_phase5_threadx_probe.phase6_lis_fifo_ctrl_after
printf "lis_intcfg_after            = 0x%lx\n", g_ps_phase5_threadx_probe.phase6_lis_interrupt_cfg_after
printf "lis_md1_after               = 0x%lx\n", g_ps_phase5_threadx_probe.phase6_lis_md1_cfg_after
printf "lis_md2_after               = 0x%lx\n", g_ps_phase5_threadx_probe.phase6_lis_md2_cfg_after
printf "lis_sleep_after             = 0x%lx\n", g_ps_phase5_threadx_probe.phase6_lis_sleep_after
printf "=== END LIS2DUX12 IMU SLEEP ===\n"
