# PeepShow authoritative breakpoint set.
# Keep total active breakpoints <= 5.

set pagination off
set confirm off
set $ps_run_mark_valid = 0

# Primary fault traps.
break HardFault_Handler
break Error_Handler

# Power-thread event helpers.
# Usage:
#   ps_smoke      (recommended one-shot check)
#   ps_mode_flags
#   ps_power_flags
#   ps_power_perf
#   ps_quiesce
#   ps_timeout
#   ps_resume
#   ps_ack
#   ps_display_probe
#   ps_spi_status
#   ps_invalidate
#   ps_present
#   ps_mode_stop
#   ps_mode_static
#   ps_mode_realtime
#   ps_mode_flashing
#   ps_mode_verify_stop
#   ps_mode_verify_static
#   ps_mode_verify_realtime
#   ps_mode_verify_flashing
#   ps_sensor_health
#   ps_resume_sensor
#   ps_sensor_poll_all
#   ps_sensor_cfg_all
#   ps_sensor_snapshot
#   ps_sensor_poll_all_wait
#   ps_sensor_cfg_all_wait
#   ps_sensor_lis_autorecover
#   ps_pmic_diag
#   ps_lis_diag
#   ps_lis_stream_smoke_static
#   ps_lis_stream_smoke_realtime
#   ps_lis_stream_smoke_end
#   ps_sensor_policy
#   ps_sensor_mode_token
#   ps_joy_cal_status
#   ps_joy_cal_start
#   ps_joy_cal_start_wait
#   ps_joy_cal_save
#   ps_joy_cal_save_wait
#   ps_storage_status
#   ps_storage_probe
#   ps_storage_probe_wait
#   ps_storage_smoke
#   ps_storage_smoke_wait
#   ps_storage_filex_mount
#   ps_storage_filex_mount_wait
#   ps_storage_filex_format
#   ps_storage_filex_format_wait
#   ps_storage_filex_unmount
#   ps_storage_filex_unmount_wait
#   ps_input_status
#   ps_audio_status
#   ps_input_latch
#   ps_input_reset
#   ps_input_snap
#   ps_display_stack
#   ps_cal_stall_dump
#   ps_joy_diag
#   ps_audio_start
#   ps_audio_start_wait
#   ps_audio_stop
#   ps_audio_stop_wait
#   ps_audio_power_smoke
#   ps_mark_runs
#   ps_delta_runs
#   ps_perf_mark
#   ps_perf_delta

define __ps_wait_power_updates
  set $__ps_n = (int)$arg0
  set $__ps_i = 0
  while $__ps_i < $__ps_n
    tbreak AppPowerFlagsUpdate
    continue
    finish
    set $__ps_i = $__ps_i + 1
  end
end

define ps_mode_flags
  printf "egMode =0x%08lx\n", (unsigned long)g_eg_mode.tx_event_flags_group_current
end

define ps_power_flags
  printf "egPower=0x%08lx\n", (unsigned long)g_eg_power.tx_event_flags_group_current
end

define ps_power_perf
  printf "== Power perf ==\n"
  printf "profile: current=%lu target=%lu last_switch_tick=%lu\n", (unsigned long)g_power_perf_profile_current, (unsigned long)g_power_perf_profile_target, (unsigned long)g_power_perf_last_switch_tick
  printf "hint: seq=%lu post=%lu drop=%lu rx=%lu last_present_ticks=%lu\n", (unsigned long)g_power_perf_hint_seq, (unsigned long)g_power_perf_hint_post_count, (unsigned long)g_power_perf_hint_drop_count, (unsigned long)g_power_perf_hint_rx_count, (unsigned long)g_power_perf_last_present_ticks
  printf "streak: miss=%lu headroom=%lu up=%lu down=%lu dwell_block=%lu\n", (unsigned long)g_power_perf_miss_streak, (unsigned long)g_power_perf_headroom_streak, (unsigned long)g_power_perf_up_count, (unsigned long)g_power_perf_down_count, (unsigned long)g_power_perf_dwell_block_count
  printf "knobs: stride=%lu budget=%lu miss_margin=%lu headroom_margin=%lu up_streak=%lu down_streak=%lu dwell=%lu\n", (unsigned long)KNOB_POWER_PERF_HINT_STRIDE, (unsigned long)KNOB_POWER_PERF_FRAME_BUDGET_TICKS, (unsigned long)KNOB_POWER_PERF_MISS_MARGIN_TICKS, (unsigned long)KNOB_POWER_PERF_HEADROOM_MARGIN_TICKS, (unsigned long)KNOB_POWER_PERF_UP_STREAK_FRAMES, (unsigned long)KNOB_POWER_PERF_DOWN_STREAK_FRAMES, (unsigned long)KNOB_POWER_PERF_MIN_DWELL_TICKS
end

define ps_quiesce
  set $ps_rc = (unsigned int)App_SysEvent_QuiesceReq()
  printf "queue QUIESCE rc=%u\n", $ps_rc
  __ps_wait_power_updates 2
  printf "after QUIESCE egPower=0x%08lx\n", (unsigned long)g_eg_power.tx_event_flags_group_current
end

define ps_resume
  set $ps_rc = (unsigned int)App_SysEvent_ResumeReq()
  printf "queue RESUME rc=%u\n", $ps_rc
  __ps_wait_power_updates 1
  printf "after RESUME egPower=0x%08lx\n", (unsigned long)g_eg_power.tx_event_flags_group_current
end

define ps_ack
  set $ps_rc = (unsigned int)App_SysEvent_QuiesceAck(31)
  printf "queue QUIESCE_ACK rc=%u mask=0x%08x\n", $ps_rc, 31
  __ps_wait_power_updates 1
  printf "after ACK egPower=0x%08lx\n", (unsigned long)g_eg_power.tx_event_flags_group_current
end

define ps_timeout
  set $ps_rc = (unsigned int)App_SysEvent_QuiesceReq()
  printf "queue QUIESCE rc=%u (timeout path)\n", $ps_rc
  __ps_wait_power_updates 1
  set g_power_pending_ack_mask = 0x80000000
  set g_power_quiesce_wait_active = 1
  set g_power_quiesce_wait_elapsed_ticks = 0
  set g_eg_power.tx_event_flags_group_current = (((unsigned long)g_eg_power.tx_event_flags_group_current & ~0x12UL) | 0x01UL)
  printf "after QUIESCE (forced pending) egPower=0x%08lx\n", (unsigned long)g_eg_power.tx_event_flags_group_current
  __ps_wait_power_updates 1
  printf "after TIMEOUT egPower=0x%08lx\n", (unsigned long)g_eg_power.tx_event_flags_group_current
end

define ps_smoke
  printf "== Power flag smoke ==\n"
  ps_mode_flags
  ps_power_flags
  ps_quiesce
  ps_resume
end

define ps_display_probe
  printf "== Display probe ==\n"
  tbreak AppDisplayPresent
  tbreak LCD_FlushAll
  set $ps_rc = (unsigned int)App_SysEvent_ResumeReq()
  printf "queue RESUME rc=%u\n", $ps_rc
  continue
  printf "hit AppDisplayPresent\n"
  continue
  printf "hit LCD_FlushAll\n"
  printf "hspi3.State=%u ErrorCode=0x%08lx\n", (unsigned int)hspi3.State, (unsigned long)hspi3.ErrorCode
end

define ps_invalidate
  set $ps_rc = (unsigned int)App_Display_InvalidateAll()
  printf "queue DISPLAY_INVALIDATE_ALL rc=%u\n", $ps_rc
end

define ps_present
  set $ps_rc = (unsigned int)App_Display_Present()
  printf "queue DISPLAY_PRESENT rc=%u\n", $ps_rc
end

define ps_mode_stop
  set $ps_rc = (unsigned int)App_SysEvent_ModeSet(0)
  printf "queue MODE_SET STOP rc=%u\n", $ps_rc
end

define ps_mode_static
  set $ps_rc = (unsigned int)App_SysEvent_ModeSet(1)
  printf "queue MODE_SET STATIC rc=%u\n", $ps_rc
end

define ps_mode_realtime
  set $ps_rc = (unsigned int)App_SysEvent_ModeSet(2)
  printf "queue MODE_SET REALTIME rc=%u\n", $ps_rc
end

define ps_mode_flashing
  set $ps_rc = (unsigned int)App_SysEvent_ModeSet(3)
  printf "queue MODE_SET FLASHING rc=%u\n", $ps_rc
end

define __ps_mode_verify
  set $__ps_mode = (unsigned int)$arg0
  set $__ps_q_before = (unsigned int)g_q_display_cmd.tx_queue_enqueued
  tbreak AppSetModeFlag
  set $ps_rc = (unsigned int)App_SysEvent_ModeSet($__ps_mode)
  printf "queue MODE_SET(%u) rc=%u qDisplay(before)=%u qSys(before)=%u qSensor(before)=%u\n", $__ps_mode, $ps_rc, $__ps_q_before, (unsigned int)g_q_sys_events.tx_queue_enqueued, (unsigned int)g_q_sensor_req.tx_queue_enqueued
  continue
  printf "hit AppSetModeFlag\n"
  finish
  printf "egMode =0x%08lx\n", (unsigned long)g_eg_mode.tx_event_flags_group_current
  printf "qDisplay(after)=%u qSys(after)=%u qSensor(after)=%u\n", (unsigned int)g_q_display_cmd.tx_queue_enqueued, (unsigned int)g_q_sys_events.tx_queue_enqueued, (unsigned int)g_q_sensor_req.tx_queue_enqueued
end

define ps_mode_verify_stop
  __ps_mode_verify 0
end

define ps_mode_verify_static
  __ps_mode_verify 1
end

define ps_mode_verify_realtime
  __ps_mode_verify 2
end

define ps_mode_verify_realtime_sync
  tbreak AppSensorHandleModeChange
  ps_mode_verify_realtime
  continue
  printf "hit AppSensorHandleModeChange\n"
  finish
  ps_sensor_mode_token
end

define ps_realtime_stream_start_safe
  ps_mode_verify_realtime_sync
  set $ps_rc = (unsigned int)App_SensorReq_LisStreamStart()
  printf "queue LIS_STREAM_START rc=%u\n", $ps_rc
end

define ps_mode_verify_flashing
  __ps_mode_verify 3
end

define ps_spi_status
  printf "== SPI status ==\n"
  tbreak HAL_SPI_Transmit
  set $ps_rc = (unsigned int)App_SysEvent_ResumeReq()
  printf "queue RESUME rc=%u\n", $ps_rc
  continue
  finish
  printf "hspi3.State=%u ErrorCode=0x%08lx\n", (unsigned int)hspi3.State, (unsigned long)hspi3.ErrorCode
end

define ps_sensor_health
  printf "== Sensor health ==\n"
  printf "egSensor=%lu\n", (unsigned long)g_eg_sensor_health.tx_event_flags_group_current
  printf "suspended_flags=%lu\n", (unsigned long)(g_eg_sensor_health.tx_event_flags_group_current & 0x00000380UL)
  printf "sensor_mode_token=%lu\n", (unsigned long)g_sensor_mode_token
  printf "pmic: state=%lu fails=%lu tries=%lu next=%lu ok=%lu err=%ld\n", (unsigned long)g_sensor_pmic.state, (unsigned long)g_sensor_pmic.fail_count, (unsigned long)g_sensor_pmic.recovery_attempts, (unsigned long)g_sensor_pmic.next_retry_tick, (unsigned long)g_sensor_pmic.last_success_tick, (long)g_sensor_pmic.last_error
  printf "tmag: state=%lu fails=%lu tries=%lu next=%lu ok=%lu err=%ld\n", (unsigned long)g_sensor_tmag.state, (unsigned long)g_sensor_tmag.fail_count, (unsigned long)g_sensor_tmag.recovery_attempts, (unsigned long)g_sensor_tmag.next_retry_tick, (unsigned long)g_sensor_tmag.last_success_tick, (long)g_sensor_tmag.last_error
  printf "lis : state=%lu fails=%lu tries=%lu next=%lu ok=%lu err=%ld\n", (unsigned long)g_sensor_lis.state, (unsigned long)g_sensor_lis.fail_count, (unsigned long)g_sensor_lis.recovery_attempts, (unsigned long)g_sensor_lis.next_retry_tick, (unsigned long)g_sensor_lis.last_success_tick, (long)g_sensor_lis.last_error
  printf "bus_fault=%lu\n", (unsigned long)g_sensor_bus_fault
  printf "joy: stage=%lu pending=%lu err=%ld load_ok=%lu load_fail=%lu save_ok=%lu save_fail=%lu progress_milli=%lu\n", (unsigned long)g_sensor_joy_cal_status.stage, (unsigned long)g_sensor_joy_cal_status.save_pending, (long)g_sensor_joy_cal_status.last_error, (unsigned long)g_sensor_joy_cal_status.load_ok_count, (unsigned long)g_sensor_joy_cal_status.load_fail_count, (unsigned long)g_sensor_joy_cal_status.save_ok_count, (unsigned long)g_sensor_joy_cal_status.save_fail_count, (unsigned long)(g_sensor_joy_cal_status.progress * 1000.0)
end

define ps_sensor_mode_token
  printf "sensor_mode_token=%lu egMode=0x%08lx\n", (unsigned long)g_sensor_mode_token, (unsigned long)g_eg_mode.tx_event_flags_group_current
end

define ps_sensor_policy
  set $ps_mode = (unsigned long)g_eg_mode.tx_event_flags_group_current
  set $ps_power = (unsigned long)g_eg_power.tx_event_flags_group_current
  set $ps_allow = 0
  if (($ps_power & 0x08UL) != 0UL) && (($ps_power & 0x03UL) == 0UL) && (($ps_mode & 0x0cUL) == 0UL) && (($ps_mode & 0x03UL) != 0UL)
    set $ps_allow = 1
  end
  printf "== Sensor policy ==\n"
  printf "egMode =0x%08lx egPower=0x%08lx\n", $ps_mode, $ps_power
  printf "autorecover_allowed=%u (allow in STOP/STATIC only)\n", (unsigned int)$ps_allow
end

define ps_resume_sensor
  printf "== Resume + Sensor Health ==\n"
  set $ps_rc = (unsigned int)App_SysEvent_ResumeReq()
  printf "queue RESUME rc=%u\n", $ps_rc
  if ((g_eg_mode.tx_event_flags_group_current & 0x08UL) != 0UL)
    printf "resume path suppressed in FLASHING mode\n"
  else
    tbreak AppSensorRunResumeSequence
    continue
    finish
  end
  ps_sensor_health
end

define ps_sensor_poll_all
  set $ps_rc = (unsigned int)App_SensorReq_Poll(7)
  printf "queue SENSOR_POLL_ALL rc=%u\n", $ps_rc
end

define ps_sensor_cfg_all
  set $ps_rc = (unsigned int)App_SensorReq_ConfigDefaults(7)
  printf "queue SENSOR_CONFIG_DEFAULTS_ALL rc=%u\n", $ps_rc
end

define ps_sensor_snapshot
  set $ps_rc = (unsigned int)App_SensorReq_HealthSnapshot()
  printf "queue SENSOR_HEALTH_SNAPSHOT rc=%u\n", $ps_rc
end

define ps_sensor_poll_all_wait
  if ((g_eg_mode.tx_event_flags_group_current & 0x0cUL) != 0UL)
    ps_sensor_poll_all
    printf "poll path suppressed in REALTIME/FLASHING mode\n"
  else
    tbreak AppSensorRunPollSequence
    ps_sensor_poll_all
    continue
    finish
  end
  ps_sensor_health
end

define ps_sensor_cfg_all_wait
  if ((g_eg_mode.tx_event_flags_group_current & 0x0cUL) != 0UL)
    ps_sensor_cfg_all
    printf "config-defaults path suppressed in REALTIME/FLASHING mode\n"
  else
    tbreak AppSensorApplyDefaults
    ps_sensor_cfg_all
    continue
    finish
  end
  ps_sensor_health
end

define ps_sensor_lis_autorecover
  printf "== Sensor LIS auto-recovery ==\n"
  ps_resume_sensor
  set g_sensor_lis.state = 3
  set g_sensor_lis.last_error = -399
  printf "forced lis: state=%lu fails=%lu err=%ld\n", (unsigned long)g_sensor_lis.state, (unsigned long)g_sensor_lis.fail_count, (long)g_sensor_lis.last_error
  tbreak AppSensorRunPollSequence
  continue
  finish
  ps_sensor_health
end

define ps_pmic_diag
  printf "== PMIC diag ==\n"
  printf "fsm: state=%lu fails=%lu tries=%lu next=%lu ok=%lu err=%ld\n", (unsigned long)g_sensor_pmic.state, (unsigned long)g_sensor_pmic.fail_count, (unsigned long)g_sensor_pmic.recovery_attempts, (unsigned long)g_sensor_pmic.next_retry_tick, (unsigned long)g_sensor_pmic.last_success_tick, (long)g_sensor_pmic.last_error
  printf "live: sample=%lu fail=%lu trans_fail=%lu fault_evt=%lu last_tick=%lu lerr=%ld lterr=%ld\n", (unsigned long)g_sensor_pmic_live.sample_count, (unsigned long)g_sensor_pmic_live.fail_count, (unsigned long)g_sensor_pmic_live.transport_error_count, (unsigned long)g_sensor_pmic_live.fault_event_count, (unsigned long)g_sensor_pmic_live.last_sample_tick, (long)g_sensor_pmic_live.last_error, (long)g_sensor_pmic_live.last_transport_error
  printf "vbat: mv=%lu raw=%lu soc=%lu(raw=%lu) status2=0x%02lx fault=0x%02lx pgood=0x%02lx\n", (unsigned long)g_sensor_pmic_live.vbat_mV, (unsigned long)g_sensor_pmic_live.vbat_raw, (unsigned long)g_sensor_pmic_live.battery_soc_percent, (unsigned long)g_sensor_pmic_live.battery_soc_raw, (unsigned long)g_sensor_pmic_live.status2_raw, (unsigned long)g_sensor_pmic_live.fault_raw, (unsigned long)g_sensor_pmic_live.pgood_raw
  printf "bat: health=%lu reason=0x%02lx\n", (unsigned long)g_sensor_pmic_live.battery_health_state, (unsigned long)g_sensor_pmic_live.battery_health_reason
  printf "chg: enabled_cfg=%lu active=%lu state=%lu last_fault=0x%02lx\n", (unsigned long)g_sensor_pmic_live.charging_enabled_cfg, (unsigned long)g_sensor_pmic_live.charging_active, (unsigned long)g_sensor_pmic_live.charger_state, (unsigned long)g_sensor_pmic_live.last_fault_mask
  printf "guard: en=%lu cutoff=%lu hys=%lu confirm=%lu streak=%lu latched=%lu isofet_off=%lu\n", (unsigned long)g_sensor_pmic_live.guard_enabled, (unsigned long)g_sensor_pmic_live.cutoff_mv, (unsigned long)g_sensor_pmic_live.cutoff_hys_mv, (unsigned long)g_sensor_pmic_live.cutoff_confirm_samples, (unsigned long)g_sensor_pmic_live.cutoff_low_streak, (unsigned long)g_sensor_pmic_live.cutoff_latched, (unsigned long)g_sensor_pmic_live.isofet_forced_off
end

define ps_lis_diag
  printf "== LIS diag ==\n"
  printf "fsm: state=%lu fails=%lu tries=%lu next=%lu ok=%lu err=%ld\n", (unsigned long)g_sensor_lis.state, (unsigned long)g_sensor_lis.fail_count, (unsigned long)g_sensor_lis.recovery_attempts, (unsigned long)g_sensor_lis.next_retry_tick, (unsigned long)g_sensor_lis.last_success_tick, (long)g_sensor_lis.last_error
  printf "stream: enabled=%lu\n", (unsigned long)g_sensor_lis_stream_enabled
  printf "profile: requested=%lu applied=%lu\n", (unsigned long)g_sensor_lis_profile_requested, (unsigned long)g_sensor_lis_profile_applied
  printf "live: addr=0x%02lx whoami=0x%02lx status=0x%02lx sample=%lu fail=%lu last_tick=%lu lerr=%ld\n", (unsigned long)g_sensor_lis_live.addr, (unsigned long)g_sensor_lis_live.whoami, (unsigned long)g_sensor_lis_live.status, (unsigned long)g_sensor_lis_live.sample_count, (unsigned long)g_sensor_lis_live.fail_count, (unsigned long)g_sensor_lis_live.last_sample_tick, (long)g_sensor_lis_live.last_error
  printf "raw: x=%d y=%d z=%d\n", (int)g_sensor_lis_live.x_raw, (int)g_sensor_lis_live.y_raw, (int)g_sensor_lis_live.z_raw
end

define ps_lis_stream_smoke_static
  printf "== LIS stream smoke: STATIC ==\n"
  ps_mode_verify_static
  set $__ps_mode_wait = 0
  while ((unsigned long)g_sensor_mode_token != 1) && ($__ps_mode_wait < 6)
    tbreak AppSensorHandleModeChange
    continue
    finish
    set $__ps_mode_wait = $__ps_mode_wait + 1
  end
  printf "sensor_mode_token=%lu egMode=0x%08lx\n", (unsigned long)g_sensor_mode_token, (unsigned long)g_eg_mode.tx_event_flags_group_current
  set $ps_rc = (unsigned int)App_SensorReq_LisStreamStop()
  printf "queue LIS_STREAM_STOP rc=%u\n", $ps_rc
  set $ps_rc = (unsigned int)App_SensorReq_LisSetLowPower()
  printf "queue LIS_SET_LOW_POWER rc=%u\n", $ps_rc
  set $ps_s0 = (unsigned long)g_sensor_lis_live.sample_count
  set $ps_t0 = (unsigned long)g_sensor_lis_live.last_sample_tick
  set $ps_rc = (unsigned int)App_SensorReq_LisStreamStart()
  printf "queue LIS_STREAM_START rc=%u\n", $ps_rc
  printf "now run target for 1-2s, then Ctrl-C, then run: ps_lis_stream_smoke_end\n"
end

define ps_lis_stream_smoke_realtime
  printf "== LIS stream smoke: REALTIME ==\n"
  ps_mode_verify_realtime
  set $__ps_mode_wait = 0
  while ((unsigned long)g_sensor_mode_token != 2) && ($__ps_mode_wait < 6)
    tbreak AppSensorHandleModeChange
    continue
    finish
    set $__ps_mode_wait = $__ps_mode_wait + 1
  end
  printf "sensor_mode_token=%lu egMode=0x%08lx\n", (unsigned long)g_sensor_mode_token, (unsigned long)g_eg_mode.tx_event_flags_group_current
  set $ps_rc = (unsigned int)App_SensorReq_LisStreamStop()
  printf "queue LIS_STREAM_STOP rc=%u\n", $ps_rc
  set $ps_rc = (unsigned int)App_SensorReq_LisSetLowPower()
  printf "queue LIS_SET_LOW_POWER rc=%u\n", $ps_rc
  set $ps_s0 = (unsigned long)g_sensor_lis_live.sample_count
  set $ps_t0 = (unsigned long)g_sensor_lis_live.last_sample_tick
  set $ps_rc = (unsigned int)App_SensorReq_LisStreamStart()
  printf "queue LIS_STREAM_START rc=%u\n", $ps_rc
  printf "now run target for 1-2s, then Ctrl-C, then run: ps_lis_stream_smoke_end\n"
end

define ps_lis_stream_smoke_end
  printf "== LIS stream smoke result ==\n"
  printf "sensor_mode_token=%lu egMode=0x%08lx\n", (unsigned long)g_sensor_mode_token, (unsigned long)g_eg_mode.tx_event_flags_group_current
  ps_lis_diag
  printf "delta: sample=%lu tick=%lu\n", (unsigned long)(g_sensor_lis_live.sample_count - $ps_s0), (unsigned long)(g_sensor_lis_live.last_sample_tick - $ps_t0)
end

define ps_joy_cal_status
  p/d g_sensor_joy_cal_status.stage
  p/d g_sensor_joy_cal_active
  p/d g_sensor_joy_cal_wait_confirm
  p/d g_sensor_joy_cal_status.last_error
  p/u g_sensor_joy_cal_capture.n
end

define ps_joy_cal_start
  set $ps_rc = (unsigned int)App_SensorReq_JoyCalStart()
  printf "queue SENSOR_JOY_CAL_START rc=%u\n", $ps_rc
end

define ps_joy_cal_start_wait
  tbreak AppSensorJoyCalStart
  ps_joy_cal_start
  continue
  finish
  ps_joy_cal_status
end

define ps_joy_cal_save
  set $ps_rc = (unsigned int)App_SensorReq_JoyCalSave()
  printf "queue SENSOR_JOY_CAL_SAVE rc=%u\n", $ps_rc
end

define ps_joy_cal_save_wait
  tbreak AppSensorJoyCalRequestSave
  ps_joy_cal_save
  continue
  finish
  ps_joy_cal_status
end

define ps_storage_status
  printf "== Storage status ==\n"
  printf "flash_ready=%lu jedec=0x%08lx last_op=%lu last_err=%ld\n", (unsigned long)g_storage_flash_ready, (unsigned long)g_storage_last_jedec_id, (unsigned long)g_storage_last_op, (long)g_storage_last_error
  printf "joycfg: valid=%lu err=%ld load_ok=%lu load_fail=%lu save_ok=%lu save_fail=%lu lseq=%lu sseq=%lu addr=0x%08lx\n", (unsigned long)g_storage_joycfg_valid, (long)g_storage_joycfg_last_error, (unsigned long)g_storage_joycfg_load_ok_count, (unsigned long)g_storage_joycfg_load_fail_count, (unsigned long)g_storage_joycfg_save_ok_count, (unsigned long)g_storage_joycfg_save_fail_count, (unsigned long)g_storage_joycfg_load_seq, (unsigned long)g_storage_joycfg_save_seq, (unsigned long)g_storage_settings_addr_dbg
  printf "smoke: pass=%lu fail=%lu addr=0x%08lx len=%lu\n", (unsigned long)g_storage_smoke_pass_count, (unsigned long)g_storage_smoke_fail_count, (unsigned long)KNOB_STORAGE_SMOKE_ADDR, (unsigned long)KNOB_STORAGE_SMOKE_LEN
  printf "filex: mounted=%lu m_ok=%lu m_fail=%lu fmt=%lu um_ok=%lu um_fail=%lu fx_status=%lu\n", (unsigned long)g_storage_filex_mounted, (unsigned long)g_storage_filex_mount_count, (unsigned long)g_storage_filex_mount_fail_count, (unsigned long)g_storage_filex_format_count, (unsigned long)g_storage_filex_unmount_count, (unsigned long)g_storage_filex_unmount_fail_count, (unsigned long)g_storage_filex_last_status
  printf "fat: base=0x%08lx size=%lu cache=%lu spc=%lu dir=%lu\n", (unsigned long)KNOB_STORAGE_FAT_BASE_ADDR, (unsigned long)KNOB_STORAGE_FAT_SIZE_BYTES, (unsigned long)KNOB_STORAGE_FILEX_CACHE_BYTES, (unsigned long)KNOB_STORAGE_FILEX_SECTORS_PER_CLUSTER, (unsigned long)KNOB_STORAGE_FILEX_DIR_ENTRIES
  printf "at25: op=%u cmd=%u io=%u addr=0x%08lx n=%lu herr=0x%08lx seq=%lu\n", (unsigned int)g_storage_at25_dbg.last_op, (unsigned int)g_storage_at25_dbg.cmd_status, (unsigned int)g_storage_at25_dbg.io_status, (unsigned long)g_storage_at25_dbg.addr, (unsigned long)g_storage_at25_dbg.nbytes, (unsigned long)g_storage_at25_dbg.hal_error, (unsigned long)g_storage_at25_dbg.seq
end

define ps_storage_probe
  set $ps_rc = (unsigned int)App_StorageReq_FlashProbe()
  printf "queue STORAGE_FLASH_PROBE rc=%u\n", $ps_rc
end

define ps_storage_probe_wait
  tbreak AppStorageRunFlashProbe
  ps_storage_probe
  continue
  finish
  ps_storage_status
end

define ps_storage_smoke
  set $ps_rc = (unsigned int)App_StorageReq_RawSmoke()
  printf "queue STORAGE_RAW_SMOKE rc=%u\n", $ps_rc
end

define ps_storage_smoke_wait
  tbreak AppStorageRunRawSmoke
  ps_storage_smoke
  continue
  finish
  ps_storage_status
end

define ps_storage_filex_mount
  set $ps_rc = (unsigned int)App_StorageReq_FileXMount()
  printf "queue STORAGE_FILEX_MOUNT rc=%u\n", $ps_rc
end

define ps_storage_filex_mount_wait
  tbreak AppStorageRunFileXMount
  ps_storage_filex_mount
  continue
  finish
  ps_storage_status
end

define ps_storage_filex_format
  set $ps_rc = (unsigned int)App_StorageReq_FileXFormat()
  printf "queue STORAGE_FILEX_FORMAT rc=%u\n", $ps_rc
end

define ps_storage_filex_format_wait
  tbreak AppStorageRunFileXFormat
  ps_storage_filex_format
  continue
  finish
  ps_storage_status
end

define ps_storage_filex_unmount
  set $ps_rc = (unsigned int)App_StorageReq_FileXUnmount()
  printf "queue STORAGE_FILEX_UNMOUNT rc=%u\n", $ps_rc
end

define ps_storage_filex_unmount_wait
  tbreak AppStorageRunFileXUnmount
  ps_storage_filex_unmount
  continue
  finish
  ps_storage_status
end

define ps_input_status
  printf "== Input status ==\n"
  printf "qInputCmd=%u qInputRaw=%u qUI=%u qGame=%u quiesced=%lu\n", (unsigned int)g_q_input_cmd.tx_queue_enqueued, (unsigned int)g_q_input_raw.tx_queue_enqueued, (unsigned int)g_q_ui_events.tx_queue_enqueued, (unsigned int)g_q_game_events.tx_queue_enqueued, (unsigned long)g_input_quiesced
  printf "raw: post=%lu recv=%lu drop=%lu suppressed=%lu\n", (unsigned long)g_input_raw_post_count, (unsigned long)g_input_raw_recv_count, (unsigned long)g_input_raw_drop_count, (unsigned long)g_input_raw_suppressed_count
  printf "action: total=%lu ui=%lu game=%lu sys=%lu ignored=%lu\n", (unsigned long)g_input_action_total_count, (unsigned long)g_input_action_ui_route_count, (unsigned long)g_input_action_game_route_count, (unsigned long)g_input_action_system_route_count, (unsigned long)g_input_action_ignored_count
  printf "posts: ui_ok=%lu ui_drop=%lu ui_drop_old=%lu game_ok=%lu game_drop=%lu game_drop_old=%lu\n", (unsigned long)g_input_action_ui_post_count, (unsigned long)g_input_action_ui_drop_count, (unsigned long)g_input_action_ui_drop_oldest_count, (unsigned long)g_input_action_game_post_count, (unsigned long)g_input_action_game_drop_count, (unsigned long)g_input_action_game_drop_oldest_count
  printf "filter: debounce_drop=%lu release_pass=%lu release_reconcile=%lu repeat_emit=%lu\n", (unsigned long)g_input_debounce_drop_count, (unsigned long)g_input_release_pass_count, (unsigned long)g_input_release_reconcile_count, (unsigned long)g_input_repeat_emit_count
  printf "long_emit=%lu joy_mask=0x%08lx\n", (unsigned long)g_input_long_emit_count, (unsigned long)g_sensor_joy_input_mask
  printf "syspost: activity_ok=%lu activity_drop=%lu menu_ok=%lu menu_drop=%lu\n", (unsigned long)g_input_sys_activity_post_count, (unsigned long)g_input_sys_activity_drop_count, (unsigned long)g_input_sys_menu_post_count, (unsigned long)g_input_sys_menu_drop_count
  printf "consumed: ui=%lu (last=%lu) game=%lu (last=%lu)\n", (unsigned long)g_ui_event_recv_count, (unsigned long)g_ui_event_last_action, (unsigned long)g_game_event_recv_count, (unsigned long)g_game_event_last_action
  printf "handled: ui_ok=%lu ui_ignored=%lu ui_qerr=%lu game_ok=%lu game_ignored=%lu game_qerr=%lu\n", (unsigned long)g_ui_event_handled_count, (unsigned long)g_ui_event_ignored_count, (unsigned long)g_ui_event_queue_error_count, (unsigned long)g_game_event_handled_count, (unsigned long)g_game_event_ignored_count, (unsigned long)g_game_event_queue_error_count
  printf "last_action=%lu last_mode=0x%08lx\n", (unsigned long)g_input_action_last, (unsigned long)g_input_action_last_mode
  printf "last: src=%lu edge=%lu level=%lu tick=%lu\n", (unsigned long)g_input_last_source, (unsigned long)g_input_last_edge, (unsigned long)g_input_last_level, (unsigned long)g_input_last_tick
end

define ps_input_reset
  set g_input_raw_post_count = 0
  set g_input_raw_recv_count = 0
  set g_input_raw_drop_count = 0
  set g_input_raw_suppressed_count = 0
  set g_input_action_total_count = 0
  set g_input_action_ui_route_count = 0
  set g_input_action_game_route_count = 0
  set g_input_action_system_route_count = 0
  set g_input_action_ignored_count = 0
  set g_input_action_ui_post_count = 0
  set g_input_action_ui_drop_count = 0
  set g_input_action_ui_drop_oldest_count = 0
  set g_input_action_game_post_count = 0
  set g_input_action_game_drop_count = 0
  set g_input_action_game_drop_oldest_count = 0
  set g_ui_event_recv_count = 0
  set g_game_event_recv_count = 0
  set g_ui_event_handled_count = 0
  set g_ui_event_ignored_count = 0
  set g_ui_event_queue_error_count = 0
  set g_game_event_handled_count = 0
  set g_game_event_ignored_count = 0
  set g_game_event_queue_error_count = 0
  set g_input_debounce_drop_count = 0
  set g_input_release_pass_count = 0
  set g_input_release_reconcile_count = 0
  set g_input_repeat_emit_count = 0
  set g_input_long_emit_count = 0
  set g_input_sys_activity_post_count = 0
  set g_input_sys_activity_drop_count = 0
  set g_input_sys_menu_post_count = 0
  set g_input_sys_menu_drop_count = 0
  set g_input_action_last = 0
  set g_input_action_last_mode = 0
  set g_input_last_source = 0
  set g_input_last_edge = 0
  set g_input_last_level = 0
  set g_input_last_tick = 0
  set $__ps_i = 0
  while $__ps_i <= 9
    set g_input_button_state[$__ps_i].edge_seen = 0
    set g_input_button_state[$__ps_i].pressed = 0
    set g_input_button_state[$__ps_i].last_edge_tick = 0
    set g_input_button_state[$__ps_i].press_tick = 0
    set g_input_button_state[$__ps_i].long_sent = 0
    set g_input_button_state[$__ps_i].next_repeat_tick = 0
    set $__ps_i = $__ps_i + 1
  end
  set g_sensor_joy_input_mask = 0
  printf "input counters+latches reset\n"
end

define ps_input_latch
  printf "== Input latch per source ==\n"
  set $__ps_i = 1
  while $__ps_i <= 9
    set $__ps_held = 0
    if (g_input_button_state[$__ps_i].pressed != 0)
      set $__ps_held = (unsigned long)(HAL_GetTick() - g_input_button_state[$__ps_i].press_tick)
    end
    printf "src=%u pressed=%lu edge_seen=%lu press_tick=%lu held_ms=%lu next_rep=%lu long=%lu last_edge=%lu\n", $__ps_i, (unsigned long)g_input_button_state[$__ps_i].pressed, (unsigned long)g_input_button_state[$__ps_i].edge_seen, (unsigned long)g_input_button_state[$__ps_i].press_tick, (unsigned long)$__ps_held, (unsigned long)g_input_button_state[$__ps_i].next_repeat_tick, (unsigned long)g_input_button_state[$__ps_i].long_sent, (unsigned long)g_input_button_state[$__ps_i].last_edge_tick
    set $__ps_i = $__ps_i + 1
  end
end

define ps_input_snap
  ps_input_status
  ps_input_latch
  ps_input_decode_last
end

define ps_display_stack
  set $__ps_start = (unsigned long)g_th_display.tx_thread_stack_start
  set $__ps_endp1 = ((unsigned long)g_th_display.tx_thread_stack_end) + 1
  set $__ps_now = (unsigned long)g_th_display.tx_thread_stack_ptr
  set $__ps_min = (unsigned long)g_dbg_display_stack_min_sp
  printf "== Display stack ==\n"
  printf "samples=%lu now=0x%08lx min=0x%08lx start=0x%08lx end=0x%08lx\n", (unsigned long)g_dbg_display_stack_sample_count, $__ps_now, $__ps_min, $__ps_start, (unsigned long)g_th_display.tx_thread_stack_end
  if $__ps_min != 0
    printf "low-water: free=%lu used_max=%lu bytes\n", ($__ps_min - $__ps_start), ($__ps_endp1 - $__ps_min)
  end
  printf "qDisplay: enq=%u avail=%u susp=%u run_count=%lu state=%u\n", (unsigned int)g_q_display_cmd.tx_queue_enqueued, (unsigned int)g_q_display_cmd.tx_queue_available_storage, (unsigned int)g_q_display_cmd.tx_queue_suspended_count, (unsigned long)g_th_display.tx_thread_run_count, (unsigned int)g_th_display.tx_thread_state
  printf "present: posted=%lu coalesced=%lu send_fail=%lu pending=%lu\n", (unsigned long)g_display_present_post_count, (unsigned long)g_display_present_coalesce_count, (unsigned long)g_display_present_send_fail_count, (unsigned long)g_display_present_pending
end

define ps_cal_stall_dump
  echo === cal_stall ===\n
  echo stage active wait last_error n t_start last_sample duration sample_every settle tick\n
  p g_sensor_joy_cal_status.stage
  p g_sensor_joy_cal_active
  p g_sensor_joy_cal_wait_confirm
  p g_sensor_joy_cal_status.last_error
  p g_sensor_joy_cal_capture.n
  p g_sensor_joy_cal_capture.t_start_ms
  p g_sensor_joy_cal_capture.last_sample_ms
  p g_sensor_joy_cal_capture.duration_ms
  p g_sensor_joy_cal_capture.sample_every_ms
  p g_sensor_joy_cal_capture.settle_ms
  p HAL_GetTick()
  bt 8
end

define ps_joy_diag
  echo === joy_diag ===\n
  echo mode power bus tmag_state tmag_err tmag_fail tmag_try tmag_next tmag_ok\n
  p g_eg_mode.tx_event_flags_group_current
  p g_eg_power.tx_event_flags_group_current
  p g_sensor_bus_fault
  p g_sensor_tmag.state
  p g_sensor_tmag.last_error
  p g_sensor_tmag.fail_count
  p g_sensor_tmag.recovery_attempts
  p g_sensor_tmag.next_retry_tick
  p g_sensor_tmag.last_success_tick
  echo cal_stage cal_active cal_wait cal_progress cal_err cal_save_pending cap_active cap_n cap_t0 cap_last cap_dur cap_step\n
  p g_sensor_joy_cal_status.stage
  p g_sensor_joy_cal_active
  p g_sensor_joy_cal_wait_confirm
  p g_sensor_joy_cal_status.progress
  p g_sensor_joy_cal_status.last_error
  p g_sensor_joy_cal_status.save_pending
  p g_sensor_joy_cal_capture.active
  p g_sensor_joy_cal_capture.n
  p g_sensor_joy_cal_capture.t_start_ms
  p g_sensor_joy_cal_capture.last_sample_ms
  p g_sensor_joy_cal_capture.duration_ms
  p g_sensor_joy_cal_capture.sample_every_ms
  echo cal_dir_avg up_x up_y right_x right_y down_x down_y left_x left_y\n
  p g_sensor_joy_cal_dir_avg_x[0]
  p g_sensor_joy_cal_dir_avg_y[0]
  p g_sensor_joy_cal_dir_avg_x[1]
  p g_sensor_joy_cal_dir_avg_y[1]
  p g_sensor_joy_cal_dir_avg_x[2]
  p g_sensor_joy_cal_dir_avg_y[2]
  p g_sensor_joy_cal_dir_avg_x[3]
  p g_sensor_joy_cal_dir_avg_y[3]
  echo gate neutral_armed neutral_cnt input_mask live_dir live_mask nx ny r center_x center_y span_x span_y rot\n
  p g_sensor_joy_input_gate_valid
  p g_sensor_joy_input_neutral_armed
  p g_sensor_joy_input_neutral_stable_count
  p g_sensor_joy_input_mask
  p g_sensor_joy_live_status.dir
  p g_sensor_joy_live_status.input_mask
  p g_sensor_joy_live_status.nx
  p g_sensor_joy_live_status.ny
  p g_sensor_joy_live_status.r_abs_mT
  p g_sensor_joy_live_status.center_x_mT
  p g_sensor_joy_live_status.center_y_mT
  p g_sensor_joy_live_status.span_x_mT
  p g_sensor_joy_live_status.span_y_mT
  p g_sensor_joy_live_status.rotation_deg
  echo q_sensor q_input_raw q_ui q_game\n
  p g_q_sensor_req.tx_queue_enqueued
  p g_q_input_raw.tx_queue_enqueued
  p g_q_ui_events.tx_queue_enqueued
  p g_q_game_events.tx_queue_enqueued
  bt 12
end

define ps_input_decode_last
  printf "last-src: "
  if (g_input_last_source == 1)
    printf "BTN_A"
  else
    if (g_input_last_source == 2)
      printf "BTN_B"
    else
      if (g_input_last_source == 3)
        printf "BTN_L"
      else
        if (g_input_last_source == 4)
          printf "BTN_R"
        else
          if (g_input_last_source == 5)
            printf "BTN_BOOT"
          else
            if (g_input_last_source == 6)
              printf "JOY_UP"
            else
              if (g_input_last_source == 7)
                printf "JOY_RIGHT"
              else
                if (g_input_last_source == 8)
                  printf "JOY_DOWN"
                else
                  if (g_input_last_source == 9)
                    printf "JOY_LEFT"
                  else
                    printf "UNKNOWN(%lu)", (unsigned long)g_input_last_source
                  end
                end
              end
            end
          end
        end
      end
    end
  end
  printf ", last-edge: "
  if (g_input_last_edge == 1)
    printf "RELEASE"
  else
    if (g_input_last_edge == 2)
      printf "PRESS"
    else
      printf "EDGE(%lu)", (unsigned long)g_input_last_edge
    end
  end
  printf ", last-action: "
  if (g_input_action_last == 1)
    printf "CONFIRM"
  else
    if (g_input_action_last == 2)
      printf "CANCEL"
    else
      if (g_input_action_last == 3)
        printf "LEFT"
      else
        if (g_input_action_last == 4)
          printf "RIGHT"
        else
          if (g_input_action_last == 5)
            printf "MENU"
          else
            if (g_input_action_last == 6)
              printf "UP"
            else
              if (g_input_action_last == 7)
                printf "DOWN"
              else
                printf "ACTION(%lu)", (unsigned long)g_input_action_last
              end
            end
          end
        end
      end
    end
  end
  printf "\n"
end

define ps_audio_status
  printf "== Audio status ==\n"
  printf "state=%lu starts=%lu stops=%lu restarts=%lu underrun=%lu last_err=%ld\n", (unsigned long)g_audio_state, (unsigned long)g_audio_start_count, (unsigned long)g_audio_stop_count, (unsigned long)g_audio_restart_count, (unsigned long)g_audio_underflow_count, (long)g_audio_last_error
  printf "dma: events=0x%08lx half=%lu full=%lu err=%lu\n", (unsigned long)g_audio_dma_events, (unsigned long)g_audio_half_irq_count, (unsigned long)g_audio_full_irq_count, (unsigned long)g_audio_error_irq_count
  printf "qAudio=%u\n", (unsigned int)g_q_audio_cmd.tx_queue_enqueued
end

define ps_audio_start
  set $ps_rc = (unsigned int)App_AudioReq_StartTone()
  printf "queue AUDIO_START_TONE rc=%u\n", $ps_rc
end

define ps_audio_start_wait
  tbreak AppAudioStartTone
  ps_audio_start
  continue
  finish
  ps_audio_status
end

define ps_audio_stop
  set $ps_rc = (unsigned int)App_AudioReq_Stop()
  printf "queue AUDIO_STOP rc=%u\n", $ps_rc
end

define ps_audio_stop_wait
  tbreak AppAudioStop
  ps_audio_stop
  continue
  finish
  ps_audio_status
end

define ps_audio_power_smoke
  printf "== Audio + Power smoke ==\n"
  ps_audio_start_wait
  ps_quiesce
  ps_audio_status
  ps_resume
  ps_audio_status
end

define ps_freeze_dump
  printf "== Freeze dump ==\n"
  ps_mode_flags
  ps_display_stack
  ps_input_snap
  ps_power_perf
  printf "== Blit health ==\n"
  p g_render_blit_invalid_arg_count
  p g_render_blit_stride_reject_count
  p g_render_blit_bounds_break_count
  printf "== Renderer lock ==\n"
  p g_renderer_lock_error_count
  p g_mtx_renderer.tx_mutex_owner
  p g_mtx_renderer.tx_mutex_ownership_count
  p g_mtx_renderer.tx_mutex_suspended_count
  p g_mtx_renderer.tx_mutex_suspension_list
  printf "== Display queue ==\n"
  p g_display_present_pending
  p g_q_display_cmd.tx_queue_enqueued
  p g_q_display_cmd.tx_queue_available_storage
  p g_q_display_cmd.tx_queue_suspended_count
  p g_q_display_cmd.tx_queue_read
  p g_q_display_cmd.tx_queue_write
  p g_q_display_cmd.tx_queue_start
  p g_q_display_cmd.tx_queue_end
  x/16dw &g_q_display_cmd_storage
  x/2dw g_q_display_cmd.tx_queue_read
  printf "== System queue ==\n"
  p g_q_sys_events.tx_queue_enqueued
  p g_q_sys_events.tx_queue_available_storage
  p g_q_sys_events.tx_queue_suspended_count
  p g_power_perf_hint_inflight
  printf "== Thread states ==\n"
  p g_th_display.tx_thread_state
  p g_th_display.tx_thread_run_count
  p g_th_game.tx_thread_state
  p g_th_game.tx_thread_run_count
  p g_th_power.tx_thread_state
  p g_th_power.tx_thread_run_count
  p g_th_audio.tx_thread_state
  p g_th_audio.tx_thread_run_count
  p g_th_ui.tx_thread_state
  p g_th_ui.tx_thread_run_count
  bt 10
end

define ps_blit_health
  printf "== Blit health ==\n"
  p g_render_blit_invalid_arg_count
  p g_render_blit_stride_reject_count
  p g_render_blit_bounds_break_count
end

define ps_freeze_delta_start
  set $ps_d0 = g_th_display.tx_thread_run_count
  set $ps_g0 = g_th_game.tx_thread_run_count
  set $ps_a0 = g_th_audio.tx_thread_run_count
  set $ps_u0 = g_th_ui.tx_thread_run_count
  set $ps_p0 = g_th_power.tx_thread_run_count
  set $ps_fp0 = g_display_present_post_count
  set $ps_fi0 = s_demo.frame_id
  set $ps_sx0 = s_demo.scroll_x
  printf "delta-start captured\n"
end

define ps_freeze_delta_end
  printf "== Freeze delta ==\n"
  p (unsigned long)(g_th_display.tx_thread_run_count - $ps_d0)
  p (unsigned long)(g_th_game.tx_thread_run_count - $ps_g0)
  p (unsigned long)(g_th_audio.tx_thread_run_count - $ps_a0)
  p (unsigned long)(g_th_ui.tx_thread_run_count - $ps_u0)
  p (unsigned long)(g_th_power.tx_thread_run_count - $ps_p0)
  p (unsigned long)(g_display_present_post_count - $ps_fp0)
  p (unsigned long)(s_demo.frame_id - $ps_fi0)
  p (unsigned long)(s_demo.scroll_x - $ps_sx0)
  p g_mtx_renderer.tx_mutex_owner
  p g_mtx_renderer.tx_mutex_ownership_count
  p g_mtx_renderer.tx_mutex_suspended_count
end

define ps_mark_runs
  set $ps_run_tick0 = (unsigned long)HAL_GetTick()
  set $ps_run_display0 = (unsigned long)g_th_display.tx_thread_run_count
  set $ps_run_game0 = (unsigned long)g_th_game.tx_thread_run_count
  set $ps_run_ui0 = (unsigned long)g_th_ui.tx_thread_run_count
  set $ps_run_input0 = (unsigned long)g_th_input.tx_thread_run_count
  set $ps_run_sensor0 = (unsigned long)g_th_sensor.tx_thread_run_count
  set $ps_run_power0 = (unsigned long)g_th_power.tx_thread_run_count
  set $ps_run_audio0 = (unsigned long)g_th_audio.tx_thread_run_count
  set $ps_run_storage0 = (unsigned long)g_th_storage.tx_thread_run_count
  set $ps_run_mark_valid = 1
  printf "run mark captured @tick=%lu\n", $ps_run_tick0
end

define ps_delta_runs
  if ($ps_run_mark_valid == 0)
    printf "run mark not set (call ps_mark_runs first)\n"
  else
    set $ps_run_tick1 = (unsigned long)HAL_GetTick()
    printf "== Run deltas ==\n"
    printf "tick=%lu\n", (unsigned long)($ps_run_tick1 - $ps_run_tick0)
    printf "display=%lu game=%lu ui=%lu input=%lu sensor=%lu power=%lu audio=%lu storage=%lu\n", (unsigned long)(g_th_display.tx_thread_run_count - $ps_run_display0), (unsigned long)(g_th_game.tx_thread_run_count - $ps_run_game0), (unsigned long)(g_th_ui.tx_thread_run_count - $ps_run_ui0), (unsigned long)(g_th_input.tx_thread_run_count - $ps_run_input0), (unsigned long)(g_th_sensor.tx_thread_run_count - $ps_run_sensor0), (unsigned long)(g_th_power.tx_thread_run_count - $ps_run_power0), (unsigned long)(g_th_audio.tx_thread_run_count - $ps_run_audio0), (unsigned long)(g_th_storage.tx_thread_run_count - $ps_run_storage0)
    printf "qDisplay(enq=%lu avail=%lu) qSys(enq=%lu avail=%lu) qSensorReq(enq=%lu avail=%lu)\n", (unsigned long)g_q_display_cmd.tx_queue_enqueued, (unsigned long)g_q_display_cmd.tx_queue_available_storage, (unsigned long)g_q_sys_events.tx_queue_enqueued, (unsigned long)g_q_sys_events.tx_queue_available_storage, (unsigned long)g_q_sensor_req.tx_queue_enqueued, (unsigned long)g_q_sensor_req.tx_queue_available_storage
  end
end

define ps_perf_mark
  set $ps_pf_mark_valid = 1
  set $ps_pf_tick0 = (unsigned long)HAL_GetTick()
  set $ps_pf_present0 = (unsigned long)g_display_present_post_count
  set $ps_pf_coalesce0 = (unsigned long)g_display_present_coalesce_count
  set $ps_pf_lock_err0 = (unsigned long)g_renderer_lock_error_count
  set $ps_pf_sensor_run0 = (unsigned long)g_th_sensor.tx_thread_run_count
  set $ps_pf_game_run0 = (unsigned long)g_th_game.tx_thread_run_count
  set $ps_pf_display_run0 = (unsigned long)g_th_display.tx_thread_run_count
  set $ps_pf_lis_sample0 = (unsigned long)g_sensor_lis_live.sample_count
  set $ps_pf_lis_tick0 = (unsigned long)g_sensor_lis_live.last_sample_tick
  set $ps_pf_input_game0 = (unsigned long)g_input_action_game_route_count
  printf "perf mark captured @tick=%lu\n", $ps_pf_tick0
end

define ps_perf_delta
  if ($ps_pf_mark_valid == 0)
    printf "perf mark not set (call ps_perf_mark first)\n"
  else
    set $ps_pf_tick1 = (unsigned long)HAL_GetTick()
    set $ps_pf_dt = (unsigned long)($ps_pf_tick1 - $ps_pf_tick0)
    if ($ps_pf_dt == 0)
      set $ps_pf_dt = 1
    end
    set $ps_pf_frames = (unsigned long)(g_display_present_post_count - $ps_pf_present0)
    set $ps_pf_presents = (unsigned long)(g_display_present_post_count - $ps_pf_present0)
    set $ps_pf_coalesced = (unsigned long)(g_display_present_coalesce_count - $ps_pf_coalesce0)
    set $ps_pf_lock_err = (unsigned long)(g_renderer_lock_error_count - $ps_pf_lock_err0)
    set $ps_pf_sensor_runs = (unsigned long)(g_th_sensor.tx_thread_run_count - $ps_pf_sensor_run0)
    set $ps_pf_game_runs = (unsigned long)(g_th_game.tx_thread_run_count - $ps_pf_game_run0)
    set $ps_pf_display_runs = (unsigned long)(g_th_display.tx_thread_run_count - $ps_pf_display_run0)
    set $ps_pf_lis_samples = (unsigned long)(g_sensor_lis_live.sample_count - $ps_pf_lis_sample0)
    set $ps_pf_lis_tick_delta = (unsigned long)(g_sensor_lis_live.last_sample_tick - $ps_pf_lis_tick0)
    set $ps_pf_input_game = (unsigned long)(g_input_action_game_route_count - $ps_pf_input_game0)
    printf "== Perf delta ==\n"
    printf "mode=0x%08lx sensor_mode_token=%lu game_exit_pending=%lu\n", (unsigned long)g_eg_mode.tx_event_flags_group_current, (unsigned long)g_sensor_mode_token, (unsigned long)g_game_exit_to_static_pending
    printf "dt_ms=%lu frames(est)=%lu presents=%lu fps~=%lu\n", $ps_pf_dt, $ps_pf_frames, $ps_pf_presents, (unsigned long)(($ps_pf_frames * 1000) / $ps_pf_dt)
    printf "thread runs: sensor=%lu game=%lu display=%lu\n", $ps_pf_sensor_runs, $ps_pf_game_runs, $ps_pf_display_runs
    printf "render/present: lock_err=%lu coalesced=%lu pending=%lu qDisplay(enq=%lu avail=%lu)\n", $ps_pf_lock_err, $ps_pf_coalesced, (unsigned long)g_display_present_pending, (unsigned long)g_q_display_cmd.tx_queue_enqueued, (unsigned long)g_q_display_cmd.tx_queue_available_storage
    printf "lis: samples=%lu sample_hz~=%lu last_tick_delta=%lu stream=%lu profile_req=%lu profile_applied=%lu\n", $ps_pf_lis_samples, (unsigned long)(($ps_pf_lis_samples * 1000) / $ps_pf_dt), $ps_pf_lis_tick_delta, (unsigned long)g_sensor_lis_stream_enabled, (unsigned long)g_sensor_lis_profile_requested, (unsigned long)g_sensor_lis_profile_applied
    printf "input_game_routes=%lu qSensorReq(enq=%lu avail=%lu)\n", $ps_pf_input_game, (unsigned long)g_q_sensor_req.tx_queue_enqueued, (unsigned long)g_q_sensor_req.tx_queue_available_storage
  end
end

define ps_sched_freeze
  printf "== Sched freeze ==\n"
  p _tx_thread_system_state
  p _tx_thread_preempt_disable
  p _tx_thread_highest_priority
  p/x _tx_thread_priority_maps[0]
  p _tx_thread_execute_ptr
  p _tx_thread_current_ptr
  if (_tx_thread_execute_ptr != 0)
    p ((TX_THREAD*)_tx_thread_execute_ptr)->tx_thread_name
  end
  if (_tx_thread_current_ptr != 0)
    p ((TX_THREAD*)_tx_thread_current_ptr)->tx_thread_name
  end
  printf "== Display queue head ==\n"
  p g_q_display_cmd.tx_queue_enqueued
  p g_q_display_cmd.tx_queue_available_storage
  p g_q_display_cmd.tx_queue_read
  p g_q_display_cmd.tx_queue_write
  if (g_q_display_cmd.tx_queue_enqueued != 0)
    set $qr = (unsigned long*)g_q_display_cmd.tx_queue_read
    p *$qr
    p *($qr + 1)
  end
  printf "== Renderer mutex ==\n"
  p g_mtx_renderer.tx_mutex_owner
  p g_mtx_renderer.tx_mutex_ownership_count
  p g_mtx_renderer.tx_mutex_suspended_count
  if (g_mtx_renderer.tx_mutex_owner != 0)
    p ((TX_THREAD*)g_mtx_renderer.tx_mutex_owner)->tx_thread_name
  end
  printf "== Thread state/run ==\n"
  p g_th_display.tx_thread_state
  p g_th_display.tx_thread_run_count
  p g_th_game.tx_thread_state
  p g_th_game.tx_thread_run_count
  p g_th_audio.tx_thread_state
  p g_th_audio.tx_thread_run_count
  p g_th_ui.tx_thread_state
  p g_th_ui.tx_thread_run_count
  p g_th_power.tx_thread_state
  p g_th_power.tx_thread_run_count
  p g_th_sensor.tx_thread_state
  p g_th_sensor.tx_thread_run_count
  p g_renderer_lock_error_count
  bt 10
end

# Optional strategic points (enable intentionally, keep <= 5 total):
# break App_ThreadX_LowPower_Enter
# break App_ThreadX_LowPower_Exit
# break HAL_PCD_ConnectCallback
