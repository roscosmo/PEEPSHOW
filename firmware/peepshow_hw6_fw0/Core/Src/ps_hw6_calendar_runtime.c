#include "ps_hw6_calendar_runtime.h"
#include "ps_hw6_calendar.h"
#include <stdatomic.h>

/* Extended CAL1 control operations; payload remains four words. REGISTER uses
 * a single immutable leased request, exactly like the system-time owner API.
 * Only runtime writes the lease; only a matching REGISTERED reply releases it. */
enum { CAL_REGISTER = 7, CAL_REGISTERED, CAL_CANCEL, CAL_REJECT, CAL_DEFER };
typedef struct
{
  uint32_t registration, scene, binding, request, time_of_day, day_offset;
} calendar_intent_t;
static calendar_intent_t intent;
static uint32_t runtime_registration, runtime_scene, runtime_binding;
static uint32_t runtime_setup, runtime_active, runtime_notice, runtime_done;
static ps_calendar_message_t runtime_out, power_out;
static uint32_t runtime_out_valid, power_out_valid;
static ps_calendar_transport_t transport;
static ps_calendar_delivery_key_t power_key;
static uint32_t power_active;
volatile ps_hw6_calendar_runtime_probe_t g_ps_calendar_runtime_probe = {
  .api_version = 1U, .setup_status = UINT32_MAX
};

static ps_calendar_message_t Packet(uint32_t kind, uint32_t sequence,
  uint32_t registration)
{
  return (ps_calendar_message_t){PS_CALENDAR_TRANSPORT_MAGIC, kind,
    sequence, registration};
}

static void Publish(void)
{
  g_ps_calendar_runtime_probe.delivery_state = transport.delivery.state;
  g_ps_calendar_runtime_probe.delivery_sequence = transport.delivery.sequence;
  g_ps_calendar_runtime_probe.occurrence = transport.delivery.occurrence;
  g_ps_calendar_runtime_probe.clock_generation = transport.delivery.key.clock;
}

uint32_t PS_HW6_CalendarRuntime_Ready(void)
{
  return !power_active ||
    ((transport.delivery.state != PS_CALENDAR_DELIVERY_PENDING) &&
     (transport.delivery.state != PS_CALENDAR_DELIVERY_CLAIMED));
}

uint32_t PS_HW6_CalendarRuntime_Bound(void) { return power_active; }

uint32_t PS_HW6_CalendarRuntime_NeedsService(void)
{
  if (!power_active) { return 0U; }
  return power_out_valid ||
    (transport.delivery.state == PS_CALENDAR_DELIVERY_CLAIMED) ||
    ((transport.delivery.state == PS_CALENDAR_DELIVERY_PENDING) &&
     !transport.notification_sent);
}

void PS_HW6_CalendarRuntime_Due(uint32_t occurrence, uint32_t generation)
{
  if (!power_active) { return; }
  power_key.clock = generation;
  if (PS_CalendarTransport_Offer(&transport, &power_key, occurrence) !=
      PS_CALENDAR_DELIVERY_OK)
  {
    /* Capacity is checked before the scheduler consumes. Never retry actions
     * after an unexpected internal admission failure. */
    power_active = 0U;
    PS_HW6_Calendar_Cancel();
  }
  Publish();
}

void PS_HW6_CalendarRuntime_TimeChanged(void)
{
  PS_CalendarTransport_Invalidate(&transport);
  Publish();
}

void PS_HW6_CalendarRuntime_PowerMessage(const ps_calendar_message_t *m)
{
  ps_calendar_delivery_result_t result;
  if (m->magic != PS_CALENDAR_TRANSPORT_MAGIC) { return; }
  if (m->kind == CAL_REGISTER)
  {
    uint32_t value = 0U;
    ps_system_time_snapshot_t now;
    ps_system_time_status_t status;
    atomic_thread_fence(memory_order_acquire);
    if ((m->sequence != intent.registration) ||
        (m->registration != intent.scene) || power_out_valid ||
        (transport.delivery.state == PS_CALENDAR_DELIVERY_CLAIMED))
    { return; }
    status = PS_HW6_Calendar_Read(&now);
    if ((status == PS_SYSTEM_TIME_OK) && (intent.request != 1U))
    {
      status = PS_CalendarTimer_ResolveOnce(
        intent.request == 2U ? PS_CALENDAR_TODAY_OFFSET : PS_CALENDAR_NEXT_OCCURRENCE,
        intent.time_of_day, intent.day_offset, status, &now, &value);
    }
    else if ((intent.time_of_day >= 86400U) || (intent.day_offset != 0U))
    { status = PS_SYSTEM_TIME_ARGUMENT; }
    if (status == PS_SYSTEM_TIME_OK)
    {
      PS_CalendarTransport_Invalidate(&transport);
      power_key = (ps_calendar_delivery_key_t){intent.scene, intent.scene,
        intent.binding, intent.registration, intent.registration, now.generation};
      power_active = 1U;
      if (PS_HW6_Calendar_Register(intent.request == 1U ? PS_CALENDAR_DAILY :
          PS_CALENDAR_ONCE, intent.request == 1U ? intent.time_of_day : value) ==
          PS_CALENDAR_ARGUMENT)
      { power_active = 0U; status = PS_SYSTEM_TIME_ARGUMENT; }
      if (g_ps_calendar_probe.time_status != PS_SYSTEM_TIME_OK)
      {
        status = (ps_system_time_status_t)g_ps_calendar_probe.time_status;
        power_active = 0U;
        PS_HW6_Calendar_Cancel();
      }
    }
    power_out = Packet(CAL_REGISTERED, (uint32_t)status, intent.registration);
    power_out_valid = 1U;
  }
  else if (m->kind == CAL_CANCEL)
  {
    if (m->registration == power_key.registration)
    {
      power_active = 0U;
      PS_CalendarTransport_Invalidate(&transport);
      PS_HW6_Calendar_Cancel();
    }
  }
  else if (m->kind == CAL_DEFER)
  {
    if ((m->sequence == transport.delivery.sequence) &&
        (m->registration == transport.delivery.key.registration) &&
        (transport.delivery.state == PS_CALENDAR_DELIVERY_CLAIMED))
    {
      /* Runtime did not execute: return the occurrence to pending. A clock edit
       * during the grant interval invalidates it instead of reviving old work. */
      if (power_active &&
          (transport.delivery.key.clock == g_ps_calendar_probe.generation))
      {
        transport.delivery.state = PS_CALENDAR_DELIVERY_PENDING;
        transport.notification_sent = 0U;
        transport.grant_sent = 0U;
      }
      else { transport.delivery.state = PS_CALENDAR_DELIVERY_INVALIDATED; }
    }
  }
  else
  {
    result = PS_CalendarTransport_Receive(&transport, m);
    if ((m->kind == PS_CALENDAR_CLAIM) && (result != PS_CALENDAR_DELIVERY_OK))
    {
      /* One outstanding claim per runtime registration; retain rejection until
       * enqueued, so an invalidated notification cannot strand runtime. */
      power_out = Packet(CAL_REJECT, m->sequence, m->registration);
      power_out_valid = 1U;
    }
  }
  Publish();
}

void PS_HW6_CalendarRuntime_PowerService(uint32_t allowed)
{
  ps_calendar_message_t message;
  if (!allowed)
  {
    power_active = 0U;
    PS_CalendarTransport_Invalidate(&transport);
    PS_HW6_Calendar_Cancel();
  }
  if (power_out_valid)
  {
    if (PS_HW6_CalendarRuntime_Send(0U, &power_out) == 0U) { power_out_valid = 0U; }
    else { g_ps_calendar_runtime_probe.power_send_failures++; }
  }
  else if (PS_CalendarTransport_Next(&transport, &message))
  {
    if (PS_HW6_CalendarRuntime_Send(0U, &message) == 0U)
    { PS_CalendarTransport_Sent(&transport, &message); }
    else { g_ps_calendar_runtime_probe.power_send_failures++; }
  }
  Publish();
}

void PS_HW6_CalendarRuntime_RuntimeMessage(const ps_calendar_message_t *m)
{
  if ((m->magic != PS_CALENDAR_TRANSPORT_MAGIC) ||
      (m->registration != runtime_registration))
  { g_ps_calendar_runtime_probe.stale++; return; }
  if (m->kind == CAL_REGISTERED)
  {
    runtime_setup = 0U;
    runtime_active = m->sequence == PS_SYSTEM_TIME_OK;
    g_ps_calendar_runtime_probe.setup_status = m->sequence;
  }
  else if ((m->kind == PS_CALENDAR_NOTIFY) && runtime_active &&
           (m->sequence > runtime_done))
  {
    runtime_notice = m->sequence;
    g_ps_calendar_runtime_probe.notifications++;
  }
  else if ((m->kind == CAL_REJECT) && (m->sequence == runtime_notice))
  { runtime_setup = 0U; runtime_notice = 0U; g_ps_calendar_runtime_probe.stale++; }
  else if ((m->kind == PS_CALENDAR_GRANT) &&
           (m->sequence == runtime_notice) && (m->sequence > runtime_done) &&
           !runtime_out_valid)
  {
    ps_calendar_delivery_state_t outcome;
    uint32_t op;
    runtime_setup = 0U;
    g_ps_calendar_runtime_probe.grants++;
    if (!runtime_active || (PS_HW6_CalendarRuntime_Scene() != runtime_scene))
    { outcome = PS_CALENDAR_DELIVERY_IGNORED; }
    else if (!PS_HW6_CalendarRuntime_Running())
    {
      runtime_out = Packet(CAL_DEFER, m->sequence, runtime_registration);
      runtime_out_valid = 1U;
      runtime_notice = 0U;
      return;
    }
    else { outcome = PS_HW6_CalendarRuntime_Apply(runtime_binding); }
    if (outcome == PS_CALENDAR_DELIVERY_APPLIED)
    { op = PS_CALENDAR_ACK_APPLIED; g_ps_calendar_runtime_probe.applied++; }
    else if (outcome == PS_CALENDAR_DELIVERY_IGNORED)
    { op = PS_CALENDAR_ACK_IGNORED; g_ps_calendar_runtime_probe.ignored++; }
    else { op = PS_CALENDAR_ACK_FAILED; g_ps_calendar_runtime_probe.failed++; }
    runtime_done = m->sequence;
    runtime_notice = 0U;
    runtime_out = Packet(op, m->sequence, runtime_registration);
    runtime_out_valid = 1U;
  }
  else { g_ps_calendar_runtime_probe.stale++; }
}

void PS_HW6_CalendarRuntime_RuntimeService(void)
{
  uint32_t scene = PS_HW6_CalendarRuntime_Scene();
  uint32_t request = g_ps_calendar_runtime_probe.request;
  if (runtime_out_valid)
  {
    if (PS_HW6_CalendarRuntime_Send(1U, &runtime_out) == 0U)
    { runtime_out_valid = 0U; }
    else { g_ps_calendar_runtime_probe.runtime_send_failures++; }
  }
  if (!runtime_out_valid && !runtime_setup)
  {
    if (runtime_active && ((scene != runtime_scene) || (request == 4U)))
    {
      runtime_out = Packet(CAL_CANCEL, 0U, runtime_registration);
      runtime_out_valid = 1U;
      runtime_active = 0U;
      runtime_notice = 0U;
      if (request == 4U) { g_ps_calendar_runtime_probe.request = 0U; }
    }
    else if ((request != 0U) && !runtime_active)
    {
      g_ps_calendar_runtime_probe.request = 0U;
      if ((request > 3U) || !scene || !PS_HW6_CalendarRuntime_Running() ||
          !PS_HW6_CalendarRuntime_BindingValid(g_ps_calendar_runtime_probe.binding) ||
          (runtime_registration == UINT32_MAX))
      { g_ps_calendar_runtime_probe.setup_status = PS_SYSTEM_TIME_ARGUMENT; }
      else
      {
        runtime_registration++;
        runtime_scene = scene;
        runtime_binding = g_ps_calendar_runtime_probe.binding;
        intent = (calendar_intent_t){runtime_registration, scene, runtime_binding,
          request, g_ps_calendar_runtime_probe.time_of_day,
          g_ps_calendar_runtime_probe.day_offset};
        atomic_thread_fence(memory_order_release);
        runtime_setup = 1U;
        runtime_out = Packet(CAL_REGISTER, runtime_registration, scene);
        runtime_out_valid = 1U;
      }
    }
    else if (request != 0U)
    {
      g_ps_calendar_runtime_probe.request = 0U;
      g_ps_calendar_runtime_probe.setup_status = PS_SYSTEM_TIME_ARGUMENT;
    }
    else if (runtime_active && runtime_notice && PS_HW6_CalendarRuntime_Running())
    {
      runtime_out = Packet(PS_CALENDAR_CLAIM, runtime_notice, runtime_registration);
      runtime_out_valid = 1U;
      g_ps_calendar_runtime_probe.claims++;
      /* Keep the sequence for GRANT, but do not enqueue another claim. */
      runtime_setup = 2U;
    }
  }
  g_ps_calendar_runtime_probe.registration = runtime_registration;
  g_ps_calendar_runtime_probe.scene = runtime_scene;
  g_ps_calendar_runtime_probe.active = runtime_active;
  g_ps_calendar_runtime_probe.pending_ack = runtime_out_valid || runtime_setup;
}
