#ifndef PS_PACKAGE_WORKFLOW_H
#define PS_PACKAGE_WORKFLOW_H

#include <stdint.h>

#define PS_PACKAGE_WORKFLOW_API_VERSION (1UL)
#define PS_PACKAGE_WORKFLOW_DISPLAY_BASE (200UL)

typedef enum
{
  PS_PACKAGE_WORKFLOW_IDLE = 0,
  PS_PACKAGE_WORKFLOW_STARTING,
  PS_PACKAGE_WORKFLOW_PREPARING,
  PS_PACKAGE_WORKFLOW_SCANNING,
  PS_PACKAGE_WORKFLOW_READING,
  PS_PACKAGE_WORKFLOW_VALIDATING,
  PS_PACKAGE_WORKFLOW_ERASING,
  PS_PACKAGE_WORKFLOW_PROGRAMMING,
  PS_PACKAGE_WORKFLOW_VERIFYING,
  PS_PACKAGE_WORKFLOW_COMMITTING,
  PS_PACKAGE_WORKFLOW_LOADING,
  PS_PACKAGE_WORKFLOW_LAUNCHING,
  PS_PACKAGE_WORKFLOW_EXPORTING,
  PS_PACKAGE_WORKFLOW_RECLAIMING,
  PS_PACKAGE_WORKFLOW_DONE,
  PS_PACKAGE_WORKFLOW_ERROR,
  PS_PACKAGE_WORKFLOW_PHASE_COUNT
} ps_package_workflow_phase_t;

typedef struct
{
  uint32_t api_version;
  uint32_t sequence;
  uint32_t active;
  uint32_t action;
  uint32_t phase;
  uint32_t status;
  uint32_t terminal_event;
  uint32_t tick_hz;
  uint32_t accepted_tick;
  uint32_t displayed_tick;
  uint32_t work_start_tick;
  uint32_t end_tick;
  uint32_t phase_start_tick;
  uint32_t display_status;
  uint32_t display_send_failures;
  uint32_t duplicate_count;
  uint32_t phase_ticks[PS_PACKAGE_WORKFLOW_PHASE_COUNT];
  uint32_t phase_cpu_hz[PS_PACKAGE_WORKFLOW_PHASE_COUNT];
  uint32_t phase_ospi_hz[PS_PACKAGE_WORKFLOW_PHASE_COUNT];
  uint32_t phase_count[PS_PACKAGE_WORKFLOW_PHASE_COUNT];
  uint32_t validation_count;
  uint32_t validation_status;
  uint32_t validation_scene;
  uint32_t validation_reason;
} ps_package_workflow_probe_t;

extern volatile ps_package_workflow_probe_t g_ps_package_workflow_probe;

/* Owner-thread calls only; progress never authorizes a flash operation. */
void PS_HW6_RTOS_PackageProgress(uint32_t phase);
uint32_t PS_HW6_RTOS_ValidatePackage(const uint8_t *blob, uint32_t size);
uint32_t PS_HW6_RTOS_PackageValidationBusy(void);

#endif
