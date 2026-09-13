#include <assert.h>
#include <stdint.h>
#include <string.h>
#include "ps_dev_tmag3001.h"
#include "joystick_sample_definitions.inc"

static uint32_t held, sleeps, acquisitions, reads, releases, other_owner_work;
static ps_status_t acquire_status, read_status, release_status;

static UINT tx_thread_sleep(ULONG ticks)
{
  assert(ticks == PS_DEV_TMAG3001_SAMPLE_SETTLE_TICKS);
  assert(held == 0U);
  sleeps++;
  /* Simulate another bus owner completing work while the input owner sleeps. */
  held = 1U;
  other_owner_work++;
  held = 0U;
  return 0U;
}

ps_hw_i2c3_lease_result_t ps_hw_i2c3_acquire(ps_hw_i2c3_client_t owner,
    uint32_t timeout, uint32_t max_lease, ps_hw_i2c3_lease_t *lease)
{
  assert(owner == PS_HW_I2C3_CLIENT_INPUT && held == 0U);
  assert(timeout == PS_DEV_TMAG3001_ACQUIRE_TIMEOUT_MS);
  assert(max_lease == PS_DEV_TMAG3001_MAX_LEASE_MS);
  assert(sleeps == acquisitions + 1U && other_owner_work == sleeps);
  acquisitions++;
  if (acquire_status == PS_STATUS_OK)
  {
    held = 1U;
    memset(lease, 0, sizeof(*lease));
    lease->active = 1U;
  }
  return (ps_hw_i2c3_lease_result_t){.status = acquire_status};
}

ps_hw_i2c3_transfer_result_t ps_hw_i2c3_mem_read(const ps_hw_i2c3_lease_t *lease,
    uint8_t address, uint8_t reg, uint8_t *data, uint16_t length, uint32_t timeout)
{
  static const uint8_t sample[] = {0x80, 0x01, 0x7f, 0xfe, 0xff, 0xff,
                                  0x91, 0, 0, 0x42, 0x10};
  assert(held == 1U && lease->active == 1U);
  assert(address == 0x34U && reg == PS_DEV_TMAG3001_REG_X_RESULT_MSB);
  assert(length == sizeof(sample) && timeout == PS_DEV_TMAG3001_TRANSFER_TIMEOUT_MS);
  reads++;
  if (read_status == PS_STATUS_OK) { memcpy(data, sample, sizeof(sample)); }
  return (ps_hw_i2c3_transfer_result_t){.status = read_status,
      .hal_status = read_status == PS_STATUS_OK ? HAL_OK : HAL_ERROR,
      .hal_error = read_status == PS_STATUS_OK ? 0U : 0x40U};
}

ps_hw_i2c3_lease_result_t ps_hw_i2c3_release(ps_hw_i2c3_lease_t *lease)
{
  assert(held == 1U && lease->active == 1U);
  releases++;
  held = lease->active = 0U;
  return (ps_hw_i2c3_lease_result_t){.status = release_status};
}

#include "joystick_sample_under_test.inc"

static void reset(ps_dev_tmag3001_t *device)
{
  memset(device, 0, sizeof(*device));
  device->initialized = 1U;
  device->state = PS_DEV_TMAG3001_STATE_ACTIVE;
  device->address_7bit = 0x34U;
  held = sleeps = acquisitions = reads = releases = other_owner_work = 0U;
  acquire_status = read_status = release_status = PS_STATUS_OK;
}

int main(void)
{
  ps_dev_tmag3001_t device;
  ps_dev_tmag3001_raw_sample_t sample;
  reset(&device);
  assert(ps_dev_tmag3001_read_raw_sample(NULL, &sample) == PS_STATUS_INVALID_ARGUMENT);
  assert(ps_dev_tmag3001_read_raw_sample(&device, NULL) == PS_STATUS_INVALID_ARGUMENT);
  device.initialized = 0U;
  assert(ps_dev_tmag3001_read_raw_sample(&device, &sample) == PS_STATUS_NOT_INITIALIZED);
  device.initialized = 1U;
  device.state = PS_DEV_TMAG3001_STATE_SUSPENDED;
  assert(ps_dev_tmag3001_read_raw_sample(&device, &sample) == PS_STATUS_INVALID_STATE);
  assert(sleeps == 0U && acquisitions == 0U && device.operation_count == 0U);

  reset(&device);
  for (uint32_t i = 0U; i < 3U; ++i)
  {
    assert(ps_dev_tmag3001_read_raw_sample(&device, &sample) == PS_STATUS_OK);
    assert(sample.x == -32767 && sample.y == 32766 && sample.z == -1);
    assert(sample.conv_status == 0x91U && sample.magnitude_result == 0x42U);
    assert(sample.device_status == 0x10U && sample.last_hal_status == HAL_OK);
    assert(sample.last_hal_error == 0U && sample.status == PS_STATUS_OK);
    assert(device.state == PS_DEV_TMAG3001_STATE_ACTIVE && device.last_status == PS_STATUS_OK);
    assert(sleeps == i + 1U && acquisitions == sleeps && reads == sleeps);
    assert(releases == sleeps && held == 0U && device.operation_count == sleeps);
  }

  reset(&device);
  acquire_status = PS_STATUS_TIMEOUT;
  assert(ps_dev_tmag3001_read_raw_sample(&device, &sample) == PS_STATUS_TIMEOUT);
  assert(sleeps == 1U && acquisitions == 1U && reads == 0U && releases == 0U);
  assert(device.state == PS_DEV_TMAG3001_STATE_FAULT && held == 0U);
  assert(device.last_status == PS_STATUS_TIMEOUT && sample.status == PS_STATUS_TIMEOUT);

  reset(&device);
  read_status = PS_STATUS_IO_ERROR;
  release_status = PS_STATUS_TIMEOUT;
  assert(ps_dev_tmag3001_read_raw_sample(&device, &sample) == PS_STATUS_IO_ERROR);
  assert(reads == 1U && releases == 1U && held == 0U);
  assert(sample.last_hal_status == HAL_ERROR && sample.last_hal_error == 0x40U);
  assert(sample.x == 0 && sample.y == 0 && sample.z == 0);
  assert(device.state == PS_DEV_TMAG3001_STATE_FAULT && device.last_status == PS_STATUS_IO_ERROR);

  reset(&device);
  release_status = PS_STATUS_TIMEOUT;
  assert(ps_dev_tmag3001_read_raw_sample(&device, &sample) == PS_STATUS_TIMEOUT);
  assert(reads == 1U && releases == 1U && held == 0U);
  assert(device.state == PS_DEV_TMAG3001_STATE_FAULT && device.last_status == PS_STATUS_TIMEOUT);
  return 0;
}
