#ifndef PS_DEV_AUDIO_H
#define PS_DEV_AUDIO_H

#include <stdint.h>

#include "ps_status.h"
#include "stm32u5xx_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PS_DEV_AUDIO_API_VERSION (11UL)
#define PS_DEV_AUDIO_STREAM_EVENT_FIRST_HALF (1UL)
#define PS_DEV_AUDIO_STREAM_EVENT_SECOND_HALF (2UL)

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
  uint32_t post_stop_resume_mark_count;
  uint32_t post_stop_recovery_pending;
  uint32_t post_stop_recovery_attempt_count;
  uint32_t post_stop_recovery_success_count;
  uint32_t post_stop_recovery_status;
  SAI_HandleTypeDef *sai;
  DMA_HandleTypeDef *dma;
  DMA_QListTypeDef *dma_queue;
  GPIO_TypeDef *sd_gpio_port;
  uint16_t sd_gpio_pin;
} ps_dev_audio_t;

typedef struct
{
  ps_status_t status;
  uint32_t sai_kernel_hz;
  uint32_t sd_state_before;
  uint32_t pre_stop_hal_status;
  uint32_t rearm_status;
  uint32_t sd_state_enabled;
  uint32_t start_hal_status;
  uint32_t completion_wait_status;
  uint32_t completion_callback_status;
  uint32_t pre_cleanup_dma_irq_delta;
  uint32_t pre_cleanup_tx_callback_delta;
  uint32_t pre_cleanup_error_callback_delta;
  uint32_t pre_cleanup_sai_kernel_hz;
  uint32_t pre_cleanup_sai_state;
  uint32_t pre_cleanup_sai_error;
  uint32_t pre_cleanup_sai_cr1;
  uint32_t pre_cleanup_sai_cr2;
  uint32_t pre_cleanup_sai_frcr;
  uint32_t pre_cleanup_sai_slotr;
  uint32_t pre_cleanup_sai_imr;
  uint32_t pre_cleanup_sai_sr;
  uint32_t pre_cleanup_sai_gcr;
  uint32_t pre_cleanup_dma_state;
  uint32_t pre_cleanup_dma_error;
  uint32_t pre_cleanup_dma_ccr;
  uint32_t pre_cleanup_dma_csr;
  uint32_t pre_cleanup_dma_cbr1;
  uint32_t pre_cleanup_dma_ctr1;
  uint32_t pre_cleanup_dma_ctr2;
  uint32_t pre_cleanup_dma_csar;
  uint32_t pre_cleanup_dma_cdar;
  uint32_t pre_cleanup_dma_cllr;
  uint32_t stop_hal_status;
  uint32_t sd_state_after;
  uint32_t sai_state_after;
  uint32_t sai_error_after;
  uint32_t dma_state_after;
  uint32_t dma_error_after;
  uint32_t stream_wait_status;
  uint32_t stream_wait_preempt_disable_before;
  uint32_t stream_wait_system_state_before;
  uint32_t stream_wait_current_thread_before;
  uint32_t stream_first_half_callback_count;
  uint32_t stream_second_half_callback_count;
  uint32_t stream_underrun_count;
  uint32_t stream_pending_event_mask;
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
                                  uint32_t completion_timeout_ticks,
                                  uint32_t expected_sai_kernel_hz,
                                  ps_dev_audio_play_result_t *result);

ps_status_t ps_dev_audio_stream_start(ps_dev_audio_t *device,
                                      int16_t *samples,
                                      uint32_t sample_halfwords,
                                      uint32_t amp_settle_ticks,
                                      uint32_t expected_sai_kernel_hz,
                                      ps_dev_audio_play_result_t *result);

ps_status_t ps_dev_audio_stream_wait(ps_dev_audio_t *device,
                                     uint32_t timeout_ticks,
                                     uint32_t *event_mask,
                                     ps_dev_audio_play_result_t *result);

ps_status_t ps_dev_audio_stream_release_half(ps_dev_audio_t *device,
                                             uint32_t event_mask);

ps_status_t ps_dev_audio_stream_stop(ps_dev_audio_t *device,
                                     ps_status_t playback_status,
                                     ps_dev_audio_play_result_t *result);

ps_status_t ps_dev_audio_verify_idle(ps_dev_audio_t *device,
                                     ps_dev_audio_play_result_t *result);

ps_status_t ps_dev_audio_mark_post_stop_resume(ps_dev_audio_t *device);

void ps_dev_audio_record_dma_irq(void);

#ifdef __cplusplus
}
#endif

#endif /* PS_DEV_AUDIO_H */
