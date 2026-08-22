#ifndef PS_HW6_HASH_H
#define PS_HW6_HASH_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PS_HW6_HASH_API_VERSION (1UL)
#define PS_HW6_HASH_STATUS_NOT_RUN (0xFFFFFFFFUL)

typedef struct
{
  uint32_t api_version;
  uint32_t sha256_count;
  uint32_t last_status;
  uint32_t hal_status;
  uint32_t hal_error;
  uint32_t input_bytes;
  uint32_t start_tick;
  uint32_t end_tick;
  uint32_t duration_ticks;
} ps_hw6_hash_probe_t;

extern volatile ps_hw6_hash_probe_t g_ps_hw6_hash_probe;

uint32_t PS_HW6_HASH_Sha256(const uint8_t *bytes,
                            uint32_t size,
                            uint8_t digest[32]);

#ifdef __cplusplus
}
#endif

#endif /* PS_HW6_HASH_H */
