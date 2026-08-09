/* Thread entry implementation for App_ThreadX runtime. */

/* Storage helpers. */
static VOID AppStorageCaptureDebug(void);
static LONG AppStorageHalToError(HAL_StatusTypeDef hal_status);
static UINT AppStorageRunFlashProbe(void);
static UINT AppStorageRunRawSmoke(void);
static UINT AppStorageRunAudioCatalogLoad(uint32_t catalog_addr);
static UINT AppStorageRunAudioChunkRead(uint32_t addr, uint32_t len, uint32_t token);
static UINT AppStorageRunAudioCatalogInstallEmbedded(void);
static UINT AppStorageRunAudioCatalogInstallManifestRefs(void);
static UINT AppStorageRunGamePackageManifestLoad(uint32_t manifest_addr, uint32_t manifest_size);
static UINT AppStorageRunGamePackageManifestLoadDefault(void);
static UINT AppStorageRunGamePackageManifestErase(void);
static UINT AppStorageRunGamePackageManifestImportFat(void);
static UINT AppStorageRunGamePackageSceneImportFat(void);
static UINT AppStorageRunRawAppErase(void);
static UINT AppStorageRunGamePackageManifestWriteTest(void);
static UINT AppStorageRunSceneMapLoad(uint32_t map_addr, uint32_t map_size);
static UINT AppStorageRunSceneTilesetLoad(uint32_t tileset_addr, uint32_t tileset_size);
static VOID AppStorageAudioChunkCacheReset(void);
static VOID AppStorageAudioChunkCachePublish(ULONG token, ULONG addr, ULONG len, ULONG status, ULONG crc32, const uint8_t *data_ptr);
static UINT AppStorageAudioChunkCacheConsume(ULONG token, ULONG addr, ULONG len, uint8_t *dst_ptr);
static ULONG AppStorageFatTotalSectors(void);
static UINT AppStorageRunFileXMount(void);
static UINT AppStorageRunFileXFormat(void);
static UINT AppStorageRunFileXUnmount(void);
static UINT AppStorageRunUsbMscRead(uint32_t lba, uint32_t number_blocks, uint8_t *data_pointer, ULONG *media_status_out);
static UINT AppStorageRunUsbMscWrite(uint32_t lba, uint32_t number_blocks, const uint8_t *data_pointer, ULONG *media_status_out);
static UINT AppStorageRunUsbMscFlush(ULONG *media_status_out);
static UINT AppStorageRunUsbMscStatus(ULONG *media_status_out);
static uint32_t AppStorageCrc32(const uint8_t *data, uint32_t len);
static UINT AppStorageRunJoyCfgLoad(void);
static UINT AppStorageRunJoyCfgSave(void);
static UINT AppStorageRunFlashQuiesce(void);
static UINT AppStorageRunFlashResume(void);
static UINT AppStorageReqPostEx(app_storage_req_type_t req_type, ULONG arg0, ULONG arg1, ULONG arg2);
static UINT AppStorageReqPost(app_storage_req_type_t req_type, ULONG arg0);
static uint8_t AppStorageReqAllowedInFlashing(app_storage_req_type_t req_type);

static uint8_t AppStorageReqAllowedInFlashing(app_storage_req_type_t req_type)
{
  switch (req_type)
  {
    case APP_STORAGE_REQ_QUIESCE:
    case APP_STORAGE_REQ_FILEX_MOUNT:
    case APP_STORAGE_REQ_FILEX_FORMAT:
    case APP_STORAGE_REQ_FILEX_UNMOUNT:
    case APP_STORAGE_REQ_USB_MSC_READ:
    case APP_STORAGE_REQ_USB_MSC_WRITE:
    case APP_STORAGE_REQ_USB_MSC_FLUSH:
    case APP_STORAGE_REQ_USB_MSC_STATUS:
      return 1U;
    default:
      break;
  }

  return 0U;
}

static VOID AppStorageThreadEntry(ULONG thread_input)
{
  UINT status;
  ULONG mode_flags;
  app_storage_req_t req;

  (void)thread_input;
  (void)AppStorageRunJoyCfgLoad();
  (void)AppStorageRunGamePackageManifestLoadDefault();
  (void)AppStorageRunAudioCatalogLoad((uint32_t)KNOB_STORAGE_AUDIO_CATALOG_ADDR);

  for (;;)
  {
    status = tx_queue_receive(&g_q_storage_req, &req, KNOB_RTOS_STORAGE_WAIT_TICKS);
    if (status == TX_SUCCESS)
    {
      mode_flags = (g_eg_mode.tx_event_flags_group_current & APP_MODE_FLAGS_ALL);
      if (((mode_flags & APP_MODE_FLAG_FLASHING) != 0UL) &&
          (AppStorageReqAllowedInFlashing((app_storage_req_type_t)req.type) == 0U))
      {
        continue;
      }

      switch ((app_storage_req_type_t)req.type)
      {
        case APP_STORAGE_REQ_QUIESCE:
          (void)AppStorageRunFlashQuiesce();
          (void)App_SysEvent_QuiesceAck(APP_POWER_ACK_SRC_STORAGE);
          break;

        case APP_STORAGE_REQ_RESUME:
          (void)AppStorageRunFlashResume();
          (void)AppStorageRunJoyCfgLoad();
          (void)AppStorageRunGamePackageManifestLoadDefault();
          (void)AppStorageRunAudioCatalogLoad((uint32_t)KNOB_STORAGE_AUDIO_CATALOG_ADDR);
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

        case APP_STORAGE_REQ_AUDIO_CATALOG_LOAD:
          (void)AppStorageRunAudioCatalogLoad((uint32_t)req.arg0);
          break;

        case APP_STORAGE_REQ_AUDIO_CHUNK_READ:
          (void)AppStorageRunAudioChunkRead((uint32_t)req.arg0, (uint32_t)req.arg1, (uint32_t)req.arg2);
          break;

        case APP_STORAGE_REQ_AUDIO_CATALOG_INSTALL_EMBEDDED:
          (void)AppStorageRunAudioCatalogInstallEmbedded();
          break;

        case APP_STORAGE_REQ_AUDIO_CATALOG_INSTALL_MANIFEST_REFS:
          (void)AppStorageRunAudioCatalogInstallManifestRefs();
          break;

        case APP_STORAGE_REQ_GAME_PACKAGE_MANIFEST_LOAD:
          (void)AppStorageRunGamePackageManifestLoad((uint32_t)req.arg0, (uint32_t)req.arg1);
          break;

        case APP_STORAGE_REQ_GAME_PACKAGE_MANIFEST_LOAD_DEFAULT:
          (void)AppStorageRunGamePackageManifestLoadDefault();
          break;

        case APP_STORAGE_REQ_GAME_PACKAGE_MANIFEST_ERASE:
          (void)AppStorageRunGamePackageManifestErase();
          break;

        case APP_STORAGE_REQ_GAME_PACKAGE_MANIFEST_IMPORT_FAT:
          (void)AppStorageRunGamePackageManifestImportFat();
          break;

        case APP_STORAGE_REQ_GAME_PACKAGE_SCENE_IMPORT_FAT:
          (void)AppStorageRunGamePackageSceneImportFat();
          break;

        case APP_STORAGE_REQ_RAW_APP_ERASE:
          (void)AppStorageRunRawAppErase();
          break;

        case APP_STORAGE_REQ_GAME_PACKAGE_MANIFEST_WRITE_TEST:
          (void)AppStorageRunGamePackageManifestWriteTest();
          break;

        case APP_STORAGE_REQ_SCENE_MAP_LOAD:
          (void)AppStorageRunSceneMapLoad((uint32_t)req.arg0, (uint32_t)req.arg1);
          break;

        case APP_STORAGE_REQ_SCENE_TILESET_LOAD:
          (void)AppStorageRunSceneTilesetLoad((uint32_t)req.arg0, (uint32_t)req.arg1);
          break;

        case APP_STORAGE_REQ_USB_MSC_READ:
        {
          ULONG media_status = 1UL;
          UINT req_status = AppStorageRunUsbMscRead((uint32_t)req.arg1,
                                                    (uint32_t)req.arg2,
                                                    (uint8_t *)(uintptr_t)req.arg0,
                                                    &media_status);
          AppStorageUsbMscSignal(req_status, media_status);
          break;
        }

        case APP_STORAGE_REQ_USB_MSC_WRITE:
        {
          ULONG media_status = 1UL;
          UINT req_status = AppStorageRunUsbMscWrite((uint32_t)req.arg1,
                                                     (uint32_t)req.arg2,
                                                     (const uint8_t *)(uintptr_t)req.arg0,
                                                     &media_status);
          AppStorageUsbMscSignal(req_status, media_status);
          break;
        }

        case APP_STORAGE_REQ_USB_MSC_FLUSH:
        {
          ULONG media_status = 1UL;
          UINT req_status = AppStorageRunUsbMscFlush(&media_status);
          AppStorageUsbMscSignal(req_status, media_status);
          break;
        }

        case APP_STORAGE_REQ_USB_MSC_STATUS:
        {
          ULONG media_status = 1UL;
          UINT req_status = AppStorageRunUsbMscStatus(&media_status);
          AppStorageUsbMscSignal(req_status, media_status);
          break;
        }

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
