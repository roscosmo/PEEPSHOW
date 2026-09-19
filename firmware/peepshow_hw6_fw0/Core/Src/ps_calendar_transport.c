#include "ps_calendar_transport.h"
#include <stddef.h>

_Static_assert(sizeof(ps_calendar_message_t) == 4U * sizeof(uint32_t),
  "Calendar v1 must fit the four-word owner envelope");

static uint32_t PS_CalendarTransport_Matches(const ps_calendar_transport_t *t,
  const ps_calendar_message_t *m)
{
  return (m->magic == PS_CALENDAR_TRANSPORT_MAGIC) &&
    (m->sequence != 0U) && (m->sequence == t->delivery.sequence) &&
    (m->registration == t->delivery.key.registration);
}

ps_calendar_delivery_result_t PS_CalendarTransport_Offer(
  ps_calendar_transport_t *t, const ps_calendar_delivery_key_t *key,
  uint32_t occurrence)
{
  ps_calendar_delivery_result_t result;
  if (t == NULL) { return PS_CALENDAR_DELIVERY_ARGUMENT; }
  result = PS_CalendarDelivery_Offer(&t->delivery, key, occurrence);
  if (result == PS_CALENDAR_DELIVERY_OK)
  { t->notification_sent = 0U; t->grant_sent = 0U; }
  return result;
}

uint32_t PS_CalendarTransport_Next(const ps_calendar_transport_t *t,
  ps_calendar_message_t *m)
{
  uint32_t kind;
  if ((t == NULL) || (m == NULL)) { return 0U; }
  if ((t->delivery.state == PS_CALENDAR_DELIVERY_PENDING) && !t->notification_sent)
  { kind = PS_CALENDAR_NOTIFY; }
  else if ((t->delivery.state == PS_CALENDAR_DELIVERY_CLAIMED) && !t->grant_sent)
  { kind = PS_CALENDAR_GRANT; }
  else { return 0U; }
  *m = (ps_calendar_message_t){PS_CALENDAR_TRANSPORT_MAGIC, kind,
    t->delivery.sequence, t->delivery.key.registration};
  return 1U;
}

void PS_CalendarTransport_Sent(ps_calendar_transport_t *t,
  const ps_calendar_message_t *m)
{
  if ((t == NULL) || (m == NULL) || !PS_CalendarTransport_Matches(t, m)) { return; }
  if ((m->kind == PS_CALENDAR_NOTIFY) &&
      (t->delivery.state == PS_CALENDAR_DELIVERY_PENDING))
  { t->notification_sent = 1U; }
  if ((m->kind == PS_CALENDAR_GRANT) &&
      (t->delivery.state == PS_CALENDAR_DELIVERY_CLAIMED))
  { t->grant_sent = 1U; }
}

ps_calendar_delivery_result_t PS_CalendarTransport_Receive(
  ps_calendar_transport_t *t, const ps_calendar_message_t *m)
{
  ps_calendar_delivery_state_t outcome;
  if ((t == NULL) || (m == NULL)) { return PS_CALENDAR_DELIVERY_ARGUMENT; }
  if (!PS_CalendarTransport_Matches(t, m)) { return PS_CALENDAR_DELIVERY_STALE; }
  if (m->kind == PS_CALENDAR_CLAIM)
  {
    if (!t->notification_sent) { return PS_CALENDAR_DELIVERY_STALE; }
    return PS_CalendarDelivery_Claim(&t->delivery, &t->delivery.key, m->sequence, 1U);
  }
  if (!t->grant_sent) { return PS_CALENDAR_DELIVERY_STALE; }
  switch (m->kind)
  {
    case PS_CALENDAR_ACK_APPLIED: outcome = PS_CALENDAR_DELIVERY_APPLIED; break;
    case PS_CALENDAR_ACK_IGNORED: outcome = PS_CALENDAR_DELIVERY_IGNORED; break;
    case PS_CALENDAR_ACK_FAILED: outcome = PS_CALENDAR_DELIVERY_FAILED; break;
    default: return PS_CALENDAR_DELIVERY_ARGUMENT;
  }
  return PS_CalendarDelivery_Complete(&t->delivery, &t->delivery.key,
    m->sequence, outcome);
}

void PS_CalendarTransport_Invalidate(ps_calendar_transport_t *t)
{
  if (t != NULL) { PS_CalendarDelivery_Invalidate(&t->delivery); }
}
