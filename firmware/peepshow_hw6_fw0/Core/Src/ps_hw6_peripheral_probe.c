#include "ps_hw6_peripheral_probe.h"

#include "main.h"

#include <stddef.h>
#include <string.h>

#define PS_HW6_PERIPHERAL_PROBE_MAGIC          0x48365052UL
#define PS_HW6_PERIPHERAL_PROBE_VERSION        0x00000002UL
#define PS_HW6_PERIPHERAL_PROBE_PHASE_START    0x00006500UL
#define PS_HW6_PERIPHERAL_PROBE_PHASE_PMIC     0x00006501UL
#define PS_HW6_PERIPHERAL_PROBE_PHASE_IMU      0x00006502UL
#define PS_HW6_PERIPHERAL_PROBE_PHASE_JOYSTICK 0x00006503UL
#define PS_HW6_PERIPHERAL_PROBE_PHASE_FLASH    0x00006504UL
#define PS_HW6_PERIPHERAL_PROBE_PHASE_NINA     0x00006505UL
#define PS_HW6_PERIPHERAL_PROBE_PHASE_COMPLETE 0x000065FFUL

#define PS_HW6_PMIC_ADDRESS_7BIT         0x46U
#define PS_HW6_PMIC_REG_ID               0x00U
#define PS_HW6_PMIC_REG_FAULT            0x2EU
#define PS_HW6_PMIC_REG_PGOOD            0x2FU
#define PS_HW6_PMIC_EXPECTED_ID          0x10U
#define PS_HW6_PMIC_RAIL_PGOOD_MASK      0x03U

#define PS_HW6_IMU_ADDRESS_7BIT          0x18U
#define PS_HW6_IMU_ALT_ADDRESS_7BIT      0x19U
#define PS_HW6_IMU_REG_WHO_AM_I          0x0FU
#define PS_HW6_IMU_EXPECTED_WHO_AM_I     0x47U

#define PS_HW6_JOYSTICK_ADDRESS_7BIT     0x34U
#define PS_HW6_JOYSTICK_REG_DEVICE_ID    0x0DU
#define PS_HW6_JOYSTICK_REG_MFR_LSB      0x0EU
#define PS_HW6_JOYSTICK_REG_MFR_MSB      0x0FU
#define PS_HW6_JOYSTICK_EXPECTED_MFR_LSB 0x49U
#define PS_HW6_JOYSTICK_EXPECTED_MFR_MSB 0x54U

#define PS_HW6_FLASH_CMD_READ_JEDEC_ID   0x9FU
#define PS_HW6_FLASH_CMD_READ_STATUS1    0x05U
#define PS_HW6_FLASH_CMD_READ_STATUS2    0x35U
#define PS_HW6_FLASH_CMD_READ_STATUS3    0x15U
#define PS_HW6_FLASH_EXPECTED_ID0        0x1FU
#define PS_HW6_FLASH_EXPECTED_ID1        0x42U
#define PS_HW6_FLASH_EXPECTED_ID2        0x18U

#define PS_HW6_I2C_TIMEOUT_MS            50U
#define PS_HW6_OSPI_TIMEOUT_MS           100U
#define PS_HW6_NINA_RESET_ASSERT_MS      20U
#define PS_HW6_NINA_BOOT_WAIT_MS         750U
#define PS_HW6_NINA_BOOT_DRAIN_MS        100U
#define PS_HW6_NINA_RX_WINDOW_MS         500U
#define PS_HW6_NINA_RX_BYTE_TIMEOUT_MS   10U
#define PS_HW6_NINA_TX_TIMEOUT_MS        250U
#define PS_HW6_NINA_MAX_ATTEMPTS         2U

extern I2C_HandleTypeDef hi2c3;
extern UART_HandleTypeDef hlpuart1;
extern OSPI_HandleTypeDef hospi1;
extern RTC_HandleTypeDef hrtc;
extern SAI_HandleTypeDef hsai_BlockA1;
extern LPTIM_HandleTypeDef hlptim1;
extern SPI_HandleTypeDef hspi3;
extern PCD_HandleTypeDef hpcd_USB_OTG_FS;

volatile PS_HW6_PeripheralProbe g_ps_hw6_peripheral_probe;

static HAL_StatusTypeDef PS_HW6_ReadI2CRegister(uint8_t address_7bit,
                                                uint8_t reg,
                                                uint8_t *value)
{
  return HAL_I2C_Mem_Read(&hi2c3,
                          (uint16_t)(address_7bit << 1),
                          reg,
                          I2C_MEMADD_SIZE_8BIT,
                          value,
                          1U,
                          PS_HW6_I2C_TIMEOUT_MS);
}

static HAL_StatusTypeDef PS_HW6_ReadFlash(uint8_t instruction,
                                          uint8_t *data,
                                          uint32_t length)
{
  OSPI_RegularCmdTypeDef command = {0};
  HAL_StatusTypeDef status;

  if ((data == NULL) || (length == 0U))
  {
    return HAL_ERROR;
  }

  command.OperationType = HAL_OSPI_OPTYPE_COMMON_CFG;
  command.FlashId = HAL_OSPI_FLASH_ID_1;
  command.Instruction = instruction;
  command.InstructionMode = HAL_OSPI_INSTRUCTION_1_LINE;
  command.InstructionSize = HAL_OSPI_INSTRUCTION_8_BITS;
  command.InstructionDtrMode = HAL_OSPI_INSTRUCTION_DTR_DISABLE;
  command.AddressMode = HAL_OSPI_ADDRESS_NONE;
  command.AddressDtrMode = HAL_OSPI_ADDRESS_DTR_DISABLE;
  command.AlternateBytesMode = HAL_OSPI_ALTERNATE_BYTES_NONE;
  command.AlternateBytesDtrMode = HAL_OSPI_ALTERNATE_BYTES_DTR_DISABLE;
  command.DataMode = HAL_OSPI_DATA_1_LINE;
  command.NbData = length;
  command.DataDtrMode = HAL_OSPI_DATA_DTR_DISABLE;
  command.DummyCycles = 0U;
  command.DQSMode = HAL_OSPI_DQS_DISABLE;
  command.SIOOMode = HAL_OSPI_SIOO_INST_EVERY_CMD;

  status = HAL_OSPI_Command(&hospi1, &command, PS_HW6_OSPI_TIMEOUT_MS);
  if (status != HAL_OK)
  {
    return status;
  }

  return HAL_OSPI_Receive(&hospi1, data, PS_HW6_OSPI_TIMEOUT_MS);
}

static uint32_t PS_HW6_ReceiveWindow(uint8_t *buffer,
                                     uint32_t capacity,
                                     uint32_t window_ms)
{
  uint32_t count = 0U;
  const uint32_t start_tick = HAL_GetTick();

  while (((uint32_t)(HAL_GetTick() - start_tick) < window_ms) &&
         (count < capacity))
  {
    uint8_t byte = 0U;

    if (HAL_UART_Receive(&hlpuart1,
                         &byte,
                         1U,
                         PS_HW6_NINA_RX_BYTE_TIMEOUT_MS) == HAL_OK)
    {
      buffer[count] = byte;
      count++;
    }
  }

  return count;
}

static uint32_t PS_HW6_BufferContains(const uint8_t *buffer,
                                      uint32_t length,
                                      const char *needle)
{
  const uint32_t needle_length = (uint32_t)strlen(needle);

  if ((needle_length == 0U) || (needle_length > length))
  {
    return 0U;
  }

  for (uint32_t start = 0U; start <= (length - needle_length); start++)
  {
    uint32_t match = 1U;

    for (uint32_t index = 0U; index < needle_length; index++)
    {
      if (buffer[start + index] != (uint8_t)needle[index])
      {
        match = 0U;
        break;
      }
    }

    if (match != 0U)
    {
      return 1U;
    }
  }

  return 0U;
}

static void PS_HW6_CopyNinaResponse(const uint8_t *buffer, uint32_t length)
{
  volatile PS_HW6_PeripheralProbe *probe = &g_ps_hw6_peripheral_probe;
  uint32_t copy_length = length;

  if (copy_length >= sizeof(probe->nina_at_rx))
  {
    copy_length = sizeof(probe->nina_at_rx) - 1U;
  }

  for (uint32_t index = 0U; index < copy_length; index++)
  {
    probe->nina_at_rx[index] = (char)buffer[index];
  }
  probe->nina_at_rx[copy_length] = '\0';
}

static void PS_HW6_ProbePmic(void)
{
  volatile PS_HW6_PeripheralProbe *probe = &g_ps_hw6_peripheral_probe;
  uint8_t value = 0U;

  probe->phase = PS_HW6_PERIPHERAL_PROBE_PHASE_PMIC;
  probe->attempted_mask |= PS_HW6_PERIPHERAL_PMIC;
  probe->pmic_address_7bit = PS_HW6_PMIC_ADDRESS_7BIT;
  probe->pmic_ready_status =
      (uint32_t)HAL_I2C_IsDeviceReady(&hi2c3,
                                      (uint16_t)(PS_HW6_PMIC_ADDRESS_7BIT << 1),
                                      2U,
                                      PS_HW6_I2C_TIMEOUT_MS);

  probe->pmic_id_status =
      (uint32_t)PS_HW6_ReadI2CRegister(PS_HW6_PMIC_ADDRESS_7BIT,
                                      PS_HW6_PMIC_REG_ID,
                                      &value);
  probe->pmic_id = value;

  value = 0U;
  probe->pmic_fault_status =
      (uint32_t)PS_HW6_ReadI2CRegister(PS_HW6_PMIC_ADDRESS_7BIT,
                                      PS_HW6_PMIC_REG_FAULT,
                                      &value);
  probe->pmic_fault = value;

  value = 0U;
  probe->pmic_pgood_status =
      (uint32_t)PS_HW6_ReadI2CRegister(PS_HW6_PMIC_ADDRESS_7BIT,
                                      PS_HW6_PMIC_REG_PGOOD,
                                      &value);
  probe->pmic_pgood = value;
  probe->pmic_identity_match =
      (probe->pmic_id == PS_HW6_PMIC_EXPECTED_ID) ? 1U : 0U;
  probe->pmic_rails_ready =
      (((probe->pmic_pgood & PS_HW6_PMIC_RAIL_PGOOD_MASK) ==
        PS_HW6_PMIC_RAIL_PGOOD_MASK) &&
       (probe->pmic_fault == 0U)) ? 1U : 0U;

  if ((probe->pmic_ready_status == (uint32_t)HAL_OK) &&
      (probe->pmic_id_status == (uint32_t)HAL_OK) &&
      (probe->pmic_fault_status == (uint32_t)HAL_OK) &&
      (probe->pmic_pgood_status == (uint32_t)HAL_OK) &&
      (probe->pmic_identity_match != 0U) &&
      (probe->pmic_rails_ready != 0U))
  {
    probe->pass_mask |= PS_HW6_PERIPHERAL_PMIC;
  }
}

static void PS_HW6_ProbeImu(void)
{
  volatile PS_HW6_PeripheralProbe *probe = &g_ps_hw6_peripheral_probe;
  uint8_t value = 0U;
  uint32_t primary_match;
  uint32_t alternate_match;

  probe->phase = PS_HW6_PERIPHERAL_PROBE_PHASE_IMU;
  probe->attempted_mask |= PS_HW6_PERIPHERAL_IMU;
  probe->imu_address_7bit = PS_HW6_IMU_ADDRESS_7BIT;
  probe->imu_ready_status =
      (uint32_t)HAL_I2C_IsDeviceReady(&hi2c3,
                                      (uint16_t)(PS_HW6_IMU_ADDRESS_7BIT << 1),
                                      2U,
                                      PS_HW6_I2C_TIMEOUT_MS);
  probe->imu_ready_error = HAL_I2C_GetError(&hi2c3);
  probe->imu_whoami_status =
      (uint32_t)PS_HW6_ReadI2CRegister(PS_HW6_IMU_ADDRESS_7BIT,
                                      PS_HW6_IMU_REG_WHO_AM_I,
                                      &value);
  probe->imu_whoami_error = HAL_I2C_GetError(&hi2c3);
  probe->imu_whoami = value;
  primary_match =
      ((probe->imu_ready_status == (uint32_t)HAL_OK) &&
       (probe->imu_whoami_status == (uint32_t)HAL_OK) &&
       (probe->imu_whoami == PS_HW6_IMU_EXPECTED_WHO_AM_I)) ? 1U : 0U;

  value = 0U;
  probe->imu_alt_address_7bit = PS_HW6_IMU_ALT_ADDRESS_7BIT;
  probe->imu_alt_ready_status =
      (uint32_t)HAL_I2C_IsDeviceReady(&hi2c3,
                                      (uint16_t)(PS_HW6_IMU_ALT_ADDRESS_7BIT << 1),
                                      2U,
                                      PS_HW6_I2C_TIMEOUT_MS);
  probe->imu_alt_ready_error = HAL_I2C_GetError(&hi2c3);
  probe->imu_alt_whoami_status =
      (uint32_t)PS_HW6_ReadI2CRegister(PS_HW6_IMU_ALT_ADDRESS_7BIT,
                                      PS_HW6_IMU_REG_WHO_AM_I,
                                      &value);
  probe->imu_alt_whoami_error = HAL_I2C_GetError(&hi2c3);
  probe->imu_alt_whoami = value;
  alternate_match =
      ((probe->imu_alt_ready_status == (uint32_t)HAL_OK) &&
       (probe->imu_alt_whoami_status == (uint32_t)HAL_OK) &&
       (probe->imu_alt_whoami == PS_HW6_IMU_EXPECTED_WHO_AM_I)) ? 1U : 0U;

  probe->imu_detected_address_7bit =
      (primary_match != 0U) ? PS_HW6_IMU_ADDRESS_7BIT :
      ((alternate_match != 0U) ? PS_HW6_IMU_ALT_ADDRESS_7BIT : 0U);
  probe->imu_identity_match =
      ((primary_match != 0U) || (alternate_match != 0U)) ? 1U : 0U;

  if (probe->imu_identity_match != 0U)
  {
    probe->pass_mask |= PS_HW6_PERIPHERAL_IMU;
  }
}

static void PS_HW6_ProbeJoystick(void)
{
  volatile PS_HW6_PeripheralProbe *probe = &g_ps_hw6_peripheral_probe;
  uint8_t value = 0U;

  probe->phase = PS_HW6_PERIPHERAL_PROBE_PHASE_JOYSTICK;
  probe->attempted_mask |= PS_HW6_PERIPHERAL_JOYSTICK;
  probe->joystick_address_7bit = PS_HW6_JOYSTICK_ADDRESS_7BIT;
  probe->joystick_ready_status =
      (uint32_t)HAL_I2C_IsDeviceReady(
          &hi2c3,
          (uint16_t)(PS_HW6_JOYSTICK_ADDRESS_7BIT << 1),
          2U,
          PS_HW6_I2C_TIMEOUT_MS);

  probe->joystick_device_id_status =
      (uint32_t)PS_HW6_ReadI2CRegister(PS_HW6_JOYSTICK_ADDRESS_7BIT,
                                      PS_HW6_JOYSTICK_REG_DEVICE_ID,
                                      &value);
  probe->joystick_device_id = value;

  value = 0U;
  probe->joystick_manufacturer_lsb_status =
      (uint32_t)PS_HW6_ReadI2CRegister(PS_HW6_JOYSTICK_ADDRESS_7BIT,
                                      PS_HW6_JOYSTICK_REG_MFR_LSB,
                                      &value);
  probe->joystick_manufacturer_lsb = value;

  value = 0U;
  probe->joystick_manufacturer_msb_status =
      (uint32_t)PS_HW6_ReadI2CRegister(PS_HW6_JOYSTICK_ADDRESS_7BIT,
                                      PS_HW6_JOYSTICK_REG_MFR_MSB,
                                      &value);
  probe->joystick_manufacturer_msb = value;
  probe->joystick_identity_match =
      ((probe->joystick_manufacturer_lsb ==
        PS_HW6_JOYSTICK_EXPECTED_MFR_LSB) &&
       (probe->joystick_manufacturer_msb ==
        PS_HW6_JOYSTICK_EXPECTED_MFR_MSB)) ? 1U : 0U;

  if ((probe->joystick_ready_status == (uint32_t)HAL_OK) &&
      (probe->joystick_device_id_status == (uint32_t)HAL_OK) &&
      (probe->joystick_manufacturer_lsb_status == (uint32_t)HAL_OK) &&
      (probe->joystick_manufacturer_msb_status == (uint32_t)HAL_OK) &&
      (probe->joystick_identity_match != 0U))
  {
    probe->pass_mask |= PS_HW6_PERIPHERAL_JOYSTICK;
  }
}

static void PS_HW6_ProbeFlash(void)
{
  volatile PS_HW6_PeripheralProbe *probe = &g_ps_hw6_peripheral_probe;
  uint8_t jedec_id[3] = {0U, 0U, 0U};
  uint8_t status[3] = {0U, 0U, 0U};

  probe->phase = PS_HW6_PERIPHERAL_PROBE_PHASE_FLASH;
  probe->attempted_mask |= PS_HW6_PERIPHERAL_FLASH;
  probe->flash_ospi_state_before = HAL_OSPI_GetState(&hospi1);
  probe->flash_jedec_status =
      (uint32_t)PS_HW6_ReadFlash(PS_HW6_FLASH_CMD_READ_JEDEC_ID,
                                 jedec_id,
                                 sizeof(jedec_id));
  probe->flash_status1_status =
      (uint32_t)PS_HW6_ReadFlash(PS_HW6_FLASH_CMD_READ_STATUS1,
                                 &status[0],
                                 1U);
  probe->flash_status2_status =
      (uint32_t)PS_HW6_ReadFlash(PS_HW6_FLASH_CMD_READ_STATUS2,
                                 &status[1],
                                 1U);
  probe->flash_status3_status =
      (uint32_t)PS_HW6_ReadFlash(PS_HW6_FLASH_CMD_READ_STATUS3,
                                 &status[2],
                                 1U);

  for (uint32_t index = 0U; index < 3U; index++)
  {
    probe->flash_jedec_id[index] = jedec_id[index];
    probe->flash_status[index] = status[index];
  }

  probe->flash_identity_match =
      ((jedec_id[0] == PS_HW6_FLASH_EXPECTED_ID0) &&
       (jedec_id[1] == PS_HW6_FLASH_EXPECTED_ID1) &&
       (jedec_id[2] == PS_HW6_FLASH_EXPECTED_ID2)) ? 1U : 0U;
  probe->flash_ospi_state_after = HAL_OSPI_GetState(&hospi1);
  probe->flash_ospi_error_after = HAL_OSPI_GetError(&hospi1);

  if ((probe->flash_jedec_status == (uint32_t)HAL_OK) &&
      (probe->flash_status1_status == (uint32_t)HAL_OK) &&
      (probe->flash_status2_status == (uint32_t)HAL_OK) &&
      (probe->flash_status3_status == (uint32_t)HAL_OK) &&
      (probe->flash_identity_match != 0U) &&
      (probe->flash_ospi_error_after == HAL_OSPI_ERROR_NONE))
  {
    probe->pass_mask |= PS_HW6_PERIPHERAL_FLASH;
  }
}

static void PS_HW6_ProbeNina(void)
{
  volatile PS_HW6_PeripheralProbe *probe = &g_ps_hw6_peripheral_probe;
  static const uint8_t at_command[] = "AT\r\n";
  uint8_t rx[sizeof(probe->nina_at_rx)] = {0};
  uint8_t boot_rx[16] = {0};

  probe->phase = PS_HW6_PERIPHERAL_PROBE_PHASE_NINA;
  probe->attempted_mask |= PS_HW6_PERIPHERAL_NINA;
  probe->nina_uart_state_before = (uint32_t)HAL_UART_GetState(&hlpuart1);
  probe->nina_uart_error_before = HAL_UART_GetError(&hlpuart1);
  probe->nina_nrst_before =
      (uint32_t)HAL_GPIO_ReadPin(NINA_NRST_GPIO_Port, NINA_NRST_Pin);

  HAL_GPIO_WritePin(NINA_NRST_GPIO_Port, NINA_NRST_Pin, GPIO_PIN_RESET);
  HAL_Delay(PS_HW6_NINA_RESET_ASSERT_MS);
  HAL_GPIO_WritePin(NINA_NRST_GPIO_Port, NINA_NRST_Pin, GPIO_PIN_SET);
  probe->nina_release_tick = HAL_GetTick();
  probe->nina_nrst_released =
      (uint32_t)HAL_GPIO_ReadPin(NINA_NRST_GPIO_Port, NINA_NRST_Pin);
  probe->nina_boot_wait_ms = PS_HW6_NINA_BOOT_WAIT_MS;
  HAL_Delay(PS_HW6_NINA_BOOT_WAIT_MS);
  probe->nina_boot_rx_len =
      PS_HW6_ReceiveWindow(boot_rx,
                           sizeof(boot_rx),
                           PS_HW6_NINA_BOOT_DRAIN_MS);

  for (uint32_t attempt = 0U;
       (attempt < PS_HW6_NINA_MAX_ATTEMPTS) && (probe->nina_at_ok == 0U);
       attempt++)
  {
    memset(rx, 0, sizeof(rx));
    probe->nina_attempt_count = attempt + 1U;
    probe->nina_at_tx_status =
        (uint32_t)HAL_UART_Transmit(&hlpuart1,
                                   at_command,
                                   (uint16_t)(sizeof(at_command) - 1U),
                                   PS_HW6_NINA_TX_TIMEOUT_MS);
    probe->nina_at_rx_len =
        PS_HW6_ReceiveWindow(rx,
                             sizeof(rx) - 1U,
                             PS_HW6_NINA_RX_WINDOW_MS);
    probe->nina_at_ok =
        PS_HW6_BufferContains(rx, probe->nina_at_rx_len, "OK");
    probe->nina_at_error =
        PS_HW6_BufferContains(rx, probe->nina_at_rx_len, "ERROR");
  }

  PS_HW6_CopyNinaResponse(rx, probe->nina_at_rx_len);
  HAL_GPIO_WritePin(NINA_NRST_GPIO_Port, NINA_NRST_Pin, GPIO_PIN_RESET);
  HAL_Delay(PS_HW6_NINA_RESET_ASSERT_MS);
  probe->nina_nrst_after =
      (uint32_t)HAL_GPIO_ReadPin(NINA_NRST_GPIO_Port, NINA_NRST_Pin);
  probe->nina_uart_state_after = (uint32_t)HAL_UART_GetState(&hlpuart1);
  probe->nina_uart_error_after = HAL_UART_GetError(&hlpuart1);

  if ((probe->nina_at_tx_status == (uint32_t)HAL_OK) &&
      (probe->nina_at_ok != 0U) &&
      (probe->nina_uart_error_after == HAL_UART_ERROR_NONE) &&
      (probe->nina_nrst_after == (uint32_t)GPIO_PIN_RESET))
  {
    probe->pass_mask |= PS_HW6_PERIPHERAL_NINA;
  }
}

void PS_HW6_PeripheralProbe_Run(void)
{
  volatile PS_HW6_PeripheralProbe *probe = &g_ps_hw6_peripheral_probe;

  memset((void *)probe, 0, sizeof(*probe));
  probe->magic = PS_HW6_PERIPHERAL_PROBE_MAGIC;
  probe->version = PS_HW6_PERIPHERAL_PROBE_VERSION;
  probe->phase = PS_HW6_PERIPHERAL_PROBE_PHASE_START;
  probe->start_tick = HAL_GetTick();
  probe->required_mask = PS_HW6_PERIPHERAL_PMIC |
                         PS_HW6_PERIPHERAL_IMU |
                         PS_HW6_PERIPHERAL_JOYSTICK |
                         PS_HW6_PERIPHERAL_FLASH |
                         PS_HW6_PERIPHERAL_NINA;
  probe->skipped_mask = PS_HW6_PERIPHERAL_DISPLAY |
                        PS_HW6_PERIPHERAL_AUDIO |
                        PS_HW6_PERIPHERAL_USB;
  probe->i2c_state_before = (uint32_t)HAL_I2C_GetState(&hi2c3);
  probe->i2c_error_before = HAL_I2C_GetError(&hi2c3);

  PS_HW6_ProbePmic();
  PS_HW6_ProbeImu();
  PS_HW6_ProbeJoystick();
  PS_HW6_ProbeFlash();
  PS_HW6_ProbeNina();

  probe->i2c_state_after = (uint32_t)HAL_I2C_GetState(&hi2c3);
  probe->i2c_error_after = HAL_I2C_GetError(&hi2c3);
  probe->rtc_state = (uint32_t)HAL_RTC_GetState(&hrtc);
  probe->audio_sai_state = (uint32_t)HAL_SAI_GetState(&hsai_BlockA1);
  probe->display_spi_state = (uint32_t)HAL_SPI_GetState(&hspi3);
  probe->display_lptim_state = (uint32_t)HAL_LPTIM_GetState(&hlptim1);
  probe->usb_pcd_state = (uint32_t)HAL_PCD_GetState(&hpcd_USB_OTG_FS);
  probe->failure_mask = probe->required_mask & ~probe->pass_mask;
  probe->end_tick = HAL_GetTick();
  probe->duration_ticks = probe->end_tick - probe->start_tick;
  probe->complete = 1U;
  probe->phase = PS_HW6_PERIPHERAL_PROBE_PHASE_COMPLETE;
}
