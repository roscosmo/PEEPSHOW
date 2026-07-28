printf "=== FW0 PHASE6 LIVE WAKE REGS ===\n"
printf "--- firmware probe ---\n"
printf "stage                  = 0x%lx\n", g_ps_phase5_threadx_probe.phase6_stage
printf "stop2_attempted        = 0x%lx\n", g_ps_phase5_threadx_probe.phase6_stop2_attempted
printf "stop2_returned         = 0x%lx\n", g_ps_phase5_threadx_probe.phase6_stop2_returned
printf "stop2_return_count     = 0x%lx\n", g_ps_phase5_threadx_probe.phase6_stop2_return_count
printf "button_rearm           = 0x%lx\n", g_ps_phase5_threadx_probe.phase6_button_wake_rearm_done
printf "exti_imr1_before_stop  = 0x%lx\n", g_ps_phase5_threadx_probe.phase6_exti_imr1_before_stop
printf "exti_rtsr1_before_stop = 0x%lx\n", g_ps_phase5_threadx_probe.phase6_exti_rtsr1_before_stop
printf "exti_ftsr1_before_stop = 0x%lx\n", g_ps_phase5_threadx_probe.phase6_exti_ftsr1_before_stop
printf "nvic_iser0_before_stop = 0x%lx\n", g_ps_phase5_threadx_probe.phase6_nvic_iser0_before_stop
printf "dbgmc_cr_before_stop2  = 0x%lx\n", g_ps_phase5_threadx_probe.phase6_dbgmcu_cr_before_stop2
printf "exti_callback_count       = 0x%lx\n", g_ps_phase5_threadx_probe.phase6_exti_callback_count
printf "exti_callback_pin         = 0x%lx\n", g_ps_phase5_threadx_probe.phase6_exti_callback_pin
printf "exti_callback_button_id   = 0x%lx\n", g_ps_phase5_threadx_probe.phase6_exti_callback_button_id
printf "exti_callback_active      = 0x%lx\n", g_ps_phase5_threadx_probe.phase6_exti_callback_active_level
printf "exti_callback_tick        = 0x%lx\n", g_ps_phase5_threadx_probe.phase6_exti_callback_tick
printf "exti_callback_stage       = 0x%lx\n", g_ps_phase5_threadx_probe.phase6_exti_callback_stage
printf "exti_callback_gpioa_idr   = 0x%lx\n", g_ps_phase5_threadx_probe.phase6_exti_callback_gpioa_idr
printf "exti_callback_gpiob_idr   = 0x%lx\n", g_ps_phase5_threadx_probe.phase6_exti_callback_gpiob_idr
printf "\n--- system/control ---\n"
printf "SCB_SCR                = 0x%lx\n", *(unsigned int *)0xE000ED10
printf "DBGMCU_CR              = 0x%lx\n", *(unsigned int *)0xE0044004
printf "DBGMCU_APB1LFZR        = 0x%lx\n", *(unsigned int *)0xE0044008
printf "NVIC_ISER0             = 0x%lx\n", *(unsigned int *)0xE000E100
printf "NVIC_ISPR0             = 0x%lx\n", *(unsigned int *)0xE000E200
printf "\n--- secure aliases ---\n"
printf "PWR_S_SR               = 0x%lx\n", *(unsigned int *)0x56020838
printf "PWR_S_WUSR             = 0x%lx\n", *(unsigned int *)0x56020844
printf "EXTI_S_RTSR1           = 0x%lx\n", *(unsigned int *)0x56022000
printf "EXTI_S_FTSR1           = 0x%lx\n", *(unsigned int *)0x56022004
printf "EXTI_S_RPR1            = 0x%lx\n", *(unsigned int *)0x5602200c
printf "EXTI_S_FPR1            = 0x%lx\n", *(unsigned int *)0x56022010
printf "EXTI_S_IMR1            = 0x%lx\n", *(unsigned int *)0x56022080
printf "GPIOA_S_IDR            = 0x%lx\n", *(unsigned int *)0x52020010
printf "GPIOB_S_IDR            = 0x%lx\n", *(unsigned int *)0x52020410
printf "\n--- non-secure aliases ---\n"
printf "PWR_NS_SR              = 0x%lx\n", *(unsigned int *)0x46020838
printf "PWR_NS_WUSR            = 0x%lx\n", *(unsigned int *)0x46020844
printf "EXTI_NS_RTSR1          = 0x%lx\n", *(unsigned int *)0x46022000
printf "EXTI_NS_FTSR1          = 0x%lx\n", *(unsigned int *)0x46022004
printf "EXTI_NS_RPR1           = 0x%lx\n", *(unsigned int *)0x4602200c
printf "EXTI_NS_FPR1           = 0x%lx\n", *(unsigned int *)0x46022010
printf "EXTI_NS_IMR1           = 0x%lx\n", *(unsigned int *)0x46022080
printf "GPIOA_NS_IDR           = 0x%lx\n", *(unsigned int *)0x42020010
printf "GPIOB_NS_IDR           = 0x%lx\n", *(unsigned int *)0x42020410
printf "=== END LIVE WAKE REGS ===\n"