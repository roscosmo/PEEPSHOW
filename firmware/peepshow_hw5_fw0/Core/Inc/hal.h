/**
 * Minimal STM32 transport shim for the TI TMAG3001 example driver.
 *
 * This is intentionally narrow: it provides only the symbols used by
 * tmag3001_ex.c and keeps the active I2C address explicit for bring-up.
 */
#ifndef PS_TMAG3001_HAL_H_
#define PS_TMAG3001_HAL_H_

#include "stm32u5xx_hal.h"
#include <stdint.h>

#define Address_Select (0x34U)
#define LOW            (0U)
#define HIGH           (1U)

void TMAG3001_HAL_SetI2CAddress(uint8_t address_7bit);
HAL_StatusTypeDef TMAG3001_HAL_GetLastStatus(void);
uint32_t TMAG3001_HAL_GetLastError(void);

void delay_ms(uint32_t delay_ms);
void delay_us(uint32_t delay_us);
void setINT(uint8_t level);
void i2cSendArrays(uint8_t *data, uint8_t count);
void i2cReceiveArrays(uint8_t *data_rx, uint8_t count, uint8_t *data_tx);
void i2cOneByteReceiveArrays(uint8_t *data_rx, uint8_t count);

#endif /* PS_TMAG3001_HAL_H_ */
