#ifndef PS_PACKAGE_READER_H
#define PS_PACKAGE_READER_H

#include <stdint.h>

#include "ps_status.h"
#include "ps_storage_flash_block.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PS_PACKAGE_READER_API_VERSION (2UL)
#define PS_PACKAGE_READER_WINDOW_BYTES (4096UL)
#define PS_PACKAGE_READER_STATUS_NOT_RUN (0xFFFFFFFFUL)

typedef enum
{
  PS_PACKAGE_READER_REASON_NONE = 0,
  PS_PACKAGE_READER_REASON_ARGUMENT,
  PS_PACKAGE_READER_REASON_UNAVAILABLE,
  PS_PACKAGE_READER_REASON_BOUNDS,
  PS_PACKAGE_READER_REASON_WINDOW,
  PS_PACKAGE_READER_REASON_STORAGE,
  PS_PACKAGE_READER_REASON_BUSY
} ps_package_reader_reason_t;

typedef struct
{
  uint32_t package_start;
  uint32_t package_size;
  uint32_t generation;
} ps_package_reader_descriptor_t;

typedef struct
{
  uint32_t api_version;
  uint32_t mount_count;
  uint32_t clear_count;
  uint32_t read_count;
  uint32_t read_bytes;
  uint32_t available;
  uint32_t package_start;
  uint32_t package_size;
  uint32_t generation;
  uint32_t last_offset;
  uint32_t last_length;
  uint32_t last_status;
  uint32_t reason;
  uint32_t runtime_request_count;
  uint32_t storage_service_count;
  uint32_t complete_count;
  uint32_t request_pending;
  uint32_t request_offset;
  uint32_t request_length;
  uint32_t request_status;
} ps_package_reader_probe_t;

extern volatile ps_package_reader_probe_t g_ps_package_reader_probe;

void PS_PackageReader_StorageClear(void);
ps_status_t PS_PackageReader_StorageMount(uint32_t package_start,
                                          uint32_t package_size,
                                          uint32_t generation);
ps_status_t PS_PackageReader_StorageReadWindow(
  ps_storage_flash_block_t *block,
  uint32_t package_offset,
  uint8_t *destination,
  uint32_t length);
ps_status_t PS_PackageReader_RuntimeBeginWindowRead(
  uint32_t package_offset,
  uint8_t *destination,
  uint32_t length);
uint32_t PS_PackageReader_StorageTakeWindowRead(
  uint32_t *package_offset,
  uint8_t **destination,
  uint32_t *length);
void PS_PackageReader_StorageCompleteWindowRead(ps_status_t status);
ps_status_t PS_PackageReader_RuntimeFinishWindowRead(void);
void PS_PackageReader_RuntimeCancelWindowRead(void);
uint32_t PS_PackageReader_GetDescriptor(ps_package_reader_descriptor_t *descriptor);

#ifdef __cplusplus
}
#endif

#endif /* PS_PACKAGE_READER_H */
