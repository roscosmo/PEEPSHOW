/* Thread entry implementation for App_ThreadX runtime. */

/* Sensor helpers. */
static UINT AppSensorHealthFlagsWrite(ULONG set_mask, ULONG clear_mask);
static UINT AppSensorHealthFlagsPublish(void);
static VOID AppSensorMarkAllOff(void);
static VOID AppSensorMarkAllSuspended(uint8_t keep_pmic_active);
static VOID AppSensorRunResumeSequence(void);
static VOID AppSensorRunPollSequence(ULONG targets_mask);
static VOID AppSensorApplyDefaults(ULONG targets_mask);
static VOID AppSensorHandleModeChange(app_mode_t mode_token);
static uint8_t AppSensorRecoveryNeeded(void);
static uint8_t AppSensorModeHasAny(ULONG mode_mask);
static uint8_t AppSensorAutoRecoveryAllowed(void);
static void AppSensorJoyCalResetStatus(void);
static void AppSensorJoyCalApplyLoadedIfReady(void);
static void AppSensorJoyCalStart(void);
static void AppSensorJoyCalStep(void);
static void AppSensorJoyCalQualityReset(void);
static void AppSensorJoyCalComputeQuality(const TMAGJoy_Cal *final_cal);
static void AppSensorJoyCalSnapshot(void);
static void AppSensorJoyCalRestoreSnapshot(void);
static void AppSensorJoyCalCancel(void);
static void AppSensorJoyCaptureBegin(uint32_t duration_ms, uint32_t sample_every_ms);
static uint8_t AppSensorJoyCaptureStep(uint32_t now_ms, float *progress_out, float *avg_x_out, float *avg_y_out);
static uint8_t AppSensorJoyCalApplyDirectionalSolve(void);
static uint8_t AppSensorJoyCalDirectionMatchesStage(ULONG stage, float avg_x, float avg_y);
static void AppSensorJoyCalRequestSave(void);
static UINT AppSensorSettingsSaveRequest(void);
static uint8_t AppSensorJoyLiveUpdate(TMAGJoy_Dir dir, ULONG input_mask, uint8_t enabled);
static void AppSensorJoyEnsureAbsDeadzoneConfigured(void);
static VOID AppSensorJoyInputUpdate(uint8_t enabled);
static void AppSensorJoySeedNeutralArm(void);
static ULONG AppSensorJoyDirMask(TMAGJoy_Dir dir);
static VOID AppSensorTmagMarkRuntimeFault(LONG error_code);
static VOID AppSensorResetRecoveryState(app_sensor_fsm_t *dev);
static uint8_t AppSensorRetryDue(ULONG now_ticks, ULONG deadline_ticks);
static VOID AppSensorBusRecoverPulseDelay(void);
static uint8_t AppSensorBusRecover(LONG *error_out);
static uint8_t AppSensorProbeWithRecovery(uint8_t (*probe_fn)(LONG *), LONG *error_out);
static uint8_t AppSensorBusSanityCheck(LONG *error_out);
static VOID AppSensorPmicPolicyRefresh(void);
static VOID AppSensorPmicUpdateBatteryHealth(ULONG vbat_mV, ULONG soc_pct);
static VOID AppSensorPmicRuntimeReset(void);
static VOID AppSensorPmicRecordTransportError(LONG error_code);
static VOID AppSensorPmicRecordFaultMask(uint8_t fault_mask);
static VOID AppSensorPmicHandleIrq(void);
static VOID AppSensorPmicUpdateVbusPresence(uint8_t vbus_present, uint8_t force_post);
static uint8_t AppSensorPmicForceIsofetOff(LONG *error_out);
static uint8_t AppSensorPmicGuardApply(ULONG vbat_mV, LONG *error_out);
static uint8_t AppSensorProbePmic(LONG *error_out);
static uint8_t AppSensorProbeTmag(LONG *error_out);
static uint8_t AppSensorPollPmic(LONG *error_out);
static uint8_t AppSensorPollTmag(LONG *error_out);
static uint8_t AppSensorLisResolveDevice(stmdev_ctx_t *driver_ctx, app_sensor_lis_ctx_t *lis_ctx, uint8_t *whoami_out);
static uint8_t AppSensorLisOdrIsValid(uint8_t odr);
static uint8_t AppSensorLisOdrSupportsBw(uint8_t odr);
static uint8_t AppSensorLisResolveOdrKnob(ULONG knob_value, uint8_t fallback_odr);
static uint8_t AppSensorLisResolveBwKnob(ULONG knob_value, uint8_t fallback_bw);
static uint8_t AppSensorLisResolveFsKnob(ULONG knob_value, uint8_t fallback_fs);
static uint8_t AppSensorLisApplyProfile(const stmdev_ctx_t *driver_ctx, app_sensor_lis_profile_t profile, LONG *error_out);
static uint8_t AppSensorLisApplyStepConfig(const stmdev_ctx_t *driver_ctx, ULONG step_enable, LONG *error_out);
static VOID AppSensorLisMarkRuntimeFault(LONG error_code);
static VOID AppSensorLisApplyRequestedProfileNow(void);
static VOID AppSensorLisResetStepCounterNow(void);
static int32_t AppSensorLisRead(void *ctx, uint8_t reg, uint8_t *data, uint16_t len);
static int32_t AppSensorLisWrite(void *ctx, uint8_t reg, const uint8_t *data, uint16_t len);
static uint8_t AppSensorProbeLis(LONG *error_out);
static uint8_t AppSensorPollLis(LONG *error_out);
static uint8_t AppSensorPollLisStreamFast(LONG *error_out);
static VOID AppSensorLisRefreshStepStatusNow(void);
static VOID AppSensorDeviceInit(app_sensor_fsm_t *dev, uint8_t (*probe_fn)(LONG *));
static VOID AppSensorDevicePoll(app_sensor_fsm_t *dev, uint8_t (*poll_fn)(LONG *), uint8_t (*probe_fn)(LONG *));

static VOID AppSensorThreadEntry(ULONG thread_input)
{
  UINT status;
  app_sensor_req_t req;
  ULONG pmic_next_poll_tick = 0UL;
  ULONG lis_next_poll_tick = 0UL;
  ULONG lis_step_next_poll_tick = 0UL;
  ULONG joy_next_poll_tick = 0UL;

  (void)thread_input;

  AppSensorRunResumeSequence();

  for (;;)
  {
    status = tx_queue_receive(&g_q_sensor_req, &req, KNOB_RTOS_SENSOR_WAIT_TICKS);
    if (status == TX_SUCCESS)
    {
      switch ((app_sensor_req_type_t)req.type)
      {
        case APP_SENSOR_REQ_QUIESCE:
        {
          ULONG mode_flags = (g_eg_mode.tx_event_flags_group_current & APP_MODE_FLAGS_ALL);

          pmic_next_poll_tick = 0UL;
          lis_next_poll_tick = 0UL;
          lis_step_next_poll_tick = 0UL;
          joy_next_poll_tick = 0UL;
          g_sensor_lis_stream_enabled = 0UL;
          if ((g_sensor_joy_cal_active != 0UL) ||
              (g_sensor_joy_cal_wait_confirm != 0UL) ||
              ((g_sensor_joy_cal_stage >= (ULONG)APP_JOY_CAL_STAGE_NEUTRAL) &&
               (g_sensor_joy_cal_stage <= (ULONG)APP_JOY_CAL_STAGE_SWEEP)))
          {
            AppSensorJoyCalRestoreSnapshot();
          }
          g_sensor_joy_cal_active = 0UL;
          g_sensor_joy_cal_capture.active = 0U;
          g_sensor_joy_cal_stage = (ULONG)APP_JOY_CAL_STAGE_IDLE;
          g_sensor_joy_cal_wait_confirm = 0UL;
          g_sensor_joy_cal_status.stage = (ULONG)APP_JOY_CAL_STAGE_IDLE;
          g_sensor_joy_cal_status.progress = 0.0f;
          g_sensor_joy_cal_status.save_pending = 0UL;
          AppSensorJoyInputUpdate(0U);
          AppSensorSuspendHardwareForStop();
          AppSensorMarkAllSuspended(((mode_flags & APP_MODE_FLAG_FLASHING) != 0UL) ? 1U : 0U);
          (void)AppSensorHealthFlagsPublish();
          (void)App_SysEvent_QuiesceAck(APP_POWER_ACK_SRC_SENSOR);
          break;
        }

        case APP_SENSOR_REQ_RESUME:
          pmic_next_poll_tick = 0UL;
          lis_next_poll_tick = 0UL;
          lis_step_next_poll_tick = 0UL;
          joy_next_poll_tick = 0UL;
          if (AppSensorModeHasAny(APP_MODE_FLAG_FLASHING) != 0U)
          {
            AppSensorJoyInputUpdate(0U);
            AppSensorMarkAllSuspended(1U);
            (void)AppSensorHealthFlagsPublish();
          }
          else
          {
            AppSensorRunResumeSequence();
            AppSensorJoyInputUpdate((g_sensor_joy_input_gate_valid != 0UL) ? 1U : 0U);
          }
          break;

        case APP_SENSOR_REQ_POLL:
          if (AppSensorModeHasAny(APP_MODE_FLAG_FLASHING) != 0U)
          {
            (void)AppSensorHealthFlagsPublish();
          }
          else if (AppSensorModeHasAny(APP_MODE_FLAG_REALTIME) != 0U)
          {
            ULONG lis_only_targets = req.arg0 & (ULONG)APP_SENSOR_TARGET_LIS;
            if (lis_only_targets != 0UL)
            {
              AppSensorRunPollSequence(lis_only_targets);
            }
            else
            {
              (void)AppSensorHealthFlagsPublish();
            }
          }
          else
          {
            AppSensorRunPollSequence(req.arg0);
          }
          break;

        case APP_SENSOR_REQ_CONFIG_DEFAULTS:
          if (AppSensorModeHasAny(APP_MODE_FLAG_REALTIME | APP_MODE_FLAG_FLASHING) == 0U)
          {
            AppSensorApplyDefaults(req.arg0);
          }
          else
          {
            (void)AppSensorHealthFlagsPublish();
          }
          break;

        case APP_SENSOR_REQ_HEALTH_SNAPSHOT:
          (void)AppSensorHealthFlagsPublish();
          break;

        case APP_SENSOR_REQ_MODE_CHANGED:
          pmic_next_poll_tick = 0UL;
          lis_next_poll_tick = 0UL;
          lis_step_next_poll_tick = 0UL;
          joy_next_poll_tick = 0UL;
          AppSensorHandleModeChange((app_mode_t)req.arg0);
          break;

        case APP_SENSOR_REQ_PMIC_IRQ:
          pmic_next_poll_tick = 0UL;
          if ((g_sensor_pmic.state == (ULONG)APP_SENSOR_STATE_READY) ||
              (g_sensor_pmic.state == (ULONG)APP_SENSOR_STATE_SUSPENDED))
          {
            AppSensorPmicHandleIrq();
          }
          else
          {
            AppSensorRunPollSequence(APP_SENSOR_TARGET_PMIC);
          }
          break;

        case APP_SENSOR_REQ_LIS_SET_PROFILE:
          {
            if (req.arg0 > (ULONG)APP_SENSOR_LIS_PROFILE_LIVE)
            {
              g_sensor_lis.last_error = -312L;
              break;
            }

            g_sensor_lis_profile_requested = (app_sensor_lis_profile_t)req.arg0;
            if (AppSensorModeHasAny(APP_MODE_FLAG_FLASHING) != 0U)
            {
              g_sensor_lis.last_error = -505L;
              break;
            }

            if (g_sensor_lis.state != (ULONG)APP_SENSOR_STATE_READY)
            {
              AppSensorRunPollSequence(APP_SENSOR_TARGET_LIS);
              break;
            }

            AppSensorLisApplyRequestedProfileNow();
          }
          break;

        case APP_SENSOR_REQ_LIS_STREAM_START:
          {
            if (AppSensorModeHasAny(APP_MODE_FLAG_FLASHING) != 0U)
            {
              g_sensor_lis_stream_enabled = 0UL;
              g_sensor_lis.last_error = -505L;
              break;
            }

            if (AppSensorModeHasAny(APP_MODE_FLAG_STATIC | APP_MODE_FLAG_REALTIME) == 0U)
            {
              g_sensor_lis_stream_enabled = 0UL;
              g_sensor_lis.last_error = -504L;
              break;
            }

            g_sensor_lis_stream_enabled = 1UL;
            lis_next_poll_tick = 0UL;
            lis_step_next_poll_tick = 0UL;
            if (g_sensor_lis.state != (ULONG)APP_SENSOR_STATE_READY)
            {
              AppSensorRunPollSequence(APP_SENSOR_TARGET_LIS);
              break;
            }

            AppSensorLisApplyRequestedProfileNow();
            AppSensorDevicePoll(&g_sensor_lis, AppSensorPollLisStreamFast, AppSensorProbeLis);
            if (g_sensor_lis.state == (ULONG)APP_SENSOR_STATE_READY)
            {
              g_sensor_bus_fault = 0UL;
            }
            else
            {
              (void)AppSensorHealthFlagsPublish();
            }
          }
          break;

        case APP_SENSOR_REQ_LIS_STREAM_STOP:
          g_sensor_lis_stream_enabled = 0UL;
          lis_next_poll_tick = 0UL;
          lis_step_next_poll_tick = 0UL;
          break;

        case APP_SENSOR_REQ_LIS_STEP_ENABLE:
          if (AppSensorModeHasAny(APP_MODE_FLAG_FLASHING) != 0U)
          {
            g_sensor_lis.last_error = -505L;
            break;
          }

          g_sensor_lis_step_enabled_requested = 1UL;
          if (g_sensor_lis.state != (ULONG)APP_SENSOR_STATE_READY)
          {
            AppSensorRunPollSequence(APP_SENSOR_TARGET_LIS);
          }
          AppSensorLisApplyRequestedProfileNow();
          AppSensorRunPollSequence(APP_SENSOR_TARGET_LIS);
          break;

        case APP_SENSOR_REQ_LIS_STEP_DISABLE:
          if (AppSensorModeHasAny(APP_MODE_FLAG_FLASHING) != 0U)
          {
            g_sensor_lis.last_error = -505L;
            break;
          }

          g_sensor_lis_step_enabled_requested = 0UL;
          if (g_sensor_lis.state != (ULONG)APP_SENSOR_STATE_READY)
          {
            AppSensorRunPollSequence(APP_SENSOR_TARGET_LIS);
          }
          AppSensorLisApplyRequestedProfileNow();
          AppSensorRunPollSequence(APP_SENSOR_TARGET_LIS);
          break;

        case APP_SENSOR_REQ_LIS_STEP_RESET:
          if (AppSensorModeHasAny(APP_MODE_FLAG_FLASHING) != 0U)
          {
            g_sensor_lis.last_error = -505L;
            break;
          }

          if (g_sensor_lis.state != (ULONG)APP_SENSOR_STATE_READY)
          {
            AppSensorRunPollSequence(APP_SENSOR_TARGET_LIS);
          }
          AppSensorLisResetStepCounterNow();
          AppSensorRunPollSequence(APP_SENSOR_TARGET_LIS);
          break;

        case APP_SENSOR_REQ_JOY_CAL_START:
          if (AppSensorModeHasAny(APP_MODE_FLAG_REALTIME | APP_MODE_FLAG_FLASHING) == 0U)
          {
            if ((g_sensor_joy_cal_active != 0UL) &&
                (g_sensor_joy_cal_wait_confirm != 0UL) &&
                (g_sensor_joy_cal_stage >= (ULONG)APP_JOY_CAL_STAGE_UP) &&
                (g_sensor_joy_cal_stage <= (ULONG)APP_JOY_CAL_STAGE_LEFT))
            {
              uint32_t capture_ms = (uint32_t)KNOB_SENSOR_JOY_CAL_DIRECTION_WINDOW_MS;
              if (capture_ms < 320U)
              {
                capture_ms = 320U;
              }
              if (capture_ms > 2000U)
              {
                capture_ms = 2000U;
              }
              AppSensorJoyCaptureBegin(capture_ms,
                                       (uint32_t)KNOB_SENSOR_JOY_CAL_DIRECTION_STEP_MS);
              g_sensor_joy_cal_wait_confirm = 0UL;
              g_sensor_joy_cal_status.progress = 0.0f;
              g_sensor_joy_cal_status.last_error = 0L;
            }
            else if ((g_sensor_joy_cal_active == 0UL) ||
                     (g_sensor_joy_cal_stage == (ULONG)APP_JOY_CAL_STAGE_IDLE) ||
                     (g_sensor_joy_cal_stage == (ULONG)APP_JOY_CAL_STAGE_DONE) ||
                     (g_sensor_joy_cal_stage == (ULONG)APP_JOY_CAL_STAGE_ERROR))
            {
              AppSensorJoyCalStart();
            }
          }
          else
          {
            g_sensor_joy_cal_status.last_error = -504L;
          }
          break;

        case APP_SENSOR_REQ_JOY_CAL_SAVE:
          if (AppSensorModeHasAny(APP_MODE_FLAG_FLASHING) == 0U)
          {
            AppSensorJoyCalRequestSave();
          }
          else
          {
            g_sensor_joy_cal_status.last_error = -505L;
          }
          break;

        case APP_SENSOR_REQ_JOY_CAL_CANCEL:
          if (AppSensorModeHasAny(APP_MODE_FLAG_FLASHING) == 0U)
          {
            AppSensorJoyCalCancel();
          }
          else
          {
            g_sensor_joy_cal_status.last_error = -505L;
          }
          break;

        case APP_SENSOR_REQ_JOY_DEADZONE_SET:
          if (AppSensorModeHasAny(APP_MODE_FLAG_FLASHING) == 0U)
          {
            if (g_sensor_joy != TX_NULL)
            {
              uint8_t deadzone_en = 0U;
              float deadzone_live_mT = 0.0f;
              float deadzone_mT = AppStorageDeadzoneClampMt(((float)req.arg0) / 10.0f);
              TMAGJoy_SetAbsDeadzone(g_sensor_joy, 1U, deadzone_mT);
              g_storage_joycfg_deadzone_enabled = 1UL;
              g_storage_joycfg_deadzone_mT = deadzone_mT;
              TMAGJoy_GetAbsDeadzone(g_sensor_joy, &deadzone_en, &deadzone_live_mT);
              g_sensor_joy_live_status.deadzone_enabled = (ULONG)deadzone_en;
              g_sensor_joy_live_status.deadzone_mT = deadzone_live_mT;
            }
            else
            {
              g_sensor_joy_cal_status.last_error = -502L;
            }
          }
          else
          {
            g_sensor_joy_cal_status.last_error = -505L;
          }
          break;

        case APP_SENSOR_REQ_SETTINGS_SAVE:
          if (AppSensorModeHasAny(APP_MODE_FLAG_FLASHING) == 0U)
          {
            if (AppSensorSettingsSaveRequest() != TX_SUCCESS)
            {
              if (g_sensor_joy_cal_status.last_error == 0L)
              {
                g_sensor_joy_cal_status.last_error = -503L;
              }
            }
          }
          else
          {
            g_sensor_joy_cal_status.last_error = -505L;
          }
          break;

        default:
          break;
      }
    }
    if ((status == TX_QUEUE_EMPTY) || (status == TX_SUCCESS))
    {
      ULONG now_ms = (ULONG)HAL_GetTick();

      {
        ULONG poll_period_ms = (AppSensorModeHasAny(APP_MODE_FLAG_STOP) != 0U) ?
                               (ULONG)KNOB_SENSOR_PMIC_STOP_POLL_PERIOD_MS :
                               (ULONG)KNOB_SENSOR_PMIC_POLL_PERIOD_MS;

        if (poll_period_ms == 0UL)
        {
          poll_period_ms = 1UL;
        }

        if ((pmic_next_poll_tick == 0UL) ||
            (((LONG)(now_ms - pmic_next_poll_tick)) >= 0L))
        {
          AppSensorDevicePoll(&g_sensor_pmic, AppSensorPollPmic, AppSensorProbePmic);
          if (g_sensor_pmic.state == (ULONG)APP_SENSOR_STATE_READY)
          {
            g_sensor_bus_fault = 0UL;
          }
          (void)AppSensorHealthFlagsPublish();
          pmic_next_poll_tick = now_ms + poll_period_ms;
        }
      }

      if ((g_sensor_lis_stream_enabled != 0UL) &&
          (AppSensorModeHasAny(APP_MODE_FLAG_FLASHING) == 0U) &&
          (AppSensorModeHasAny(APP_MODE_FLAG_STATIC | APP_MODE_FLAG_REALTIME) != 0U))
      {
        ULONG lis_poll_period_ms = (ULONG)KNOB_SENSOR_LIS_STREAM_POLL_PERIOD_MS;

        if (lis_poll_period_ms == 0UL)
        {
          lis_poll_period_ms = 1UL;
        }

        if ((lis_next_poll_tick == 0UL) ||
            (((LONG)(now_ms - lis_next_poll_tick)) >= 0L))
        {
          ULONG lis_prev_state = g_sensor_lis.state;
          AppSensorDevicePoll(&g_sensor_lis, AppSensorPollLisStreamFast, AppSensorProbeLis);
          if (g_sensor_lis.state == (ULONG)APP_SENSOR_STATE_READY)
          {
            g_sensor_bus_fault = 0UL;
            if (lis_prev_state != (ULONG)APP_SENSOR_STATE_READY)
            {
              (void)AppSensorHealthFlagsPublish();
            }
          }
          else
          {
            (void)AppSensorHealthFlagsPublish();
          }
          lis_next_poll_tick = now_ms + lis_poll_period_ms;
        }

        if ((g_sensor_lis.state == (ULONG)APP_SENSOR_STATE_READY) &&
            ((g_sensor_lis_step_enabled_requested != 0UL) ||
             (g_sensor_lis_live.step_enabled != 0UL)))
        {
          ULONG lis_step_period_ms = (ULONG)KNOB_SENSOR_LIS_STEP_STATUS_POLL_PERIOD_MS;

          if (lis_step_period_ms == 0UL)
          {
            lis_step_period_ms = 1UL;
          }

          if ((lis_step_next_poll_tick == 0UL) ||
              (((LONG)(now_ms - lis_step_next_poll_tick)) >= 0L))
          {
            AppSensorLisRefreshStepStatusNow();
            lis_step_next_poll_tick = now_ms + lis_step_period_ms;
          }
        }
        else
        {
          lis_step_next_poll_tick = 0UL;
        }
      }
      else
      {
        lis_next_poll_tick = 0UL;
        lis_step_next_poll_tick = 0UL;
      }

      AppSensorJoyCalApplyLoadedIfReady();
      if (AppSensorModeHasAny(APP_MODE_FLAG_REALTIME | APP_MODE_FLAG_FLASHING) == 0U)
      {
        AppSensorJoyCalStep();
      }
      else if (g_sensor_joy_cal_active != 0UL)
      {
        g_sensor_joy_cal_active = 0UL;
        g_sensor_joy_cal_capture.active = 0U;
        g_sensor_joy_cal_stage = (ULONG)APP_JOY_CAL_STAGE_IDLE;
        g_sensor_joy_cal_wait_confirm = 0UL;
        g_sensor_joy_cal_status.stage = (ULONG)APP_JOY_CAL_STAGE_IDLE;
      }

      if ((AppSensorModeHasAny(APP_MODE_FLAG_FLASHING) == 0U) &&
          (AppSensorModeHasAny(APP_MODE_FLAG_STATIC | APP_MODE_FLAG_REALTIME) != 0U) &&
          (g_sensor_joy_cal_active == 0UL) &&
          (g_sensor_joy_input_gate_valid != 0UL))
      {
        ULONG joy_poll_period_ms = (ULONG)KNOB_SENSOR_JOY_POLL_PERIOD_MS;

        if (joy_poll_period_ms == 0UL)
        {
          joy_poll_period_ms = 1UL;
        }

        if ((joy_next_poll_tick == 0UL) ||
            (((LONG)(now_ms - joy_next_poll_tick)) >= 0L))
        {
          AppSensorJoyInputUpdate(1U);
          joy_next_poll_tick = now_ms + joy_poll_period_ms;
        }
      }
      else
      {
        joy_next_poll_tick = 0UL;
        AppSensorJoyInputUpdate(0U);
      }

      if ((AppSensorAutoRecoveryAllowed() != 0U) && (AppSensorRecoveryNeeded() != 0U))
      {
        AppSensorRunPollSequence(APP_SENSOR_TARGET_MASK_ALL);
      }
    }
    else if (status != TX_QUEUE_EMPTY)
    {
      /* Queue error in sensor thread: continue processing future requests. */
    }
  }
}
