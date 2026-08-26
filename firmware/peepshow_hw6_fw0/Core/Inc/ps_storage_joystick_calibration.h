#ifndef PS_STORAGE_JOYSTICK_CALIBRATION_H
#define PS_STORAGE_JOYSTICK_CALIBRATION_H

#include <stdint.h>

#include "ps_input_joystick.h"
#include "ps_status.h"
#include "ps_storage_flash_block.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PS_STORAGE_JOYSTICK_CALIBRATION_API_VERSION       (1UL)
#define PS_STORAGE_JOYSTICK_CALIBRATION_RECORD_COUNT      (2UL)
#define PS_STORAGE_JOYSTICK_CALIBRATION_RECORD_BODY_SIZE  (256UL)
#define PS_STORAGE_JOYSTICK_CALIBRATION_COMMIT_OFFSET     (256UL)
#define PS_STORAGE_JOYSTICK_CALIBRATION_READ_SIZE         (260UL)
#define PS_STORAGE_JOYSTICK_CALIBRATION_SECTOR_SIZE       (0x00001000UL)
#define PS_STORAGE_JOYSTICK_CALIBRATION_INVALID_SELECTION (0xFFFFFFFFUL)

typedef enum
{
  PS_STORAGE_JOYSTICK_CALIBRATION_REASON_NONE = 0,
  PS_STORAGE_JOYSTICK_CALIBRATION_REASON_ARGUMENT,
  PS_STORAGE_JOYSTICK_CALIBRATION_REASON_LAYOUT,
  PS_STORAGE_JOYSTICK_CALIBRATION_REASON_READ,
  PS_STORAGE_JOYSTICK_CALIBRATION_REASON_UNCOMMITTED,
  PS_STORAGE_JOYSTICK_CALIBRATION_REASON_FORMAT,
  PS_STORAGE_JOYSTICK_CALIBRATION_REASON_CRC,
  PS_STORAGE_JOYSTICK_CALIBRATION_REASON_BOUNDS,
  PS_STORAGE_JOYSTICK_CALIBRATION_REASON_RESERVED,
  PS_STORAGE_JOYSTICK_CALIBRATION_REASON_CONFLICT
} ps_storage_joystick_calibration_reason_t;

typedef enum
{
  PS_STORAGE_JOYSTICK_CALIBRATION_SAVE_STAGE_IDLE = 0,
  PS_STORAGE_JOYSTICK_CALIBRATION_SAVE_STAGE_VALIDATE,
  PS_STORAGE_JOYSTICK_CALIBRATION_SAVE_STAGE_SCAN,
  PS_STORAGE_JOYSTICK_CALIBRATION_SAVE_STAGE_ERASE,
  PS_STORAGE_JOYSTICK_CALIBRATION_SAVE_STAGE_PROGRAM,
  PS_STORAGE_JOYSTICK_CALIBRATION_SAVE_STAGE_VERIFY,
  PS_STORAGE_JOYSTICK_CALIBRATION_SAVE_STAGE_COMMIT,
  PS_STORAGE_JOYSTICK_CALIBRATION_SAVE_STAGE_RESCAN,
  PS_STORAGE_JOYSTICK_CALIBRATION_SAVE_STAGE_COMPLETE
} ps_storage_joystick_calibration_save_stage_t;

typedef struct
{
  uint32_t read_status;
  uint32_t valid;
  uint32_t reason;
  uint32_t magic;
  uint32_t commit_marker;
  uint32_t generation;
  uint32_t stored_crc32;
  uint32_t computed_crc32;
  ps_input_joystick_calibration_t calibration;
} ps_storage_joystick_calibration_record_probe_t;

typedef struct
{
  uint32_t api_version;
  uint32_t scan_count;
  uint32_t status;
  uint32_t region_start;
  uint32_t region_length;
  uint32_t valid_record_count;
  uint32_t calibration_available;
  uint32_t selected_record;
  uint32_t selected_generation;
  uint32_t selection_reason;
  ps_input_joystick_calibration_t selected_calibration;
  ps_storage_joystick_calibration_record_probe_t
    record[PS_STORAGE_JOYSTICK_CALIBRATION_RECORD_COUNT];
} ps_storage_joystick_calibration_probe_t;

typedef struct
{
  uint32_t api_version;
  uint32_t save_count;
  uint32_t status;
  uint32_t stage;
  uint32_t source_record;
  uint32_t target_record;
  uint32_t target_generation;
  uint32_t target_address;
  uint32_t erase_status;
  uint32_t erase_poll_count;
  uint32_t program_status;
  uint32_t verify_status;
  uint32_t verify_mismatches;
  uint32_t record_crc32;
  uint32_t commit_status;
  uint32_t commit_verify_status;
  uint32_t commit_marker;
  uint32_t rescan_status;
  uint32_t selected_record;
  uint32_t selected_generation;
} ps_storage_joystick_calibration_save_probe_t;

extern volatile ps_storage_joystick_calibration_probe_t
  g_ps_storage_joystick_calibration_probe;
extern volatile ps_storage_joystick_calibration_save_probe_t
  g_ps_storage_joystick_calibration_save_probe;

ps_status_t PS_StorageJoystickCalibration_Scan(
  ps_storage_flash_block_t *block);
ps_status_t PS_StorageJoystickCalibration_Save(
  ps_storage_flash_block_t *block,
  const ps_input_joystick_calibration_t *calibration);

#ifdef __cplusplus
}
#endif

#endif /* PS_STORAGE_JOYSTICK_CALIBRATION_H */
