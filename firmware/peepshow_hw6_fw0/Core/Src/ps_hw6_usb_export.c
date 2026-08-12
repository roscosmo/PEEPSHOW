#include "ps_hw6_usb_export.h"

#include "main.h"
#include "app_usbx_device.h"
#include "ps_hw6_clock_policy.h"
#include "ps_hw6_owner_state_machines.h"

extern PCD_HandleTypeDef hpcd_USB_OTG_FS;
extern volatile UINT g_ps_hw6_usbx_byte_pool_create_status;
extern volatile UINT g_ps_hw6_usbx_device_init_status;
extern volatile UINT g_ps_hw6_usbx_init_stage;
extern volatile UINT g_ps_hw6_usbx_init_error_code;
extern volatile UINT g_ps_hw6_usbx_dcd_status;

static ULONG ps_hw6_usb_device_active;
static ULONG ps_hw6_usb_device_start_ok_count;
static ULONG ps_hw6_usb_device_start_fail_count;
static ULONG ps_hw6_usb_device_stop_ok_count;
static ULONG ps_hw6_usb_device_stop_fail_count;
static LONG ps_hw6_usb_device_last_error;

static UINT PS_HW6_UsbExport_Clock48Set(uint32_t hsi48_state)
{
  RCC_OscInitTypeDef osc = {0};
  HAL_StatusTypeDef hal_status;

  osc.OscillatorType = RCC_OSCILLATORTYPE_HSI48;
  osc.HSI48State = hsi48_state;
  osc.PLL.PLLState = RCC_PLL_NONE;

  hal_status = HAL_RCC_OscConfig(&osc);
  return (hal_status == HAL_OK) ? TX_SUCCESS : TX_NOT_DONE;
}

static void PS_HW6_UsbExport_VddUsbSet(UINT enabled)
{
  UINT pwr_clock_was_disabled =
    (__HAL_RCC_PWR_IS_CLK_DISABLED() != 0U) ? 1U : 0U;

  if (pwr_clock_was_disabled != 0U)
  {
    __HAL_RCC_PWR_CLK_ENABLE();
  }

  if (enabled != 0U)
  {
    HAL_PWREx_EnableVddUSB();
  }
  else
  {
    HAL_PWREx_DisableVddUSB();
  }

  if (pwr_clock_was_disabled != 0U)
  {
    __HAL_RCC_PWR_CLK_DISABLE();
  }
}

static UINT PS_HW6_UsbExport_DeviceHardwareOff(void)
{
  HAL_StatusTypeDef hal_status = HAL_OK;
  uint8_t had_error = 0U;

  HAL_NVIC_DisableIRQ(OTG_FS_IRQn);
  NVIC_ClearPendingIRQ(OTG_FS_IRQn);

  if (hpcd_USB_OTG_FS.State != HAL_PCD_STATE_RESET)
  {
    hal_status = HAL_PCD_DeInit(&hpcd_USB_OTG_FS);
    g_ps_hw6_owner_sm_probe.usb_reclaim_deinit_status =
      (uint32_t)hal_status;
    if (hal_status != HAL_OK)
    {
      had_error = 1U;
    }
  }
  else
  {
    __HAL_RCC_USB_CLK_DISABLE();
  }
  __HAL_RCC_USB_CLK_DISABLE();

  PS_HW6_UsbExport_VddUsbSet(0U);
  if (PS_HW6_UsbExport_Clock48Set(RCC_HSI48_OFF) != TX_SUCCESS)
  {
    had_error = 1U;
  }

  return (had_error == 0U) ? TX_SUCCESS : TX_NOT_DONE;
}

static UINT PS_HW6_UsbExport_StopDeviceWithGrace(ULONG disconnect_grace_ticks)
{
  HAL_StatusTypeDef hal_status;
  UINT hw_status;
  uint8_t had_error = 0U;
  uint8_t need_teardown =
    ((ps_hw6_usb_device_active != 0UL) ||
     (hpcd_USB_OTG_FS.State != HAL_PCD_STATE_RESET)) ? 1U : 0U;

  if (need_teardown != 0U)
  {
    hal_status = HAL_PCD_DevDisconnect(&hpcd_USB_OTG_FS);
    g_ps_hw6_owner_sm_probe.usb_reclaim_devdisconnect_status =
      (uint32_t)hal_status;
    if ((hal_status != HAL_OK) && (hal_status != HAL_BUSY))
    {
      had_error = 1U;
      ps_hw6_usb_device_last_error = -3L;
    }

    if (disconnect_grace_ticks > 0UL)
    {
      tx_thread_sleep(disconnect_grace_ticks);
    }

    hal_status = HAL_PCD_Stop(&hpcd_USB_OTG_FS);
    g_ps_hw6_owner_sm_probe.usb_reclaim_pcd_stop_status =
      (uint32_t)hal_status;
    if ((hal_status != HAL_OK) && (hal_status != HAL_BUSY))
    {
      had_error = 1U;
      ps_hw6_usb_device_last_error = -4L;
    }
  }

  HAL_NVIC_DisableIRQ(OTG_FS_IRQn);
  NVIC_ClearPendingIRQ(OTG_FS_IRQn);
  hw_status = PS_HW6_UsbExport_DeviceHardwareOff();
  if (hw_status != TX_SUCCESS)
  {
    had_error = 1U;
    if (ps_hw6_usb_device_last_error == 0L)
    {
      ps_hw6_usb_device_last_error = -5L;
    }
  }

  ps_hw6_usb_device_active = 0UL;

  if (need_teardown == 0U)
  {
    ps_hw6_usb_device_last_error = 0L;
    return TX_SUCCESS;
  }

  if (had_error == 0U)
  {
    ps_hw6_usb_device_last_error = 0L;
    if (ps_hw6_usb_device_stop_ok_count < 0xFFFFFFFFUL)
    {
      ps_hw6_usb_device_stop_ok_count++;
    }
    return TX_SUCCESS;
  }

  if (ps_hw6_usb_device_stop_fail_count < 0xFFFFFFFFUL)
  {
    ps_hw6_usb_device_stop_fail_count++;
  }
  return TX_NOT_DONE;
}

static UINT PS_HW6_UsbExport_DeviceHardwareInit(void)
{
  HAL_StatusTypeDef hal_status;

  if (hpcd_USB_OTG_FS.State != HAL_PCD_STATE_RESET)
  {
    return TX_SUCCESS;
  }

  if (PS_HW6_ClockPolicy_ProfileIsActive(
        (uint32_t)PS_HW6_CLOCK_PROFILE_IO_HIGH,
        PS_HW6_CLOCK_CAP_USB_DEVICE_ACTIVE) == 0UL)
  {
    return TX_NOT_DONE;
  }

  if (PS_HW6_UsbExport_Clock48Set(RCC_HSI48_ON) != TX_SUCCESS)
  {
    return TX_NOT_DONE;
  }

  PS_HW6_UsbExport_VddUsbSet(1U);

  hal_status = HAL_PCD_Init(&hpcd_USB_OTG_FS);
  g_ps_hw6_owner_sm_probe.usb_export_pcd_init_status =
    (uint32_t)hal_status;
  if (hal_status == HAL_OK)
  {
    g_ps_hw6_owner_sm_probe.usb_export_irq_priority_before =
      (uint32_t)NVIC_GetPriority(OTG_FS_IRQn);
    g_ps_hw6_owner_sm_probe.usb_export_irq_priority_after =
      (uint32_t)NVIC_GetPriority(OTG_FS_IRQn);
  }
  if (hal_status != HAL_OK)
  {
    (void)PS_HW6_UsbExport_DeviceHardwareOff();
    return TX_NOT_DONE;
  }

  hal_status = HAL_PCDEx_SetRxFiFo(&hpcd_USB_OTG_FS, 0x80U);
  g_ps_hw6_owner_sm_probe.usb_export_pcd_init_status =
    (uint32_t)hal_status;
  if (hal_status != HAL_OK)
  {
    (void)PS_HW6_UsbExport_DeviceHardwareOff();
    return TX_NOT_DONE;
  }

  hal_status = HAL_PCDEx_SetTxFiFo(&hpcd_USB_OTG_FS, 0U, 0x40U);
  g_ps_hw6_owner_sm_probe.usb_export_pcd_init_status =
    (uint32_t)hal_status;
  if (hal_status != HAL_OK)
  {
    (void)PS_HW6_UsbExport_DeviceHardwareOff();
    return TX_NOT_DONE;
  }

  hal_status = HAL_PCDEx_SetTxFiFo(&hpcd_USB_OTG_FS, 1U, 0x80U);
  g_ps_hw6_owner_sm_probe.usb_export_pcd_init_status =
    (uint32_t)hal_status;
  if (hal_status != HAL_OK)
  {
    (void)PS_HW6_UsbExport_DeviceHardwareOff();
    return TX_NOT_DONE;
  }

  return TX_SUCCESS;
}

void PS_HW6_UsbExport_Reset(void)
{
  ps_hw6_usb_device_active = 0UL;
  ps_hw6_usb_device_start_ok_count = 0UL;
  ps_hw6_usb_device_start_fail_count = 0UL;
  ps_hw6_usb_device_stop_ok_count = 0UL;
  ps_hw6_usb_device_stop_fail_count = 0UL;
  ps_hw6_usb_device_last_error = 0L;
}

UINT PS_HW6_UsbExport_StartDevice(void)
{
  HAL_StatusTypeDef hal_status;
  UINT hw_status;

  g_ps_hw6_owner_sm_probe.usb_export_dcd_status =
    (uint32_t)g_ps_hw6_usbx_dcd_status;

  if (ps_hw6_usb_device_active != 0UL)
  {
    return TX_SUCCESS;
  }

  if (hpcd_USB_OTG_FS.State != HAL_PCD_STATE_RESET)
  {
    (void)PS_HW6_UsbExport_StopDevice();
  }

  if ((g_ps_hw6_usbx_byte_pool_create_status != TX_SUCCESS) ||
      (g_ps_hw6_usbx_device_init_status != UX_SUCCESS) ||
      (g_ps_hw6_usbx_init_stage < 101U) ||
      (g_ps_hw6_usbx_init_error_code != UX_SUCCESS))
  {
    ps_hw6_usb_device_last_error = -6L;
    if (ps_hw6_usb_device_start_fail_count < 0xFFFFFFFFUL)
    {
      ps_hw6_usb_device_start_fail_count++;
    }
    return TX_NOT_DONE;
  }

  hw_status = PS_HW6_UsbExport_DeviceHardwareInit();
  if (hw_status != TX_SUCCESS)
  {
    ps_hw6_usb_device_last_error = -7L;
    if (ps_hw6_usb_device_start_fail_count < 0xFFFFFFFFUL)
    {
      ps_hw6_usb_device_start_fail_count++;
    }
    return hw_status;
  }

  hal_status = HAL_PCD_Start(&hpcd_USB_OTG_FS);
  g_ps_hw6_owner_sm_probe.usb_export_pcd_start_status =
    (uint32_t)hal_status;
  if ((hal_status != HAL_OK) && (hal_status != HAL_BUSY))
  {
    ps_hw6_usb_device_last_error = -1L;
    if (ps_hw6_usb_device_start_fail_count < 0xFFFFFFFFUL)
    {
      ps_hw6_usb_device_start_fail_count++;
    }
    (void)PS_HW6_UsbExport_DeviceHardwareOff();
    return TX_NOT_DONE;
  }

  NVIC_ClearPendingIRQ(OTG_FS_IRQn);
  HAL_NVIC_EnableIRQ(OTG_FS_IRQn);

  hal_status = HAL_PCD_DevConnect(&hpcd_USB_OTG_FS);
  g_ps_hw6_owner_sm_probe.usb_export_devconnect_status =
    (uint32_t)hal_status;
  if ((hal_status != HAL_OK) && (hal_status != HAL_BUSY))
  {
    ps_hw6_usb_device_last_error = -2L;
    if (ps_hw6_usb_device_start_fail_count < 0xFFFFFFFFUL)
    {
      ps_hw6_usb_device_start_fail_count++;
    }
    (void)PS_HW6_UsbExport_DeviceHardwareOff();
    return TX_NOT_DONE;
  }

  ps_hw6_usb_device_active = 1UL;
  ps_hw6_usb_device_last_error = 0L;
  if (ps_hw6_usb_device_start_ok_count < 0xFFFFFFFFUL)
  {
    ps_hw6_usb_device_start_ok_count++;
  }
  return TX_SUCCESS;
}

UINT PS_HW6_UsbExport_StopDevice(void)
{
  return PS_HW6_UsbExport_StopDeviceWithGrace(0UL);
}
