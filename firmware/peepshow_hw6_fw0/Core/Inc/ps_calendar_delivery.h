#ifndef PS_CALENDAR_DELIVERY_H
#define PS_CALENDAR_DELIVERY_H

#include <stdint.h>

typedef struct
{
  uint32_t session, scene, binding, arm, registration, clock;
} ps_calendar_delivery_key_t;

typedef enum
{
  PS_CALENDAR_DELIVERY_EMPTY = 0,
  PS_CALENDAR_DELIVERY_PENDING,
  PS_CALENDAR_DELIVERY_CLAIMED,
  PS_CALENDAR_DELIVERY_APPLIED,
  PS_CALENDAR_DELIVERY_IGNORED,
  PS_CALENDAR_DELIVERY_FAILED,
  PS_CALENDAR_DELIVERY_INVALIDATED
} ps_calendar_delivery_state_t;

typedef enum
{
  PS_CALENDAR_DELIVERY_OK = 0,
  PS_CALENDAR_DELIVERY_ARGUMENT,
  PS_CALENDAR_DELIVERY_BUSY,
  PS_CALENDAR_DELIVERY_STALE,
  PS_CALENDAR_DELIVERY_SUSPENDED,
  PS_CALENDAR_DELIVERY_EXHAUSTED
} ps_calendar_delivery_result_t;

/* Zero-initialize once. Single-owner mutation only: not a shared-thread mailbox.
 * Pass key/sequence values in typed messages; never pass this record's address.
 * Queue-send success/failure does not change state. The adapter must serialize
 * claims with clock/lifetime changes and revalidate runtime lifetime on dispatch.
 * Sequence survives invalidation; resetting it with notifications in flight is
 * forbidden. Clock generation zero means unavailable and cannot be offered. */
typedef struct
{
  ps_calendar_delivery_key_t key;
  uint32_t sequence, occurrence;
  ps_calendar_delivery_state_t state;
} ps_calendar_delivery_t;

ps_calendar_delivery_result_t PS_CalendarDelivery_Offer(
  ps_calendar_delivery_t *delivery, const ps_calendar_delivery_key_t *key,
  uint32_t occurrence);
ps_calendar_delivery_result_t PS_CalendarDelivery_Claim(
  ps_calendar_delivery_t *delivery, const ps_calendar_delivery_key_t *key,
  uint32_t sequence, uint32_t runtime_running);
/* Terminal failure is retained, never an implicit gameplay retry. */
ps_calendar_delivery_result_t PS_CalendarDelivery_Complete(
  ps_calendar_delivery_t *delivery, const ps_calendar_delivery_key_t *key,
  uint32_t sequence, ps_calendar_delivery_state_t outcome);
/* Invalidates unclaimed work only. A claimed transaction must finish before
 * replacement work can occupy the slot, including after a clock edit. */
void PS_CalendarDelivery_Invalidate(ps_calendar_delivery_t *delivery);

#endif
