#ifndef PS_DEV_LS013B7DH05_H
#define PS_DEV_LS013B7DH05_H

#include <stdint.h>

#include "LS013B7DH05.h"
#include "ps_status.h"
#include "stm32u5xx_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PS_DEV_LS013B7DH05_API_VERSION (1UL)
#define PS_DEV_LS013B7DH05_WIDTH       DISPLAY_WIDTH
#define PS_DEV_LS013B7DH05_HEIGHT      DISPLAY_HEIGHT
#define PS_DEV_LS013B7DH05_LINE_WIDTH  LINE_WIDTH
#define PS_DEV_LS013B7DH05_BUFFER_SIZE BUFFER_LENGTH

typedef enum
{
  PS_DEV_LS013B7DH05_STATE_UNINITIALIZED = 0,
  PS_DEV_LS013B7DH05_STATE_READY,
  PS_DEV_LS013B7DH05_STATE_STATIC_HOLD,
  PS_DEV_LS013B7DH05_STATE_FAULT
} ps_dev_ls013b7dh05_state_t;

typedef struct
{
  uint32_t api_version;
  uint32_t initialized;
  uint32_t operation_count;
  uint32_t state;
  uint32_t last_status;
  SPI_HandleTypeDef *bus;
  LS013B7DH05 panel;
} ps_dev_ls013b7dh05_t;

typedef struct
{
  ps_status_t status;
  uint32_t init_hal_status;
  uint32_t present_hal_status;
  uint32_t dma_done;
  uint32_t spi_state_after;
  uint32_t spi_error_after;
  uint32_t dma_state_after;
  uint32_t dma_error_after;
} ps_dev_ls013b7dh05_present_result_t;

ps_status_t ps_dev_ls013b7dh05_init(ps_dev_ls013b7dh05_t *device,
                                    SPI_HandleTypeDef *bus);
ps_status_t ps_dev_ls013b7dh05_present_full_dma(
  ps_dev_ls013b7dh05_t *device,
  const uint8_t *framebuffer,
  uint32_t timeout_ms,
  DMA_HandleTypeDef *dma,
  ps_dev_ls013b7dh05_present_result_t *result);

#ifdef __cplusplus
}
#endif

#endif /* PS_DEV_LS013B7DH05_H */
