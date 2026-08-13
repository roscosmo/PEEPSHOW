set pagination off
set $rt = &g_ps_hw6_rtos_probe
set $ui = &g_ps_ui_router_probe
printf "--- HW6 system action admission scaffold ---\n"
printf "rtos api/runtime        = %u / %u\n", $rt->version, $rt->runtime_complete
printf "admission api/req/allow/deny/suspend/resume = %u / %u / %u / %u / %u / %u\n", $rt->admission_api_version, $rt->admission_request_count, $rt->admission_allow_count, $rt->admission_deny_count, $rt->admission_suspend_count, $rt->admission_resume_count
printf "last action/result/reason/status/tick = %u / %u / %u / 0x%x / %u\n", $rt->admission_last_action, $rt->admission_last_result, $rt->admission_last_reason, $rt->admission_last_status, $rt->admission_last_tick
printf "last runtime/lifecycle/ui/pkg/shutdown/overlay = %u / %u / %u / %u / %u / %u\n", $rt->admission_last_runtime_class, $rt->admission_last_runtime_lifecycle, $rt->admission_last_ui_page, $rt->admission_last_package_state, $rt->admission_last_shutdown_state, $rt->admission_last_overlay_active
printf "power suspend by-system/action resume reason/status = %u / %u / %u / 0x%x\n", $rt->admission_runtime_suspended_by_system, $rt->admission_runtime_suspended_action, $rt->admission_runtime_resume_reason, $rt->admission_runtime_resume_status
printf "runtime class/exec/life/suspend/resume = %u / %u / %u / %u / %u\n", $rt->runtime_current_class, $rt->runtime_execution, $rt->runtime_lifecycle, $rt->runtime_suspend_count, $rt->runtime_resume_count
printf "runtime request count/status = %u / 0x%x\n", $rt->runtime_owner_request_count, $rt->runtime_owner_request_status
printf "ui page/package/shutdown = %u / %u / %u\n", $ui->current_page, $ui->package_state, $ui->shutdown_state
printf "actions: NONE=0 MSC_ENTER=1 MSC_EXIT=2 PKG_INSTALL=3 POWER_START_SHUT_PREP=4 POWER_BATT_CRIT_PREP=5 POWER_BOOT_LOW_PREP=6\n"
printf "results: DENY=0 ALLOW=1 ALLOW_AFTER_SUSPEND=2\n"
printf "reasons: NONE=0 UI_SHELL=1 OVERLAY=2 RUNTIME_SUSPENDED=3 INSTALLER=4 BUSY=5 SEND_FAILED=6 UNSUPPORTED=7\n"
printf "resume reasons: NONE=0 START_CANCEL=1\n"
printf "status: TX_SUCCESS=0x0 TX_NOT_DONE=0x20 NOT_RUN=0xffffffff\n"
