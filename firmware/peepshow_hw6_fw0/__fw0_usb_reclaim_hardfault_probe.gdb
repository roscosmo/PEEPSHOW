set pagination off

printf "\n--- HW6 USB reclaim HardFault probe ---\n"
printf "Use while halted in HardFault after MSC RECLAIM WAIT. Read-only; do not reset first.\n"

printf "\nthread pointers:\n"
printf "current/execute/system/preempt = %p / %p / %lu / %u\n", _tx_thread_current_ptr, _tx_thread_execute_ptr, _tx_thread_system_state, _tx_thread_preempt_disable
set $current_valid = (((ULONG)_tx_thread_current_ptr >= 0x20000000) && ((ULONG)_tx_thread_current_ptr < 0x20100000))
set $execute_valid = (((ULONG)_tx_thread_execute_ptr >= 0x20000000) && ((ULONG)_tx_thread_execute_ptr < 0x20100000))
set $execute_name_valid = 0
if $execute_valid != 0
  set $execute_name_valid = (((ULONG)_tx_thread_execute_ptr->tx_thread_name >= 0x08000000) && ((ULONG)_tx_thread_execute_ptr->tx_thread_name < 0x08100000))
end
set $current_name_valid = 0
if $current_valid != 0
  set $current_name_valid = (((ULONG)_tx_thread_current_ptr->tx_thread_name >= 0x08000000) && ((ULONG)_tx_thread_current_ptr->tx_thread_name < 0x08100000))
end
printf "thread pointer valid current/execute = %lu / %lu\n", $current_valid, $execute_valid
if $current_valid != 0
  printf "current state/prio/preempt/run/susp/susp_cb/nameptr/name-valid = %u / %u / %u / %lu / %u / %p / %p / %lu\n", _tx_thread_current_ptr->tx_thread_state, _tx_thread_current_ptr->tx_thread_priority, _tx_thread_current_ptr->tx_thread_preempt_threshold, _tx_thread_current_ptr->tx_thread_run_count, _tx_thread_current_ptr->tx_thread_suspending, _tx_thread_current_ptr->tx_thread_suspend_control_block, _tx_thread_current_ptr->tx_thread_name, $current_name_valid
  if $current_name_valid != 0
    printf "current name = %s\n", _tx_thread_current_ptr->tx_thread_name
  end
  printf "current stack start/end/ptr/size/high = 0x%lx / 0x%lx / 0x%lx / %lu / 0x%lx\n", _tx_thread_current_ptr->tx_thread_stack_start, _tx_thread_current_ptr->tx_thread_stack_end, _tx_thread_current_ptr->tx_thread_stack_ptr, _tx_thread_current_ptr->tx_thread_stack_size, _tx_thread_current_ptr->tx_thread_stack_highest_ptr
  printf "current stack lower margin = %ld\n", (long)((ULONG)_tx_thread_current_ptr->tx_thread_stack_ptr - (ULONG)_tx_thread_current_ptr->tx_thread_stack_start)
  printf "current stack words at SP:\n"
  x/32wx _tx_thread_current_ptr->tx_thread_stack_ptr
end
if $execute_valid != 0
  printf "execute state/prio/preempt/run/susp/susp_cb/nameptr/name-valid = %u / %u / %u / %lu / %u / %p / %p / %lu\n", _tx_thread_execute_ptr->tx_thread_state, _tx_thread_execute_ptr->tx_thread_priority, _tx_thread_execute_ptr->tx_thread_preempt_threshold, _tx_thread_execute_ptr->tx_thread_run_count, _tx_thread_execute_ptr->tx_thread_suspending, _tx_thread_execute_ptr->tx_thread_suspend_control_block, _tx_thread_execute_ptr->tx_thread_name, $execute_name_valid
  if $execute_name_valid != 0
    printf "execute name = %s\n", _tx_thread_execute_ptr->tx_thread_name
  end
end

printf "\nreclaim state:\n"
printf "reclaim req/tick/dirty/parked = %lu / %lu / %lu / %lu\n", g_ps_hw6_owner_sm_probe.usb_reclaim_request_count, g_ps_hw6_owner_sm_probe.usb_reclaim_start_tick, g_ps_hw6_owner_sm_probe.usb_reclaim_dirty_seen, g_ps_hw6_owner_sm_probe.usb_reclaim_parked
printf "reclaim devdisc/pcd stop/deinit/fxlx close = 0x%lx / 0x%lx / 0x%lx / 0x%lx\n", g_ps_hw6_owner_sm_probe.usb_reclaim_devdisconnect_status, g_ps_hw6_owner_sm_probe.usb_reclaim_pcd_stop_status, g_ps_hw6_owner_sm_probe.usb_reclaim_deinit_status, g_ps_hw6_owner_sm_probe.usb_reclaim_fxlx_close_status
printf "stage rescan count/tick/pending/status/package/class = %lu / %lu / %lu / 0x%lx / 0x%lx / %lu\n", g_ps_hw6_owner_sm_probe.usb_stage_rescan_request_count, g_ps_hw6_owner_sm_probe.usb_stage_rescan_start_tick, g_ps_hw6_owner_sm_probe.usb_stage_rescan_pending, g_ps_hw6_owner_sm_probe.usb_stage_rescan_status, g_ps_hw6_owner_sm_probe.usb_stage_rescan_package_scan_status, g_ps_hw6_owner_sm_probe.usb_stage_rescan_classification
printf "stage entries file/dir/pkg/unsupported/bounded = %lu / %lu / %lu / %lu / %lu\n", g_ps_hw6_owner_sm_probe.usb_stage_rescan_file_count, g_ps_hw6_owner_sm_probe.usb_stage_rescan_directory_count, g_ps_hw6_owner_sm_probe.usb_stage_rescan_package_candidate_count, g_ps_hw6_owner_sm_probe.usb_stage_rescan_unsupported_count, g_ps_hw6_owner_sm_probe.usb_stage_rescan_bounded

printf "\nUSB/clock state:\n"
printf "PCD state/error = 0x%x / 0x%lx\n", hpcd_USB_OTG_FS.State, hpcd_USB_OTG_FS.ErrorCode
printf "USB clk/vdd/hsi48 ready = %u / %lu / %u\n", (__HAL_RCC_USB_IS_CLK_ENABLED() != 0U), ((READ_BIT(PWR->SVMCR, PWR_SVMCR_USV) != 0U) ? 1UL : 0UL), (__HAL_RCC_GET_FLAG(RCC_FLAG_HSI48RDY) != 0U)
printf "storage clock req/rel/reason/caps/status = %lu / %lu / %lu / 0x%lx / 0x%lx\n", g_ps_hw6_rtos_probe.storage_clock_request_count, g_ps_hw6_rtos_probe.storage_clock_release_count, g_ps_hw6_rtos_probe.storage_clock_last_reason, g_ps_hw6_rtos_probe.storage_clock_last_capabilities, g_ps_hw6_rtos_probe.storage_clock_last_status
printf "storage clock export/reclaim/release = 0x%lx / 0x%lx / 0x%lx\n", g_ps_hw6_rtos_probe.storage_clock_export_status, g_ps_hw6_rtos_probe.storage_clock_reclaim_status, g_ps_hw6_rtos_probe.storage_clock_release_status

printf "\nThreadX owner stack snapshots:\n"
printf "owner index order: PWR AUD INP DSP SNS STO COM UI RT\n"
printf "stack bytes: %lu %lu %lu %lu %lu %lu %lu %lu %lu\n", g_ps_hw6_rtos_probe.thread_stack_config_bytes[0], g_ps_hw6_rtos_probe.thread_stack_config_bytes[1], g_ps_hw6_rtos_probe.thread_stack_config_bytes[2], g_ps_hw6_rtos_probe.thread_stack_config_bytes[3], g_ps_hw6_rtos_probe.thread_stack_config_bytes[4], g_ps_hw6_rtos_probe.thread_stack_config_bytes[5], g_ps_hw6_rtos_probe.thread_stack_config_bytes[6], g_ps_hw6_rtos_probe.thread_stack_config_bytes[7], g_ps_hw6_rtos_probe.thread_stack_config_bytes[8]
printf "stack ptrs: 0x%lx 0x%lx 0x%lx 0x%lx 0x%lx 0x%lx 0x%lx 0x%lx 0x%lx\n", g_ps_hw6_rtos_probe.thread_stack_ptr[0], g_ps_hw6_rtos_probe.thread_stack_ptr[1], g_ps_hw6_rtos_probe.thread_stack_ptr[2], g_ps_hw6_rtos_probe.thread_stack_ptr[3], g_ps_hw6_rtos_probe.thread_stack_ptr[4], g_ps_hw6_rtos_probe.thread_stack_ptr[5], g_ps_hw6_rtos_probe.thread_stack_ptr[6], g_ps_hw6_rtos_probe.thread_stack_ptr[7], g_ps_hw6_rtos_probe.thread_stack_ptr[8]
printf "stack starts: 0x%lx 0x%lx 0x%lx 0x%lx 0x%lx 0x%lx 0x%lx 0x%lx 0x%lx\n", g_ps_hw6_rtos_probe.thread_stack_start[0], g_ps_hw6_rtos_probe.thread_stack_start[1], g_ps_hw6_rtos_probe.thread_stack_start[2], g_ps_hw6_rtos_probe.thread_stack_start[3], g_ps_hw6_rtos_probe.thread_stack_start[4], g_ps_hw6_rtos_probe.thread_stack_start[5], g_ps_hw6_rtos_probe.thread_stack_start[6], g_ps_hw6_rtos_probe.thread_stack_start[7], g_ps_hw6_rtos_probe.thread_stack_start[8]

printf "\nDirect ThreadX stack bounds from ps_threads, if the control blocks are still intact:\n"
printf "PWR start/end/ptr/size = 0x%lx / 0x%lx / 0x%lx / %lu\n", ps_threads[0].tx_thread_stack_start, ps_threads[0].tx_thread_stack_end, ps_threads[0].tx_thread_stack_ptr, ps_threads[0].tx_thread_stack_size
printf "STO start/end/ptr/size = 0x%lx / 0x%lx / 0x%lx / %lu\n", ps_threads[5].tx_thread_stack_start, ps_threads[5].tx_thread_stack_end, ps_threads[5].tx_thread_stack_ptr, ps_threads[5].tx_thread_stack_size
printf "PWR/STO lower margins = %ld / %ld\n", (long)((ULONG)ps_threads[0].tx_thread_stack_ptr - (ULONG)ps_threads[0].tx_thread_stack_start), (long)((ULONG)ps_threads[5].tx_thread_stack_ptr - (ULONG)ps_threads[5].tx_thread_stack_start)

printf "\nexpected: current thread and stack bounds identify whether reclaim overflowed thPower/thStorage/USBX stack; reclaim stage shows whether fault occurred before USB stop, during close, or during staging rescan.\n"
printf "--- end USB reclaim HardFault probe ---\n"
