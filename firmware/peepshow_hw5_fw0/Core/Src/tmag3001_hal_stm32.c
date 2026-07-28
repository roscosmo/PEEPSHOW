/**
 * Minimal STM32 transport shim for the TI TMAG3001 example driver.
 */
#include "hal.h"
#include "main.h"

extern I2C_HandleTypeDef hi2c3;

#define TMAG3001_HAL_I2C_TIMEOUT_MS (25U)

static uint16_t s_tmag3001_hal_addr_shifted = (0x34U << 1);
static HAL_StatusTypeDef s_tmag3001_hal_last_status = HAL_OK;
static uint32_t s_tmag3001_hal_last_error = 0U;

void TMAG3001_HAL_SetI2CAddress(uint8_t address_7bit)
{
  s_tmag3001_hal_addr_shifted = (uint16_t)((uint16_t)address_7bit << 1);
  s_tmag3001_hal_last_status = HAL_OK;
  s_tmag3001_hal_last_error = 0U;
}

HAL_StatusTypeDef TMAG3001_HAL_GetLastStatus(void)
{
  return s_tmag3001_hal_last_status;
}

uint32_t TMAG3001_HAL_GetLastError(void)
{
  return s_tmag3001_hal_last_error;
}

void delay_ms(uint32_t delay)
{
  HAL_Delay(delay);
}

void delay_us(uint32_t delay)
{
  /* The bring-up HAL tick is millisecond based; round up for safety. */
  HAL_Delay((delay + 999U) / 1000U);
}

void setINT(uint8_t level)
{
  (void)level;
}

void i2cSendArrays(uint8_t *data, uint8_t count)
{
  s_tmag3001_hal_last_status = HAL_I2C_Master_Transmit(&hi2c3,
                                                       s_tmag3001_hal_addr_shifted,
                                                       data,
                                                       count,
                                                       TMAG3001_HAL_I2C_TIMEOUT_MS);
  s_tmag3001_hal_last_error = HAL_I2C_GetError(&hi2c3);
}

void i2cOneByteReceiveArrays(uint8_t *data_rx, uint8_t count)
{
  s_tmag3001_hal_last_status = HAL_I2C_Master_Receive(&hi2c3,
                                                      s_tmag3001_hal_addr_shifted,
                                                      data_rx,
                                                      count,
                                                      TMAG3001_HAL_I2C_TIMEOUT_MS);
  s_tmag3001_hal_last_error = HAL_I2C_GetError(&hi2c3);
}

void i2cReceiveArrays(uint8_t *data_rx, uint8_t count, uint8_t *data_tx)
{
  s_tmag3001_hal_last_status = HAL_I2C_Master_Transmit(&hi2c3,
                                                       s_tmag3001_hal_addr_shifted,
                                                       data_tx,
                                                       1U,
                                                       TMAG3001_HAL_I2C_TIMEOUT_MS);
  if (s_tmag3001_hal_last_status == HAL_OK)
  {
    s_tmag3001_hal_last_status = HAL_I2C_Master_Receive(&hi2c3,
                                                        s_tmag3001_hal_addr_shifted,
                                                        data_rx,
                                                        count,
                                                        TMAG3001_HAL_I2C_TIMEOUT_MS);
  }
  s_tmag3001_hal_last_error = HAL_I2C_GetError(&hi2c3);
}
