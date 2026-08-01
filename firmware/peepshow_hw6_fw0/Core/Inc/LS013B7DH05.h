#ifndef LS013B7DH05_H
#define LS013B7DH05_H

#pragma once
#include "stm32u5xx_hal.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Panel resolution */
#define DISPLAY_WIDTH   144u
#define DISPLAY_HEIGHT  168u

/* 1bpp, 8 pixels/byte */
#define LINE_WIDTH      (DISPLAY_WIDTH / 8u)           /* 18 bytes */
#define BUFFER_LENGTH   (DISPLAY_HEIGHT * LINE_WIDTH)  /* 3024 bytes */

/* Validated HW5 LPDMA/SPI3 transport limit: 48-row row-window streams. */
#define LCD_DMA_MAX_ROWS_PER_TRANSFER 48u

typedef struct {
    SPI_HandleTypeDef *Bus;
} LS013B7DH05;

/* Init / clear */
HAL_StatusTypeDef LCD_Init(LS013B7DH05 *MemDisp,
                           SPI_HandleTypeDef *Bus);

HAL_StatusTypeDef LCD_Clean(LS013B7DH05 *MemDisp);

/* Blocking flush */
HAL_StatusTypeDef LCD_FlushAll(LS013B7DH05 *MemDisp, const uint8_t *buf);
HAL_StatusTypeDef LCD_FlushRows(LS013B7DH05 *MemDisp, const uint8_t *buf,
                                const uint16_t *rows, uint16_t rowCount);

/* Single DMA row-window transfer. rowCount must be <= LCD_DMA_MAX_ROWS_PER_TRANSFER. */
HAL_StatusTypeDef LCD_FlushAll_DMA(LS013B7DH05 *MemDisp, const uint8_t *buf);
HAL_StatusTypeDef LCD_FlushRows_DMA(LS013B7DH05 *MemDisp, const uint8_t *buf,
                                    const uint16_t *rows, uint16_t rowCount);
HAL_StatusTypeDef LCD_FlushRowRange_DMA_Async(LS013B7DH05 *MemDisp, const uint8_t *buf,
                                              uint16_t startRow, uint16_t rowCount);

/* Validated blocking present helpers. These split into bounded DMA row windows. */
HAL_StatusTypeDef LCD_PresentFull_DMA(LS013B7DH05 *MemDisp, const uint8_t *buf,
                                      uint32_t timeout_ms);
HAL_StatusTypeDef LCD_PresentRowRange_DMA(LS013B7DH05 *MemDisp, const uint8_t *buf,
                                          uint16_t startRow, uint16_t rowCount,
                                          uint32_t timeout_ms);
HAL_StatusTypeDef LCD_PresentRows_DMA(LS013B7DH05 *MemDisp, const uint8_t *buf,
                                      const uint16_t *rows, uint16_t rowCount,
                                      uint32_t timeout_ms);

bool              LCD_FlushDMA_IsDone(void);
HAL_StatusTypeDef LCD_FlushDMA_WaitWFI(uint32_t timeout_ms);
HAL_StatusTypeDef LCD_FlushDMA_Wait(uint32_t timeout_ms);

/* Optional DMA completion hooks (ISR context). */
void LCD_FlushDmaDoneCallback(void);
void LCD_FlushDmaErrorCallback(void);

#endif
