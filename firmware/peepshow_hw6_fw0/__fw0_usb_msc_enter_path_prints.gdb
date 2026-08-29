set pagination off

printf "\n--- HW6 USB MSC enter path ---\n"
printf "Use while halted after selecting USB MSC from MENU or pressing A on the package browser when MSC did not enumerate. Read-only; do not reset first.\n"

printf "\nUI router:\n"
printf "page previous/requested/package = %u / %u / %u / %u\n", g_ps_ui_router_probe.current_page, g_ps_ui_router_probe.previous_page, g_ps_ui_router_probe.requested_page, g_ps_ui_router_probe.package_state
printf "page/package/pending action/count/last action = %u / %u / %u / %u / %u\n", g_ps_ui_router_probe.current_page, g_ps_ui_router_probe.package_state, g_ps_ui_router_probe.pending_action, g_ps_ui_router_probe.action_request_count, g_ps_ui_router_probe.last_action
printf "button event/count/last focus = %u / %u / %u / %u\n", g_ps_ui_router_probe.button_event_count, g_ps_ui_router_probe.last_button_event, g_ps_ui_router_probe.last_event, g_ps_ui_router_probe.focus_index
printf "router status/rejected/taken/transitions = 0x%x / %u / %u / %u\n", g_ps_ui_router_probe.last_status, g_ps_ui_router_probe.rejected_event_count, g_ps_ui_router_probe.action_take_count, g_ps_ui_router_probe.transition_count

printf "\nRTOS UI action dispatch:\n"
printf "ui action last/count/status msc enter/exit = %u / %u / 0x%x / %u / %u\n", g_ps_hw6_rtos_probe.ui_action_last, g_ps_hw6_rtos_probe.ui_action_count, g_ps_hw6_rtos_probe.ui_action_send_status, g_ps_hw6_rtos_probe.ui_action_msc_enter_count, g_ps_hw6_rtos_probe.ui_action_msc_exit_count
printf "msc exit intercept count = %u\n", g_ps_hw6_rtos_probe.ui_action_msc_exit_intercept_count
printf "admission action/result/reason/status = %u / %u / %u / 0x%x\n", g_ps_hw6_rtos_probe.admission_last_action, g_ps_hw6_rtos_probe.admission_last_result, g_ps_hw6_rtos_probe.admission_last_reason, g_ps_hw6_rtos_probe.admission_last_status
printf "admission runtime class/lifecycle ui page/package/shutdown/overlay = %u / %u / %u / %u / %u / %u\n", g_ps_hw6_rtos_probe.admission_last_runtime_class, g_ps_hw6_rtos_probe.admission_last_runtime_lifecycle, g_ps_hw6_rtos_probe.admission_last_ui_page, g_ps_hw6_rtos_probe.admission_last_package_state, g_ps_hw6_rtos_probe.admission_last_shutdown_state, g_ps_hw6_rtos_probe.admission_last_overlay_active

printf "\nStorage command flags:\n"
printf "usb export/reclaim request flags = %u / %u\n", g_ps_hw6_storage_usb_export_request, g_ps_hw6_storage_usb_reclaim_request
printf "storage clock export/reclaim/release last status = 0x%x / 0x%x / 0x%x / 0x%x\n", g_ps_hw6_rtos_probe.storage_clock_export_status, g_ps_hw6_rtos_probe.storage_clock_reclaim_status, g_ps_hw6_rtos_probe.storage_clock_release_status, g_ps_hw6_rtos_probe.storage_clock_last_status
printf "export req/tick policy/dcd/init/start = %u / %u / 0x%x / 0x%x / 0x%x / 0x%x\n", g_ps_hw6_owner_sm_probe.usb_export_request_count, g_ps_hw6_owner_sm_probe.usb_export_start_tick, g_ps_hw6_owner_sm_probe.usb_export_policy_status, g_ps_hw6_owner_sm_probe.usb_export_dcd_status, g_ps_hw6_owner_sm_probe.usb_export_pcd_init_status, g_ps_hw6_owner_sm_probe.usb_export_pcd_start_status
printf "fxlx msc api/status/stage/active open/close = %u / 0x%x / %u / %u / %u / %u\n", g_ps_storage_filex_levelx_msc_probe.api_version, g_ps_storage_filex_levelx_msc_probe.status, g_ps_storage_filex_levelx_msc_probe.last_stage, g_ps_storage_filex_levelx_msc_probe.active, g_ps_storage_filex_levelx_msc_probe.open_count, g_ps_storage_filex_levelx_msc_probe.close_count

printf "\nUSB availability:\n"
printf "state/event/update/tick ext/data/avail/active = %u / %u / %u / %u / %u / %u / %u / %u\n", g_ps_hw6_owner_sm_probe.usb_host_availability_state, g_ps_hw6_owner_sm_probe.usb_host_availability_event, g_ps_hw6_owner_sm_probe.usb_host_availability_update_count, g_ps_hw6_owner_sm_probe.usb_host_availability_tick, g_ps_hw6_owner_sm_probe.usb_host_external_power_present, g_ps_hw6_owner_sm_probe.usb_host_data_seen, g_ps_hw6_owner_sm_probe.usb_host_msc_available, g_ps_hw6_owner_sm_probe.usb_host_msc_active

printf "\nexpected after selecting USB MSC from MENU or pressing A on package browser: page=PACKAGE_BROWSER, button last=17, action MSC_ENTER=1, ui action msc enter increments, admission status=0, usb export request becomes nonzero, and export policy/dcd/init/start leave 0xffffffff once storage runs.\n"
printf "expected after pressing B while MSC is active: action MSC_EXIT=2, ui action msc exit increments, reclaim request becomes nonzero.\n"
printf "actions: NONE=0 MSC_ENTER=1 MSC_EXIT=2 PACKAGE_INSTALL=3 PACKAGE_LAUNCH=4\n"
printf "admission results: DENY=0 ALLOW=1 ALLOW_AFTER_SUSPEND=2; reasons include UI_SHELL=1 SYSTEM_BUSY=2 RUNTIME_SUSPENDED=3 SEND_FAILED=4 UNSUPPORTED=5\n"
printf "--- end USB MSC enter path ---\n"
