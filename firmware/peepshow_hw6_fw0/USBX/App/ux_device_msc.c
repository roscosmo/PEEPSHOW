/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    ux_device_msc.c
  * @author  MCD Application Team
  * @brief   USBX Device applicative file
  ******************************************************************************
   * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "ux_device_msc.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "ps_storage_msc_bridge.h"
#include "ux_device_class_storage.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
volatile ULONG g_usbd_storage_read_entry_count = 0UL;
volatile ULONG g_usbd_storage_write_entry_count = 0UL;
volatile ULONG g_usbd_storage_flush_entry_count = 0UL;
volatile ULONG g_usbd_storage_status_entry_count = 0UL;
volatile ULONG g_usbd_storage_read_last_lba = 0UL;
volatile ULONG g_usbd_storage_read_last_blocks = 0UL;
volatile ULONG g_usbd_storage_read_last_status = 0UL;
volatile ULONG g_usbd_storage_read_last_media_status = 0UL;
volatile ULONG g_usbd_storage_write_last_lba = 0UL;
volatile ULONG g_usbd_storage_write_last_blocks = 0UL;
volatile ULONG g_usbd_storage_write_last_status = 0UL;
volatile ULONG g_usbd_storage_write_last_media_status = 0UL;
volatile ULONG g_usbd_storage_flush_last_lba = 0UL;
volatile ULONG g_usbd_storage_flush_last_blocks = 0UL;
volatile ULONG g_usbd_storage_flush_last_status = 0UL;
volatile ULONG g_usbd_storage_flush_last_media_status = 0UL;
volatile ULONG g_usbd_storage_status_last_status = 0UL;
volatile ULONG g_usbd_storage_status_last_media_status = 0UL;

static ULONG PS_UsbMscSenseNotReadyNoMedium(void)
{
  return UX_DEVICE_CLASS_STORAGE_SENSE_STATUS(
    UX_SLAVE_CLASS_STORAGE_SENSE_KEY_NOT_READY,
    0x3AU,
    0x00U);
}

/* USER CODE END 0 */

/**
  * @brief  USBD_STORAGE_Activate
  *         This function is called when insertion of a storage device.
  * @param  storage_instance: Pointer to the storage class instance.
  * @retval none
  */
VOID USBD_STORAGE_Activate(VOID *storage_instance)
{
  /* USER CODE BEGIN USBD_STORAGE_Activate */
  UX_PARAMETER_NOT_USED(storage_instance);
  PS_StorageMscBridge_MarkActivated();
  /* USER CODE END USBD_STORAGE_Activate */

  return;
}

/**
  * @brief  USBD_STORAGE_Deactivate
  *         This function is called when extraction of a storage device.
  * @param  storage_instance: Pointer to the storage class instance.
  * @retval none
  */
VOID USBD_STORAGE_Deactivate(VOID *storage_instance)
{
  /* USER CODE BEGIN USBD_STORAGE_Deactivate  */
  UX_PARAMETER_NOT_USED(storage_instance);
  PS_StorageMscBridge_MarkDeactivated();
  /* USER CODE END USBD_STORAGE_Deactivate */

  return;
}

/**
  * @brief  USBD_STORAGE_Read
  *         This function is invoked to read from media.
  * @param  storage_instance : Pointer to the storage class instance.
  * @param  lun: Logical unit number is the command is directed to.
  * @param  data_pointer: Address of the buffer to be used for reading or writing.
  * @param  number_blocks: number of sectors to read/write.
  * @param  lba: Logical block address is the sector address to read.
  * @param  media_status: should be filled out exactly like the media status
  *                       callback return value.
  * @retval status
  */
UINT USBD_STORAGE_Read(VOID *storage_instance, ULONG lun, UCHAR *data_pointer,
                       ULONG number_blocks, ULONG lba, ULONG *media_status)
{
  UINT status = UX_SUCCESS;

  /* USER CODE BEGIN USBD_STORAGE_Read */
  ULONG media_status_local = 1UL;
  g_usbd_storage_read_entry_count++;
  g_usbd_storage_read_last_lba = lba;
  g_usbd_storage_read_last_blocks = number_blocks;
  UX_PARAMETER_NOT_USED(storage_instance);
  UX_PARAMETER_NOT_USED(lun);
  if (media_status == UX_NULL)
  {
    media_status = &media_status_local;
  }

  status = PS_StorageMscBridge_Submit(PS_STORAGE_MSC_COMMAND_READ,
                                      data_pointer,
                                      (uint32_t)lba,
                                      (uint32_t)number_blocks,
                                      media_status);
  g_usbd_storage_read_last_status = status;
  g_usbd_storage_read_last_media_status = *media_status;
  if (status != TX_SUCCESS)
  {
    *media_status = PS_UsbMscSenseNotReadyNoMedium();
    return UX_ERROR;
  }
  /* USER CODE END USBD_STORAGE_Read */

  return status;
}

/**
  * @brief  USBD_STORAGE_Write
  *         This function is invoked to write in media.
  * @param  storage_instance : Pointer to the storage class instance.
  * @param  lun: Logical unit number is the command is directed to.
  * @param  data_pointer: Address of the buffer to be used for reading or writing.
  * @param  number_blocks: number of sectors to read/write.
  * @param  lba: Logical block address is the sector address to read.
  * @param  media_status: should be filled out exactly like the media status
  *                       callback return value.
  * @retval status
  */
UINT USBD_STORAGE_Write(VOID *storage_instance, ULONG lun, UCHAR *data_pointer,
                        ULONG number_blocks, ULONG lba, ULONG *media_status)
{
  UINT status = UX_SUCCESS;

  /* USER CODE BEGIN USBD_STORAGE_Write */
  ULONG media_status_local = 1UL;
  g_usbd_storage_write_entry_count++;
  g_usbd_storage_write_last_lba = lba;
  g_usbd_storage_write_last_blocks = number_blocks;
  UX_PARAMETER_NOT_USED(storage_instance);
  UX_PARAMETER_NOT_USED(lun);
  if (media_status == UX_NULL)
  {
    media_status = &media_status_local;
  }

  status = PS_StorageMscBridge_Submit(PS_STORAGE_MSC_COMMAND_WRITE,
                                      data_pointer,
                                      (uint32_t)lba,
                                      (uint32_t)number_blocks,
                                      media_status);
  g_usbd_storage_write_last_status = status;
  g_usbd_storage_write_last_media_status = *media_status;
  if (status != TX_SUCCESS)
  {
    *media_status = PS_UsbMscSenseNotReadyNoMedium();
    return UX_ERROR;
  }
  /* USER CODE END USBD_STORAGE_Write */

  return status;
}

/**
  * @brief  USBD_STORAGE_Flush
  *         This function is invoked to flush media.
  * @param  storage_instance : Pointer to the storage class instance.
  * @param  lun: Logical unit number is the command is directed to.
  * @param  number_blocks: number of sectors to read/write.
  * @param  lba: Logical block address is the sector address to read.
  * @param  media_status: should be filled out exactly like the media status
  *                       callback return value.
  * @retval status
  */
UINT USBD_STORAGE_Flush(VOID *storage_instance, ULONG lun, ULONG number_blocks,
                        ULONG lba, ULONG *media_status)
{
  UINT status = UX_SUCCESS;

  /* USER CODE BEGIN USBD_STORAGE_Flush */
  ULONG media_status_local = 1UL;
  g_usbd_storage_flush_entry_count++;
  g_usbd_storage_flush_last_lba = lba;
  g_usbd_storage_flush_last_blocks = number_blocks;
  UX_PARAMETER_NOT_USED(storage_instance);
  UX_PARAMETER_NOT_USED(lun);
  if (media_status == UX_NULL)
  {
    media_status = &media_status_local;
  }

  status = PS_StorageMscBridge_Submit(PS_STORAGE_MSC_COMMAND_FLUSH,
                                      UX_NULL,
                                      (uint32_t)lba,
                                      (uint32_t)number_blocks,
                                      media_status);
  g_usbd_storage_flush_last_status = status;
  g_usbd_storage_flush_last_media_status = *media_status;
  if (status != TX_SUCCESS)
  {
    *media_status = PS_UsbMscSenseNotReadyNoMedium();
    return UX_ERROR;
  }
  /* USER CODE END USBD_STORAGE_Flush */

  return status;
}

/**
  * @brief  USBD_STORAGE_Status
  *         This function is invoked to obtain the status of the device.
  * @param  storage_instance : Pointer to the storage class instance.
  * @param  lun: Logical unit number is the command is directed to.
  * @param  media_id: is not currently used.
  * @param  media_status: should be filled out exactly like the media status
  *                       callback return value.
  * @retval status
  */
UINT USBD_STORAGE_Status(VOID *storage_instance, ULONG lun, ULONG media_id,
                         ULONG *media_status)
{
  UINT status = UX_SUCCESS;

  /* USER CODE BEGIN USBD_STORAGE_Status */
  ULONG media_status_local = 1UL;
  g_usbd_storage_status_entry_count++;
  UX_PARAMETER_NOT_USED(storage_instance);
  UX_PARAMETER_NOT_USED(lun);
  UX_PARAMETER_NOT_USED(media_id);
  if (media_status == UX_NULL)
  {
    media_status = &media_status_local;
  }

  status = PS_StorageMscBridge_Status(media_status);
  g_usbd_storage_status_last_status = status;
  g_usbd_storage_status_last_media_status = *media_status;
  if (status != TX_SUCCESS)
  {
    *media_status = PS_UsbMscSenseNotReadyNoMedium();
    return UX_ERROR;
  }
  /* USER CODE END USBD_STORAGE_Status */

  return status;
}

/**
  * @brief  USBD_STORAGE_Notification
  *         This function is invoked to obtain the notification of the device.
  * @param  storage_instance : Pointer to the storage class instance.
  * @param  lun: Logical unit number is the command is directed to.
  * @param  media_id: is not currently used.
  * @param  notification_class: specifies the class of notification.
  * @param  media_notification: response for the notification.
  * @param  media_notification_length: length of the response buffer.
  * @retval status
  */
UINT USBD_STORAGE_Notification(VOID *storage_instance, ULONG lun, ULONG media_id,
                               ULONG notification_class, UCHAR **media_notification,
                               ULONG *media_notification_length)
{
  UINT status = UX_SUCCESS;

  /* USER CODE BEGIN USBD_STORAGE_Notification */
  UX_PARAMETER_NOT_USED(storage_instance);
  UX_PARAMETER_NOT_USED(lun);
  UX_PARAMETER_NOT_USED(media_id);
  UX_PARAMETER_NOT_USED(notification_class);
  UX_PARAMETER_NOT_USED(media_notification);
  UX_PARAMETER_NOT_USED(media_notification_length);
  /* USER CODE END USBD_STORAGE_Notification */

  return status;
}

/**
  * @brief  USBD_STORAGE_GetMediaLastLba
  *         Get Media last LBA.
  * @param  none
  * @retval last lba
  */
ULONG USBD_STORAGE_GetMediaLastLba(VOID)
{
  ULONG LastLba = 0U;

  /* USER CODE BEGIN USBD_STORAGE_GetMediaLastLba */
  LastLba = PS_STORAGE_MSC_BRIDGE_LAST_LBA;
  /* USER CODE END USBD_STORAGE_GetMediaLastLba */

  return LastLba;
}

/**
  * @brief  USBD_STORAGE_GetMediaBlocklength
  *         Get Media block length.
  * @param  none.
  * @retval block length.
  */
ULONG USBD_STORAGE_GetMediaBlocklength(VOID)
{
  ULONG MediaBlockLen = 0U;

  /* USER CODE BEGIN USBD_STORAGE_GetMediaBlocklength */
  MediaBlockLen = PS_STORAGE_MSC_BRIDGE_BLOCK_SIZE;
  /* USER CODE END USBD_STORAGE_GetMediaBlocklength */

  return MediaBlockLen;
}

/* USER CODE BEGIN 1 */

/* USER CODE END 1 */
