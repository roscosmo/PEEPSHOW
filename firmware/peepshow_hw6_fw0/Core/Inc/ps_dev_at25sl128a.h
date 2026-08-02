#ifndef PS_DEV_AT25SL128A_H
#define PS_DEV_AT25SL128A_H

#include <stdint.h>

#include "ps_status.h"
#include "stm32u5xx_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PS_DEV_AT25SL128A_API_VERSION (1UL)

#define PS_DEV_AT25SL128A_JEDEC_ID0 (0x1FU)
#define PS_DEV_AT25SL128A_JEDEC_ID1 (0x42U)
#define PS_DEV_AT25SL128A_JEDEC_ID2 (0x18U)
#define PS_DEV_AT25SL128A_PAGE_SIZE  (256UL)
#define PS_DEV_AT25SL128A_SECTOR_SIZE (4096UL)
#define PS_DEV_AT25SL128A_TOTAL_SIZE  (16UL * 1024UL * 1024UL)

typedef enum
{
  PS_DEV_AT25SL128A_STATE_UNINITIALIZED = 0,
  PS_DEV_AT25SL128A_STATE_READY,
  PS_DEV_AT25SL128A_STATE_ACTIVE,
  PS_DEV_AT25SL128A_STATE_DEEP_POWER_DOWN,
  PS_DEV_AT25SL128A_STATE_FAULT
} ps_dev_at25sl128a_state_t;

typedef struct
{
  uint32_t api_version;
  uint32_t initialized;
  uint32_t operation_count;
  uint32_t state;
  uint32_t last_status;
  OSPI_HandleTypeDef *ospi;
  DMA_HandleTypeDef *dma_rx;
  DMA_HandleTypeDef *dma_tx;
  uint32_t timeout_ms;
} ps_dev_at25sl128a_t;

typedef struct
{
  ps_status_t status;
  uint32_t hal_status;
  uint8_t jedec_id[3];
  uint32_t identity_match;
  uint32_t ospi_state_after;
  uint32_t ospi_error_after;
} ps_dev_at25sl128a_jedec_result_t;

typedef struct
{
  ps_status_t status;
  uint32_t hal_status;
  uint32_t ospi_state_after;
  uint32_t ospi_error_after;
} ps_dev_at25sl128a_command_result_t;

typedef struct
{
  ps_status_t status;
  uint32_t address;
  uint32_t length;
  uint32_t write_enable_hal_status;
  uint32_t write_enable_status1;
  uint32_t command_hal_status;
  uint32_t transfer_wait_status;
  uint32_t transfer_poll_count;
  uint32_t flash_wait_status;
  uint32_t flash_poll_count;
  uint32_t dma_state_after;
  uint32_t dma_error_after;
  uint32_t ospi_state_after;
  uint32_t ospi_error_after;
} ps_dev_at25sl128a_io_result_t;

typedef struct
{
  ps_status_t status;
  uint32_t address;
  uint32_t length;
  uint32_t status_read_hal_status;
  uint32_t status1_before;
  uint32_t erase_write_enable_hal_status;
  uint32_t erase_write_enable_status1;
  uint32_t erase_hal_status;
  uint32_t erase_command_status1;
  uint32_t erase_wait_status;
  uint32_t erase_poll_count;
  uint32_t erase_blank_read_hal_status;
  uint32_t erase_blank_mismatch_count;
  uint8_t erase_blank_first16[16];
  uint32_t program_write_enable_hal_status;
  uint32_t program_write_enable_status1;
  uint32_t program_hal_status;
  uint32_t program_wait_status;
  uint32_t program_poll_count;
  uint32_t program_read_hal_status;
  uint32_t program_mismatch_count;
  uint8_t program_first16[16];
  uint32_t dma_program_write_enable_hal_status;
  uint32_t dma_program_write_enable_status1;
  uint32_t dma_program_hal_status;
  uint32_t dma_program_transfer_wait_status;
  uint32_t dma_program_transfer_poll_count;
  uint32_t dma_program_flash_wait_status;
  uint32_t dma_program_flash_poll_count;
  uint32_t dma_read_hal_status;
  uint32_t dma_read_transfer_wait_status;
  uint32_t dma_read_transfer_poll_count;
  uint32_t dma_verify_mismatch_count;
  uint8_t dma_first16[16];
  uint32_t dma_tx_state_after;
  uint32_t dma_tx_error_after;
  uint32_t dma_rx_state_after;
  uint32_t dma_rx_error_after;
  uint32_t cleanup_write_enable_hal_status;
  uint32_t cleanup_write_enable_status1;
  uint32_t cleanup_erase_hal_status;
  uint32_t cleanup_wait_status;
  uint32_t cleanup_poll_count;
  uint32_t cleanup_blank_read_hal_status;
  uint32_t cleanup_blank_mismatch_count;
  uint8_t cleanup_first16[16];
  uint32_t ospi_state_after;
  uint32_t ospi_error_after;
} ps_dev_at25sl128a_scratch_result_t;

ps_status_t ps_dev_at25sl128a_init(ps_dev_at25sl128a_t *device,
                                   OSPI_HandleTypeDef *ospi,
                                   DMA_HandleTypeDef *dma_rx,
                                   DMA_HandleTypeDef *dma_tx,
                                   uint32_t timeout_ms);

ps_status_t ps_dev_at25sl128a_release_from_deep_power_down(
  ps_dev_at25sl128a_t *device,
  ps_dev_at25sl128a_command_result_t *result);

ps_status_t ps_dev_at25sl128a_read_jedec(
  ps_dev_at25sl128a_t *device,
  ps_dev_at25sl128a_jedec_result_t *result);

ps_status_t ps_dev_at25sl128a_enter_deep_power_down(
  ps_dev_at25sl128a_t *device,
  ps_dev_at25sl128a_command_result_t *result);

ps_status_t ps_dev_at25sl128a_erase_4k(
  ps_dev_at25sl128a_t *device,
  uint32_t address,
  ps_dev_at25sl128a_io_result_t *result);

ps_status_t ps_dev_at25sl128a_read(
  ps_dev_at25sl128a_t *device,
  uint32_t address,
  uint8_t *data,
  uint32_t length,
  ps_dev_at25sl128a_io_result_t *result);

ps_status_t ps_dev_at25sl128a_read_dma(
  ps_dev_at25sl128a_t *device,
  uint32_t address,
  uint8_t *data,
  uint32_t length,
  ps_dev_at25sl128a_io_result_t *result);

ps_status_t ps_dev_at25sl128a_program_page(
  ps_dev_at25sl128a_t *device,
  uint32_t address,
  const uint8_t *data,
  uint32_t length,
  ps_dev_at25sl128a_io_result_t *result);

ps_status_t ps_dev_at25sl128a_program_page_dma(
  ps_dev_at25sl128a_t *device,
  uint32_t address,
  const uint8_t *data,
  uint32_t length,
  ps_dev_at25sl128a_io_result_t *result);

ps_status_t ps_dev_at25sl128a_run_scratch_test(
  ps_dev_at25sl128a_t *device,
  uint32_t address,
  ps_dev_at25sl128a_scratch_result_t *result);

#ifdef __cplusplus
}
#endif

#endif /* PS_DEV_AT25SL128A_H */
