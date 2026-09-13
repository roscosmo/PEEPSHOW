#ifndef PS_DISPLAY_WORK_PROFILE_H
#define PS_DISPLAY_WORK_PROFILE_H

#include <stdint.h>
#include <stddef.h>

typedef enum
{
  PS_DISPLAY_WORK_PROJECT = 0,
  PS_DISPLAY_WORK_RASTER,
  PS_DISPLAY_WORK_COMPARE,
  PS_DISPLAY_WORK_PAYLOAD,
  PS_DISPLAY_WORK_COPY,
  PS_DISPLAY_WORK_COUNT
} ps_display_work_stage_t;

/* Optional synchronous observer, owned by the calling display transaction.
 * Clock must be a bounded read with no peripheral/clock-policy side effects.
 * Caller zeroes totals once; nested projection/packing calls accumulate them. */
typedef struct
{
  uint32_t (*clock)(void);
  uint32_t ticks[PS_DISPLAY_WORK_COUNT];
  uint32_t calls[PS_DISPLAY_WORK_COUNT];
} ps_display_work_profile_t;

static inline uint32_t PS_DisplayWork_Begin(ps_display_work_profile_t *profile)
{
  return ((profile != NULL) && (profile->clock != NULL)) ? profile->clock() : 0UL;
}

static inline void PS_DisplayWork_End(ps_display_work_profile_t *profile,
  ps_display_work_stage_t stage, uint32_t start)
{
  if ((profile != NULL) && (profile->clock != NULL))
  {
    profile->ticks[stage] += profile->clock() - start;
    profile->calls[stage]++;
  }
}

#endif
