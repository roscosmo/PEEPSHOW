#include "ps_calendar_delivery.h"
#include <assert.h>
#include <stddef.h>
#include <string.h>

int main(void)
{
  ps_calendar_delivery_t d = {0}, saved;
  ps_calendar_delivery_key_t key = {1, 2, 0, 3, 4, 5}, old, wrong;
  uint32_t i;
  assert(PS_CalendarDelivery_Offer(NULL, &key, 10) == PS_CALENDAR_DELIVERY_ARGUMENT);
  assert(PS_CalendarDelivery_Offer(&d, NULL, 10) == PS_CALENDAR_DELIVERY_ARGUMENT);
  wrong = key; wrong.clock = 0;
  assert(PS_CalendarDelivery_Offer(&d, &wrong, 10) == PS_CALENDAR_DELIVERY_ARGUMENT);
  assert(d.sequence == 0);
  assert(PS_CalendarDelivery_Offer(&d, &key, 10) == PS_CALENDAR_DELIVERY_OK);
  saved = d;
  /* A failed notification send performs no mutation. Reoffers cannot overwrite
   * the retained occurrence while the transport recovers. */
  for (i = 0; i < 8; ++i)
  {
    assert(PS_CalendarDelivery_Offer(&d, &key, 11) == PS_CALENDAR_DELIVERY_BUSY);
    assert(PS_CalendarDelivery_Claim(&d, &key, 1, 0) == PS_CALENDAR_DELIVERY_SUSPENDED);
    assert(memcmp(&d, &saved, sizeof(d)) == 0);
  }
  assert(PS_CalendarDelivery_Complete(&d, &key, 1,
    PS_CALENDAR_DELIVERY_APPLIED) == PS_CALENDAR_DELIVERY_STALE);
  /* Every component of identity is checked; binding zero is valid. */
  for (i = 0; i < 6; ++i)
  {
    wrong = key;
    switch (i)
    {
      case 0: wrong.session++; break;
      case 1: wrong.scene++; break;
      case 2: wrong.binding++; break;
      case 3: wrong.arm++; break;
      case 4: wrong.registration++; break;
      default: wrong.clock++; break;
    }
    assert(PS_CalendarDelivery_Claim(&d, &wrong, 1, 1) == PS_CALENDAR_DELIVERY_STALE);
  }
  assert(PS_CalendarDelivery_Claim(&d, &key, 0, 1) == PS_CALENDAR_DELIVERY_STALE);
  assert(PS_CalendarDelivery_Claim(&d, &key, 1, 1) == PS_CALENDAR_DELIVERY_OK);
  assert(PS_CalendarDelivery_Claim(&d, &key, 1, 1) == PS_CALENDAR_DELIVERY_STALE);
  PS_CalendarDelivery_Invalidate(&d);
  assert(d.state == PS_CALENDAR_DELIVERY_CLAIMED);
  assert(PS_CalendarDelivery_Offer(&d, &key, 11) == PS_CALENDAR_DELIVERY_BUSY);
  assert(PS_CalendarDelivery_Complete(&d, &key, 1,
    PS_CALENDAR_DELIVERY_PENDING) == PS_CALENDAR_DELIVERY_ARGUMENT);
  assert(PS_CalendarDelivery_Complete(&d, &key, 1,
    PS_CALENDAR_DELIVERY_APPLIED) == PS_CALENDAR_DELIVERY_OK);
  assert(PS_CalendarDelivery_Complete(&d, &key, 1,
    PS_CALENDAR_DELIVERY_APPLIED) == PS_CALENDAR_DELIVERY_STALE);
  assert(PS_CalendarDelivery_Offer(&d, &key, 10) == PS_CALENDAR_DELIVERY_STALE);
  assert(PS_CalendarDelivery_Offer(&d, &key, 9) == PS_CALENDAR_DELIVERY_STALE);

  assert(PS_CalendarDelivery_Offer(&d, &key, 11) == PS_CALENDAR_DELIVERY_OK);
  PS_CalendarDelivery_Invalidate(&d);
  assert(PS_CalendarDelivery_Claim(&d, &key, 2, 1) == PS_CALENDAR_DELIVERY_STALE);
  old = key; key.clock++;
  assert(PS_CalendarDelivery_Offer(&d, &key, 12) == PS_CALENDAR_DELIVERY_OK);
  assert(PS_CalendarDelivery_Claim(&d, &old, 3, 1) == PS_CALENDAR_DELIVERY_STALE);
  assert(PS_CalendarDelivery_Claim(&d, &key, 2, 1) == PS_CALENDAR_DELIVERY_STALE);
  assert(PS_CalendarDelivery_Claim(&d, &key, 3, 1) == PS_CALENDAR_DELIVERY_OK);
  assert(PS_CalendarDelivery_Complete(&d, &old, 3,
    PS_CALENDAR_DELIVERY_APPLIED) == PS_CALENDAR_DELIVERY_STALE);
  assert(PS_CalendarDelivery_Complete(&d, &key, 3,
    PS_CALENDAR_DELIVERY_IGNORED) == PS_CALENDAR_DELIVERY_OK);
  assert(PS_CalendarDelivery_Offer(&d, &key, 12) == PS_CALENDAR_DELIVERY_STALE);

  /* A fresh scene/arm may legitimately use an earlier local deadline. */
  key.scene++; key.arm++; key.registration++;
  assert(PS_CalendarDelivery_Offer(&d, &key, 1) == PS_CALENDAR_DELIVERY_OK);
  assert(PS_CalendarDelivery_Claim(&d, &key, 4, 1) == PS_CALENDAR_DELIVERY_OK);
  assert(PS_CalendarDelivery_Complete(&d, &key, 4,
    PS_CALENDAR_DELIVERY_FAILED) == PS_CALENDAR_DELIVERY_OK);
  assert(PS_CalendarDelivery_Offer(&d, &key, 1) == PS_CALENDAR_DELIVERY_STALE);
  assert(PS_CalendarDelivery_Claim(&d, &key, 4, 1) == PS_CALENDAR_DELIVERY_STALE);
  d.sequence = UINT32_MAX;
  saved = d;
  assert(PS_CalendarDelivery_Offer(&d, &key, 2) == PS_CALENDAR_DELIVERY_EXHAUSTED);
  assert(memcmp(&d, &saved, sizeof(d)) == 0);
  PS_CalendarDelivery_Invalidate(NULL);
  return 0;
}
