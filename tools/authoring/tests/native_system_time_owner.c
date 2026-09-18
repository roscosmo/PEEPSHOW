#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "ps_hw6_system_time.h"

typedef uint32_t ULONG;
typedef uint32_t UINT;
enum { TX_SUCCESS, TX_CALLER_ERROR, TX_PTR_ERROR, TX_NOT_AVAILABLE, TX_NO_EVENTS,
       TX_QUEUE_FULL, TX_NO_WAIT = 0, HAL_OK = 0, HAL_ERROR = 1, RTC_FORMAT_BIN = 0,
       PS_HW6_RTOS_OWNER_UI = 0, PS_HW6_RTOS_OWNER_POWER = 1,
       PS_HW6_RTOS_MESSAGE_WORDS = 4 };
#define __DMB() ((void)0)
typedef struct { uint8_t Hours, Minutes, Seconds; uint32_t SubSeconds, SecondFraction; } RTC_TimeTypeDef;
typedef struct { uint8_t Year, Month, Date; } RTC_DateTypeDef;
static uint32_t ps_threads[2], ps_queues[2], hrtc;
static struct { uint32_t runtime_complete; } g_ps_hw6_rtos_probe = {1};
static void *current;
static ULONG queued[4];
static UINT send_status, time_status, date_status;
static uint32_t reads, dates, sends, immediate;
static RTC_TimeTypeDef raw_time = {0, 0, 0, 255, 255};
static RTC_DateTypeDef raw_date = {0, 1, 1};
static void PS_HW6_SystemTime_Owner(const ULONG *message);
static void *tx_thread_identify(void) { return current; }
static UINT tx_queue_send(void *queue, const void *message, UINT wait)
{
  assert(queue == &ps_queues[PS_HW6_RTOS_OWNER_POWER] && wait == TX_NO_WAIT);
  ++sends;
  if (send_status != TX_SUCCESS) { return send_status; }
  memcpy(queued, message, sizeof(queued));
  if (immediate != 0U)
  {
    current = &ps_threads[PS_HW6_RTOS_OWNER_POWER];
    PS_HW6_SystemTime_Owner(queued);
    current = &ps_threads[PS_HW6_RTOS_OWNER_UI];
  }
  return TX_SUCCESS;
}
static UINT HAL_RTC_GetTime(void *handle, RTC_TimeTypeDef *time, UINT format)
{
  assert(current == &ps_threads[PS_HW6_RTOS_OWNER_POWER]);
  assert(handle == &hrtc && format == RTC_FORMAT_BIN);
  ++reads;
  *time = raw_time;
  return time_status;
}
static UINT HAL_RTC_GetDate(void *handle, RTC_DateTypeDef *date, UINT format)
{
  assert(current == &ps_threads[PS_HW6_RTOS_OWNER_POWER]);
  assert(handle == &hrtc && format == RTC_FORMAT_BIN);
  ++dates;
  *date = raw_date;
  return date_status;
}

#include "system_time_owner.inc"

static void deliver(void)
{
  current = &ps_threads[PS_HW6_RTOS_OWNER_POWER];
  PS_HW6_SystemTime_Owner(queued);
  current = &ps_threads[PS_HW6_RTOS_OWNER_UI];
}
static ps_hw6_system_time_result_t transact(uint32_t operation, const ps_system_datetime_t *local)
{
  uint32_t token;
  ps_hw6_system_time_result_t result;
  assert(PS_HW6_SystemTime_Request(operation, local, &token) == TX_SUCCESS);
  if (immediate == 0U) { deliver(); }
  assert(PS_HW6_SystemTime_Take(token, &result) == TX_SUCCESS);
  return result;
}
int main(void)
{
  ps_system_datetime_t local = {2026, 9, 18, 23, 59, 50};
  ps_hw6_system_time_result_t result;
  uint32_t token = 99, other = 88, count, generation;
  ULONG bad[4];
  PS_SystemTime_Init(&ps_system_clock);
  current = &ps_threads[PS_HW6_RTOS_OWNER_POWER];
  assert(PS_HW6_SystemTime_Request(1, NULL, &token) == TX_CALLER_ERROR);
  assert(PS_HW6_SystemTime_Take(1, &result) == TX_CALLER_ERROR);
  current = &ps_threads[PS_HW6_RTOS_OWNER_UI];
  PS_HW6_SystemTime_DebugUi();
  assert(sends == 0 && reads == 0);
  g_ps_hw6_rtos_probe.runtime_complete = 0;
  assert(PS_HW6_SystemTime_Request(1, NULL, &token) == TX_NOT_AVAILABLE);
  g_ps_hw6_rtos_probe.runtime_complete = 1;
  assert(PS_HW6_SystemTime_Request(9, NULL, &token) == TX_PTR_ERROR);
  assert(PS_HW6_SystemTime_Request(1, NULL, NULL) == TX_PTR_ERROR);
  assert(PS_HW6_SystemTime_Request(2, NULL, &token) == TX_PTR_ERROR);
  local.day = 31;
  assert(PS_HW6_SystemTime_Request(2, &local, &token) == TX_PTR_ERROR);
  local.day = 18;
  assert(sends == 0 && token == 99);
  assert(PS_HW6_SystemTime_Request(1, NULL, &token) == TX_SUCCESS);
  assert(reads == 0 && g_ps_system_time_probe.complete == 0);
  assert(PS_HW6_SystemTime_Request(2, &local, &other) == TX_NOT_AVAILABLE);
  assert(other == 88);
  memset(&result, 0xA5, sizeof(result));
  assert(PS_HW6_SystemTime_Take(token, &result) == TX_NO_EVENTS);
  assert(result.status == 0xA5A5A5A5U && g_ps_system_time_probe.pending == 1);
  assert(PS_HW6_SystemTime_Take(token + 1, &result) == TX_PTR_ERROR);
  PS_HW6_SystemTime_Owner(queued);
  assert(reads == 0);
  memcpy(bad, queued, sizeof(bad));
  current = &ps_threads[PS_HW6_RTOS_OWNER_POWER];
  for (uint32_t i = 0; i < 4; ++i)
  {
    bad[i] ^= 1U;
    PS_HW6_SystemTime_Owner(bad);
    bad[i] ^= 1U;
  }
  assert(reads == 0);
  deliver();
  assert(reads == 1 && dates == 1);
  deliver();
  assert(reads == 1);
  assert(PS_HW6_SystemTime_Take(token, &result) == TX_SUCCESS);
  assert(result.status == PS_SYSTEM_TIME_UNSET && result.source_status == HAL_OK);
  assert(PS_HW6_SystemTime_Take(token, &result) == TX_PTR_ERROR);

  send_status = TX_QUEUE_FULL;
  assert(PS_HW6_SystemTime_Request(2, &local, &other) == TX_QUEUE_FULL);
  assert(other == 88 && g_ps_system_time_probe.pending == 0);
  send_status = TX_SUCCESS;
  assert(PS_HW6_SystemTime_Request(2, &local, &token) == TX_SUCCESS);
  local.year = 2001;
  deliver();
  assert(PS_HW6_SystemTime_Take(token, &result) == TX_SUCCESS);
  assert(result.status == 0 && result.snapshot.local.year == 2026);
  assert(result.snapshot.local.second == 50 && result.source_ms == 0);
  assert(raw_time.Seconds == 0 && raw_date.Year == 0);
  generation = result.snapshot.generation;
  raw_time.Seconds = 15;
  raw_time.SubSeconds = 127;
  result = transact(1, NULL);
  assert(result.snapshot.local.day == 19 && result.snapshot.local.hour == 0);
  assert(result.snapshot.local.second == 5 && result.snapshot.millisecond == 500);
  assert(result.source_ms == 15500);
  immediate = 1;
  result = transact(2, &local);
  assert(result.snapshot.local.year == 2001 && result.snapshot.generation == generation + 1);
  assert(result.source_ms == 15500 && raw_time.Seconds == 15);
  local.year = 2030;
  result = transact(2, &local);
  assert(result.snapshot.local.year == 2030 && result.source_ms == 15500);

  date_status = HAL_ERROR;
  result = transact(1, NULL);
  assert(result.status == PS_SYSTEM_TIME_SOURCE_LOST && result.snapshot.generation == 0);
  date_status = HAL_OK;
  result = transact(1, NULL);
  assert(result.status == PS_SYSTEM_TIME_SOURCE_LOST);
  result = transact(2, &local);
  assert(result.status == 0);
  count = dates;
  time_status = HAL_ERROR;
  result = transact(1, NULL);
  assert(result.status == PS_SYSTEM_TIME_SOURCE_LOST && dates == count);
  time_status = HAL_OK;
  raw_date.Month = 0;
  result = transact(2, &local);
  assert(result.status == PS_SYSTEM_TIME_SOURCE_LOST);
  raw_date.Month = 1;
  raw_time.SubSeconds = 256;
  result = transact(1, NULL);
  assert(result.source_status == HAL_ERROR);
  raw_time.SubSeconds = 255;
  result = transact(2, &local);
  assert(result.status == 0);
  raw_time.Seconds = 14;
  result = transact(1, NULL);
  assert(result.status == PS_SYSTEM_TIME_SOURCE_LOST);

  g_ps_system_time_request_local = local;
  g_ps_system_time_request = 2;
  PS_HW6_SystemTime_DebugUi();
  assert(g_ps_system_time_request == 0 && ps_system_time_debug_token != 0);
  count = reads;
  PS_HW6_SystemTime_DebugUi();
  PS_HW6_SystemTime_DebugUi();
  assert(reads == count && g_ps_system_time_probe.pending == 0);
  ps_system_clock.generation = UINT32_MAX;
  result = transact(2, &local);
  assert(result.status == PS_SYSTEM_TIME_GENERATION_EXHAUSTED);
  g_ps_system_time_probe.request = UINT32_MAX;
  assert(PS_HW6_SystemTime_Request(1, NULL, &token) == TX_NOT_AVAILABLE);
  puts("system time owner checks passed");
  return 0;
}
