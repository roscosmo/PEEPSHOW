#ifndef PS_RUNTIME_TEXT_H
#define PS_RUNTIME_TEXT_H

#include <stdint.h>
#include <stddef.h>

/* One fixed system font. Style: scale-minus-one in bits 0..2, alignment
 * left/center/right in bits 3..4. Input is a bounded, non-NUL STR1 span. */
static inline uint32_t PS_RuntimeTextFits(const uint8_t *text, uint32_t length,
  uint32_t style, uint32_t width, uint32_t height)
{
  uint32_t cell = 8U * ((style & 7U) + 1U);
  uint32_t columns = 0U;
  uint32_t lines = 1U;
  if ((text == NULL) || (length == 0U) || (length > 256U) ||
      (style > 23U) || (cell > height)) { return 0U; }
  for (uint32_t i = 0U; i < length; ++i)
  {
    if (text[i] == '\n') { columns = 0U; ++lines; }
    else if ((text[i] >= 32U) && (text[i] <= 126U)) { ++columns; }
    else { return 0U; }
    if ((columns * cell > width) || (lines * cell > height)) { return 0U; }
  }
  return 1U;
}

static inline uint32_t PS_RuntimeTextU32(const uint8_t *p)
{
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8U) |
    ((uint32_t)p[2] << 16U) | ((uint32_t)p[3] << 24U);
}

static inline uint32_t PS_RuntimeTextResolve(const uint8_t *strings, uint32_t size,
  uint32_t index, const uint8_t **text, uint32_t *length)
{
  uint32_t count, base, first, end;
  if ((strings == NULL) || (text == NULL) || (length == NULL) || (size < 12U) ||
      (strings[0] != 'S') || (strings[1] != 'T') || (strings[2] != 'R') ||
      (strings[3] != '1') || (strings[4] != 1U) || (strings[5] != 0U)) { return 0U; }
  count = (uint32_t)strings[6] | ((uint32_t)strings[7] << 8U);
  base = 12U + (count + 1U) * 4U;
  if ((index >= count) || (base > size) ||
      (PS_RuntimeTextU32(strings + 8U) != size - base)) { return 0U; }
  first = PS_RuntimeTextU32(strings + 12U + index * 4U);
  end = PS_RuntimeTextU32(strings + 16U + index * 4U);
  if ((end <= first) || (end > size - base) || (end - first > 256U)) { return 0U; }
  *text = strings + base + first;
  *length = end - first;
  return 1U;
}
#endif
