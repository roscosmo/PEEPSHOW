# PeepShow authoritative breakpoint set.
# Keep total active breakpoints <= 5.

set pagination off
set confirm off
echo [debug.gdb] loaded: peepshow-debug\n
set $ps_run_mark_valid = 0
set $ps_tb_mark_valid = 0
set $ps_ad_music_valid = 0
set $ps_ad_stress_valid = 0
set $ps_poly_valid = 0

# Primary fault traps.
if $_isvoid($ps_hf_bp)
  set $ps_hf_bp = 0
end
if ($ps_hf_bp == 0)
  break HardFault_Handler
  set $ps_hf_bp = $bpnum
end
if $_isvoid($ps_err_bp)
  set $ps_err_bp = 0
end
if ($ps_err_bp == 0)
  break Error_Handler
  set $ps_err_bp = $bpnum
end

# Power-thread event helpers.
# Usage:
#   ps_smoke      (recommended one-shot check)
#   ps_mode_flags
#   ps_power_flags
#   ps_power_perf
#   ps_dbg_lp_policy
#   ps_stop2_status
#   ps_stop2_status_min
#   ps_stop2_power_snapshot
#   ps_stop2_power_snapshot_full
#   ps_stop2_wake_decode
#   ps_stop2_timebase_mark
#   ps_stop2_timebase_delta
#   ps_stop2_timebase_probe [wait_ticks]
#   ps_stop2_timebase_persisted
#   ps_stop2_timebase_persisted_clear
#   ps_retained_status
#   ps_retained_clear
#   ps_stop2_prep_smoke
#   ps_stop2_audio_soak
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
#   ps_storage_audio_status
#   ps_storage_audio_load_dbg
#   ps_storage_audio_load_dbg_wait
#   ps_storage_audio_install_embedded
#   ps_storage_audio_install_embedded_wait
#   ps_storage_pkg_manifest_status
#   ps_storage_pkg_manifest_load <addr> <size>
#   ps_storage_pkg_manifest_load_wait <addr> <size>
#   ps_storage_pkg_manifest_load_default
#   ps_storage_pkg_manifest_load_default_wait
#   ps_storage_pkg_manifest_import_fat
#   ps_storage_pkg_manifest_import_fat_wait
#   ps_storage_scene_import_fat
#   ps_storage_scene_import_fat_wait
#   ps_storage_pkg_manifest_slot_info
#   ps_storage_pkg_manifest_slot_prepare_wait
#   ps_storage_pkg_manifest_slot_verify_wait
#   ps_storage_pkg_manifest_slot_workflow_help
#   ps_storage_pkg_manifest_txn_smoke [bad_size]
#   ps_storage_pkg_manifest_erase
#   ps_storage_pkg_manifest_erase_wait
#   ps_storage_pkg_manifest_write_test
#   ps_storage_pkg_manifest_write_test_wait
#   ps_storage_pkg_manifest_install_bin_wait
#   ps_storage_scene_reload_map_helpers
#   ps_storage_scene_assets_list_maps
#   ps_storage_scene_assets_install_all_maps
#   ps_storage_raw_app_erase
#   ps_storage_raw_app_erase_wait
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
#   ps_storage_filex_fallback_smoke
#   ps_storage_install_index_status
#   ps_storage_install_index_smoke
#   ps_storage_install_index_atomic_smoke
#   ps_input_status
#   ps_audio_status
#   ps_input_latch
#   ps_input_reset
#   ps_input_snap
#   ps_rt_tune_help
#   ps_rt_tune_status
#   ps_rt_tune_reset
#   ps_rt_tune_camera <follow_permille> <lookahead_x_px> <max_speed_px_s>
#   ps_rt_tune_move <speed_px_s> <accel_px_s2> <decel_px_s2>
#   ps_rt_tune_render <scale> <present_mode:0/1/2>
#   ps_rt_tune_input <controller_profile_id> <input_flags> <deadzone_permille>
#   ps_display_stack
#   ps_cal_stall_dump
#   ps_joy_diag
#   ps_joy_raw
#   ps_audio_start
#   ps_audio_start_wait
#   ps_audio_stop
#   ps_audio_stop_wait
#   ps_audio_power_smoke
#   ps_audio_phase3_stress20 [cycles]
#   ps_audio_poly4_start
#   ps_audio_poly4_end
#   ps_audio_overlap_start
#   ps_audio_overlap_end
#   ps_mark_runs
#   ps_delta_runs
#   ps_perf_mark
#   ps_perf_delta
#   ps_topdown_m1_prepare
#   ps_topdown_m1_verify
#   ps_topdown_m2_prepare
#   ps_topdown_m2_verify
#   ps_topdown_mx_prepare
#   ps_topdown_mx_verify

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
  set $ps_base_sysclk_mhz = (unsigned long)((HAL_RCC_GetSysClockFreq() + 500000UL) / 1000000UL)
  printf "== Power perf ==\n"
  printf "profile: current=%lu target=%lu base_sysclk_mhz=%lu last_switch_tick=%lu\n", (unsigned long)g_power_perf_profile_current, (unsigned long)g_power_perf_profile_target, (unsigned long)$ps_base_sysclk_mhz, (unsigned long)g_power_perf_last_switch_tick
  printf "audio_boost_active=%lu\n", (unsigned long)g_power_perf_audio_boost_active
  printf "hint: seq=%lu post=%lu drop=%lu rx=%lu last_present_ticks=%lu\n", (unsigned long)g_power_perf_hint_seq, (unsigned long)g_power_perf_hint_post_count, (unsigned long)g_power_perf_hint_drop_count, (unsigned long)g_power_perf_hint_rx_count, (unsigned long)g_power_perf_last_present_ticks
  printf "work: draw_ticks=%lu dirty_rows=%lu full_flush=%lu\n", (unsigned long)g_power_perf_last_draw_ticks, (unsigned long)g_power_perf_last_dirty_rows, (unsigned long)g_power_perf_last_full_flush
  printf "streak: miss=%lu headroom=%lu up=%lu down=%lu dwell_block=%lu\n", (unsigned long)g_power_perf_miss_streak, (unsigned long)g_power_perf_headroom_streak, (unsigned long)g_power_perf_up_count, (unsigned long)g_power_perf_down_count, (unsigned long)g_power_perf_dwell_block_count
  printf "knobs: compile-time KNOB_* macros may be unavailable in this debug context\n"
end

define ps_dbg_lp_policy
  set $ps_dbgcr = *((unsigned long *)0xE0044004)
  printf "== Debug low-power policy ==\n"
  printf "DBGMCU_CR=0x%08lx stop_stby_bits=0x%02lx (bit1=STOP bit2=STANDBY)\n", (unsigned long)$ps_dbgcr, (unsigned long)($ps_dbgcr & 0x00000006)
  if (($ps_dbgcr & 0x00000006) != 0)
    printf "policy(runtime): debug-hold in STOP/STANDBY appears ENABLED\n"
  else
    printf "policy(runtime): debug-hold in STOP/STANDBY appears DISABLED\n"
  end
  printf "mode=0x%08lx power=0x%08lx sysclk=%lu\n", (unsigned long)g_eg_mode.tx_event_flags_group_current, (unsigned long)g_eg_power.tx_event_flags_group_current, (unsigned long)HAL_RCC_GetSysClockFreq()
end

define ps_stop2_status
  printf "== STOP2 status ==\n"
  printf "armed=%lu entry=%lu wake=%lu abort=%lu last_err=%ld\n", (unsigned long)g_power_stop2_armed, (unsigned long)g_power_stop2_entry_count, (unsigned long)g_power_stop2_wake_count, (unsigned long)g_power_stop2_abort_count, (long)g_power_stop2_last_error
  printf "last_wusr=0x%08lx last_sr=0x%08lx mode=0x%08lx power=0x%08lx sysclk=%lu\n", (unsigned long)g_power_stop2_last_wusr, (unsigned long)g_power_stop2_last_sr, (unsigned long)g_eg_mode.tx_event_flags_group_current, (unsigned long)g_eg_power.tx_event_flags_group_current, (unsigned long)HAL_RCC_GetSysClockFreq()
  printf "decision: last=%lu reenter=%lu resume=%lu defer=%lu abort_decisions=%lu\n", (unsigned long)g_power_stop2_decision_last, (unsigned long)g_power_stop2_decision_reenter_count, (unsigned long)g_power_stop2_decision_resume_count, (unsigned long)g_power_stop2_decision_defer_count, (unsigned long)g_power_stop2_decision_abort_count
  printf "decision_text: "
  if (g_power_stop2_decision_last == 1)
    printf "ARMED"
  else
    if (g_power_stop2_decision_last == 2)
      printf "DEFER_CADENCE"
    else
      if (g_power_stop2_decision_last == 3)
        printf "REENTER_STOP"
      else
        if (g_power_stop2_decision_last == 4)
          printf "RESUME_MODE_EXIT"
        else
          if (g_power_stop2_decision_last == 5)
            printf "RESUME_REQ"
          else
            if (g_power_stop2_decision_last == 6)
              printf "ABORT"
            else
              printf "NONE"
            end
          end
        end
      end
    end
  end
  printf "\n"
end

define ps_stop2_status_min
  printf "stop2: armed=%lu entry=%lu wake=%lu abort=%lu mode=0x%08lx power=0x%08lx dec=%lu\n", (unsigned long)g_power_stop2_armed, (unsigned long)g_power_stop2_entry_count, (unsigned long)g_power_stop2_wake_count, (unsigned long)g_power_stop2_abort_count, (unsigned long)g_eg_mode.tx_event_flags_group_current, (unsigned long)g_eg_power.tx_event_flags_group_current, (unsigned long)g_power_stop2_decision_last
end

define ps_stop2_timebase_mark
  set $ps_tb_mark_valid = 1
  set $ps_tb_hal0 = (unsigned long)HAL_GetTick()
  set $ps_tb_tx0 = (unsigned long)_tx_timer_system_clock
  set $ps_tb_entry0 = (unsigned long)g_power_stop2_entry_count
  set $ps_tb_wake0 = (unsigned long)g_power_stop2_wake_count
  printf "timebase mark: hal=%lu tx=%lu stop_entry=%lu stop_wake=%lu\n", $ps_tb_hal0, $ps_tb_tx0, $ps_tb_entry0, $ps_tb_wake0
end

define ps_stop2_timebase_delta
  if ($ps_tb_mark_valid == 0)
    printf "timebase mark not set (call ps_stop2_timebase_mark first)\n"
  else
    set $ps_tb_hal1 = (unsigned long)HAL_GetTick()
    set $ps_tb_tx1 = (unsigned long)_tx_timer_system_clock
    set $ps_tb_dhal = (unsigned long)($ps_tb_hal1 - $ps_tb_hal0)
    set $ps_tb_dtx = (unsigned long)($ps_tb_tx1 - $ps_tb_tx0)
    set $ps_tb_dentry = (unsigned long)(g_power_stop2_entry_count - $ps_tb_entry0)
    set $ps_tb_dwake = (unsigned long)(g_power_stop2_wake_count - $ps_tb_wake0)
    if ($ps_tb_dhal >= $ps_tb_dtx)
      set $ps_tb_absdiff = (unsigned long)($ps_tb_dhal - $ps_tb_dtx)
    else
      set $ps_tb_absdiff = (unsigned long)($ps_tb_dtx - $ps_tb_dhal)
    end
    printf "== STOP2 timebase delta ==\n"
    printf "hal_dt=%lu tx_dt=%lu abs_diff=%lu\n", $ps_tb_dhal, $ps_tb_dtx, $ps_tb_absdiff
    printf "stop2: entry_delta=%lu wake_delta=%lu\n", $ps_tb_dentry, $ps_tb_dwake
    printf "state: mode=0x%08lx power=0x%08lx armed=%lu decision=%lu\n", (unsigned long)g_eg_mode.tx_event_flags_group_current, (unsigned long)g_eg_power.tx_event_flags_group_current, (unsigned long)g_power_stop2_armed, (unsigned long)g_power_stop2_decision_last
    printf "note: expect close hal/tx deltas over the same run window (small debugger jitter is normal)\n"
  end
end

define ps_stop2_timebase_probe
  set $ps_tb_wait_ticks = (unsigned long)$arg0
  if ($ps_tb_wait_ticks == 0)
    set $ps_tb_wait_ticks = 120
  end
  ps_mode_stop
  ps_stop2_timebase_mark
  __ps_continue_for_ticks $ps_tb_wait_ticks
  ps_stop2_timebase_delta
end

define ps_stop2_timebase_persisted
  printf "== STOP2 persisted timebase ==\n"
  printf "persist: load_ok=%lu magic=0x%08lx\n", (unsigned long)g_power_stop2_tb_persist_load_ok, (unsigned long)g_power_stop2_tb_persist_magic
  printf "samples=%lu last_wake=%lu\n", (unsigned long)g_power_stop2_tb_sample_count, (unsigned long)g_power_stop2_tb_last_wake_count
  printf "awake_window_ms: hal_dt=%lu tx_dt=%lu abs_diff=%lu\n", (unsigned long)g_power_stop2_tb_last_hal_dt, (unsigned long)g_power_stop2_tb_last_tx_dt, (unsigned long)g_power_stop2_tb_last_abs_diff
  printf "awake_window_max_abs_diff_ms=%lu\n", (unsigned long)g_power_stop2_tb_max_abs_diff
  ps_stop2_status_min
end

define ps_stop2_timebase_persisted_clear
  p App_Power_Stop2TimebaseTelemetryClear()
  ps_stop2_timebase_persisted
end

define ps_pet_status
  printf "== Pet status ==\n"
  printf "state=%lu tick=%lu wake=%lu last_action=%lu h=%lu e=%lu m=%lu\n", (unsigned long)g_pet_state, (unsigned long)g_pet_tick_count, (unsigned long)g_pet_wake_count, (unsigned long)g_pet_last_action, (unsigned long)g_pet_hunger_pct, (unsigned long)g_pet_energy_pct, (unsigned long)g_pet_mood_pct
end

define ps_retained_status
  printf "== Retained state ==\n"
  printf "magic=0x%08lx seq=%lu valid_mask=0x%08lx crc_ok=%lu\n", (unsigned long)g_retained_state_magic, (unsigned long)g_retained_state_seq, (unsigned long)g_retained_state_valid_mask, (unsigned long)g_retained_state_crc_ok
  printf "load: ok=%lu fail=%lu save: ok=%lu fail=%lu\n", (unsigned long)g_retained_state_load_ok_count, (unsigned long)g_retained_state_load_fail_count, (unsigned long)g_retained_state_save_ok_count, (unsigned long)g_retained_state_save_fail_count
  printf "game: mode=%lu backend=%lu topdown_valid=%lu save_ok=%lu save_fail=%lu restore_ok=%lu restore_fail=%lu\n", (unsigned long)g_retained_state_game_mode_id, (unsigned long)g_retained_state_game_backend_id, (unsigned long)g_retained_state_game_topdown_valid, (unsigned long)g_retained_state_game_save_ok_count, (unsigned long)g_retained_state_game_save_fail_count, (unsigned long)g_retained_state_game_restore_ok_count, (unsigned long)g_retained_state_game_restore_fail_count
end

define ps_retained_clear
  set $ps_rc = (unsigned int)App_RetainedStateClear()
  printf "retained clear rc=%u\n", $ps_rc
  ps_retained_status
end

define ps_power_input_status
  set $ps_now_ms = (unsigned long)HAL_GetTick()
  set $ps_now_tx = (unsigned long)_tx_timer_system_clock
  printf "== Power input/inactivity ==\n"
  printf "now_ms=%lu now_tx=%lu last_input_tx=%lu age_tx=%lu\n", $ps_now_ms, $ps_now_tx, (unsigned long)g_power_last_input_tick, (unsigned long)($ps_now_tx - (unsigned long)g_power_last_input_tick)
  printf "power_input_activity=%lu power_menu_events=%lu\n", (unsigned long)g_power_input_activity_count, (unsigned long)g_power_menu_event_count
  printf "input_syspost: activity_ok=%lu activity_drop=%lu last_activity_tick=%lu\n", (unsigned long)g_input_sys_activity_post_count, (unsigned long)g_input_sys_activity_drop_count, (unsigned long)g_input_sys_activity_last_tick
  printf "last_raw: src=%lu edge=%lu tick=%lu level=%lu joy_mask=0x%08lx\n", (unsigned long)g_input_last_source, (unsigned long)g_input_last_edge, (unsigned long)g_input_last_tick, (unsigned long)g_input_last_level, (unsigned long)g_sensor_joy_input_mask
  ps_input_decode_last
  ps_mode_flags
  ps_power_flags
  ps_pet_status
  printf "stop_select: active=%lu last_input_tx=%lu\n", (unsigned long)g_power_stop_select_active, (unsigned long)g_power_stop_select_last_input_tick
  ps_stop2_status_min
end

define ps_stop_select_status
  set $ps_now_tx = (unsigned long)_tx_timer_system_clock
  printf "== STOP select ==\n"
  printf "active=%lu now_tx=%lu last_input_tx=%lu age_tx=%lu\n", (unsigned long)g_power_stop_select_active, $ps_now_tx, (unsigned long)g_power_stop_select_last_input_tick, (unsigned long)($ps_now_tx - (unsigned long)g_power_stop_select_last_input_tick)
  printf "note: timeout uses knob rtos_power_stop_select_timeout_ticks\n"
end

define __ps_stop2_decode_wusr
  set $ps_wusr = $arg0
  printf "wusr_flags:"
  set $__ps_any = 0
  set $__ps_i = 0
  while $__ps_i < 8
    set $__ps_mask = (1 << $__ps_i)
    if (($ps_wusr & $__ps_mask) != 0)
      printf " WUF%u", $__ps_i + 1
      set $__ps_any = 1
    end
    set $__ps_i = $__ps_i + 1
  end
  if $__ps_any == 0
    printf " none"
  end
  printf "\n"
end

define __ps_stop2_decode_sr
  set $ps_sr = $arg0
  printf "sr_flags:"
  set $__ps_any = 0
  if (($ps_sr & 0x00000002) != 0)
    printf " STOPF"
    set $__ps_any = 1
  end
  if (($ps_sr & 0x00000004) != 0)
    printf " SBF"
    set $__ps_any = 1
  end
  if (($ps_sr & 0x00000001) != 0)
    printf " CSSF"
    set $__ps_any = 1
  end
  if $__ps_any == 0
    printf " none"
  end
  printf "\n"
end

define ps_stop2_wake_decode
  printf "== STOP2 wake decode ==\n"
  printf "latched: wusr=0x%08lx sr=0x%08lx\n", (unsigned long)g_power_stop2_last_wusr, (unsigned long)g_power_stop2_last_sr
  __ps_stop2_decode_wusr g_power_stop2_last_wusr
  __ps_stop2_decode_sr g_power_stop2_last_sr
  set $ps_live_wusr = *((unsigned long *)0x46020844)
  set $ps_live_sr = *((unsigned long *)0x46020838)
  printf "live:    wusr=0x%08lx sr=0x%08lx\n", (unsigned long)$ps_live_wusr, (unsigned long)$ps_live_sr
  __ps_stop2_decode_wusr $ps_live_wusr
  __ps_stop2_decode_sr $ps_live_sr
end

define ps_stop2_power_snapshot
  set $ps_rcc = (RCC_TypeDef *)0x46020c00
  set $ps_pwr = (PWR_TypeDef *)0x46020800
  set $ps_dbgcr = *((unsigned long *)0xE0044004)
  printf "== STOP2 power snapshot ==\n"
  printf "mode=0x%08lx power=0x%08lx sysclk=%lu stop2(entry=%lu wake=%lu abort=%lu armed=%lu)\n", (unsigned long)g_eg_mode.tx_event_flags_group_current, (unsigned long)g_eg_power.tx_event_flags_group_current, (unsigned long)HAL_RCC_GetSysClockFreq(), (unsigned long)g_power_stop2_entry_count, (unsigned long)g_power_stop2_wake_count, (unsigned long)g_power_stop2_abort_count, (unsigned long)g_power_stop2_armed
  printf "dbg: DBGMCU_CR=0x%08lx hold_in_stop=%lu\n", (unsigned long)$ps_dbgcr, (unsigned long)(($ps_dbgcr & 0x00000006) != 0)
  printf "rcc_raw: CR=0x%08lx CFGR1=0x%08lx CFGR2=0x%08lx CFGR3=0x%08lx BDCR=0x%08lx CCIPR1=0x%08lx CCIPR2=0x%08lx CCIPR3=0x%08lx\n", (unsigned long)$ps_rcc->CR, (unsigned long)$ps_rcc->CFGR1, (unsigned long)$ps_rcc->CFGR2, (unsigned long)$ps_rcc->CFGR3, (unsigned long)$ps_rcc->BDCR, (unsigned long)$ps_rcc->CCIPR1, (unsigned long)$ps_rcc->CCIPR2, (unsigned long)$ps_rcc->CCIPR3
  printf "rcc_en: AHB1ENR=0x%08lx AHB2ENR1=0x%08lx AHB2ENR2=0x%08lx AHB3ENR=0x%08lx APB1ENR1=0x%08lx APB2ENR=0x%08lx APB3ENR=0x%08lx\n", (unsigned long)$ps_rcc->AHB1ENR, (unsigned long)$ps_rcc->AHB2ENR1, (unsigned long)$ps_rcc->AHB2ENR2, (unsigned long)$ps_rcc->AHB3ENR, (unsigned long)$ps_rcc->APB1ENR1, (unsigned long)$ps_rcc->APB2ENR, (unsigned long)$ps_rcc->APB3ENR
  printf "rcc_smen: AHB1SMENR=0x%08lx AHB2SMENR1=0x%08lx AHB2SMENR2=0x%08lx AHB3SMENR=0x%08lx APB1SMENR1=0x%08lx APB2SMENR=0x%08lx APB3SMENR=0x%08lx\n", (unsigned long)$ps_rcc->AHB1SMENR, (unsigned long)$ps_rcc->AHB2SMENR1, (unsigned long)$ps_rcc->AHB2SMENR2, (unsigned long)$ps_rcc->AHB3SMENR, (unsigned long)$ps_rcc->APB1SMENR1, (unsigned long)$ps_rcc->APB2SMENR, (unsigned long)$ps_rcc->APB3SMENR
  set $ps_sws = (unsigned long)(($ps_rcc->CFGR1 >> 2) & 0x3)
  printf "sysclk_src=%lu ", (unsigned long)$ps_sws
  if ($ps_sws == 0)
    printf "(MSIS)"
  else
    if ($ps_sws == 1)
      printf "(HSI16)"
    else
      if ($ps_sws == 2)
        printf "(HSE)"
      else
        if ($ps_sws == 3)
          printf "(PLL1)"
        else
          printf "(unknown)"
        end
      end
    end
  end
  printf "\n"
  printf "osc: msis=%lu/%lu msik=%lu/%lu hsi16=%lu/%lu hsi48=%lu/%lu pll1=%lu/%lu pll2=%lu/%lu pll3=%lu/%lu lse=%lu/%lu rtcen=%lu\n", (unsigned long)(($ps_rcc->CR & 0x00000001) != 0), (unsigned long)(($ps_rcc->CR & 0x00000004) != 0), (unsigned long)(($ps_rcc->CR & 0x00000010) != 0), (unsigned long)(($ps_rcc->CR & 0x00000020) != 0), (unsigned long)(($ps_rcc->CR & 0x00000100) != 0), (unsigned long)(($ps_rcc->CR & 0x00000400) != 0), (unsigned long)(($ps_rcc->CR & 0x00001000) != 0), (unsigned long)(($ps_rcc->CR & 0x00002000) != 0), (unsigned long)(($ps_rcc->CR & 0x01000000) != 0), (unsigned long)(($ps_rcc->CR & 0x02000000) != 0), (unsigned long)(($ps_rcc->CR & 0x04000000) != 0), (unsigned long)(($ps_rcc->CR & 0x08000000) != 0), (unsigned long)(($ps_rcc->CR & 0x10000000) != 0), (unsigned long)(($ps_rcc->CR & 0x20000000) != 0), (unsigned long)(($ps_rcc->BDCR & 0x00000001) != 0), (unsigned long)(($ps_rcc->BDCR & 0x00000002) != 0), (unsigned long)(($ps_rcc->BDCR & 0x00008000) != 0)
  printf "periph_en: gpdma1=%lu lpdma1=%lu tim2=%lu otg=%lu octospim=%lu octospi1=%lu sai1=%lu spi3=%lu lpuart1=%lu i2c3=%lu\n", (unsigned long)(($ps_rcc->AHB1ENR & 0x00000001) != 0), (unsigned long)(($ps_rcc->AHB3ENR & 0x00000200) != 0), (unsigned long)(($ps_rcc->APB1ENR1 & 0x00000001) != 0), (unsigned long)(($ps_rcc->AHB2ENR1 & 0x00004000) != 0), (unsigned long)(($ps_rcc->AHB2ENR1 & 0x00200000) != 0), (unsigned long)(($ps_rcc->AHB2ENR2 & 0x00000010) != 0), (unsigned long)(($ps_rcc->APB2ENR & 0x00200000) != 0), (unsigned long)(($ps_rcc->APB3ENR & 0x00000020) != 0), (unsigned long)(($ps_rcc->APB3ENR & 0x00000040) != 0), (unsigned long)(($ps_rcc->APB3ENR & 0x00000080) != 0)
  printf "periph_smen: gpdma1=%lu lpdma1=%lu tim2=%lu otg=%lu octospim=%lu octospi1=%lu sai1=%lu spi3=%lu lpuart1=%lu i2c3=%lu\n", (unsigned long)(($ps_rcc->AHB1SMENR & 0x00000001) != 0), (unsigned long)(($ps_rcc->AHB3SMENR & 0x00000200) != 0), (unsigned long)(($ps_rcc->APB1SMENR1 & 0x00000001) != 0), (unsigned long)(($ps_rcc->AHB2SMENR1 & 0x00004000) != 0), (unsigned long)(($ps_rcc->AHB2SMENR1 & 0x00200000) != 0), (unsigned long)(($ps_rcc->AHB2SMENR2 & 0x00000010) != 0), (unsigned long)(($ps_rcc->APB2SMENR & 0x00200000) != 0), (unsigned long)(($ps_rcc->APB3SMENR & 0x00000020) != 0), (unsigned long)(($ps_rcc->APB3SMENR & 0x00000040) != 0), (unsigned long)(($ps_rcc->APB3SMENR & 0x00000080) != 0)
  printf "pwr_raw: CR1=0x%08lx CR2=0x%08lx CR3=0x%08lx VOSR=0x%08lx SVMCR=0x%08lx SVMSR=0x%08lx SR=0x%08lx WUSR=0x%08lx BDCR1=0x%08lx BDCR2=0x%08lx UCPDR=0x%08lx\n", (unsigned long)$ps_pwr->CR1, (unsigned long)$ps_pwr->CR2, (unsigned long)$ps_pwr->CR3, (unsigned long)$ps_pwr->VOSR, (unsigned long)$ps_pwr->SVMCR, (unsigned long)$ps_pwr->SVMSR, (unsigned long)$ps_pwr->SR, (unsigned long)$ps_pwr->WUSR, (unsigned long)$ps_pwr->BDCR1, (unsigned long)$ps_pwr->BDCR2, (unsigned long)$ps_pwr->UCPDR
  printf "pwr_decode: vos=%lu vosrdy=%lu boosten=%lu boostrdy=%lu regs=%lu usv=%lu stopf=%lu sbf=%lu\n", (unsigned long)(($ps_pwr->VOSR >> 16) & 0x3), (unsigned long)(($ps_pwr->VOSR & 0x00008000) != 0), (unsigned long)(($ps_pwr->VOSR & 0x00040000) != 0), (unsigned long)(($ps_pwr->VOSR & 0x00004000) != 0), (unsigned long)(($ps_pwr->SVMSR & 0x00000002) != 0), (unsigned long)(($ps_pwr->SVMCR & 0x10000000) != 0), (unsigned long)(($ps_pwr->SR & 0x00000002) != 0), (unsigned long)(($ps_pwr->SR & 0x00000004) != 0)
  if ((unsigned long)hrtc.Instance != 0)
    set $ps_rtc = (RTC_TypeDef *)hrtc.Instance
    printf "rtc: base=0x%08lx TR=0x%08lx DR=0x%08lx ICSR=0x%08lx WUTR=0x%08lx CR=0x%08lx SR=0x%08lx\n", (unsigned long)$ps_rtc, (unsigned long)$ps_rtc->TR, (unsigned long)$ps_rtc->DR, (unsigned long)$ps_rtc->ICSR, (unsigned long)$ps_rtc->WUTR, (unsigned long)$ps_rtc->CR, (unsigned long)$ps_rtc->SR
  else
    printf "rtc: hrtc.Instance is null\n"
  end
  printf "handles: usb(active=%lu state=%u err=0x%08lx) ospi(state=%u err=0x%08lx) spi3(state=%u err=0x%08lx)\n", (unsigned long)g_usb_device_active, (unsigned int)hpcd_USB_OTG_FS.State, (unsigned long)hpcd_USB_OTG_FS.ErrorCode, (unsigned int)hospi1.State, (unsigned long)hospi1.ErrorCode, (unsigned int)hspi3.State, (unsigned long)hspi3.ErrorCode
  printf "handles: i2c3(state=%u err=0x%08lx) sai1a(state=%u err=0x%08lx) lpuart1(g=%u rx=%u err=0x%08lx)\n", (unsigned int)hi2c3.State, (unsigned long)hi2c3.ErrorCode, (unsigned int)hsai_BlockA1.State, (unsigned long)hsai_BlockA1.ErrorCode, (unsigned int)hlpuart1.gState, (unsigned int)hlpuart1.RxState, (unsigned long)hlpuart1.ErrorCode
  printf "owners: flash_ready=%lu audio_state=%lu sensor_token=%lu sensor_flags=0x%08lx pending_ack=0x%08lx\n", (unsigned long)g_storage_flash_ready, (unsigned long)g_audio_state, (unsigned long)g_sensor_mode_token, (unsigned long)g_eg_sensor_health.tx_event_flags_group_current, (unsigned long)g_power_pending_ack_mask
end

define ps_stop2_power_snapshot_full
  ps_stop2_power_snapshot
  ps_power_perf
  ps_retained_status
  ps_stop2_status
  ps_stop2_wake_decode
  ps_usb_status
  ps_storage_status
  ps_sensor_health
  ps_pmic_diag
  ps_lis_diag
  ps_audio_status
  ps_input_status
end

define ps_stop2_prep_smoke
  printf "== STOP2 preflight smoke ==\n"
  ps_dbg_lp_policy
  set $ps_q_active0 = (unsigned long)g_power_quiesce_wait_active
  set $ps_q_elapsed0 = (unsigned long)g_power_quiesce_wait_elapsed_ticks
  set $ps_q_ack0 = (unsigned long)g_power_pending_ack_mask
  ps_smoke
  printf "quiesce trackers: active %lu->%lu elapsed %lu->%lu pending_ack 0x%08lx->0x%08lx\n", (unsigned long)$ps_q_active0, (unsigned long)g_power_quiesce_wait_active, (unsigned long)$ps_q_elapsed0, (unsigned long)g_power_quiesce_wait_elapsed_ticks, (unsigned long)$ps_q_ack0, (unsigned long)g_power_pending_ack_mask
  ps_dbg_lp_policy
end

define __ps_ensure_wait_bp
  if $_isvoid($ps_wait_bp)
    set $ps_wait_bp = 0
  end
  if ($ps_wait_bp == 0)
    break __tx_ts_wait
    set $ps_wait_bp = $bpnum
    commands $ps_wait_bp
      silent
    end
  end
end

define __ps_release_wait_bp
  if $_isvoid($ps_wait_bp)
    set $ps_wait_bp = 0
  end
  if ($ps_wait_bp != 0)
    delete $ps_wait_bp
    set $ps_wait_bp = 0
  end
end

define __ps_continue_to_idle
  __ps_ensure_wait_bp
  ignore $ps_wait_bp 0
  continue
end

define __ps_continue_for_ticks
  set $ps_wait_ticks = (unsigned long)$arg0
  if ($ps_wait_ticks == 0)
    set $ps_wait_ticks = 1
  end
  __ps_ensure_wait_bp
  ignore $ps_wait_bp $ps_wait_ticks
  continue
end

define ps_stop2_audio_soak
  set $ps_soak_stop_ticks = 24
  set $ps_soak_static_ticks = 4
  set $ps_soak_run_ticks = 12
  set $ps_soak_i = 0
  while ($ps_soak_i < 10)
    ps_mode_stop
    __ps_continue_for_ticks $ps_soak_stop_ticks
    ps_mode_static
    __ps_continue_for_ticks $ps_soak_static_ticks
    p App_AudioReq_PlayEvent(APP_AUDIO_EVENT_UI_CONFIRM)
    __ps_continue_for_ticks $ps_soak_run_ticks
    set $ps_soak_i = $ps_soak_i + 1
  end
  if $_isvoid($ps_wait_bp)
    set $ps_wait_bp = 0
  end
  if ($ps_wait_bp != 0)
    delete $ps_wait_bp
    set $ps_wait_bp = 0
  end
  ps_stop2_status
  ps_audio_status
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

define ps_usb_status
  printf "== USB status ==\n"
  printf "pmic: vbus_present=%lu known=%lu\n", (unsigned long)g_sensor_pmic_live.vbus_present, (unsigned long)g_sensor_pmic_vbus_known
  printf "mode: egMode=0x%08lx\n", (unsigned long)g_eg_mode.tx_event_flags_group_current
  printf "usbx-init: pool_status=%u init_status=%u pool_fail=%lu init_ok=%lu init_fail=%lu stage=%u err=%u\n", (unsigned int)g_usbx_device_pool_create_status, (unsigned int)g_usbx_device_init_status, (unsigned long)g_usbx_device_pool_create_fail_count, (unsigned long)g_usbx_device_init_ok_count, (unsigned long)g_usbx_device_init_fail_count, (unsigned int)g_usbx_init_stage, (unsigned int)g_usbx_init_error_code
  printf "usb: active=%lu start_ok=%lu start_fail=%lu stop_ok=%lu stop_fail=%lu last_err=%ld\n", (unsigned long)g_usb_device_active, (unsigned long)g_usb_device_start_ok_count, (unsigned long)g_usb_device_start_fail_count, (unsigned long)g_usb_device_stop_ok_count, (unsigned long)g_usb_device_stop_fail_count, (long)g_usb_device_last_error
  printf "irq-guard: drops=%lu\n", (unsigned long)g_usb_irq_guard_drop_count
  printf "filex: mounted=%lu lx_open=%lu m_ok=%lu m_fail=%lu um_ok=%lu um_fail=%lu fx_status=%lu\n", (unsigned long)g_storage_filex_mounted, (unsigned long)g_storage_usb_msc_lx_opened, (unsigned long)g_storage_filex_mount_count, (unsigned long)g_storage_filex_mount_fail_count, (unsigned long)g_storage_filex_unmount_count, (unsigned long)g_storage_filex_unmount_fail_count, (unsigned long)g_storage_filex_last_status
  printf "msc: read=%lu write=%lu flush=%lu status=%lu fail=%lu mtx_fail=%lu q_fail=%lu done_fail=%lu mode_reject=%lu last_type=%lu last_lba=%lu last_blocks=%lu last_req=%lu last_media=%lu\n", (unsigned long)g_storage_usb_msc_req_read_count, (unsigned long)g_storage_usb_msc_req_write_count, (unsigned long)g_storage_usb_msc_req_flush_count, (unsigned long)g_storage_usb_msc_req_status_count, (unsigned long)g_storage_usb_msc_req_fail_count, (unsigned long)g_storage_usb_msc_req_mutex_fail_count, (unsigned long)g_storage_usb_msc_req_queue_fail_count, (unsigned long)g_storage_usb_msc_req_done_fail_count, (unsigned long)g_storage_usb_msc_req_mode_reject_count, (unsigned long)g_storage_usb_msc_last_req_type, (unsigned long)g_storage_usb_msc_last_lba, (unsigned long)g_storage_usb_msc_last_blocks, (unsigned long)g_storage_usb_msc_last_req_status, (unsigned long)g_storage_usb_msc_last_media_status
  printf "msc_recover: pending=%lu trig=%lu attempt=%lu ok=%lu fail=%lu reason=%lu req_type=%lu req_status=%lu media=%lu mode=0x%08lx active_before=%lu active_after=%lu\n", (unsigned long)g_usb_msc_recover_pending, (unsigned long)g_usb_msc_recover_trigger_count, (unsigned long)g_usb_msc_recover_attempt_count, (unsigned long)g_usb_msc_recover_ok_count, (unsigned long)g_usb_msc_recover_fail_count, (unsigned long)g_usb_msc_recover_reason, (unsigned long)g_usb_msc_recover_last_req_type, (unsigned long)g_usb_msc_recover_last_req_status, (unsigned long)g_usb_msc_recover_last_media_status, (unsigned long)g_usb_msc_recover_last_mode_flags, (unsigned long)g_usb_msc_recover_last_usb_active_before, (unsigned long)g_usb_msc_recover_last_usb_active_after
  printf "scsi: cbw=%lu last=0x%02lx flags=0x%02lx host_len=%lu unknown=%lu last_unknown=0x%02lx csw=0x%02lx\n", (unsigned long)g_usbx_scsi_cbw_count, (unsigned long)g_usbx_scsi_last_opcode, (unsigned long)g_usbx_scsi_last_cbw_flags, (unsigned long)g_usbx_scsi_last_host_length, (unsigned long)g_usbx_scsi_unknown_count, (unsigned long)g_usbx_scsi_unknown_opcode, (unsigned long)g_usbx_scsi_last_csw_status
end

define ps_usb_scsi_trace
  printf "== USB SCSI trace ==\n"
  printf "wr=%lu count=%lu depth=16\n", (unsigned long)g_usbx_scsi_trace_wr, (unsigned long)g_usbx_scsi_trace_count
  set $ps_depth = 16
  set $ps_count = (unsigned long)g_usbx_scsi_trace_count
  if ($ps_count > $ps_depth)
    set $ps_count = $ps_depth
  end
  set $ps_i = 0
  while ($ps_i < $ps_count)
    set $ps_idx = (int)((((unsigned long)g_usbx_scsi_trace_wr + $ps_depth) - 1 - $ps_i) % $ps_depth)
    printf "#%lu idx=%d op=0x%02lx host=%lu flags=0x%02lx cmd=0x%08lx csw=0x%02lx csw_send=0x%08lx residue=%lu sense=0x%06lx out(st=%lu cc=%lu req=%lu act=%lu)\n", (unsigned long)$ps_i, $ps_idx, (unsigned long)g_usbx_scsi_trace_opcode[$ps_idx], (unsigned long)g_usbx_scsi_trace_host_len[$ps_idx], (unsigned long)g_usbx_scsi_trace_flags[$ps_idx], (unsigned long)g_usbx_scsi_trace_cmd_status[$ps_idx], (unsigned long)g_usbx_scsi_trace_csw_status[$ps_idx], (unsigned long)g_usbx_scsi_trace_csw_send_status[$ps_idx], (unsigned long)g_usbx_scsi_trace_residue[$ps_idx], (unsigned long)g_usbx_scsi_trace_sense[$ps_idx], (unsigned long)g_usbx_scsi_trace_out_status[$ps_idx], (unsigned long)g_usbx_scsi_trace_out_completion[$ps_idx], (unsigned long)g_usbx_scsi_trace_out_requested[$ps_idx], (unsigned long)g_usbx_scsi_trace_out_actual[$ps_idx]
    set $ps_i = $ps_i + 1
  end
end

define ps_usb_scsi_trace_reset
  set g_usbx_scsi_trace_wr = 0
  set g_usbx_scsi_trace_count = 0
  set g_usbx_scsi_cbw_count = 0
  set g_usbx_scsi_last_opcode = 0
  set g_usbx_scsi_last_cbw_flags = 0
  set g_usbx_scsi_last_host_length = 0
  set g_usbx_scsi_unknown_count = 0
  set g_usbx_scsi_unknown_opcode = 0
  set g_usbx_scsi_last_csw_status = 0
  set $ps_i = 0
  while ($ps_i < 16)
    set g_usbx_scsi_trace_opcode[$ps_i] = 0
    set g_usbx_scsi_trace_host_len[$ps_i] = 0
    set g_usbx_scsi_trace_flags[$ps_i] = 0
    set g_usbx_scsi_trace_cmd_status[$ps_i] = 0
    set g_usbx_scsi_trace_csw_status[$ps_i] = 0
    set g_usbx_scsi_trace_csw_send_status[$ps_i] = 0
    set g_usbx_scsi_trace_residue[$ps_i] = 0
    set g_usbx_scsi_trace_sense[$ps_i] = 0
    set g_usbx_scsi_trace_out_status[$ps_i] = 0
    set g_usbx_scsi_trace_out_completion[$ps_i] = 0
    set g_usbx_scsi_trace_out_requested[$ps_i] = 0
    set g_usbx_scsi_trace_out_actual[$ps_i] = 0
    set $ps_i = $ps_i + 1
  end
  printf "USB SCSI trace reset\n"
end

define ps_storage_boot_sector0
  set $ps_rc = (unsigned int)App_StorageReq_UsbMscRead(0, 1, (unsigned char*)g_storage_filex_cache, &g_storage_usb_msc_last_media_status)
  printf "msc_read0: rc=%u media=0x%08lx\n", $ps_rc, (unsigned long)g_storage_usb_msc_last_media_status
  x/64bx g_storage_filex_cache
  x/2bx g_storage_filex_cache+510
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
  printf "pmic: state=%lu fails=%lu tries=%lu retry_tick=%lu ok=%lu err=%ld\n", (unsigned long)g_sensor_pmic.state, (unsigned long)g_sensor_pmic.fail_count, (unsigned long)g_sensor_pmic.recovery_attempts, (unsigned long)g_sensor_pmic.next_retry_tick, (unsigned long)g_sensor_pmic.last_success_tick, (long)g_sensor_pmic.last_error
  printf "tmag: state=%lu fails=%lu tries=%lu retry_tick=%lu ok=%lu err=%ld\n", (unsigned long)g_sensor_tmag.state, (unsigned long)g_sensor_tmag.fail_count, (unsigned long)g_sensor_tmag.recovery_attempts, (unsigned long)g_sensor_tmag.next_retry_tick, (unsigned long)g_sensor_tmag.last_success_tick, (long)g_sensor_tmag.last_error
  printf "lis : state=%lu fails=%lu tries=%lu retry_tick=%lu ok=%lu err=%ld\n", (unsigned long)g_sensor_lis.state, (unsigned long)g_sensor_lis.fail_count, (unsigned long)g_sensor_lis.recovery_attempts, (unsigned long)g_sensor_lis.next_retry_tick, (unsigned long)g_sensor_lis.last_success_tick, (long)g_sensor_lis.last_error
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
  printf "fsm: state=%lu fails=%lu tries=%lu retry_tick=%lu ok=%lu err=%ld\n", (unsigned long)g_sensor_pmic.state, (unsigned long)g_sensor_pmic.fail_count, (unsigned long)g_sensor_pmic.recovery_attempts, (unsigned long)g_sensor_pmic.next_retry_tick, (unsigned long)g_sensor_pmic.last_success_tick, (long)g_sensor_pmic.last_error
  printf "live: sample=%lu fail=%lu trans_fail=%lu fault_evt=%lu last_tick=%lu lerr=%ld lterr=%ld\n", (unsigned long)g_sensor_pmic_live.sample_count, (unsigned long)g_sensor_pmic_live.fail_count, (unsigned long)g_sensor_pmic_live.transport_error_count, (unsigned long)g_sensor_pmic_live.fault_event_count, (unsigned long)g_sensor_pmic_live.last_sample_tick, (long)g_sensor_pmic_live.last_error, (long)g_sensor_pmic_live.last_transport_error
  printf "vbat: mv=%lu raw=%lu soc=%lu(raw=%lu) status2=0x%02lx fault=0x%02lx pgood=0x%02lx\n", (unsigned long)g_sensor_pmic_live.vbat_mV, (unsigned long)g_sensor_pmic_live.vbat_raw, (unsigned long)g_sensor_pmic_live.battery_soc_percent, (unsigned long)g_sensor_pmic_live.battery_soc_raw, (unsigned long)g_sensor_pmic_live.status2_raw, (unsigned long)g_sensor_pmic_live.fault_raw, (unsigned long)g_sensor_pmic_live.pgood_raw
  printf "bat: health=%lu reason=0x%02lx\n", (unsigned long)g_sensor_pmic_live.battery_health_state, (unsigned long)g_sensor_pmic_live.battery_health_reason
  printf "chg: enabled_cfg=%lu active=%lu state=%lu last_fault=0x%02lx\n", (unsigned long)g_sensor_pmic_live.charging_enabled_cfg, (unsigned long)g_sensor_pmic_live.charging_active, (unsigned long)g_sensor_pmic_live.charger_state, (unsigned long)g_sensor_pmic_live.last_fault_mask
  printf "guard: en=%lu cutoff=%lu hys=%lu confirm=%lu streak=%lu latched=%lu isofet_off=%lu\n", (unsigned long)g_sensor_pmic_live.guard_enabled, (unsigned long)g_sensor_pmic_live.cutoff_mv, (unsigned long)g_sensor_pmic_live.cutoff_hys_mv, (unsigned long)g_sensor_pmic_live.cutoff_confirm_samples, (unsigned long)g_sensor_pmic_live.cutoff_low_streak, (unsigned long)g_sensor_pmic_live.cutoff_latched, (unsigned long)g_sensor_pmic_live.isofet_forced_off
end

define ps_lis_diag
  printf "== LIS diag ==\n"
  printf "fsm: state=%lu fails=%lu tries=%lu retry_tick=%lu ok=%lu err=%ld\n", (unsigned long)g_sensor_lis.state, (unsigned long)g_sensor_lis.fail_count, (unsigned long)g_sensor_lis.recovery_attempts, (unsigned long)g_sensor_lis.next_retry_tick, (unsigned long)g_sensor_lis.last_success_tick, (long)g_sensor_lis.last_error
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
  printf "layout: settings=0x%08lx smoke=0x%08lx manifest=0x%08lx/%lu audio=0x%08lx/%lu installed=0x%08lx/%lu fat=0x%08lx/%lu\n", (unsigned long)g_storage_settings_addr_dbg, (unsigned long)g_storage_smoke_addr_dbg, (unsigned long)g_storage_game_pkg_manifest_addr_dbg, (unsigned long)g_storage_game_pkg_manifest_max_bytes_dbg, (unsigned long)g_storage_audio_catalog_addr_dbg, (unsigned long)g_storage_audio_catalog_max_bytes_dbg, (unsigned long)g_storage_installed_base_addr_dbg, (unsigned long)g_storage_installed_size_bytes_dbg, (unsigned long)g_storage_fat_base_addr_dbg, (unsigned long)g_storage_fat_size_bytes_dbg
  printf "joycfg: valid=%lu err=%ld load_ok=%lu load_fail=%lu save_ok=%lu save_fail=%lu lseq=%lu sseq=%lu addr=0x%08lx\n", (unsigned long)g_storage_joycfg_valid, (long)g_storage_joycfg_last_error, (unsigned long)g_storage_joycfg_load_ok_count, (unsigned long)g_storage_joycfg_load_fail_count, (unsigned long)g_storage_joycfg_save_ok_count, (unsigned long)g_storage_joycfg_save_fail_count, (unsigned long)g_storage_joycfg_load_seq, (unsigned long)g_storage_joycfg_save_seq, (unsigned long)g_storage_settings_addr_dbg
  printf "audio_cat: loaded=%lu ok=%lu fail=%lu addr=0x%08lx entries=%lu ver=%lu hdr=%u entry=%u crc_hdr=0x%08lx crc_calc=0x%08lx\n", (unsigned long)g_storage_audio_catalog_loaded, (unsigned long)g_storage_audio_catalog_load_ok_count, (unsigned long)g_storage_audio_catalog_load_fail_count, (unsigned long)g_storage_audio_catalog_addr, (unsigned long)g_storage_audio_catalog_entry_count, (unsigned long)g_storage_audio_catalog_version, (unsigned int)g_storage_audio_catalog_header.header_size, (unsigned int)g_storage_audio_catalog_header.entry_size, (unsigned long)g_storage_audio_catalog_header.table_crc32, (unsigned long)g_storage_audio_catalog_table_crc32
  printf "pkg_manifest: loaded=%lu ok=%lu fail=%lu imp_ok=%lu imp_fail=%lu imp_stat=%lu imp_bytes=%lu erase_ok=%lu erase_fail=%lu addr=0x%08lx size=%lu status=%lu src=%lu pkg=%lu/%lu m=%lu r=%lu\n", (unsigned long)g_storage_game_package_manifest_loaded, (unsigned long)g_storage_game_package_manifest_load_ok_count, (unsigned long)g_storage_game_package_manifest_load_fail_count, (unsigned long)g_storage_game_package_manifest_import_ok_count, (unsigned long)g_storage_game_package_manifest_import_fail_count, (unsigned long)g_storage_game_package_manifest_import_last_status, (unsigned long)g_storage_game_package_manifest_import_last_bytes, (unsigned long)g_storage_game_package_manifest_erase_ok_count, (unsigned long)g_storage_game_package_manifest_erase_fail_count, (unsigned long)g_storage_game_package_manifest_addr, (unsigned long)g_storage_game_package_manifest_size, (unsigned long)g_storage_game_package_manifest_last_status, (unsigned long)g_storage_game_package_source, (unsigned long)g_storage_game_package_id, (unsigned long)g_storage_game_package_version, (unsigned long)g_storage_game_package_mode_count, (unsigned long)g_storage_game_package_pet_route_count
  printf "scene_import: ok=%lu fail=%lu last_status=%lu map_bytes=%lu tileset_bytes=%lu\n", (unsigned long)g_storage_game_package_scene_import_ok_count, (unsigned long)g_storage_game_package_scene_import_fail_count, (unsigned long)g_storage_game_package_scene_import_last_status, (unsigned long)g_storage_game_package_scene_import_map_bytes, (unsigned long)g_storage_game_package_scene_import_tileset_bytes
  printf "install_idx: valid=%lu l_ok=%lu l_fail=%lu w_ok=%lu w_fail=%lu slot=%lu seq=%lu lstat=%lu wstat=%lu scan=%lu pkg=%lu/%lu blob_off=0x%08lx blob_size=%lu m=0x%08lx/%lu\n", (unsigned long)g_storage_install_index_valid, (unsigned long)g_storage_install_index_load_ok_count, (unsigned long)g_storage_install_index_load_fail_count, (unsigned long)g_storage_install_index_write_ok_count, (unsigned long)g_storage_install_index_write_fail_count, (unsigned long)g_storage_install_index_active_slot, (unsigned long)g_storage_install_index_sequence, (unsigned long)g_storage_install_index_load_last_status, (unsigned long)g_storage_install_index_write_last_status, (unsigned long)g_storage_install_index_scan_done, (unsigned long)g_storage_install_index_package_id, (unsigned long)g_storage_install_index_package_version, (unsigned long)g_storage_install_index_blob_offset, (unsigned long)g_storage_install_index_blob_size, (unsigned long)g_storage_install_index_manifest_addr, (unsigned long)g_storage_install_index_manifest_size
  printf "scene_map: loaded=%lu ok=%lu fail=%lu addr=0x%08lx size=%lu status=%lu wh=%lux%lu tiles=%lu objs=%lu\n", (unsigned long)g_storage_scene_map_loaded, (unsigned long)g_storage_scene_map_load_ok_count, (unsigned long)g_storage_scene_map_load_fail_count, (unsigned long)g_storage_scene_map_addr, (unsigned long)g_storage_scene_map_size, (unsigned long)g_storage_scene_map_last_status, (unsigned long)g_storage_scene_map_width, (unsigned long)g_storage_scene_map_height, (unsigned long)g_storage_scene_map_tile_count, (unsigned long)g_storage_scene_map_object_count
  printf "scene_tileset: loaded=%lu ok=%lu fail=%lu addr=0x%08lx size=%lu status=%lu tile=%lux%lu count=%lu base_gid=%lu\n", (unsigned long)g_storage_scene_tileset_loaded, (unsigned long)g_storage_scene_tileset_load_ok_count, (unsigned long)g_storage_scene_tileset_load_fail_count, (unsigned long)g_storage_scene_tileset_addr, (unsigned long)g_storage_scene_tileset_size, (unsigned long)g_storage_scene_tileset_last_status, (unsigned long)g_storage_scene_tileset_tile_width, (unsigned long)g_storage_scene_tileset_tile_height, (unsigned long)g_storage_scene_tileset_tile_count, (unsigned long)g_storage_scene_tileset_base_gid
  printf "raw_erase: ok=%lu fail=%lu last_addr=0x%08lx last_size=%lu\n", (unsigned long)g_storage_raw_app_erase_ok_count, (unsigned long)g_storage_raw_app_erase_fail_count, (unsigned long)g_storage_last_erase_addr, (unsigned long)g_storage_last_erase_size
  printf "audio_install: ok=%lu fail=%lu bytes=%lu base=0x%08lx\n", (unsigned long)g_storage_audio_catalog_install_ok_count, (unsigned long)g_storage_audio_catalog_install_fail_count, (unsigned long)g_storage_audio_catalog_install_last_bytes, (unsigned long)g_storage_audio_catalog_addr_dbg
  printf "audio_chunk: ok=%lu fail=%lu addr=0x%08lx len=%lu token=%lu crc=0x%08lx\n", (unsigned long)g_storage_audio_chunk_read_ok_count, (unsigned long)g_storage_audio_chunk_read_fail_count, (unsigned long)g_storage_audio_chunk_last_addr, (unsigned long)g_storage_audio_chunk_last_len, (unsigned long)g_storage_audio_chunk_last_token, (unsigned long)g_storage_audio_chunk_last_crc32
  printf "smoke: pass=%lu fail=%lu addr=0x%08lx len=%lu\n", (unsigned long)g_storage_smoke_pass_count, (unsigned long)g_storage_smoke_fail_count, (unsigned long)g_storage_smoke_addr_dbg, (unsigned long)g_storage_smoke_len_dbg
  printf "filex: mounted=%lu m_ok=%lu m_fail=%lu fmt=%lu um_ok=%lu um_fail=%lu fx_status=%lu\n", (unsigned long)g_storage_filex_mounted, (unsigned long)g_storage_filex_mount_count, (unsigned long)g_storage_filex_mount_fail_count, (unsigned long)g_storage_filex_format_count, (unsigned long)g_storage_filex_unmount_count, (unsigned long)g_storage_filex_unmount_fail_count, (unsigned long)g_storage_filex_last_status
  printf "fat: base=0x%08lx size=%lu cache=%lu spc=%lu dir=%lu\n", (unsigned long)g_storage_fat_base_addr_dbg, (unsigned long)g_storage_fat_size_bytes_dbg, (unsigned long)g_storage_filex_cache_bytes_dbg, (unsigned long)g_storage_filex_spc_dbg, (unsigned long)g_storage_filex_dir_entries_dbg
  printf "at25: op=%u cmd=%u io=%u addr=0x%08lx n=%lu herr=0x%08lx seq=%lu\n", (unsigned int)g_storage_at25_dbg.last_op, (unsigned int)g_storage_at25_dbg.cmd_status, (unsigned int)g_storage_at25_dbg.io_status, (unsigned long)g_storage_at25_dbg.addr, (unsigned long)g_storage_at25_dbg.nbytes, (unsigned long)g_storage_at25_dbg.hal_error, (unsigned long)g_storage_at25_dbg.seq
end

define ps_storage_audio_status
  printf "== Storage audio status ==\n"
  printf "cat: loaded=%lu ok=%lu fail=%lu addr=0x%08lx entries=%lu ver=%lu hdr=%u entry=%u\n", (unsigned long)g_storage_audio_catalog_loaded, (unsigned long)g_storage_audio_catalog_load_ok_count, (unsigned long)g_storage_audio_catalog_load_fail_count, (unsigned long)g_storage_audio_catalog_addr, (unsigned long)g_storage_audio_catalog_entry_count, (unsigned long)g_storage_audio_catalog_version, (unsigned int)g_storage_audio_catalog_header.header_size, (unsigned int)g_storage_audio_catalog_header.entry_size
  printf "cat_crc: hdr=0x%08lx calc=0x%08lx\n", (unsigned long)g_storage_audio_catalog_header.table_crc32, (unsigned long)g_storage_audio_catalog_table_crc32
  printf "install: ok=%lu fail=%lu bytes=%lu base=0x%08lx\n", (unsigned long)g_storage_audio_catalog_install_ok_count, (unsigned long)g_storage_audio_catalog_install_fail_count, (unsigned long)g_storage_audio_catalog_install_last_bytes, (unsigned long)g_storage_audio_catalog_addr_dbg
  printf "chunk: ok=%lu fail=%lu addr=0x%08lx len=%lu token=%lu crc=0x%08lx\n", (unsigned long)g_storage_audio_chunk_read_ok_count, (unsigned long)g_storage_audio_chunk_read_fail_count, (unsigned long)g_storage_audio_chunk_last_addr, (unsigned long)g_storage_audio_chunk_last_len, (unsigned long)g_storage_audio_chunk_last_token, (unsigned long)g_storage_audio_chunk_last_crc32
end

define ps_storage_audio_load_dbg
  set $ps_addr = (unsigned long)g_storage_audio_catalog_addr_dbg
  set $ps_rc = (unsigned int)App_StorageReq_AudioCatalogLoad((unsigned long)$ps_addr)
  printf "queue STORAGE_AUDIO_CATALOG_LOAD(addr=0x%08lx) rc=%u\n", $ps_addr, $ps_rc
end

define ps_storage_audio_load_dbg_wait
  tbreak AppStorageRunAudioCatalogLoad
  ps_storage_audio_load_dbg
  continue
  finish
  ps_storage_audio_status
end

define ps_storage_audio_install_embedded
  set $ps_rc = (unsigned int)App_StorageReq_AudioCatalogInstallEmbedded()
  printf "queue STORAGE_AUDIO_CATALOG_INSTALL_EMBEDDED rc=%u base=0x%08lx\n", $ps_rc, (unsigned long)g_storage_audio_catalog_addr_dbg
end

define ps_storage_audio_install_embedded_wait
  tbreak AppStorageRunAudioCatalogInstallEmbedded
  ps_storage_audio_install_embedded
  continue
  finish
  ps_storage_audio_status
end

define ps_storage_audio_install_manifest_refs
  set $ps_rc = (unsigned int)App_StorageReq_AudioCatalogInstallManifestRefs()
  printf "queue STORAGE_AUDIO_CATALOG_INSTALL_MANIFEST_REFS rc=%u base=0x%08lx\n", $ps_rc, (unsigned long)g_storage_audio_catalog_addr_dbg
end

define ps_storage_audio_install_manifest_refs_wait
  tbreak AppStorageRunAudioCatalogInstallManifestRefs
  ps_storage_audio_install_manifest_refs
  continue
  finish
  ps_storage_audio_status
end

define ps_storage_pkg_manifest_status
  set $ps_magic = *((unsigned int*)g_storage_game_package_manifest_buf)
  set $ps_pkg = (game_package_desc_t*)GamePackage_GetActive()
  printf "== Storage package manifest status ==\n"
  printf "slot: addr=0x%08lx max=%lu\n", (unsigned long)g_storage_game_pkg_manifest_addr_dbg, (unsigned long)g_storage_game_pkg_manifest_max_bytes_dbg
  printf "loaded=%lu ok=%lu fail=%lu addr=0x%08lx size=%lu last_status=%lu source=%lu\n", (unsigned long)g_storage_game_package_manifest_loaded, (unsigned long)g_storage_game_package_manifest_load_ok_count, (unsigned long)g_storage_game_package_manifest_load_fail_count, (unsigned long)g_storage_game_package_manifest_addr, (unsigned long)g_storage_game_package_manifest_size, (unsigned long)g_storage_game_package_manifest_last_status, (unsigned long)g_storage_game_package_source
  printf "import: ok=%lu fail=%lu last_status=%lu bytes=%lu\n", (unsigned long)g_storage_game_package_manifest_import_ok_count, (unsigned long)g_storage_game_package_manifest_import_fail_count, (unsigned long)g_storage_game_package_manifest_import_last_status, (unsigned long)g_storage_game_package_manifest_import_last_bytes
  printf "scene_import: ok=%lu fail=%lu last_status=%lu map_bytes=%lu tileset_bytes=%lu\n", (unsigned long)g_storage_game_package_scene_import_ok_count, (unsigned long)g_storage_game_package_scene_import_fail_count, (unsigned long)g_storage_game_package_scene_import_last_status, (unsigned long)g_storage_game_package_scene_import_map_bytes, (unsigned long)g_storage_game_package_scene_import_tileset_bytes
  printf "buf_magic=0x%08x (expect 0x4b50474d)\n", (unsigned int)$ps_magic
  printf "cache_pkg: id=%lu ver=%lu mode_count=%lu pet_route_count=%lu\n", (unsigned long)g_storage_game_package_id, (unsigned long)g_storage_game_package_version, (unsigned long)g_storage_game_package_mode_count, (unsigned long)g_storage_game_package_pet_route_count
  if $ps_pkg == 0
    printf "active_pkg: none\n"
  else
    printf "active_pkg: ptr=0x%08lx id=%lu ver=%lu mode_count=%lu pet_route_count=%lu\n", (unsigned long)$ps_pkg, (unsigned long)$ps_pkg->package_id, (unsigned long)$ps_pkg->package_version, (unsigned long)$ps_pkg->mode_count, (unsigned long)$ps_pkg->pet_route_count
    if (($ps_pkg->modes != 0) && ($ps_pkg->mode_count > 0))
      set $ps_cfg = &$ps_pkg->modes[0].runtime_config
      printf "mode0_blob: map=0x%08lx/%lu tileset=0x%08lx/%lu\n", (unsigned long)$ps_cfg->scene_map_addr, (unsigned long)$ps_cfg->scene_map_size_bytes, (unsigned long)$ps_cfg->scene_tileset_addr, (unsigned long)$ps_cfg->scene_tileset_size_bytes
      printf "mode0_refs: map_id=%lu tileset_id=%lu music=%lu sfx_interact=%lu sfx_confirm=%lu sfx_error=%lu\n", (unsigned long)$ps_cfg->scene_map_id, (unsigned long)$ps_cfg->scene_tileset_id, (unsigned long)$ps_cfg->music_asset_id, (unsigned long)$ps_cfg->sfx_interact_asset_id, (unsigned long)$ps_cfg->sfx_confirm_asset_id, (unsigned long)$ps_cfg->sfx_error_asset_id
    end
  end
  printf "last_op=%lu last_err=%ld\n", (unsigned long)g_storage_last_op, (long)g_storage_last_error
end

define ps_storage_install_index_status
  printf "== Storage install-index status ==\n"
  printf "layout: data_end=0x%08lx slot0=0x%08lx slot1=0x%08lx slot_size=%lu rec_size=%lu crc_off=%lu\n", (unsigned long)g_storage_installed_data_end_addr_dbg, (unsigned long)g_storage_install_index_slot0_addr_dbg, (unsigned long)g_storage_install_index_slot1_addr_dbg, (unsigned long)g_storage_install_index_slot_size_bytes_dbg, (unsigned long)g_storage_install_index_record_size_dbg, (unsigned long)g_storage_install_index_record_crc_offset_dbg
  printf "state: valid=%lu l_ok=%lu l_fail=%lu w_ok=%lu w_fail=%lu slot=%lu seq=%lu lstat=%lu wstat=%lu scan=%lu\n", (unsigned long)g_storage_install_index_valid, (unsigned long)g_storage_install_index_load_ok_count, (unsigned long)g_storage_install_index_load_fail_count, (unsigned long)g_storage_install_index_write_ok_count, (unsigned long)g_storage_install_index_write_fail_count, (unsigned long)g_storage_install_index_active_slot, (unsigned long)g_storage_install_index_sequence, (unsigned long)g_storage_install_index_load_last_status, (unsigned long)g_storage_install_index_write_last_status, (unsigned long)g_storage_install_index_scan_done
  printf "entry: pkg=%lu/%lu blob_off=0x%08lx blob_size=%lu blob_crc=0x%08lx manifest=0x%08lx/%lu m_crc=0x%08lx rec_crc=0x%08lx\n", (unsigned long)g_storage_install_index_package_id, (unsigned long)g_storage_install_index_package_version, (unsigned long)g_storage_install_index_blob_offset, (unsigned long)g_storage_install_index_blob_size, (unsigned long)g_storage_install_index_blob_crc32, (unsigned long)g_storage_install_index_manifest_addr, (unsigned long)g_storage_install_index_manifest_size, (unsigned long)g_storage_install_index_manifest_crc32, (unsigned long)g_storage_install_index_record_crc32
end

define ps_storage_install_index_smoke
  set $ps_slot_addr = (unsigned long)g_storage_game_pkg_manifest_addr_dbg
  printf "== Install-index smoke ==\n"
  printf "step1: establish baseline install-index entry\n"
  ps_storage_pkg_manifest_write_test_wait
  ps_storage_pkg_manifest_load_default_wait
  set $ps_seq0 = (unsigned long)g_storage_install_index_sequence
  set $ps_slot0 = (unsigned long)g_storage_install_index_active_slot
  set $ps_wok0 = (unsigned long)g_storage_install_index_write_ok_count
  if g_storage_install_index_valid == 0
    printf "FAIL: baseline install-index not valid\n"
  else
    printf "baseline: slot=%lu seq=%lu write_ok=%lu\n", $ps_slot0, $ps_seq0, $ps_wok0
    printf "step2: force manifest-load failure (size=64)\n"
    ps_storage_pkg_manifest_load_wait $ps_slot_addr 64
    set $ps_seq1 = (unsigned long)g_storage_install_index_sequence
    set $ps_slot1 = (unsigned long)g_storage_install_index_active_slot
    set $ps_wok1 = (unsigned long)g_storage_install_index_write_ok_count
    if (($ps_seq1 == $ps_seq0) && ($ps_slot1 == $ps_slot0) && ($ps_wok1 == $ps_wok0))
      printf "PASS: failed manifest load left install-index unchanged\n"
    else
      printf "FAIL: install-index changed (slot %lu->%lu seq %lu->%lu w_ok %lu->%lu)\n", $ps_slot0, $ps_slot1, $ps_seq0, $ps_seq1, $ps_wok0, $ps_wok1
    end
  end
  ps_storage_install_index_status
end

define ps_storage_install_index_atomic_smoke
  printf "== Install-index atomic fallback smoke ==\n"
  printf "step1: create baseline valid entry\n"
  ps_storage_pkg_manifest_write_test_wait
  ps_storage_pkg_manifest_load_default_wait
  if g_storage_install_index_valid == 0
    printf "FAIL: baseline install-index invalid\n"
  else
    set $ps_base_slot = (unsigned long)g_storage_install_index_active_slot
    set $ps_base_seq = (unsigned long)g_storage_install_index_sequence
    printf "baseline: slot=%lu seq=%lu\n", $ps_base_slot, $ps_base_seq
    printf "step2: create predecessor + newest entries\n"
    ps_storage_pkg_manifest_write_test_wait
    set $ps_prev_slot = (unsigned long)g_storage_install_index_active_slot
    set $ps_prev_seq = (unsigned long)g_storage_install_index_sequence
    ps_storage_pkg_manifest_load_default_wait
    set $ps_new_slot = (unsigned long)g_storage_install_index_active_slot
    set $ps_new_seq = (unsigned long)g_storage_install_index_sequence
    printf "predecessor: slot=%lu seq=%lu\n", $ps_prev_slot, $ps_prev_seq
    printf "newest: slot=%lu seq=%lu\n", $ps_new_slot, $ps_new_seq
    if ($ps_new_seq <= $ps_prev_seq)
      printf "WARN: sequence did not advance (%lu -> %lu)\n", $ps_prev_seq, $ps_new_seq
    end
    if ($ps_prev_seq <= $ps_base_seq)
      printf "WARN: predecessor sequence did not advance from baseline (%lu -> %lu)\n", $ps_base_seq, $ps_prev_seq
    end
    printf "step3: corrupt active slot CRC to simulate torn commit\n"
    if $ps_new_slot == 0
      set $ps_corrupt_addr = (unsigned long)g_storage_install_index_slot0_addr_dbg + (unsigned long)g_storage_install_index_record_crc_offset_dbg
    else
      if $ps_new_slot == 1
        set $ps_corrupt_addr = (unsigned long)g_storage_install_index_slot1_addr_dbg + (unsigned long)g_storage_install_index_record_crc_offset_dbg
      else
        set $ps_corrupt_addr = 0
      end
    end
    if $ps_corrupt_addr == 0
      printf "FAIL: invalid active slot (%lu)\n", $ps_new_slot
    else
      set {unsigned int}g_storage_game_package_manifest_buf = 0
      set $ps_hal = (int)AT25_PageProgram(&hospi1, (unsigned int)$ps_corrupt_addr, (const unsigned char*)g_storage_game_package_manifest_buf, 4)
      printf "inject: crc_zero_addr=0x%08lx hal=%d\n", $ps_corrupt_addr, $ps_hal
      printf "step4: reload journal from flash\n"
      set g_storage_install_index_scan_done = 0
      ps_storage_probe_wait
      set $ps_after_slot = (unsigned long)g_storage_install_index_active_slot
      set $ps_after_seq = (unsigned long)g_storage_install_index_sequence
      if (($ps_after_slot == $ps_prev_slot) && ($ps_after_seq == $ps_prev_seq))
        printf "PASS: loader ignored corrupt newest slot and recovered immediate predecessor\n"
      else
        printf "FAIL: fallback mismatch predecessor(slot=%lu seq=%lu) now(slot=%lu seq=%lu)\n", $ps_prev_slot, $ps_prev_seq, $ps_after_slot, $ps_after_seq
      end
    end
  end
  ps_storage_install_index_status
end

define ps_storage_pkg_manifest_load
  if $argc < 2
    printf "usage: ps_storage_pkg_manifest_load <addr> <size>\n"
    printf "example: ps_storage_pkg_manifest_load 0x00180000 0x00000120\n"
  else
    set $ps_addr = (unsigned long)$arg0
    set $ps_size = (unsigned long)$arg1
    set $ps_rc = (unsigned int)App_StorageReq_GamePackageManifestLoad($ps_addr, $ps_size)
    printf "queue STORAGE_GAME_PACKAGE_MANIFEST_LOAD(addr=0x%08lx size=%lu) rc=%u\n", $ps_addr, $ps_size, $ps_rc
  end
end

define ps_storage_pkg_manifest_load_wait
  if $argc < 2
    printf "usage: ps_storage_pkg_manifest_load_wait <addr> <size>\n"
    printf "example: ps_storage_pkg_manifest_load_wait 0x00180000 0x00000120\n"
  else
    tbreak AppStorageRunGamePackageManifestLoad
    ps_storage_pkg_manifest_load $arg0 $arg1
    continue
    finish
    ps_storage_pkg_manifest_status
  end
end

define ps_storage_pkg_manifest_load_default
  set $ps_rc = (unsigned int)App_StorageReq_GamePackageManifestLoadDefault()
  printf "queue STORAGE_GAME_PACKAGE_MANIFEST_LOAD_DEFAULT rc=%u slot=0x%08lx max=%lu\n", $ps_rc, (unsigned long)g_storage_game_pkg_manifest_addr_dbg, (unsigned long)g_storage_game_pkg_manifest_max_bytes_dbg
end

define ps_storage_pkg_manifest_load_default_wait
  tbreak AppStorageRunGamePackageManifestLoadDefault
  set $ps_bp = $bpnum
  set $ps_pm = $primask
  set $primask = 1
  set $ps_rc = (unsigned int)App_StorageReq_GamePackageManifestLoadDefault()
  set $primask = $ps_pm
  printf "queue STORAGE_GAME_PACKAGE_MANIFEST_LOAD_DEFAULT rc=%u slot=0x%08lx max=%lu\n", $ps_rc, (unsigned long)g_storage_game_pkg_manifest_addr_dbg, (unsigned long)g_storage_game_pkg_manifest_max_bytes_dbg
  if $ps_rc != 0
    delete $ps_bp
    printf "wait aborted: queue post failed (rc=%u)\n", $ps_rc
  else
    continue
    finish
    ps_storage_pkg_manifest_status
  end
end

define ps_storage_pkg_manifest_import_fat
  set $ps_rc = (unsigned int)App_StorageReq_GamePackageManifestImportFat()
  printf "queue STORAGE_GAME_PACKAGE_MANIFEST_IMPORT_FAT rc=%u path=\"%s\" fallback=\"%s\"\n", $ps_rc, "INBOX/MANIFEST.BIN", "MANIFEST.BIN"
end

define ps_storage_pkg_manifest_import_fat_wait
  tbreak AppStorageRunGamePackageManifestImportFat
  set $ps_bp = $bpnum
  set $ps_pm = $primask
  set $primask = 1
  set $ps_rc = (unsigned int)App_StorageReq_GamePackageManifestImportFat()
  set $primask = $ps_pm
  printf "queue STORAGE_GAME_PACKAGE_MANIFEST_IMPORT_FAT rc=%u path=\"%s\" fallback=\"%s\"\n", $ps_rc, "INBOX/MANIFEST.BIN", "MANIFEST.BIN"
  if $ps_rc != 0
    delete $ps_bp
    printf "wait aborted: queue post failed (rc=%u)\n", $ps_rc
  else
    continue
    finish
    ps_storage_pkg_manifest_status
  end
end

define ps_storage_scene_import_fat
  set $ps_rc = (unsigned int)App_StorageReq_GamePackageSceneImportFat()
  printf "queue STORAGE_GAME_PACKAGE_SCENE_IMPORT_FAT rc=%u map=\"%s\" fallback=\"%s\" tileset=\"%s\" fallback=\"%s\"\n", $ps_rc, "INBOX/SCENE_MAP.BIN", "SCENE_MAP.BIN", "INBOX/SCENE_TILESET.BIN", "SCENE_TILESET.BIN"
end

define ps_storage_scene_import_fat_wait
  tbreak AppStorageRunGamePackageSceneImportFat
  set $ps_bp = $bpnum
  set $ps_pm = $primask
  set $primask = 1
  set $ps_rc = (unsigned int)App_StorageReq_GamePackageSceneImportFat()
  set $primask = $ps_pm
  printf "queue STORAGE_GAME_PACKAGE_SCENE_IMPORT_FAT rc=%u map=\"%s\" fallback=\"%s\" tileset=\"%s\" fallback=\"%s\"\n", $ps_rc, "INBOX/SCENE_MAP.BIN", "SCENE_MAP.BIN", "INBOX/SCENE_TILESET.BIN", "SCENE_TILESET.BIN"
  if $ps_rc != 0
    delete $ps_bp
    printf "wait aborted: queue post failed (rc=%u)\n", $ps_rc
  else
    continue
    finish
    ps_storage_status
  end
end

define ps_storage_pkg_manifest_slot_info
  printf "== Manifest slot ==\n"
  printf "addr=0x%08lx max_bytes=%lu\n", (unsigned long)g_storage_game_pkg_manifest_addr_dbg, (unsigned long)g_storage_game_pkg_manifest_max_bytes_dbg
end

define ps_storage_pkg_manifest_slot_prepare_wait
  ps_storage_pkg_manifest_slot_info
  ps_storage_pkg_manifest_erase_wait
  ps_storage_pkg_manifest_status
end

define ps_storage_pkg_manifest_slot_verify_wait
  ps_storage_pkg_manifest_slot_info
  ps_storage_pkg_manifest_load_default_wait
end

define ps_storage_pkg_manifest_slot_workflow_help
  printf "Pre-USBX direct-slot workflow:\n"
  printf "  1) ps_storage_pkg_manifest_slot_prepare_wait\n"
  printf "  2) program manifest bytes into slot with external tool (ST-LINK/CubeProgrammer)\n"
  printf "  3) ps_storage_pkg_manifest_slot_verify_wait\n"
  printf "  4) ps_storage_pkg_manifest_status\n"
end

define ps_storage_pkg_manifest_txn_smoke
  set $ps_bad_size = 64
  if $argc >= 1
    set $ps_bad_size = (unsigned long)$arg0
  end
  if $ps_bad_size == 0
    set $ps_bad_size = 64
  end

  set $ps_slot_addr = (unsigned long)g_storage_game_pkg_manifest_addr_dbg
  printf "== Manifest transactional smoke ==\n"
  printf "slot=0x%08lx bad_size=%lu\n", $ps_slot_addr, $ps_bad_size
  printf "step1: erase + write_test + load_default\n"
  ps_storage_pkg_manifest_erase_wait
  ps_storage_pkg_manifest_write_test_wait
  ps_storage_pkg_manifest_load_default_wait

  set $ps_pkg_before = (game_package_desc_t*)GamePackage_GetActive()
  if $ps_pkg_before == 0
    printf "FAIL: no active package after known-good load\n"
  else
    set $ps_id_before = (unsigned long)$ps_pkg_before->package_id
    set $ps_ver_before = (unsigned long)$ps_pkg_before->package_version
    set $ps_modes_before = (unsigned long)$ps_pkg_before->mode_count
    printf "baseline: ptr=0x%08lx id=%lu ver=%lu modes=%lu\n", (unsigned long)$ps_pkg_before, $ps_id_before, $ps_ver_before, $ps_modes_before

    printf "step2: intentionally failing load (size=%lu)\n", $ps_bad_size
    ps_storage_pkg_manifest_load_wait $ps_slot_addr $ps_bad_size

    set $ps_pkg_after = (game_package_desc_t*)GamePackage_GetActive()
    if $ps_pkg_after == 0
      printf "FAIL: active package cleared after failed load\n"
    else
      set $ps_id_ok = (unsigned long)($ps_pkg_after->package_id == $ps_id_before)
      set $ps_ver_ok = (unsigned long)($ps_pkg_after->package_version == $ps_ver_before)
      set $ps_modes_ok = (unsigned long)($ps_pkg_after->mode_count == $ps_modes_before)
      printf "after_fail: ptr=0x%08lx id=%lu ver=%lu modes=%lu\n", (unsigned long)$ps_pkg_after, (unsigned long)$ps_pkg_after->package_id, (unsigned long)$ps_pkg_after->package_version, (unsigned long)$ps_pkg_after->mode_count
      if (($ps_id_ok != 0) && ($ps_ver_ok != 0) && ($ps_modes_ok != 0))
        printf "PASS: active package preserved across failed load\n"
      else
        printf "FAIL: active package changed across failed load (id_ok=%lu ver_ok=%lu modes_ok=%lu)\n", $ps_id_ok, $ps_ver_ok, $ps_modes_ok
      end
    end
  end

  ps_storage_pkg_manifest_status
end

define ps_storage_pkg_manifest_erase
  set $ps_rc = (unsigned int)App_StorageReq_GamePackageManifestErase()
  printf "queue STORAGE_GAME_PACKAGE_MANIFEST_ERASE rc=%u slot=0x%08lx size=%lu\n", $ps_rc, (unsigned long)g_storage_game_pkg_manifest_addr_dbg, (unsigned long)g_storage_game_pkg_manifest_max_bytes_dbg
end

define ps_storage_pkg_manifest_erase_wait
  tbreak AppStorageRunGamePackageManifestErase
  set $ps_bp = $bpnum
  set $ps_pm = $primask
  set $primask = 1
  set $ps_rc = (unsigned int)App_StorageReq_GamePackageManifestErase()
  set $primask = $ps_pm
  printf "queue STORAGE_GAME_PACKAGE_MANIFEST_ERASE rc=%u slot=0x%08lx size=%lu\n", $ps_rc, (unsigned long)g_storage_game_pkg_manifest_addr_dbg, (unsigned long)g_storage_game_pkg_manifest_max_bytes_dbg
  if $ps_rc != 0
    delete $ps_bp
    printf "wait aborted: queue post failed (rc=%u)\n", $ps_rc
  else
    continue
    finish
    ps_storage_status
  end
end

define ps_storage_pkg_manifest_write_test
  set $ps_rc = (unsigned int)App_StorageReq_GamePackageManifestWriteTest()
  printf "queue STORAGE_GAME_PACKAGE_MANIFEST_WRITE_TEST rc=%u slot=0x%08lx size=%lu\n", $ps_rc, (unsigned long)g_storage_game_pkg_manifest_addr_dbg, (unsigned long)g_storage_game_pkg_manifest_max_bytes_dbg
end

define ps_storage_pkg_manifest_write_test_wait
  tbreak AppStorageRunGamePackageManifestWriteTest
  set $ps_bp = $bpnum
  set $ps_pm = $primask
  set $primask = 1
  set $ps_rc = (unsigned int)App_StorageReq_GamePackageManifestWriteTest()
  set $primask = $ps_pm
  printf "queue STORAGE_GAME_PACKAGE_MANIFEST_WRITE_TEST rc=%u slot=0x%08lx size=%lu\n", $ps_rc, (unsigned long)g_storage_game_pkg_manifest_addr_dbg, (unsigned long)g_storage_game_pkg_manifest_max_bytes_dbg
  if $ps_rc != 0
    delete $ps_bp
    printf "wait aborted: queue post failed (rc=%u)\n", $ps_rc
  else
    continue
    finish
    ps_storage_pkg_manifest_status
  end
end

define __ps_storage_pkg_manifest_program_staged
  if $argc < 1
    printf "usage: __ps_storage_pkg_manifest_program_staged <addr>\n"
    printf "example: __ps_storage_pkg_manifest_program_staged 0x00181000\n"
  else
    set $__ps_addr = (unsigned long)$arg0
    set $__ps_buf = (unsigned char*)g_storage_game_package_manifest_buf
    set $__ps_magic = (unsigned long)(*(unsigned int*)$__ps_buf)
    set $__ps_size = (unsigned long)(*(unsigned int*)($__ps_buf + 8))
    set $__ps_max_buf = (unsigned long)sizeof(g_storage_game_package_manifest_buf)
    set $__ps_max_slot = (unsigned long)g_storage_game_pkg_manifest_max_bytes_dbg
    if $__ps_magic != 0x4b50474d
      printf "abort: staged manifest magic=0x%08lx expect=0x4b50474d\n", $__ps_magic
    else
      if ($__ps_size == 0) || ($__ps_size > $__ps_max_buf) || ($__ps_size > $__ps_max_slot)
        printf "abort: staged manifest size=%lu invalid (buf_max=%lu slot_max=%lu)\n", $__ps_size, $__ps_max_buf, $__ps_max_slot
      else
        set $__ps_off = 0
        set $__ps_src = (const unsigned char*)g_storage_game_package_manifest_buf
        set $__ps_ok = 1
        printf "program manifest: addr=0x%08lx size=%lu\n", $__ps_addr, $__ps_size
        while $__ps_off < $__ps_size
          if $__ps_ok == 0
            set $__ps_off = $__ps_size
          else
            set $__ps_n = $__ps_size - $__ps_off
            if $__ps_n > 256
              set $__ps_n = 256
            end
            set $__ps_hal = (int)AT25_PageProgram(&hospi1, (unsigned int)($__ps_addr + $__ps_off), $__ps_src, (unsigned int)$__ps_n)
            if $__ps_hal != 0
              set $__ps_ok = 0
              printf "program fail: addr=0x%08lx n=%lu hal=%d\n", (unsigned long)($__ps_addr + $__ps_off), $__ps_n, $__ps_hal
            else
              set $__ps_off = $__ps_off + $__ps_n
              set $__ps_src = $__ps_src + $__ps_n
            end
          end
        end
        if $__ps_ok != 0
          ps_storage_pkg_manifest_load_wait $__ps_addr $__ps_size
        else
          printf "manifest install aborted due to HAL error\n"
        end
      end
    end
  end
end

define ps_storage_pkg_manifest_install_bin_wait
  printf "== Install package manifest (manifest.bin) ==\n"
  ps_mode_static
  ps_storage_probe_wait
  ps_storage_pkg_manifest_slot_prepare_wait
  restore Assets/game_package/manifest.bin binary g_storage_game_package_manifest_buf
  set $ps_manifest_addr = (unsigned long)g_storage_game_pkg_manifest_addr_dbg
  __ps_storage_pkg_manifest_program_staged $ps_manifest_addr
end

define ps_storage_scene_map_status
  printf "== Storage scene-map status ==\n"
  printf "loaded=%lu ok=%lu fail=%lu addr=0x%08lx size=%lu status=%lu\n", (unsigned long)g_storage_scene_map_loaded, (unsigned long)g_storage_scene_map_load_ok_count, (unsigned long)g_storage_scene_map_load_fail_count, (unsigned long)g_storage_scene_map_addr, (unsigned long)g_storage_scene_map_size, (unsigned long)g_storage_scene_map_last_status
  printf "map: width=%lu height=%lu tiles=%lu objects=%lu\n", (unsigned long)g_storage_scene_map_width, (unsigned long)g_storage_scene_map_height, (unsigned long)g_storage_scene_map_tile_count, (unsigned long)g_storage_scene_map_object_count
  printf "last_op=%lu last_err=%ld\n", (unsigned long)g_storage_last_op, (long)g_storage_last_error
end

define ps_storage_scene_map_load
  if $argc < 2
    printf "usage: ps_storage_scene_map_load <addr> <size>\n"
    printf "example: ps_storage_scene_map_load 0x00300000 0x00000198\n"
  else
    set $ps_addr = (unsigned long)$arg0
    set $ps_size = (unsigned long)$arg1
    set $ps_rc = (unsigned int)App_StorageReq_SceneMapLoad($ps_addr, $ps_size)
    printf "queue STORAGE_SCENE_MAP_LOAD(addr=0x%08lx size=%lu) rc=%u\n", $ps_addr, $ps_size, $ps_rc
  end
end

define ps_storage_scene_map_load_wait
  if $argc < 2
    printf "usage: ps_storage_scene_map_load_wait <addr> <size>\n"
    printf "example: ps_storage_scene_map_load_wait 0x00300000 0x00000198\n"
  else
    tbreak AppStorageRunSceneMapLoad
    set $ps_bp = $bpnum
    set $ps_addr = (unsigned long)$arg0
    set $ps_size = (unsigned long)$arg1
    set $ps_pm = $primask
    set $primask = 1
    set $ps_rc = (unsigned int)App_StorageReq_SceneMapLoad($ps_addr, $ps_size)
    set $primask = $ps_pm
    printf "queue STORAGE_SCENE_MAP_LOAD(addr=0x%08lx size=%lu) rc=%u\n", $ps_addr, $ps_size, $ps_rc
    if $ps_rc != 0
      delete $ps_bp
      printf "wait aborted: queue post failed (rc=%u)\n", $ps_rc
    else
      continue
      finish
      ps_storage_scene_map_status
    end
  end
end

define __ps_storage_scene_map_program_staged
  if $argc < 1
    printf "usage: __ps_storage_scene_map_program_staged <addr>\n"
    printf "example: __ps_storage_scene_map_program_staged 0x00300000\n"
  else
    set $__ps_addr = (unsigned long)$arg0
    set $__ps_hdr = (game_map_blob_header_t*)g_storage_scene_map_blob_buf
    set $__ps_magic = (unsigned long)$__ps_hdr->magic
    set $__ps_size = (unsigned long)$__ps_hdr->total_size
    if $__ps_magic != 0x50414d54
      printf "abort: staged map magic=0x%08lx expect=0x50414d54\n", $__ps_magic
    else
      if ($__ps_size == 0) || ($__ps_size > (unsigned long)sizeof(g_storage_scene_map_blob_buf))
        printf "abort: staged map size=%lu invalid (max=%lu)\n", $__ps_size, (unsigned long)sizeof(g_storage_scene_map_blob_buf)
      else
        set $__ps_end = $__ps_addr + $__ps_size
        set $__ps_erase = $__ps_addr & 0xfffff000
        set $__ps_ok = 1
        printf "program map: addr=0x%08lx size=%lu\n", $__ps_addr, $__ps_size
        while $__ps_erase < $__ps_end
          if $__ps_ok == 0
            set $__ps_erase = $__ps_end
          else
            set $__ps_hal = (int)AT25_Erase4K(&hospi1, (unsigned int)$__ps_erase)
            if $__ps_hal != 0
              set $__ps_ok = 0
              printf "erase fail: addr=0x%08lx hal=%d\n", $__ps_erase, $__ps_hal
            else
              set $__ps_erase = $__ps_erase + 0x1000
            end
          end
        end
        set $__ps_off = 0
        set $__ps_src = (const unsigned char*)g_storage_scene_map_blob_buf
        while $__ps_off < $__ps_size
          if $__ps_ok == 0
            set $__ps_off = $__ps_size
          else
            set $__ps_n = $__ps_size - $__ps_off
            if $__ps_n > 256
              set $__ps_n = 256
            end
            set $__ps_hal = (int)AT25_PageProgram(&hospi1, (unsigned int)($__ps_addr + $__ps_off), $__ps_src, (unsigned int)$__ps_n)
            if $__ps_hal != 0
              set $__ps_ok = 0
              printf "program fail: addr=0x%08lx n=%lu hal=%d\n", (unsigned long)($__ps_addr + $__ps_off), $__ps_n, $__ps_hal
            else
              set $__ps_off = $__ps_off + $__ps_n
              set $__ps_src = $__ps_src + $__ps_n
            end
          end
        end
        if $__ps_ok != 0
          ps_storage_scene_map_load_wait $__ps_addr 0
        else
          printf "map install aborted due to HAL error\n"
        end
      end
    end
  end
end

define ps_storage_scene_map_install_pet_house
  printf "== Install scene map (pet_house.tmap.bin) ==\n"
  ps_mode_verify_static
  ps_storage_probe_wait
  restore Assets/maps/build/pet_house.tmap.bin binary g_storage_scene_map_blob_buf
  __ps_storage_scene_map_program_staged (unsigned long)g_storage_installed_base_addr_dbg
end

define ps_storage_scene_tileset_status
  printf "== Storage scene-tileset status ==\n"
  printf "slot: addr=0x%08lx size=%lu\n", (unsigned long)g_storage_scene_tileset_addr_dbg, (unsigned long)g_storage_scene_tileset_size_bytes_dbg
  printf "loaded=%lu ok=%lu fail=%lu addr=0x%08lx size=%lu status=%lu\n", (unsigned long)g_storage_scene_tileset_loaded, (unsigned long)g_storage_scene_tileset_load_ok_count, (unsigned long)g_storage_scene_tileset_load_fail_count, (unsigned long)g_storage_scene_tileset_addr, (unsigned long)g_storage_scene_tileset_size, (unsigned long)g_storage_scene_tileset_last_status
  printf "tileset: tile=%lux%lu count=%lu base_gid=%lu\n", (unsigned long)g_storage_scene_tileset_tile_width, (unsigned long)g_storage_scene_tileset_tile_height, (unsigned long)g_storage_scene_tileset_tile_count, (unsigned long)g_storage_scene_tileset_base_gid
  printf "last_op=%lu last_err=%ld\n", (unsigned long)g_storage_last_op, (long)g_storage_last_error
end

define ps_storage_scene_tileset_load
  if $argc < 2
    printf "usage: ps_storage_scene_tileset_load <addr> <size>\n"
    printf "example: ps_storage_scene_tileset_load 0x00301000 0x00001800\n"
  else
    set $ps_addr = (unsigned long)$arg0
    set $ps_size = (unsigned long)$arg1
    set $ps_rc = (unsigned int)App_StorageReq_SceneTilesetLoad($ps_addr, $ps_size)
    printf "queue STORAGE_SCENE_TILESET_LOAD(addr=0x%08lx size=%lu) rc=%u\n", $ps_addr, $ps_size, $ps_rc
  end
end

define ps_storage_scene_tileset_load_wait
  if $argc < 2
    printf "usage: ps_storage_scene_tileset_load_wait <addr> <size>\n"
    printf "example: ps_storage_scene_tileset_load_wait 0x00301000 0x00001800\n"
  else
    tbreak AppStorageRunSceneTilesetLoad
    set $ps_bp = $bpnum
    set $ps_addr = (unsigned long)$arg0
    set $ps_size = (unsigned long)$arg1
    set $ps_pm = $primask
    set $primask = 1
    set $ps_rc = (unsigned int)App_StorageReq_SceneTilesetLoad($ps_addr, $ps_size)
    set $primask = $ps_pm
    printf "queue STORAGE_SCENE_TILESET_LOAD(addr=0x%08lx size=%lu) rc=%u\n", $ps_addr, $ps_size, $ps_rc
    if $ps_rc != 0
      delete $ps_bp
      printf "wait aborted: queue post failed (rc=%u)\n", $ps_rc
    else
      continue
      finish
      ps_storage_scene_tileset_status
    end
  end
end

define __ps_storage_scene_tileset_program_staged
  if $argc < 1
    printf "usage: __ps_storage_scene_tileset_program_staged <addr>\n"
    printf "example: __ps_storage_scene_tileset_program_staged 0x00301000\n"
  else
    set $__ps_addr = (unsigned long)$arg0
    set $__ps_hdr = (game_tileset_blob_header_t*)g_storage_scene_tileset_blob_buf
    set $__ps_magic = (unsigned long)$__ps_hdr->magic
    set $__ps_size = (unsigned long)$__ps_hdr->total_size
    if $__ps_magic != 0x54455354
      printf "abort: staged tileset magic=0x%08lx expect=0x54455354\n", $__ps_magic
    else
      if ($__ps_size == 0) || ($__ps_size > (unsigned long)sizeof(g_storage_scene_tileset_blob_buf))
        printf "abort: staged tileset size=%lu invalid (max=%lu)\n", $__ps_size, (unsigned long)sizeof(g_storage_scene_tileset_blob_buf)
      else
        set $__ps_end = $__ps_addr + $__ps_size
        set $__ps_erase = $__ps_addr & 0xfffff000
        set $__ps_ok = 1
        printf "program tileset: addr=0x%08lx size=%lu\n", $__ps_addr, $__ps_size
        while $__ps_erase < $__ps_end
          if $__ps_ok == 0
            set $__ps_erase = $__ps_end
          else
            set $__ps_hal = (int)AT25_Erase4K(&hospi1, (unsigned int)$__ps_erase)
            if $__ps_hal != 0
              set $__ps_ok = 0
              printf "erase fail: addr=0x%08lx hal=%d\n", $__ps_erase, $__ps_hal
            else
              set $__ps_erase = $__ps_erase + 0x1000
            end
          end
        end
        set $__ps_off = 0
        set $__ps_src = (const unsigned char*)g_storage_scene_tileset_blob_buf
        while $__ps_off < $__ps_size
          if $__ps_ok == 0
            set $__ps_off = $__ps_size
          else
            set $__ps_n = $__ps_size - $__ps_off
            if $__ps_n > 256
              set $__ps_n = 256
            end
            set $__ps_hal = (int)AT25_PageProgram(&hospi1, (unsigned int)($__ps_addr + $__ps_off), $__ps_src, (unsigned int)$__ps_n)
            if $__ps_hal != 0
              set $__ps_ok = 0
              printf "program fail: addr=0x%08lx n=%lu hal=%d\n", (unsigned long)($__ps_addr + $__ps_off), $__ps_n, $__ps_hal
            else
              set $__ps_off = $__ps_off + $__ps_n
              set $__ps_src = $__ps_src + $__ps_n
            end
          end
        end
        if $__ps_ok != 0
          ps_storage_scene_tileset_load_wait $__ps_addr 0
        else
          printf "tileset install aborted due to HAL error\n"
        end
      end
    end
  end
end

define ps_storage_scene_tileset_install_pet_house
  printf "== Install scene tileset (pet_house.tset.bin) ==\n"
  ps_mode_verify_static
  ps_storage_probe_wait
  restore Assets/maps/build/pet_house.tset.bin binary g_storage_scene_tileset_blob_buf
  __ps_storage_scene_tileset_program_staged (unsigned long)g_storage_scene_tileset_addr_dbg
end

define ps_storage_scene_assets_install_pet_house
  printf "== Install scene assets (pet_house) ==\n"
  ps_storage_scene_map_install_pet_house
  ps_storage_scene_tileset_install_pet_house
end

define ps_storage_scene_map_install_town_map
  printf "== Install scene map (town_map.tmap.bin) ==\n"
  ps_mode_verify_static
  ps_storage_probe_wait
  restore Assets/maps/build/town_map.tmap.bin binary g_storage_scene_map_blob_buf
  __ps_storage_scene_map_program_staged 0x00310000
end

define ps_storage_scene_tileset_install_town_map
  printf "== Install scene tileset (town_map.tset.bin) ==\n"
  ps_mode_verify_static
  ps_storage_probe_wait
  restore Assets/maps/build/town_map.tset.bin binary g_storage_scene_tileset_blob_buf
  __ps_storage_scene_tileset_program_staged 0x00311000
end

define ps_storage_scene_assets_install_town_map
  printf "== Install scene assets (town_map) ==\n"
  ps_storage_scene_map_install_town_map
  ps_storage_scene_tileset_install_town_map
end

define ps_storage_scene_reload_map_helpers
  source Assets/game_project/build/map_debug_helpers_autogen.gdb
end

define ps_storage_raw_app_erase
  set $ps_rc = (unsigned int)App_StorageReq_RawAppErase()
  printf "queue STORAGE_RAW_APP_ERASE rc=%u\n", $ps_rc
end

define ps_storage_raw_app_erase_wait
  tbreak AppStorageRunRawAppErase
  set $ps_bp = $bpnum
  set $ps_pm = $primask
  set $primask = 1
  set $ps_rc = (unsigned int)App_StorageReq_RawAppErase()
  set $primask = $ps_pm
  printf "queue STORAGE_RAW_APP_ERASE rc=%u\n", $ps_rc
  if $ps_rc != 0
    delete $ps_bp
    printf "wait aborted: queue post failed (rc=%u)\n", $ps_rc
  else
    continue
    finish
    ps_storage_status
  end
end

define ps_storage_probe
  set $ps_rc = (unsigned int)App_StorageReq_FlashProbe()
  printf "queue STORAGE_FLASH_PROBE rc=%u\n", $ps_rc
end

define ps_storage_probe_wait
  tbreak AppStorageRunFlashProbe
  set $ps_bp = $bpnum
  set $ps_pm = $primask
  set $primask = 1
  set $ps_rc = (unsigned int)App_StorageReq_FlashProbe()
  set $primask = $ps_pm
  printf "queue STORAGE_FLASH_PROBE rc=%u\n", $ps_rc
  if $ps_rc != 0
    delete $ps_bp
    printf "wait aborted: queue post failed (rc=%u)\n", $ps_rc
  else
    continue
    finish
    ps_storage_status
  end
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

define ps_storage_filex_fallback_smoke
  set $ps_mode = (unsigned long)g_eg_mode.tx_event_flags_group_current
  printf "== FileX fallback smoke ==\n"
  if (($ps_mode & 0x00000008) != 0)
    printf "abort: currently in FLASHING mode (mode=0x%08lx). switch to STATIC first.\n", $ps_mode
  else
    printf "step1: establish active package baseline (raw manifest path)\n"
    ps_storage_pkg_manifest_write_test_wait
    ps_storage_pkg_manifest_load_default_wait

    set $ps_pkg_before = (game_package_desc_t*)GamePackage_GetActive()
    if $ps_pkg_before == 0
      printf "FAIL: baseline active package missing\n"
    else
      set $ps_id_before = (unsigned long)$ps_pkg_before->package_id
      set $ps_ver_before = (unsigned long)$ps_pkg_before->package_version
      set $ps_modes_before = (unsigned long)$ps_pkg_before->mode_count
      set $ps_m_ok0 = (unsigned long)g_storage_filex_mount_count
      set $ps_m_fail0 = (unsigned long)g_storage_filex_mount_fail_count

      printf "baseline: ptr=0x%08lx id=%lu ver=%lu modes=%lu\n", (unsigned long)$ps_pkg_before, $ps_id_before, $ps_ver_before, $ps_modes_before
      printf "step2: attempt FileX mount\n"
      ps_storage_filex_mount_wait

      set $ps_pkg_after = (game_package_desc_t*)GamePackage_GetActive()
      set $ps_m_ok = (unsigned long)(g_storage_filex_mount_count - $ps_m_ok0)
      set $ps_m_fail = (unsigned long)(g_storage_filex_mount_fail_count - $ps_m_fail0)

      if $ps_pkg_after == 0
        printf "FAIL: active package missing after FileX mount attempt\n"
      else
        set $ps_id_ok = (unsigned long)($ps_pkg_after->package_id == $ps_id_before)
        set $ps_ver_ok = (unsigned long)($ps_pkg_after->package_version == $ps_ver_before)
        set $ps_modes_ok = (unsigned long)($ps_pkg_after->mode_count == $ps_modes_before)
        printf "after_mount: ptr=0x%08lx id=%lu ver=%lu modes=%lu\n", (unsigned long)$ps_pkg_after, (unsigned long)$ps_pkg_after->package_id, (unsigned long)$ps_pkg_after->package_version, (unsigned long)$ps_pkg_after->mode_count

        if (($ps_id_ok != 0) && ($ps_ver_ok != 0) && ($ps_modes_ok != 0))
          if $ps_m_fail > 0
            printf "PASS: FileX mount failed and active package remained usable\n"
          else
            if $ps_m_ok > 0
              printf "INFO: FileX mount succeeded; failure fallback path not exercised (active package preserved)\n"
            else
              printf "WARN: mount outcome counters unchanged (m_ok=%lu m_fail=%lu)\n", $ps_m_ok, $ps_m_fail
            end
          end
        else
          printf "FAIL: active package changed across FileX mount attempt (id_ok=%lu ver_ok=%lu modes_ok=%lu)\n", $ps_id_ok, $ps_ver_ok, $ps_modes_ok
        end
      end

      if $ps_m_ok > 0
        printf "step3: cleanup unmount after successful mount\n"
        ps_storage_filex_unmount_wait
      end
    end
    ps_storage_status
    ps_storage_pkg_manifest_status
  end
end

define ps_input_status
  printf "== Input status ==\n"
  printf "qInputCmd=%u qInputRaw=%u qUI=%u qGame=%u quiesced=%lu\n", (unsigned int)g_q_input_cmd.tx_queue_enqueued, (unsigned int)g_q_input_raw.tx_queue_enqueued, (unsigned int)g_q_ui_events.tx_queue_enqueued, (unsigned int)g_q_game_events.tx_queue_enqueued, (unsigned long)g_input_quiesced
  printf "raw: post=%lu recv=%lu drop=%lu suppressed=%lu\n", (unsigned long)g_input_raw_post_count, (unsigned long)g_input_raw_recv_count, (unsigned long)g_input_raw_drop_count, (unsigned long)g_input_raw_suppressed_count
  printf "action: total=%lu ui=%lu game=%lu sys=%lu ignored=%lu\n", (unsigned long)g_input_action_total_count, (unsigned long)g_input_action_ui_route_count, (unsigned long)g_input_action_game_route_count, (unsigned long)g_input_action_system_route_count, (unsigned long)g_input_action_ignored_count
  printf "posts: ui_ok=%lu ui_drop=%lu ui_drop_old=%lu game_ok=%lu game_drop=%lu game_drop_old=%lu\n", (unsigned long)g_input_action_ui_post_count, (unsigned long)g_input_action_ui_drop_count, (unsigned long)g_input_action_ui_drop_oldest_count, (unsigned long)g_input_action_game_post_count, (unsigned long)g_input_action_game_drop_count, (unsigned long)g_input_action_game_drop_oldest_count
  printf "filter: debounce_drop=%lu release_pass=%lu release_reconcile=%lu repeat_emit=%lu\n", (unsigned long)g_input_debounce_drop_count, (unsigned long)g_input_release_pass_count, (unsigned long)g_input_release_reconcile_count, (unsigned long)g_input_repeat_emit_count
  printf "stop_wake_pending_mask=0x%08lx\n", (unsigned long)g_input_stop_wake_pending_mask
  printf "long_emit=%lu joy_mask=0x%08lx\n", (unsigned long)g_input_long_emit_count, (unsigned long)g_sensor_joy_input_mask
  printf "syspost: activity_ok=%lu activity_drop=%lu menu_ok=%lu menu_drop=%lu\n", (unsigned long)g_input_sys_activity_post_count, (unsigned long)g_input_sys_activity_drop_count, (unsigned long)g_input_sys_menu_post_count, (unsigned long)g_input_sys_menu_drop_count
  printf "consumed: ui=%lu (last=%lu) game=%lu (last=%lu)\n", (unsigned long)g_ui_event_recv_count, (unsigned long)g_ui_event_last_action, (unsigned long)g_game_event_recv_count, (unsigned long)g_game_event_last_action
  printf "handled: ui_ok=%lu ui_ignored=%lu ui_qerr=%lu game_ok=%lu game_ignored=%lu game_qerr=%lu\n", (unsigned long)g_ui_event_handled_count, (unsigned long)g_ui_event_ignored_count, (unsigned long)g_ui_event_queue_error_count, (unsigned long)g_game_event_handled_count, (unsigned long)g_game_event_ignored_count, (unsigned long)g_game_event_queue_error_count
  printf "game_stale_drops=%lu\n", (unsigned long)g_game_event_stale_drop_count
  printf "ui_ignored_reason: null=%lu mode=%lu stop_joy=%lu no_action=%lu no_event=%lu map_fail=%lu no_evt=%lu long_rej=%lu repeat_rej=%lu release_rej=%lu policy_rej=%lu menu_noop=%lu router_unhandled=%lu denied_audio=%lu\n", (unsigned long)g_ui_ignore_null_evt_count, (unsigned long)g_ui_ignore_not_ui_mode_count, (unsigned long)g_ui_ignore_stop_joy_count, (unsigned long)g_ui_ignore_unmapped_action_count, (unsigned long)g_ui_ignore_unmapped_event_count, (unsigned long)g_ui_ignore_router_map_fail_count, (unsigned long)g_ui_ignore_router_no_evt_count, (unsigned long)g_ui_ignore_router_long_reject_count, (unsigned long)g_ui_ignore_router_repeat_reject_count, (unsigned long)g_ui_ignore_router_release_reject_count, (unsigned long)g_ui_ignore_router_policy_reject_count, (unsigned long)g_ui_ignore_router_menu_boundary_noop_count, (unsigned long)g_ui_ignore_router_unhandled_count, (unsigned long)g_ui_denied_audio_emit_count
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
  set g_ui_ignore_null_evt_count = 0
  set g_ui_ignore_not_ui_mode_count = 0
  set g_ui_ignore_stop_joy_count = 0
  set g_ui_ignore_unmapped_action_count = 0
  set g_ui_ignore_unmapped_event_count = 0
  set g_ui_ignore_router_map_fail_count = 0
  set g_ui_ignore_router_no_evt_count = 0
  set g_ui_ignore_router_long_reject_count = 0
  set g_ui_ignore_router_repeat_reject_count = 0
  set g_ui_ignore_router_release_reject_count = 0
  set g_ui_ignore_router_policy_reject_count = 0
  set g_ui_ignore_router_menu_boundary_noop_count = 0
  set g_ui_ignore_router_unhandled_count = 0
  set g_ui_denied_audio_emit_count = 0
  set g_game_event_handled_count = 0
  set g_game_event_ignored_count = 0
  set g_game_event_stale_drop_count = 0
  set g_game_event_queue_error_count = 0
  set g_input_debounce_drop_count = 0
  set g_input_release_pass_count = 0
  set g_input_release_reconcile_count = 0
  set g_input_repeat_emit_count = 0
  set g_input_stop_wake_pending_mask = 0
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
    printf "src=%u pressed=%lu edge_seen=%lu press_tick=%lu held_ms=%lu next_repeat_tick=%lu long=%lu last_edge=%lu\n", $__ps_i, (unsigned long)g_input_button_state[$__ps_i].pressed, (unsigned long)g_input_button_state[$__ps_i].edge_seen, (unsigned long)g_input_button_state[$__ps_i].press_tick, (unsigned long)$__ps_held, (unsigned long)g_input_button_state[$__ps_i].next_repeat_tick, (unsigned long)g_input_button_state[$__ps_i].long_sent, (unsigned long)g_input_button_state[$__ps_i].last_edge_tick
    set $__ps_i = $__ps_i + 1
  end
end

define ps_input_snap
  ps_input_status
  ps_input_latch
  ps_input_decode_last
end

define ps_rt_input_tune
  printf "== RT topdown input tune ==\n"
  printf "override controller_profile=%lu\n", (unsigned long)g_topdown_dbg_controller_profile_override
  printf "set profile:    set g_topdown_dbg_controller_profile_override = <0..4> (0=package)\n"
end

define ps_rt_input_tune_clear
  set g_topdown_dbg_controller_profile_override = 0
  ps_rt_input_tune
end

define ps_rt_tune_help
  printf "== RT live tune wrappers ==\n"
  printf "safe scratch: g_game_runtime_dbg_tune_patch\n"
  printf "ps_rt_tune_status\n"
  printf "ps_rt_tune_reset\n"
  printf "ps_rt_tune_camera <follow_permille> <lookahead_x_px> <max_speed_px_s>\n"
  printf "ps_rt_tune_move <speed_px_s> <accel_px_s2> <decel_px_s2>\n"
  printf "ps_rt_tune_render <scale> <present_mode:0/1/2>\n"
  printf "ps_rt_tune_input <controller_profile_id> <input_flags> <deadzone_permille>\n"
end

define ps_rt_tune_status
  set $__ps_cfg = (const game_package_runtime_config_t*)GameRuntime_GetActiveModeConfig()
  if $__ps_cfg == 0
    printf "rt tune: no active runtime config (enter REALTIME first)\n"
  else
    printf "== RT active mode tune ==\n"
    printf "move: speed=%lu accel=%lu decel=%lu\n", (unsigned long)$__ps_cfg->move_speed_px_s, (unsigned long)$__ps_cfg->move_accel_px_s2, (unsigned long)$__ps_cfg->move_decel_px_s2
    printf "camera: profile=%lu follow=%lu max=%lu dz=%lux%lu lookahead=(%ld,%ld)\n", (unsigned long)$__ps_cfg->camera_profile_id, (unsigned long)$__ps_cfg->camera_follow_permille, (unsigned long)$__ps_cfg->camera_max_speed_px_s, (unsigned long)$__ps_cfg->camera_deadzone_w_px, (unsigned long)$__ps_cfg->camera_deadzone_h_px, (long)$__ps_cfg->camera_lookahead_x_px, (long)$__ps_cfg->camera_lookahead_y_px
    printf "input: profile=%lu flags=0x%08lx deadzone_permille=%lu\n", (unsigned long)$__ps_cfg->controller_profile_id, (unsigned long)$__ps_cfg->input_flags, (unsigned long)$__ps_cfg->input_deadzone_permille
    printf "render: scale=%lu present_mode=%lu\n", (unsigned long)$__ps_cfg->topdown_render_scale, (unsigned long)$__ps_cfg->topdown_tile_present_mode
  end
  printf "debug_tune: apply_pending=%lu reset_pending=%lu ok=%lu fail=%lu last_status=%lu last_mode=%lu\n", (unsigned long)g_game_runtime_dbg_tune_apply_pending, (unsigned long)g_game_runtime_dbg_tune_reset_pending, (unsigned long)g_game_runtime_dbg_tune_apply_ok_count, (unsigned long)g_game_runtime_dbg_tune_apply_fail_count, (unsigned long)g_game_runtime_dbg_tune_last_status, (unsigned long)g_game_runtime_dbg_tune_last_mode_id
end

define ps_rt_manifest_refs
  set $__ps_cfg = (const game_package_runtime_config_t*)GameRuntime_GetActiveModeConfig()
  if $__ps_cfg == 0
    printf "rt manifest refs: no active runtime config (enter REALTIME first)\n"
  else
    printf "== RT manifest refs ==\n"
    printf "scene refs: map_id=%lu tileset_id=%lu\n", (unsigned long)$__ps_cfg->scene_map_id, (unsigned long)$__ps_cfg->scene_tileset_id
    printf "scene bound: map=0x%08lx/%lu tileset=0x%08lx/%lu\n", (unsigned long)g_game_rt_scene_map_addr, (unsigned long)g_game_rt_scene_map_size, (unsigned long)g_game_rt_scene_tileset_addr, (unsigned long)g_game_rt_scene_tileset_size
    printf "audio refs: music=%lu interact=%lu confirm=%lu error=%lu\n", (unsigned long)$__ps_cfg->music_asset_id, (unsigned long)$__ps_cfg->sfx_interact_asset_id, (unsigned long)$__ps_cfg->sfx_confirm_asset_id, (unsigned long)$__ps_cfg->sfx_error_asset_id
    printf "audio bound: music=%lu interact=%lu confirm=%lu error=%lu started=%lu\n", (unsigned long)g_game_rt_music_asset, (unsigned long)g_game_rt_sfx_interact_asset, (unsigned long)g_game_rt_sfx_confirm_asset, (unsigned long)g_game_rt_sfx_error_asset, (unsigned long)g_game_rt_music_started
  end
end

define ps_rt_tune_reset
  set g_game_runtime_dbg_tune_reset_pending = 1
  printf "rt tune reset queued (continue for thGame to apply)\n"
  ps_rt_tune_status
end

define ps_rt_tune_camera
  if $argc < 3
    printf "usage: ps_rt_tune_camera <follow_permille> <lookahead_x_px> <max_speed_px_s>\n"
  else
    set $__ps_patch = (game_runtime_tune_patch_t*)&g_game_runtime_dbg_tune_patch
    p (void*)memset($__ps_patch, 0, sizeof(game_runtime_tune_patch_t))
    set $__ps_patch->field_mask = (unsigned int)(GAME_RT_TUNE_CAMERA_FOLLOW_PERMILLE | GAME_RT_TUNE_CAMERA_LOOKAHEAD_X_PX | GAME_RT_TUNE_CAMERA_MAX_SPEED_PX_S)
    set $__ps_patch->camera_follow_permille = (unsigned int)$arg0
    set $__ps_patch->camera_lookahead_x_px = (int)$arg1
    set $__ps_patch->camera_max_speed_px_s = (unsigned int)$arg2
    set g_game_runtime_dbg_tune_apply_pending = 1
    printf "rt tune camera queued (continue for thGame to apply)\n"
    ps_rt_tune_status
  end
end

define ps_rt_tune_move
  if $argc < 3
    printf "usage: ps_rt_tune_move <speed_px_s> <accel_px_s2> <decel_px_s2>\n"
  else
    set $__ps_patch = (game_runtime_tune_patch_t*)&g_game_runtime_dbg_tune_patch
    p (void*)memset($__ps_patch, 0, sizeof(game_runtime_tune_patch_t))
    set $__ps_patch->field_mask = (unsigned int)(GAME_RT_TUNE_MOVE_SPEED_PX_S | GAME_RT_TUNE_MOVE_ACCEL_PX_S2 | GAME_RT_TUNE_MOVE_DECEL_PX_S2)
    set $__ps_patch->move_speed_px_s = (unsigned int)$arg0
    set $__ps_patch->move_accel_px_s2 = (unsigned int)$arg1
    set $__ps_patch->move_decel_px_s2 = (unsigned int)$arg2
    set g_game_runtime_dbg_tune_apply_pending = 1
    printf "rt tune move queued (continue for thGame to apply)\n"
    ps_rt_tune_status
  end
end

define ps_rt_tune_render
  if $argc < 2
    printf "usage: ps_rt_tune_render <scale> <present_mode:0/1/2>\n"
  else
    set $__ps_patch = (game_runtime_tune_patch_t*)&g_game_runtime_dbg_tune_patch
    p (void*)memset($__ps_patch, 0, sizeof(game_runtime_tune_patch_t))
    set $__ps_patch->field_mask = (unsigned int)(GAME_RT_TUNE_TOPDOWN_RENDER_SCALE | GAME_RT_TUNE_TOPDOWN_TILE_PRESENT_MODE)
    set $__ps_patch->topdown_render_scale = (unsigned int)$arg0
    set $__ps_patch->topdown_tile_present_mode = (unsigned int)$arg1
    set g_game_runtime_dbg_tune_apply_pending = 1
    printf "rt tune render queued (continue for thGame to apply)\n"
    ps_rt_tune_status
  end
end

define ps_rt_tune_input
  if $argc < 3
    printf "usage: ps_rt_tune_input <controller_profile_id> <input_flags> <deadzone_permille>\n"
  else
    set $__ps_patch = (game_runtime_tune_patch_t*)&g_game_runtime_dbg_tune_patch
    p (void*)memset($__ps_patch, 0, sizeof(game_runtime_tune_patch_t))
    set $__ps_patch->field_mask = (unsigned int)(GAME_RT_TUNE_CONTROLLER_PROFILE_ID | GAME_RT_TUNE_INPUT_FLAGS | GAME_RT_TUNE_INPUT_DEADZONE_PERMILLE)
    set $__ps_patch->controller_profile_id = (unsigned int)$arg0
    set $__ps_patch->input_flags = (unsigned int)$arg1
    set $__ps_patch->input_deadzone_permille = (unsigned int)$arg2
    set g_game_runtime_dbg_tune_apply_pending = 1
    printf "rt tune input queued (continue for thGame to apply)\n"
    ps_rt_tune_status
  end
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
  echo mode power bus tmag_state tmag_err tmag_fail tmag_try tmag_retry_tick tmag_ok\n
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

define ps_joy_raw
  echo === joy_raw ===\n
  if (g_sensor_joy == 0)
    printf "g_sensor_joy is null\n"
  else
    set $__ps_x = 0.0f
    set $__ps_y = 0.0f
    set $__ps_z = 0.0f
    set $__ps_rc_mT = (int)TMAG5273_read_mT(&$__ps_x, &$__ps_y, &$__ps_z)
    printf "TMAG5273_read_mT rc=%d x_mT=%.3f y_mT=%.3f z_mT=%.3f\n", $__ps_rc_mT, $__ps_x, $__ps_y, $__ps_z
    set $__ps_nx = 0.0f
    set $__ps_ny = 0.0f
    set $__ps_r = 0.0f
    set $__ps_rc_cal = (int)TMAGJoy_ReadCalibratedRaw(g_sensor_joy, &$__ps_nx, &$__ps_ny, &$__ps_r)
    printf "TMAGJoy_ReadCalibratedRaw rc=%d nx=%.4f ny=%.4f r_mT=%.3f\n", $__ps_rc_cal, $__ps_nx, $__ps_ny, $__ps_r
    printf "live_status nx=%.4f ny=%.4f r_mT=%.3f dir=%lu mask=0x%08lx\n", g_sensor_joy_live_status.nx, g_sensor_joy_live_status.ny, g_sensor_joy_live_status.r_abs_mT, (unsigned long)g_sensor_joy_live_status.dir, (unsigned long)g_sensor_joy_live_status.input_mask
  end
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
  printf "dma_pending: half=%lu full=%lu err=%lu missed_half=%lu missed_full=%lu missed_err=%lu\n", (unsigned long)g_audio_dma_half_pending, (unsigned long)g_audio_dma_full_pending, (unsigned long)g_audio_dma_error_pending, (unsigned long)g_audio_half_missed_count, (unsigned long)g_audio_full_missed_count, (unsigned long)g_audio_error_missed_count
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

define ps_audio_phase3_stress20
  set $ps_cycles = 20
  if $argc >= 1
    set $ps_cycles = (unsigned long)$arg0
  end
  if $ps_cycles == 0
    set $ps_cycles = 20
  end
  set $ps_keepalive_every = 4

  printf "== Audio phase3 rapid stress ==\n"
  printf "cycles=%lu\n", $ps_cycles
  set $ps_fail = 0
  set $ps_start0 = (unsigned long)g_audio_start_count
  set $ps_stop0 = (unsigned long)g_audio_stop_count
  set $ps_und0 = (unsigned long)g_audio_underflow_count
  set $ps_eirq0 = (unsigned long)g_audio_error_irq_count
  set $ps_mh0 = (unsigned long)g_audio_half_missed_count
  set $ps_mf0 = (unsigned long)g_audio_full_missed_count
  set $ps_me0 = (unsigned long)g_audio_error_missed_count
  set $ps_last_err0 = (long)g_audio_last_error

  set $ps_rc = (unsigned int)App_SysEvent_ModeSet(2)
  printf "queue MODE_SET(2) rc=%u\n", $ps_rc
  __ps_continue_for_ticks 2

  printf "phaseA: start/stop cycles\n"
  set $__ps_i = 0
  while $__ps_i < $ps_cycles
    if (($__ps_i % $ps_keepalive_every) == 0)
      set $ps_rc = (unsigned int)App_SysEvent_ModeSet(2)
      if $ps_rc != 0
        printf "WARN: mode keepalive post failed at cycle=%lu rc=%u\n", $__ps_i, $ps_rc
      end
    end
    set $ps_rc = (unsigned int)App_AudioReq_StartTone()
    if $ps_rc != 0
      printf "FAIL: start post failed at cycle=%lu rc=%u\n", $__ps_i, $ps_rc
      set $ps_fail = 1
      set $__ps_i = $ps_cycles
    else
      __ps_continue_for_ticks 1
      set $ps_rc = (unsigned int)App_AudioReq_Stop()
      if $ps_rc != 0
        printf "FAIL: stop post failed at cycle=%lu rc=%u\n", $__ps_i, $ps_rc
        set $ps_fail = 1
        set $__ps_i = $ps_cycles
      else
        __ps_continue_for_ticks 1
      end
      set $__ps_i = $__ps_i + 1
    end
  end

  if $ps_fail == 0
    printf "phaseB: start + quiesce/resume cycles\n"
    set $__ps_i = 0
    while $__ps_i < $ps_cycles
      if (($__ps_i % $ps_keepalive_every) == 0)
        set $ps_rc = (unsigned int)App_SysEvent_ModeSet(2)
        if $ps_rc != 0
          printf "WARN: mode keepalive post failed at cycle=%lu rc=%u\n", $__ps_i, $ps_rc
        end
      end
      set $ps_rc = (unsigned int)App_AudioReq_StartTone()
      if $ps_rc != 0
        printf "FAIL: start(post-qr) failed at cycle=%lu rc=%u\n", $__ps_i, $ps_rc
        set $ps_fail = 1
        set $__ps_i = $ps_cycles
      else
        __ps_continue_for_ticks 1
        set $ps_rc = (unsigned int)App_SysEvent_QuiesceReq()
        if $ps_rc != 0
          printf "FAIL: quiesce post failed at cycle=%lu rc=%u\n", $__ps_i, $ps_rc
          set $ps_fail = 1
          set $__ps_i = $ps_cycles
        else
          __ps_continue_for_ticks 2
          set $ps_rc = (unsigned int)App_SysEvent_ResumeReq()
          if $ps_rc != 0
            printf "FAIL: resume post failed at cycle=%lu rc=%u\n", $__ps_i, $ps_rc
            set $ps_fail = 1
            set $__ps_i = $ps_cycles
          else
            __ps_continue_for_ticks 2
            set $__ps_i = $__ps_i + 1
          end
        end
      end
    end
  end

  set $ps_rc = (unsigned int)App_AudioReq_Stop()
  if $ps_rc == 0
    __ps_continue_for_ticks 2
  end
  __ps_release_wait_bp

  printf "== Audio phase3 stress delta ==\n"
  printf "starts=%lu stops=%lu underrun=%lu err_irq=%lu\n", (unsigned long)((unsigned long)g_audio_start_count - $ps_start0), (unsigned long)((unsigned long)g_audio_stop_count - $ps_stop0), (unsigned long)((unsigned long)g_audio_underflow_count - $ps_und0), (unsigned long)((unsigned long)g_audio_error_irq_count - $ps_eirq0)
  printf "missed_half=%lu missed_full=%lu missed_err=%lu last_err:%ld->%ld\n", (unsigned long)((unsigned long)g_audio_half_missed_count - $ps_mh0), (unsigned long)((unsigned long)g_audio_full_missed_count - $ps_mf0), (unsigned long)((unsigned long)g_audio_error_missed_count - $ps_me0), (long)$ps_last_err0, (long)g_audio_last_error
  ps_audio_status
  if $ps_fail == 0
    if (((unsigned long)g_audio_underflow_count - $ps_und0) == 0) && (((unsigned long)g_audio_error_irq_count - $ps_eirq0) == 0) && ((long)g_audio_last_error == 0)
      printf "PASS: rapid stress completed with no underrun/error regression\n"
    else
      printf "WARN: stress completed but counters indicate audio service errors; review above deltas\n"
    end
  else
    printf "FAIL: stress aborted due to command-post failure\n"
  end
end

define ps_audio_diag_realtime_start
  printf "== Audio realtime diag start ==\n"
  set $ps_ad_music_valid = 0
  set $ps_ad_stress_valid = 0
  set $ps_mode_rc = (unsigned int)App_SysEvent_ModeSet(2)
  printf "queue MODE_SET(2) rc=%u\n", $ps_mode_rc
  if ($ps_mode_rc != 0)
    printf "mode request failed (system likely not fully running). resume target, then rerun.\n"
  else
    set $ps_rc = (unsigned int)App_AudioReq_Stop()
    printf "queue AUDIO_STOP rc=%u\n", $ps_rc
    set $ps_cnt = (unsigned long)AppAudioAssets_Count()
    set $ps_ad_music_id = 0
    set $ps_ad_sfx_id = 0
    set $ps_ad_music_samples = 0
    set $ps_ad_sfx_samples = 0
    set $__ps_i = 1
    while $__ps_i < $ps_cnt
      set $ps_clip = AppAudioAssets_GetClip((uint32_t)$__ps_i)
      if ($ps_clip != 0)
        set $ps_samples = (unsigned long)$ps_clip->total_samples
        if ($ps_samples > $ps_ad_music_samples)
          set $ps_ad_sfx_id = $ps_ad_music_id
          set $ps_ad_sfx_samples = $ps_ad_music_samples
          set $ps_ad_music_id = $__ps_i
          set $ps_ad_music_samples = $ps_samples
        else
          if ($ps_samples > $ps_ad_sfx_samples)
            set $ps_ad_sfx_id = $__ps_i
            set $ps_ad_sfx_samples = $ps_samples
          end
        end
      end
      set $__ps_i = $__ps_i + 1
    end
    printf "-- assets --\n"
    printf "count=%lu music_id=%lu sfx_id=%lu\n", (unsigned long)$ps_cnt, (unsigned long)$ps_ad_music_id, (unsigned long)$ps_ad_sfx_id
    if ($ps_ad_music_id == 0)
      printf "no valid embedded music clip found\n"
    else
      printf "music_sr=%lu music_total=%lu\n", (unsigned long)AppAudioAssets_GetClip((uint32_t)$ps_ad_music_id)->sample_rate_hz, (unsigned long)$ps_ad_music_samples
      if ($ps_ad_sfx_id != 0)
        printf "sfx_sr=%lu sfx_total=%lu\n", (unsigned long)AppAudioAssets_GetClip((uint32_t)$ps_ad_sfx_id)->sample_rate_hz, (unsigned long)$ps_ad_sfx_samples
      end
      set $ps_play_rc = (unsigned int)App_AudioReq_PlayAsset((app_audio_asset_id_t)$ps_ad_music_id)
      printf "queue PLAY_ASSET(music=%lu) rc=%u\n", (unsigned long)$ps_ad_music_id, $ps_play_rc
      ps_audio_status
      if ($ps_play_rc == 0)
        set $ps_ad_music_valid = 1
        set $ps_ad_h0 = (unsigned long)g_audio_half_irq_count
        set $ps_ad_f0 = (unsigned long)g_audio_full_irq_count
        set $ps_ad_u0 = (unsigned long)g_audio_underflow_count
        set $ps_ad_ar0 = (unsigned long)g_th_audio.tx_thread_run_count
        set $ps_ad_dr0 = (unsigned long)g_th_display.tx_thread_run_count
        set $ps_ad_t0 = (unsigned long)HAL_GetTick()
        printf "run target ~5s, Ctrl-C, then run: ps_audio_diag_realtime_music_end\n"
      else
        printf "music request failed; do not run *_music_end yet.\n"
      end
    end
  end
end

define ps_audio_diag_realtime_music_end
  if (($ps_ad_music_valid + 0) == 0)
    printf "music mark not set (run ps_audio_diag_realtime_start first)\n"
  else
    printf "== Audio realtime music-only delta ==\n"
    printf "dt_ms=%lu\n", (unsigned long)((unsigned long)HAL_GetTick() - $ps_ad_t0)
    printf "half=%lu full=%lu underrun=%lu\n", (unsigned long)((unsigned long)g_audio_half_irq_count - $ps_ad_h0), (unsigned long)((unsigned long)g_audio_full_irq_count - $ps_ad_f0), (unsigned long)((unsigned long)g_audio_underflow_count - $ps_ad_u0)
    printf "thAudio_runs=%lu thDisplay_runs=%lu qAudio=%u\n", (unsigned long)((unsigned long)g_th_audio.tx_thread_run_count - $ps_ad_ar0), (unsigned long)((unsigned long)g_th_display.tx_thread_run_count - $ps_ad_dr0), (unsigned int)g_q_audio_cmd.tx_queue_enqueued
    ps_audio_status
  end
end

define ps_audio_diag_realtime_stress_start
  set $ps_ad_stress_valid = 0
  if (($ps_ad_music_valid + 0) == 0)
    printf "music mark not set; run ps_audio_diag_realtime_start first.\n"
  else
    if (($ps_ad_sfx_id + 0) == 0)
      printf "no valid secondary/burst clip discovered in start pass.\n"
    else
      set $ps_ad_stress_valid = 1
      set $ps_ad_h1 = (unsigned long)g_audio_half_irq_count
      set $ps_ad_f1 = (unsigned long)g_audio_full_irq_count
      set $ps_ad_u1 = (unsigned long)g_audio_underflow_count
      set $ps_ad_ar1 = (unsigned long)g_th_audio.tx_thread_run_count
      set $ps_ad_dr1 = (unsigned long)g_th_display.tx_thread_run_count
      set $ps_ad_t1 = (unsigned long)HAL_GetTick()
      set $__ps_i = 0
      while $__ps_i < 20
        set $ps_rc = (unsigned int)App_AudioReq_PlayAsset((app_audio_asset_id_t)$ps_ad_sfx_id)
        set $__ps_i = $__ps_i + 1
      end
      printf "queued 20 x PLAY_ASSET(sfx=%lu); run target ~3s, Ctrl-C, then run: ps_audio_diag_realtime_stress_end\n", (unsigned long)$ps_ad_sfx_id
    end
  end
end

define ps_audio_assets
  set $ps_cnt = (unsigned long)AppAudioAssets_Count()
  printf "== Audio assets ==\n"
  printf "count=%lu\n", $ps_cnt
  set $__ps_i = 0
  while $__ps_i < $ps_cnt
    if ($__ps_i == 0)
      printf "[%lu] Silence\n", (unsigned long)$__ps_i
    else
      set $ps_clip = AppAudioAssets_GetClip((uint32_t)$__ps_i)
      if ($ps_clip != 0)
        printf "[%lu] %s sr=%lu samples=%lu\n", (unsigned long)$__ps_i, AppAudioAssets_Name((uint32_t)$__ps_i), (unsigned long)$ps_clip->sample_rate_hz, (unsigned long)$ps_clip->total_samples
      else
        printf "[%lu] (invalid)\n", (unsigned long)$__ps_i
      end
    end
    set $__ps_i = $__ps_i + 1
  end
end

define ps_audio_diag_realtime_stress_end
  if (($ps_ad_stress_valid + 0) == 0)
    printf "stress mark not set (run ps_audio_diag_realtime_stress_start first)\n"
  else
    printf "== Audio realtime stress delta ==\n"
    printf "dt_ms=%lu\n", (unsigned long)((unsigned long)HAL_GetTick() - $ps_ad_t1)
    printf "half=%lu full=%lu underrun=%lu\n", (unsigned long)((unsigned long)g_audio_half_irq_count - $ps_ad_h1), (unsigned long)((unsigned long)g_audio_full_irq_count - $ps_ad_f1), (unsigned long)((unsigned long)g_audio_underflow_count - $ps_ad_u1)
    printf "thAudio_runs=%lu thDisplay_runs=%lu qAudio=%u\n", (unsigned long)((unsigned long)g_th_audio.tx_thread_run_count - $ps_ad_ar1), (unsigned long)((unsigned long)g_th_display.tx_thread_run_count - $ps_ad_dr1), (unsigned int)g_q_audio_cmd.tx_queue_enqueued
    ps_audio_status
  end
end

define ps_audio_poly4_start
  printf "== Audio poly(4) mixed-clip start ==\n"
  set $ps_poly_valid = 0
  set $ps_poly_posts = 0
  set $ps_poly_drop = 0
  set $ps_mode_rc = (unsigned int)App_SysEvent_ModeSet(2)
  printf "queue MODE_SET(2) rc=%u\n", $ps_mode_rc
  if ($ps_mode_rc != 0)
    printf "mode request failed (system likely not fully running). resume target, then rerun.\n"
  else
    set $ps_rc = (unsigned int)App_AudioReq_Stop()
    printf "queue AUDIO_STOP rc=%u\n", $ps_rc
    set $ps_music_rc = (unsigned int)App_AudioReq_PlayAsset((app_audio_asset_id_t)6)
    printf "queue PLAY_ASSET(6 music) rc=%u\n", $ps_music_rc
    if ($ps_music_rc == 0)
      set $ps_poly_valid = 1
      set $ps_poly_h0 = (unsigned long)g_audio_half_irq_count
      set $ps_poly_f0 = (unsigned long)g_audio_full_irq_count
      set $ps_poly_u0 = (unsigned long)g_audio_underflow_count
      set $ps_poly_mh0 = (unsigned long)g_audio_half_missed_count
      set $ps_poly_mf0 = (unsigned long)g_audio_full_missed_count
      set $ps_poly_ar0 = (unsigned long)g_th_audio.tx_thread_run_count
      set $ps_poly_dr0 = (unsigned long)g_th_display.tx_thread_run_count
      set $ps_poly_t0 = (unsigned long)HAL_GetTick()
      set $__ps_round = 0
      while $__ps_round < 6
        set $ps_rc = (unsigned int)App_AudioReq_PlayAsset((app_audio_asset_id_t)1)
        set $ps_poly_posts = $ps_poly_posts + 1
        if ($ps_rc != 0)
          set $ps_poly_drop = $ps_poly_drop + 1
        end
        set $ps_rc = (unsigned int)App_AudioReq_PlayAsset((app_audio_asset_id_t)5)
        set $ps_poly_posts = $ps_poly_posts + 1
        if ($ps_rc != 0)
          set $ps_poly_drop = $ps_poly_drop + 1
        end
        set $ps_rc = (unsigned int)App_AudioReq_PlayAsset((app_audio_asset_id_t)2)
        set $ps_poly_posts = $ps_poly_posts + 1
        if ($ps_rc != 0)
          set $ps_poly_drop = $ps_poly_drop + 1
        end
        set $ps_rc = (unsigned int)App_AudioReq_PlayAsset((app_audio_asset_id_t)3)
        set $ps_poly_posts = $ps_poly_posts + 1
        if ($ps_rc != 0)
          set $ps_poly_drop = $ps_poly_drop + 1
        end
        set $ps_rc = (unsigned int)App_AudioReq_PlayAsset((app_audio_asset_id_t)4)
        set $ps_poly_posts = $ps_poly_posts + 1
        if ($ps_rc != 0)
          set $ps_poly_drop = $ps_poly_drop + 1
        end
        set $__ps_round = $__ps_round + 1
      end
      printf "queued mixed pattern: posts=%lu drops=%lu\n", (unsigned long)$ps_poly_posts, (unsigned long)$ps_poly_drop
      printf "listen for concurrent different SFX over music for ~4s, then run: ps_audio_poly4_end\n"
    else
      printf "music request failed; do not run ps_audio_poly4_end yet.\n"
    end
  end
end

define ps_audio_poly4_end
  if (($ps_poly_valid + 0) == 0)
    printf "poly mark not set (run ps_audio_poly4_start first)\n"
  else
    printf "== Audio poly(4) mixed-clip delta ==\n"
    printf "dt_ms=%lu posts=%lu drops=%lu\n", (unsigned long)((unsigned long)HAL_GetTick() - $ps_poly_t0), (unsigned long)$ps_poly_posts, (unsigned long)$ps_poly_drop
    printf "half=%lu full=%lu underrun=%lu\n", (unsigned long)((unsigned long)g_audio_half_irq_count - $ps_poly_h0), (unsigned long)((unsigned long)g_audio_full_irq_count - $ps_poly_f0), (unsigned long)((unsigned long)g_audio_underflow_count - $ps_poly_u0)
    printf "missed_half=%lu missed_full=%lu\n", (unsigned long)((unsigned long)g_audio_half_missed_count - $ps_poly_mh0), (unsigned long)((unsigned long)g_audio_full_missed_count - $ps_poly_mf0)
    printf "thAudio_runs=%lu thDisplay_runs=%lu qAudio=%u\n", (unsigned long)((unsigned long)g_th_audio.tx_thread_run_count - $ps_poly_ar0), (unsigned long)((unsigned long)g_th_display.tx_thread_run_count - $ps_poly_dr0), (unsigned int)g_q_audio_cmd.tx_queue_enqueued
    ps_audio_status
  end
end

define ps_audio_overlap_start
  printf "== Audio overlap start (music-loop + burst) ==\n"
  set $ps_ov_valid = 0
  set $ps_ov_posts = 0
  set $ps_ov_drop = 0
  set $ps_mode_rc = (unsigned int)App_SysEvent_ModeSet(2)
  printf "queue MODE_SET(2) rc=%u\n", $ps_mode_rc
  if ($ps_mode_rc != 0)
    printf "mode request failed (system likely not fully running). resume target, then rerun.\n"
  else
    set $ps_sysclk = (unsigned long)HAL_RCC_GetSysClockFreq()
    set $ps_prof = (unsigned long)g_power_perf_profile_current
    if (($ps_prof != 1) || ($ps_sysclk < 80000000))
      printf "turbo not active yet (profile=%lu sysclk=%lu). run target ~1s, then rerun ps_audio_overlap_start.\n", $ps_prof, $ps_sysclk
    else
      set $ps_cnt = (unsigned long)AppAudioAssets_Count()
      if ($ps_cnt <= 1)
        printf "no embedded audio assets found (count=%lu)\n", $ps_cnt
      else
        set $ps_long_id = 0
        set $ps_burst_id = 0
        set $ps_long_samples = 0
        set $ps_burst_samples = 0
        set $ps_burst_sfx_id = 0
        set $ps_burst_sfx_samples = 0
        set $ps_burst_non_ui_id = 0
        set $ps_burst_non_ui_samples = 0
        set $__ps_i = 1
        while $__ps_i < $ps_cnt
          set $ps_clip = AppAudioAssets_GetClip((uint32_t)$__ps_i)
          if ($ps_clip != 0)
            set $ps_samples = (unsigned long)$ps_clip->total_samples
            set $ps_name = AppAudioAssets_Name((uint32_t)$__ps_i)
            if ($ps_samples > $ps_long_samples)
              set $ps_burst_id = $ps_long_id
              set $ps_burst_samples = $ps_long_samples
              set $ps_long_id = $__ps_i
              set $ps_long_samples = $ps_samples
            else
              if ($ps_samples > $ps_burst_samples)
                set $ps_burst_samples = $ps_samples
                set $ps_burst_id = $__ps_i
              end
            end
            if ($ps_name != 0)
              if (($ps_name[0] == 'S') && ($ps_name[1] == 'F') && ($ps_name[2] == 'X') && ($ps_name[3] == '_'))
                if ($ps_samples > $ps_burst_sfx_samples)
                  set $ps_burst_sfx_samples = $ps_samples
                  set $ps_burst_sfx_id = $__ps_i
                end
              else
                if ((($ps_name[0] != 'U') || ($ps_name[1] != 'I') || ($ps_name[2] != '_')) && ($ps_samples > $ps_burst_non_ui_samples))
                  set $ps_burst_non_ui_samples = $ps_samples
                  set $ps_burst_non_ui_id = $__ps_i
                end
              end
            end
          end
          set $__ps_i = $__ps_i + 1
        end

        if (($ps_burst_sfx_id != 0) && ($ps_burst_sfx_id != $ps_long_id))
          set $ps_burst_id = $ps_burst_sfx_id
          set $ps_burst_samples = $ps_burst_sfx_samples
        else
          if (($ps_burst_non_ui_id != 0) && ($ps_burst_non_ui_id != $ps_long_id))
            set $ps_burst_id = $ps_burst_non_ui_id
            set $ps_burst_samples = $ps_burst_non_ui_samples
          end
        end

        if (($ps_long_id == 0) || ($ps_burst_id == 0))
          printf "overlap needs at least 2 valid embedded clips (long=%lu burst=%lu count=%lu)\n", (unsigned long)$ps_long_id, (unsigned long)$ps_burst_id, (unsigned long)$ps_cnt
        else
          set $ps_stop_rc = (unsigned int)App_AudioReq_Stop()
          printf "queue AUDIO_STOP rc=%u\n", $ps_stop_rc
          printf "long_id=%lu samples=%lu name=%s\n", (unsigned long)$ps_long_id, (unsigned long)$ps_long_samples, AppAudioAssets_Name((uint32_t)$ps_long_id)
          printf "burst_id=%lu samples=%lu name=%s\n", (unsigned long)$ps_burst_id, (unsigned long)$ps_burst_samples, AppAudioAssets_Name((uint32_t)$ps_burst_id)
          printf "perf(start): profile_cur=%lu target=%lu sysclk=%lu\n", (unsigned long)g_power_perf_profile_current, (unsigned long)g_power_perf_profile_target, (unsigned long)HAL_RCC_GetSysClockFreq()

          set $ps_long_rc = (unsigned int)App_AudioReq_PlayAsset((app_audio_asset_id_t)$ps_long_id)
          printf "queue PLAY_ASSET(long=%lu) rc=%u\n", (unsigned long)$ps_long_id, $ps_long_rc
          if ($ps_long_rc == 0)
            set $ps_ov_valid = 1
            set $ps_ov_h0 = (unsigned long)g_audio_half_irq_count
            set $ps_ov_f0 = (unsigned long)g_audio_full_irq_count
            set $ps_ov_u0 = (unsigned long)g_audio_underflow_count
            set $ps_ov_mh0 = (unsigned long)g_audio_half_missed_count
            set $ps_ov_mf0 = (unsigned long)g_audio_full_missed_count
            set $ps_ov_ar0 = (unsigned long)g_th_audio.tx_thread_run_count
            set $ps_ov_dr0 = (unsigned long)g_th_display.tx_thread_run_count
            set $ps_ov_t0 = (unsigned long)HAL_GetTick()
            set $ps_ov_mc0 = (unsigned long)g_audio_music_voice.sample_cursor

            set $__ps_n = 0
            set $__ps_i = 1
            while $__ps_i < $ps_cnt
              set $ps_name = AppAudioAssets_Name((uint32_t)$__ps_i)
              if (($ps_name != 0) && ($__ps_i != $ps_long_id))
                if (($ps_name[0] == 'S') && ($ps_name[1] == 'F') && ($ps_name[2] == 'X') && ($ps_name[3] == '_'))
                  set $ps_rc = (unsigned int)App_AudioReq_PlayAsset((app_audio_asset_id_t)$__ps_i)
                  set $ps_ov_posts = $ps_ov_posts + 1
                  if ($ps_rc != 0)
                    set $ps_ov_drop = $ps_ov_drop + 1
                  end
                  set $__ps_n = $__ps_n + 1
                  if ($__ps_n >= 4)
                    set $__ps_i = $ps_cnt
                  end
                end
              end
              set $__ps_i = $__ps_i + 1
            end

            if ($ps_ov_posts == 0)
              set $__ps_n = 0
              while $__ps_n < 2
                set $ps_rc = (unsigned int)App_AudioReq_PlayAsset((app_audio_asset_id_t)$ps_burst_id)
                set $ps_ov_posts = $ps_ov_posts + 1
                if ($ps_rc != 0)
                  set $ps_ov_drop = $ps_ov_drop + 1
                end
                set $__ps_n = $__ps_n + 1
              end
            end
            printf "queued burst posts=%lu drops=%lu\n", (unsigned long)$ps_ov_posts, (unsigned long)$ps_ov_drop
            printf "music(start): active=%u kind=%u cursor=%lu\n", (unsigned int)g_audio_music_voice.active, (unsigned int)g_audio_music_voice.source_kind, (unsigned long)g_audio_music_voice.sample_cursor
            printf "let target run ~6s, Ctrl-C, then run: ps_audio_overlap_end\n"
          else
            printf "long clip start failed; do not run ps_audio_overlap_end yet.\n"
          end
        end
      end
    end
  end
end

define ps_audio_overlap_end
  if (($ps_ov_valid + 0) == 0)
    printf "overlap mark not set (run ps_audio_overlap_start first)\n"
  else
    printf "== Audio overlap delta ==\n"
    printf "dt_ms=%lu posts=%lu drops=%lu\n", (unsigned long)((unsigned long)HAL_GetTick() - $ps_ov_t0), (unsigned long)$ps_ov_posts, (unsigned long)$ps_ov_drop
    printf "perf: profile_cur=%lu target=%lu sysclk=%lu\n", (unsigned long)g_power_perf_profile_current, (unsigned long)g_power_perf_profile_target, (unsigned long)HAL_RCC_GetSysClockFreq()
    printf "half=%lu full=%lu underrun=%lu\n", (unsigned long)((unsigned long)g_audio_half_irq_count - $ps_ov_h0), (unsigned long)((unsigned long)g_audio_full_irq_count - $ps_ov_f0), (unsigned long)((unsigned long)g_audio_underflow_count - $ps_ov_u0)
    printf "missed_half=%lu missed_full=%lu\n", (unsigned long)((unsigned long)g_audio_half_missed_count - $ps_ov_mh0), (unsigned long)((unsigned long)g_audio_full_missed_count - $ps_ov_mf0)
    printf "music(end): active=%u kind=%u cursor=%lu d_cursor=%lu\n", (unsigned int)g_audio_music_voice.active, (unsigned int)g_audio_music_voice.source_kind, (unsigned long)g_audio_music_voice.sample_cursor, (unsigned long)((unsigned long)g_audio_music_voice.sample_cursor - $ps_ov_mc0)
    printf "thAudio_runs=%lu thDisplay_runs=%lu qAudio=%u\n", (unsigned long)((unsigned long)g_th_audio.tx_thread_run_count - $ps_ov_ar0), (unsigned long)((unsigned long)g_th_display.tx_thread_run_count - $ps_ov_dr0), (unsigned int)g_q_audio_cmd.tx_queue_enqueued
    ps_audio_status
  end
end

define ps_audio_quick_play_music
  printf "== Audio quick play music ==\n"
  set $ps_rc = (unsigned int)App_SysEvent_ModeSet(2)
  printf "queue MODE_SET(2) rc=%u\n", $ps_rc
  set $ps_rc = (unsigned int)App_AudioReq_Stop()
  printf "queue AUDIO_STOP rc=%u\n", $ps_rc
  set $ps_cnt = (unsigned long)AppAudioAssets_Count()
  set $ps_long_id = 0
  set $ps_long_samples = 0
  set $__ps_i = 1
  while $__ps_i < $ps_cnt
    set $ps_clip = AppAudioAssets_GetClip((uint32_t)$__ps_i)
    if ($ps_clip != 0)
      set $ps_samples = (unsigned long)$ps_clip->total_samples
      if ($ps_samples > $ps_long_samples)
        set $ps_long_samples = $ps_samples
        set $ps_long_id = $__ps_i
      end
    end
    set $__ps_i = $__ps_i + 1
  end
  if ($ps_long_id == 0)
    printf "no valid embedded clip found (count=%lu)\n", $ps_cnt
  else
    set $ps_rc = (unsigned int)App_AudioReq_PlayAsset((app_audio_asset_id_t)$ps_long_id)
    printf "queue PLAY_ASSET(long=%lu) rc=%u samples=%lu\n", (unsigned long)$ps_long_id, $ps_rc, (unsigned long)$ps_long_samples
  end
  ps_audio_status
  printf "run ~2s, Ctrl-C, then run: ps_audio_status\n"
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

define ps_topdown_m1_prepare
  printf "== Topdown M1 prepare ==\n"
  ps_mode_verify_static
  ps_storage_scene_assets_install_pet_house
  ps_storage_audio_install_manifest_refs_wait
  ps_storage_pkg_manifest_write_test_wait
  ps_storage_pkg_manifest_load_default_wait
  set $ps_rc = (unsigned int)GamePackage_RequestRuntimeModeById(1)
  printf "queue REQUEST_RUNTIME_MODE_BY_ID(1) rc=%u\n", $ps_rc
  ps_mode_verify_realtime
  ps_rt_manifest_refs
  ps_storage_status
end

define ps_topdown_m1_verify
  printf "== Topdown M1 verify ==\n"
  ps_rt_manifest_refs
  ps_storage_status
  ps_audio_status
  printf "manual check: walk over exit and confirm teleport to target spawn\n"
  printf "manual check: press A on interact object and confirm game-action SFX\n"
end

define ps_topdown_m1_help
  printf "Topdown M1 quick flow:\n"
  printf "  ps_topdown_m1_prepare\n"
  printf "  (includes scene map+tileset install from Assets/maps/build)\n"
  printf "  # walk to interact + exit in REALTIME\n"
  printf "  ps_topdown_m1_verify\n"
end

define ps_topdown_m2_prepare
  printf "== Topdown M2 prepare (cross-map) ==\n"
  ps_mode_verify_static
  ps_storage_scene_assets_install_pet_house
  ps_storage_scene_assets_install_town_map
  ps_storage_audio_install_manifest_refs_wait
  ps_storage_pkg_manifest_write_test_wait
  ps_storage_pkg_manifest_load_default_wait
  set $ps_rc = (unsigned int)GamePackage_RequestRuntimeModeById(1)
  printf "queue REQUEST_RUNTIME_MODE_BY_ID(1) rc=%u\n", $ps_rc
  ps_mode_verify_realtime
  ps_rt_manifest_refs
  ps_storage_status
end

define ps_topdown_m2_verify
  printf "== Topdown M2 verify (cross-map) ==\n"
  ps_rt_manifest_refs
  ps_storage_scene_map_status
  ps_storage_scene_tileset_status
  ps_audio_status
  printf "manual check: walk over exit and confirm map switches (size/tileset counters update)\n"
end

define ps_topdown_mx_prepare
  printf "== Topdown MX prepare (registry maps) ==\n"
  ps_storage_scene_reload_map_helpers
  ps_storage_scene_assets_list_maps
  ps_mode_verify_static
  ps_storage_scene_assets_install_all_maps
  ps_storage_audio_install_manifest_refs_wait
  ps_storage_pkg_manifest_write_test_wait
  ps_storage_pkg_manifest_load_default_wait
  set $ps_rc = (unsigned int)GamePackage_RequestRuntimeModeById(1)
  printf "queue REQUEST_RUNTIME_MODE_BY_ID(1) rc=%u\n", $ps_rc
  ps_mode_verify_realtime
  ps_rt_manifest_refs
  ps_storage_status
end

define ps_topdown_mx_verify
  printf "== Topdown MX verify (registry maps) ==\n"
  ps_rt_manifest_refs
  ps_storage_scene_map_status
  ps_storage_scene_tileset_status
  ps_audio_status
  printf "manual check: walk exits and confirm each transition switches map+tileset counters\n"
end

define ps_hardfault_dump
  echo == hardfault dump ==\n
  info registers

  set $ps_exc_lr = $lr
  set $ps_msp = $msp
  set $ps_psp = $psp
  if (($ps_exc_lr & 4) != 0)
    set $ps_stk = $ps_psp
    echo active_stack=PSP\n
  else
    set $ps_stk = $ps_msp
    echo active_stack=MSP\n
  end

  printf "EXC_RETURN(LR)=0x%08lx stacked_sp=0x%08lx\n", (unsigned long)$ps_exc_lr, (unsigned long)$ps_stk
  x/8wx $ps_stk

  set $ps_r0 = *((unsigned int *)$ps_stk + 0)
  set $ps_r1 = *((unsigned int *)$ps_stk + 1)
  set $ps_r2 = *((unsigned int *)$ps_stk + 2)
  set $ps_r3 = *((unsigned int *)$ps_stk + 3)
  set $ps_r12 = *((unsigned int *)$ps_stk + 4)
  set $ps_lr_stacked = *((unsigned int *)$ps_stk + 5)
  set $ps_pc_stacked = *((unsigned int *)$ps_stk + 6)
  set $ps_xpsr_stacked = *((unsigned int *)$ps_stk + 7)
  printf "stacked: r0=0x%08lx r1=0x%08lx r2=0x%08lx r3=0x%08lx r12=0x%08lx lr=0x%08lx pc=0x%08lx xpsr=0x%08lx\n", (unsigned long)$ps_r0, (unsigned long)$ps_r1, (unsigned long)$ps_r2, (unsigned long)$ps_r3, (unsigned long)$ps_r12, (unsigned long)$ps_lr_stacked, (unsigned long)$ps_pc_stacked, (unsigned long)$ps_xpsr_stacked

  set $ps_cfsr = *(unsigned int *)0xE000ED28
  set $ps_hfsr = *(unsigned int *)0xE000ED2C
  set $ps_mmfar = *(unsigned int *)0xE000ED34
  set $ps_bfar = *(unsigned int *)0xE000ED38
  printf "SCB: CFSR=0x%08lx HFSR=0x%08lx MMFAR=0x%08lx BFAR=0x%08lx\n", (unsigned long)$ps_cfsr, (unsigned long)$ps_hfsr, (unsigned long)$ps_mmfar, (unsigned long)$ps_bfar

  x/i $pc
  bt
end

define ps_hf
  ps_hardfault_dump
end

# Optional strategic points (enable intentionally, keep <= 5 total):
# break App_ThreadX_LowPower_Enter
# break App_ThreadX_LowPower_Exit
# break HAL_PCD_ConnectCallback
