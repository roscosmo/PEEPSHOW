#include <assert.h>
#include <string.h>
#include "ps_dev_adp5360.h"

static uint8_t registers[256];
static uint8_t reads[32];
static uint8_t writes[4];
static uint32_t read_count, write_count, acquire_count, release_count;
static uint32_t fail_read, fail_write, fail_acquire, fail_release;
static ps_dev_adp5360_t device;
static ps_dev_adp5360_power_snapshot_t snapshot;

/* Existing ALL transfer order, including the deliberate second 0x0a read. */
static const uint8_t expected_reads[] = {
  0x00, 0x29, 0x2a, 0x2b, 0x2c, 0x2e, 0x2f,
  0x02, 0x03, 0x04, 0x07, 0x0a, 0x32, 0x33, 0x34, 0x35,
  0x08, 0x09, 0x0a, 0x20, 0x21, 0x25, 0x26, 0x27
};
static const uint8_t expected_groups[] = {
  3, 3, 3, 3, 3, 0, 0, 3, 3, 3, 3, 3, 3, 3, 1, 1,
  0, 0, 0, 3, 2, 0, 0, 3
};
static const uint32_t expected_counts[] = {7, 2, 1, 14};

static void reset_fake(void)
{
  memset(registers, 0, sizeof(registers));
  memset(reads, 0, sizeof(reads));
  memset(writes, 0, sizeof(writes));
  read_count = write_count = acquire_count = release_count = 0;
  fail_read = fail_write = fail_acquire = fail_release = 0;
  registers[0x00] = 0x10;
  registers[0x29] = 0x31;
  registers[0x2a] = 0x18;
  registers[0x2b] = 0x18;
  registers[0x2c] = 0x13;
  registers[0x2f] = 0x07;
  registers[0x34] = 0x01;
  registers[0x35] = 0x02;
  registers[0x21] = 75;
  registers[0x25] = 0x73;
  registers[0x26] = 0xa0; /* 3700 mV */
  assert(ps_dev_adp5360_init(&device, 0x46) == PS_STATUS_OK);
}

ps_hw_i2c3_lease_result_t ps_hw_i2c3_acquire(ps_hw_i2c3_client_t owner,
  uint32_t timeout, uint32_t maximum, ps_hw_i2c3_lease_t *lease)
{
  assert(owner == PS_HW_I2C3_CLIENT_POWER && timeout == 200 && maximum == 250);
  acquire_count++;
  lease->active = !fail_acquire;
  return (ps_hw_i2c3_lease_result_t){fail_acquire ? PS_STATUS_BUSY : PS_STATUS_OK, 0};
}

ps_hw_i2c3_lease_result_t ps_hw_i2c3_release(ps_hw_i2c3_lease_t *lease)
{
  assert(lease->active);
  lease->active = 0;
  release_count++;
  return (ps_hw_i2c3_lease_result_t){fail_release ? PS_STATUS_LEASE_EXPIRED : PS_STATUS_OK, 0};
}

ps_hw_i2c3_transfer_result_t ps_hw_i2c3_mem_read(const ps_hw_i2c3_lease_t *lease,
  uint8_t address, uint8_t reg, uint8_t *data, uint16_t length, uint32_t timeout)
{
  assert(lease->active && address == 0x46 && length == 1 && timeout == 50);
  assert(read_count < sizeof(reads));
  reads[read_count++] = reg;
  if (read_count == fail_read)
  {
    return (ps_hw_i2c3_transfer_result_t){PS_STATUS_IO_ERROR, 1, 4, 1};
  }
  *data = registers[reg];
  /* An additional flag arriving after the read must survive W1C. */
  if (reg == 0x34 || reg == 0x35) { registers[reg] |= 0x80; }
  return (ps_hw_i2c3_transfer_result_t){PS_STATUS_OK, 0, 0, 1};
}

ps_hw_i2c3_transfer_result_t ps_hw_i2c3_mem_write(const ps_hw_i2c3_lease_t *lease,
  uint8_t address, uint8_t reg, const uint8_t *data, uint16_t length, uint32_t timeout)
{
  assert(lease->active && address == 0x46 && length == 1 && timeout == 50);
  assert(reg == 0x34 || reg == 0x35);
  assert(*data == (reg == 0x34 ? 1 : 2));
  assert(write_count < sizeof(writes));
  /* Both flags must be read before either is cleared, before status sampling. */
  assert(read_count >= 2 && reads[read_count - 1] == 0x35);
  writes[write_count++] = reg;
  if (write_count == fail_write)
  {
    return (ps_hw_i2c3_transfer_result_t){PS_STATUS_IO_ERROR, 1, 4, 1};
  }
  registers[reg] &= (uint8_t)~*data;
  return (ps_hw_i2c3_transfer_result_t){PS_STATUS_OK, 0, 0, 1};
}

static void test_selection(void)
{
  uint32_t mask, index, count, group;
  for (mask = 1; mask <= PS_DEV_ADP5360_GROUP_ALL; ++mask)
  {
    reset_fake();
    assert(ps_dev_adp5360_read_groups(&device, mask, &snapshot) == PS_STATUS_OK);
    assert(snapshot.requested_groups == mask && snapshot.valid_groups == mask);
    assert(acquire_count == 1 && release_count == 1);
    count = 0;
    for (index = 0; index < sizeof(expected_reads); ++index)
    {
      if (mask & (1UL << expected_groups[index]))
      {
        assert(reads[count++] == expected_reads[index]);
      }
    }
    assert(count == read_count);
    assert(write_count == ((mask & 2) ? 2U : 0U));
    for (group = 0; group < PS_DEV_ADP5360_GROUP_COUNT; ++group)
    {
      assert(snapshot.groups[group].read_count ==
        ((mask & (1UL << group)) ? expected_counts[group] : 0U));
      assert(snapshot.groups[group].status ==
        ((mask & (1UL << group)) ? PS_STATUS_OK : PS_STATUS_INTERNAL_ERROR));
    }
    if (mask & 2) { assert(registers[0x34] == 0x80 && registers[0x35] == 0x80); }
  }
  reset_fake();
  assert(ps_dev_adp5360_read_power_snapshot(&device, &snapshot) == PS_STATUS_OK);
  assert(read_count == 24 && memcmp(reads, expected_reads, 24) == 0);
  assert(snapshot.read_ok_mask == 0x7f && snapshot.expected_match_mask == 0x7f);
  assert(snapshot.fuel_read_ok_mask == 0x1f && snapshot.fuel_vbat_mv == 3700);
  assert(snapshot.fuel_soc_percent == 75 && snapshot.regulator_read_ok_mask == 0x1f);
  assert(snapshot.charger_monitor_read_ok_mask == 7 && snapshot.interrupt_clear_ok_mask == 3);
}

static void test_failures(void)
{
  uint32_t index;
  for (index = 1; index <= 24; ++index)
  {
    reset_fake();
    fail_read = index;
    assert(ps_dev_adp5360_read_power_snapshot(&device, &snapshot) == PS_STATUS_IO_ERROR);
    assert(snapshot.valid_groups == (15UL & ~(1UL << expected_groups[index - 1])));
    assert(snapshot.groups[expected_groups[index - 1]].status == PS_STATUS_IO_ERROR);
    assert(read_count == 24 && release_count == 1);
    if (index == 15) { assert(write_count == 1 && writes[0] == 0x35); }
    if (index == 16) { assert(write_count == 1 && writes[0] == 0x34); }
  }
  for (index = 1; index <= 2; ++index)
  {
    reset_fake();
    fail_write = index;
    assert(ps_dev_adp5360_read_power_snapshot(&device, &snapshot) == PS_STATUS_IO_ERROR);
    assert(snapshot.valid_groups == 13);
  }
  for (index = 0; index < 7; ++index)
  {
    reset_fake();
    registers[expected_reads[index]] = (index < 5) ? 0 : ((index == 5) ? 1 : 0);
    assert(ps_dev_adp5360_read_power_snapshot(&device, &snapshot) == PS_STATUS_VERIFY_FAILED);
    assert(snapshot.valid_groups == ((index < 5) ? 7U : 14U));
  }
  reset_fake();
  fail_acquire = 1;
  assert(ps_dev_adp5360_read_power_snapshot(&device, &snapshot) == PS_STATUS_BUSY);
  assert(snapshot.valid_groups == 0 && read_count == 0 && release_count == 0);
  reset_fake();
  fail_release = 1;
  assert(ps_dev_adp5360_read_power_snapshot(&device, &snapshot) == PS_STATUS_LEASE_EXPIRED);
  assert(snapshot.valid_groups == 0 && release_count == 1);
  reset_fake();
  device.initialized = 0;
  assert(ps_dev_adp5360_read_power_snapshot(&device, &snapshot) == PS_STATUS_NOT_INITIALIZED);
  assert(snapshot.valid_groups == 0 && acquire_count == 0);
  reset_fake();
  assert(ps_dev_adp5360_read_groups(&device, 0, &snapshot) == PS_STATUS_INVALID_ARGUMENT);
  assert(ps_dev_adp5360_read_groups(&device, 16, &snapshot) == PS_STATUS_INVALID_ARGUMENT);
  assert(ps_dev_adp5360_read_power_snapshot(NULL, &snapshot) == PS_STATUS_INVALID_ARGUMENT);
  assert(snapshot.valid_groups == 0 && acquire_count == 0);
  assert(ps_dev_adp5360_read_power_snapshot(&device, NULL) == PS_STATUS_INVALID_ARGUMENT);
  reset_fake();
  registers[0x34] = registers[0x35] = 0;
  assert(ps_dev_adp5360_read_power_snapshot(&device, &snapshot) == PS_STATUS_OK);
  assert(write_count == 0 && snapshot.interrupt_clear_ok_mask == 3);
}

static void test_cache(void)
{
  ps_dev_adp5360_monitor_t monitor = {0};
  ps_dev_adp5360_group_sample_t previous;
  reset_fake();
  assert(ps_dev_adp5360_read_power_snapshot(&device, &snapshot) == PS_STATUS_OK);
  ps_dev_adp5360_monitor_record(&monitor, &snapshot, 0);
  assert(monitor.valid_mask == 15 && monitor.groups[0].success_count == 1);
  assert(monitor.groups[0].last_success_tick == 0); /* Tick zero is valid. */
  previous = monitor.groups[0].last_good;
  reset_fake();
  fail_read = 7;
  assert(ps_dev_adp5360_read_groups(&device, 1, &snapshot) == PS_STATUS_IO_ERROR);
  ps_dev_adp5360_monitor_record(&monitor, &snapshot, 10);
  assert(monitor.requested_mask == 1 && monitor.valid_mask == 14);
  assert(monitor.groups[0].attempt_count == 2 && monitor.groups[0].success_count == 1);
  assert(monitor.groups[0].last_attempt_tick == 10 && monitor.groups[0].last_success_tick == 0);
  assert(memcmp(&previous, &monitor.groups[0].last_good, sizeof(previous)) == 0);
  assert(monitor.groups[3].attempt_count == 1 && monitor.groups[3].valid == 1);
  ps_dev_adp5360_monitor_invalidate(&monitor);
  assert(monitor.valid_mask == 0 && monitor.groups[3].valid == 0);
  reset_fake();
  assert(ps_dev_adp5360_read_groups(&device, 4, &snapshot) == PS_STATUS_OK);
  ps_dev_adp5360_monitor_record(&monitor, &snapshot, 20);
  assert(monitor.valid_mask == 4 && monitor.groups[3].valid == 0);
  assert(monitor.groups[2].success_count == 2 && monitor.groups[2].last_success_tick == 20);
}

int main(void)
{
  test_selection();
  test_failures();
  test_cache();
  return 0;
}
