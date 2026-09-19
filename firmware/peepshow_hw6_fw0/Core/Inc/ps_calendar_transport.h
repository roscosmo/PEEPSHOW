#ifndef PS_CALENDAR_TRANSPORT_H
#define PS_CALENDAR_TRANSPORT_H
#include "ps_calendar_delivery.h"

/* CAL1: exactly four words, suitable for existing HW6 owner queues. */
#define PS_CALENDAR_TRANSPORT_MAGIC (0x43414C31UL)
typedef enum
{
  PS_CALENDAR_NOTIFY = 1, PS_CALENDAR_CLAIM, PS_CALENDAR_GRANT,
  PS_CALENDAR_ACK_APPLIED, PS_CALENDAR_ACK_IGNORED, PS_CALENDAR_ACK_FAILED
} ps_calendar_message_kind_t;
typedef struct
{
  uint32_t magic, kind, sequence, registration;
} ps_calendar_message_t;

/* This is the POWER endpoint only. The immutable registration identity is
 * established separately by the runtime registration transaction. Runtime must
 * validate its scene lifetime before CLAIM and again before executing GRANT.
 * Queue packets identify a retained slot; they do not contain object pointers.
 * Single owner, initialized once. Do not reset sequence during a live session. */
typedef struct
{
  ps_calendar_delivery_t delivery;
  uint32_t notification_sent, grant_sent;
} ps_calendar_transport_t;

ps_calendar_delivery_result_t PS_CalendarTransport_Offer(
  ps_calendar_transport_t *transport, const ps_calendar_delivery_key_t *key,
  uint32_t occurrence);
/* Construct at most one outbound message. Call Sent only after successful
 * non-blocking enqueue. A failed send leaves the message available for retry.
 * Retry scheduling/budget belongs to the adapter, not this core. */
uint32_t PS_CalendarTransport_Next(const ps_calendar_transport_t *transport,
  ps_calendar_message_t *message);
void PS_CalendarTransport_Sent(ps_calendar_transport_t *transport,
  const ps_calendar_message_t *message);
ps_calendar_delivery_result_t PS_CalendarTransport_Receive(
  ps_calendar_transport_t *transport, const ps_calendar_message_t *message);
void PS_CalendarTransport_Invalidate(ps_calendar_transport_t *transport);
#endif
