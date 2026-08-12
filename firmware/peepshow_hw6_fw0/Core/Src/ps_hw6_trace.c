#include "ps_hw6_trace.h"

#include "knobs_autogen.h"
#include "stm32u5xx.h"
#include "tx_api.h"

volatile ps_hw6_trace_probe_t g_ps_hw6_trace_probe;

static void PS_HW6_TracePrimeProbe(void)
{
  if (g_ps_hw6_trace_probe.api_version != PS_HW6_TRACE_API_VERSION)
  {
    g_ps_hw6_trace_probe.api_version = PS_HW6_TRACE_API_VERSION;
    g_ps_hw6_trace_probe.last_status = PS_HW6_TRACE_STATUS_NOT_RUN;
    g_ps_hw6_trace_probe.swo_last_status = PS_HW6_TRACE_STATUS_NOT_RUN;
  }
}

static uint32_t PS_HW6_TraceSwoReady(void)
{
  if ((CoreDebug->DEMCR & CoreDebug_DEMCR_TRCENA_Msk) == 0UL)
  {
    return 0UL;
  }

  if ((ITM->TCR & ITM_TCR_ITMENA_Msk) == 0UL)
  {
    return 0UL;
  }

  if ((ITM->TER & 1UL) == 0UL)
  {
    return 0UL;
  }

  if (ITM->PORT[0U].u32 == 0UL)
  {
    return 0UL;
  }

  return 1UL;
}

static void PS_HW6_TraceSwoWriteToken(uint32_t token)
{
  ITM->PORT[0U].u8 = (uint8_t)(token & 0xFFUL);
  ITM->PORT[0U].u8 = (uint8_t)((token >> 8) & 0xFFUL);
  ITM->PORT[0U].u8 = (uint8_t)((token >> 16) & 0xFFUL);
  ITM->PORT[0U].u8 = (uint8_t)'\n';
}

void PS_HW6_TraceSwoLifecycle(uint32_t token)
{
  PS_HW6_TracePrimeProbe();

  g_ps_hw6_trace_probe.swo_last_token = token;

  if (KNOB_DEBUG_SWO_LIFECYCLE_ENABLE == 0UL)
  {
    g_ps_hw6_trace_probe.swo_disabled_count++;
    g_ps_hw6_trace_probe.swo_last_status = PS_HW6_TRACE_STATUS_DISABLED;
    return;
  }

  if (PS_HW6_TraceSwoReady() == 0UL)
  {
    g_ps_hw6_trace_probe.swo_drop_count++;
    g_ps_hw6_trace_probe.swo_last_status = PS_HW6_TRACE_STATUS_NOT_READY;
    return;
  }

  PS_HW6_TraceSwoWriteToken(token);
  g_ps_hw6_trace_probe.swo_emit_count++;
  g_ps_hw6_trace_probe.swo_last_status = 0UL;
}
static void PS_HW6_TraceInsert(uint32_t event_id,
                               uint32_t info1,
                               uint32_t info2,
                               uint32_t info3,
                               uint32_t info4)
{
  PS_HW6_TracePrimeProbe();

  g_ps_hw6_trace_probe.insert_count++;
  g_ps_hw6_trace_probe.last_event_id = event_id;
  g_ps_hw6_trace_probe.last_info1 = info1;
  g_ps_hw6_trace_probe.last_info2 = info2;
  g_ps_hw6_trace_probe.last_info3 = info3;
  g_ps_hw6_trace_probe.last_info4 = info4;

  if ((KNOB_DEBUG_TRACEX_ENABLE == 0UL) ||
      (KNOB_DEBUG_TRACEX_USER_EVENTS_ENABLE == 0UL))
  {
    g_ps_hw6_trace_probe.skipped_count++;
    g_ps_hw6_trace_probe.last_status = PS_HW6_TRACE_STATUS_DISABLED;
    return;
  }

#if defined(TX_ENABLE_EVENT_TRACE)
  {
    UINT status = tx_trace_user_event_insert((ULONG)event_id,
                                             (ULONG)info1,
                                             (ULONG)info2,
                                             (ULONG)info3,
                                             (ULONG)info4);
    g_ps_hw6_trace_probe.last_status = (uint32_t)status;
    if (status == TX_SUCCESS)
    {
      g_ps_hw6_trace_probe.success_count++;
    }
    else
    {
      g_ps_hw6_trace_probe.error_count++;
    }
  }
#else
  g_ps_hw6_trace_probe.skipped_count++;
  g_ps_hw6_trace_probe.last_status = PS_HW6_TRACE_STATUS_DISABLED;
#endif
}

void PS_HW6_TraceOwnerState(uint32_t owner_id,
                            uint32_t from_state,
                            uint32_t event,
                            uint32_t to_state)
{
  PS_HW6_TraceInsert(PS_HW6_TRACE_EVENT_OWNER_STATE,
                     owner_id,
                     from_state,
                     event,
                     to_state);
}

void PS_HW6_TraceOwnerReject(uint32_t owner_id,
                             uint32_t from_state,
                             uint32_t event,
                             uint32_t status)
{
  PS_HW6_TraceInsert(PS_HW6_TRACE_EVENT_OWNER_REJECT,
                     owner_id,
                     from_state,
                     event,
                     status);
}

void PS_HW6_TraceUiDispatch(uint32_t event,
                            uint32_t from_page,
                            uint32_t to_page,
                            uint32_t status)
{
  PS_HW6_TraceInsert(PS_HW6_TRACE_EVENT_UI_DISPATCH,
                     event,
                     from_page,
                     to_page,
                     status);
}

void PS_HW6_TraceInputButton(uint32_t button_id,
                             uint32_t destination_owner,
                             uint32_t send_status,
                             uint32_t reserved)
{
  PS_HW6_TraceInsert(PS_HW6_TRACE_EVENT_INPUT_BUTTON,
                     button_id,
                     destination_owner,
                     send_status,
                     reserved);
}

void PS_HW6_TracePowerStart(uint32_t event,
                            uint32_t hold_ticks,
                            uint32_t send_status,
                            uint32_t reserved)
{
  PS_HW6_TraceInsert(PS_HW6_TRACE_EVENT_POWER_START,
                     event,
                     hold_ticks,
                     send_status,
                     reserved);
}

void PS_HW6_TracePmicInterrupt(uint32_t pending_count,
                               uint32_t irq_count,
                               uint32_t level,
                               uint32_t snapshot_status)
{
  PS_HW6_TraceInsert(PS_HW6_TRACE_EVENT_PMIC_INTERRUPT,
                     pending_count,
                     irq_count,
                     level,
                     snapshot_status);
}

void PS_HW6_TraceSleep(uint32_t stage,
                       uint32_t reason,
                       uint32_t state,
                       uint32_t status)
{
  PS_HW6_TraceInsert(PS_HW6_TRACE_EVENT_SLEEP,
                     stage,
                     reason,
                     state,
                     status);
}

void PS_HW6_TraceClockPolicy(uint32_t profile,
                             uint32_t capabilities,
                             uint32_t status,
                             uint32_t sysclk_hz)
{
  PS_HW6_TraceInsert(PS_HW6_TRACE_EVENT_CLOCK_POLICY,
                     profile,
                     capabilities,
                     status,
                     sysclk_hz);
}
