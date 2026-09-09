set pagination off
if (ps_dev_audio_stream_active != 0) || (g_ps_hw6_rtos_probe.audio_sfx_clock_held != 0)
  printf "--- HW6 SFX clock profile NOT changed: audio is active ---\n"
else
  set var g_ps_hw6_clock_audio_mix_profile_override = 5
  printf "--- HW6 next STATE SFX mix will use IO_HIGH 160 MHz ---\n"
end
