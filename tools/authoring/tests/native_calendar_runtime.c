#include <assert.h>
#include "ps_hw6_calendar_runtime.h"
#include "ps_hw6_calendar.h"
#define TX_TIMER_TICKS_PER_SECOND 100U
static uint32_t tick, scene = 1U, running = 1U, applications;
static uint32_t fail_power, fail_runtime;
static ps_system_time_snapshot_t snapshot = {
  .local = {2026, 9, 19, 23, 59, 40}, .generation = 1U};
static ps_calendar_delivery_state_t apply_result = PS_CALENDAR_DELIVERY_APPLIED;
typedef struct { ps_calendar_message_t message[16]; uint32_t read, count; } queue_t;
static queue_t qp, qr;
static uint32_t tx_time_get(void) { return tick; }
ps_system_time_status_t PS_HW6_Calendar_Read(ps_system_time_snapshot_t *out)
{ *out = snapshot; return PS_SYSTEM_TIME_OK; }
uint32_t PS_HW6_CalendarRuntime_Send(uint32_t to_power, const ps_calendar_message_t *m)
{
  queue_t *q = to_power ? &qp : &qr;
  if ((to_power ? fail_power : fail_runtime) || q->count == 16) { return 1U; }
  q->message[(q->read + q->count) % 16] = *m;
  q->count++;
  return 0U;
}
uint32_t PS_HW6_CalendarRuntime_Scene(void) { return scene; }
uint32_t PS_HW6_CalendarRuntime_Running(void) { return running; }
uint32_t PS_HW6_CalendarRuntime_BindingValid(uint32_t binding) { return binding == 0; }
ps_calendar_delivery_state_t PS_HW6_CalendarRuntime_Apply(uint32_t binding)
{ assert(binding == 0 && running); applications++; return apply_result; }
#include "calendar_owner.inc"
#include "calendar_runtime.inc"

static void receive(queue_t *q, uint32_t power)
{
  ps_calendar_message_t m;
  if (!q->count) { return; }
  m = q->message[q->read]; q->read = (q->read + 1) % 16; q->count--;
  if (power) { PS_HW6_CalendarRuntime_PowerMessage(&m); }
  else { PS_HW6_CalendarRuntime_RuntimeMessage(&m); }
}
static void pump(uint32_t count)
{
  while (count--)
  {
    receive(&qp, 1); receive(&qr, 0);
    PS_HW6_CalendarRuntime_RuntimeService();
    PS_HW6_Calendar_Service(tick, 1);
    PS_HW6_CalendarRuntime_PowerService(1);
    tick++;
  }
}
static void midnight(void)
{
  snapshot.local.day++; snapshot.local.hour = 0;
  snapshot.local.minute = 0; snapshot.local.second = 0;
  PS_HW6_Calendar_Finish();
}
static void arm(void)
{
  g_ps_calendar_runtime_probe.request = 1;
  pump(10);
  assert(g_ps_calendar_runtime_probe.active == 1);
  assert(g_ps_calendar_runtime_probe.setup_status == 0);
}
int main(void)
{
  uint32_t before;
  arm();
  fail_runtime = 1;
  midnight(); pump(10);
  assert(g_ps_calendar_runtime_probe.delivery_state == PS_CALENDAR_DELIVERY_PENDING);
  assert(applications == 0);
  assert(PS_HW6_Calendar_Remaining(tick) == UINT32_MAX);
  assert(PS_HW6_Calendar_Prepare(tick) == 0); /* no zero receive-wait spin */
  fail_runtime = 0;
  pump(20);
  assert(applications == 1 && g_ps_calendar_runtime_probe.applied == 1);
  assert(g_ps_calendar_runtime_probe.delivery_state == PS_CALENDAR_DELIVERY_APPLIED);
  pump(20); assert(applications == 1);

  /* Suspend before expiry: occurrence remains pending, no handler until resume. */
  running = 0; midnight(); pump(20);
  assert(applications == 1 && transport.delivery.state == PS_CALENDAR_DELIVERY_PENDING);
  assert(PS_HW6_Calendar_Prepare(tick) == UINT32_MAX);
  running = 1; pump(20); assert(applications == 2);

  /* Failed completion enqueue cannot execute the handler again. */
  midnight(); pump(3);
  fail_power = 1; pump(10);
  before = applications;
  assert(before == 3 && transport.delivery.state == PS_CALENDAR_DELIVERY_CLAIMED);
  assert(g_ps_calendar_runtime_probe.pending_ack);
  pump(10); assert(applications == before);
  fail_power = 0; pump(20); assert(applications == 3);

  /* A queued notification is invalidated by an explicit clock edit. */
  running = 0; midnight(); pump(10);
  snapshot.generation++; snapshot.local.hour = 12;
  PS_HW6_Calendar_TimeChanged();
  running = 1; pump(20); assert(applications == 3);
  assert(runtime_setup == 0);

  /* Scene replacement discards the old scene event before applying it. */
  running = 0; midnight(); pump(10);
  scene++; running = 1; pump(20);
  assert(applications == 3 && !power_active && !runtime_active);
  arm();
  apply_result = PS_CALENDAR_DELIVERY_IGNORED;
  midnight(); pump(20);
  assert(applications == 4 && g_ps_calendar_runtime_probe.ignored == 1);
  pump(20); assert(applications == 4);
  /* Replacement/cancel while a grant is queued must release the claimed slot. */
  midnight();
  pump(3);
  scene++; pump(20);
  assert(!runtime_active && !power_active);
  assert(transport.delivery.state != PS_CALENDAR_DELIVERY_CLAIMED);
  /* Suspend precisely between power admission and runtime receipt of GRANT. */
  arm();
  apply_result = PS_CALENDAR_DELIVERY_APPLIED;
  midnight();
  before = applications;
  {
    uint32_t turns;
    for (turns = 0; turns < 20; ++turns)
    {
      if (qr.count && qr.message[qr.read].kind == PS_CALENDAR_GRANT) { break; }
      pump(1);
    }
    assert(turns < 20);
  }
  running = 0; pump(10);
  assert(applications == before);
  assert(transport.delivery.state == PS_CALENDAR_DELIVERY_PENDING);
  running = 1; pump(20); assert(applications == before + 1);
  /* Explicit cancel then one-shot day-offset registration. */
  g_ps_calendar_runtime_probe.request = 4; pump(10);
  g_ps_calendar_runtime_probe.time_of_day = 43200;
  g_ps_calendar_runtime_probe.day_offset = 1;
  g_ps_calendar_runtime_probe.request = 2; pump(10);
  assert(g_ps_calendar_runtime_probe.setup_status == 0);
  assert(ps_calendar_timer.kind == PS_CALENDAR_ONCE && ps_calendar_timer.armed);
  before = applications;
  snapshot.local.day++; snapshot.local.hour = 12;
  PS_HW6_Calendar_Finish(); pump(20);
  assert(applications == before + 1 && ps_calendar_timer.consumed);
  pump(20); assert(applications == before + 1);
  return 0;
}
