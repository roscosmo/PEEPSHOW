#ifndef PS_HW6_PERIPHERAL_PROBE_H
#define PS_HW6_PERIPHERAL_PROBE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PS_HW6_PERIPHERAL_PMIC      (1UL << 0)
#define PS_HW6_PERIPHERAL_IMU       (1UL << 1)
#define PS_HW6_PERIPHERAL_JOYSTICK  (1UL << 2)
#define PS_HW6_PERIPHERAL_FLASH     (1UL << 3)
#define PS_HW6_PERIPHERAL_NINA      (1UL << 4)
#define PS_HW6_PERIPHERAL_DISPLAY   (1UL << 5)
#define PS_HW6_PERIPHERAL_AUDIO     (1UL << 6)
#define PS_HW6_PERIPHERAL_USB       (1UL << 7)

typedef struct
{
  uint32_t magic;
  uint32_t version;
  uint32_t phase;
  uint32_t complete;
  uint32_t start_tick;
  uint32_t end_tick;
  uint32_t duration_ticks;
  uint32_t required_mask;
  uint32_t attempted_mask;
  uint32_t pass_mask;
  uint32_t failure_mask;
  uint32_t skipped_mask;

  uint32_t i2c_state_before;
  uint32_t i2c_error_before;
  uint32_t i2c_state_after;
  uint32_t i2c_error_after;

  uint32_t pmic_address_7bit;
  uint32_t pmic_ready_status;
  uint32_t pmic_id_status;
  uint32_t pmic_fault_status;
  uint32_t pmic_pgood_status;
  uint32_t pmic_id;
  uint32_t pmic_fault;
  uint32_t pmic_pgood;
  uint32_t pmic_identity_match;
  uint32_t pmic_rails_ready;

  uint32_t imu_address_7bit;
  uint32_t imu_ready_status;
  uint32_t imu_ready_error;
  uint32_t imu_whoami_status;
  uint32_t imu_whoami_error;
  uint32_t imu_whoami;
  uint32_t imu_alt_address_7bit;
  uint32_t imu_alt_ready_status;
  uint32_t imu_alt_ready_error;
  uint32_t imu_alt_whoami_status;
  uint32_t imu_alt_whoami_error;
  uint32_t imu_alt_whoami;
  uint32_t imu_detected_address_7bit;
  uint32_t imu_identity_match;

  uint32_t joystick_address_7bit;
  uint32_t joystick_ready_status;
  uint32_t joystick_device_id_status;
  uint32_t joystick_manufacturer_lsb_status;
  uint32_t joystick_manufacturer_msb_status;
  uint32_t joystick_device_id;
  uint32_t joystick_manufacturer_lsb;
  uint32_t joystick_manufacturer_msb;
  uint32_t joystick_identity_match;

  uint32_t flash_ospi_state_before;
  uint32_t flash_jedec_status;
  uint32_t flash_status1_status;
  uint32_t flash_status2_status;
  uint32_t flash_status3_status;
  uint32_t flash_jedec_id[3];
  uint32_t flash_status[3];
  uint32_t flash_identity_match;
  uint32_t flash_ospi_state_after;
  uint32_t flash_ospi_error_after;

  uint32_t nina_uart_state_before;
  uint32_t nina_uart_error_before;
  uint32_t nina_nrst_before;
  uint32_t nina_nrst_released;
  uint32_t nina_release_tick;
  uint32_t nina_boot_wait_ms;
  uint32_t nina_boot_rx_len;
  uint32_t nina_attempt_count;
  uint32_t nina_at_tx_status;
  uint32_t nina_at_rx_len;
  uint32_t nina_at_ok;
  uint32_t nina_at_error;
  uint32_t nina_nrst_after;
  uint32_t nina_uart_state_after;
  uint32_t nina_uart_error_after;
  char nina_at_rx[64];

  uint32_t rtc_state;
  uint32_t audio_sai_state;
  uint32_t display_spi_state;
  uint32_t display_lptim_state;
  uint32_t usb_pcd_state;
} PS_HW6_PeripheralProbe;

extern volatile PS_HW6_PeripheralProbe g_ps_hw6_peripheral_probe;

void PS_HW6_PeripheralProbe_Run(void);

#ifdef __cplusplus
}
#endif

#endif
