set pagination off
printf "--- HW6 development V2 awake object scene ---\n"
if (g_ps_object_development_probe.api_version != 1) || (g_ps_scene_runtime_probe.api_version != 22)
  printf "Probe mismatch: expected development/scene APIs 1/22.\n"
else
  printf "request / launches / launch status = %u / %u / 0x%x\n", g_ps_object_development_request, g_ps_object_development_probe.launch_count, g_ps_object_development_probe.launch_status
  printf "launch blockers = 0x%x (lease=1 overlay/workflow=2 audio=4 display=8 page=16 clock=32)\n", g_ps_object_development_probe.admission_blockers
  printf "scene active / development / scene / state / activation = %u / %u / %u / %u / %u\n", g_ps_scene_runtime_probe.active, s_ps_scene_runtime_development_objects, g_ps_scene_runtime_probe.scene_id, g_ps_scene_runtime_probe.state_id, s_ps_scene_runtime_scene_activation
  printf "display request / consumed / result / queue / wait / lease fault = %u / %u / 0x%x / 0x%x / 0x%x / %u\n", g_ps_object_development_probe.render_request, g_ps_object_development_probe.render_complete, g_ps_object_development_probe.render_status, g_ps_object_development_probe.queue_status, g_ps_object_development_probe.wait_status, g_ps_object_development_probe.lease_fault
  printf "last projected elapsed ms / next tick / state / first asset = %u / %u / %u / %u\n", g_ps_object_development_probe.elapsed_ms, g_ps_object_development_probe.next_tick, g_ps_object_development_probe.state_id, g_ps_object_development_probe.first_asset_id
  printf "first object phase / remaining ms / animation visible = %u / %u / %u\n", s_ps_object_snapshot.objects[0].step, s_ps_object_snapshot.objects[0].remaining_ms, s_ps_object_snapshot.objects[0].animation_visible
  printf "last projected marker x/y = %d / %d\n", s_ps_object_snapshot.objects[1].effective.x, s_ps_object_snapshot.objects[1].effective.y
  printf "input events / matched transitions / state changes = %u / %u / %u\n", g_ps_hw6_rtos_probe.runtime_input_event_count, g_ps_scene_runtime_probe.transition_match_count, g_ps_scene_runtime_probe.state_change_count
  printf "UI page / runtime class / life / display renders / success = %u / %u / %u / %u / %u\n", g_ps_ui_router_probe.current_page, g_ps_hw6_rtos_probe.runtime_current_class, g_ps_hw6_rtos_probe.runtime_lifecycle, g_ps_hw6_owner_probe.display_ui_render_count, g_ps_hw6_owner_probe.display_success
  printf "STOP2 entries = %u (must not increase while this development scene is active)\n", g_ps_hw6_rtos_probe.stop2_auto_entry_count
  printf "Expected: active/development=1/1, launch/result/queue/wait/fault=0, UI/class/life=6/2/2. Consumed alone is not proof of drawing: require result=0, display success=1 and visible movement.\n"
  printf "Studio fixture: state 1/2 selects marker x=32/120, y=104. First asset cycles 65537..65538 every 500 ms; A then B must preserve sprite phase and remaining frame time. B returns to state 1, not the shell.\n"
  printf "If halted during a transfer, request/consumed and result may still be incomplete. Resume before judging that as a failed render.\n"
end
