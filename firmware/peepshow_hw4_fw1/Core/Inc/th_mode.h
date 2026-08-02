#ifndef TH_MODE_H
#define TH_MODE_H

#include <stdint.h>

typedef enum
{
    TH_MODE_STOP = 0u,
    TH_MODE_STATIC = 1u,
    TH_MODE_REALTIME = 2u,
    TH_MODE_FLASHING = 3u
} th_mode_t;

#endif /* TH_MODE_H */
