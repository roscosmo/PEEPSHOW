#include <assert.h>
#include <stdint.h>
#include <string.h>
#include "ps_hw6_trace.h"
typedef uint32_t UINT;
typedef uintptr_t ULONG;
typedef void VOID;
#define TX_ENABLE_EVENT_TRACE
#define TX_SUCCESS 0U
#define TX_NOT_DONE 1U
#define TX_FEATURE_NOT_ENABLED 255U
#define KNOB_DEBUG_TRACEX_ENABLE 1U
#define KNOB_DEBUG_TRACEX_USER_EVENTS_ENABLE 1U
#define CoreDebug_DEMCR_TRCENA_Msk 1U
#define DWT_CTRL_CYCCNTENA_Msk 1U
static struct { uint32_t DEMCR; } core;
static struct { uint32_t CTRL, CYCCNT; } dwt;
#define CoreDebug (&core)
#define DWT (&dwt)
static uint32_t counter_runs = 1;
#define __NOP() (dwt.CYCCNT += counter_runs)
#define __DMB() ((void)0)
volatile ps_hw6_object_trace_probe_t g_ps_object_trace_probe;
volatile UINT g_ps_hw6_tracex_enable_status;
volatile ULONG g_ps_hw6_tracex_runtime_enabled, g_ps_hw6_tracex_buffer_address;
volatile ULONG g_ps_hw6_tracex_buffer_bytes = 32768, g_ps_hw6_tracex_registry_entries = 64;
static uint8_t buffer[32768];
static uint32_t running, enable_fail, insert_fail, freezes, events, last_event, last_a;
static VOID (*wrap_notify)(VOID *);
static UINT tx_trace_disable(void)
{ if (!running) { return TX_NOT_DONE; } running = 0; freezes++; return TX_SUCCESS; }
static UINT tx_trace_buffer_full_notify(VOID (*notify)(VOID *)) { wrap_notify = notify; return 0; }
static UINT tx_trace_enable(VOID *ptr, ULONG size, ULONG registry)
{
  assert(!running && ptr == buffer && size == sizeof(buffer) && registry == 64);
  if (enable_fail) { return 7; }
  running = 1; return 0;
}
static UINT tx_trace_user_event_insert(ULONG event, ULONG a, ULONG b, ULONG c, ULONG d)
{ (void)b; (void)c; (void)d; assert(running); events++; last_event = event; last_a = a; return insert_fail; }
static uint32_t HAL_RCC_GetHCLKFreq(void) { return 24000000; }
static uint32_t tx_time_get(void) { return 100; }
#include "trace_under_test.inc"

int main(void)
{
  g_ps_hw6_tracex_buffer_address = (ULONG)buffer;
  assert(!PS_HW6_TraceObjectArm(1));
  PS_HW6_TraceObjectBegin(1); PS_HW6_TraceObjectRaster(1, 0); PS_HW6_TraceObjectEnd(0);
  assert(events == 0);
  g_ps_object_trace_probe.request = 1;
  assert(!PS_HW6_TraceObjectArm(0) && events == 0);
  counter_runs = 0; g_ps_object_trace_probe.request = 1;
  assert(!PS_HW6_TraceObjectArm(1));
  assert(g_ps_object_trace_probe.arm_status == PS_HW6_TRACE_STATUS_NOT_READY);
  counter_runs = 1; enable_fail = 1; g_ps_object_trace_probe.request = 1;
  assert(!PS_HW6_TraceObjectArm(1) && !g_ps_hw6_tracex_runtime_enabled);
  enable_fail = 0; g_ps_object_trace_probe.request = 1;
  assert(PS_HW6_TraceObjectArm(1) && running && g_ps_object_trace_probe.armed);
  assert(last_event == PS_HW6_TRACE_EVENT_OBJECT_CAPTURE && last_a == 1);
  wrap_notify(buffer);
  PS_HW6_TraceObjectBegin(9);
  assert(g_ps_object_trace_probe.active && g_ps_object_trace_probe.wraps_at_receive == 1);
  assert(last_a == 2 && g_ps_object_trace_probe.sequence == 9);
  PS_HW6_TraceObjectStage(10, 23, 24000000);
  assert(last_event == PS_HW6_TRACE_EVENT_OBJECT_STAGE && last_a == 10);
  PS_HW6_TraceObjectRaster(3, 0);
  assert(last_event == PS_HW6_TRACE_EVENT_OBJECT_RASTER && last_a == 3);
  insert_fail = 8; PS_HW6_TraceObjectRaster(3, 1);
  assert(g_ps_object_trace_probe.marker_errors == 1);
  insert_fail = 0;
  PS_HW6_TraceObjectEnd(0);
  assert(g_ps_object_trace_probe.complete && !g_ps_object_trace_probe.active && !running);
  assert(g_ps_object_trace_probe.freeze_status == 0 && !g_ps_hw6_tracex_runtime_enabled);
  assert(last_event == PS_HW6_TRACE_EVENT_OBJECT_CAPTURE && last_a == 3);
  uint32_t saved = events;
  PS_HW6_TraceObjectRaster(1, 0); PS_HW6_TraceObjectStage(1, 2, 3); PS_HW6_TraceObjectEnd(1);
  assert(saved == events && freezes == 1);
  g_ps_object_trace_probe.request = 1;
  assert(PS_HW6_TraceObjectArm(1) && !g_ps_object_trace_probe.complete);
  assert(!g_ps_object_trace_probe.marker_errors && !g_ps_object_trace_probe.wraps);
  return 0;
}
