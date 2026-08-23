set pagination off
set $discard = g_ps_package_source_probe.api_version
set $source_api = g_ps_package_source_probe.api_version
set $discard = g_ps_package_source_override
set $source_override = g_ps_package_source_override
set $discard = g_ps_package_source_probe.selected_source
set $source_selected = g_ps_package_source_probe.selected_source
set $discard = g_ps_package_source_probe.resolve_count
set $source_resolve = g_ps_package_source_probe.resolve_count
set $discard = g_ps_package_source_probe.success_count
set $source_success = g_ps_package_source_probe.success_count
set $discard = g_ps_package_source_probe.unavailable_count
set $source_unavailable = g_ps_package_source_probe.unavailable_count
set $discard = g_ps_package_source_probe.last_status
set $source_status = g_ps_package_source_probe.last_status
set $discard = g_ps_package_source_probe.reason
set $source_reason = g_ps_package_source_probe.reason
set $discard = g_ps_package_source_probe.package_size
set $source_bytes = g_ps_package_source_probe.package_size
set $discard = g_ps_scene_runtime_probe.api_version
set $scene_api = g_ps_scene_runtime_probe.api_version
set $discard = g_ps_scene_runtime_probe.active
set $scene_active = g_ps_scene_runtime_probe.active
set $discard = g_ps_scene_runtime_probe.last_status
set $scene_status = g_ps_scene_runtime_probe.last_status
set $discard = g_ps_scene_runtime_probe.activation_status
set $scene_activation = g_ps_scene_runtime_probe.activation_status
set $discard = g_ps_scene_runtime_probe.package_source
set $scene_source = g_ps_scene_runtime_probe.package_source
set $discard = g_ps_scene_runtime_probe.package_source_status
set $scene_source_status = g_ps_scene_runtime_probe.package_source_status
set $discard = g_ps_ui_router_probe.api_version
set $ui_api = g_ps_ui_router_probe.api_version
set $discard = g_ps_ui_router_probe.current_page
set $ui_page = g_ps_ui_router_probe.current_page
set $discard = g_ps_ui_router_probe.eggless
set $ui_eggless = g_ps_ui_router_probe.eggless
set $discard = g_ps_ui_router_probe.last_status
set $ui_status = g_ps_ui_router_probe.last_status
set $discard = g_ps_hw6_rtos_probe.runtime_current_class
set $runtime_class = g_ps_hw6_rtos_probe.runtime_current_class
set $discard = g_ps_hw6_rtos_probe.runtime_execution
set $runtime_exec = g_ps_hw6_rtos_probe.runtime_execution
set $discard = g_ps_hw6_rtos_probe.runtime_lifecycle
set $runtime_lifecycle = g_ps_hw6_rtos_probe.runtime_lifecycle
set $discard = g_ps_hw6_rtos_probe.runtime_active_capabilities
set $runtime_caps = g_ps_hw6_rtos_probe.runtime_active_capabilities
set $discard = g_ps_hw6_rtos_probe.runtime_admission_last_status
set $runtime_admission = g_ps_hw6_rtos_probe.runtime_admission_last_status
set $discard = g_ps_hw6_rtos_probe.runtime_last_status
set $runtime_status = g_ps_hw6_rtos_probe.runtime_last_status
set $discard = g_ps_hw6_owner_probe.display_ui_page
set $display_page = g_ps_hw6_owner_probe.display_ui_page
set $discard = g_ps_hw6_owner_probe.display_ui_render_count
set $display_render_count = g_ps_hw6_owner_probe.display_ui_render_count
printf "--- HW6 no-package source fallback ---\n"
printf "source api/override/selected = %u / %u / %u\n", $source_api, $source_override, $source_selected
printf "source resolve/success/unavailable = %u / %u / %u\n", $source_resolve, $source_success, $source_unavailable
printf "source status/reason/bytes = 0x%x / %u / %u\n", $source_status, $source_reason, $source_bytes
printf "scene api/active/status/activation = %u / %u / 0x%x / 0x%x\n", $scene_api, $scene_active, $scene_status, $scene_activation
printf "scene source/status = %u / 0x%x\n", $scene_source, $scene_source_status
printf "ui api/page/eggless/status = %u / %u / %u / 0x%x\n", $ui_api, $ui_page, $ui_eggless, $ui_status
printf "runtime class/exec/lifecycle = %u / %u / %u\n", $runtime_class, $runtime_exec, $runtime_lifecycle
printf "runtime caps/admission/status = 0x%x / 0x%x / 0x%x\n", $runtime_caps, $runtime_admission, $runtime_status
printf "display page/render count = %u / %u\n", $display_page, $display_render_count
printf "expected source: api=2 override=1 selected=0 status=1 reason=2 bytes=0\n"
printf "expected scene: api=10 active=0 activation=2 source=0 (last status may be 0 after shell rendering)\n"
printf "expected ui: api=9 page=1 eggless=1 status=0\n"
printf "expected runtime: SHELL/REACTIVE/RUNNING, capabilities=0, admission=TX_NO_INSTANCE\n"
printf "--- end HW6 no-package source fallback ---\n"
