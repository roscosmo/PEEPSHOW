echo --- HW6 installed scene-exit SFX bench ---\n
if (g_ps_scene_runtime_probe.api_version != 22) || (g_ps_audio_package_probe.api_version != 1)
  echo Probe mismatch: use the matching ELF.\n
else
  printf "installed bytes / scene / replacement attempts/failures/status = %u / %u / %u/%u/0x%x\n", g_ps_egg_state_loader_probe.package_size, g_ps_scene_runtime_probe.scene_id, g_ps_scene_runtime_probe.scene_replace_count, g_ps_scene_runtime_probe.scene_replace_fail_count, g_ps_scene_runtime_probe.scene_replace_status
  printf "committed SFX / last cue = %u / %u\n", g_ps_scene_runtime_probe.sfx_action_commit_count, g_ps_scene_runtime_probe.last_sfx_cue_index
  printf "dispatch/send owner/status outstanding/clock = %u/0x%x %u/0x%x %u/%u\n", g_ps_hw6_rtos_probe.audio_sfx_dispatch_count, g_ps_hw6_rtos_probe.audio_sfx_send_status, g_ps_hw6_rtos_probe.audio_sfx_owner_count, g_ps_hw6_rtos_probe.audio_sfx_owner_status, g_ps_hw6_rtos_probe.audio_sfx_outstanding_count, g_ps_hw6_rtos_probe.audio_sfx_clock_held
  printf "blocked/fault stop request/complete/status = %u/%u %u/%u/0x%x\n", g_ps_audio_package_probe.blocked, g_ps_audio_package_probe.fault, g_ps_audio_package_probe.request, g_ps_audio_package_probe.complete, g_ps_audio_package_probe.status
  printf "voices active / decoded / underruns = %u / %u / %u\n", g_ps_hw6_owner_probe.audio_sfx_voice_active_count, g_ps_hw6_owner_probe.audio_sfx_decoded_samples, g_ps_hw6_owner_probe.audio_sfx_stream_underrun_count
  echo Fixture: 54660 bytes. HOME=1, AWAY=2. A HOME->AWAY plays one six-second cue; B AWAY->HOME plays one short cue.\n
  echo L/R and HOME reveal are silent. A in AWAY and B in HOME do nothing. AWAY has no automatic return.\n
  echo During the long cue B must change scene without stopping it. HOLD START must discard it; Resume remains silent.\n
  echo Observe before halting, never halt during playback. After drain require no outstanding/clock-held/underrun/fault. Counters do not prove audible output or current.\n
end
