#ifndef PS_HW6_CLOCK_POLICY_H
#define PS_HW6_CLOCK_POLICY_H

#include <stdint.h>

#include "tx_api.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PS_HW6_CLOCK_POLICY_API_VERSION (1UL)
#define PS_HW6_CLOCK_POLICY_STATUS_NOT_RUN (0xFFFFFFFFUL)

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
#define PS_HW6_CLOCK_CAP_ALL \
  (PS_HW6_CLOCK_CAP_USB_DEVICE_ACTIVE | \
   PS_HW6_CLOCK_CAP_OCTOSPI_ACTIVE | \
   PS_HW6_CLOCK_CAP_SAI_AUDIO_ACTIVE | \
   PS_HW6_CLOCK_CAP_DISPLAY_TRANSFER_ACTIVE | \
   PS_HW6_CLOCK_CAP_REALTIME_DEADLINE_ACTIVE | \
   PS_HW6_CLOCK_CAP_REACTIVE_TRANSACTION_ACTIVE)

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
  uint32_t pll1_ready;
  uint32_t pll2_ready;
  uint32_t usb_kernel_hz;
  uint32_t sai1_kernel_hz;
  uint32_t ospi_kernel_hz;
} ps_hw6_clock_policy_probe_t;

extern volatile ps_hw6_clock_policy_probe_t g_ps_hw6_clock_policy_probe;

void PS_HW6_ClockPolicy_Reset(void);
uint32_t PS_HW6_ClockPolicy_SelectProfile(uint32_t capabilities);
uint32_t PS_HW6_ClockPolicy_ProfileIsActive(
  uint32_t profile,
  uint32_t required_capabilities);
UINT PS_HW6_ClockPolicy_ApplyProfile(
  uint32_t requested_profile,
  uint32_t capabilities);
UINT PS_HW6_ClockPolicy_RestoreBase(void);

#ifdef __cplusplus
}
#endif

#endif /* PS_HW6_CLOCK_POLICY_H */
