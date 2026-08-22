#include "ps_hw6_hash.h"

#include <stddef.h>

#include "knobs_autogen.h"
#include "main.h"

extern HASH_HandleTypeDef hhash;

volatile ps_hw6_hash_probe_t g_ps_hw6_hash_probe =
{
  .api_version = PS_HW6_HASH_API_VERSION,
  .last_status = PS_HW6_HASH_STATUS_NOT_RUN,
  .hal_status = PS_HW6_HASH_STATUS_NOT_RUN,
  .hal_error = PS_HW6_HASH_STATUS_NOT_RUN
};

uint32_t PS_HW6_HASH_Sha256(const uint8_t *bytes,
                            uint32_t size,
                            uint8_t digest[32])
{
  HAL_StatusTypeDef status;

  g_ps_hw6_hash_probe.api_version = PS_HW6_HASH_API_VERSION;
  g_ps_hw6_hash_probe.sha256_count++;
  g_ps_hw6_hash_probe.last_status = PS_HW6_HASH_STATUS_NOT_RUN;
  g_ps_hw6_hash_probe.hal_status = PS_HW6_HASH_STATUS_NOT_RUN;
  g_ps_hw6_hash_probe.hal_error = PS_HW6_HASH_STATUS_NOT_RUN;
  g_ps_hw6_hash_probe.input_bytes = size;
  g_ps_hw6_hash_probe.start_tick = HAL_GetTick();
  g_ps_hw6_hash_probe.end_tick = 0UL;
  g_ps_hw6_hash_probe.duration_ticks = 0UL;

  if ((bytes == NULL) || (size == 0UL) || (digest == NULL))
  {
    g_ps_hw6_hash_probe.last_status = 1UL;
    return 1UL;
  }

  status = HAL_HASHEx_SHA256_Start(
    &hhash,
    (uint8_t *)(uintptr_t)bytes,
    size,
    digest,
    (uint32_t)KNOB_RUNTIME_EGG_HASH_TIMEOUT_MS);

  g_ps_hw6_hash_probe.end_tick = HAL_GetTick();
  g_ps_hw6_hash_probe.duration_ticks =
    g_ps_hw6_hash_probe.end_tick - g_ps_hw6_hash_probe.start_tick;
  g_ps_hw6_hash_probe.hal_status = (uint32_t)status;
  g_ps_hw6_hash_probe.hal_error = hhash.ErrorCode;
  g_ps_hw6_hash_probe.last_status = (status == HAL_OK) ? 0UL : 1UL;

  return g_ps_hw6_hash_probe.last_status;
}
