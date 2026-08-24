#include "ps_package_source.h"

#include <stddef.h>

#include "ps_embedded_egg.h"

volatile uint32_t g_ps_package_source_override;
volatile ps_package_source_probe_t g_ps_package_source_probe =
{
  .api_version = PS_PACKAGE_SOURCE_API_VERSION,
  .selected_source = PS_PACKAGE_SOURCE_NONE,
  .generation = 0UL,
  .package_size = 0UL,
  .staged_capacity = PS_PACKAGE_SOURCE_STAGED_CAPACITY_BYTES,
  .last_status = PS_PACKAGE_SOURCE_STATUS_NOT_RUN,
  .reason = PS_PACKAGE_SOURCE_REASON_NONE
};

static uint8_t s_ps_package_source_staged[
  PS_PACKAGE_SOURCE_STAGED_CAPACITY_BYTES] __attribute__((aligned(4)));
static volatile uint32_t s_ps_package_source_staged_size;
static volatile uint32_t s_ps_package_source_staged_generation = 1UL;
static volatile uint32_t s_ps_package_source_staged_available;
static volatile uint32_t s_ps_package_source_resident_source;

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
            (uint32_t)PS_PACKAGE_SOURCE_OVERRIDE_DEFAULT) &&
           (s_ps_package_source_staged_available != 0UL))
  {
    selected_source = s_ps_package_source_resident_source;
  }
  else if (g_ps_package_source_override ==
           (uint32_t)PS_PACKAGE_SOURCE_OVERRIDE_EMBEDDED)
  {
    selected_source = (uint32_t)PS_PACKAGE_SOURCE_EMBEDDED;
  }
  else if (g_ps_package_source_override ==
           (uint32_t)PS_PACKAGE_SOURCE_OVERRIDE_STAGED_RAM)
  {
    selected_source = (uint32_t)PS_PACKAGE_SOURCE_STAGED_RAM;
  }
  else if (g_ps_package_source_override ==
           (uint32_t)PS_PACKAGE_SOURCE_OVERRIDE_INSTALLED_RAM)
  {
    selected_source = (uint32_t)PS_PACKAGE_SOURCE_INSTALLED_RAM;
  }
  else if (g_ps_package_source_override ==
           (uint32_t)PS_PACKAGE_SOURCE_OVERRIDE_DEFAULT)
  {
    selected_source = (uint32_t)PS_PACKAGE_SOURCE_NONE;
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
  if (((selected_source == (uint32_t)PS_PACKAGE_SOURCE_STAGED_RAM) ||
       (selected_source == (uint32_t)PS_PACKAGE_SOURCE_INSTALLED_RAM)) &&
      ((s_ps_package_source_staged_available == 0UL) ||
       (s_ps_package_source_resident_source != selected_source)))
  {
    g_ps_package_source_probe.unavailable_count++;
    g_ps_package_source_probe.package_size = 0UL;
    return PS_PackageSource_Fail(PS_PACKAGE_SOURCE_REASON_UNAVAILABLE);
  }

  if ((selected_source == (uint32_t)PS_PACKAGE_SOURCE_STAGED_RAM) ||
      (selected_source == (uint32_t)PS_PACKAGE_SOURCE_INSTALLED_RAM))
  {
    view->blob = s_ps_package_source_staged;
    view->size = s_ps_package_source_staged_size;
    view->generation = s_ps_package_source_staged_generation;
  }
  else
  {
    view->blob = g_ps_embedded_egg;
    view->size = g_ps_embedded_egg_size;
    view->generation = 1UL;
  }
  view->source = selected_source;
  g_ps_package_source_probe.success_count++;
  g_ps_package_source_probe.generation = view->generation;
  g_ps_package_source_probe.package_size = view->size;
  g_ps_package_source_probe.last_status = 0UL;
  return 0UL;
}

uint32_t PS_PackageSource_BeginStagedWrite(uint8_t **buffer,
                                           uint32_t *capacity)
{
  if ((buffer == NULL) || (capacity == NULL))
  {
    return PS_PackageSource_Fail(PS_PACKAGE_SOURCE_REASON_ARGUMENT);
  }

  if (s_ps_package_source_staged_available != 0UL)
  {
    g_ps_package_source_probe.staged_invalidate_count++;
  }
  s_ps_package_source_staged_available = 0UL;
  s_ps_package_source_staged_size = 0UL;
  s_ps_package_source_resident_source = (uint32_t)PS_PACKAGE_SOURCE_NONE;
  g_ps_package_source_probe.staged_available = 0UL;
  g_ps_package_source_probe.resident_source =
    (uint32_t)PS_PACKAGE_SOURCE_NONE;
  *buffer = s_ps_package_source_staged;
  *capacity = PS_PACKAGE_SOURCE_STAGED_CAPACITY_BYTES;
  return 0UL;
}

uint32_t PS_PackageSource_CommitStagedWrite(uint32_t size)
{
  if ((size == 0UL) ||
      (size > PS_PACKAGE_SOURCE_STAGED_CAPACITY_BYTES))
  {
    return PS_PackageSource_Fail(PS_PACKAGE_SOURCE_REASON_CAPACITY);
  }

  s_ps_package_source_staged_size = size;
  s_ps_package_source_staged_generation++;
  s_ps_package_source_resident_source =
    (uint32_t)PS_PACKAGE_SOURCE_STAGED_RAM;
  s_ps_package_source_staged_available = 1UL;
  g_ps_package_source_probe.staged_publish_count++;
  g_ps_package_source_probe.staged_available = 1UL;
  g_ps_package_source_probe.generation =
    s_ps_package_source_staged_generation;
  g_ps_package_source_probe.package_size = size;
  g_ps_package_source_probe.resident_source =
    (uint32_t)PS_PACKAGE_SOURCE_STAGED_RAM;
  g_ps_package_source_probe.last_status = 0UL;
  g_ps_package_source_probe.reason = PS_PACKAGE_SOURCE_REASON_NONE;
  return 0UL;
}

uint32_t PS_PackageSource_BeginInstalledWrite(uint8_t **buffer,
                                              uint32_t *capacity)
{
  return PS_PackageSource_BeginStagedWrite(buffer, capacity);
}

uint32_t PS_PackageSource_CommitInstalledWrite(uint32_t size,
                                               uint32_t generation)
{
  if ((size == 0UL) ||
      (size > PS_PACKAGE_SOURCE_STAGED_CAPACITY_BYTES) ||
      (generation == 0UL))
  {
    return PS_PackageSource_Fail(PS_PACKAGE_SOURCE_REASON_CAPACITY);
  }

  s_ps_package_source_staged_size = size;
  s_ps_package_source_staged_generation = generation;
  s_ps_package_source_resident_source =
    (uint32_t)PS_PACKAGE_SOURCE_INSTALLED_RAM;
  s_ps_package_source_staged_available = 1UL;
  g_ps_package_source_probe.installed_publish_count++;
  g_ps_package_source_probe.staged_available = 1UL;
  g_ps_package_source_probe.resident_source =
    (uint32_t)PS_PACKAGE_SOURCE_INSTALLED_RAM;
  g_ps_package_source_probe.generation = generation;
  g_ps_package_source_probe.package_size = size;
  g_ps_package_source_probe.last_status = 0UL;
  g_ps_package_source_probe.reason = PS_PACKAGE_SOURCE_REASON_NONE;
  return 0UL;
}

void PS_PackageSource_AbortStagedWrite(void)
{
  if (s_ps_package_source_staged_available != 0UL)
  {
    g_ps_package_source_probe.staged_invalidate_count++;
  }
  s_ps_package_source_staged_available = 0UL;
  s_ps_package_source_staged_size = 0UL;
  s_ps_package_source_resident_source = (uint32_t)PS_PACKAGE_SOURCE_NONE;
  g_ps_package_source_probe.staged_available = 0UL;
  g_ps_package_source_probe.resident_source =
    (uint32_t)PS_PACKAGE_SOURCE_NONE;
}
