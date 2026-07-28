#include "LS013B7DH05.h"
#include <string.h>
#include <stdbool.h>

/*
 * Panel: 144 x 168 (LS013B7DH05 class)
 *
 * Stream format (EXTCOM handled externally):
 *   [WRITE_CMD = 0x01]
 *   repeat per line:
 *     [GATE_ADDR (1..168)]
 *     [LINE_DATA (LINE_WIDTH bytes)]
 *     [DUMMY 0x00]  (8 clocks)
 *   end:
 *     [DUMMY 0x00]  (extra 8 clocks; total 16 after last line)
 *
 * IMPORTANT:
 *   Buffer is stored in PANEL BYTE ORDER already.
 *   => flush does NOT do rev8() on pixel bytes.
 *
 * SPI:
 *   - 8-bit
 *   - CPOL=0, CPHA=1Edge
 *   - FirstBit = LSB (recommended, matches Sharp MIP examples)
 *   - SPI3 hardware NSS is ACTIVE HIGH and must stay HIGH during the whole
 *     update stream. Do not manually toggle PA15; it is SPI3_NSS on HW5.
 */

#define MLCD_CMD_WRITE   (0x01u)
#define MLCD_CMD_CLEAR   (0x04u)

#define SPI_TIMEOUT_MS   (150u)

/* Full-screen stream bytes:
 * 1 + H*(addr + data + dummy) + 1
 * = 1 + 168*(1+18+1) + 1 = 3362
 */
#define TXBUF_MAX (1u + (DISPLAY_HEIGHT * (LINE_WIDTH + 2u)) + 1u)

/* Put txBuf in SRAM4 for the validated LPDMA display path. */
#if defined(__GNUC__)
  #define SRAM4_BUF_ATTR __attribute__((section(".sram4"))) __attribute__((aligned(4)))
#elif defined(__ICCARM__)
  #define SRAM4_BUF_ATTR __attribute__((section(".sram4"))) __attribute__((aligned(4)))
#else
  #define SRAM4_BUF_ATTR
#endif

static uint8_t txBuf[TXBUF_MAX] SRAM4_BUF_ATTR;

/* --------------------------- Build write burst ----------------------------- */
/* rows[] are 1-based gate lines (1..DISPLAY_HEIGHT). */
static HAL_StatusTypeDef BuildWriteBurst(const uint8_t *buf, const uint16_t *rows,
                                         uint16_t rowCount, uint16_t *outLen)
{
    if (!outLen || !buf || !rows || rowCount == 0u) return HAL_ERROR;
    *outLen = 0;

    uint32_t needed = 1u + ((uint32_t)rowCount * (1u + LINE_WIDTH + 1u)) + 1u;
    if (needed > TXBUF_MAX) return HAL_ERROR;

    uint8_t *w = txBuf;
    *w++ = MLCD_CMD_WRITE;

    /*
     * Coalesce contiguous dirty rows into runs.
     * Protocol still emits [addr][line][dummy] per row, but run handling avoids
     * repeated address math for contiguous spans.
     */
    for (uint16_t i = 0u; i < rowCount; ) {
        uint16_t r = rows[i];
        uint16_t run_last = r;
        if ((r < 1u) || (r > DISPLAY_HEIGHT)) return HAL_ERROR;

        while ((i + 1u) < rowCount) {
            uint16_t next_r = rows[(uint16_t)(i + 1u)];
            if (next_r != (uint16_t)(run_last + 1u)) {
                break;
            }
            if ((next_r < 1u) || (next_r > DISPLAY_HEIGHT)) return HAL_ERROR;
            run_last = next_r;
            i++;
        }

        {
            uint16_t row = r;
            const uint8_t *src_row = buf + ((uint32_t)(row - 1u) * LINE_WIDTH);
            while (row <= run_last) {
                *w++ = (uint8_t)row;

                /* NO rev8(): buffer is already in panel order */
                memcpy(w, src_row, LINE_WIDTH);
                w += LINE_WIDTH;

                *w++ = 0x00u; /* per-line dummy */

                row++;
                src_row += LINE_WIDTH;
            }
        }

        i++;
    }

    *w++ = 0x00u; /* final dummy */
    *outLen = (uint16_t)(w - txBuf);
    return HAL_OK;
}

/* Faster full-screen builder: avoids allocating/filling a rows[] array and avoids
 * segmented/blocking per-row HAL_SPI_Transmit calls. */
static HAL_StatusTypeDef BuildWriteBurstAll(const uint8_t *buf, uint16_t *outLen)
{
    if (!outLen || !buf) return HAL_ERROR;
    *outLen = 0;

    /* Full frame: 1 + H*(1 + LINE_WIDTH + 1) + 1 */
    uint32_t needed = 1u + ((uint32_t)DISPLAY_HEIGHT * (1u + LINE_WIDTH + 1u)) + 1u;
    if (needed > TXBUF_MAX) return HAL_ERROR;

    uint8_t *w = txBuf;
    const uint8_t *src_row = buf;
    *w++ = MLCD_CMD_WRITE;

    for (uint16_t r = 1u; r <= DISPLAY_HEIGHT; r++) {
        *w++ = (uint8_t)r;

        memcpy(w, src_row, LINE_WIDTH);
        w += LINE_WIDTH;
        src_row += LINE_WIDTH;

        *w++ = 0x00u; /* per-line dummy */
    }

    *w++ = 0x00u; /* final dummy */
    *outLen = (uint16_t)(w - txBuf);
    return HAL_OK;
}

/* --------------------------- Public: init/clean ---------------------------- */
HAL_StatusTypeDef LCD_Init(LS013B7DH05 *MemDisp,
                           SPI_HandleTypeDef *Bus)
{
    if (!MemDisp || !Bus) return HAL_ERROR;

    MemDisp->Bus = Bus;

    return LCD_Clean(MemDisp);
}

HAL_StatusTypeDef LCD_Clean(LS013B7DH05 *MemDisp)
{
    if (!MemDisp) return HAL_ERROR;

    uint8_t clearSeq[2] = { MLCD_CMD_CLEAR, 0x00u };

    return HAL_SPI_Transmit(MemDisp->Bus, clearSeq, sizeof(clearSeq), SPI_TIMEOUT_MS);
}

/* --------------------------- Public: blocking flush ------------------------ */
HAL_StatusTypeDef LCD_FlushAll(LS013B7DH05 *MemDisp, const uint8_t *buf)
{
    if (!MemDisp || !buf) return HAL_ERROR;

    uint16_t len = 0u;
    HAL_StatusTypeDef st = BuildWriteBurstAll(buf, &len);
    if (st != HAL_OK) return st;

    return HAL_SPI_Transmit(MemDisp->Bus, txBuf, len, SPI_TIMEOUT_MS);
}

HAL_StatusTypeDef LCD_FlushRows(LS013B7DH05 *MemDisp, const uint8_t *buf,
                                const uint16_t *rows, uint16_t rowCount)
{
    if (!MemDisp || !buf) return HAL_ERROR;

    uint16_t len = 0;
    HAL_StatusTypeDef st = BuildWriteBurst(buf, rows, rowCount, &len);
    if (st != HAL_OK) return st;

    return HAL_SPI_Transmit(MemDisp->Bus, txBuf, len, SPI_TIMEOUT_MS);
}

/* --------------------------- DMA row-window transfer ----------------------- */
typedef struct {
    LS013B7DH05        *dev;
    const uint8_t      *p;
    uint32_t            remaining;
    HAL_StatusTypeDef   last;
} lcd_dma_chain_t;

static volatile bool g_dma_done = true;
static lcd_dma_chain_t g_chain = {0};

bool LCD_FlushDMA_IsDone(void) { return g_dma_done; }

__weak void LCD_FlushDmaDoneCallback(void) { }
__weak void LCD_FlushDmaErrorCallback(void) { }

static HAL_StatusTypeDef lcd_dma_kick_next(void)
{
    if (!g_chain.dev) return HAL_ERROR;
    if (g_chain.remaining == 0u) return HAL_OK;

    uint16_t n = (g_chain.remaining > UINT16_MAX) ? UINT16_MAX : (uint16_t)g_chain.remaining;
    return HAL_SPI_Transmit_DMA(g_chain.dev->Bus, (uint8_t*)g_chain.p, n);
}

static HAL_StatusTypeDef lcd_dma_start(LS013B7DH05 *dev, const uint8_t *buf, uint32_t len)
{
    if (!dev || !buf || len == 0u) return HAL_ERROR;
    if (!g_dma_done) return HAL_BUSY;

    g_chain.dev = dev;
    g_chain.p = buf;
    g_chain.remaining = len;
    g_chain.last = HAL_OK;

    g_dma_done = false;

    HAL_StatusTypeDef st = lcd_dma_kick_next();
    if (st != HAL_OK) {
        g_chain.dev = NULL;
        g_dma_done = true;
        return st;
    }
    return HAL_OK;
}

static HAL_StatusTypeDef lcd_dma_wait(uint32_t timeout_ms)
{
    uint32_t t0 = HAL_GetTick();
    while (!g_dma_done) {
        if ((HAL_GetTick() - t0) > timeout_ms) return HAL_TIMEOUT;
    }
    return g_chain.last;
}

/* NOTE: These callbacks are still defined here for drop-in compatibility.
 * If you later have multiple SPI DMA users, move the HAL callbacks to a central
 * file and dispatch into this module via thin wrappers. */
void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi)
{
    if (!g_chain.dev || !g_chain.dev->Bus) return;
    if (hspi != g_chain.dev->Bus) return;

    uint32_t sent = (g_chain.remaining > UINT16_MAX) ? (uint32_t)UINT16_MAX : g_chain.remaining;

    g_chain.p += sent;
    g_chain.remaining -= sent;

    if (g_chain.remaining == 0u) {
        g_chain.dev = NULL;
        g_dma_done = true;
        LCD_FlushDmaDoneCallback();
        return;
    }

    HAL_StatusTypeDef st = lcd_dma_kick_next();
    if (st != HAL_OK) {
        g_chain.last = st;
        g_chain.dev = NULL;
        g_dma_done = true;
        LCD_FlushDmaErrorCallback();
    }
}

void HAL_SPI_ErrorCallback(SPI_HandleTypeDef *hspi)
{
    if (!g_chain.dev || !g_chain.dev->Bus) return;
    if (hspi != g_chain.dev->Bus) return;

    g_chain.last = HAL_ERROR;
    g_chain.dev = NULL;
    g_dma_done = true;
    LCD_FlushDmaErrorCallback();
}

HAL_StatusTypeDef LCD_FlushAll_DMA(LS013B7DH05 *MemDisp, const uint8_t *buf)
{
    return LCD_PresentFull_DMA(MemDisp, buf, SPI_TIMEOUT_MS);
}

HAL_StatusTypeDef LCD_FlushRows_DMA(LS013B7DH05 *MemDisp, const uint8_t *buf,
                                    const uint16_t *rows, uint16_t rowCount)
{
    if (!MemDisp || !buf) return HAL_ERROR;
    if ((rowCount == 0u) || (rowCount > LCD_DMA_MAX_ROWS_PER_TRANSFER)) return HAL_ERROR;

    uint16_t len = 0;
    HAL_StatusTypeDef st = BuildWriteBurst(buf, rows, rowCount, &len);
    if (st != HAL_OK) return st;

    return lcd_dma_start(MemDisp, txBuf, len);
}

HAL_StatusTypeDef LCD_FlushRowRange_DMA_Async(LS013B7DH05 *MemDisp, const uint8_t *buf,
                                              uint16_t startRow, uint16_t rowCount)
{
    if (!MemDisp || !buf) return HAL_ERROR;
    if ((startRow == 0u) || (rowCount == 0u) || (rowCount > LCD_DMA_MAX_ROWS_PER_TRANSFER)) return HAL_ERROR;

    uint32_t finalRow = (uint32_t)startRow + rowCount - 1u;
    if (finalRow > DISPLAY_HEIGHT) return HAL_ERROR;

    uint16_t rows[LCD_DMA_MAX_ROWS_PER_TRANSFER];
    for (uint16_t i = 0u; i < rowCount; i++) {
        rows[i] = (uint16_t)(startRow + i);
    }

    return LCD_FlushRows_DMA(MemDisp, buf, rows, rowCount);
}

HAL_StatusTypeDef LCD_PresentFull_DMA(LS013B7DH05 *MemDisp, const uint8_t *buf,
                                      uint32_t timeout_ms)
{
    return LCD_PresentRowRange_DMA(MemDisp, buf, 1u, DISPLAY_HEIGHT, timeout_ms);
}

HAL_StatusTypeDef LCD_PresentRowRange_DMA(LS013B7DH05 *MemDisp, const uint8_t *buf,
                                          uint16_t startRow, uint16_t rowCount,
                                          uint32_t timeout_ms)
{
    if (!MemDisp || !buf) return HAL_ERROR;
    if ((startRow == 0u) || (rowCount == 0u)) return HAL_ERROR;

    uint32_t finalRow = (uint32_t)startRow + rowCount - 1u;
    if (finalRow > DISPLAY_HEIGHT) return HAL_ERROR;

    uint16_t rows[LCD_DMA_MAX_ROWS_PER_TRANSFER];
    uint32_t nextRow = startRow;

    while (nextRow <= finalRow) {
        uint32_t remaining = finalRow - nextRow + 1u;
        uint16_t chunkRows = (remaining > LCD_DMA_MAX_ROWS_PER_TRANSFER)
                             ? LCD_DMA_MAX_ROWS_PER_TRANSFER
                             : (uint16_t)remaining;

        for (uint16_t i = 0u; i < chunkRows; i++) {
            rows[i] = (uint16_t)(nextRow + i);
        }

        HAL_StatusTypeDef st = LCD_FlushRows_DMA(MemDisp, buf, rows, chunkRows);
        if (st != HAL_OK) return st;

        st = lcd_dma_wait(timeout_ms);
        if (st != HAL_OK) return st;

        nextRow += chunkRows;
    }

    return HAL_OK;
}

HAL_StatusTypeDef LCD_PresentRows_DMA(LS013B7DH05 *MemDisp, const uint8_t *buf,
                                      const uint16_t *rows, uint16_t rowCount,
                                      uint32_t timeout_ms)
{
    if (!MemDisp || !buf || !rows || (rowCount == 0u)) return HAL_ERROR;

    uint16_t offset = 0u;
    while (offset < rowCount) {
        uint16_t remaining = (uint16_t)(rowCount - offset);
        uint16_t chunkRows = (remaining > LCD_DMA_MAX_ROWS_PER_TRANSFER)
                             ? LCD_DMA_MAX_ROWS_PER_TRANSFER
                             : remaining;

        HAL_StatusTypeDef st = LCD_FlushRows_DMA(MemDisp, buf, &rows[offset], chunkRows);
        if (st != HAL_OK) return st;

        st = lcd_dma_wait(timeout_ms);
        if (st != HAL_OK) return st;

        offset = (uint16_t)(offset + chunkRows);
    }

    return HAL_OK;
}

HAL_StatusTypeDef LCD_FlushDMA_Wait(uint32_t timeout_ms)
{
    return lcd_dma_wait(timeout_ms);
}

HAL_StatusTypeDef LCD_FlushDMA_WaitWFI(uint32_t timeout_ms)
{
    uint32_t t0 = HAL_GetTick();
    while (!g_dma_done) {
        __WFI();
        if ((HAL_GetTick() - t0) > timeout_ms) return HAL_TIMEOUT;
    }
    return g_chain.last;
}
