#include "ps_calendar_delivery.h"
#include <stddef.h>

static uint32_t PS_CalendarDelivery_KeyEqual(
  const ps_calendar_delivery_key_t *a, const ps_calendar_delivery_key_t *b)
{
  return (a->session == b->session) && (a->scene == b->scene) &&
    (a->binding == b->binding) && (a->arm == b->arm) &&
    (a->registration == b->registration) && (a->clock == b->clock);
}

ps_calendar_delivery_result_t PS_CalendarDelivery_Offer(
  ps_calendar_delivery_t *delivery, const ps_calendar_delivery_key_t *key,
  uint32_t occurrence)
{
  if ((delivery == NULL) || (key == NULL) || (key->clock == 0U))
  { return PS_CALENDAR_DELIVERY_ARGUMENT; }
  if ((delivery->state == PS_CALENDAR_DELIVERY_PENDING) ||
      (delivery->state == PS_CALENDAR_DELIVERY_CLAIMED))
  { return PS_CALENDAR_DELIVERY_BUSY; }
  if ((delivery->state != PS_CALENDAR_DELIVERY_EMPTY) &&
      PS_CalendarDelivery_KeyEqual(&delivery->key, key) &&
      (occurrence <= delivery->occurrence))
  { return PS_CALENDAR_DELIVERY_STALE; }
  if (delivery->sequence == UINT32_MAX)
  { return PS_CALENDAR_DELIVERY_EXHAUSTED; }
  delivery->key = *key;
  delivery->occurrence = occurrence;
  delivery->sequence++;
  delivery->state = PS_CALENDAR_DELIVERY_PENDING;
  return PS_CALENDAR_DELIVERY_OK;
}

ps_calendar_delivery_result_t PS_CalendarDelivery_Claim(
  ps_calendar_delivery_t *delivery, const ps_calendar_delivery_key_t *key,
  uint32_t sequence, uint32_t runtime_running)
{
  if ((delivery == NULL) || (key == NULL))
  { return PS_CALENDAR_DELIVERY_ARGUMENT; }
  if ((delivery->state != PS_CALENDAR_DELIVERY_PENDING) ||
      (delivery->sequence != sequence) ||
      !PS_CalendarDelivery_KeyEqual(&delivery->key, key))
  { return PS_CALENDAR_DELIVERY_STALE; }
  if (runtime_running == 0U)
  { return PS_CALENDAR_DELIVERY_SUSPENDED; }
  delivery->state = PS_CALENDAR_DELIVERY_CLAIMED;
  return PS_CALENDAR_DELIVERY_OK;
}

ps_calendar_delivery_result_t PS_CalendarDelivery_Complete(
  ps_calendar_delivery_t *delivery, const ps_calendar_delivery_key_t *key,
  uint32_t sequence, ps_calendar_delivery_state_t outcome)
{
  if ((delivery == NULL) || (key == NULL) ||
      ((outcome != PS_CALENDAR_DELIVERY_APPLIED) &&
       (outcome != PS_CALENDAR_DELIVERY_IGNORED) &&
       (outcome != PS_CALENDAR_DELIVERY_FAILED)))
  { return PS_CALENDAR_DELIVERY_ARGUMENT; }
  if ((delivery->state != PS_CALENDAR_DELIVERY_CLAIMED) ||
      (delivery->sequence != sequence) ||
      !PS_CalendarDelivery_KeyEqual(&delivery->key, key))
  { return PS_CALENDAR_DELIVERY_STALE; }
  delivery->state = outcome;
  return PS_CALENDAR_DELIVERY_OK;
}

void PS_CalendarDelivery_Invalidate(ps_calendar_delivery_t *delivery)
{
  if ((delivery != NULL) &&
      (delivery->state == PS_CALENDAR_DELIVERY_PENDING))
  { delivery->state = PS_CALENDAR_DELIVERY_INVALIDATED; }
}
