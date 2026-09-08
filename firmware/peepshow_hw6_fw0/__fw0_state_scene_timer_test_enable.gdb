set pagination off
set $scene = s_ps_scene_runtime_state_scene
set $candidate = -1
set $fallback_state = -1
set $index = 0
set $timer_delay_ms = 5000
set $new_binding_id = 1
set $new_transition_id = 1

printf "--- HW6 temporary STATE timer injection ---\n"
if (g_ps_scene_runtime_probe.active == 0) || ($scene == 0)
  printf "Timer NOT armed: launch a STATE scene first.\n"
else
  if ($scene->event_binding_count >= 16) || ($scene->transition_count >= 16)
    printf "Timer NOT armed: the active scene has no spare binding or transition slot.\n"
  else
    while ($index < $scene->event_binding_count)
      if $scene->event_bindings[$index].binding_id >= $new_binding_id
        set $new_binding_id = $scene->event_bindings[$index].binding_id + 1
      end
      set $index = $index + 1
    end
    set $index = 0
    while ($index < $scene->transition_count)
      if $scene->transitions[$index].transition_id >= $new_transition_id
        set $new_transition_id = $scene->transitions[$index].transition_id + 1
      end
      if ($candidate < 0) && ($scene->transitions[$index].source_state_id == g_ps_scene_runtime_probe.state_id) && (($scene->transitions[$index].target_scene_id != 0) || ($scene->transitions[$index].target_state_id != g_ps_scene_runtime_probe.state_id))
        set $candidate = $index
      end
      set $index = $index + 1
    end
    set $index = 0
    while ($index < $scene->state_count)
      if ($fallback_state < 0) && ($scene->states[$index].state_id != g_ps_scene_runtime_probe.state_id)
        set $fallback_state = $scene->states[$index].state_id
      end
      set $index = $index + 1
    end

    set $binding_index = $scene->event_binding_count
    set $transition_index = $scene->transition_count
    set $scene->event_bindings[$binding_index].binding_id = $new_binding_id
    set $scene->event_bindings[$binding_index].event_class = 2
    set $scene->event_bindings[$binding_index].event_kind = 1
    set $scene->event_bindings[$binding_index].source = 0
    set $scene->event_bindings[$binding_index].parameter = $timer_delay_ms

    if $candidate >= 0
      set $scene->transitions[$transition_index] = $scene->transitions[$candidate]
    else
      set $scene->transitions[$transition_index].target_scene_id = 0
      if $fallback_state >= 0
        set $scene->transitions[$transition_index].target_state_id = $fallback_state
      else
        set $scene->transitions[$transition_index].target_state_id = g_ps_scene_runtime_probe.state_id
      end
    end
    set $scene->transitions[$transition_index].transition_id = $new_transition_id
    set $scene->transitions[$transition_index].source_state_id = g_ps_scene_runtime_probe.state_id
    set $scene->transitions[$transition_index].scene_event_id = $binding_index + 1
    set $scene->transitions[$transition_index].first_guard = 0
    set $scene->transitions[$transition_index].guard_count = 0
    set $scene->transitions[$transition_index].first_action = 0
    set $scene->transitions[$transition_index].action_count = 0
    set $scene->event_binding_count = $binding_index + 1
    set $scene->transition_count = $transition_index + 1
    set g_ps_scene_runtime_probe.descriptor_event_binding_count = $scene->event_binding_count
    set g_ps_scene_runtime_probe.descriptor_transition_count = $scene->transition_count
    set ps_runtime_state_timer_scene_revision = g_ps_scene_runtime_probe.state_revision + 1
    set ps_runtime_state_timer_paused = 0

    if $candidate >= 0
      printf "Timer armed in RAM: delay=%u ms event slot=%u cloned destination=%u source state=%u.\n", $timer_delay_ms, $binding_index, $candidate, g_ps_scene_runtime_probe.state_id
      printf "Resume normally. The cloned destination should appear after the delay, including across STOP2.\n"
    else
      if $fallback_state >= 0
        printf "Timer armed in RAM: delay=%u ms event slot=%u target state=%u source state=%u.\n", $timer_delay_ms, $binding_index, $fallback_state, g_ps_scene_runtime_probe.state_id
        printf "Resume normally. The alternate state should appear after the delay, including across STOP2.\n"
      else
        printf "Timer armed in RAM: delay=%u ms event slot=%u self-transition source state=%u.\n", $timer_delay_ms, $binding_index, g_ps_scene_runtime_probe.state_id
        printf "Resume normally. No visible scene change is guaranteed; halt after one interval and use the timer print helper for proof.\n"
      end
    end
  end
end
printf "A reset discards this injection and restores the installed package unchanged.\n"
printf "--- end HW6 temporary STATE timer injection ---\n"
