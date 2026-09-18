#include "ps_hw6_time_retention.h"
#include "main.h"

extern RTC_HandleTypeDef hrtc;
volatile ps_hw6_time_retention_probe_t g_ps_time_retention_probe;

/* DR0 identifies our raw RTC epoch/configuration; DR1..8 hold the mapping. */
#define PS_TIME_SOURCE_COOKIE (0x52544302UL)

uint32_t PS_HW6_TimeRetention_BootPreserve(void)
{
  uint32_t flags = RCC->CSR;
  uint32_t warm = RCC_CSR_PINRSTF | RCC_CSR_SFTRSTF;
  uint32_t unsafe = RCC_CSR_BORRSTF | RCC_CSR_OBLRSTF | RCC_CSR_LPWRRSTF |
    RCC_CSR_IWDGRSTF | RCC_CSR_WWDGRSTF;
  g_ps_time_retention_probe.reset_flags = flags;
  g_ps_time_retention_probe.source_preserved =
    (((flags & warm) != 0U) && ((flags & unsafe) == 0U) &&
     (__HAL_RTC_IS_CALENDAR_INITIALIZED(&hrtc) != 0U) &&
     (HAL_RTCEx_BKUPRead(&hrtc, RTC_BKP_DR0) == PS_TIME_SOURCE_COOKIE)) ? 1U : 0U;
  __HAL_RCC_CLEAR_RESET_FLAGS();
  if (g_ps_time_retention_probe.source_preserved == 0U)
  {
    /* Invalidate first: a reset during fresh calendar initialization is cold. */
    HAL_RTCEx_BKUPWrite(&hrtc, RTC_BKP_DR0, 0U);
    HAL_RTCEx_BKUPWrite(&hrtc, RTC_BKP_DR1, 0U);
  }
  return g_ps_time_retention_probe.source_preserved;
}

uint32_t PS_HW6_TimeRetention_BootFresh(void)
{
  RTC_DateTypeDef date = {0};
  RTC_DateTypeDef readback = {0};
  /* INITS depends on a nonzero year. Year 00 makes HAL reinitialize on reset.
   * This private epoch is unrelated to the user's editable local date. */
  date.Year = 1U;
  date.Month = RTC_MONTH_JANUARY;
  date.Date = 1U;
  date.WeekDay = RTC_WEEKDAY_MONDAY;
  HAL_RTCEx_BKUPWrite(&hrtc, RTC_BKP_DR0, 0U);
  if ((HAL_RTC_SetDate(&hrtc, &date, RTC_FORMAT_BIN) != HAL_OK) ||
      (HAL_RTC_GetDate(&hrtc, &readback, RTC_FORMAT_BIN) != HAL_OK) ||
      (readback.Year != date.Year) || (readback.Month != date.Month) ||
      (readback.Date != date.Date) || (readback.WeekDay != date.WeekDay))
  {
    g_ps_time_retention_probe.failures++;
    return 0U;
  }
  HAL_RTCEx_BKUPWrite(&hrtc, RTC_BKP_DR0, PS_TIME_SOURCE_COOKIE);
  if (HAL_RTCEx_BKUPRead(&hrtc, RTC_BKP_DR0) != PS_TIME_SOURCE_COOKIE)
  {
    g_ps_time_retention_probe.failures++;
    return 0U;
  }
  return 1U;
}

void PS_HW6_TimeRetention_Restore(ps_system_time_t *clock)
{
  uint32_t words[PS_SYSTEM_TIME_RECORD_WORDS];
  uint32_t i;
  PS_SystemTime_Init(clock);
  if (g_ps_time_retention_probe.source_preserved == 0U) { return; }
  for (i = 0U; i < PS_SYSTEM_TIME_RECORD_WORDS; ++i)
  { words[i] = HAL_RTCEx_BKUPRead(&hrtc, RTC_BKP_DR1 + i); }
  g_ps_time_retention_probe.mapping_restored = PS_SystemTime_LoadRecord(clock, words);
  if (g_ps_time_retention_probe.mapping_restored == 0U)
  { HAL_RTCEx_BKUPWrite(&hrtc, RTC_BKP_DR1, 0U); }
}

uint32_t PS_HW6_TimeRetention_Store(const ps_system_time_t *clock)
{
  uint32_t words[PS_SYSTEM_TIME_RECORD_WORDS];
  uint32_t i;
  HAL_RTCEx_BKUPWrite(&hrtc, RTC_BKP_DR1, 0U);
  if (HAL_RTCEx_BKUPRead(&hrtc, RTC_BKP_DR1) != 0U) { goto failed; }
  if (PS_SystemTime_SaveRecord(clock, words) == 0U) { return 1U; }
  for (i = 1U; i < PS_SYSTEM_TIME_RECORD_WORDS; ++i)
  { HAL_RTCEx_BKUPWrite(&hrtc, RTC_BKP_DR1 + i, words[i]); }
  for (i = 1U; i < PS_SYSTEM_TIME_RECORD_WORDS; ++i)
  {
    if (HAL_RTCEx_BKUPRead(&hrtc, RTC_BKP_DR1 + i) != words[i]) { goto failed; }
  }
  __DMB();
  HAL_RTCEx_BKUPWrite(&hrtc, RTC_BKP_DR1, words[0]);
  if (HAL_RTCEx_BKUPRead(&hrtc, RTC_BKP_DR1) != words[0]) { goto failed; }
  g_ps_time_retention_probe.writes++;
  return 1U;
failed:
  HAL_RTCEx_BKUPWrite(&hrtc, RTC_BKP_DR1, 0U);
  g_ps_time_retention_probe.failures++;
  return 0U;
}
