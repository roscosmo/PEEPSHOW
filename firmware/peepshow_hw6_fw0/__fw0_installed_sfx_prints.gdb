echo --- HW6 installed resident V2 SFX ---\n
if (g_ps_scene_runtime_probe.api_version != 22) || (g_ps_audio_package_probe.api_version != 1)
  echo Probe mismatch: expected scene/audio-package APIs 22/1. Use the matching ELF.\n
else
  printf "source/model/active scene/state = %u/%u/%u %u/%u\n", g_ps_scene_runtime_probe.package_source, s_ps_scene_runtime_state_scene->execution_model, g_ps_scene_runtime_probe.active, g_ps_scene_runtime_probe.scene_id, g_ps_scene_runtime_probe.state_id
  printf "loaded bytes / audio assets/cues/ADPCM bytes/package-backed = %u / %u/%u/%u/%u\n", g_ps_egg_state_loader_probe.package_size, g_ps_egg_state_loader_probe.audio_asset_count, g_ps_egg_state_loader_probe.audio_cue_count, g_ps_egg_state_loader_probe.audio_adpcm_bytes, g_ps_egg_state_loader_probe.audio_package_backed
  printf "package audio blocked/fault request/complete/status = %u/%u %u/%u/0x%x\n", g_ps_audio_package_probe.blocked, g_ps_audio_package_probe.fault, g_ps_audio_package_probe.request, g_ps_audio_package_probe.complete, g_ps_audio_package_probe.status
  printf "stop send/wait/owner discarded = 0x%x/0x%x/0x%x %u\n", g_ps_audio_package_probe.send_status, g_ps_audio_package_probe.wait_status, g_ps_audio_package_probe.owner_status, g_ps_audio_package_probe.discarded
  printf "SFX dispatch/send owner/status outstanding/clock held = %u/0x%x %u/0x%x %u/%u\n", g_ps_hw6_rtos_probe.audio_sfx_dispatch_count, g_ps_hw6_rtos_probe.audio_sfx_send_status, g_ps_hw6_rtos_probe.audio_sfx_owner_count, g_ps_hw6_rtos_probe.audio_sfx_owner_status, g_ps_hw6_rtos_probe.audio_sfx_outstanding_count, g_ps_hw6_rtos_probe.audio_sfx_clock_held
  printf "voices active/peak/completed refills/decoded/underrun = %u/%u/%u %u/%u/%u\n", g_ps_hw6_owner_probe.audio_sfx_voice_active_count, g_ps_hw6_owner_probe.audio_sfx_voice_peak_count, g_ps_hw6_owner_probe.audio_sfx_voice_complete_count, g_ps_hw6_owner_probe.audio_sfx_stream_refill_count, g_ps_hw6_owner_probe.audio_sfx_decoded_samples, g_ps_hw6_owner_probe.audio_sfx_stream_underrun_count
  echo OS fixture: 54696 bytes, HOME=1 AWAY=2, two resident audio assets/cues; package-backed=0.\n
  echo L/R selection changes play 80ms tones. HOME reveals its bottom square after 2s and starts a 6s tone. A enters AWAY; B returns HOME. Exits play no new cue. AWAY has no timer or automatic return.\n
  echo HOLD START during the long tone: it must stop early. Resume must remain silent; L/R can play fresh tones. Do not halt during playback.\n
  echo After drain: require audible playback, decoded/refill work, zero underruns/faults/outstanding/clock-held. Shell stop requires blocked=1, request=complete and stop statuses=0; Resume clears blocked without replay.\n
  echo Counts are cumulative. Dispatch alone is not audible playback; no current measurement is implied. Use the installed-object print for display and sleep evidence.\n
end
