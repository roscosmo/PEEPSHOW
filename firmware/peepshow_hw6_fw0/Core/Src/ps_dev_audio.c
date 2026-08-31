#include "ps_dev_audio.h"

#include <string.h>

#include "tx_api.h"

#define PS_DEV_AUDIO_CALLBACK_NOT_RUN (0xFFFFFFFFUL)

static TX_SEMAPHORE ps_dev_audio_completion_semaphore;
static uint32_t ps_dev_audio_completion_semaphore_created;
static SAI_HandleTypeDef *volatile ps_dev_audio_active_sai;
static volatile uint32_t ps_dev_audio_callback_status;
static volatile uint32_t ps_dev_audio_dma_irq_count;
static volatile uint32_t ps_dev_audio_tx_callback_count;
static volatile uint32_t ps_dev_audio_error_callback_count;

void ps_dev_audio_record_dma_irq(void)
{
  ps_dev_audio_dma_irq_count++;
}

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

static ps_status_t ps_dev_audio_configure_one_shot_dma(
  DMA_HandleTypeDef *dma,
  DMA_QListTypeDef *queue)
{
  if ((dma == NULL) || (queue == NULL) || (queue->Head == NULL))
  {
    return PS_STATUS_NOT_INITIALIZED;
  }
  if (HAL_DMAEx_List_ClearCircularMode(queue) != HAL_OK)
  {
    return PS_STATUS_IO_ERROR;
  }

  dma->InitLinkedList.LinkedListMode = DMA_LINKEDLIST;
  return PS_STATUS_OK;
}

static ps_status_t ps_dev_audio_rearm_dma(ps_dev_audio_t *device)
{
  ps_status_t status;

  if ((device->dma == NULL) || (device->dma_queue == NULL) ||
      (device->dma_queue->Head == NULL))
  {
    return PS_STATUS_NOT_INITIALIZED;
  }

  /* Reset only the audio-owned channel. GPDMA1 is shared by other owners. */
  if (HAL_DMAEx_List_DeInit(device->dma) != HAL_OK)
  {
    return PS_STATUS_IO_ERROR;
  }

  status = ps_dev_audio_configure_one_shot_dma(device->dma,
                                                device->dma_queue);
  if (status != PS_STATUS_OK)
  {
    return status;
  }
  if (HAL_DMAEx_List_Init(device->dma) != HAL_OK)
  {
    return PS_STATUS_IO_ERROR;
  }
  if (HAL_DMAEx_List_LinkQ(device->dma, device->dma_queue) != HAL_OK)
  {
    return PS_STATUS_IO_ERROR;
  }

  device->sai->hdmatx = device->dma;
  device->dma->Parent = device->sai;
  if (HAL_DMA_ConfigChannelAttributes(device->dma,
                                      DMA_CHANNEL_NPRIV) != HAL_OK)
  {
    return PS_STATUS_IO_ERROR;
  }
  if ((device->dma->LinkedListQueue != device->dma_queue) ||
      (device->dma->Parent != device->sai) ||
      (HAL_DMA_GetState(device->dma) != HAL_DMA_STATE_READY) ||
      (HAL_DMA_GetError(device->dma) != HAL_DMA_ERROR_NONE))
  {
    return PS_STATUS_VERIFY_FAILED;
  }

  return PS_STATUS_OK;
}

static ps_status_t ps_dev_audio_rearm(ps_dev_audio_t *device)
{
  ps_status_t status;

  if ((device->sai->Instance != SAI1_Block_A) ||
      (device->sai->hdmatx != device->dma))
  {
    return PS_STATUS_NOT_INITIALIZED;
  }

  status = ps_dev_audio_rearm_dma(device);
  if (status != PS_STATUS_OK)
  {
    return status;
  }

  device->sai->State = HAL_SAI_STATE_READY;
  device->sai->ErrorCode = HAL_SAI_ERROR_NONE;
  device->sai->Lock = HAL_UNLOCKED;
  if (HAL_SAI_Init(device->sai) != HAL_OK)
  {
    return PS_STATUS_IO_ERROR;
  }

  if ((HAL_SAI_GetState(device->sai) != HAL_SAI_STATE_READY) ||
      (HAL_SAI_GetError(device->sai) != HAL_SAI_ERROR_NONE) ||
      (HAL_DMA_GetState(device->dma) != HAL_DMA_STATE_READY) ||
      (HAL_DMA_GetError(device->dma) != HAL_DMA_ERROR_NONE))
  {
    return PS_STATUS_VERIFY_FAILED;
  }

  return PS_STATUS_OK;
}

ps_status_t ps_dev_audio_init(ps_dev_audio_t *device,
                              SAI_HandleTypeDef *sai,
                              DMA_HandleTypeDef *dma,
                              GPIO_TypeDef *sd_gpio_port,
                              uint16_t sd_gpio_pin)
{
  DMA_QListTypeDef *dma_queue;
  ps_status_t status;

  if ((device == NULL) || (sai == NULL) || (dma == NULL) ||
      (sd_gpio_port == NULL))
  {
    return PS_STATUS_INVALID_ARGUMENT;
  }

  dma_queue = dma->LinkedListQueue;

  if (ps_dev_audio_completion_semaphore_created == 0UL)
  {
    if (tx_semaphore_create(&ps_dev_audio_completion_semaphore,
                            "audio DMA complete", 0UL) != TX_SUCCESS)
    {
      return PS_STATUS_INTERNAL_ERROR;
    }
    ps_dev_audio_completion_semaphore_created = 1UL;
  }

  status = ps_dev_audio_configure_one_shot_dma(dma, dma_queue);
  if (status != PS_STATUS_OK)
  {
    return status;
  }

  (void)memset(device, 0, sizeof(*device));
  device->api_version = PS_DEV_AUDIO_API_VERSION;
  device->sai = sai;
  device->dma = dma;
  device->dma_queue = dma_queue;
  device->sd_gpio_port = sd_gpio_port;
  device->sd_gpio_pin = sd_gpio_pin;
  device->state = PS_DEV_AUDIO_STATE_READY;
  device->last_status = PS_STATUS_OK;
  device->post_stop_recovery_status = PS_STATUS_OK;
  device->initialized = 1U;
  return PS_STATUS_OK;
}

ps_status_t ps_dev_audio_mark_post_stop_resume(ps_dev_audio_t *device)
{
  if (device == NULL)
  {
    return PS_STATUS_INVALID_ARGUMENT;
  }
  if (device->initialized == 0U)
  {
    return PS_STATUS_NOT_INITIALIZED;
  }

  device->post_stop_resume_mark_count++;
  device->post_stop_recovery_pending = 1UL;
  return PS_STATUS_OK;
}

ps_status_t ps_dev_audio_play_dma(ps_dev_audio_t *device,
                                  int16_t *samples,
                                  uint32_t sample_halfwords,
                                  uint32_t amp_settle_ticks,
                                  uint32_t completion_timeout_ticks,
                                  uint32_t expected_sai_kernel_hz,
                                  ps_dev_audio_play_result_t *result)
{
  HAL_StatusTypeDef start_status = HAL_ERROR;
  HAL_StatusTypeDef stop_status = HAL_ERROR;
  UINT wait_status = TX_WAIT_ERROR;
  uint32_t dma_irq_before = 0UL;
  uint32_t tx_callback_before = 0UL;
  uint32_t error_callback_before = 0UL;
  uint32_t post_stop_recovery_required;
  ps_status_t rearm_status;
  ps_status_t status;

  if ((device == NULL) || (samples == NULL) || (sample_halfwords == 0UL) ||
      (result == NULL))
  {
    return PS_STATUS_INVALID_ARGUMENT;
  }
  (void)memset(result, 0, sizeof(*result));
  result->status = PS_STATUS_INTERNAL_ERROR;
  result->rearm_status = PS_STATUS_INTERNAL_ERROR;
  result->start_hal_status = (uint32_t)HAL_ERROR;
  result->completion_wait_status = (uint32_t)TX_WAIT_ERROR;
  result->completion_callback_status = PS_DEV_AUDIO_CALLBACK_NOT_RUN;
  result->stop_hal_status = (uint32_t)HAL_ERROR;
  if (device->initialized == 0U)
  {
    result->status = PS_STATUS_NOT_INITIALIZED;
    return result->status;
  }

  device->operation_count++;
  device->state = PS_DEV_AUDIO_STATE_PLAYING;
  /* thPower physically verified this grant before dispatching audio work. */
  result->sai_kernel_hz = expected_sai_kernel_hz;
  result->sd_state_before =
    HAL_GPIO_ReadPin(device->sd_gpio_port, device->sd_gpio_pin);

  ps_dev_audio_active_sai = NULL;
  ps_dev_audio_callback_status = PS_DEV_AUDIO_CALLBACK_NOT_RUN;
  result->pre_stop_hal_status = (uint32_t)HAL_SAI_DMAStop(device->sai);
  post_stop_recovery_required = device->post_stop_recovery_pending;
  if (post_stop_recovery_required != 0UL)
  {
    device->post_stop_recovery_attempt_count++;
  }
  rearm_status = ps_dev_audio_rearm(device);
  if (post_stop_recovery_required != 0UL)
  {
    device->post_stop_recovery_status = (uint32_t)rearm_status;
    if (rearm_status == PS_STATUS_OK)
    {
      device->post_stop_recovery_pending = 0UL;
      device->post_stop_recovery_success_count++;
    }
  }
  result->rearm_status = (uint32_t)rearm_status;
  if (rearm_status == PS_STATUS_OK)
  {
    HAL_GPIO_WritePin(device->sd_gpio_port, device->sd_gpio_pin, GPIO_PIN_SET);
    result->sd_state_enabled =
      HAL_GPIO_ReadPin(device->sd_gpio_port, device->sd_gpio_pin);
    tx_thread_sleep(amp_settle_ticks);

    (void)tx_semaphore_get(&ps_dev_audio_completion_semaphore, TX_NO_WAIT);
    dma_irq_before = ps_dev_audio_dma_irq_count;
    tx_callback_before = ps_dev_audio_tx_callback_count;
    error_callback_before = ps_dev_audio_error_callback_count;
    ps_dev_audio_active_sai = device->sai;
    start_status = HAL_SAI_Transmit_DMA(
      device->sai,
      (uint8_t *)samples,
      sample_halfwords);
    if (start_status == HAL_OK)
    {
      wait_status = tx_semaphore_get(&ps_dev_audio_completion_semaphore,
                                     completion_timeout_ticks);
      result->pre_cleanup_dma_irq_delta =
        ps_dev_audio_dma_irq_count - dma_irq_before;
      result->pre_cleanup_tx_callback_delta =
        ps_dev_audio_tx_callback_count - tx_callback_before;
      result->pre_cleanup_error_callback_delta =
        ps_dev_audio_error_callback_count - error_callback_before;
      result->pre_cleanup_sai_kernel_hz =
        expected_sai_kernel_hz;
      result->pre_cleanup_sai_state =
        (uint32_t)HAL_SAI_GetState(device->sai);
      result->pre_cleanup_sai_error = HAL_SAI_GetError(device->sai);
      result->pre_cleanup_sai_cr1 = device->sai->Instance->CR1;
      result->pre_cleanup_sai_cr2 = device->sai->Instance->CR2;
      result->pre_cleanup_sai_frcr = device->sai->Instance->FRCR;
      result->pre_cleanup_sai_slotr = device->sai->Instance->SLOTR;
      result->pre_cleanup_sai_imr = device->sai->Instance->IMR;
      result->pre_cleanup_sai_sr = device->sai->Instance->SR;
      result->pre_cleanup_sai_gcr = SAI1->GCR;
      result->pre_cleanup_dma_state =
        (uint32_t)HAL_DMA_GetState(device->dma);
      result->pre_cleanup_dma_error = HAL_DMA_GetError(device->dma);
      result->pre_cleanup_dma_ccr = device->dma->Instance->CCR;
      result->pre_cleanup_dma_csr = device->dma->Instance->CSR;
      result->pre_cleanup_dma_cbr1 = device->dma->Instance->CBR1;
      result->pre_cleanup_dma_ctr1 = device->dma->Instance->CTR1;
      result->pre_cleanup_dma_ctr2 = device->dma->Instance->CTR2;
      result->pre_cleanup_dma_csar = device->dma->Instance->CSAR;
      result->pre_cleanup_dma_cdar = device->dma->Instance->CDAR;
      result->pre_cleanup_dma_cllr = device->dma->Instance->CLLR;
      ps_dev_audio_active_sai = NULL;
      stop_status = HAL_SAI_DMAStop(device->sai);
    }
    else
    {
      ps_dev_audio_active_sai = NULL;
    }
  }
  result->start_hal_status = (uint32_t)start_status;
  result->completion_wait_status = (uint32_t)wait_status;
  result->completion_callback_status = ps_dev_audio_callback_status;
  result->stop_hal_status = (uint32_t)stop_status;

  HAL_GPIO_WritePin(device->sd_gpio_port, device->sd_gpio_pin, GPIO_PIN_RESET);
  result->sd_state_after =
    HAL_GPIO_ReadPin(device->sd_gpio_port, device->sd_gpio_pin);
  result->sai_state_after = (uint32_t)HAL_SAI_GetState(device->sai);
  result->sai_error_after = HAL_SAI_GetError(device->sai);
  result->dma_state_after = (uint32_t)HAL_DMA_GetState(device->dma);
  result->dma_error_after = HAL_DMA_GetError(device->dma);

  status = rearm_status;
  if (status == PS_STATUS_OK)
  {
    status = ps_dev_audio_hal_status(start_status);
  }
  if ((status == PS_STATUS_OK) && (wait_status != TX_SUCCESS))
  {
    status = PS_STATUS_TIMEOUT;
  }
  if ((status == PS_STATUS_OK) &&
      (ps_dev_audio_callback_status != (uint32_t)HAL_OK))
  {
    status = PS_STATUS_IO_ERROR;
  }
  if ((status == PS_STATUS_OK) && (stop_status != HAL_OK))
  {
    status = ps_dev_audio_hal_status(stop_status);
  }
  if ((status == PS_STATUS_OK) &&
      ((result->sd_state_enabled == 0UL) ||
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

void HAL_SAI_TxCpltCallback(SAI_HandleTypeDef *hsai)
{
  if (ps_dev_audio_active_sai == hsai)
  {
    ps_dev_audio_tx_callback_count++;
    ps_dev_audio_active_sai = NULL;
    ps_dev_audio_callback_status = (uint32_t)HAL_OK;
    (void)tx_semaphore_put(&ps_dev_audio_completion_semaphore);
  }
}

void HAL_SAI_ErrorCallback(SAI_HandleTypeDef *hsai)
{
  if (ps_dev_audio_active_sai == hsai)
  {
    ps_dev_audio_error_callback_count++;
    ps_dev_audio_active_sai = NULL;
    ps_dev_audio_callback_status = (uint32_t)HAL_ERROR;
    (void)tx_semaphore_put(&ps_dev_audio_completion_semaphore);
  }
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
