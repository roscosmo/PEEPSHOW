#include "ps_dev_ls013b7dh05.h"

#include <string.h>

static ps_status_t ps_dev_ls013b7dh05_hal_status(HAL_StatusTypeDef status)
{
  switch (status)
  {
    case HAL_OK:
      return PS_STATUS_OK;

    case HAL_BUSY:
      return PS_STATUS_BUSY;

    case HAL_TIMEOUT:
      return PS_STATUS_TIMEOUT;

    case HAL_ERROR:
    default:
      return PS_STATUS_IO_ERROR;
  }
}

ps_status_t ps_dev_ls013b7dh05_init(ps_dev_ls013b7dh05_t *device,
                                    SPI_HandleTypeDef *bus)
{
  if ((device == NULL) || (bus == NULL))
  {
    return PS_STATUS_INVALID_ARGUMENT;
  }

  (void)memset(device, 0, sizeof(*device));
  device->api_version = PS_DEV_LS013B7DH05_API_VERSION;
  device->bus = bus;
  device->state = PS_DEV_LS013B7DH05_STATE_READY;
  device->last_status = PS_STATUS_OK;
  device->initialized = 1U;
  return PS_STATUS_OK;
}

ps_status_t ps_dev_ls013b7dh05_clear(ps_dev_ls013b7dh05_t *device,
                                     uint32_t *clear_hal_status)
{
  HAL_StatusTypeDef init_status;
  ps_status_t status;

  if (device == NULL)
  {
    return PS_STATUS_INVALID_ARGUMENT;
  }
  if (clear_hal_status != NULL)
  {
    *clear_hal_status = (uint32_t)HAL_ERROR;
  }
  if (device->initialized == 0U)
  {
    device->last_status = (uint32_t)PS_STATUS_NOT_INITIALIZED;
    return PS_STATUS_NOT_INITIALIZED;
  }

  device->operation_count++;
  init_status = LCD_Init(&device->panel, device->bus);
  if (clear_hal_status != NULL)
  {
    *clear_hal_status = (uint32_t)init_status;
  }

  status = ps_dev_ls013b7dh05_hal_status(init_status);
  device->last_status = (uint32_t)status;
  if (status == PS_STATUS_OK)
  {
    device->state = PS_DEV_LS013B7DH05_STATE_STATIC_HOLD;
  }
  else
  {
    device->state = PS_DEV_LS013B7DH05_STATE_FAULT;
  }
  return status;
}

ps_status_t ps_dev_ls013b7dh05_present_full_dma(
  ps_dev_ls013b7dh05_t *device,
  const uint8_t *framebuffer,
  uint32_t timeout_ms,
  DMA_HandleTypeDef *dma,
  ps_dev_ls013b7dh05_present_result_t *result)
{
  HAL_StatusTypeDef init_status;
  HAL_StatusTypeDef present_status = HAL_ERROR;
  ps_status_t status;

  if ((device == NULL) || (framebuffer == NULL) || (dma == NULL) ||
      (result == NULL))
  {
    return PS_STATUS_INVALID_ARGUMENT;
  }
  (void)memset(result, 0, sizeof(*result));
  result->status = PS_STATUS_INTERNAL_ERROR;
  result->present_hal_status = (uint32_t)HAL_ERROR;
  if (device->initialized == 0U)
  {
    result->status = PS_STATUS_NOT_INITIALIZED;
    return result->status;
  }
  device->operation_count++;

  init_status = LCD_Init(&device->panel, device->bus);
  result->init_hal_status = (uint32_t)init_status;
  if (init_status == HAL_OK)
  {
    present_status = LCD_PresentFull_DMA(
      &device->panel,
      framebuffer,
      timeout_ms);
  }
  result->present_hal_status = (uint32_t)present_status;
  result->dma_done = LCD_FlushDMA_IsDone() ? 1UL : 0UL;
  result->spi_state_after = (uint32_t)HAL_SPI_GetState(device->bus);
  result->spi_error_after = HAL_SPI_GetError(device->bus);
  result->dma_state_after = (uint32_t)HAL_DMA_GetState(dma);
  result->dma_error_after = HAL_DMA_GetError(dma);

  status = ps_dev_ls013b7dh05_hal_status(init_status);
  if ((status == PS_STATUS_OK) && (present_status != HAL_OK))
  {
    status = ps_dev_ls013b7dh05_hal_status(present_status);
  }
  if ((status == PS_STATUS_OK) &&
      ((result->dma_done == 0UL) ||
       (result->spi_error_after != HAL_SPI_ERROR_NONE) ||
       (result->dma_error_after != HAL_DMA_ERROR_NONE)))
  {
    status = PS_STATUS_VERIFY_FAILED;
  }

  result->status = status;
  device->last_status = (uint32_t)status;
  if (status == PS_STATUS_OK)
  {
    device->state = PS_DEV_LS013B7DH05_STATE_STATIC_HOLD;
  }
  else
  {
    device->state = PS_DEV_LS013B7DH05_STATE_FAULT;
  }
  return result->status;
}
