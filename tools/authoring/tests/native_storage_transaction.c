#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
#include <string.h>
#include "ps_storage_layout.c"
#include "ps_storage_package_index.c"
#include "ps_package_reader.c"

#define INDEX_START 0xB0000U
#define ACTIVE_START 0xC0000U
#define RESERVE_START 0x5C0000U
#define FLASH_SIZE 0x1000000U
static uint8_t flash_bytes[FLASH_SIZE], saved_flash[FLASH_SIZE];
static uint8_t old_package[2048], new_package[PS_STORAGE_PACKAGE_SLOT_SIZE];
static ps_storage_flash_block_t device = {
  .initialized = 1, .geometry = {FLASH_SIZE, 4096, 256, 512, FLASH_SIZE / 512}
};
static jmp_buf power_loss;
static uint32_t operations, mutations, cut_at, partial, error_at, corrupt_at;
static uint32_t active_writes;

static void bounds(uint32_t address, uint32_t length)
{ assert(address <= FLASH_SIZE && length <= FLASH_SIZE - address); }
static void writable(uint32_t address, uint32_t length)
{
  bounds(address, length);
  assert((address >= INDEX_START && address + length <= INDEX_START + 8192) ||
         (address >= ACTIVE_START && address + length <= RESERVE_START));
  if (address >= ACTIVE_START)
  {
    /* Before EVERY destructive package operation the only committed authority
     * is a verified PENDING. No stale VALID record remains to select on reboot. */
    uint32_t record, pending = 0;
    for (record = 0; record < 2; record++)
    {
      const uint8_t *body = flash_bytes + INDEX_START + record * 4096;
      if (PS_StoragePackageIndex_U32(body + 256) == PS_STORAGE_PACKAGE_INDEX_COMMIT_MARKER)
      {
        assert(PS_StoragePackageIndex_U16(body + 4) == 2);
        assert(PS_StoragePackageIndex_U32(body + 12) == PS_STORAGE_PACKAGE_TRANSACTION_PENDING);
        assert(PS_StoragePackageIndex_U32(body + 64) == PS_StoragePackageIndex_Crc32(body));
        pending++;
      }
    }
    assert(pending == 1);
    active_writes++;
  }
}
ps_status_t ps_storage_flash_block_read(ps_storage_flash_block_t *block,
  uint32_t address, uint8_t *data, uint32_t length)
{
  assert(block == &device);
  bounds(address, length);
  if (++operations == error_at) { return PS_STATUS_INTERNAL_ERROR; }
  memcpy(data, flash_bytes + address, length);
  if (operations == corrupt_at && length) { data[0] ^= 1; }
  return PS_STATUS_OK;
}
ps_status_t ps_storage_flash_block_program(ps_storage_flash_block_t *block,
  uint32_t address, const uint8_t *data, uint32_t length)
{
  uint32_t index, count = length;
  assert(block == &device);
  writable(address, length);
  assert(length <= 256 && address / 256 == (address + length - 1) / 256);
  if (++operations == error_at) { return PS_STATUS_INTERNAL_ERROR; }
  mutations++;
  if (mutations == cut_at && partial) { count = length / 2; }
  for (index = 0; index < count; index++)
  {
    assert((flash_bytes[address + index] & data[index]) == data[index]);
    flash_bytes[address + index] &= data[index];
  }
  if (mutations == cut_at) { longjmp(power_loss, 1); }
  return PS_STATUS_OK;
}
ps_status_t ps_storage_flash_block_erase(ps_storage_flash_block_t *block,
  uint32_t block_index, uint32_t *poll_count)
{
  uint32_t address = block_index * 4096;
  assert(block == &device);
  writable(address, 4096);
  if (++operations == error_at) { return PS_STATUS_INTERNAL_ERROR; }
  mutations++;
  memset(flash_bytes + address, 255, (mutations == cut_at && partial) ? 2048 : 4096);
  *poll_count = 1;
  if (mutations == cut_at) { longjmp(power_loss, 1); }
  return PS_STATUS_OK;
}
ps_status_t ps_storage_flash_block_verify_erased(ps_storage_flash_block_t *block,
  uint32_t block_index, uint32_t *mismatches)
{
  uint8_t buffer[256];
  uint32_t offset, index;
  ps_status_t status;
  *mismatches = 0;
  for (offset = 0; offset < 4096; offset += sizeof(buffer))
  {
    status = ps_storage_flash_block_read(block, block_index * 4096 + offset, buffer, sizeof(buffer));
    if (status != PS_STATUS_OK) { return status; }
    for (index = 0; index < sizeof(buffer); index++) { *mismatches += buffer[index] != 255; }
  }
  return *mismatches ? PS_STATUS_VERIFY_FAILED : PS_STATUS_OK;
}

static void reset_io(void)
{ operations = mutations = cut_at = partial = error_at = corrupt_at = active_writes = 0; }
static void package(uint8_t *bytes, uint32_t size, uint8_t value)
{
  memset(bytes, value, size);
  memcpy(bytes, "PKG1", 4);
  PS_StoragePackageIndex_PutU32(bytes + 8, size);
  PS_StoragePackageIndex_PutU32(bytes + 16, size - 40);
  memcpy(bytes + size - 40, "END1", 4);
}
static void record(uint32_t sector, uint32_t generation, uint32_t state)
{
  PS_StoragePackageIndex_BuildRecord(old_package, sizeof(old_package), sizeof(old_package) - 40,
    generation, state);
  memcpy(flash_bytes + INDEX_START + sector * 4096, ps_storage_package_index_body, 256);
  PS_StoragePackageIndex_PutU32(flash_bytes + INDEX_START + sector * 4096 + 256,
    PS_STORAGE_PACKAGE_INDEX_COMMIT_MARKER);
}
static void scan(void) { assert(PS_StoragePackageIndex_Scan(&device) == PS_STATUS_OK); }
static void no_package(void) { scan(); assert(g_ps_storage_package_index_probe.installed_available == 0); }
static void preserved(void)
{
  assert(memcmp(flash_bytes, saved_flash, INDEX_START) == 0);
  assert(memcmp(flash_bytes + INDEX_START + 8192, saved_flash + INDEX_START + 8192,
    ACTIVE_START - INDEX_START - 8192) == 0);
  assert(memcmp(flash_bytes + RESERVE_START, saved_flash + RESERVE_START,
    FLASH_SIZE - RESERVE_START) == 0);
}
static void boot_safe(uint32_t new_size)
{
  scan();
  if (g_ps_storage_package_index_probe.installed_available)
  {
    uint32_t size = g_ps_storage_package_index_probe.selected_package_size;
    assert(g_ps_storage_package_index_probe.selected_package_start == ACTIVE_START);
    assert(g_ps_storage_package_index_probe.selected_slot == 0);
    assert((size == sizeof(old_package) && memcmp(flash_bytes + ACTIVE_START, old_package, size) == 0) ||
           (size == new_size && memcmp(flash_bytes + ACTIVE_START, new_package, size) == 0));
  }
  preserved();
}

int main(void)
{
  uint32_t index, count, total_io, total_mutations;
  const ps_storage_region_t *regions;
  ps_storage_layout_validation_t layout;
  package(old_package, sizeof(old_package), 0x35);
  package(new_package, 1024, 0xA6);
  memset(flash_bytes, 0x59, sizeof(flash_bytes));
  memset(flash_bytes + INDEX_START, 255, 8192);
  memset(flash_bytes + ACTIVE_START, 255, PS_STORAGE_PACKAGE_SLOT_SIZE);
  memcpy(saved_flash, flash_bytes, sizeof(flash_bytes));
  assert(ps_storage_layout_validate(&device.geometry, &layout) == PS_STATUS_OK);
  regions = ps_storage_layout_regions(&count);
  assert(count == 11);
  for (index = 0; index < count; index++)
  {
    if (regions[index].id == PS_STORAGE_REGION_INSTALLED_PACKAGE)
    { assert(regions[index].start == ACTIVE_START && regions[index].length == 0x500000); }
    if (regions[index].id == PS_STORAGE_REGION_CONTENT_RESERVE)
    { assert(regions[index].start == RESERVE_START && !regions[index].host_exposed); }
    assert((regions[index].host_exposed != 0) == (regions[index].id == PS_STORAGE_REGION_USB_STAGING));
  }
  no_package();
  record(0, 40, 1);
  PS_StoragePackageIndex_PutU16(flash_bytes + INDEX_START + 4, 1); /* Legacy A/B record. */
  no_package();
  assert(g_ps_storage_package_index_probe.selection_reason == PS_STORAGE_PACKAGE_INDEX_REASON_LEGACY);
  record(1, 41, 2);
  PS_StoragePackageIndex_PutU16(flash_bytes + INDEX_START + 4096 + 4, 1);
  no_package();
  reset_io();
  assert(PS_StoragePackageIndex_InstallValidated(&device, old_package, sizeof(old_package)) == PS_STATUS_OK);
  scan();
  assert(g_ps_storage_package_index_probe.installed_available == 1);
  assert(g_ps_storage_package_index_probe.selected_generation == 2);
  assert(g_ps_storage_package_install_probe.pending_status == 0 && g_ps_storage_package_install_probe.retire_status == 0);
  preserved();
  memcpy(saved_flash, flash_bytes, sizeof(flash_bytes));
  reset_io();
  assert(PS_StoragePackageIndex_InstallValidated(&device, new_package, 1024) == PS_STATUS_OK);
  total_io = operations;
  total_mutations = mutations;
  boot_safe(1024);
  assert(g_ps_storage_package_index_probe.selected_generation == 4);
  /* Power loss after every mutation, both partial and complete. */
  for (index = 1; index <= total_mutations; index++)
  {
    volatile uint32_t variant;
    for (variant = 0; variant < 2; variant++)
    {
      memcpy(flash_bytes, saved_flash, sizeof(flash_bytes));
      reset_io(); cut_at = index; partial = variant;
      if (setjmp(power_loss) == 0)
      { (void)PS_StoragePackageIndex_InstallValidated(&device, new_package, 1024); assert(0); }
      reset_io(); boot_safe(1024);
      /* Reinstall must recover from every interrupted state. */
      assert(PS_StoragePackageIndex_InstallValidated(&device, new_package, 1024) == PS_STATUS_OK);
      boot_safe(1024);
    }
  }
  for (index = 1; index <= total_io; index++)
  {
    memcpy(flash_bytes, saved_flash, sizeof(flash_bytes));
    reset_io(); error_at = index;
    assert(PS_StoragePackageIndex_InstallValidated(&device, new_package, 1024) != PS_STATUS_OK);
    reset_io(); boot_safe(1024);
    memcpy(flash_bytes, saved_flash, sizeof(flash_bytes));
    reset_io(); corrupt_at = index;
    (void)PS_StoragePackageIndex_InstallValidated(&device, new_package, 1024);
    reset_io(); boot_safe(1024);
  }
  memcpy(flash_bytes, saved_flash, sizeof(flash_bytes));
  record(0, 3, PS_STORAGE_PACKAGE_TRANSACTION_PENDING);
  no_package(); /* New PENDING blocks the older VALID. */
  record(0, 3, PS_STORAGE_PACKAGE_TRANSACTION_FAILED);
  no_package();
  record(0, 2, PS_STORAGE_PACKAGE_TRANSACTION_PENDING);
  no_package();
  assert(g_ps_storage_package_index_probe.selection_reason == PS_STORAGE_PACKAGE_INDEX_REASON_CONFLICT);
  record(0, 0x80000002U, PS_STORAGE_PACKAGE_TRANSACTION_VALID);
  no_package();
  assert(g_ps_storage_package_index_probe.selection_reason == PS_STORAGE_PACKAGE_INDEX_REASON_CONFLICT);
  memset(flash_bytes + INDEX_START, 255, 8192);
  record(0, UINT32_MAX, PS_STORAGE_PACKAGE_TRANSACTION_VALID);
  assert(PS_StoragePackageIndex_InstallValidated(&device, new_package, 1024) == PS_STATUS_OK);
  assert(g_ps_storage_package_install_probe.pending_generation == 1);
  assert(g_ps_storage_package_index_probe.selected_generation == 2);
  assert(PS_PackageReader_StorageMount(ACTIVE_START, 1024, 2) == PS_STATUS_OK);
  assert(PS_PackageReader_StorageMount(RESERVE_START, 1024, 2) == PS_STATUS_INVALID_ARGUMENT);
  assert(PS_PackageReader_StorageMount(ACTIVE_START + 1, 1024, 2) == PS_STATUS_INVALID_ARGUMENT);
  assert(PS_PackageReader_StorageMount(ACTIVE_START, PS_STORAGE_PACKAGE_SLOT_SIZE + 1, 2) == PS_STATUS_INVALID_ARGUMENT);
  /* Low-level bounds include the full product slot; the staged RAM bridge is still bounded separately. */
  package(new_package, sizeof(new_package), 0xA6);
  assert(PS_StoragePackageIndex_InstallValidated(&device, new_package, sizeof(new_package)) == PS_STATUS_OK);
  boot_safe(sizeof(new_package));
  device.geometry.program_page_size = 0;
  assert(PS_StoragePackageIndex_InstallValidated(&device, old_package, sizeof(old_package)) == PS_STATUS_INVALID_ARGUMENT);
  printf("single-slot journal: %u mutation boundaries and %u IO failure points passed\n", total_mutations, total_io);
  return 0;
}
