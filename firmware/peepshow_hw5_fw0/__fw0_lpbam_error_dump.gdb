set pagination off
printf "=== FW0 LPBAM ERROR DUMP ===\n"
printf "\n--- backtrace ---\n"
bt
printf "\n--- core registers ---\n"
info registers
printf "\n--- phase6 lpbam probe ---\n"
printf "stage                          = 0x%lx\n", g_ps_phase5_threadx_probe.phase6_stage
printf "stop2_attempted                = 0x%lx\n", g_ps_phase5_threadx_probe.phase6_stop2_attempted
printf "stop2_returned                 = 0x%lx\n", g_ps_phase5_threadx_probe.phase6_stop2_returned
printf "stop2_return_count             = 0x%lx\n", g_ps_phase5_threadx_probe.phase6_stop2_return_count
printf "stop2_return_tick              = 0x%lx\n", g_ps_phase5_threadx_probe.phase6_stop2_return_tick
printf "button_wake_rearm_done         = 0x%lx\n", g_ps_phase5_threadx_probe.phase6_button_wake_rearm_done
printf "exti_imr1_before_stop          = 0x%lx\n", g_ps_phase5_threadx_probe.phase6_exti_imr1_before_stop
printf "exti_rtsr1_before_stop         = 0x%lx\n", g_ps_phase5_threadx_probe.phase6_exti_rtsr1_before_stop
printf "exti_ftsr1_before_stop         = 0x%lx\n", g_ps_phase5_threadx_probe.phase6_exti_ftsr1_before_stop
printf "exti_rpr1_before_stop          = 0x%lx\n", g_ps_phase5_threadx_probe.phase6_exti_rpr1_before_stop
printf "exti_fpr1_before_stop          = 0x%lx\n", g_ps_phase5_threadx_probe.phase6_exti_fpr1_before_stop
printf "exti_imr1_after_wake           = 0x%lx\n", g_ps_phase5_threadx_probe.phase6_exti_imr1_after_wake
printf "exti_rpr1_after_wake           = 0x%lx\n", g_ps_phase5_threadx_probe.phase6_exti_rpr1_after_wake
printf "exti_fpr1_after_wake           = 0x%lx\n", g_ps_phase5_threadx_probe.phase6_exti_fpr1_after_wake
printf "nvic_iser0_before_stop         = 0x%lx\n", g_ps_phase5_threadx_probe.phase6_nvic_iser0_before_stop
printf "nvic_ispr0_before_stop         = 0x%lx\n", g_ps_phase5_threadx_probe.phase6_nvic_ispr0_before_stop
printf "nvic_ispr0_after_wake          = 0x%lx\n", g_ps_phase5_threadx_probe.phase6_nvic_ispr0_after_wake
printf "exti_callback_count            = 0x%lx\n", g_ps_phase5_threadx_probe.phase6_exti_callback_count
printf "exti_callback_pin              = 0x%lx\n", g_ps_phase5_threadx_probe.phase6_exti_callback_pin
printf "exti_callback_button_id        = 0x%lx\n", g_ps_phase5_threadx_probe.phase6_exti_callback_button_id
printf "exti_callback_active           = 0x%lx\n", g_ps_phase5_threadx_probe.phase6_exti_callback_active_level
printf "exti_callback_tick             = 0x%lx\n", g_ps_phase5_threadx_probe.phase6_exti_callback_tick
printf "exti_callback_stage            = 0x%lx\n", g_ps_phase5_threadx_probe.phase6_exti_callback_stage
printf "exti_callback_gpioa_idr        = 0x%lx\n", g_ps_phase5_threadx_probe.phase6_exti_callback_gpioa_idr
printf "exti_callback_gpiob_idr        = 0x%lx\n", g_ps_phase5_threadx_probe.phase6_exti_callback_gpiob_idr
printf "lpbam_display_enabled          = 0x%lx\n", g_ps_phase5_threadx_probe.phase6_lpbam_display_enabled
printf "lpbam_display_rows             = 0x%lx\n", g_ps_phase5_threadx_probe.phase6_lpbam_display_rows
printf "lpbam_prepare_status           = 0x%lx\n", g_ps_phase5_threadx_probe.phase6_lpbam_prepare_status
printf "lpbam_fill_status              = 0x%lx\n", g_ps_phase5_threadx_probe.phase6_lpbam_fill_status
printf "lpbam_start_status             = 0x%lx\n", g_ps_phase5_threadx_probe.phase6_lpbam_start_status
printf "lpbam_dma_start_status         = 0x%lx\n", g_ps_phase5_threadx_probe.phase6_lpbam_dma_start_status
printf "lpbam_dma_mode_after_link      = 0x%lx\n", g_ps_phase5_threadx_probe.phase6_lpbam_dma_mode_after_link
printf "lpbam_dma_state_after_link     = 0x%lx\n", g_ps_phase5_threadx_probe.phase6_lpbam_dma_state_after_link
printf "lpbam_dma_error_after_link     = 0x%lx\n", g_ps_phase5_threadx_probe.phase6_lpbam_dma_error_after_link
printf "lpbam_queue_head               = 0x%lx\n", g_ps_phase5_threadx_probe.phase6_lpbam_queue_head
printf "lpbam_queue_first_circular     = 0x%lx\n", g_ps_phase5_threadx_probe.phase6_lpbam_queue_first_circular
printf "lpbam_queue_node_count         = 0x%lx\n", g_ps_phase5_threadx_probe.phase6_lpbam_queue_node_count
printf "lpbam_queue_state_after_link   = 0x%lx\n", g_ps_phase5_threadx_probe.phase6_lpbam_queue_state_after_link
printf "lpbam_queue_error_after_link   = 0x%lx\n", g_ps_phase5_threadx_probe.phase6_lpbam_queue_error_after_link
printf "lpbam_queue_type_after_link    = 0x%lx\n", g_ps_phase5_threadx_probe.phase6_lpbam_queue_type_after_link
printf "lpbam_dma_state_after_start    = 0x%lx\n", g_ps_phase5_threadx_probe.phase6_lpbam_dma_state_after_start
printf "lpbam_dma_error_after_start    = 0x%lx\n", g_ps_phase5_threadx_probe.phase6_lpbam_dma_error_after_start
printf "lpbam_queue_state_after_start  = 0x%lx\n", g_ps_phase5_threadx_probe.phase6_lpbam_queue_state_after_start
printf "lpbam_queue_error_after_start  = 0x%lx\n", g_ps_phase5_threadx_probe.phase6_lpbam_queue_error_after_start
printf "\n--- raw Queue1_Q ---\n"
p/x Queue1_Q
printf "\n--- raw LPDMA handle ---\n"
p/x handle_LPDMA1_Channel0
printf "\n--- raw live registers ---\n"
printf "LPDMA0_CLBAR                   = 0x%lx\n", handle_LPDMA1_Channel0.Instance->CLBAR
printf "LPDMA0_CSR                     = 0x%lx\n", handle_LPDMA1_Channel0.Instance->CSR
printf "LPDMA0_CCR                     = 0x%lx\n", handle_LPDMA1_Channel0.Instance->CCR
printf "LPDMA0_CLLR                    = 0x%lx\n", handle_LPDMA1_Channel0.Instance->CLLR
printf "SPI3_CR1                       = 0x%lx\n", hspi3.Instance->CR1
printf "SPI3_SR                        = 0x%lx\n", hspi3.Instance->SR
printf "SPI3_AUTOCR                    = 0x%lx\n", hspi3.Instance->AUTOCR
printf "LPTIM1_CR                      = 0x%lx\n", hlptim1.Instance->CR
printf "LPTIM1_ISR                     = 0x%lx\n", hlptim1.Instance->ISR
printf "\n--- queue node memory ---\n"
if Queue1_Q.Head != 0
  x/80wx Queue1_Q.Head
end
printf "=== END FW0 LPBAM ERROR DUMP ===\n"