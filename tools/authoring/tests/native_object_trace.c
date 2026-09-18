#include <assert.h>
#include <stdint.h>
#include <string.h>
#include "ps_hw6_trace.h"
#include "ps_hw6_object_latency.h"
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
static struct { uint32_t LOAD, VAL, CTRL; } systick;
static struct { uint32_t ICSR; } scb;
#define SysTick (&systick)
#define SCB (&scb)
#define SysTick_CTRL_ENABLE_Msk 1U
#define SysTick_CTRL_TICKINT_Msk 2U
#define SCB_ICSR_PENDSTCLR_Msk (1U << 25)
#define __DSB() ((void)0)
#define __ISB() ((void)0)
static struct
{
  uint32_t stop2_systick_ctrl_before, stop2_systick_icsr_before;
  uint32_t stop2_systick_ctrl_sleep, stop2_systick_icsr_sleep;
  uint32_t stop2_systick_ctrl_after, stop2_systick_icsr_after;
} g_ps_hw6_owner_sm_probe;
#define TX_TIMER_TICKS_PER_SECOND 100U
static uint32_t hclk_hz = 24000000U;
static uint32_t kernel_tick = 100U;
static uint32_t counter_runs = 1;
#define __NOP() (dwt.CYCCNT += counter_runs)
#define __DMB() ((void)0)
volatile ps_hw6_object_trace_probe_t g_ps_object_trace_probe;
static uint32_t ps_object_panel_sequence;
volatile UINT g_ps_hw6_tracex_enable_status;
volatile ULONG g_ps_hw6_tracex_runtime_enabled, g_ps_hw6_tracex_buffer_address;
volatile ULONG g_ps_hw6_tracex_buffer_bytes = 32768, g_ps_hw6_tracex_registry_entries = 64;
static uint8_t buffer[32768];
static uint32_t running, enable_fail, insert_fail, freezes, events, last_event, last_a;
static uint32_t last_b, last_c, last_d;
static uint32_t records[256][5];
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
{ assert(running); assert(events < 256U);
  records[events][0] = event; records[events][1] = a; records[events][2] = b;
  records[events][3] = c; records[events][4] = d;
  events++; last_event = event; last_a = a;
  last_b = b; last_c = c; last_d = d; return insert_fail; }
static uint32_t HAL_RCC_GetHCLKFreq(void) { return hclk_hz; }
static uint32_t tx_time_get(void) { return kernel_tick; }
#include "trace_under_test.inc"

#define PS_HW6_RUNTIME_LIFECYCLE_RUNNING 2U
#define PS_UI_ROUTER_PAGE_RUNTIME_HANDOFF 6U
static uint32_t object_active;
static uint32_t PS_SceneRuntime_DevelopmentObjectsActive(void) { return object_active; }
static struct { uint32_t runtime_lifecycle; } g_ps_hw6_rtos_probe;
static struct { uint32_t current_page; } g_ps_ui_router_probe;
static struct { uint32_t leased; } g_ps_object_candidate_probe;
static struct { uint32_t lease_fault, render_request, render_complete; } g_ps_object_development_probe;
static struct { uint32_t enabled; } g_ps_object_lpbam_probe;
volatile ps_hw6_object_latency_probe_t g_ps_object_latency_probe;
#include "trace_service_under_test.inc"

static void test_runtime_arm_eligibility(void)
{
  struct { volatile uint32_t *field; uint32_t blocked; } cases[] = {
    {&object_active, 0},
    {&g_ps_hw6_rtos_probe.runtime_lifecycle, 3},
    {&g_ps_ui_router_probe.current_page, 2},
    {&g_ps_object_latency_probe.active, 1},
    {&g_ps_object_latency_probe.request, 1},
    {&g_ps_object_candidate_probe.leased, 1},
    {&g_ps_object_development_probe.lease_fault, 1},
    {&g_ps_object_development_probe.render_request, 1}
  };
  object_active = 1;
  g_ps_hw6_rtos_probe.runtime_lifecycle = 2;
  g_ps_ui_router_probe.current_page = 6;
  for (uint32_t index = 0; index < sizeof(cases) / sizeof(cases[0]); ++index)
  {
    uint32_t old = *cases[index].field;
    *cases[index].field = cases[index].blocked;
    uint32_t latency_request = g_ps_object_latency_probe.request;
    g_ps_object_trace_probe.request = 1;
    PS_HW6_RTOS_ObjectTraceService();
    assert(!g_ps_object_trace_probe.request && !g_ps_object_trace_probe.armed);
    assert(g_ps_object_trace_probe.arm_status == TX_NOT_DONE);
    assert(g_ps_object_latency_probe.request == latency_request);
    *cases[index].field = old;
  }
  for (uint32_t autonomous = 0; autonomous <= 1; ++autonomous)
  {
    g_ps_object_lpbam_probe.enabled = autonomous;
    g_ps_object_trace_probe.request = 1;
    PS_HW6_RTOS_ObjectTraceService();
    assert(g_ps_object_trace_probe.api_version == 3);
    assert(g_ps_object_trace_probe.armed && g_ps_object_latency_probe.request);
    assert(g_ps_object_lpbam_probe.enabled == autonomous);
    uint32_t saved_events = events;
    g_ps_object_trace_probe.request = 1; /* Pending capture cannot be replaced. */
    PS_HW6_RTOS_ObjectTraceService();
    assert(events == saved_events && g_ps_object_trace_probe.armed);
    g_ps_object_latency_probe.request = 0;
    PS_HW6_TraceObjectBegin(11 + autonomous);
    PS_HW6_TraceObjectStage(0, 0, 24000000);
    PS_HW6_TraceObjectEnd(autonomous);
    assert(g_ps_object_trace_probe.complete && !running);
    assert(last_a == 3 && last_b == 11 + autonomous && last_d == autonomous);
    assert(g_ps_object_lpbam_probe.enabled == autonomous);
  }
}

int main(void)
{
  g_ps_hw6_tracex_buffer_address = (ULONG)buffer;
  ps_hw6_trace_systick_snapshot_t snapshot;
  systick.LOAD = 239999U; systick.VAL = 119999U; systick.CTRL = 7U;
  scb.ICSR = 1U << 26;
  PS_HW6_TraceSysTickBefore(&snapshot, 239999U);
  assert(snapshot.sequence == 0U);
  PS_HW6_TraceSysTickAfter(&snapshot);
  PS_HW6_TraceSysTickBefore(NULL, 0U); PS_HW6_TraceSysTickAfter(NULL);
  assert(PS_HW6_ClockPolicy_RetuneThreadXSysTick() == TX_SUCCESS);
  assert(systick.LOAD == 239999U && systick.VAL == 119999U && systick.CTRL == 7U && events == 0U);
  assert(!PS_HW6_TraceObjectArm(1));
  PS_HW6_TraceObjectBegin(1); PS_HW6_TraceObjectRaster(1, 0); PS_HW6_TraceObjectEnd(0);
  assert(events == 0);
  assert(PS_HW6_TraceObjectOwnerBegin(PS_TRACE_OWNER_PMIC_SNAPSHOT) == 0);
  PS_HW6_TraceObjectOwnerEnd(PS_TRACE_OWNER_PMIC_SNAPSHOT, 0, 0);
  assert(events == 0);
  PS_HW6_TraceObjectPanelBegin();
  PS_HW6_TraceObjectPanel(PS_TRACE_PANEL_COMPOSE, 0, 12);
  PS_HW6_TraceObjectPanelEnd(0);
  assert(events == 0 && ps_object_panel_sequence == 0);
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
  systick.VAL = 119999U;
  uint32_t start = events;
  uint32_t cycles = dwt.CYCCNT;
  for (uint32_t i = 0U; i < 6U; ++i)
  {
    assert(PS_HW6_ClockPolicy_RetuneThreadXSysTick() == TX_SUCCESS);
    assert(systick.LOAD == 239999U && systick.VAL == 119999U);
    assert(systick.CTRL == 7U && scb.ICSR == (1U << 26) && events == start);
  }
  /* Execute the production STOP2 control paths around a no-op rate check. */
  uint32_t saved_ctrl = PS_HW6_SM_SuspendThreadXSystick();
  assert(saved_ctrl == 7U && systick.CTRL == 4U && systick.VAL == 119999U);
  assert(PS_HW6_ClockPolicy_RetuneThreadXSysTick() == TX_SUCCESS);
  assert(systick.VAL == 119999U && systick.CTRL == 4U && events == start);
  PS_HW6_SM_RestoreThreadXSystick(saved_ctrl);
  assert(systick.CTRL == 7U && systick.VAL == 119999U && systick.LOAD == 239999U);
  /* Real ICSR is write-one-to-clear; this fake records the written mask. */
  assert(scb.ICSR == SCB_ICSR_PENDSTCLR_Msk);
  scb.ICSR = 1U << 26;
  const uint32_t boundary_values[] = {0U, 1U, 239999U};
  for (uint32_t i = 0U; i < 3U; ++i)
  {
    systick.VAL = boundary_values[i];
    assert(PS_HW6_ClockPolicy_RetuneThreadXSysTick() == TX_SUCCESS);
    assert(systick.VAL == boundary_values[i] && scb.ICSR == (1U << 26));
    assert(events == start);
  }
  systick.VAL = 119999U;
  /* Genuine changes still write LOAD/VAL and emit the diagnostic triple. */
  hclk_hz = 48000000U;
  assert(PS_HW6_ClockPolicy_RetuneThreadXSysTick() == TX_SUCCESS);
  assert(events == start + 3U && systick.LOAD == 479999U && systick.VAL == 0U);
  assert(records[start][0] == PS_HW6_TRACE_EVENT_SYSTICK_REGISTERS);
  assert(records[start][1] == 9U && records[start][2] == 239999U);
  assert(records[start][3] == 119999U && records[start][4] == 479999U);
  assert(records[start + 1U][0] == PS_HW6_TRACE_EVENT_SYSTICK_BEFORE);
  assert(records[start + 2U][0] == PS_HW6_TRACE_EVENT_SYSTICK_AFTER);
  for (uint32_t i = start + 1U; i <= start + 2U; ++i)
  {
    assert(records[i][1] == 9U && records[i][2] == cycles);
    assert(records[i][3] == 100U && records[i][4] == (1U << 26));
  }
  /* Returning to the base rate also retunes, including a disabled counter. */
  hclk_hz = 24000000U; systick.CTRL = 4U; systick.VAL = 98765U;
  assert(PS_HW6_ClockPolicy_RetuneThreadXSysTick() == TX_SUCCESS);
  assert(systick.LOAD == 239999U && systick.VAL == 0U && systick.CTRL == 4U);
  systick.CTRL = 7U;
  hclk_hz = 0U; systick.VAL = 123U; start = events;
  assert(PS_HW6_ClockPolicy_RetuneThreadXSysTick() == TX_NOT_DONE);
  assert(systick.LOAD == 239999U && systick.VAL == 123U && events == start);
  hclk_hz = 24000000U;
  /* Preserve wrapped cycles and a tick/pending change for offline analysis. */
  dwt.CYCCNT = UINT32_MAX - 15U;
  PS_HW6_TraceSysTickBefore(&snapshot, 239999U);
  dwt.CYCCNT = 16U; kernel_tick = 101U; scb.ICSR = 0U;
  start = events;
  PS_HW6_TraceSysTickAfter(&snapshot);
  assert(records[start + 1U][2] == UINT32_MAX - 15U);
  assert(records[start + 1U][3] == 100U && records[start + 1U][4] == (1U << 26));
  assert(records[start + 2U][2] == 16U);
  assert(records[start + 2U][3] == 101U && records[start + 2U][4] == 0U);
  kernel_tick = 100U; scb.ICSR = 1U << 26;
  /* Snapshot reads do not alter timer or pending-exception state. */
  PS_HW6_TraceSysTickBefore(&snapshot, 239999U);
  assert(systick.VAL == 123U && systick.CTRL == 7U && scb.ICSR == (1U << 26));
  PS_HW6_TraceObjectStage(10, 23, 24000000);
  assert(last_event == PS_HW6_TRACE_EVENT_OBJECT_STAGE && last_a == 10);
  PS_HW6_TraceObjectRaster(3, 0);
  assert(last_event == PS_HW6_TRACE_EVENT_OBJECT_RASTER && last_a == 3);
  start = events;
  PS_HW6_TraceObjectPanel(PS_TRACE_PANEL_CLEAR, 0, 3024);
  assert(events == start); /* Candidate work is outside the panel scope. */
  PS_HW6_TraceObjectPanelBegin();
  assert(last_event == PS_HW6_TRACE_EVENT_OBJECT_PANEL && last_a == PS_TRACE_PANEL_TOTAL);
  assert(last_b == 0 && last_c == 9 && last_d == UINT32_MAX);
  PS_HW6_TraceObjectPanel(PS_TRACE_PANEL_DMA_START, 0, 582);
  assert(last_a == PS_TRACE_PANEL_DMA_START && last_b == 0 && last_d == 582);
  PS_HW6_TraceObjectPanel(PS_TRACE_PANEL_DMA_START, 1, 2);
  assert(last_a == PS_TRACE_PANEL_DMA_START && last_b == 1 && last_d == 2);
  PS_HW6_TraceObjectPanelEnd(2);
  assert(last_a == PS_TRACE_PANEL_TOTAL && last_b == 1 && last_d == 2);
  assert(ps_object_panel_sequence == 0);
  start = events;
  PS_HW6_TraceObjectPanel(PS_TRACE_PANEL_COMMIT, 0, 3024);
  PS_HW6_TraceObjectPanelEnd(0);
  assert(events == start);
  PS_HW6_TraceObjectPanelBegin();
  insert_fail = 8;
  PS_HW6_TraceObjectPanel(PS_TRACE_PANEL_CLEAR, 0, 3024);
  assert(g_ps_object_trace_probe.marker_errors == 1);
  insert_fail = 0;
  PS_HW6_TraceObjectPanelEnd(0);
  g_ps_object_trace_probe.marker_errors = 0;
  for (uint32_t stage = PS_TRACE_OWNER_PMIC_SNAPSHOT; stage <= PS_TRACE_OWNER_JOYSTICK_SUSPEND; ++stage)
  {
    uint32_t token = PS_HW6_TraceObjectOwnerBegin(stage);
    assert(token == 9 && last_event == PS_HW6_TRACE_EVENT_OBJECT_OWNER);
    assert(last_a == stage && last_b == 0 && last_c == token && last_d == UINT32_MAX);
    uint32_t saved_events = events;
    PS_HW6_TraceObjectOwnerEnd(stage, 0, 0);
    PS_HW6_TraceObjectOwnerEnd(stage, token + 1, 0);
    assert(events == saved_events);
    PS_HW6_TraceObjectOwnerEnd(stage, token, stage == PS_TRACE_OWNER_PMIC_SNAPSHOT ? 0 : 5);
    assert(last_a == stage && last_b == 1 && last_c == token);
    assert(last_d == (stage == PS_TRACE_OWNER_PMIC_SNAPSHOT ? 0U : 5U));
  }
  insert_fail = 8; PS_HW6_TraceObjectRaster(3, 1);
  assert(g_ps_object_trace_probe.marker_errors == 1);
  PS_HW6_TraceObjectOwnerEnd(PS_TRACE_OWNER_JOYSTICK_READ, 9, 0);
  assert(g_ps_object_trace_probe.marker_errors == 2);
  insert_fail = 0;
  PS_HW6_TraceObjectPanelBegin(); /* Runtime may time out during a presentation. */
  PS_HW6_TraceObjectEnd(0);
  assert(ps_object_panel_sequence == 0);
  assert(g_ps_object_trace_probe.complete && !g_ps_object_trace_probe.active && !running);
  assert(g_ps_object_trace_probe.freeze_status == 0 && !g_ps_hw6_tracex_runtime_enabled);
  assert(last_event == PS_HW6_TRACE_EVENT_OBJECT_CAPTURE && last_a == 3);
  uint32_t saved = events;
  PS_HW6_TraceObjectPanel(PS_TRACE_PANEL_COMPOSE, 1, 100);
  PS_HW6_TraceObjectPanelEnd(0);
  assert(saved == events);
  PS_HW6_TraceObjectRaster(1, 0); PS_HW6_TraceObjectStage(1, 2, 3); PS_HW6_TraceObjectEnd(1);
  assert(saved == events && freezes == 1);
  PS_HW6_TraceSysTickAfter(&snapshot);
  assert(saved == events);
  PS_HW6_TraceObjectOwnerEnd(PS_TRACE_OWNER_JOYSTICK_READ, 9, 0);
  assert(saved == events);
  g_ps_object_trace_probe.request = 1;
  assert(PS_HW6_TraceObjectArm(1) && !g_ps_object_trace_probe.complete);
  assert(!g_ps_object_trace_probe.marker_errors && !g_ps_object_trace_probe.wraps);
  PS_HW6_TraceObjectBegin(10);
  saved = events;
  PS_HW6_TraceObjectPanelEnd(0); /* Late end must not enter the next capture. */
  PS_HW6_TraceObjectPanel(PS_TRACE_PANEL_COMPOSE, 1, 100);
  assert(saved == events);
  PS_HW6_TraceSysTickAfter(&snapshot);
  assert(saved == events);
  PS_HW6_TraceObjectOwnerEnd(PS_TRACE_OWNER_JOYSTICK_READ, 9, 0);
  assert(saved == events);
  insert_fail = 8U; systick.LOAD = 479999U;
  assert(PS_HW6_ClockPolicy_RetuneThreadXSysTick() == TX_SUCCESS);
  assert(g_ps_object_trace_probe.marker_errors == 3U);
  assert(systick.LOAD == 239999U && systick.VAL == 0U);
  insert_fail = 0U;
  PS_HW6_TraceObjectEnd(0);
  test_runtime_arm_eligibility();
  return 0;
}
