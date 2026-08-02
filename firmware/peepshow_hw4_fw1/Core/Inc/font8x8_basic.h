#ifndef FONT8X8_BASIC_H
#define FONT8X8_BASIC_H

#include <stdint.h>

#define FONT8X8_WIDTH       (8u)
#define FONT8X8_HEIGHT      (8u)
#define FONT8X8_START_CHAR  (0u)
#define FONT8X8_END_CHAR    (127u)

extern const unsigned char font8x8_basic[128][8];

#endif /* FONT8X8_BASIC_H */
