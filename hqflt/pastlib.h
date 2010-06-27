/* This source code is Copyright (C) 2006-2009 by WolfWings. */
/*      It is, however, released under the ISC license.      */
/*             en.wikipedia.org/wiki/ISC_Licence             */
/* --------------------------------------------------------- */
/* It is a formula-level rederiviation of the HQ2x technique */
/* and is, thus, not a derivative work of the original code, */
/* only the original equations behind the code.              */

#ifndef __PAST_LIBRARY_H
#define __PAST_LIBRARY_H

#include <stdint.h>

#ifndef __PAST_LIBRARY_WIDTH
#define __PAST_LIBRARY_WIDTH 256
#endif

#ifndef __PAST_LIBRARY_HEIGHT
#define __PAST_LIBRARY_HEIGHT 224
#endif

typedef uint16_t pixel;

/* These two functions are what need to be modified to interface other
 * pixel formats with the library. They need to return values inside a
 * format like this: (Red/Blue can be reversed.)
 *
 * 0000-00GG-GGG0-0000-0RRR-RR00-000B-BBBB
 *
 * This is VERY fast to create from 15-bit (NOT 16-bit) RGB/BGR, as
 * below.
 *
 * If you can't return data in this format, you'll have to rebuild the
 * ExpandedDiff function and DiffTable array to compensate, and possibly
 * change the blending functions in the Process* functions to handle any
 * guard-bits and masking.
 */

static inline uint32_t RGBUnpack(pixel i) {
	uint32_t o = i;
	o = (o * 0x10001);
	o = o & 0x03E07C1F;
	return o;
}
static inline pixel RGBPack(uint32_t x) {
	x &= 0x03E07C1F;
	x |= (x >> 16);
	return x;
}

void ProcessHQ2x(const pixel * restrict inbuffer, pixel * restrict outbuffer);
void ProcessHQ4x(const pixel * restrict inbuffer, pixel * restrict outbuffer);

#endif /* __PAST_LIBRARY_H */
