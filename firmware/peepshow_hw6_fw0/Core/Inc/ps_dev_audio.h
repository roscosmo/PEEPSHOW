#ifndef PS_DEV_AUDIO_H
#define PS_DEV_AUDIO_H

#include <stdint.h>

#include "ps_status.h"
#include "stm32u5xx_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PS_DEV_AUDIO_API_VERSION (1UL)

typedef enum
{
  PS_DEV_AUDIO_STATE_UNINITIALIZED = 0,
  PS_DEV_AUDIO_STATE_READY,
  PS_DEV_AUDIO_STATE_PLAYING,
  PS_DEV_AUDIO_STATE_IDLE,
  PS_DEV_AUDIO_STATE_FAULT
} ps_dev_audio_state_t;

typedef struct
{
  uint32_t api_version;
  uint32_t initialized;
  uint32_t operation_count;
  uint32_t state;
  uint32_t last_status;
  SAI_HandleTypeDef *sai;
  DMA_HandleTypeDef *dma;
  GPIO_TypeDef *sd_gpio_port;
  uint16_t sd_gpio_pin;
} ps_dev_audio_t;

typedef struct
{
  ps_status_t status;
  uint32_t sai_kernel_hz;
  uint32_t sd_state_before;
  uint32_t pre_stop_hal_status;
  uint32_t sd_state_enabled;
  uint32_t start_hal_status;
  uint32_t stop_hal_status;
  uint32_t sd_state_after;
  uint32_t sai_state_after;
  uint32_t sai_error_after;
  uint32_t dma_state_after;
  uint32_t dma_error_after;
} ps_dev_audio_play_result_t;

ps_status_t ps_dev_audio_init(ps_dev_audio_t *device,
                              SAI_HandleTypeDef *sai,
                              DMA_HandleTypeDef *dma,
                              GPIO_TypeDef *sd_gpio_port,
                              uint16_t sd_gpio_pin);

ps_status_t ps_dev_audio_play_dma(ps_dev_audio_t *device,
                                  int16_t *samples,
                                  uint32_t sample_halfwords,
                                  uint32_t amp_settle_ticks,
                                  uint32_t duration_ticks,
                                  uint32_t expected_sai_kernel_hz,
                                  ps_dev_audio_play_result_t *result);

ps_status_t ps_dev_audio_verify_idle(ps_dev_audio_t *device,
                                     ps_dev_audio_play_result_t *result);

#ifdef __cplusplus
}
#endif

#endif /* PS_DEV_AUDIO_H */
