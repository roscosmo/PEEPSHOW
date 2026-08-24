#ifndef PS_PACKAGE_SOURCE_H
#define PS_PACKAGE_SOURCE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PS_PACKAGE_SOURCE_API_VERSION    (3UL)
#define PS_PACKAGE_SOURCE_STATUS_NOT_RUN (0xFFFFFFFFUL)
#define PS_PACKAGE_SOURCE_STAGED_CAPACITY_BYTES (65536UL)

typedef enum
{
  PS_PACKAGE_SOURCE_NONE = 0,
  PS_PACKAGE_SOURCE_EMBEDDED,
  PS_PACKAGE_SOURCE_STAGED_RAM,
  PS_PACKAGE_SOURCE_INSTALLED_RAM
} ps_package_source_t;

typedef enum
{
  PS_PACKAGE_SOURCE_OVERRIDE_DEFAULT = 0,
  PS_PACKAGE_SOURCE_OVERRIDE_NONE,
  PS_PACKAGE_SOURCE_OVERRIDE_EMBEDDED,
  PS_PACKAGE_SOURCE_OVERRIDE_STAGED_RAM,
  PS_PACKAGE_SOURCE_OVERRIDE_INSTALLED_RAM
} ps_package_source_override_t;

typedef enum
{
  PS_PACKAGE_SOURCE_REASON_NONE = 0,
  PS_PACKAGE_SOURCE_REASON_ARGUMENT,
  PS_PACKAGE_SOURCE_REASON_UNAVAILABLE,
  PS_PACKAGE_SOURCE_REASON_OVERRIDE,
  PS_PACKAGE_SOURCE_REASON_CAPACITY
} ps_package_source_reason_t;

typedef struct
{
  const uint8_t *blob;
  uint32_t size;
  uint32_t source;
  uint32_t generation;
} ps_package_source_view_t;

typedef struct
{
  uint32_t api_version;
  uint32_t resolve_count;
  uint32_t success_count;
  uint32_t unavailable_count;
  uint32_t selected_source;
  uint32_t generation;
  uint32_t package_size;
  uint32_t staged_capacity;
  uint32_t staged_available;
  uint32_t staged_publish_count;
  uint32_t staged_invalidate_count;
  uint32_t resident_source;
  uint32_t installed_publish_count;
  uint32_t last_status;
  uint32_t reason;
} ps_package_source_probe_t;

extern volatile uint32_t g_ps_package_source_override;
extern volatile ps_package_source_probe_t g_ps_package_source_probe;

uint32_t PS_PackageSource_Resolve(ps_package_source_view_t *view);
uint32_t PS_PackageSource_BeginStagedWrite(uint8_t **buffer,
                                           uint32_t *capacity);
uint32_t PS_PackageSource_CommitStagedWrite(uint32_t size);
uint32_t PS_PackageSource_BeginInstalledWrite(uint8_t **buffer,
                                              uint32_t *capacity);
uint32_t PS_PackageSource_CommitInstalledWrite(uint32_t size,
                                               uint32_t generation);
void PS_PackageSource_AbortStagedWrite(void);

#ifdef __cplusplus
}
#endif

#endif /* PS_PACKAGE_SOURCE_H */
