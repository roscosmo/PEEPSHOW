#include "ps_package_source.h"

#include <stddef.h>

#include "ps_embedded_egg.h"

volatile uint32_t g_ps_package_source_override;
volatile ps_package_source_probe_t g_ps_package_source_probe =
{
  .api_version = PS_PACKAGE_SOURCE_API_VERSION,
  .selected_source = PS_PACKAGE_SOURCE_EMBEDDED,
  .generation = 1UL,
  .package_size = 0UL,
  .last_status = PS_PACKAGE_SOURCE_STATUS_NOT_RUN,
  .reason = PS_PACKAGE_SOURCE_REASON_NONE
};

static uint32_t PS_PackageSource_Fail(uint32_t reason)
{
  g_ps_package_source_probe.last_status = 1UL;
  g_ps_package_source_probe.reason = reason;
  return 1UL;
}

uint32_t PS_PackageSource_Resolve(ps_package_source_view_t *view)
{
  uint32_t selected_source;

  g_ps_package_source_probe.resolve_count++;
  g_ps_package_source_probe.last_status =
    PS_PACKAGE_SOURCE_STATUS_NOT_RUN;
  g_ps_package_source_probe.reason = PS_PACKAGE_SOURCE_REASON_NONE;
  if (view == NULL)
  {
    return PS_PackageSource_Fail(PS_PACKAGE_SOURCE_REASON_ARGUMENT);
  }

  view->blob = NULL;
  view->size = 0UL;
  view->source = PS_PACKAGE_SOURCE_NONE;
  view->generation = 0UL;

  if (g_ps_package_source_override ==
      (uint32_t)PS_PACKAGE_SOURCE_OVERRIDE_NONE)
  {
    selected_source = (uint32_t)PS_PACKAGE_SOURCE_NONE;
  }
  else if ((g_ps_package_source_override ==
            (uint32_t)PS_PACKAGE_SOURCE_OVERRIDE_DEFAULT) ||
           (g_ps_package_source_override ==
            (uint32_t)PS_PACKAGE_SOURCE_OVERRIDE_EMBEDDED))
  {
    selected_source = (uint32_t)PS_PACKAGE_SOURCE_EMBEDDED;
  }
  else
  {
    g_ps_package_source_probe.selected_source =
      (uint32_t)PS_PACKAGE_SOURCE_NONE;
    g_ps_package_source_probe.package_size = 0UL;
    return PS_PackageSource_Fail(PS_PACKAGE_SOURCE_REASON_OVERRIDE);
  }

  g_ps_package_source_probe.selected_source = selected_source;
  if (selected_source == (uint32_t)PS_PACKAGE_SOURCE_NONE)
  {
    g_ps_package_source_probe.unavailable_count++;
    g_ps_package_source_probe.package_size = 0UL;
    return PS_PackageSource_Fail(PS_PACKAGE_SOURCE_REASON_UNAVAILABLE);
  }

  view->blob = g_ps_embedded_egg;
  view->size = g_ps_embedded_egg_size;
  view->source = selected_source;
  view->generation = 1UL;
  g_ps_package_source_probe.success_count++;
  g_ps_package_source_probe.generation = view->generation;
  g_ps_package_source_probe.package_size = view->size;
  g_ps_package_source_probe.last_status = 0UL;
  return 0UL;
}
