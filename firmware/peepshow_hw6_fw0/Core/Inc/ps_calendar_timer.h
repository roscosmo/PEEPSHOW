#ifndef PS_CALENDAR_TIMER_H
#define PS_CALENDAR_TIMER_H

#include "ps_system_time.h"

typedef enum { PS_CALENDAR_ONCE = 1, PS_CALENDAR_DAILY = 2 } ps_calendar_kind_t;
typedef enum
{
  PS_CALENDAR_TODAY_OFFSET = 1,
  PS_CALENDAR_NEXT_OCCURRENCE
} ps_calendar_once_rule_t;

/* Resolve only on a new arm/restart, never on wake, resume or clock rebase.
 * time_of_day is seconds since midnight; day_offset is nonnegative calendar
 * days and must be zero for NEXT_OCCURRENCE. NEXT is strictly future.
 * TODAY_OFFSET may resolve to a past/equal deadline: Configure(ONCE) consumes
 * it without delivery rather than rolling it to tomorrow. Failure preserves
 * output. Unset time cannot select an anchor date; the adapter owns deferral. */
ps_system_time_status_t PS_CalendarTimer_ResolveOnce(
  ps_calendar_once_rule_t rule, uint32_t time_of_day, uint32_t day_offset,
  ps_system_time_status_t time_status, const ps_system_time_snapshot_t *now,
  uint32_t *deadline);
typedef enum
{
  PS_CALENDAR_IDLE = 0, PS_CALENDAR_ARMED, PS_CALENDAR_DUE,
  PS_CALENDAR_REBASED, PS_CALENDAR_ARGUMENT, PS_CALENDAR_STALE
} ps_calendar_result_t;

/* Single-owner record, with no HAL, queue, callbacks or retained pointers.
 * Lifetime/persistence and the bounded number of records belong to the adapter.
 * The adapter must reject stale registration/scene tokens before calling Fire;
 * a calendar generation is not a registration identity. */
typedef struct
{
  uint32_t configured, armed, consumed;
  uint32_t kind, value;
  uint32_t generation, deadline_seconds;
  uint32_t delivered, last_delivered_seconds;
} ps_calendar_timer_t;

/* ONCE value: encoded local seconds since 2000. DAILY: seconds since midnight.
 * Invalid arguments leave the record unchanged. Past ONCE is consumed, not fired.
 * Explicit Configure creates a new registration and clears delivery history. */
ps_calendar_result_t PS_CalendarTimer_Configure(ps_calendar_timer_t *timer,
  ps_calendar_kind_t kind, uint32_t value, ps_system_time_status_t time_status,
  const ps_system_time_snapshot_t *now);
void PS_CalendarTimer_Cancel(ps_calendar_timer_t *timer);
/* Call on clock edits/loss/recovery, not by a periodic polling loop. */
ps_calendar_result_t PS_CalendarTimer_Rebase(ps_calendar_timer_t *timer,
  ps_system_time_status_t time_status, const ps_system_time_snapshot_t *now);
/* Evaluate only a scheduled wake, using its captured calendar generation.
 * DUE consumes before return and writes the original occurrence timestamp.
 * Other results leave occurrence unchanged. Late daily wakes coalesce to one
 * event; next deadline is strictly future. A backward edit cannot redeliver
 * an occurrence already delivered by this registration. */
ps_calendar_result_t PS_CalendarTimer_Fire(ps_calendar_timer_t *timer,
  uint32_t wake_generation, ps_system_time_status_t time_status,
  const ps_system_time_snapshot_t *now, uint32_t *occurrence);
#endif
