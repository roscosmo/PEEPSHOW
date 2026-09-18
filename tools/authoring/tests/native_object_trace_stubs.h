#ifndef NATIVE_OBJECT_TRACE_STUBS_H
#define NATIVE_OBJECT_TRACE_STUBS_H
#include "ps_hw6_trace.h"
volatile ps_hw6_object_trace_probe_t g_ps_object_trace_probe;
uint32_t PS_HW6_TraceObjectArm(uint32_t allowed) { (void)allowed; return 0; }
void PS_HW6_TraceObjectBegin(uint32_t sequence) { (void)sequence; }
void PS_HW6_TraceObjectStage(uint32_t stage, uint32_t token, uint32_t hclk)
{ (void)stage; (void)token; (void)hclk; }
void PS_HW6_TraceObjectRaster(uint32_t stage, uint32_t end)
{
#ifdef NATIVE_OBJECT_RASTER_OBSERVE
  NATIVE_OBJECT_RASTER_OBSERVE(stage, end);
#else
  (void)stage; (void)end;
#endif
}
void PS_HW6_TraceObjectEnd(uint32_t status) { (void)status; }
void PS_HW6_TraceObjectPanelBegin(void) { }
void PS_HW6_TraceObjectPanel(uint32_t stage, uint32_t end, uint32_t value)
{ (void)stage; (void)end; (void)value; }
void PS_HW6_TraceObjectPanelEnd(uint32_t status) { (void)status; }
uint32_t PS_HW6_TraceObjectOwnerBegin(uint32_t stage) { (void)stage; return 0; }
void PS_HW6_TraceObjectOwnerEnd(uint32_t stage, uint32_t sequence, uint32_t status)
{ (void)stage; (void)sequence; (void)status; }
#endif
