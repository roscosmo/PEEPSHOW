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

ps_status_t ps_dev_at25sl128a_init(ps_dev_at25sl128a_t *device,
                                   OSPI_HandleTypeDef *ospi,
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

#ifdef __cplusplus
}
#endif

#endif /* PS_DEV_AT25SL128A_H */
