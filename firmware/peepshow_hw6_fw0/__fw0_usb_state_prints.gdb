printf "=== HW6 FW0 USBX / PCD STATE PRINT ===\n"
printf "Use while halted after USB MSC export attempt. Read-only; do not reset first.\n\n"

printf "--- USB export probe summary ---\n"
printf "display page/shutdown = %lu / %lu\n", g_ps_hw6_owner_probe.display_ui_page, g_ps_hw6_owner_probe.display_ui_shutdown_state
printf "export req/tick/vbus = %lu / %lu / %lu\n", g_ps_hw6_owner_sm_probe.usb_export_request_count, g_ps_hw6_owner_sm_probe.usb_export_start_tick, g_ps_hw6_owner_sm_probe.usb_export_vbus_present
printf "export policy/dcd/init/start = 0x%lx / 0x%lx / 0x%lx / 0x%lx\n", g_ps_hw6_owner_sm_probe.usb_export_policy_status, g_ps_hw6_owner_sm_probe.usb_export_dcd_status, g_ps_hw6_owner_sm_probe.usb_export_pcd_init_status, g_ps_hw6_owner_sm_probe.usb_export_pcd_start_status
printf "export flash_wake/fxlx_open = 0x%lx / 0x%lx\n", g_ps_hw6_owner_sm_probe.usb_export_flash_wake_status, g_ps_hw6_owner_sm_probe.usb_export_fxlx_open_status
set $fxlx_msc = &g_ps_storage_filex_levelx_msc_probe
printf "fxlx msc api/status/stage/active = %lu / 0x%lx / %lu / %lu\n", $fxlx_msc->api_version, $fxlx_msc->status, $fxlx_msc->last_stage, $fxlx_msc->active
printf "fxlx msc open/close/old/export = %lu / %lu / %lu / %lu\n", $fxlx_msc->open_count, $fxlx_msc->close_count, $fxlx_msc->already_open, $fxlx_msc->export_length
printf "fxlx msc region id/start/len = %lu / 0x%lx / %lu\n", $fxlx_msc->region_id, $fxlx_msc->region_start, $fxlx_msc->region_length
printf "fxlx msc validate/lx init/open/close = 0x%lx / 0x%lx / 0x%lx / 0x%lx\n", $fxlx_msc->validate_status, $fxlx_msc->lx_initialize_status, $fxlx_msc->lx_open_status, $fxlx_msc->lx_close_status
printf "fxlx msc recovery invalid/count lx/driver = %lu / %lu / 0x%lx / 0x%lx\n", $fxlx_msc->invalid_media_detected, $fxlx_msc->recovery_required_count, $fxlx_msc->recovery_lx_open_status, $fxlx_msc->recovery_driver_status
printf "fxlx msc LX rd/wr/erase/verify/last = %lu / %lu / %lu / %lu / 0x%lx\n", $fxlx_msc->lx_driver_read_count, $fxlx_msc->lx_driver_write_count, $fxlx_msc->lx_driver_erase_count, $fxlx_msc->lx_driver_verify_count, $fxlx_msc->lx_driver_last_status
printf "fxlx msc block/flash/nor/ospi = 0x%lx / %lu / 0x%lx / %lu / 0x%lx\n", $fxlx_msc->block_last_status, $fxlx_msc->flash_state, $fxlx_msc->flash_last_status, $fxlx_msc->nor_state, $fxlx_msc->ospi_error_after
printf "fxlx msc stages: IDLE=0 VALIDATE=1 OLD_OPEN=2 LX_INIT=3 LX_OPEN=4 OPENED=5 CLOSE=6 FAULT=7\n"
printf "ui shutdown: NONE=0 PREP=1 WARNING=2 IMMINENT=3 CANCELLED=4 LOW_BOOT=5 LOW_CHARGE=6 FLASH_INIT=7 FLASH_DONE=8 FLASH_ERROR=9 MSC_EXPORT=10 MSC_ACTIVE=11 MSC_RECLAIM=12 MSC_DONE=13 MSC_ERROR=14 MSC_RECOVERY=15\n"
printf "usbx pool/init/stage/error/dcd irqguard = 0x%lx / 0x%lx / %lu / 0x%lx / 0x%lx / %lu\n", g_ps_hw6_usbx_byte_pool_create_status, g_ps_hw6_usbx_device_init_status, g_ps_hw6_usbx_init_stage, g_ps_hw6_usbx_init_error_code, g_ps_hw6_usbx_dcd_status, g_ps_hw6_usb_irq_guard_drop_count
printf "rtos low-power enter/usb-skip = %lu / %lu\n", g_ps_hw6_rtos_probe.low_power_enter_count, g_ps_hw6_rtos_low_power_usb_skip_count
printf "export irq prio before/after devconnect = %lu / %lu / 0x%lx\n", g_ps_hw6_owner_sm_probe.usb_export_irq_priority_before, g_ps_hw6_owner_sm_probe.usb_export_irq_priority_after, g_ps_hw6_owner_sm_probe.usb_export_devconnect_status
printf "export pcd/clk/vdd/started = 0x%lx / %lu / %lu / %lu\n", g_ps_hw6_owner_sm_probe.usb_export_pcd_state_after, g_ps_hw6_owner_sm_probe.usb_export_clock_enabled_after, g_ps_hw6_owner_sm_probe.usb_export_vddusb_enabled_after, g_ps_hw6_owner_sm_probe.usb_export_started
printf "clock SystemCoreClock/SysTick_LOAD/SysTick_CTRL = %lu / 0x%lx / 0x%lx\n", SystemCoreClock, *(unsigned int*)0xE000E014, *(unsigned int*)0xE000E010
printf "bridge activate/deactivate = %lu / %lu\n", g_ps_storage_msc_bridge_probe.activate_count, g_ps_storage_msc_bridge_probe.deactivate_count
printf "bridge submit/done/timeout/busy = %lu / %lu / %lu / %lu\n", g_ps_storage_msc_bridge_probe.submit_count, g_ps_storage_msc_bridge_probe.completed_count, g_ps_storage_msc_bridge_probe.timeout_count, g_ps_storage_msc_bridge_probe.busy_count
printf "bridge rd/wr/fl/status/fast_status = %lu / %lu / %lu / %lu / %lu\n", g_ps_storage_msc_bridge_probe.read_count, g_ps_storage_msc_bridge_probe.write_count, g_ps_storage_msc_bridge_probe.flush_count, g_ps_storage_msc_bridge_probe.status_count, g_ps_storage_msc_bridge_probe.fast_status_count
printf "bridge policy/media/write/dirty/init = %lu / %lu / %lu / %lu / %lu\n", g_ps_storage_msc_bridge_probe.export_enabled, g_ps_storage_msc_bridge_probe.media_present, g_ps_storage_msc_bridge_probe.write_enabled, g_ps_storage_msc_bridge_probe.dirty, g_ps_storage_msc_bridge_probe.initialized
printf "bridge last cmd/lba/blocks tx/owner/ux/media/ps = %lu / %lu / %lu / 0x%lx / 0x%lx / 0x%lx / 0x%lx / 0x%lx\n", g_ps_storage_msc_bridge_probe.last_command, g_ps_storage_msc_bridge_probe.last_lba, g_ps_storage_msc_bridge_probe.last_block_count, g_ps_storage_msc_bridge_probe.last_tx_status, g_ps_storage_msc_bridge_probe.last_owner_status, g_ps_storage_msc_bridge_probe.last_ux_status, g_ps_storage_msc_bridge_probe.last_media_status, g_ps_storage_msc_bridge_probe.last_ps_status
printf "reclaim devdisc/uxdisc-not-used/fxlx_close = 0x%lx / 0x%lx / 0x%lx\n", g_ps_hw6_owner_sm_probe.usb_reclaim_devdisconnect_status, g_ps_hw6_owner_sm_probe.usb_reclaim_disconnect_status, g_ps_hw6_owner_sm_probe.usb_reclaim_fxlx_close_status
printf "dcd extra timing telemetry = reverted to FW4 transfer_request.c; use endpoint transfer fields below\n\n"

printf "--- USBX storage-thread SCSI trace ---\n"
printf "live phase/recv ux/cmpl/req/act/len/lun/sig/cblen/op/cmd/csw_send = %lu / 0x%lx / 0x%lx / %lu / %lu / %lu / %lu / 0x%lx / %lu / 0x%lx / 0x%lx / 0x%lx\n", g_usbx_scsi_live_phase, g_usbx_scsi_live_receive_status, g_usbx_scsi_live_receive_completion, g_usbx_scsi_live_receive_requested, g_usbx_scsi_live_receive_actual, g_usbx_scsi_live_length, g_usbx_scsi_live_lun, g_usbx_scsi_live_cbw_signature, g_usbx_scsi_live_cbwcb_length, g_usbx_scsi_live_opcode, g_usbx_scsi_live_cmd_status, g_usbx_scsi_live_csw_send_status
printf "media callbacks rd/wr/fl/status = %lu / %lu / %lu / %lu\n", g_usbd_storage_read_entry_count, g_usbd_storage_write_entry_count, g_usbd_storage_flush_entry_count, g_usbd_storage_status_entry_count
printf "media read last lba/blocks/status/media = %lu / %lu / 0x%lx / 0x%lx\n", g_usbd_storage_read_last_lba, g_usbd_storage_read_last_blocks, g_usbd_storage_read_last_status, g_usbd_storage_read_last_media_status
printf "media write last lba/blocks/status/media = %lu / %lu / 0x%lx / 0x%lx\n", g_usbd_storage_write_last_lba, g_usbd_storage_write_last_blocks, g_usbd_storage_write_last_status, g_usbd_storage_write_last_media_status
printf "media flush last lba/blocks/status/media = %lu / %lu / 0x%lx / 0x%lx\n", g_usbd_storage_flush_last_lba, g_usbd_storage_flush_last_blocks, g_usbd_storage_flush_last_status, g_usbd_storage_flush_last_media_status
printf "media status last status/media = 0x%lx / 0x%lx\n", g_usbd_storage_status_last_status, g_usbd_storage_status_last_media_status
printf "cbw/last_op/flags/host_len/unknown/op/csw = %lu / 0x%lx / 0x%lx / %lu / %lu / 0x%lx / 0x%lx\n", g_usbx_scsi_cbw_count, g_usbx_scsi_last_opcode, g_usbx_scsi_last_cbw_flags, g_usbx_scsi_last_host_length, g_usbx_scsi_unknown_count, g_usbx_scsi_unknown_opcode, g_usbx_scsi_last_csw_status
printf "trace wr/count = %lu / %lu\n", g_usbx_scsi_trace_wr, g_usbx_scsi_trace_count
set $i = 0
while $i < 16
  printf "trace[%02u] op/host/flags/cmd/csw/send/res/sense/out st/cmpl/req/act = 0x%lx / %lu / 0x%lx / 0x%lx / 0x%lx / 0x%lx / %lu / 0x%lx / 0x%lx / 0x%lx / %lu / %lu\n", $i, g_usbx_scsi_trace_opcode[$i], g_usbx_scsi_trace_host_len[$i], g_usbx_scsi_trace_flags[$i], g_usbx_scsi_trace_cmd_status[$i], g_usbx_scsi_trace_csw_status[$i], g_usbx_scsi_trace_csw_send_status[$i], g_usbx_scsi_trace_residue[$i], g_usbx_scsi_trace_sense[$i], g_usbx_scsi_trace_out_status[$i], g_usbx_scsi_trace_out_completion[$i], g_usbx_scsi_trace_out_requested[$i], g_usbx_scsi_trace_out_actual[$i]
  set $i = $i + 1
end
printf "\n"
printf "--- ThreadX scheduler state ---\n"
printf "current/execute/system/preempt/highest = %p / %p / %lu / %u / %u\n", _tx_thread_current_ptr, _tx_thread_execute_ptr, _tx_thread_system_state, _tx_thread_preempt_disable, _tx_thread_highest_priority
if _tx_thread_current_ptr != 0
  printf "current name/state/prio/preempt/run/susp/susp_cb = %s / %u / %u / %u / %lu / %u / %p\n", _tx_thread_current_ptr->tx_thread_name, _tx_thread_current_ptr->tx_thread_state, _tx_thread_current_ptr->tx_thread_priority, _tx_thread_current_ptr->tx_thread_preempt_threshold, _tx_thread_current_ptr->tx_thread_run_count, _tx_thread_current_ptr->tx_thread_suspending, _tx_thread_current_ptr->tx_thread_suspend_control_block
end
if _tx_thread_execute_ptr != 0
  printf "execute name/state/prio/preempt/run/susp/susp_cb = %s / %u / %u / %u / %lu / %u / %p\n", _tx_thread_execute_ptr->tx_thread_name, _tx_thread_execute_ptr->tx_thread_state, _tx_thread_execute_ptr->tx_thread_priority, _tx_thread_execute_ptr->tx_thread_preempt_threshold, _tx_thread_execute_ptr->tx_thread_run_count, _tx_thread_execute_ptr->tx_thread_suspending, _tx_thread_execute_ptr->tx_thread_suspend_control_block
end
printf "\n"
printf "--- USBX system pointers/status ---\n"
printf "_ux_system_slave = %p\n", _ux_system_slave
printf "slave speed/power/remote wake cap/en = %lu / %lu / %lu / %lu\n", _ux_system_slave->ux_system_slave_speed, _ux_system_slave->ux_system_slave_power_state, _ux_system_slave->ux_system_slave_remote_wakeup_capability, _ux_system_slave->ux_system_slave_remote_wakeup_enabled
printf "framework lengths active/fs/hs/string/lang = %lu / %lu / %lu / %lu / %lu\n", _ux_system_slave->ux_system_slave_device_framework_length, _ux_system_slave->ux_system_slave_device_framework_length_full_speed, _ux_system_slave->ux_system_slave_device_framework_length_high_speed, _ux_system_slave->ux_system_slave_string_framework_length, _ux_system_slave->ux_system_slave_language_id_framework_length
printf "class array = %p interface class[0] = %p\n", _ux_system_slave->ux_system_slave_class_array, _ux_system_slave->ux_system_slave_interface_class_array[0]

printf "--- USBX DCD/device state ---\n"
printf "dcd status/type/irq/io/address/hw = 0x%x / 0x%x / 0x%x / 0x%lx / %lu / %p\n", _ux_system_slave->ux_system_slave_dcd.ux_slave_dcd_status, _ux_system_slave->ux_system_slave_dcd.ux_slave_dcd_controller_type, _ux_system_slave->ux_system_slave_dcd.ux_slave_dcd_irq, _ux_system_slave->ux_system_slave_dcd.ux_slave_dcd_io, _ux_system_slave->ux_system_slave_dcd.ux_slave_dcd_device_address, _ux_system_slave->ux_system_slave_dcd.ux_slave_dcd_controller_hardware
printf "device state/config/power = 0x%lx / %lu / %lu\n", _ux_system_slave->ux_system_slave_device.ux_slave_device_state, _ux_system_slave->ux_system_slave_device.ux_slave_device_configuration_selected, _ux_system_slave->ux_system_slave_device.ux_slave_device_power_state
printf "interfaces first/pool/count = %p / %p / %lu\n", _ux_system_slave->ux_system_slave_device.ux_slave_device_first_interface, _ux_system_slave->ux_system_slave_device.ux_slave_device_interfaces_pool, _ux_system_slave->ux_system_slave_device.ux_slave_device_interfaces_pool_number
printf "endpoints pool/count = %p / %lu\n", _ux_system_slave->ux_system_slave_device.ux_slave_device_endpoints_pool, _ux_system_slave->ux_system_slave_device.ux_slave_device_endpoints_pool_number
printf "control ep status/state addr/attr/mps = 0x%lx / 0x%lx / 0x%x / 0x%x / %u\n", _ux_system_slave->ux_system_slave_device.ux_slave_device_control_endpoint.ux_slave_endpoint_status, _ux_system_slave->ux_system_slave_device.ux_slave_device_control_endpoint.ux_slave_endpoint_state, _ux_system_slave->ux_system_slave_device.ux_slave_device_control_endpoint.ux_slave_endpoint_descriptor.bEndpointAddress, _ux_system_slave->ux_system_slave_device.ux_slave_device_control_endpoint.ux_slave_endpoint_descriptor.bmAttributes, _ux_system_slave->ux_system_slave_device.ux_slave_device_control_endpoint.ux_slave_endpoint_descriptor.wMaxPacketSize

set $if0 = _ux_system_slave->ux_system_slave_device.ux_slave_device_first_interface
printf "--- interface 0 / endpoints ---\n"
printf "if0 = %p\n", $if0
if $if0 != 0
  printf "if0 status/class/instance/next/first_ep = 0x%lx / %p / %p / %p / %p\n", $if0->ux_slave_interface_status, $if0->ux_slave_interface_class, $if0->ux_slave_interface_class_instance, $if0->ux_slave_interface_next_interface, $if0->ux_slave_interface_first_endpoint
  printf "if0 desc number/alt/eps/class/sub/proto = %u / %u / %u / 0x%x / 0x%x / 0x%x\n", $if0->ux_slave_interface_descriptor.bInterfaceNumber, $if0->ux_slave_interface_descriptor.bAlternateSetting, $if0->ux_slave_interface_descriptor.bNumEndpoints, $if0->ux_slave_interface_descriptor.bInterfaceClass, $if0->ux_slave_interface_descriptor.bInterfaceSubClass, $if0->ux_slave_interface_descriptor.bInterfaceProtocol
  set $ep0 = $if0->ux_slave_interface_first_endpoint
  printf "ep0 = %p\n", $ep0
  if $ep0 != 0
    printf "ep0 status/state addr/attr/mps/next = 0x%lx / 0x%lx / 0x%x / 0x%x / %u / %p\n", $ep0->ux_slave_endpoint_status, $ep0->ux_slave_endpoint_state, $ep0->ux_slave_endpoint_descriptor.bEndpointAddress, $ep0->ux_slave_endpoint_descriptor.bmAttributes, $ep0->ux_slave_endpoint_descriptor.wMaxPacketSize, $ep0->ux_slave_endpoint_next_endpoint
    printf "ep0 transfer xfer_status/completion/actual/requested/timeout = 0x%lx / 0x%lx / %lu / %lu / %lu\n", $ep0->ux_slave_endpoint_transfer_request.ux_slave_transfer_request_status, $ep0->ux_slave_endpoint_transfer_request.ux_slave_transfer_request_completion_code, $ep0->ux_slave_endpoint_transfer_request.ux_slave_transfer_request_actual_length, $ep0->ux_slave_endpoint_transfer_request.ux_slave_transfer_request_requested_length, $ep0->ux_slave_endpoint_transfer_request.ux_slave_transfer_request_timeout
    printf "ep0 semaphore count/suspended = %lu / %u\n", $ep0->ux_slave_endpoint_transfer_request.ux_slave_transfer_request_semaphore.tx_semaphore_count, $ep0->ux_slave_endpoint_transfer_request.ux_slave_transfer_request_semaphore.tx_semaphore_suspended_count
    set $ep1 = $ep0->ux_slave_endpoint_next_endpoint
    printf "ep1 = %p\n", $ep1
    if $ep1 != 0
      printf "ep1 status/state addr/attr/mps/next = 0x%lx / 0x%lx / 0x%x / 0x%x / %u / %p\n", $ep1->ux_slave_endpoint_status, $ep1->ux_slave_endpoint_state, $ep1->ux_slave_endpoint_descriptor.bEndpointAddress, $ep1->ux_slave_endpoint_descriptor.bmAttributes, $ep1->ux_slave_endpoint_descriptor.wMaxPacketSize, $ep1->ux_slave_endpoint_next_endpoint
      printf "ep1 transfer xfer_status/completion/actual/requested/timeout = 0x%lx / 0x%lx / %lu / %lu / %lu\n", $ep1->ux_slave_endpoint_transfer_request.ux_slave_transfer_request_status, $ep1->ux_slave_endpoint_transfer_request.ux_slave_transfer_request_completion_code, $ep1->ux_slave_endpoint_transfer_request.ux_slave_transfer_request_actual_length, $ep1->ux_slave_endpoint_transfer_request.ux_slave_transfer_request_requested_length, $ep1->ux_slave_endpoint_transfer_request.ux_slave_transfer_request_timeout
      printf "ep1 semaphore count/suspended = %lu / %u\n", $ep1->ux_slave_endpoint_transfer_request.ux_slave_transfer_request_semaphore.tx_semaphore_count, $ep1->ux_slave_endpoint_transfer_request.ux_slave_transfer_request_semaphore.tx_semaphore_suspended_count
      printf "--- last BOT/transfer buffers ---\n"
      set $bulk_in = (unsigned char *)$ep0->ux_slave_endpoint_transfer_request.ux_slave_transfer_request_data_pointer
      set $bulk_out = (unsigned char *)$ep1->ux_slave_endpoint_transfer_request.ux_slave_transfer_request_data_pointer
      printf "bulk IN data ptr = %p first32:\n", $bulk_in
      x/32xb $bulk_in
      printf "bulk OUT CBW ptr = %p first31:\n", $bulk_out
      x/31xb $bulk_out
      printf "CBW flags/lun/cblen/op/cdb1..5 = 0x%x / %u / %u / 0x%x / 0x%x 0x%x 0x%x 0x%x 0x%x\n", $bulk_out[12], $bulk_out[13], $bulk_out[14], $bulk_out[15], $bulk_out[16], $bulk_out[17], $bulk_out[18], $bulk_out[19], $bulk_out[20]
    end
  end
end

printf "--- storage class instance ---\n"
if _ux_system_slave->ux_system_slave_class_array != 0
  set $cls = _ux_system_slave->ux_system_slave_class_array
  printf "class status/instance/thread_stack/ifnum/cfg = 0x%x / %p / %p / %lu / %lu\n", $cls->ux_slave_class_status, $cls->ux_slave_class_instance, $cls->ux_slave_class_thread_stack, $cls->ux_slave_class_interface_number, $cls->ux_slave_class_configuration_number
  printf "class thread name/state/prio/preempt/run/susp/susp_cb/susp_status = %s / %u / %u / %u / %lu / %u / %p / 0x%x\n", $cls->ux_slave_class_thread.tx_thread_name, $cls->ux_slave_class_thread.tx_thread_state, $cls->ux_slave_class_thread.tx_thread_priority, $cls->ux_slave_class_thread.tx_thread_preempt_threshold, $cls->ux_slave_class_thread.tx_thread_run_count, $cls->ux_slave_class_thread.tx_thread_suspending, $cls->ux_slave_class_thread.tx_thread_suspend_control_block, $cls->ux_slave_class_thread.tx_thread_suspend_status
  if $cls->ux_slave_class_instance != 0
    set $st = (UX_SLAVE_CLASS_STORAGE *)$cls->ux_slave_class_instance
    printf "storage iface/luns/host_len/cbw_flags/cbw_lun/tag/residue/csw = %p / %lu / %lu / 0x%x / %u / 0x%lx / %lu / 0x%lx\n", $st->ux_slave_class_storage_interface, $st->ux_slave_class_storage_number_lun, $st->ux_slave_class_storage_host_length, $st->ux_slave_class_storage_cbw_flags, $st->ux_slave_class_storage_cbw_lun, $st->ux_slave_class_storage_scsi_tag, $st->ux_slave_class_storage_csw_residue, $st->ux_slave_class_storage_csw_status
    printf "storage ids vendor/product/rev/serial = %p / %p / %p / %p\n", $st->ux_slave_class_storage_vendor_id, $st->ux_slave_class_storage_product_id, $st->ux_slave_class_storage_product_rev, $st->ux_slave_class_storage_product_serial
    printf "lun0 lba/block/type/removable/readonly/media_id/sense/disk = %lu / %lu / %lu / %lu / %lu / %lu / 0x%lx / 0x%lx\n", $st->ux_slave_class_storage_lun[0].ux_slave_class_storage_media_last_lba, $st->ux_slave_class_storage_lun[0].ux_slave_class_storage_media_block_length, $st->ux_slave_class_storage_lun[0].ux_slave_class_storage_media_type, $st->ux_slave_class_storage_lun[0].ux_slave_class_storage_media_removable_flag, $st->ux_slave_class_storage_lun[0].ux_slave_class_storage_media_read_only_flag, $st->ux_slave_class_storage_lun[0].ux_slave_class_storage_media_id, $st->ux_slave_class_storage_lun[0].ux_slave_class_storage_request_sense_status, $st->ux_slave_class_storage_lun[0].ux_slave_class_storage_disk_status
  end
end

printf "--- STM32 PCD / OTG registers ---\n"
printf "HAL PCD state/error = 0x%x / 0x%lx\n", hpcd_USB_OTG_FS.State, hpcd_USB_OTG_FS.ErrorCode
printf "GAHBCFG/GUSBCFG/GRSTCTL/GINTSTS/GINTMSK = 0x%08lx / 0x%08lx / 0x%08lx / 0x%08lx / 0x%08lx\n", hpcd_USB_OTG_FS.Instance->GAHBCFG, hpcd_USB_OTG_FS.Instance->GUSBCFG, hpcd_USB_OTG_FS.Instance->GRSTCTL, hpcd_USB_OTG_FS.Instance->GINTSTS, hpcd_USB_OTG_FS.Instance->GINTMSK
printf "OTG device/endpoint typed-register detail omitted: debugger has no USB_OTG_FS_DEVICE symbol.\n"
printf "=== END HW6 FW0 USBX / PCD STATE PRINT ===\n"