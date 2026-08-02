/* Extracted from app_threadx.c for thread-level organization. */

static VOID AppStorageThreadEntry(ULONG thread_input)
{
  UINT status;
  app_storage_req_t req;

  (void)thread_input;
  (void)AppStorageRunJoyCfgLoad();

  for (;;)
  {
    status = tx_queue_receive(&g_q_storage_req, &req, KNOB_RTOS_STORAGE_WAIT_TICKS);
    if (status == TX_SUCCESS)
    {
      switch ((app_storage_req_type_t)req.type)
      {
        case APP_STORAGE_REQ_QUIESCE:
          (void)App_SysEvent_QuiesceAck(APP_POWER_ACK_SRC_STORAGE);
          break;

        case APP_STORAGE_REQ_RESUME:
          (void)AppStorageRunJoyCfgLoad();
          break;

        case APP_STORAGE_REQ_FLASH_PROBE:
          (void)AppStorageRunFlashProbe();
          break;

        case APP_STORAGE_REQ_RAW_SMOKE:
          (void)AppStorageRunRawSmoke();
          break;

        case APP_STORAGE_REQ_FILEX_MOUNT:
          (void)AppStorageRunFileXMount();
          break;

        case APP_STORAGE_REQ_FILEX_FORMAT:
          (void)AppStorageRunFileXFormat();
          break;

        case APP_STORAGE_REQ_FILEX_UNMOUNT:
          (void)AppStorageRunFileXUnmount();
          break;

        case APP_STORAGE_REQ_JOYCFG_LOAD:
          (void)AppStorageRunJoyCfgLoad();
          break;

        case APP_STORAGE_REQ_JOYCFG_SAVE:
          (void)AppStorageRunJoyCfgSave();
          break;

        default:
          break;
      }
    }
    else if (status != TX_QUEUE_EMPTY)
    {
      /* Queue error in storage stub: continue processing future requests. */
    }
  }
}
