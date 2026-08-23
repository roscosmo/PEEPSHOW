#ifndef PS_PACKAGE_SOURCE_H
#define PS_PACKAGE_SOURCE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PS_PACKAGE_SOURCE_API_VERSION    (1UL)
#define PS_PACKAGE_SOURCE_STATUS_NOT_RUN (0xFFFFFFFFUL)

typedef enum
{
  PS_PACKAGE_SOURCE_NONE = 0,
  PS_PACKAGE_SOURCE_EMBEDDED
} ps_package_source_t;

typedef enum
{
  PS_PACKAGE_SOURCE_OVERRIDE_DEFAULT = 0,
  PS_PACKAGE_SOURCE_OVERRIDE_NONE,
  PS_PACKAGE_SOURCE_OVERRIDE_EMBEDDED
} ps_package_source_override_t;

typedef enum
{
  PS_PACKAGE_SOURCE_REASON_NONE = 0,
  PS_PACKAGE_SOURCE_REASON_ARGUMENT,
  PS_PACKAGE_SOURCE_REASON_UNAVAILABLE,
  PS_PACKAGE_SOURCE_REASON_OVERRIDE
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
  uint32_t last_status;
  uint32_t reason;
} ps_package_source_probe_t;

extern volatile uint32_t g_ps_package_source_override;
extern volatile ps_package_source_probe_t g_ps_package_source_probe;

uint32_t PS_PackageSource_Resolve(ps_package_source_view_t *view);

#ifdef __cplusplus
}
#endif

#endif /* PS_PACKAGE_SOURCE_H */
