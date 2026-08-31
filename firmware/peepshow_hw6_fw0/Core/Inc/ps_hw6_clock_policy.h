#ifndef PS_HW6_CLOCK_POLICY_H
#define PS_HW6_CLOCK_POLICY_H

#include <stdint.h>

#include "tx_api.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PS_HW6_CLOCK_POLICY_API_VERSION (11UL)
#define PS_HW6_CLOCK_POLICY_STATUS_NOT_RUN (0xFFFFFFFFUL)
#define PS_HW6_CLOCK_REQUESTER_COUNT (9U)

typedef enum
{
  PS_HW6_CLOCK_PROFILE_UNKNOWN = 0,
  PS_HW6_CLOCK_PROFILE_BOOT_RECOVERY,
  PS_HW6_CLOCK_PROFILE_REACTIVE_BASE,
  PS_HW6_CLOCK_PROFILE_REACTIVE_BURST,
  PS_HW6_CLOCK_PROFILE_REALTIME_BALANCED,
  PS_HW6_CLOCK_PROFILE_IO_HIGH,
  PS_HW6_CLOCK_PROFILE_STOP_PREP
} PS_HW6_ClockProfile;

#define PS_HW6_CLOCK_CAP_USB_DEVICE_ACTIVE \
  (1UL << 0)
#define PS_HW6_CLOCK_CAP_OCTOSPI_ACTIVE \
  (1UL << 1)
#define PS_HW6_CLOCK_CAP_SAI_AUDIO_ACTIVE \
  (1UL << 2)
#define PS_HW6_CLOCK_CAP_DISPLAY_TRANSFER_ACTIVE \
  (1UL << 3)
#define PS_HW6_CLOCK_CAP_REALTIME_DEADLINE_ACTIVE \
  (1UL << 4)
#define PS_HW6_CLOCK_CAP_REACTIVE_TRANSACTION_ACTIVE \
  (1UL << 5)
#define PS_HW6_CLOCK_CAP_LPBAM_DISPLAY_AUTONOMOUS \
  (1UL << 6)
#define PS_HW6_CLOCK_CAP_ALL \
  (PS_HW6_CLOCK_CAP_USB_DEVICE_ACTIVE | \
   PS_HW6_CLOCK_CAP_OCTOSPI_ACTIVE | \
   PS_HW6_CLOCK_CAP_SAI_AUDIO_ACTIVE | \
   PS_HW6_CLOCK_CAP_DISPLAY_TRANSFER_ACTIVE | \
   PS_HW6_CLOCK_CAP_REALTIME_DEADLINE_ACTIVE | \
   PS_HW6_CLOCK_CAP_REACTIVE_TRANSACTION_ACTIVE | \
   PS_HW6_CLOCK_CAP_LPBAM_DISPLAY_AUTONOMOUS)

#define PS_HW6_CLOCK_DOMAIN_USB_DEVICE \
  (1UL << 0)
#define PS_HW6_CLOCK_DOMAIN_PLL2_OCTOSPI \
  (1UL << 1)
#define PS_HW6_CLOCK_DOMAIN_PLL2_SAI \
  (1UL << 2)
#define PS_HW6_CLOCK_DOMAIN_DISPLAY_TRANSFER \
  (1UL << 3)
#define PS_HW6_CLOCK_DOMAIN_REALTIME_DEADLINE \
  (1UL << 4)
#define PS_HW6_CLOCK_DOMAIN_REACTIVE_TRANSACTION \
  (1UL << 5)
#define PS_HW6_CLOCK_DOMAIN_LPBAM_DISPLAY_AUTONOMOUS \
  (1UL << 6)

#define PS_HW6_CLOCK_STOP2_FAIL_REQUESTERS_ACTIVE (1UL << 0)
#define PS_HW6_CLOCK_STOP2_FAIL_SAI_GATE          (1UL << 1)
#define PS_HW6_CLOCK_STOP2_FAIL_SAI_RESET         (1UL << 2)
#define PS_HW6_CLOCK_STOP2_FAIL_PLL2_READY        (1UL << 3)
#define PS_HW6_CLOCK_STOP2_FAIL_PLL2_OUTPUT       (1UL << 4)
#define PS_HW6_CLOCK_STOP2_FAIL_PLL3_READY        (1UL << 5)
#define PS_HW6_CLOCK_STOP2_FAIL_HSI48_READY       (1UL << 6)
#define PS_HW6_CLOCK_STOP2_FAIL_SHSI_READY        (1UL << 7)
#define PS_HW6_CLOCK_STOP2_FAIL_USB_GATE          (1UL << 8)
#define PS_HW6_CLOCK_STOP2_FAIL_VDDUSB            (1UL << 9)

typedef struct
{
  uint32_t api_version;
  uint32_t apply_count;
  uint32_t restore_count;
  uint32_t usb_domain_on_count;
  uint32_t usb_domain_off_count;
  uint32_t requested_profile;
  uint32_t selected_profile;
  uint32_t current_profile;
  uint32_t active_capabilities;
  uint32_t requester_capabilities[PS_HW6_CLOCK_REQUESTER_COUNT];
  uint32_t requester_status[PS_HW6_CLOCK_REQUESTER_COUNT];
  uint32_t requester_active_mask;
  uint32_t aggregated_capabilities;
  uint32_t required_domain_mask;
  uint32_t managed_domain_mask;
  uint32_t readback_domain_mask;
  uint32_t stop2_blocker_capabilities;
  uint32_t stop2_blocker_domain_mask;
  uint32_t stop2_ready;
  uint32_t lpbam_stop2_ready;
  uint32_t pll2_autogate_enabled;
  uint32_t pll2_autogate_skip_count;
  uint32_t target_sysclk_hz;
  uint32_t supported_profile_mask;
  uint32_t scaffold_profile_mask;
  uint32_t last_stage;
  uint32_t last_status;
  uint32_t last_tick;
  uint32_t sysclk_before_hz;
  uint32_t sysclk_after_hz;
  uint32_t hclk_before_hz;
  uint32_t hclk_after_hz;
  uint32_t pclk1_after_hz;
  uint32_t pclk2_after_hz;
  uint32_t pclk3_after_hz;
  uint32_t flash_latency;
  uint32_t usb_clock_enabled;
  uint32_t vddusb_enabled;
  uint32_t hsi48_ready;
  uint32_t shsi_ready;
  uint32_t pll1_ready;
  uint32_t pll2_ready;
  uint32_t pll3_ready;
  uint32_t pll2_output_enabled_mask;
  uint32_t pll2_required_output_mask;
  uint32_t pll2_domain_on_count;
  uint32_t pll2_domain_off_count;
  uint32_t pll2_domain_last_status;
  uint32_t pll2_fast_path_count;
  uint32_t pll2_post_stop_rearm_pending;
  uint32_t pll2_post_stop_rearm_pending_domain_mask;
  uint32_t pll2_post_stop_invalidate_count;
  uint32_t pll2_post_stop_rearm_attempt_count;
  uint32_t pll2_post_stop_rearm_success_count;
  uint32_t pll2_post_stop_rearm_status;
  uint32_t post_stop_vosr_before;
  uint32_t post_stop_svmsr_before;
  uint32_t post_stop_voltage_scale_status;
  uint32_t post_stop_vosr_after;
  uint32_t post_stop_svmsr_after;
  uint32_t sai_mux_handoff_count;
  uint32_t sai_mux_handoff_success_count;
  uint32_t sai_mux_handoff_status;
  uint32_t sai_mux_park_clock_enabled;
  uint32_t sai_mux_park_reset_asserted;
  uint32_t sai_mux_park_source;
  uint32_t sai_mux_park_kernel_hz;
  uint32_t sai_mux_restore_clock_enabled;
  uint32_t sai_mux_restore_reset_asserted;
  uint32_t sai_mux_restore_source;
  uint32_t sai_mux_restore_kernel_hz;
  uint32_t sai_domain_active;
  uint32_t sai_clock_enabled;
  uint32_t sai_reset_asserted;
  uint32_t sai_domain_on_count;
  uint32_t sai_domain_off_count;
  uint32_t sai_reset_count;
  uint32_t sai_grant_epoch;
  uint32_t sai_domain_last_status;
  uint32_t stop2_prepare_count;
  uint32_t stop2_prepare_status;
  uint32_t stop2_physical_ready;
  uint32_t stop2_physical_failure_mask;
  uint32_t usb_kernel_hz;
  uint32_t sai1_kernel_hz;
  uint32_t ospi_kernel_hz;
} ps_hw6_clock_policy_probe_t;

extern volatile ps_hw6_clock_policy_probe_t g_ps_hw6_clock_policy_probe;

void PS_HW6_ClockPolicy_Reset(void);
void PS_HW6_ClockPolicy_RecordHardwareSnapshot(void);
uint32_t PS_HW6_ClockPolicy_SelectProfile(uint32_t capabilities);
uint32_t PS_HW6_ClockPolicy_ProfileIsActive(
  uint32_t profile,
  uint32_t required_capabilities);
UINT PS_HW6_ClockPolicy_ApplyProfile(
  uint32_t requested_profile,
  uint32_t capabilities);
UINT PS_HW6_ClockPolicy_ApplyBootIdleDomains(void);
UINT PS_HW6_ClockPolicy_ApplyRequesterProfile(
  uint32_t requester_id,
  uint32_t requested_profile,
  uint32_t capabilities);
UINT PS_HW6_ClockPolicy_PrepareStop2(void);
UINT PS_HW6_ClockPolicy_RestoreBase(void);

#ifdef __cplusplus
}
#endif

#endif /* PS_HW6_CLOCK_POLICY_H */
