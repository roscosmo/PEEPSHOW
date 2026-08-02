#include "ps_dev_at25sl128a.h"

#include <string.h>

#define PS_DEV_AT25SL128A_CMD_WRITE_ENABLE       (0x06U)
#define PS_DEV_AT25SL128A_CMD_READ_STATUS1       (0x05U)
#define PS_DEV_AT25SL128A_CMD_PAGE_PROGRAM       (0x02U)
#define PS_DEV_AT25SL128A_CMD_READ_DATA          (0x03U)
#define PS_DEV_AT25SL128A_CMD_SECTOR_ERASE_4K    (0x20U)
#define PS_DEV_AT25SL128A_CMD_JEDEC_ID           (0x9FU)
#define PS_DEV_AT25SL128A_CMD_RELEASE_POWER_DOWN (0xABU)
#define PS_DEV_AT25SL128A_CMD_DEEP_POWER_DOWN    (0xB9U)

#define PS_DEV_AT25SL128A_STATUS1_BUSY_MASK      (0x01U)
#define PS_DEV_AT25SL128A_WAIT_MAX_POLLS         (20000UL)
#define PS_DEV_AT25SL128A_SCRATCH_PATTERN_BASE   (0xA5U)

static uint8_t ps_dev_at25sl128a_tx_buffer[PS_DEV_AT25SL128A_PAGE_SIZE];
static uint8_t ps_dev_at25sl128a_rx_buffer[PS_DEV_AT25SL128A_PAGE_SIZE];

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

static void ps_dev_at25sl128a_prepare_address_command(
  OSPI_RegularCmdTypeDef *command,
  uint8_t instruction,
  uint32_t address,
  uint32_t data_mode,
  uint32_t length)
{
  ps_dev_at25sl128a_prepare_command(command, instruction, data_mode, length);
  command->Address = address;
  command->AddressMode = HAL_OSPI_ADDRESS_1_LINE;
  command->AddressSize = HAL_OSPI_ADDRESS_24_BITS;
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

static HAL_StatusTypeDef ps_dev_at25sl128a_read_status1(
  ps_dev_at25sl128a_t *device,
  uint8_t *status1)
{
  OSPI_RegularCmdTypeDef command;
  HAL_StatusTypeDef hal_status;

  if (status1 == NULL)
  {
    return HAL_ERROR;
  }

  ps_dev_at25sl128a_prepare_command(&command,
                                    PS_DEV_AT25SL128A_CMD_READ_STATUS1,
                                    HAL_OSPI_DATA_1_LINE,
                                    1U);
  hal_status = HAL_OSPI_Command(device->ospi, &command, device->timeout_ms);
  if (hal_status == HAL_OK)
  {
    hal_status = HAL_OSPI_Receive(device->ospi, status1, device->timeout_ms);
  }
  return hal_status;
}

static ps_status_t ps_dev_at25sl128a_wait_while_busy(
  ps_dev_at25sl128a_t *device,
  uint32_t *wait_status,
  uint32_t *poll_count)
{
  HAL_StatusTypeDef hal_status = HAL_ERROR;
  uint8_t status1 = 0U;
  uint32_t index;

  if ((wait_status == NULL) || (poll_count == NULL))
  {
    return PS_STATUS_INVALID_ARGUMENT;
  }

  *wait_status = (uint32_t)PS_STATUS_TIMEOUT;
  *poll_count = 0UL;
  for (index = 0UL; index < PS_DEV_AT25SL128A_WAIT_MAX_POLLS; ++index)
  {
    hal_status = ps_dev_at25sl128a_read_status1(device, &status1);
    *poll_count = index + 1UL;
    if (hal_status != HAL_OK)
    {
      *wait_status = (uint32_t)ps_dev_at25sl128a_hal_status(hal_status);
      return (ps_status_t)*wait_status;
    }
    if ((status1 & PS_DEV_AT25SL128A_STATUS1_BUSY_MASK) == 0U)
    {
      *wait_status = (uint32_t)PS_STATUS_OK;
      return PS_STATUS_OK;
    }
  }
  return PS_STATUS_TIMEOUT;
}

static HAL_StatusTypeDef ps_dev_at25sl128a_erase_sector(
  ps_dev_at25sl128a_t *device,
  uint32_t address)
{
  OSPI_RegularCmdTypeDef command;

  ps_dev_at25sl128a_prepare_address_command(
    &command,
    PS_DEV_AT25SL128A_CMD_SECTOR_ERASE_4K,
    address,
    HAL_OSPI_DATA_NONE,
    0U);
  return HAL_OSPI_Command(device->ospi, &command, device->timeout_ms);
}

static HAL_StatusTypeDef ps_dev_at25sl128a_read_data(
  ps_dev_at25sl128a_t *device,
  uint32_t address,
  uint8_t *data,
  uint32_t length)
{
  OSPI_RegularCmdTypeDef command;
  HAL_StatusTypeDef hal_status;

  ps_dev_at25sl128a_prepare_address_command(
    &command,
    PS_DEV_AT25SL128A_CMD_READ_DATA,
    address,
    HAL_OSPI_DATA_1_LINE,
    length);
  hal_status = HAL_OSPI_Command(device->ospi, &command, device->timeout_ms);
  if (hal_status == HAL_OK)
  {
    hal_status = HAL_OSPI_Receive(device->ospi, data, device->timeout_ms);
  }
  return hal_status;
}

static HAL_StatusTypeDef ps_dev_at25sl128a_page_program(
  ps_dev_at25sl128a_t *device,
  uint32_t address,
  uint8_t *data,
  uint32_t length)
{
  OSPI_RegularCmdTypeDef command;
  HAL_StatusTypeDef hal_status;

  ps_dev_at25sl128a_prepare_address_command(
    &command,
    PS_DEV_AT25SL128A_CMD_PAGE_PROGRAM,
    address,
    HAL_OSPI_DATA_1_LINE,
    length);
  hal_status = HAL_OSPI_Command(device->ospi, &command, device->timeout_ms);
  if (hal_status == HAL_OK)
  {
    hal_status = HAL_OSPI_Transmit(device->ospi, data, device->timeout_ms);
  }
  return hal_status;
}

static uint32_t ps_dev_at25sl128a_count_blank_mismatches(
  const uint8_t *data,
  uint32_t length)
{
  uint32_t mismatches = 0UL;
  uint32_t index;

  for (index = 0UL; index < length; ++index)
  {
    if (data[index] != 0xFFU)
    {
      mismatches++;
    }
  }
  return mismatches;
}

static uint32_t ps_dev_at25sl128a_count_pattern_mismatches(
  const uint8_t *data,
  uint32_t length)
{
  uint32_t mismatches = 0UL;
  uint32_t index;

  for (index = 0UL; index < length; ++index)
  {
    if (data[index] !=
        (uint8_t)(PS_DEV_AT25SL128A_SCRATCH_PATTERN_BASE ^ index))
    {
      mismatches++;
    }
  }
  return mismatches;
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

ps_status_t ps_dev_at25sl128a_run_scratch_test(
  ps_dev_at25sl128a_t *device,
  uint32_t address,
  ps_dev_at25sl128a_scratch_result_t *result)
{
  HAL_StatusTypeDef hal_status;
  ps_status_t status = PS_STATUS_OK;
  uint8_t status1 = 0U;
  uint32_t index;
  uint32_t cleanup_required = 0UL;

  if ((device == NULL) || (result == NULL))
  {
    return PS_STATUS_INVALID_ARGUMENT;
  }
  (void)memset(result, 0, sizeof(*result));
  result->status = PS_STATUS_INTERNAL_ERROR;
  result->address = address;
  result->length = PS_DEV_AT25SL128A_PAGE_SIZE;
  result->status_read_hal_status = (uint32_t)HAL_ERROR;
  result->erase_write_enable_hal_status = (uint32_t)HAL_ERROR;
  result->erase_hal_status = (uint32_t)HAL_ERROR;
  result->erase_wait_status = (uint32_t)PS_STATUS_NOT_INITIALIZED;
  result->erase_blank_read_hal_status = (uint32_t)HAL_ERROR;
  result->program_write_enable_hal_status = (uint32_t)HAL_ERROR;
  result->program_hal_status = (uint32_t)HAL_ERROR;
  result->program_wait_status = (uint32_t)PS_STATUS_NOT_INITIALIZED;
  result->program_read_hal_status = (uint32_t)HAL_ERROR;
  result->cleanup_write_enable_hal_status = (uint32_t)HAL_ERROR;
  result->cleanup_erase_hal_status = (uint32_t)HAL_ERROR;
  result->cleanup_wait_status = (uint32_t)PS_STATUS_NOT_INITIALIZED;
  result->cleanup_blank_read_hal_status = (uint32_t)HAL_ERROR;

  if (device->initialized == 0U)
  {
    result->status = PS_STATUS_NOT_INITIALIZED;
    return result->status;
  }

  device->operation_count++;
  device->state = PS_DEV_AT25SL128A_STATE_ACTIVE;

  hal_status = ps_dev_at25sl128a_read_status1(device, &status1);
  result->status_read_hal_status = (uint32_t)hal_status;
  result->status1_before = status1;
  if (hal_status != HAL_OK)
  {
    status = ps_dev_at25sl128a_hal_status(hal_status);
    goto scratch_done;
  }

  hal_status = ps_dev_at25sl128a_send_command(
    device,
    PS_DEV_AT25SL128A_CMD_WRITE_ENABLE);
  result->erase_write_enable_hal_status = (uint32_t)hal_status;
  if (hal_status != HAL_OK)
  {
    status = ps_dev_at25sl128a_hal_status(hal_status);
    goto scratch_done;
  }

  hal_status = ps_dev_at25sl128a_erase_sector(device, address);
  result->erase_hal_status = (uint32_t)hal_status;
  if (hal_status != HAL_OK)
  {
    status = ps_dev_at25sl128a_hal_status(hal_status);
    goto scratch_done;
  }

  cleanup_required = 1UL;

  status = ps_dev_at25sl128a_wait_while_busy(
    device,
    &result->erase_wait_status,
    &result->erase_poll_count);
  if (status != PS_STATUS_OK)
  {
    goto cleanup;
  }

  hal_status = ps_dev_at25sl128a_read_data(
    device,
    address,
    ps_dev_at25sl128a_rx_buffer,
    PS_DEV_AT25SL128A_PAGE_SIZE);
  result->erase_blank_read_hal_status = (uint32_t)hal_status;
  if (hal_status != HAL_OK)
  {
    status = ps_dev_at25sl128a_hal_status(hal_status);
    goto cleanup;
  }
  result->erase_blank_mismatch_count =
    ps_dev_at25sl128a_count_blank_mismatches(
      ps_dev_at25sl128a_rx_buffer,
      PS_DEV_AT25SL128A_PAGE_SIZE);
  if (result->erase_blank_mismatch_count != 0UL)
  {
    status = PS_STATUS_VERIFY_FAILED;
    goto cleanup;
  }

  for (index = 0UL; index < PS_DEV_AT25SL128A_PAGE_SIZE; ++index)
  {
    ps_dev_at25sl128a_tx_buffer[index] =
      (uint8_t)(PS_DEV_AT25SL128A_SCRATCH_PATTERN_BASE ^ index);
  }

  hal_status = ps_dev_at25sl128a_send_command(
    device,
    PS_DEV_AT25SL128A_CMD_WRITE_ENABLE);
  result->program_write_enable_hal_status = (uint32_t)hal_status;
  if (hal_status != HAL_OK)
  {
    status = ps_dev_at25sl128a_hal_status(hal_status);
    goto cleanup;
  }

  hal_status = ps_dev_at25sl128a_page_program(
    device,
    address,
    ps_dev_at25sl128a_tx_buffer,
    PS_DEV_AT25SL128A_PAGE_SIZE);
  result->program_hal_status = (uint32_t)hal_status;
  if (hal_status != HAL_OK)
  {
    status = ps_dev_at25sl128a_hal_status(hal_status);
    goto cleanup;
  }

  status = ps_dev_at25sl128a_wait_while_busy(
    device,
    &result->program_wait_status,
    &result->program_poll_count);
  if (status != PS_STATUS_OK)
  {
    goto cleanup;
  }

  hal_status = ps_dev_at25sl128a_read_data(
    device,
    address,
    ps_dev_at25sl128a_rx_buffer,
    PS_DEV_AT25SL128A_PAGE_SIZE);
  result->program_read_hal_status = (uint32_t)hal_status;
  if (hal_status != HAL_OK)
  {
    status = ps_dev_at25sl128a_hal_status(hal_status);
    goto cleanup;
  }
  for (index = 0UL; index < 16UL; ++index)
  {
    result->program_first16[index] = ps_dev_at25sl128a_rx_buffer[index];
  }
  result->program_mismatch_count =
    ps_dev_at25sl128a_count_pattern_mismatches(
      ps_dev_at25sl128a_rx_buffer,
      PS_DEV_AT25SL128A_PAGE_SIZE);
  if (result->program_mismatch_count != 0UL)
  {
    status = PS_STATUS_VERIFY_FAILED;
    goto cleanup;
  }

cleanup:
  if (cleanup_required == 0UL)
  {
    goto scratch_done;
  }

  hal_status = ps_dev_at25sl128a_send_command(
    device,
    PS_DEV_AT25SL128A_CMD_WRITE_ENABLE);
  result->cleanup_write_enable_hal_status = (uint32_t)hal_status;
  if ((hal_status != HAL_OK) && (status == PS_STATUS_OK))
  {
    status = ps_dev_at25sl128a_hal_status(hal_status);
    goto scratch_done;
  }
  if (hal_status == HAL_OK)
  {
    hal_status = ps_dev_at25sl128a_erase_sector(device, address);
    result->cleanup_erase_hal_status = (uint32_t)hal_status;
    if ((hal_status != HAL_OK) && (status == PS_STATUS_OK))
    {
      status = ps_dev_at25sl128a_hal_status(hal_status);
      goto scratch_done;
    }
  }
  if (hal_status == HAL_OK)
  {
    ps_status_t cleanup_status = ps_dev_at25sl128a_wait_while_busy(
      device,
      &result->cleanup_wait_status,
      &result->cleanup_poll_count);
    if ((cleanup_status != PS_STATUS_OK) && (status == PS_STATUS_OK))
    {
      status = cleanup_status;
      goto scratch_done;
    }
  }
  if (hal_status == HAL_OK)
  {
    hal_status = ps_dev_at25sl128a_read_data(
      device,
      address,
      ps_dev_at25sl128a_rx_buffer,
      PS_DEV_AT25SL128A_PAGE_SIZE);
    result->cleanup_blank_read_hal_status = (uint32_t)hal_status;
    if ((hal_status != HAL_OK) && (status == PS_STATUS_OK))
    {
      status = ps_dev_at25sl128a_hal_status(hal_status);
      goto scratch_done;
    }
    if (hal_status == HAL_OK)
    {
      for (index = 0UL; index < 16UL; ++index)
      {
        result->cleanup_first16[index] = ps_dev_at25sl128a_rx_buffer[index];
      }
      result->cleanup_blank_mismatch_count =
        ps_dev_at25sl128a_count_blank_mismatches(
          ps_dev_at25sl128a_rx_buffer,
          PS_DEV_AT25SL128A_PAGE_SIZE);
      if ((result->cleanup_blank_mismatch_count != 0UL) &&
          (status == PS_STATUS_OK))
      {
        status = PS_STATUS_VERIFY_FAILED;
      }
    }
  }

scratch_done:
  result->ospi_state_after = (uint32_t)HAL_OSPI_GetState(device->ospi);
  result->ospi_error_after = HAL_OSPI_GetError(device->ospi);
  if ((status == PS_STATUS_OK) &&
      (result->ospi_error_after != HAL_OSPI_ERROR_NONE))
  {
    status = PS_STATUS_VERIFY_FAILED;
  }
  result->status = ps_dev_at25sl128a_finish(device, status);
  return result->status;
}
