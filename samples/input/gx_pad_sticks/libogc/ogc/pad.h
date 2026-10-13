/* Stand-in for libogc's ogc/pad.h: its constants as libogc defines
 * them; the test defines the calls. */
#ifndef GX_JOYPAD_TEST_PAD_H
#define GX_JOYPAD_TEST_PAD_H

#include "../gccore.h"

#define PAD_BUTTON_LEFT   0x0001
#define PAD_BUTTON_RIGHT  0x0002
#define PAD_BUTTON_DOWN   0x0004
#define PAD_BUTTON_UP     0x0008
#define PAD_TRIGGER_Z     0x0010
#define PAD_TRIGGER_R     0x0020
#define PAD_TRIGGER_L     0x0040
#define PAD_BUTTON_A      0x0100
#define PAD_BUTTON_B      0x0200
#define PAD_BUTTON_X      0x0400
#define PAD_BUTTON_Y      0x0800
#define PAD_BUTTON_START  0x1000

u32 PAD_Init(void);
u32 PAD_ScanPads(void);
u16 PAD_ButtonsHeld(int pad);
s8  PAD_StickX(int pad);
s8  PAD_StickY(int pad);
s8  PAD_SubStickX(int pad);
s8  PAD_SubStickY(int pad);
u8  PAD_TriggerL(int pad);
u8  PAD_TriggerR(int pad);

#endif
