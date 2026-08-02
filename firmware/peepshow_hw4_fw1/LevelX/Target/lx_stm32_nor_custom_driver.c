/**************************************************************************/
/*                                                                        */
/*       Copyright (c) Microsoft Corporation. All rights reserved.        */
/*                                                                        */
/*       This software is licensed under the Microsoft Software License   */
/*       Terms for Microsoft Azure RTOS. Full text of the license can be  */
/*       found in the LICENSE file at https://aka.ms/AzureRTOS_EULA       */
/*       and in the root directory of this software.                      */
/*                                                                        */
/**************************************************************************/

#include "lx_stm32_nor_custom_driver.h"

/* Private includes ----------------------------------------------------------*/

/* USER CODE BEGIN Includes */
#include <stdint.h>
#include <string.h>
#include "main.h"
#include "AT25SL128A.h"
#include "knobs_autogen.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define LX_NOR_BLOCK_BYTES       (4096UL)
#define LX_NOR_WORD_BYTES        (sizeof(ULONG))
#define LX_NOR_VERIFY_CHUNK      (64UL)

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */
extern OSPI_HandleTypeDef hospi1;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */
static uint8_t lx_nor_addr_valid(uint32_t addr, uint32_t size_bytes);

/* USER CODE END PFP */

static UINT  lx_nor_driver_read(ULONG *flash_address, ULONG *destination, ULONG words);
static UINT  lx_nor_driver_write(ULONG *flash_address, ULONG *source, ULONG words);

static UINT  lx_nor_driver_block_erase(ULONG block, ULONG erase_count);
static UINT  lx_nor_driver_block_erased_verify(ULONG block);

/* USER CODE BEGIN USER_CODE_SECTION_1 */
static uint8_t lx_nor_addr_valid(uint32_t addr, uint32_t size_bytes)
{
  uint32_t fat_base = (uint32_t)KNOB_STORAGE_FAT_BASE_ADDR;
  uint32_t fat_end = fat_base + (uint32_t)KNOB_STORAGE_FAT_SIZE_BYTES;
  uint32_t req_end = addr + size_bytes;

  if (size_bytes == 0UL)
  {
    return 0U;
  }

  if (addr < fat_base)
  {
    return 0U;
  }

  if (req_end < addr)
  {
    return 0U;
  }

  if (req_end > fat_end)
  {
    return 0U;
  }

  return 1U;
}

/* USER CODE END USER_CODE_SECTION_1 */

#ifndef LX_DIRECT_READ

#ifndef NOR_SECTOR_BUFFER_SIZE
#define NOR_SECTOR_BUFFER_SIZE 512
#endif

static ULONG nor_sector_memory[NOR_SECTOR_BUFFER_SIZE];
#endif

UINT  lx_stm32_nor_custom_driver_initialize(LX_NOR_FLASH *nor_flash)
{
  UINT ret = LX_SUCCESS;

  ULONG total_blocks = 0;
  ULONG words_per_block = 0;

  /* USER CODE BEGIN Init_Section_0 */
  if (nor_flash == NULL)
  {
    return LX_ERROR;
  }

  /* USER CODE END Init_Section_0 */

  nor_flash->lx_nor_flash_total_blocks    = total_blocks;
  nor_flash->lx_nor_flash_words_per_block = words_per_block;

  /* USER CODE BEGIN Init_Section_1 */
  nor_flash->lx_nor_flash_base_address = (ULONG *)(uintptr_t)KNOB_STORAGE_FAT_BASE_ADDR;
  nor_flash->lx_nor_flash_total_blocks = (ULONG)((uint32_t)KNOB_STORAGE_FAT_SIZE_BYTES / LX_NOR_BLOCK_BYTES);
  nor_flash->lx_nor_flash_words_per_block = (ULONG)(LX_NOR_BLOCK_BYTES / LX_NOR_WORD_BYTES);

  /* USER CODE END Init_Section_1 */

  nor_flash->lx_nor_flash_driver_read = lx_nor_driver_read;
  nor_flash->lx_nor_flash_driver_write = lx_nor_driver_write;

  nor_flash->lx_nor_flash_driver_block_erase = lx_nor_driver_block_erase;
  nor_flash->lx_nor_flash_driver_block_erased_verify = lx_nor_driver_block_erased_verify;

#ifndef LX_DIRECT_READ
    nor_flash->lx_nor_flash_sector_buffer = nor_sector_memory;
#endif

  /* USER CODE BEGIN Init_Section_2 */

  /* USER CODE END Init_Section_2 */

    return ret;
}

/* USER CODE BEGIN USER_CODE_SECTION_2 */

/* USER CODE END USER_CODE_SECTION_2 */

static UINT lx_nor_driver_read(ULONG *flash_address, ULONG *destination, ULONG words)
{
    UINT ret = LX_SUCCESS;

    /* USER CODE BEGIN NOR_READ */
    HAL_StatusTypeDef hal_status;
    uint32_t addr = (uint32_t)(uintptr_t)flash_address;
    uint32_t size_bytes = (uint32_t)(words * LX_NOR_WORD_BYTES);

    if ((flash_address == NULL) || (destination == NULL) || (words == 0UL))
    {
      return LX_ERROR;
    }

    if (words > (ULONG)(UINT32_MAX / LX_NOR_WORD_BYTES))
    {
      return LX_ERROR;
    }

    if (lx_nor_addr_valid(addr, size_bytes) == 0U)
    {
      return LX_ERROR;
    }

    hal_status = AT25_Read(&hospi1, addr, (uint8_t *)destination, size_bytes);
    if (hal_status != HAL_OK)
    {
      return LX_ERROR;
    }

    /* USER CODE END  NOR_READ */

    return ret;
}

static UINT lx_nor_driver_write(ULONG *flash_address, ULONG *source, ULONG words)
{
    UINT ret = LX_SUCCESS;

    /* USER CODE BEGIN NOR_DRIVER_WRITE */
    HAL_StatusTypeDef hal_status;
    uint32_t addr = (uint32_t)(uintptr_t)flash_address;
    uint32_t size_bytes = (uint32_t)(words * LX_NOR_WORD_BYTES);

    if ((flash_address == NULL) || (source == NULL) || (words == 0UL))
    {
      return LX_ERROR;
    }

    if (words > (ULONG)(UINT32_MAX / LX_NOR_WORD_BYTES))
    {
      return LX_ERROR;
    }

    if (lx_nor_addr_valid(addr, size_bytes) == 0U)
    {
      return LX_ERROR;
    }

    hal_status = AT25_PageProgram(&hospi1, addr, (const uint8_t *)source, size_bytes);
    if (hal_status != HAL_OK)
    {
      return LX_ERROR;
    }

    /* USER CODE END  NOR_DRIVER_WRITE */

    return ret;
}

static UINT lx_nor_driver_block_erase(ULONG block, ULONG erase_count)
{

    UINT ret = LX_SUCCESS;

    /* USER CODE BEGIN NOR_DRIVER_BLOCK */
    HAL_StatusTypeDef hal_status;
    uint32_t addr = (uint32_t)KNOB_STORAGE_FAT_BASE_ADDR + ((uint32_t)block * (uint32_t)LX_NOR_BLOCK_BYTES);

    (void)erase_count;

    if (block >= ((uint32_t)KNOB_STORAGE_FAT_SIZE_BYTES / (uint32_t)LX_NOR_BLOCK_BYTES))
    {
      return LX_ERROR;
    }

    if (lx_nor_addr_valid(addr, (uint32_t)LX_NOR_BLOCK_BYTES) == 0U)
    {
      return LX_ERROR;
    }

    hal_status = AT25_Erase4K(&hospi1, addr);
    if (hal_status != HAL_OK)
    {
      return LX_ERROR;
    }

    /* USER CODE END  NOR_DRIVER_BLOCK */

    return ret;
}

static UINT lx_nor_driver_block_erased_verify(ULONG block)
{
    UINT ret = LX_SUCCESS;

    /* USER CODE BEGIN NOR_DRIVER_VERIFY */
    HAL_StatusTypeDef hal_status;
    uint32_t addr = (uint32_t)KNOB_STORAGE_FAT_BASE_ADDR + ((uint32_t)block * (uint32_t)LX_NOR_BLOCK_BYTES);
    uint32_t remaining = LX_NOR_BLOCK_BYTES;
    uint32_t index;
    uint8_t verify_buf[LX_NOR_VERIFY_CHUNK];

    if (block >= ((uint32_t)KNOB_STORAGE_FAT_SIZE_BYTES / (uint32_t)LX_NOR_BLOCK_BYTES))
    {
      return LX_ERROR;
    }

    if (lx_nor_addr_valid(addr, (uint32_t)LX_NOR_BLOCK_BYTES) == 0U)
    {
      return LX_ERROR;
    }

    while (remaining > 0UL)
    {
      uint32_t chunk = (remaining > LX_NOR_VERIFY_CHUNK) ? LX_NOR_VERIFY_CHUNK : remaining;

      hal_status = AT25_Read(&hospi1, addr, verify_buf, chunk);
      if (hal_status != HAL_OK)
      {
        return LX_ERROR;
      }

      for (index = 0UL; index < chunk; index++)
      {
        if (verify_buf[index] != 0xFFU)
        {
          return LX_ERROR;
        }
      }

      addr += chunk;
      remaining -= chunk;
    }

    /* USER CODE END  NOR_DRIVER_VERIFY */

    return ret;
}

/* USER CODE BEGIN USER_CODE_SECTION_3 */

/* USER CODE END USER_CODE_SECTION_3 */
