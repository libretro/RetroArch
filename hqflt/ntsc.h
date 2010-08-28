
#ifndef _NTSC_FILTER_H
#define _NTSC_FILTER_H
#include <stdint.h>
#include "snes_ntsc/snes_ntsc.h"

void ntsc_filter(uint16_t * restrict out, const uint16_t * restrict in, int width, int height);

#endif
