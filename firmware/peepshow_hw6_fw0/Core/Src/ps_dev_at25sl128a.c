#include "ps_dev_at25sl128a.h"

#include <string.h>

#define PS_DEV_AT25SL128A_CMD_JEDEC_ID          (0x9FU)
#define PS_DEV_AT25SL128A_CMD_RELEASE_POWER_DOWN (0xABU)
#define PS_DEV_AT25SL128A_CMD_DEEP_POWER_DOWN   (0xB9U)

static ps_status_t ps_dev_at25sl128a_hal_status(HAL_StatusTypeDef status)
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

static void ps_dev_at25sl128a_prepare_command(
  OSPI_RegularCmdTypeDef *command,
  uint8_t instruction,
  uint32_t data_mode,
  uint32_t length)
{
  (void)memset(command, 0, sizeof(*command));
  command->OperationType = HAL_OSPI_OPTYPE_COMMON_CFG;
  command->FlashId = HAL_OSPI_FLASH_ID_1;
  command->Instruction = instruction;
  command->InstructionMode = HAL_OSPI_INSTRUCTION_1_LINE;
  command->InstructionSize = HAL_OSPI_INSTRUCTION_8_BITS;
  command->InstructionDtrMode = HAL_OSPI_INSTRUCTION_DTR_DISABLE;
  command->AddressMode = HAL_OSPI_ADDRESS_NONE;
  command->AddressDtrMode = HAL_OSPI_ADDRESS_DTR_DISABLE;
  command->AlternateBytesMode = HAL_OSPI_ALTERNATE_BYTES_NONE;
  command->AlternateBytesDtrMode = HAL_OSPI_ALTERNATE_BYTES_DTR_DISABLE;
  command->DataMode = data_mode;
  command->NbData = length;
  command->DataDtrMode = HAL_OSPI_DATA_DTR_DISABLE;
  command->DummyCycles = 0U;
  command->DQSMode = HAL_OSPI_DQS_DISABLE;
  command->SIOOMode = HAL_OSPI_SIOO_INST_EVERY_CMD;
}

static ps_status_t ps_dev_at25sl128a_finish(
  ps_dev_at25sl128a_t *device,
  ps_status_t status)
{
  device->last_status = (uint32_t)status;
  if (status != PS_STATUS_OK)
  {
    device->state = PS_DEV_AT25SL128A_STATE_FAULT;
  }
  return status;
}

static HAL_StatusTypeDef ps_dev_at25sl128a_send_command(
  ps_dev_at25sl128a_t *device,
  uint8_t instruction)
{
  OSPI_RegularCmdTypeDef command;

  ps_dev_at25sl128a_prepare_command(&command, instruction,
                                    HAL_OSPI_DATA_NONE, 0U);
  return HAL_OSPI_Command(device->ospi, &command, device->timeout_ms);
}

ps_status_t ps_dev_at25sl128a_init(ps_dev_at25sl128a_t *device,
                                   OSPI_HandleTypeDef *ospi,
                                   uint32_t timeout_ms)
{
  if ((device == NULL) || (ospi == NULL) || (timeout_ms == 0UL))
  {
    return PS_STATUS_INVALID_ARGUMENT;
  }

  (void)memset(device, 0, sizeof(*device));
  device->api_version = PS_DEV_AT25SL128A_API_VERSION;
  device->ospi = ospi;
  device->timeout_ms = timeout_ms;
  device->state = PS_DEV_AT25SL128A_STATE_READY;
  device->last_status = PS_STATUS_OK;
  device->initialized = 1U;
  return PS_STATUS_OK;
}

ps_status_t ps_dev_at25sl128a_release_from_deep_power_down(
  ps_dev_at25sl128a_t *device,
  ps_dev_at25sl128a_command_result_t *result)
{
  HAL_StatusTypeDef hal_status;
  ps_status_t status;

  if ((device == NULL) || (result == NULL))
  {
    return PS_STATUS_INVALID_ARGUMENT;
  }
  (void)memset(result, 0, sizeof(*result));
  result->status = PS_STATUS_INTERNAL_ERROR;
  result->hal_status = (uint32_t)HAL_ERROR;
  if (device->initialized == 0U)
  {
    result->status = PS_STATUS_NOT_INITIALIZED;
    return result->status;
  }

  device->operation_count++;
  hal_status = ps_dev_at25sl128a_send_command(
    device,
    PS_DEV_AT25SL128A_CMD_RELEASE_POWER_DOWN);
  result->hal_status = (uint32_t)hal_status;
  result->ospi_state_after = (uint32_t)HAL_OSPI_GetState(device->ospi);
  result->ospi_error_after = HAL_OSPI_GetError(device->ospi);

  status = ps_dev_at25sl128a_hal_status(hal_status);
  if (status == PS_STATUS_OK)
  {
    device->state = PS_DEV_AT25SL128A_STATE_ACTIVE;
  }
  result->status = ps_dev_at25sl128a_finish(device, status);
  return result->status;
}

ps_status_t ps_dev_at25sl128a_read_jedec(
  ps_dev_at25sl128a_t *device,
  ps_dev_at25sl128a_jedec_result_t *result)
{
  OSPI_RegularCmdTypeDef command;
  HAL_StatusTypeDef hal_status;
  ps_status_t status;

  if ((device == NULL) || (result == NULL))
  {
    return PS_STATUS_INVALID_ARGUMENT;
  }
  (void)memset(result, 0, sizeof(*result));
  result->status = PS_STATUS_INTERNAL_ERROR;
  result->hal_status = (uint32_t)HAL_ERROR;
  if (device->initialized == 0U)
  {
    result->status = PS_STATUS_NOT_INITIALIZED;
    return result->status;
  }

  device->operation_count++;
  ps_dev_at25sl128a_prepare_command(&command,
                                    PS_DEV_AT25SL128A_CMD_JEDEC_ID,
                                    HAL_OSPI_DATA_1_LINE,
                                    sizeof(result->jedec_id));
  hal_status = HAL_OSPI_Command(device->ospi, &command, device->timeout_ms);
  if (hal_status == HAL_OK)
  {
    hal_status = HAL_OSPI_Receive(device->ospi,
                                  result->jedec_id,
                                  device->timeout_ms);
  }
  result->hal_status = (uint32_t)hal_status;
  result->identity_match =
    ((hal_status == HAL_OK) &&
     (result->jedec_id[0] == PS_DEV_AT25SL128A_JEDEC_ID0) &&
     (result->jedec_id[1] == PS_DEV_AT25SL128A_JEDEC_ID1) &&
     (result->jedec_id[2] == PS_DEV_AT25SL128A_JEDEC_ID2)) ? 1UL : 0UL;
  result->ospi_state_after = (uint32_t)HAL_OSPI_GetState(device->ospi);
  result->ospi_error_after = HAL_OSPI_GetError(device->ospi);

  status = ps_dev_at25sl128a_hal_status(hal_status);
  if ((status == PS_STATUS_OK) && (result->identity_match == 0UL))
  {
    status = PS_STATUS_IDENTITY_MISMATCH;
  }
  if (status == PS_STATUS_OK)
  {
    device->state = PS_DEV_AT25SL128A_STATE_ACTIVE;
  }
  result->status = ps_dev_at25sl128a_finish(device, status);
  return result->status;
}

ps_status_t ps_dev_at25sl128a_enter_deep_power_down(
  ps_dev_at25sl128a_t *device,
  ps_dev_at25sl128a_command_result_t *result)
{
  HAL_StatusTypeDef hal_status;
  ps_status_t status;

  if ((device == NULL) || (result == NULL))
  {
    return PS_STATUS_INVALID_ARGUMENT;
  }
  (void)memset(result, 0, sizeof(*result));
  result->status = PS_STATUS_INTERNAL_ERROR;
  result->hal_status = (uint32_t)HAL_ERROR;
  if (device->initialized == 0U)
  {
    result->status = PS_STATUS_NOT_INITIALIZED;
    return result->status;
  }

  device->operation_count++;
  hal_status = ps_dev_at25sl128a_send_command(
    device,
    PS_DEV_AT25SL128A_CMD_DEEP_POWER_DOWN);
  result->hal_status = (uint32_t)hal_status;
  result->ospi_state_after = (uint32_t)HAL_OSPI_GetState(device->ospi);
  result->ospi_error_after = HAL_OSPI_GetError(device->ospi);

  status = ps_dev_at25sl128a_hal_status(hal_status);
  if (status == PS_STATUS_OK)
  {
    device->state = PS_DEV_AT25SL128A_STATE_DEEP_POWER_DOWN;
  }
  result->status = ps_dev_at25sl128a_finish(device, status);
  return result->status;
}
