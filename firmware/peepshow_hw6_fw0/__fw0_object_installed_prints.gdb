set pagination off
printf "--- HW6 installed V2 object scene ---\n"
if g_ps_hw6_rtos_probe.version != 82 || g_ps_scene_runtime_probe.api_version != 22 || g_ps_object_candidate_probe.api_version != 1
  printf "Probe mismatch: expected RTOS/scene/candidate APIs 82/22/1. Check the loaded ELF.\n"
else
  printf "runtime stack bytes / start / end / saved SP / saved lower margin = %u / 0x%x / 0x%x / 0x%x / %u\n", ps_threads[8].tx_thread_stack_size, ps_threads[8].tx_thread_stack_start, ps_threads[8].tx_thread_stack_end, ps_threads[8].tx_thread_stack_ptr, (unsigned int)ps_threads[8].tx_thread_stack_ptr - (unsigned int)ps_threads[8].tx_thread_stack_start
  printf "Stack size must be 4096 for this build. Saved margin is a context-switch snapshot, not a worst-case high-water measurement.\n"
  printf "RTOS init status / pool available before / after = 0x%x / %u / %u\n", g_ps_hw6_rtos_probe.init_status, g_ps_hw6_rtos_probe.pool_available_before, g_ps_hw6_rtos_probe.pool_available_after
  printf "source / scene active / activation / execution model / installed bytes = %u / %u / 0x%x / %u / %u\n", g_ps_scene_runtime_probe.package_source, g_ps_scene_runtime_probe.active, g_ps_scene_runtime_probe.activation_status, s_ps_scene_runtime_state_scene->execution_model, s_ps_installed_object_size
  printf "index available / slot / generation / address / bytes = %u / %u / %u / 0x%x / %u\n", g_ps_storage_package_index_probe.installed_available, g_ps_storage_package_index_probe.selected_slot, g_ps_storage_package_index_probe.selected_generation, g_ps_storage_package_index_probe.selected_package_start, g_ps_storage_package_index_probe.selected_package_size
  printf "UI page / runtime class / lifecycle = %u / %u / %u\n", g_ps_ui_router_probe.current_page, g_ps_hw6_rtos_probe.runtime_current_class, g_ps_hw6_rtos_probe.runtime_lifecycle
  printf "latest admission token / complete / lease / status = %u / %u / %u / 0x%x\n", g_ps_object_candidate_probe.request_id, g_ps_object_candidate_probe.display_complete, g_ps_object_candidate_probe.leased, g_ps_object_candidate_probe.status
  printf "profile / reason / schedule / display / chunks / bytes = 0x%x / %u / 0x%x / 0x%x / %u / %u\n", g_ps_object_candidate_probe.profile_status, g_ps_object_candidate_probe.profile_reason, g_ps_object_candidate_probe.schedule_status, g_ps_object_candidate_probe.display_status, g_ps_object_candidate_probe.chunks, g_ps_object_candidate_probe.bytes
  printf "LPBAM enabled / fault / publish / steps / quantum = %u / %u / 0x%x / %u / %u\n", g_ps_object_lpbam_probe.enabled, g_ps_object_lpbam_probe.fault, g_ps_object_lpbam_probe.publish_status, g_ps_object_lpbam_prepare_probe.step_count, g_ps_object_lpbam_prepare_probe.quantum_ms
  printf "WFI returns / measured / reconciled / clock status = %u / %u / %u / 0x%x\n", g_ps_object_lpbam_probe.physical_count, g_ps_object_lpbam_probe.sleep_count, g_ps_object_lpbam_probe.reconciled_count, g_ps_object_lpbam_probe.clock_status
  printf "timer due / applied / error / RTC selects = %u / %u / %u / %u\n", g_ps_hw6_rtos_probe.runtime_state_timer_due_count, g_ps_hw6_rtos_probe.runtime_state_timer_applied_count, g_ps_hw6_rtos_probe.runtime_state_timer_error_count, g_ps_hw6_rtos_probe.runtime_state_timer_rtc_select_count
  printf "display request / complete / result / lease fault = %u / %u / 0x%x / %u\n", g_ps_object_development_probe.render_request, g_ps_object_development_probe.render_complete, g_ps_object_development_probe.render_status, g_ps_object_development_probe.lease_fault
  if s_ps_installed_object_size != 0 && s_ps_object_snapshot.count >= 3
    printf "last digit phase / remaining ms / marker x / reveal = %u / %u / %d / %u\n", s_ps_object_snapshot.objects[0].step, s_ps_object_snapshot.objects[0].remaining_ms, s_ps_object_snapshot.objects[1].effective.x, (s_ps_object_snapshot.objects[2].effective.flags & 1)
  end
  printf "GUI e7f11f0 fixture: source=3, active=1, activation=0, model=2, installed/index bytes=2196; UI/class/life=6/2/2 while playing. No development enable helper is required.\n"
  printf "Digits 1-2-3-4 at 400 ms, marker x=32(B) or 120(A), fixed outlines; bottom square appears once after 2 seconds. Require observed continuity, not just queue counters.\n"
  printf "Admission fields describe the latest transaction. Snapshots are not live DMA frames. In-flight display results may be NOT_RUN; reboot intentionally to test boot loading, not merely reconnect GDB.\n"
end
