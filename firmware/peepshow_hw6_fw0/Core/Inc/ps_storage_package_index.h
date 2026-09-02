#ifndef PS_STORAGE_PACKAGE_INDEX_H
#define PS_STORAGE_PACKAGE_INDEX_H

#include <stdint.h>

#include "ps_status.h"
#include "ps_storage_flash_block.h"
#include "ps_target_profile_autogen.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PS_STORAGE_PACKAGE_INDEX_API_VERSION       (2UL)
#define PS_STORAGE_PACKAGE_INDEX_RECORD_COUNT      (2UL)
#define PS_STORAGE_PACKAGE_INDEX_RECORD_BODY_SIZE  (256UL)
#define PS_STORAGE_PACKAGE_INDEX_COMMIT_OFFSET     (256UL)
#define PS_STORAGE_PACKAGE_INDEX_READ_SIZE         (260UL)
#define PS_STORAGE_PACKAGE_INDEX_SECTOR_SIZE       (0x00001000UL)
#define PS_STORAGE_PACKAGE_SLOT_COUNT              (2UL)
#define PS_STORAGE_PACKAGE_SLOT_SIZE \
  PS_TARGET_PROFILE_PACKAGE_MAX_BYTES
#define PS_STORAGE_PACKAGE_INDEX_INVALID_SELECTION (0xFFFFFFFFUL)

typedef enum
{
  PS_STORAGE_PACKAGE_INDEX_REASON_NONE = 0,
  PS_STORAGE_PACKAGE_INDEX_REASON_ARGUMENT,
  PS_STORAGE_PACKAGE_INDEX_REASON_LAYOUT,
  PS_STORAGE_PACKAGE_INDEX_REASON_READ,
  PS_STORAGE_PACKAGE_INDEX_REASON_UNCOMMITTED,
  PS_STORAGE_PACKAGE_INDEX_REASON_FORMAT,
  PS_STORAGE_PACKAGE_INDEX_REASON_CRC,
  PS_STORAGE_PACKAGE_INDEX_REASON_BOUNDS,
  PS_STORAGE_PACKAGE_INDEX_REASON_RESERVED,
  PS_STORAGE_PACKAGE_INDEX_REASON_CONFLICT
} ps_storage_package_index_reason_t;

typedef enum
{
  PS_STORAGE_PACKAGE_INSTALL_STAGE_IDLE = 0,
  PS_STORAGE_PACKAGE_INSTALL_STAGE_VALIDATE,
  PS_STORAGE_PACKAGE_INSTALL_STAGE_SCAN,
  PS_STORAGE_PACKAGE_INSTALL_STAGE_ERASE_PACKAGE,
  PS_STORAGE_PACKAGE_INSTALL_STAGE_PROGRAM_PACKAGE,
  PS_STORAGE_PACKAGE_INSTALL_STAGE_VERIFY_PACKAGE,
  PS_STORAGE_PACKAGE_INSTALL_STAGE_ERASE_INDEX,
  PS_STORAGE_PACKAGE_INSTALL_STAGE_PROGRAM_INDEX,
  PS_STORAGE_PACKAGE_INSTALL_STAGE_VERIFY_INDEX,
  PS_STORAGE_PACKAGE_INSTALL_STAGE_COMMIT,
  PS_STORAGE_PACKAGE_INSTALL_STAGE_RESCAN,
  PS_STORAGE_PACKAGE_INSTALL_STAGE_COMPLETE
} ps_storage_package_install_stage_t;

typedef struct
{
  uint32_t read_status;
  uint32_t valid;
  uint32_t reason;
  uint32_t magic;
  uint32_t commit_marker;
  uint32_t generation;
  uint32_t active_slot;
  uint32_t package_size;
  uint32_t package_id_hash_low;
  uint32_t package_id_hash_high;
  uint32_t flags;
  uint32_t stored_crc32;
  uint32_t computed_crc32;
  uint32_t package_sha256_words[8];
} ps_storage_package_index_record_probe_t;

typedef struct
{
  uint32_t api_version;
  uint32_t scan_count;
  uint32_t status;
  uint32_t index_region_start;
  uint32_t index_region_length;
  uint32_t package_region_start;
  uint32_t package_region_length;
  uint32_t valid_record_count;
  uint32_t installed_available;
  uint32_t selected_record;
  uint32_t selected_slot;
  uint32_t selected_generation;
  uint32_t selected_package_start;
  uint32_t selected_package_size;
  uint32_t selection_reason;
  ps_storage_package_index_record_probe_t
    record[PS_STORAGE_PACKAGE_INDEX_RECORD_COUNT];
} ps_storage_package_index_probe_t;

typedef struct
{
  uint32_t api_version;
  uint32_t install_count;
  uint32_t status;
  uint32_t stage;
  uint32_t package_size;
  uint32_t package_id_hash_low;
  uint32_t package_id_hash_high;
  uint32_t source_record;
  uint32_t source_slot;
  uint32_t target_record;
  uint32_t target_slot;
  uint32_t target_generation;
  uint32_t target_package_start;
  uint32_t target_index_start;
  uint32_t package_erase_count;
  uint32_t package_erase_status;
  uint32_t package_erase_poll_count;
  uint32_t package_program_status;
  uint32_t package_program_bytes;
  uint32_t package_program_page_count;
  uint32_t package_program_failure_offset;
  uint32_t package_program_block_initialized;
  uint32_t package_program_device_initialized;
  uint32_t package_program_device_state;
  uint32_t package_program_device_status;
  uint32_t package_verify_status;
  uint32_t package_verify_bytes;
  uint32_t package_verify_mismatches;
  uint32_t index_erase_status;
  uint32_t index_erase_poll_count;
  uint32_t index_program_status;
  uint32_t index_verify_status;
  uint32_t index_verify_mismatches;
  uint32_t index_crc32;
  uint32_t commit_status;
  uint32_t commit_verify_status;
  uint32_t commit_marker;
  uint32_t rescan_status;
  uint32_t selected_record;
  uint32_t selected_slot;
  uint32_t selected_generation;
} ps_storage_package_install_probe_t;

extern volatile ps_storage_package_index_probe_t
  g_ps_storage_package_index_probe;
extern volatile ps_storage_package_install_probe_t
  g_ps_storage_package_install_probe;

ps_status_t PS_StoragePackageIndex_Scan(ps_storage_flash_block_t *block);
ps_status_t PS_StoragePackageIndex_InstallValidated(
  ps_storage_flash_block_t *block,
  const uint8_t *package,
  uint32_t package_size);

#ifdef __cplusplus
}
#endif

#endif /* PS_STORAGE_PACKAGE_INDEX_H */
