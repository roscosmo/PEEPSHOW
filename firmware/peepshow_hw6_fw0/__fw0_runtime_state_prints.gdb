set pagination off

set $rt = &g_ps_hw6_rtos_probe
set $ui = &g_ps_ui_router_probe

printf "--- HW6 runtime host scaffold ---\n"
printf "rtos api/init/runtime = %u / %u / %u\n", $rt->version, $rt->init_complete, $rt->runtime_complete
printf "runtime class prev/current/return = %u / %u / %u\n", $rt->runtime_previous_class, $rt->runtime_current_class, $rt->runtime_return_class
printf "runtime exec/lifecycle = %u / %u\n", $rt->runtime_execution, $rt->runtime_lifecycle
printf "runtime event/count/status/tick = %u / %u / 0x%x / %u\n", $rt->runtime_last_event, $rt->runtime_event_count, $rt->runtime_last_status, $rt->runtime_last_tick
printf "runtime active pkg/unit = %u / %u\n", $rt->runtime_active_package_id, $rt->runtime_active_unit_id
printf "runtime return page     = %u\n", $rt->runtime_return_page
printf "runtime counts boot/installer enter/done/err = %u / %u / %u / %u\n", $rt->runtime_boot_shell_count, $rt->runtime_installer_enter_count, $rt->runtime_installer_complete_count, $rt->runtime_installer_error_count
printf "runtime counts pkg act/return suspend/resume = %u / %u / %u / %u\n", $rt->runtime_package_activate_stub_count, $rt->runtime_package_return_count, $rt->runtime_suspend_count, $rt->runtime_resume_count
printf "runtime request count/status = %u / 0x%x\n", $rt->runtime_owner_request_count, $rt->runtime_owner_request_status
printf "runtime clock req/rel/reason/caps/status = %u / %u / %u / 0x%x / 0x%x\n", $rt->runtime_clock_request_count, $rt->runtime_clock_release_count, $rt->runtime_clock_last_reason, $rt->runtime_clock_last_capabilities, $rt->runtime_clock_last_status
printf "runtime clock reactive/realtime/release = 0x%x / 0x%x / 0x%x\n", $rt->runtime_clock_reactive_status, $rt->runtime_clock_realtime_status, $rt->runtime_clock_release_status
printf "runtime queue rx/timeout/error = %u / %u / %u\n", $rt->queue_receive_count[8], $rt->queue_timeout_count[8], $rt->queue_message_error_count[8]
printf "ui page/package/shutdown = %u / %u / %u\n", $ui->current_page, $ui->package_state, $ui->shutdown_state
printf "classes: NONE=0 SHELL=1 LP_GRAPH=2 LP_MODULE=3 RT_SCENE=4 INSTALLER=5\n"
printf "exec: NONE=0 REACTIVE=1 REALTIME=2\n"
printf "lifecycle: NONE=0 MOUNTED=1 RUNNING=2 SUSPENDED=3 STOPPING=4 ERROR=5\n"
printf "events: BOOT_SHELL=1 INSTALLER_ENTER=2 INSTALLER_COMPLETE=3 INSTALLER_ERROR=4 PKG_ACTIVATE_STUB=5 PKG_RETURN=6 SUSPEND=7 RESUME=8\n"
printf "runtime clock reasons: NONE=0 REACTIVE_TXN=1 REALTIME_DEADLINE=2 RELEASE=3\n"
printf "pages: BOOT=0 HOME=1 MENU=2 SETTINGS=3 CAL=4 PACKAGES=5 RUNTIME=6 ERROR=7 SHUTDOWN=8\n"
