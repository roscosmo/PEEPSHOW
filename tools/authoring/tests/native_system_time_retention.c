#include <assert.h>
#include <stdint.h>
#include <string.h>
#include "ps_hw6_time_retention.h"

typedef uint32_t RTC_HandleTypeDef;
typedef struct { uint8_t Year, Month, Date, WeekDay; } RTC_DateTypeDef;
enum { HAL_OK = 0, HAL_ERROR = 1, RTC_FORMAT_BIN = 0,
       RTC_MONTH_JANUARY = 1, RTC_WEEKDAY_MONDAY = 1 };
RTC_HandleTypeDef hrtc;
static struct { uint32_t CSR; } fake_rcc;
#define RCC (&fake_rcc)
#define RCC_CSR_PINRSTF (1U)
#define RCC_CSR_SFTRSTF (2U)
#define RCC_CSR_BORRSTF (4U)
#define RCC_CSR_OBLRSTF (8U)
#define RCC_CSR_LPWRRSTF (16U)
#define RCC_CSR_IWDGRSTF (32U)
#define RCC_CSR_WWDGRSTF (64U)
#define RTC_BKP_DR0 (0U)
#define RTC_BKP_DR1 (1U)
#define __HAL_RCC_CLEAR_RESET_FLAGS() (RCC->CSR = 0U)
#define __HAL_RTC_IS_CALENDAR_INITIALIZED(h) (raw_date.Year != 0U)
#define __DMB() ((void)0)
static uint32_t backup[9], write_budget = UINT32_MAX;
static uint32_t set_status, get_status, corrupt_readback, date_writes;
static RTC_DateTypeDef raw_date = {0, 1, 1, 1};
static uint32_t HAL_RTC_SetDate(RTC_HandleTypeDef *handle, RTC_DateTypeDef *date, uint32_t format)
{
  assert(handle == &hrtc && format == RTC_FORMAT_BIN);
  date_writes++;
  if (set_status == HAL_OK) { raw_date = *date; }
  return set_status;
}
static uint32_t HAL_RTC_GetDate(RTC_HandleTypeDef *handle, RTC_DateTypeDef *date, uint32_t format)
{
  assert(handle == &hrtc && format == RTC_FORMAT_BIN);
  *date = raw_date;
  if (corrupt_readback != 0U) { date->Year = 0U; }
  return get_status;
}
static uint32_t HAL_RTCEx_BKUPRead(RTC_HandleTypeDef *handle, uint32_t reg)
{ assert(handle == &hrtc && reg < 9U); return backup[reg]; }
static void HAL_RTCEx_BKUPWrite(RTC_HandleTypeDef *handle, uint32_t reg, uint32_t value)
{
  assert(handle == &hrtc && reg < 9U);
  if (write_budget == 0U) { return; }
  if (write_budget != UINT32_MAX) { --write_budget; }
  backup[reg] = value;
}
#include "retention.inc"

int main(void)
{
  ps_system_time_t clock, restored;
  ps_system_time_snapshot_t snapshot;
  ps_system_datetime_t local = {2026, 9, 19, 23, 59, 50};
  uint32_t good[9], words[8], i, bit, cut;
  RCC->CSR = RCC_CSR_BORRSTF | RCC_CSR_PINRSTF;
  assert(PS_HW6_TimeRetention_BootPreserve() == 0U);
  assert(RCC->CSR == 0U);
  /* Reproduce the hardware year-zero case, not an always-true INITS stub. */
  assert(__HAL_RTC_IS_CALENDAR_INITIALIZED(&hrtc) == 0U);
  set_status = HAL_ERROR;
  assert(PS_HW6_TimeRetention_BootFresh() == 0U && backup[0] == 0U);
  set_status = HAL_OK;
  get_status = HAL_ERROR;
  assert(PS_HW6_TimeRetention_BootFresh() == 0U && backup[0] == 0U);
  get_status = HAL_OK;
  corrupt_readback = 1U;
  assert(PS_HW6_TimeRetention_BootFresh() == 0U && backup[0] == 0U);
  corrupt_readback = 0U;
  write_budget = 0U;
  assert(PS_HW6_TimeRetention_BootFresh() == 0U && backup[0] == 0U);
  write_budget = UINT32_MAX;
  assert(PS_HW6_TimeRetention_BootFresh() == 1U);
  assert(raw_date.Year == 1U && raw_date.Month == 1U && raw_date.Date == 1U);
  assert(__HAL_RTC_IS_CALENDAR_INITIALIZED(&hrtc) != 0U);
  PS_HW6_TimeRetention_Restore(&restored);
  assert(restored.status == PS_SYSTEM_TIME_UNSET);
  PS_SystemTime_Init(&clock);
  assert(PS_SystemTime_Set(&clock, &local, 100000U) == PS_SYSTEM_TIME_OK);
  assert(PS_HW6_TimeRetention_Store(&clock) == 1U);
  memcpy(good, backup, sizeof(good));
  for (i = 1U; i <= 2U; ++i)
  {
    uint32_t before = date_writes;
    RCC->CSR = i;
    assert(PS_HW6_TimeRetention_BootPreserve() == 1U);
    assert(date_writes == before);
    PS_HW6_TimeRetention_Restore(&restored);
    assert(PS_SystemTime_Read(&restored, 115123U, &snapshot) == PS_SYSTEM_TIME_OK);
    assert(snapshot.local.day == 20U && snapshot.local.second == 5U);
    assert(snapshot.millisecond == 123U && snapshot.generation == 1U);
  }
  assert(PS_SystemTime_SaveRecord(&clock, words) == 1U);
  for (i = 0U; i < 8U; ++i)
  {
    for (bit = 0U; bit < 32U; ++bit)
    {
      words[i] ^= 1UL << bit;
      assert(PS_SystemTime_LoadRecord(&restored, words) == 0U);
      assert(restored.status == PS_SYSTEM_TIME_UNSET);
      words[i] ^= 1UL << bit;
    }
  }
  assert(PS_SystemTime_LoadRecord(&restored, words) == 1U);
  assert(PS_SystemTime_Read(&restored, 99999U, &snapshot) == PS_SYSTEM_TIME_SOURCE_LOST);
  assert(PS_HW6_TimeRetention_Store(&restored) == 1U && backup[1] == 0U);

  /* Every interrupted write before final commit is rejected (or old intact). */
  for (cut = 0U; cut < 9U; ++cut)
  {
    memcpy(backup, good, sizeof(good));
    write_budget = cut;
    assert(PS_HW6_TimeRetention_Store(&clock) == 0U);
    write_budget = UINT32_MAX;
    RCC->CSR = RCC_CSR_PINRSTF;
    assert(PS_HW6_TimeRetention_BootPreserve() == 1U);
    PS_HW6_TimeRetention_Restore(&restored);
    assert(restored.status == ((cut == 0U) ? PS_SYSTEM_TIME_OK : PS_SYSTEM_TIME_UNSET));
  }
  /* Unsafe/reset-unknown lifetimes cannot use even an intact old record. */
  for (i = 0U; i < 6U; ++i)
  {
    memcpy(backup, good, sizeof(good));
    RCC->CSR = (i == 0U) ? 0U : (1UL << (i + 1U)) | RCC_CSR_PINRSTF;
    assert(PS_HW6_TimeRetention_BootPreserve() == 0U);
    PS_HW6_TimeRetention_Restore(&restored);
    assert(restored.status == PS_SYSTEM_TIME_UNSET && backup[1] == 0U);
  }
  memcpy(backup, good, sizeof(good));
  raw_date.Year = 0U;
  RCC->CSR = RCC_CSR_SFTRSTF;
  assert(PS_HW6_TimeRetention_BootPreserve() == 0U);
  /* The old source cookie is invalid even when INITS happens to be set. */
  raw_date.Year = 1U;
  memcpy(backup, good, sizeof(good));
  backup[0] = 0x52544301UL;
  RCC->CSR = RCC_CSR_PINRSTF;
  assert(PS_HW6_TimeRetention_BootPreserve() == 0U && backup[1] == 0U);
  return 0;
}
