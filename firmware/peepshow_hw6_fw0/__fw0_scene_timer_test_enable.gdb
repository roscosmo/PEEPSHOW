set pagination off
set $ps_scene_timer_test_armed = 0
printf "--- HW6 scene-owned timer test ---\n"
if (g_ps_hw6_rtos_probe.version != 82) || (g_ps_scene_runtime_probe.api_version != 21)
  printf "NOT armed: this helper requires RTOS/scene APIs 82/21. Check the flashed ELF.\n"
else
  set $scene = s_ps_scene_runtime_state_scene
  if (g_ps_scene_runtime_probe.active == 0) || (g_ps_hw6_rtos_probe.runtime_current_class != 2) || (g_ps_hw6_rtos_probe.runtime_lifecycle != 2)
    printf "NOT armed: launch a running STATE scene first.\n"
  else
    if ($scene->event_binding_count >= 16) || ($scene->transition_count >= 16) || ($scene->variable_count >= 8) || ($scene->action_count >= 32)
      printf "NOT armed: this test needs one spare binding, handler, variable and action.\n"
    else
      set $b = $scene->event_binding_count
      set $t = $scene->transition_count
      set $a = $scene->action_count
      set $v = $scene->variable_count
      set $variable_id = 1
      set $binding_id = 1
      set $transition_id = 1
      set $i = 0
      while $i < $v
        if $scene->variables[$i].variable_id >= $variable_id
          set $variable_id = $scene->variables[$i].variable_id + 1
        end
        set $i = $i + 1
      end
      set $i = 0
      while $i < $b
        if $scene->event_bindings[$i].binding_id >= $binding_id
          set $binding_id = $scene->event_bindings[$i].binding_id + 1
        end
        set $i = $i + 1
      end
      set $i = 0
      while $i < $t
        if $scene->transitions[$i].transition_id >= $transition_id
          set $transition_id = $scene->transitions[$i].transition_id + 1
        end
        set $i = $i + 1
      end
      set $scene->variables[$v].variable_id = $variable_id
      set $scene->variables[$v].value_type = 1
      set $scene->variables[$v].initial_value = 0
      set s_ps_scene_runtime_variables[$v] = 0
      set $scene->event_bindings[$b].binding_id = $binding_id
      set $scene->event_bindings[$b].event_class = 2
      set $scene->event_bindings[$b].event_kind = 2
      set $scene->event_bindings[$b].source = 0
      set $scene->event_bindings[$b].parameter = 5000
      set $scene->actions[$a].kind = 1
      set $scene->actions[$a].target_id = $variable_id
      set $scene->actions[$a].target_element_id = 0
      set $scene->actions[$a].operation = 2
      set $scene->actions[$a].value = 1
      set $scene->actions[$a].secondary_value = 0
      set $scene->transitions[$t].transition_id = $transition_id
      set $scene->transitions[$t].source_state_id = 0
      set $scene->transitions[$t].scene_event_id = $b + 1
      set $scene->transitions[$t].first_guard = 0
      set $scene->transitions[$t].guard_count = 0
      set $scene->transitions[$t].first_action = $a
      set $scene->transitions[$t].action_count = 1
      set $scene->transitions[$t].target_state_id = 0
      set $scene->transitions[$t].target_scene_id = 0
      set $scene->variable_count = $v + 1
      set $scene->action_count = $a + 1
      set $scene->event_binding_count = $b + 1
      set $scene->transition_count = $t + 1
      set g_ps_scene_runtime_probe.descriptor_variable_count = $v + 1
      set g_ps_scene_runtime_probe.descriptor_action_count = $a + 1
      set g_ps_scene_runtime_probe.descriptor_event_binding_count = $b + 1
      set g_ps_scene_runtime_probe.descriptor_transition_count = $t + 1
      set ps_runtime_state_timers[$b].configured = 0
      set $ps_scene_timer_test_owner = s_ps_scene_runtime_scene_activation
      set $ps_scene_timer_test_state = s_ps_scene_runtime_state_activation
      set $ps_scene_timer_test_binding = $b
      set $ps_scene_timer_test_variable = $v
      set $ps_scene_timer_test_applied = g_ps_hw6_rtos_probe.runtime_state_timer_applied_count
      set $ps_scene_timer_test_errors = g_ps_hw6_rtos_probe.runtime_state_timer_error_count
      set $ps_scene_timer_test_rtc = g_ps_hw6_rtos_probe.runtime_state_timer_rtc_select_count
      set $ps_scene_timer_test_armed = 1
      printf "Injected a 5-second scene timer and an independent counter-increment handler in RAM.\n"
      printf "No screen cue or automatic scene change is expected. Resume and move the menu once to wake it.\n"
      printf "Change selections within this same scene during the first few seconds, then leave it idle.\n"
      printf "After at least 7 seconds, halt and source __fw0_scene_timer_test_prints.gdb.\n"
      printf "The private test counter must be 1; selection changes must not restart the timer.\n"
      printf "Do not launch another scene or halt during audio. Reset/reload discards this RAM-only test.\n"
    end
  end
end
