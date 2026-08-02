/* Extracted from app_threadx.c for thread-level organization. */

static VOID AppAudioThreadEntry(ULONG thread_input)
{
  UINT status;
  app_audio_cmd_t cmd;

  (void)thread_input;
  HAL_GPIO_WritePin(SD_MODE_GPIO_Port, SD_MODE_Pin, GPIO_PIN_RESET);
  (void)AppAudioStop();

  for (;;)
  {
    status = tx_queue_receive(&g_q_audio_cmd, &cmd, KNOB_RTOS_AUDIO_WAIT_TICKS);
    if (status == TX_SUCCESS)
    {
      switch ((app_audio_cmd_type_t)cmd.type)
      {
        case APP_AUDIO_CMD_QUIESCE:
          (void)AppAudioStop();
          (void)App_SysEvent_QuiesceAck(APP_POWER_ACK_SRC_AUDIO);
          break;

        case APP_AUDIO_CMD_RESUME:
          /* Resume command intentionally leaves audio stopped until requested. */
          break;

        case APP_AUDIO_CMD_START_TONE:
          (void)AppAudioStartTone();
          break;

        case APP_AUDIO_CMD_STOP:
          (void)AppAudioStop();
          break;

        default:
          break;
      }
    }
    else if (status == TX_QUEUE_EMPTY)
    {
      AppAudioProcessDmaEvents();
    }
    else if (status != TX_QUEUE_EMPTY)
    {
      (void)AppAudioStop();
    }
  }
}
