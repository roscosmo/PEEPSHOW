#include "ps_dev_audio.h"

#include <string.h>

#include "tx_api.h"

static ps_status_t ps_dev_audio_hal_status(HAL_StatusTypeDef status)
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

ps_status_t ps_dev_audio_init(ps_dev_audio_t *device,
                              SAI_HandleTypeDef *sai,
                              DMA_HandleTypeDef *dma,
                              GPIO_TypeDef *sd_gpio_port,
                              uint16_t sd_gpio_pin)
{
  if ((device == NULL) || (sai == NULL) || (dma == NULL) ||
      (sd_gpio_port == NULL))
  {
    return PS_STATUS_INVALID_ARGUMENT;
  }

  (void)memset(device, 0, sizeof(*device));
  device->api_version = PS_DEV_AUDIO_API_VERSION;
  device->sai = sai;
  device->dma = dma;
  device->sd_gpio_port = sd_gpio_port;
  device->sd_gpio_pin = sd_gpio_pin;
  device->state = PS_DEV_AUDIO_STATE_READY;
  device->last_status = PS_STATUS_OK;
  device->initialized = 1U;
  return PS_STATUS_OK;
}

ps_status_t ps_dev_audio_play_dma(ps_dev_audio_t *device,
                                  int16_t *samples,
                                  uint32_t sample_halfwords,
                                  uint32_t amp_settle_ticks,
                                  uint32_t duration_ticks,
                                  uint32_t expected_sai_kernel_hz,
                                  ps_dev_audio_play_result_t *result)
{
  HAL_StatusTypeDef start_status;
  HAL_StatusTypeDef stop_status = HAL_ERROR;
  ps_status_t status;

  if ((device == NULL) || (samples == NULL) || (sample_halfwords == 0UL) ||
      (result == NULL))
  {
    return PS_STATUS_INVALID_ARGUMENT;
  }
  (void)memset(result, 0, sizeof(*result));
  result->status = PS_STATUS_INTERNAL_ERROR;
  result->start_hal_status = (uint32_t)HAL_ERROR;
  result->stop_hal_status = (uint32_t)HAL_ERROR;
  if (device->initialized == 0U)
  {
    result->status = PS_STATUS_NOT_INITIALIZED;
    return result->status;
  }

  device->operation_count++;
  device->state = PS_DEV_AUDIO_STATE_PLAYING;
  result->sai_kernel_hz = HAL_RCCEx_GetPeriphCLKFreq(RCC_PERIPHCLK_SAI1);
  result->sd_state_before =
    HAL_GPIO_ReadPin(device->sd_gpio_port, device->sd_gpio_pin);

  result->pre_stop_hal_status = (uint32_t)HAL_SAI_DMAStop(device->sai);
  HAL_GPIO_WritePin(device->sd_gpio_port, device->sd_gpio_pin, GPIO_PIN_SET);
  result->sd_state_enabled =
    HAL_GPIO_ReadPin(device->sd_gpio_port, device->sd_gpio_pin);
  tx_thread_sleep(amp_settle_ticks);

  start_status = HAL_SAI_Transmit_DMA(
    device->sai,
    (uint8_t *)samples,
    sample_halfwords);
  result->start_hal_status = (uint32_t)start_status;
  if (start_status == HAL_OK)
  {
    tx_thread_sleep(duration_ticks);
    stop_status = HAL_SAI_DMAStop(device->sai);
  }
  result->stop_hal_status = (uint32_t)stop_status;

  HAL_GPIO_WritePin(device->sd_gpio_port, device->sd_gpio_pin, GPIO_PIN_RESET);
  result->sd_state_after =
    HAL_GPIO_ReadPin(device->sd_gpio_port, device->sd_gpio_pin);
  result->sai_state_after = (uint32_t)HAL_SAI_GetState(device->sai);
  result->sai_error_after = HAL_SAI_GetError(device->sai);
  result->dma_state_after = (uint32_t)HAL_DMA_GetState(device->dma);
  result->dma_error_after = HAL_DMA_GetError(device->dma);

  status = ps_dev_audio_hal_status(start_status);
  if ((status == PS_STATUS_OK) && (stop_status != HAL_OK))
  {
    status = ps_dev_audio_hal_status(stop_status);
  }
  if ((status == PS_STATUS_OK) &&
      ((result->sai_kernel_hz != expected_sai_kernel_hz) ||
       (result->sd_state_enabled == 0UL) ||
       (result->sd_state_after != 0UL) ||
       (result->sai_error_after != HAL_SAI_ERROR_NONE) ||
       (result->dma_error_after != HAL_DMA_ERROR_NONE)))
  {
    status = PS_STATUS_VERIFY_FAILED;
  }

  result->status = status;
  device->last_status = (uint32_t)status;
  if (status == PS_STATUS_OK)
  {
    device->state = PS_DEV_AUDIO_STATE_IDLE;
  }
  else
  {
    device->state = PS_DEV_AUDIO_STATE_FAULT;
  }
  return result->status;
}

ps_status_t ps_dev_audio_verify_idle(ps_dev_audio_t *device,
                                     ps_dev_audio_play_result_t *result)
{
  ps_status_t status = PS_STATUS_OK;

  if ((device == NULL) || (result == NULL))
  {
    return PS_STATUS_INVALID_ARGUMENT;
  }
  (void)memset(result, 0, sizeof(*result));
  if (device->initialized == 0U)
  {
    result->status = PS_STATUS_NOT_INITIALIZED;
    return result->status;
  }

  result->sd_state_after =
    HAL_GPIO_ReadPin(device->sd_gpio_port, device->sd_gpio_pin);
  result->sai_state_after = (uint32_t)HAL_SAI_GetState(device->sai);
  result->sai_error_after = HAL_SAI_GetError(device->sai);
  result->dma_state_after = (uint32_t)HAL_DMA_GetState(device->dma);
  result->dma_error_after = HAL_DMA_GetError(device->dma);

  if ((result->sd_state_after != 0UL) ||
      (result->sai_state_after != HAL_SAI_STATE_READY) ||
      (result->sai_error_after != HAL_SAI_ERROR_NONE) ||
      (result->dma_error_after != HAL_DMA_ERROR_NONE))
  {
    status = PS_STATUS_VERIFY_FAILED;
  }

  result->status = status;
  device->last_status = (uint32_t)status;
  if (status == PS_STATUS_OK)
  {
    device->state = PS_DEV_AUDIO_STATE_IDLE;
  }
  else
  {
    device->state = PS_DEV_AUDIO_STATE_FAULT;
  }
  return result->status;
}
