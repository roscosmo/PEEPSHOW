set pagination off
printf "--- HW6 development V2 awake object scene ---\n"
if (g_ps_object_development_probe.api_version != 4) || (g_ps_scene_runtime_probe.api_version != 22) || (g_ps_audio_package_probe.api_version != 1)
  printf "Probe mismatch: expected development/scene/audio-package APIs 4/22/1.\n"
else
  printf "request / launches / launch status = %u / %u / 0x%x\n", g_ps_object_development_request, g_ps_object_development_probe.launch_count, g_ps_object_development_probe.launch_status
  printf "launch blockers = 0x%x (lease=1 overlay/workflow=2 audio=4 display=8 page=16 clock=32)\n", g_ps_object_development_probe.admission_blockers
  printf "scene active / development / scene / state / activation = %u / %u / %u / %u / %u\n", g_ps_scene_runtime_probe.active, s_ps_scene_runtime_development_objects, g_ps_scene_runtime_probe.scene_id, g_ps_scene_runtime_probe.state_id, s_ps_scene_runtime_scene_activation
  printf "display request / consumed / result / queue / wait / lease fault = %u / %u / 0x%x / 0x%x / 0x%x / %u\n", g_ps_object_development_probe.render_request, g_ps_object_development_probe.render_complete, g_ps_object_development_probe.render_status, g_ps_object_development_probe.queue_status, g_ps_object_development_probe.wait_status, g_ps_object_development_probe.lease_fault
  printf "last projected elapsed ms / next tick / state / first asset = %u / %u / %u / %u\n", g_ps_object_development_probe.elapsed_ms, g_ps_object_development_probe.next_tick, g_ps_object_development_probe.state_id, g_ps_object_development_probe.first_asset_id
  printf "first object phase / remaining ms / animation visible = %u / %u / %u\n", s_ps_object_snapshot.objects[0].step, s_ps_object_snapshot.objects[0].remaining_ms, s_ps_object_snapshot.objects[0].animation_visible
  if s_ps_object_snapshot.count > 8
    printf "slow indicator phase / remaining ms / animation visible = %u / %u / %u\n", s_ps_object_snapshot.objects[8].step, s_ps_object_snapshot.objects[8].remaining_ms, s_ps_object_snapshot.objects[8].animation_visible
  end
  printf "last projected marker x/y = %d / %d\n", s_ps_object_snapshot.objects[1].effective.x, s_ps_object_snapshot.objects[1].effective.y
  if s_ps_object_snapshot.count >= 3
    printf "timer marker visible/x/y (after 2s: 1/76/116) = %u / %d / %d\n", (s_ps_object_snapshot.objects[2].effective.flags & 1), s_ps_object_snapshot.objects[2].effective.x, s_ps_object_snapshot.objects[2].effective.y
  end
  printf "timer configured/active/paused due/applied/ignored/error = %u / %u / %u / %u / %u / %u / %u\n", g_ps_hw6_rtos_probe.runtime_state_timer_configured_count, g_ps_hw6_rtos_probe.runtime_state_timer_active_count, g_ps_hw6_rtos_probe.runtime_state_timer_paused, g_ps_hw6_rtos_probe.runtime_state_timer_due_count, g_ps_hw6_rtos_probe.runtime_state_timer_applied_count, g_ps_hw6_rtos_probe.runtime_state_timer_ignored_count, g_ps_hw6_rtos_probe.runtime_state_timer_error_count
  printf "input events / matched transitions / state changes = %u / %u / %u\n", g_ps_hw6_rtos_probe.runtime_input_event_count, g_ps_scene_runtime_probe.transition_match_count, g_ps_scene_runtime_probe.state_change_count
  printf "UI page / runtime class / life / display renders / success = %u / %u / %u / %u / %u\n", g_ps_ui_router_probe.current_page, g_ps_hw6_rtos_probe.runtime_current_class, g_ps_hw6_rtos_probe.runtime_lifecycle, g_ps_hw6_owner_probe.display_ui_render_count, g_ps_hw6_owner_probe.display_success
  printf "STOP2 entries = %u (must not increase while this development scene is active)\n", g_ps_hw6_rtos_probe.stop2_auto_entry_count
  printf "package audio blocked/fault request/complete/status = %u / %u / %u / %u / 0x%x\n", g_ps_audio_package_probe.blocked, g_ps_audio_package_probe.fault, g_ps_audio_package_probe.request, g_ps_audio_package_probe.complete, g_ps_audio_package_probe.status
  printf "package audio send/wait/owner discarded queued = 0x%x / 0x%x / 0x%x / %u\n", g_ps_audio_package_probe.send_status, g_ps_audio_package_probe.wait_status, g_ps_audio_package_probe.owner_status, g_ps_audio_package_probe.discarded
  printf "SFX dispatch/send owner/status outstanding/clock held = %u / 0x%x / %u / 0x%x / %u / %u\n", g_ps_hw6_rtos_probe.audio_sfx_dispatch_count, g_ps_hw6_rtos_probe.audio_sfx_send_status, g_ps_hw6_rtos_probe.audio_sfx_owner_count, g_ps_hw6_rtos_probe.audio_sfx_owner_status, g_ps_hw6_rtos_probe.audio_sfx_outstanding_count, g_ps_hw6_rtos_probe.audio_sfx_clock_held
  printf "SFX voices active/peak/completed refills/decoded/underrun = %u / %u / %u / %u / %u / %u\n", g_ps_hw6_owner_probe.audio_sfx_voice_active_count, g_ps_hw6_owner_probe.audio_sfx_voice_peak_count, g_ps_hw6_owner_probe.audio_sfx_voice_complete_count, g_ps_hw6_owner_probe.audio_sfx_stream_refill_count, g_ps_hw6_owner_probe.audio_sfx_decoded_samples, g_ps_hw6_owner_probe.audio_sfx_stream_underrun_count
  printf "Expected: active/development=1/1, launch/result/queue/wait/fault=0, UI/class/life=6/2/2. Consumed alone is not proof of drawing: require result=0, display success=1 and visible movement.\n"
  printf "Structured OS fixture: state 1/2 selects marker x=32/120, y=68, inside the B/A slots. Top digits at (72,12) cycle every 400 ms. A/B must preserve phase and remaining frame time.\n"
  printf "Current dual fixture also has a four-cell indicator at (72,44), advancing every 800 ms. A/B and timer reveal must preserve both clips.\n"
  printf "At 2 seconds the scene timer reveals the third square once, without state entry or animation restart; digit 2 is due at expiry. The square stays visible through later A/B changes.\n"
  printf "No audio or L/R controls are authored. Audio counters may describe an earlier installed package. HOLD START opens the shell; reset ends the development session.\n"
  printf "If halted during a transfer, request/consumed and result may still be incomplete. Resume before judging that as a failed render.\n"
end
