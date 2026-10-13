/* Host stand-in for libctru's 3ds/services/hid.h: the key bits, the
 * position records and the reads ctr_joypad.c and ctr_input.c use, as
 * libctru declares them. */
#ifndef PLATFORM_STUB_3DS_HID_H
#define PLATFORM_STUB_3DS_HID_H
#include <3ds/types.h>

enum
{
   KEY_A      = 1u << 0,
   KEY_B      = 1u << 1,
   KEY_SELECT = 1u << 2,
   KEY_START  = 1u << 3,
   KEY_DRIGHT = 1u << 4,
   KEY_DLEFT  = 1u << 5,
   KEY_DUP    = 1u << 6,
   KEY_DDOWN  = 1u << 7,
   KEY_R      = 1u << 8,
   KEY_L      = 1u << 9,
   KEY_X      = 1u << 10,
   KEY_Y      = 1u << 11,
   KEY_ZL     = 1u << 14,
   KEY_ZR     = 1u << 15,
   KEY_TOUCH  = 1u << 20
};

typedef struct
{
   u16 px;
   u16 py;
} touchPosition;

typedef struct
{
   s16 dx;
   s16 dy;
} circlePosition;

void hidScanInput(void);
u32  hidKeysHeld(void);
void hidTouchRead(touchPosition *pos);
void hidCircleRead(circlePosition *pos);

#endif
