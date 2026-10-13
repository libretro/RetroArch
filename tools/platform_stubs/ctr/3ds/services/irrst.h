/* Host stand-in for libctru's 3ds/services/irrst.h: the C-stick read
 * ctr_joypad.c uses, as libctru declares it. */
#ifndef PLATFORM_STUB_3DS_IRRST_H
#define PLATFORM_STUB_3DS_IRRST_H
#include <3ds/services/hid.h>

void irrstCstickRead(circlePosition *pos);

#endif
