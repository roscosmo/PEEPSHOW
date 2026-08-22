#include "ps_egg_state_loader.h"

#include <stddef.h>
#include <string.h>

#include "ps_embedded_egg.h"
#include "ps_hw6_hash.h"
#include "ps_input_buttons.h"

#define PS_EGG_HEADER_SIZE               (64UL)
#define PS_EGG_CHUNK_ENTRY_SIZE          (40UL)
#define PS_EGG_FOOTER_SIZE               (40UL)
#define PS_EGG_CHUNK_COUNT               (6UL)
#define PS_EGG_ALIGNMENT                 (4UL)
#define PS_EGG_PACKAGE_SIZE_MAX          (65536UL)
#define PS_EGG_STRING_COUNT_MAX          (256U)
#define PS_EGG_HEADER_CRC_OFFSET         (44UL)
#define PS_EGG_CHUNK_MANIFEST            (1U)
#define PS_EGG_CHUNK_STRINGS             (2U)
#define PS_EGG_CHUNK_SCENES              (3U)
#define PS_EGG_CHUNK_GRAPH               (4U)
#define PS_EGG_CHUNK_RENDER              (5U)
#define PS_EGG_CHUNK_WAITING             (6U)
#define PS_EGG_STRING_HEADER_SIZE        (12UL)
#define PS_EGG_MANIFEST_SIZE             (28UL)
#define PS_EGG_SCENE_HEADER_SIZE         (12UL)
#define PS_EGG_SCENE_RECORD_SIZE         (20UL)
#define PS_EGG_GRAPH_HEADER_SIZE         (40UL)
#define PS_EGG_VARIABLE_RECORD_SIZE      (16UL)
#define PS_EGG_INPUT_RECORD_SIZE         (4UL)
#define PS_EGG_STATE_RECORD_SIZE         (8UL)
#define PS_EGG_ROUTE_RECORD_SIZE         (18UL)
#define PS_EGG_GUARD_RECORD_SIZE         (8UL)
#define PS_EGG_OPERATION_RECORD_SIZE     (12UL)
#define PS_EGG_RENDER_HEADER_SIZE        (16UL)
#define PS_EGG_RENDER_MODEL_RECORD_SIZE  (8UL)
#define PS_EGG_RENDER_ELEMENT_RECORD_SIZE (16UL)
#define PS_EGG_WAIT_HEADER_SIZE          (24UL)
#define PS_EGG_WAIT_RECORD_SIZE          (16UL)
#define PS_EGG_WAIT_ELEMENT_RECORD_SIZE  (12UL)
#define PS_EGG_PRESENTATION_BASE         (0x30000000UL)

typedef struct
{
  uint16_t type;
  uint32_t offset;
  uint32_t size;
  uint32_t crc32;
} ps_egg_chunk_t;

typedef struct
{
  const uint8_t *payload;
  uint32_t size;
  uint16_t count;
  uint32_t data_offset;
  uint32_t data_size;
} ps_egg_strings_t;

typedef struct
{
  uint32_t state_record_offset;
  uint32_t route_record_offset;
  uint32_t source_offset;
  uint32_t guard_offset;
  uint32_t operation_offset;
  uint16_t entry_state;
  uint16_t variable_count;
  uint16_t input_count;
  uint16_t state_count;
  uint16_t route_count;
  uint16_t source_count;
  uint16_t guard_count;
  uint16_t operation_count;
  uint16_t default_waiting;
} ps_egg_graph_view_t;

typedef struct
{
  uint32_t model_offset;
  uint32_t element_offset;
  uint16_t model_count;
  uint16_t element_count;
} ps_egg_render_view_t;

typedef struct
{
  uint32_t record_offset;
  uint32_t element_offset;
  uint32_t phase_offset;
  uint32_t sequence_offset;
  uint16_t waiting_count;
  uint16_t element_count;
  uint16_t phase_count;
  uint16_t sequence_count;
} ps_egg_wait_view_t;

static ps_egg_chunk_t s_ps_egg_chunks[PS_EGG_CHUNK_COUNT];

volatile ps_egg_state_loader_probe_t g_ps_egg_state_loader_probe =
{
  .api_version = PS_EGG_STATE_LOADER_API_VERSION,
  .last_status = PS_EGG_STATE_LOADER_STATUS_NOT_RUN
};

static uint32_t PS_EggRangeValid(uint32_t size,
                                 uint32_t offset,
                                 uint32_t length)
{
  return ((offset <= size) && (length <= (size - offset))) ? 1UL : 0UL;
}

static uint16_t PS_EggU16(const uint8_t *bytes)
{
  return (uint16_t)((uint16_t)bytes[0] |
                    ((uint16_t)bytes[1] << 8));
}

static uint32_t PS_EggU32(const uint8_t *bytes)
{
  return (uint32_t)bytes[0] |
         ((uint32_t)bytes[1] << 8) |
         ((uint32_t)bytes[2] << 16) |
         ((uint32_t)bytes[3] << 24);
}

static uint64_t PS_EggU64(const uint8_t *bytes)
{
  return (uint64_t)PS_EggU32(bytes) |
         ((uint64_t)PS_EggU32(&bytes[4]) << 32);
}

static int16_t PS_EggI16(const uint8_t *bytes)
{
  return (int16_t)PS_EggU16(bytes);
}

static int32_t PS_EggI32(const uint8_t *bytes)
{
  return (int32_t)PS_EggU32(bytes);
}

static uint32_t PS_EggCrc32(const uint8_t *bytes, uint32_t size)
{
  uint32_t crc = 0xFFFFFFFFUL;
  uint32_t index;

  for (index = 0UL; index < size; ++index)
  {
    uint32_t bit;

    crc ^= bytes[index];
    for (bit = 0UL; bit < 8UL; ++bit)
    {
      uint32_t mask = 0UL - (crc & 1UL);
      crc = (crc >> 1) ^ (0xEDB88320UL & mask);
    }
  }
  return ~crc;
}

static uint32_t PS_EggFail(ps_egg_state_loader_reason_t reason)
{
  g_ps_egg_state_loader_probe.last_status = 1UL;
  g_ps_egg_state_loader_probe.reason = (uint32_t)reason;
  return 1UL;
}

static uint64_t PS_EggFnv1a64(const uint8_t *bytes, uint32_t size)
{
  uint64_t hash = UINT64_C(0xCBF29CE484222325);
  uint32_t index;

  for (index = 0UL; index < size; ++index)
  {
    hash ^= bytes[index];
    hash *= UINT64_C(0x100000001B3);
  }
  return hash;
}

static uint32_t PS_EggStringRange(const ps_egg_strings_t *strings,
                                  uint16_t index,
                                  const uint8_t **text,
                                  uint32_t *length)
{
  uint32_t start;
  uint32_t end;

  if ((strings == NULL) || (text == NULL) || (length == NULL) ||
      (index >= strings->count))
  {
    return 0UL;
  }
  start = PS_EggU32(&strings->payload[PS_EGG_STRING_HEADER_SIZE +
                                     ((uint32_t)index * 4UL)]);
  end = PS_EggU32(&strings->payload[PS_EGG_STRING_HEADER_SIZE +
                                   (((uint32_t)index + 1UL) * 4UL)]);
  if ((start > end) || (end > strings->data_size))
  {
    return 0UL;
  }
  *text = &strings->payload[strings->data_offset + start];
  *length = end - start;
  return 1UL;
}

static uint32_t PS_EggStringEquals(const ps_egg_strings_t *strings,
                                   uint16_t index,
                                   const char *literal)
{
  const uint8_t *text;
  uint32_t length;
  uint32_t literal_length = 0UL;

  while (literal[literal_length] != '\0')
  {
    literal_length++;
  }
  if ((PS_EggStringRange(strings, index, &text, &length) == 0UL) ||
      (length != literal_length))
  {
    return 0UL;
  }
  return (memcmp(text, literal, length) == 0) ? 1UL : 0UL;
}

static uint32_t PS_EggUtf8Valid(const uint8_t *text, uint32_t length)
{
  uint32_t index = 0UL;

  while (index < length)
  {
    uint8_t first = text[index];
    uint32_t continuation_count;
    uint32_t continuation;

    if (first <= 0x7FU)
    {
      index++;
      continue;
    }
    if ((first >= 0xC2U) && (first <= 0xDFU))
    {
      continuation_count = 1UL;
    }
    else if ((first >= 0xE0U) && (first <= 0xEFU))
    {
      continuation_count = 2UL;
    }
    else if ((first >= 0xF0U) && (first <= 0xF4U))
    {
      continuation_count = 3UL;
    }
    else
    {
      return 0UL;
    }
    if (continuation_count > (length - index - 1UL))
    {
      return 0UL;
    }
    if (((first == 0xE0U) &&
         ((text[index + 1UL] < 0xA0U) ||
          (text[index + 1UL] > 0xBFU))) ||
        ((first == 0xEDU) &&
         ((text[index + 1UL] < 0x80U) ||
          (text[index + 1UL] > 0x9FU))) ||
        ((first == 0xF0U) &&
         ((text[index + 1UL] < 0x90U) ||
          (text[index + 1UL] > 0xBFU))) ||
        ((first == 0xF4U) &&
         ((text[index + 1UL] < 0x80U) ||
          (text[index + 1UL] > 0x8FU))))
    {
      return 0UL;
    }
    for (continuation = 1UL;
         continuation <= continuation_count;
         ++continuation)
    {
      uint8_t value = text[index + continuation];
      if ((value < 0x80U) || (value > 0xBFU))
      {
        return 0UL;
      }
    }
    index += continuation_count + 1UL;
  }
  return 1UL;
}

static uint32_t PS_EggStringsOrdered(const uint8_t *previous,
                                     uint32_t previous_length,
                                     const uint8_t *current,
                                     uint32_t current_length)
{
  uint32_t compare_length = (previous_length < current_length) ?
                            previous_length : current_length;
  int32_t comparison = (int32_t)memcmp(previous, current, compare_length);

  return ((comparison < 0) ||
          ((comparison == 0) && (previous_length < current_length))) ?
         1UL : 0UL;
}

static uint32_t PS_EggValidateStrings(const ps_egg_chunk_t *chunk,
                                      const uint8_t *blob,
                                      ps_egg_strings_t *strings)
{
  const uint8_t *payload = &blob[chunk->offset];
  uint16_t count;
  uint32_t byte_size;
  uint32_t data_offset;
  uint32_t index;

  if ((chunk->size < PS_EGG_STRING_HEADER_SIZE) ||
      (memcmp(payload, "STR1", 4UL) != 0) ||
      (PS_EggU16(&payload[4]) != 1U) ||
      (PS_EggU16(&payload[6]) == 0U) ||
      (PS_EggU16(&payload[6]) > PS_EGG_STRING_COUNT_MAX))
  {
    return 0UL;
  }
  count = PS_EggU16(&payload[6]);
  byte_size = PS_EggU32(&payload[8]);
  data_offset = PS_EGG_STRING_HEADER_SIZE +
                (((uint32_t)count + 1UL) * 4UL);
  if ((data_offset > chunk->size) ||
      (byte_size != (chunk->size - data_offset)) ||
      (PS_EggU32(&payload[PS_EGG_STRING_HEADER_SIZE]) != 0UL) ||
      (PS_EggU32(&payload[PS_EGG_STRING_HEADER_SIZE +
                         ((uint32_t)count * 4UL)]) != byte_size))
  {
    return 0UL;
  }
  for (index = 0UL; index < count; ++index)
  {
    uint32_t start = PS_EggU32(&payload[PS_EGG_STRING_HEADER_SIZE +
                                      (index * 4UL)]);
    uint32_t end = PS_EggU32(&payload[PS_EGG_STRING_HEADER_SIZE +
                                    ((index + 1UL) * 4UL)]);
    const uint8_t *current = &payload[data_offset + start];
    uint32_t current_length = end - start;

    if ((start >= end) || (end > byte_size) ||
        (PS_EggUtf8Valid(current, current_length) == 0UL))
    {
      return 0UL;
    }
    if (index > 0UL)
    {
      uint32_t previous_start = PS_EggU32(
        &payload[PS_EGG_STRING_HEADER_SIZE + ((index - 1UL) * 4UL)]);
      const uint8_t *previous = &payload[data_offset + previous_start];
      uint32_t previous_length = start - previous_start;
      if (PS_EggStringsOrdered(previous, previous_length,
                               current, current_length) == 0UL)
      {
        return 0UL;
      }
    }
  }
  strings->payload = payload;
  strings->size = chunk->size;
  strings->count = count;
  strings->data_offset = data_offset;
  strings->data_size = byte_size;
  return 1UL;
}

static uint32_t PS_EggValidateContainer(const uint8_t *blob,
                                        uint32_t size,
                                        uint16_t *manifest_index)
{
  uint8_t header[PS_EGG_HEADER_SIZE];
  uint8_t digest[32];
  uint32_t table_offset;
  uint32_t footer_offset;
  uint32_t index;
  uint64_t package_hash;

  if ((blob == NULL) ||
      (size < (PS_EGG_HEADER_SIZE + PS_EGG_FOOTER_SIZE)) ||
      (size > PS_EGG_PACKAGE_SIZE_MAX) ||
      (memcmp(blob, "PKG1", 4UL) != 0) ||
      (PS_EggU16(&blob[4]) != 1U) ||
      (PS_EggU16(&blob[6]) != PS_EGG_HEADER_SIZE) ||
      (PS_EggU32(&blob[8]) != size) ||
      (PS_EggU16(&blob[20]) != PS_EGG_CHUNK_COUNT) ||
      (PS_EggU16(&blob[22]) != PS_EGG_CHUNK_ENTRY_SIZE) ||
      (PS_EggU16(&blob[26]) != PS_EGG_ALIGNMENT) ||
      (PS_EggU32(&blob[28]) != 0UL) ||
      (PS_EggU32(&blob[32]) != 0UL))
  {
    return PS_EggFail(PS_EGG_STATE_LOADER_REASON_CONTAINER);
  }
  for (index = 48UL; index < PS_EGG_HEADER_SIZE; ++index)
  {
    if (blob[index] != 0U)
    {
      return PS_EggFail(PS_EGG_STATE_LOADER_REASON_CONTAINER);
    }
  }

  table_offset = PS_EggU32(&blob[12]);
  footer_offset = PS_EggU32(&blob[16]);
  *manifest_index = PS_EggU16(&blob[24]);
  if ((table_offset != PS_EGG_HEADER_SIZE) ||
      (*manifest_index >= PS_EGG_CHUNK_COUNT) ||
      (footer_offset + PS_EGG_FOOTER_SIZE != size) ||
      (table_offset + (PS_EGG_CHUNK_COUNT * PS_EGG_CHUNK_ENTRY_SIZE) >
       footer_offset) ||
      (memcmp(&blob[footer_offset], "END1", 4UL) != 0) ||
      (PS_EggU16(&blob[footer_offset + 4UL]) != 1U) ||
      (PS_EggU16(&blob[footer_offset + 6UL]) != PS_EGG_FOOTER_SIZE))
  {
    return PS_EggFail(PS_EGG_STATE_LOADER_REASON_CONTAINER);
  }

  (void)memcpy(header, blob, sizeof(header));
  (void)memset(&header[PS_EGG_HEADER_CRC_OFFSET], 0, 4UL);
  if (PS_EggCrc32(header, sizeof(header)) !=
      PS_EggU32(&blob[PS_EGG_HEADER_CRC_OFFSET]))
  {
    return PS_EggFail(PS_EGG_STATE_LOADER_REASON_HEADER_CRC);
  }

  if (PS_HW6_HASH_Sha256(blob, footer_offset, digest) != 0UL)
  {
    return PS_EggFail(PS_EGG_STATE_LOADER_REASON_HASH);
  }
  if (memcmp(digest, &blob[footer_offset + 8UL], sizeof(digest)) != 0)
  {
    return PS_EggFail(PS_EGG_STATE_LOADER_REASON_PACKAGE_DIGEST);
  }

  package_hash = PS_EggU64(&blob[36]);
  g_ps_egg_state_loader_probe.package_size = size;
  g_ps_egg_state_loader_probe.package_id_hash_low =
    (uint32_t)package_hash;
  g_ps_egg_state_loader_probe.package_id_hash_high =
    (uint32_t)(package_hash >> 32);
  g_ps_egg_state_loader_probe.chunk_count = PS_EGG_CHUNK_COUNT;

  for (index = 0UL; index < PS_EGG_CHUNK_COUNT; ++index)
  {
    const uint8_t *entry = &blob[table_offset +
                                 (index * PS_EGG_CHUNK_ENTRY_SIZE)];
    ps_egg_chunk_t *chunk = &s_ps_egg_chunks[index];
    uint32_t compare;

    chunk->type = PS_EggU16(entry);
    chunk->offset = PS_EggU32(&entry[16]);
    chunk->size = PS_EggU32(&entry[20]);
    chunk->crc32 = PS_EggU32(&entry[24]);
    if ((chunk->type < PS_EGG_CHUNK_MANIFEST) ||
        (chunk->type > PS_EGG_CHUNK_WAITING) ||
        (PS_EggU16(&entry[2]) != 1U) ||
        (PS_EggU32(&entry[4]) != 0UL) ||
        (chunk->size == 0UL) ||
        (PS_EggU16(&entry[28]) != PS_EGG_ALIGNMENT) ||
        (PS_EggU16(&entry[30]) != 0U) ||
        (PS_EggU64(&entry[32]) != 0ULL) ||
        ((chunk->offset & (PS_EGG_ALIGNMENT - 1UL)) != 0UL) ||
        (chunk->offset < (table_offset +
          (PS_EGG_CHUNK_COUNT * PS_EGG_CHUNK_ENTRY_SIZE))) ||
        (PS_EggRangeValid(footer_offset, chunk->offset, chunk->size) == 0UL) ||
        (PS_EggCrc32(&blob[chunk->offset], chunk->size) != chunk->crc32))
    {
      return PS_EggFail(PS_EGG_STATE_LOADER_REASON_CHUNK_CRC);
    }
    for (compare = 0UL; compare < index; ++compare)
    {
      const ps_egg_chunk_t *other = &s_ps_egg_chunks[compare];
      uint32_t disjoint = ((chunk->offset + chunk->size <= other->offset) ||
                           (other->offset + other->size <= chunk->offset)) ?
                          1UL : 0UL;
      if ((disjoint == 0UL) ||
          (PS_EggU64(&entry[8]) ==
           PS_EggU64(&blob[table_offset +
                            (compare * PS_EGG_CHUNK_ENTRY_SIZE) + 8UL])))
      {
        return PS_EggFail(PS_EGG_STATE_LOADER_REASON_CHUNK);
      }
    }
  }
  for (index = table_offset +
               (PS_EGG_CHUNK_COUNT * PS_EGG_CHUNK_ENTRY_SIZE);
       index < footer_offset;
       ++index)
  {
    uint32_t chunk_index;
    uint32_t occupied = 0UL;

    for (chunk_index = 0UL;
         chunk_index < PS_EGG_CHUNK_COUNT;
         ++chunk_index)
    {
      const ps_egg_chunk_t *chunk = &s_ps_egg_chunks[chunk_index];
      if ((index >= chunk->offset) &&
          (index < (chunk->offset + chunk->size)))
      {
        occupied = 1UL;
        break;
      }
    }
    if ((occupied == 0UL) && (blob[index] != 0U))
    {
      return PS_EggFail(PS_EGG_STATE_LOADER_REASON_CHUNK);
    }
  }
  if (s_ps_egg_chunks[*manifest_index].type != PS_EGG_CHUNK_MANIFEST)
  {
    return PS_EggFail(PS_EGG_STATE_LOADER_REASON_MANIFEST);
  }
  return 0UL;
}

static uint32_t PS_EggFindSingleChunk(uint16_t type, uint16_t *index_out)
{
  uint32_t index;
  uint32_t count = 0UL;

  for (index = 0UL; index < PS_EGG_CHUNK_COUNT; ++index)
  {
    if (s_ps_egg_chunks[index].type == type)
    {
      *index_out = (uint16_t)index;
      count++;
    }
  }
  return (count == 1UL) ? 1UL : 0UL;
}

static uint32_t PS_EggParseGraph(const ps_egg_chunk_t *chunk,
                                 const uint8_t *blob,
                                 const ps_egg_strings_t *strings,
                                 ps_egg_graph_view_t *view)
{
  const uint8_t *payload = &blob[chunk->offset];
  uint32_t offset;
  uint32_t index;
  uint32_t event_offset;
  uint32_t meaningful_offset;
  uint16_t event_count;
  uint16_t meaningful_count;

  if ((chunk->size < PS_EGG_GRAPH_HEADER_SIZE) ||
      (memcmp(payload, "STG1", 4UL) != 0) ||
      (PS_EggU16(&payload[4]) != 1U) ||
      (PS_EggU16(&payload[6]) != PS_EGG_GRAPH_HEADER_SIZE))
  {
    return 0UL;
  }
  view->entry_state = PS_EggU16(&payload[8]);
  view->variable_count = PS_EggU16(&payload[10]);
  view->input_count = PS_EggU16(&payload[12]);
  view->state_count = PS_EggU16(&payload[14]);
  view->route_count = PS_EggU16(&payload[16]);
  view->source_count = PS_EggU16(&payload[18]);
  view->guard_count = PS_EggU16(&payload[20]);
  view->operation_count = PS_EggU16(&payload[22]);
  view->default_waiting = PS_EggU16(&payload[26]);
  event_count = PS_EggU16(&payload[30]);
  meaningful_count = PS_EggU16(&payload[36]);
  if ((view->state_count == 0U) ||
      (view->state_count > PS_SCENE_RUNTIME_STATE_MAX) ||
      (view->entry_state >= view->state_count) ||
      (view->variable_count > PS_SCENE_RUNTIME_VARIABLE_MAX) ||
      (view->input_count > PS_SCENE_RUNTIME_INPUT_ROUTE_MAX) ||
      (view->guard_count > PS_SCENE_RUNTIME_GUARD_MAX) ||
      (view->operation_count > (PS_SCENE_RUNTIME_ACTION_MAX +
                                PS_SCENE_RUNTIME_TRANSITION_MAX)) ||
      (PS_EggU16(&payload[24]) >= strings->count) ||
      (PS_EggU16(&payload[28]) > 1U) ||
      (PS_EggU16(&payload[32]) >= strings->count) ||
      (PS_EggU16(&payload[34]) < 1U) ||
      (PS_EggU16(&payload[34]) > 2U) ||
      (PS_EggU16(&payload[38]) != 0U))
  {
    return 0UL;
  }

  offset = PS_EGG_GRAPH_HEADER_SIZE;
  if (PS_EggRangeValid(chunk->size, offset,
      (uint32_t)view->variable_count * PS_EGG_VARIABLE_RECORD_SIZE) == 0UL)
  {
    return 0UL;
  }
  offset += (uint32_t)view->variable_count * PS_EGG_VARIABLE_RECORD_SIZE;
  if (PS_EggRangeValid(chunk->size, offset,
      (uint32_t)view->input_count * PS_EGG_INPUT_RECORD_SIZE) == 0UL)
  {
    return 0UL;
  }
  offset += (uint32_t)view->input_count * PS_EGG_INPUT_RECORD_SIZE;
  view->state_record_offset = offset;
  offset += (uint32_t)view->state_count * PS_EGG_STATE_RECORD_SIZE;
  view->route_record_offset = offset;
  offset += (uint32_t)view->route_count * PS_EGG_ROUTE_RECORD_SIZE;
  view->source_offset = offset;
  offset += (uint32_t)view->source_count * 2UL;
  view->guard_offset = offset;
  offset += (uint32_t)view->guard_count * PS_EGG_GUARD_RECORD_SIZE;
  view->operation_offset = offset;
  offset += (uint32_t)view->operation_count * PS_EGG_OPERATION_RECORD_SIZE;
  event_offset = offset;
  offset += (uint32_t)event_count * 2UL;
  meaningful_offset = offset;
  offset += (uint32_t)meaningful_count * 2UL;
  if (offset != chunk->size)
  {
    return 0UL;
  }

  for (index = 0UL; index < view->variable_count; ++index)
  {
    const uint8_t *record = &payload[PS_EGG_GRAPH_HEADER_SIZE +
                                     (index * PS_EGG_VARIABLE_RECORD_SIZE)];
    int32_t initial = PS_EggI32(&record[4]);
    int32_t minimum = PS_EggI32(&record[8]);
    int32_t maximum = PS_EggI32(&record[12]);
    if ((PS_EggU16(record) >= strings->count) ||
        (record[2] != 1U) || (record[3] != 0U) ||
        (initial < minimum) || (initial > maximum))
    {
      return 0UL;
    }
  }
  for (index = 0UL; index < event_count; ++index)
  {
    if (PS_EggU16(&payload[event_offset + (index * 2UL)]) >=
        view->input_count)
    {
      return 0UL;
    }
  }
  for (index = 0UL; index < meaningful_count; ++index)
  {
    if (PS_EggU16(&payload[meaningful_offset + (index * 2UL)]) >=
        view->input_count)
    {
      return 0UL;
    }
  }
  return 1UL;
}

static uint32_t PS_EggParseRender(const ps_egg_chunk_t *chunk,
                                  const uint8_t *blob,
                                  const ps_egg_strings_t *strings,
                                  ps_egg_render_view_t *view)
{
  const uint8_t *payload = &blob[chunk->offset];
  uint32_t expected;
  uint32_t index;

  if ((chunk->size < PS_EGG_RENDER_HEADER_SIZE) ||
      (memcmp(payload, "RND1", 4UL) != 0) ||
      (PS_EggU16(&payload[4]) != 1U) ||
      (PS_EggU16(&payload[6]) != PS_EGG_RENDER_HEADER_SIZE) ||
      (PS_EggU16(&payload[12]) != 0U) ||
      (PS_EggU16(&payload[14]) != 0U))
  {
    return 0UL;
  }
  view->model_count = PS_EggU16(&payload[8]);
  view->element_count = PS_EggU16(&payload[10]);
  view->model_offset = PS_EGG_RENDER_HEADER_SIZE;
  view->element_offset = view->model_offset +
                         ((uint32_t)view->model_count *
                          PS_EGG_RENDER_MODEL_RECORD_SIZE);
  expected = view->element_offset +
             ((uint32_t)view->element_count *
              PS_EGG_RENDER_ELEMENT_RECORD_SIZE);
  if ((view->model_count == 0U) || (expected != chunk->size))
  {
    return 0UL;
  }
  for (index = 0UL; index < view->model_count; ++index)
  {
    const uint8_t *record = &payload[view->model_offset +
      (index * PS_EGG_RENDER_MODEL_RECORD_SIZE)];
    if ((PS_EggU16(record) >= strings->count) ||
        ((uint32_t)PS_EggU16(&record[4]) + PS_EggU16(&record[6]) >
         view->element_count))
    {
      return 0UL;
    }
  }
  for (index = 0UL; index < view->element_count; ++index)
  {
    const uint8_t *record = &payload[view->element_offset +
      (index * PS_EGG_RENDER_ELEMENT_RECORD_SIZE)];
    int16_t x = PS_EggI16(&record[6]);
    int16_t y = PS_EggI16(&record[8]);
    uint16_t width = PS_EggU16(&record[10]);
    uint16_t height = PS_EggU16(&record[12]);
    if ((PS_EggU16(record) >= strings->count) ||
        (PS_EggU16(&record[2]) >= strings->count) ||
        (record[4] < 1U) || (record[4] > 3U) ||
        (record[5] > 1U) || (x < 0) || (y < 0) ||
        (width == 0U) || (height == 0U) ||
        ((uint32_t)x + width > PS_SCENE_RENDER_CANVAS_WIDTH) ||
        ((uint32_t)y + height > PS_SCENE_RENDER_CANVAS_HEIGHT))
    {
      return 0UL;
    }
  }
  return 1UL;
}

static uint32_t PS_EggParseWaiting(const ps_egg_chunk_t *chunk,
                                   const uint8_t *blob,
                                   const ps_egg_strings_t *strings,
                                   ps_egg_wait_view_t *view)
{
  const uint8_t *payload = &blob[chunk->offset];
  uint32_t index;

  if ((chunk->size < PS_EGG_WAIT_HEADER_SIZE) ||
      (memcmp(payload, "WAI1", 4UL) != 0) ||
      (PS_EggU16(&payload[4]) != 1U) ||
      (PS_EggU16(&payload[6]) != PS_EGG_WAIT_HEADER_SIZE))
  {
    return 0UL;
  }
  for (index = 16UL; index < PS_EGG_WAIT_HEADER_SIZE; index += 2UL)
  {
    if (PS_EggU16(&payload[index]) != 0U)
    {
      return 0UL;
    }
  }
  view->waiting_count = PS_EggU16(&payload[8]);
  view->element_count = PS_EggU16(&payload[10]);
  view->phase_count = PS_EggU16(&payload[12]);
  view->sequence_count = PS_EggU16(&payload[14]);
  view->record_offset = PS_EGG_WAIT_HEADER_SIZE;
  view->element_offset = view->record_offset +
                         ((uint32_t)view->waiting_count *
                          PS_EGG_WAIT_RECORD_SIZE);
  view->phase_offset = view->element_offset +
                       ((uint32_t)view->element_count *
                        PS_EGG_WAIT_ELEMENT_RECORD_SIZE);
  view->sequence_offset = view->phase_offset +
                          ((uint32_t)view->phase_count * 2UL);
  if ((view->waiting_count == 0U) ||
      (view->sequence_offset + view->sequence_count != chunk->size))
  {
    return 0UL;
  }
  for (index = 0UL; index < view->waiting_count; ++index)
  {
    const uint8_t *record = &payload[view->record_offset +
      (index * PS_EGG_WAIT_RECORD_SIZE)];
    uint16_t steps = PS_EggU16(&record[6]);
    if ((PS_EggU16(record) >= strings->count) ||
        (PS_EggU16(&record[2]) >= strings->count) ||
        (PS_EggU16(&record[4]) == 0U) ||
        (steps == 0U) || (steps > PS_SCENE_WAITING_VISUAL_SEQUENCE_MAX) ||
        (PS_EggU16(&record[8]) >= steps) ||
        (PS_EggU16(&record[10]) != 1U) ||
        ((uint32_t)PS_EggU16(&record[12]) + PS_EggU16(&record[14]) >
         view->element_count))
    {
      return 0UL;
    }
  }
  for (index = 0UL; index < view->phase_count; ++index)
  {
    if (PS_EggU16(&payload[view->phase_offset + (index * 2UL)]) >=
        strings->count)
    {
      return 0UL;
    }
  }
  for (index = 0UL; index < view->element_count; ++index)
  {
    const uint8_t *record = &payload[view->element_offset +
      (index * PS_EGG_WAIT_ELEMENT_RECORD_SIZE)];
    uint16_t first_phase = PS_EggU16(&record[4]);
    uint16_t phase_count = PS_EggU16(&record[6]);
    uint16_t first_step = PS_EggU16(&record[8]);
    uint16_t step_count = PS_EggU16(&record[10]);
    uint32_t step;
    if ((PS_EggU16(record) >= strings->count) ||
        (PS_EggU16(&record[2]) >= strings->count) ||
        (phase_count == 0U) ||
        (phase_count > PS_SCENE_WAITING_VISUAL_PHASE_MAX) ||
        ((uint32_t)first_phase + phase_count > view->phase_count) ||
        ((uint32_t)first_step + step_count > view->sequence_count))
    {
      return 0UL;
    }
    for (step = 0UL; step < step_count; ++step)
    {
      if (payload[view->sequence_offset + first_step + step] >= phase_count)
      {
        return 0UL;
      }
    }
  }
  return 1UL;
}

static uint32_t PS_EggMapRenderElement(
  const uint8_t *record,
  const ps_egg_strings_t *strings,
  ps_scene_render_element_t *element)
{
  uint16_t visual_ref = PS_EggU16(&record[2]);
  uint8_t kind = record[4];
  uint8_t focus = record[5];
  int16_t x = PS_EggI16(&record[6]);
  int16_t y = PS_EggI16(&record[8]);

  if ((PS_EggU16(record) >= strings->count) ||
      (visual_ref >= strings->count) || (x < 0) || (y < 0) ||
      (PS_EggU16(&record[10]) == 0U) ||
      (PS_EggU16(&record[12]) == 0U))
  {
    return 0UL;
  }
  (void)memset(element, 0, sizeof(*element));
  element->visible = 1UL;
  element->x = (uint16_t)x;
  element->y = (uint16_t)y;
  element->width = PS_EggU16(&record[10]);
  element->height = PS_EggU16(&record[12]);
  if ((focus == 1U) && (kind == 3U) &&
      (PS_EggStringEquals(strings, visual_ref, "cursor_outline") != 0UL) &&
      (PS_EggU16(&record[14]) == 10U))
  {
    element->type = PS_SCENE_RENDER_ELEMENT_FOCUS;
    element->layer = PS_SCENE_RENDER_LAYER_UI;
    element->animation_binding_id = PS_SCENE_RENDER_ANIMATION_CURSOR;
    return 1UL;
  }
  if ((focus == 0U) && (kind == 3U) &&
      (PS_EggStringEquals(strings, visual_ref, "marker_outline") != 0UL) &&
      (PS_EggU16(&record[14]) == 5U))
  {
    element->type = PS_SCENE_RENDER_ELEMENT_OUTLINE_RECT;
    element->layer = PS_SCENE_RENDER_LAYER_SCENE;
    return 1UL;
  }
  if ((focus == 0U) && (kind == 1U) &&
      (PS_EggStringEquals(strings, visual_ref, "diamond") != 0UL))
  {
    element->type = PS_SCENE_RENDER_ELEMENT_SPRITE_1BPP;
    element->asset_id = PS_SCENE_RENDER_SPRITE_DIAMOND;
    element->layer = PS_SCENE_RENDER_LAYER_SCENE;
    return 1UL;
  }
  return 0UL;
}

static uint32_t PS_EggBuildBinding(
  const uint8_t *render_payload,
  const ps_egg_render_view_t *render,
  const uint8_t *wait_payload,
  const ps_egg_wait_view_t *waiting,
  const ps_egg_strings_t *strings,
  uint16_t render_index,
  uint16_t waiting_index,
  uint32_t binding_id,
  ps_scene_runtime_visual_binding_t *binding,
  uint32_t *focus_index)
{
  const uint8_t *model;
  const uint8_t *wait_record;
  uint16_t first_element;
  uint16_t element_count;
  uint16_t first_wait_element;
  uint16_t wait_element_count;
  uint32_t index;
  uint32_t focus_count = 0UL;

  if ((render_index >= render->model_count) ||
      (waiting_index >= waiting->waiting_count))
  {
    return 0UL;
  }
  model = &render_payload[render->model_offset +
                          ((uint32_t)render_index *
                           PS_EGG_RENDER_MODEL_RECORD_SIZE)];
  first_element = PS_EggU16(&model[4]);
  element_count = PS_EggU16(&model[6]);
  if ((PS_EggU16(model) >= strings->count) ||
      (element_count == 0U) ||
      (element_count > PS_SCENE_RENDER_MODEL_ELEMENT_MAX) ||
      ((uint32_t)first_element + element_count > render->element_count))
  {
    return 0UL;
  }

  (void)memset(binding, 0, sizeof(*binding));
  binding->visual_binding_id = binding_id;
  binding->element_count = element_count;
  *focus_index = PS_EggU16(&model[2]);
  for (index = 0UL; index < element_count; ++index)
  {
    const uint8_t *record = &render_payload[render->element_offset +
      (((uint32_t)first_element + index) * PS_EGG_RENDER_ELEMENT_RECORD_SIZE)];
    if (PS_EggMapRenderElement(record, strings,
                               &binding->elements[index]) == 0UL)
    {
      return 0UL;
    }
    binding->elements[index].element_id = index + 1UL;
    if (binding->elements[index].type == PS_SCENE_RENDER_ELEMENT_FOCUS)
    {
      focus_count++;
    }
  }
  if (focus_count != 1UL)
  {
    return 0UL;
  }

  wait_record = &wait_payload[waiting->record_offset +
    ((uint32_t)waiting_index * PS_EGG_WAIT_RECORD_SIZE)];
  first_wait_element = PS_EggU16(&wait_record[12]);
  wait_element_count = PS_EggU16(&wait_record[14]);
  if ((PS_EggU16(wait_record) >= strings->count) ||
      (PS_EggU16(&wait_record[2]) >= strings->count) ||
      (PS_EggU16(&wait_record[4]) == 0U) ||
      (PS_EggU16(&wait_record[6]) == 0U) ||
      (PS_EggU16(&wait_record[6]) > PS_SCENE_WAITING_VISUAL_SEQUENCE_MAX) ||
      (PS_EggU16(&wait_record[8]) >= PS_EggU16(&wait_record[6])) ||
      (PS_EggU16(&wait_record[10]) != 1U) ||
      (wait_element_count == 0U) ||
      (wait_element_count > PS_SCENE_WAITING_VISUAL_ELEMENT_MAX) ||
      ((uint32_t)first_wait_element + wait_element_count >
       waiting->element_count))
  {
    return 0UL;
  }

  binding->waiting_visual.api_version =
    PS_SCENE_WAITING_VISUAL_API_VERSION;
  binding->waiting_visual.presentation_id =
    PS_EGG_PRESENTATION_BASE |
    ((uint32_t)PS_EggU16(&wait_record[2]) + 1UL);
  binding->waiting_visual.phase_quantum_ms = PS_EggU16(&wait_record[4]);
  binding->waiting_visual.sequence_step_count = PS_EggU16(&wait_record[6]);
  binding->waiting_visual.settled_sequence_step = PS_EggU16(&wait_record[8]);
  binding->waiting_visual.cycle_policy = PS_SCENE_WAITING_VISUAL_CYCLE_LOOP;
  binding->waiting_visual.rebase_policy =
    PS_SCENE_WAITING_VISUAL_REBASE_NEW_STATE;
  binding->waiting_visual.element_count = wait_element_count;

  for (index = 0UL; index < wait_element_count; ++index)
  {
    const uint8_t *record = &wait_payload[waiting->element_offset +
      (((uint32_t)first_wait_element + index) *
       PS_EGG_WAIT_ELEMENT_RECORD_SIZE)];
    ps_scene_waiting_visual_element_t *target =
      &binding->waiting_visual.elements[index];
    uint16_t source_ref = PS_EggU16(&record[2]);
    uint16_t first_phase = PS_EggU16(&record[4]);
    uint16_t phase_count = PS_EggU16(&record[6]);
    uint16_t first_step = PS_EggU16(&record[8]);
    uint16_t step_count = PS_EggU16(&record[10]);
    uint32_t render_element;
    uint32_t source_found = 0UL;
    uint32_t phase;

    if ((PS_EggU16(record) >= strings->count) ||
        (source_ref >= strings->count) ||
        (phase_count == 0U) ||
        (phase_count > PS_SCENE_WAITING_VISUAL_PHASE_MAX) ||
        ((uint32_t)first_phase + phase_count > waiting->phase_count) ||
        (step_count != binding->waiting_visual.sequence_step_count) ||
        ((uint32_t)first_step + step_count > waiting->sequence_count))
    {
      return 0UL;
    }
    target->element_id = index + 1UL;
    target->phase_count = phase_count;
    for (phase = 0UL; phase < phase_count; ++phase)
    {
      uint16_t phase_ref = PS_EggU16(&wait_payload[waiting->phase_offset +
        (((uint32_t)first_phase + phase) * 2UL)]);
      if (phase_ref >= strings->count)
      {
        return 0UL;
      }
      target->phase_visual_id[phase] = (uint32_t)phase_ref + 1UL;
    }
    for (phase = 0UL; phase < step_count; ++phase)
    {
      uint8_t sequence_phase = wait_payload[waiting->sequence_offset +
                                            first_step + phase];
      if (sequence_phase >= phase_count)
      {
        return 0UL;
      }
      target->sequence_phase[phase] = sequence_phase;
    }

    for (render_element = 0UL; render_element < element_count;
         ++render_element)
    {
      const uint8_t *source_record = &render_payload[render->element_offset +
        (((uint32_t)first_element + render_element) *
         PS_EGG_RENDER_ELEMENT_RECORD_SIZE)];
      if (PS_EggU16(source_record) == source_ref)
      {
        const ps_scene_render_element_t *source =
          &binding->elements[render_element];
        target->logical_bounds.x = source->x;
        target->logical_bounds.y = source->y;
        target->logical_bounds.width = source->width;
        target->logical_bounds.height = source->height;
        if ((source->type == PS_SCENE_RENDER_ELEMENT_FOCUS) &&
            (phase_count == 2U))
        {
          if ((PS_EggStringEquals(strings,
                 PS_EggU16(&wait_payload[waiting->phase_offset +
                   ((uint32_t)first_phase * 2UL)]),
                 "cursor_phase_a") == 0UL) ||
              (PS_EggStringEquals(strings,
                 PS_EggU16(&wait_payload[waiting->phase_offset +
                   (((uint32_t)first_phase + 1UL) * 2UL)]),
                 "cursor_phase_b") == 0UL))
          {
            return 0UL;
          }
          target->visual_source_id =
            PS_SCENE_WAITING_VISUAL_SOURCE_SHELL_CURSOR;
        }
        else if ((source->type != PS_SCENE_RENDER_ELEMENT_FOCUS) &&
                 (phase_count == 3U))
        {
          if ((PS_EggStringEquals(strings,
                 PS_EggU16(&wait_payload[waiting->phase_offset +
                   ((uint32_t)first_phase * 2UL)]),
                 "marker_phase_a") == 0UL) ||
              (PS_EggStringEquals(strings,
                 PS_EggU16(&wait_payload[waiting->phase_offset +
                   (((uint32_t)first_phase + 1UL) * 2UL)]),
                 "marker_phase_b") == 0UL) ||
              (PS_EggStringEquals(strings,
                 PS_EggU16(&wait_payload[waiting->phase_offset +
                   (((uint32_t)first_phase + 2UL) * 2UL)]),
                 "marker_phase_c") == 0UL))
          {
            return 0UL;
          }
          target->visual_source_id =
            PS_SCENE_WAITING_VISUAL_SOURCE_THREE_PHASE_MARKER;
        }
        else
        {
          return 0UL;
        }
        source_found = 1UL;
        break;
      }
    }
    if (source_found == 0UL)
    {
      return 0UL;
    }
  }
  return 1UL;
}

static uint32_t PS_EggDecodeScene(
  const uint8_t *blob,
  const ps_egg_strings_t *strings,
  const ps_egg_chunk_t *graph_chunk,
  const ps_egg_chunk_t *render_chunk,
  const ps_egg_chunk_t *wait_chunk,
  uint16_t scene_entry_state,
  ps_scene_runtime_state_scene_t *scene)
{
  ps_egg_graph_view_t graph;
  ps_egg_render_view_t render;
  ps_egg_wait_view_t waiting;
  const uint8_t *graph_payload = &blob[graph_chunk->offset];
  const uint8_t *render_payload = &blob[render_chunk->offset];
  const uint8_t *wait_payload = &blob[wait_chunk->offset];
  uint32_t index;
  uint32_t transition_count = 0UL;
  uint32_t action_count = 0UL;
  uint32_t guard_count = 0UL;

  if ((PS_EggParseGraph(graph_chunk, blob, strings, &graph) == 0UL) ||
      (scene_entry_state != graph.entry_state))
  {
    return PS_EggFail(PS_EGG_STATE_LOADER_REASON_GRAPH);
  }
  if (PS_EggParseRender(render_chunk, blob, strings, &render) == 0UL)
  {
    return PS_EggFail(PS_EGG_STATE_LOADER_REASON_RENDER);
  }
  if ((PS_EggParseWaiting(wait_chunk, blob, strings, &waiting) == 0UL) ||
      (graph.default_waiting >= waiting.waiting_count))
  {
    return PS_EggFail(PS_EGG_STATE_LOADER_REASON_WAITING);
  }

  (void)memset(scene, 0, sizeof(*scene));
  scene->api_version = PS_SCENE_RUNTIME_API_VERSION;
  scene->scene_id = 1UL;
  scene->entry_state_id = (uint32_t)graph.entry_state + 1UL;
  scene->state_count = graph.state_count;
  scene->visual_binding_count = graph.state_count;
  scene->input_route_count = graph.input_count;
  scene->variable_count = graph.variable_count;

  for (index = 0UL; index < graph.variable_count; ++index)
  {
    const uint8_t *record = &graph_payload[PS_EGG_GRAPH_HEADER_SIZE +
      (index * PS_EGG_VARIABLE_RECORD_SIZE)];
    scene->variables[index].variable_id = index + 1UL;
    scene->variables[index].value_type = PS_SCENE_RUNTIME_VALUE_S32;
    scene->variables[index].initial_value = PS_EggI32(&record[4]);
  }
  for (index = 0UL; index < graph.input_count; ++index)
  {
    const uint8_t *record = &graph_payload[
      PS_EGG_GRAPH_HEADER_SIZE +
      ((uint32_t)graph.variable_count * PS_EGG_VARIABLE_RECORD_SIZE) +
      (index * PS_EGG_INPUT_RECORD_SIZE)];
    uint16_t source = PS_EggU16(&record[2]);
    if ((PS_EggU16(record) >= strings->count) ||
        (source < PS_INPUT_BUTTON_ID_A) ||
        (source > PS_INPUT_BUTTON_ID_R))
    {
      return PS_EggFail(PS_EGG_STATE_LOADER_REASON_GRAPH);
    }
    scene->input_routes[index].logical_event =
      PS_INPUT_BUTTON_LOGICAL_EVENT_PRESS;
    scene->input_routes[index].input_id = source;
    scene->input_routes[index].scene_event_id = index + 1UL;
  }
  for (index = 0UL; index < graph.state_count; ++index)
  {
    const uint8_t *record = &graph_payload[graph.state_record_offset +
      (index * PS_EGG_STATE_RECORD_SIZE)];
    uint16_t render_index = PS_EggU16(&record[4]);
    uint16_t waiting_index = PS_EggU16(&record[6]);
    uint32_t focus_index;

    if ((PS_EggU16(record) >= strings->count) ||
        (PS_EggU16(&record[2]) >= strings->count) ||
        (PS_EggBuildBinding(render_payload, &render,
                            wait_payload, &waiting, strings,
                            render_index, waiting_index, index + 1UL,
                            &scene->visual_bindings[index],
                            &focus_index) == 0UL))
    {
      return PS_EggFail(PS_EGG_STATE_LOADER_REASON_RENDER);
    }
    scene->states[index].state_id = index + 1UL;
    scene->states[index].visual_binding_id = index + 1UL;
    scene->states[index].focus_index = focus_index;
  }

  for (index = 0UL; index < graph.route_count; ++index)
  {
    const uint8_t *route = &graph_payload[graph.route_record_offset +
      (index * PS_EGG_ROUTE_RECORD_SIZE)];
    uint16_t input_index = PS_EggU16(&route[2]);
    uint16_t target_state = PS_EggU16(&route[4]);
    uint16_t first_source = PS_EggU16(&route[6]);
    uint16_t source_count = PS_EggU16(&route[8]);
    uint16_t first_guard = PS_EggU16(&route[10]);
    uint16_t route_guard_count = PS_EggU16(&route[12]);
    uint16_t first_operation = PS_EggU16(&route[14]);
    uint16_t route_operation_count = PS_EggU16(&route[16]);
    uint32_t first_runtime_guard = guard_count;
    uint32_t first_runtime_action = action_count;
    uint32_t guard;
    uint32_t operation;
    uint32_t source;

    if ((PS_EggU16(route) >= strings->count) ||
        (input_index >= graph.input_count) ||
        (target_state >= graph.state_count) ||
        ((uint32_t)first_source + source_count > graph.source_count) ||
        ((uint32_t)first_guard + route_guard_count > graph.guard_count) ||
        ((uint32_t)first_operation + route_operation_count >
         graph.operation_count) ||
        (guard_count + route_guard_count > PS_SCENE_RUNTIME_GUARD_MAX))
    {
      return PS_EggFail(PS_EGG_STATE_LOADER_REASON_GRAPH);
    }
    for (guard = 0UL; guard < route_guard_count; ++guard)
    {
      const uint8_t *record = &graph_payload[graph.guard_offset +
        (((uint32_t)first_guard + guard) * PS_EGG_GUARD_RECORD_SIZE)];
      uint16_t variable_index = PS_EggU16(record);
      if ((variable_index >= graph.variable_count) ||
          (record[2] < PS_SCENE_RUNTIME_COMPARE_EQ) ||
          (record[2] > PS_SCENE_RUNTIME_COMPARE_GE) ||
          (record[3] != 0U))
      {
        return PS_EggFail(PS_EGG_STATE_LOADER_REASON_GRAPH);
      }
      scene->guards[guard_count].variable_id = variable_index + 1UL;
      scene->guards[guard_count].compare = record[2];
      scene->guards[guard_count].value = PS_EggI32(&record[4]);
      guard_count++;
    }
    for (operation = 0UL; operation < route_operation_count; ++operation)
    {
      const uint8_t *record = &graph_payload[graph.operation_offset +
        (((uint32_t)first_operation + operation) *
         PS_EGG_OPERATION_RECORD_SIZE)];
      if (record[0] == 1U)
      {
        uint16_t variable_index = PS_EggU16(&record[2]);
        if ((variable_index >= graph.variable_count) ||
            (record[1] < PS_SCENE_RUNTIME_MUTATION_SET) ||
            (record[1] > PS_SCENE_RUNTIME_MUTATION_ADD) ||
            (PS_EggU16(&record[4]) != 0U) ||
            (PS_EggU16(&record[6]) != 0U) ||
            (action_count >= PS_SCENE_RUNTIME_ACTION_MAX))
        {
          return PS_EggFail(PS_EGG_STATE_LOADER_REASON_GRAPH);
        }
        scene->actions[action_count].variable_id = variable_index + 1UL;
        scene->actions[action_count].mutation = record[1];
        scene->actions[action_count].value = PS_EggI32(&record[8]);
        action_count++;
      }
      else if ((record[0] != 2U) || (record[1] != 0U) ||
               (PS_EggU16(&record[2]) != 0xFFFFU) ||
               (PS_EggU32(&record[4]) != 0UL) ||
               (PS_EggU32(&record[8]) != 0UL))
      {
        return PS_EggFail(PS_EGG_STATE_LOADER_REASON_GRAPH);
      }
    }
    for (source = 0UL; source < source_count; ++source)
    {
      uint16_t source_state = PS_EggU16(&graph_payload[graph.source_offset +
        (((uint32_t)first_source + source) * 2UL)]);
      ps_scene_runtime_transition_t *transition;
      if ((source_state >= graph.state_count) ||
          (transition_count >= PS_SCENE_RUNTIME_TRANSITION_MAX))
      {
        return PS_EggFail(PS_EGG_STATE_LOADER_REASON_CAPACITY);
      }
      transition = &scene->transitions[transition_count];
      transition->transition_id = transition_count + 1UL;
      transition->source_state_id = (uint32_t)source_state + 1UL;
      transition->scene_event_id = (uint32_t)input_index + 1UL;
      transition->first_guard = first_runtime_guard;
      transition->guard_count = route_guard_count;
      transition->first_action = first_runtime_action;
      transition->action_count = action_count - first_runtime_action;
      transition->target_state_id = (uint32_t)target_state + 1UL;
      transition_count++;
    }
  }
  scene->guard_count = guard_count;
  scene->action_count = action_count;
  scene->transition_count = transition_count;

  g_ps_egg_state_loader_probe.state_count = graph.state_count;
  g_ps_egg_state_loader_probe.input_count = graph.input_count;
  g_ps_egg_state_loader_probe.route_count = graph.route_count;
  g_ps_egg_state_loader_probe.transition_count = transition_count;
  g_ps_egg_state_loader_probe.render_model_count = render.model_count;
  g_ps_egg_state_loader_probe.render_element_count = render.element_count;
  g_ps_egg_state_loader_probe.waiting_visual_count = waiting.waiting_count;
  g_ps_egg_state_loader_probe.waiting_element_count = waiting.element_count;
  return 0UL;
}

uint32_t PS_EggStateLoader_Load(
  const uint8_t *blob,
  uint32_t size,
  ps_scene_runtime_state_scene_t *scene)
{
  ps_egg_strings_t strings;
  uint16_t manifest_index;
  uint16_t strings_index;
  uint16_t scenes_index;
  const uint8_t *manifest;
  const uint8_t *scene_table;
  const uint8_t *scene_record;
  const uint8_t *package_id;
  uint32_t package_id_length;
  uint16_t graph_index;
  uint16_t render_index;
  uint16_t waiting_index;
  uint32_t load_count = g_ps_egg_state_loader_probe.load_count + 1UL;

  (void)memset((void *)&g_ps_egg_state_loader_probe, 0,
               sizeof(g_ps_egg_state_loader_probe));
  g_ps_egg_state_loader_probe.api_version =
    PS_EGG_STATE_LOADER_API_VERSION;
  g_ps_egg_state_loader_probe.load_count = load_count;
  g_ps_egg_state_loader_probe.last_status =
    PS_EGG_STATE_LOADER_STATUS_NOT_RUN;
  if (scene == NULL)
  {
    return PS_EggFail(PS_EGG_STATE_LOADER_REASON_ARGUMENT);
  }
  if (PS_EggValidateContainer(blob, size, &manifest_index) != 0UL)
  {
    return 1UL;
  }
  if ((PS_EggFindSingleChunk(PS_EGG_CHUNK_STRINGS, &strings_index) == 0UL) ||
      (PS_EggFindSingleChunk(PS_EGG_CHUNK_SCENES, &scenes_index) == 0UL) ||
      (PS_EggValidateStrings(&s_ps_egg_chunks[strings_index], blob,
                             &strings) == 0UL))
  {
    return PS_EggFail(PS_EGG_STATE_LOADER_REASON_STRINGS);
  }

  manifest = &blob[s_ps_egg_chunks[manifest_index].offset];
  if ((s_ps_egg_chunks[manifest_index].size != PS_EGG_MANIFEST_SIZE) ||
      (memcmp(manifest, "MAN1", 4UL) != 0) ||
      (PS_EggU16(&manifest[4]) != 1U) ||
      (PS_EggU16(&manifest[6]) != PS_EGG_MANIFEST_SIZE) ||
      (PS_EggU16(&manifest[8]) >= strings.count) ||
      (PS_EggU16(&manifest[10]) >= strings.count) ||
      (PS_EggU16(&manifest[12]) >= strings.count) ||
      (PS_EggStringEquals(&strings, PS_EggU16(&manifest[12]),
                          "hw6_fw0_development") == 0UL) ||
      (PS_EggU16(&manifest[20]) >= strings.count) ||
      (PS_EggU16(&manifest[22]) != 1U) ||
      (PS_EggU32(&manifest[24]) != 0UL) ||
      (PS_EggStringRange(&strings, PS_EggU16(&manifest[8]),
                         &package_id, &package_id_length) == 0UL) ||
      (PS_EggFnv1a64(package_id, package_id_length) != PS_EggU64(&blob[36])))
  {
    return PS_EggFail(PS_EGG_STATE_LOADER_REASON_MANIFEST);
  }

  scene_table = &blob[s_ps_egg_chunks[scenes_index].offset];
  if ((s_ps_egg_chunks[scenes_index].size !=
       (PS_EGG_SCENE_HEADER_SIZE + PS_EGG_SCENE_RECORD_SIZE)) ||
      (memcmp(scene_table, "SCN1", 4UL) != 0) ||
      (PS_EggU16(&scene_table[4]) != 1U) ||
      (PS_EggU16(&scene_table[6]) != PS_EGG_SCENE_HEADER_SIZE) ||
      (PS_EggU16(&scene_table[8]) != 1U) ||
      (PS_EggU16(&scene_table[10]) != 0U))
  {
    return PS_EggFail(PS_EGG_STATE_LOADER_REASON_SCENE_TABLE);
  }
  g_ps_egg_state_loader_probe.scene_count = 1UL;
  scene_record = &scene_table[PS_EGG_SCENE_HEADER_SIZE];
  graph_index = PS_EggU16(&scene_record[8]);
  render_index = PS_EggU16(&scene_record[10]);
  waiting_index = PS_EggU16(&scene_record[12]);
  if ((PS_EggU16(scene_record) != PS_EggU16(&manifest[20])) ||
      (PS_EggU16(&scene_record[2]) >= strings.count) ||
      (PS_EggU16(&scene_record[4]) != 1U) ||
      (PS_EggU16(&scene_record[14]) != 0U) ||
      (PS_EggU32(&scene_record[16]) != 0UL) ||
      (graph_index >= PS_EGG_CHUNK_COUNT) ||
      (render_index >= PS_EGG_CHUNK_COUNT) ||
      (waiting_index >= PS_EGG_CHUNK_COUNT) ||
      (s_ps_egg_chunks[graph_index].type != PS_EGG_CHUNK_GRAPH) ||
      (s_ps_egg_chunks[render_index].type != PS_EGG_CHUNK_RENDER) ||
      (s_ps_egg_chunks[waiting_index].type != PS_EGG_CHUNK_WAITING))
  {
    return PS_EggFail(PS_EGG_STATE_LOADER_REASON_SCENE_TABLE);
  }
  g_ps_egg_state_loader_probe.graph_chunk_index = graph_index;
  g_ps_egg_state_loader_probe.render_chunk_index = render_index;
  g_ps_egg_state_loader_probe.waiting_chunk_index = waiting_index;
  if (PS_EggDecodeScene(blob, &strings,
                        &s_ps_egg_chunks[graph_index],
                        &s_ps_egg_chunks[render_index],
                        &s_ps_egg_chunks[waiting_index],
                        PS_EggU16(&scene_record[6]), scene) != 0UL)
  {
    return 1UL;
  }
  g_ps_egg_state_loader_probe.last_status = 0UL;
  g_ps_egg_state_loader_probe.reason = PS_EGG_STATE_LOADER_REASON_NONE;
  return 0UL;
}

uint32_t PS_EggStateLoader_LoadEmbedded(
  ps_scene_runtime_state_scene_t *scene)
{
  return PS_EggStateLoader_Load(g_ps_embedded_egg,
                                g_ps_embedded_egg_size,
                                scene);
}
