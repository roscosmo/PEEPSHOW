#include "ps_calendar_transport.h"
#include <assert.h>
#include <string.h>

int main(void)
{
  ps_calendar_transport_t t = {0};
  ps_calendar_delivery_key_t key = {1, 2, 3, 4, 5, 6};
  ps_calendar_message_t m, retry, old, reply;
  uint32_t i;
  assert(PS_CalendarTransport_Next(&t, &m) == 0);
  assert(PS_CalendarTransport_Offer(&t, &key, 123) == PS_CALENDAR_DELIVERY_OK);
  assert(PS_CalendarTransport_Next(&t, &m));
  assert(m.kind == PS_CALENDAR_NOTIFY);
  reply = m; reply.kind = PS_CALENDAR_CLAIM;
  assert(PS_CalendarTransport_Receive(&t, &reply) == PS_CALENDAR_DELIVERY_STALE);
  for (i = 0; i < 10; ++i)
  {
    /* Simulated full queue: do not report Sent. */
    assert(PS_CalendarTransport_Next(&t, &retry));
    assert(memcmp(&m, &retry, sizeof(m)) == 0);
    assert(PS_CalendarTransport_Offer(&t, &key, 124) == PS_CALENDAR_DELIVERY_BUSY);
  }
  PS_CalendarTransport_Sent(&t, &m);
  assert(!PS_CalendarTransport_Next(&t, &retry));
  old = reply;
  reply.registration++;
  assert(PS_CalendarTransport_Receive(&t, &reply) == PS_CALENDAR_DELIVERY_STALE);
  reply = old;
  assert(PS_CalendarTransport_Receive(&t, &reply) == PS_CALENDAR_DELIVERY_OK);
  assert(PS_CalendarTransport_Receive(&t, &reply) == PS_CALENDAR_DELIVERY_STALE);
  assert(PS_CalendarTransport_Next(&t, &m));
  assert(m.kind == PS_CALENDAR_GRANT);
  reply = m; reply.kind = PS_CALENDAR_ACK_APPLIED;
  assert(PS_CalendarTransport_Receive(&t, &reply) == PS_CALENDAR_DELIVERY_STALE);
  for (i = 0; i < 10; ++i)
  {
    assert(PS_CalendarTransport_Next(&t, &retry));
    assert(memcmp(&m, &retry, sizeof(m)) == 0);
  }
  /* A clock edit after claim does not abort the admitted transaction. */
  PS_CalendarTransport_Invalidate(&t);
  assert(t.delivery.state == PS_CALENDAR_DELIVERY_CLAIMED);
  PS_CalendarTransport_Sent(&t, &m);
  assert(!PS_CalendarTransport_Next(&t, &retry));
  assert(PS_CalendarTransport_Receive(&t, &reply) == PS_CALENDAR_DELIVERY_OK);
  assert(t.delivery.state == PS_CALENDAR_DELIVERY_APPLIED);
  assert(PS_CalendarTransport_Receive(&t, &reply) == PS_CALENDAR_DELIVERY_STALE);
  assert(PS_CalendarTransport_Offer(&t, &key, 123) == PS_CALENDAR_DELIVERY_STALE);
  assert(PS_CalendarTransport_Offer(&t, &key, 124) == PS_CALENDAR_DELIVERY_OK);
  assert(PS_CalendarTransport_Next(&t, &m));
  PS_CalendarTransport_Sent(&t, &m);
  /* Clock edit before claim rejects the already queued notification. */
  PS_CalendarTransport_Invalidate(&t);
  reply = m; reply.kind = PS_CALENDAR_CLAIM;
  assert(PS_CalendarTransport_Receive(&t, &reply) == PS_CALENDAR_DELIVERY_STALE);
  key.clock++; key.registration++;
  assert(PS_CalendarTransport_Offer(&t, &key, 125) == PS_CALENDAR_DELIVERY_OK);
  assert(PS_CalendarTransport_Receive(&t, &old) == PS_CALENDAR_DELIVERY_STALE);
  assert(PS_CalendarTransport_Next(&t, &m));
  PS_CalendarTransport_Sent(&t, &m);
  m.kind = PS_CALENDAR_CLAIM;
  assert(PS_CalendarTransport_Receive(&t, &m) == PS_CALENDAR_DELIVERY_OK);
  assert(PS_CalendarTransport_Next(&t, &m));
  PS_CalendarTransport_Sent(&t, &m);
  m.kind = PS_CALENDAR_ACK_IGNORED;
  assert(PS_CalendarTransport_Receive(&t, &m) == PS_CALENDAR_DELIVERY_OK);
  assert(t.delivery.state == PS_CALENDAR_DELIVERY_IGNORED);
  assert(PS_CalendarTransport_Offer(&t, &key, 125) == PS_CALENDAR_DELIVERY_STALE);
  return 0;
}
