#include <assert.h>
#include <stddef.h>
#include "ps_battery_wake.h"

int main(void)
{
  ps_battery_wake_t s;
  uint32_t i;
  assert(PS_BatteryWake_Init(&s, 180000, 6000, 6000));
  assert(PS_BatteryWake_Remaining(&s, 0) == 0);
  PS_BatteryWake_Record(&s, 0, 1, 0);
  assert(PS_BatteryWake_Remaining(&s, 0) == 180000);
  for (i = 0; i < 10; ++i)
  {
    assert(PS_BatteryWake_Prepare(&s, 0) == 180000 - i * 1000);
    PS_BatteryWake_Finish(&s, 0, 1000, 1); /* Kernel ticks frozen in STOP2. */
  }
  assert(PS_BatteryWake_Remaining(&s, 0) == 170000);
  PS_BatteryWake_Prepare(&s, 100);
  PS_BatteryWake_Finish(&s, 110, 20, 1); /* Includes awake prepare/finish time. */
  assert(PS_BatteryWake_Remaining(&s, 110) == 169880);
  PS_BatteryWake_Shorten(&s, 110, 200000); /* Test helper cannot extend it. */
  assert(s.test_arms == 0);
  PS_BatteryWake_Shorten(&s, 110, 1500);
  assert(s.test_arms == 1 && PS_BatteryWake_Remaining(&s, 110) == 1500);
  PS_BatteryWake_Prepare(&s, 110);
  PS_BatteryWake_Finish(&s, 110, 1500, 1);
  assert(s.pending && s.due_wakes == 1);
  PS_BatteryWake_Record(&s, 110, 0, 0);
  assert(s.failures == 1 && PS_BatteryWake_Remaining(&s, 110) == 6000);
  for (i = 111; i < 6100; ++i) { PS_BatteryWake_Record(&s, i, 0, 0); }
  assert(s.deadline_tick == 6110); /* Failures cannot continually rearm retry. */
  PS_BatteryWake_Record(&s, 6110, 0, 0);
  assert(s.deadline_tick == 12110);
  PS_BatteryWake_Record(&s, 6111, 1, 1);
  assert(s.deadline_tick == 12111);
  PS_BatteryWake_Record(&s, 6112, 1, 0);
  assert(s.deadline_tick == 186112);
  PS_BatteryWake_Prepare(&s, 6112);
  PS_BatteryWake_Finish(&s, 6112, 0, 0);
  assert(s.pending && s.clock_failures == 1);
  PS_BatteryWake_Record(&s, 0xfffffff0U, 1, 0);
  assert(PS_BatteryWake_Remaining(&s, 0x10) == 179968);
  PS_BatteryWake_Prepare(&s, 0x10);
  PS_BatteryWake_Finish(&s, 0x10, 180000, 1);
  assert(PS_BatteryWake_Remaining(&s, 0x10) == 0);
  assert(!PS_BatteryWake_Init(&s, 0, 1, 1));
  assert(!PS_BatteryWake_Init(&s, 10, 11, 1));
  assert(!PS_BatteryWake_Init(&s, 10, 1, 11));
  assert(!PS_BatteryWake_Init(NULL, 10, 1, 1));
  return 0;
}
