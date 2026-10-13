/* Stand-in for the PS2 SDK's libpad.h: the constants and the report
 * layout as ps2sdk declares them; the test defines the calls. */
#ifndef PS2_JOYPAD_TEST_LIBPAD_H
#define PS2_JOYPAD_TEST_LIBPAD_H

#define PAD_LEFT      0x0080
#define PAD_DOWN      0x0040
#define PAD_RIGHT     0x0020
#define PAD_UP        0x0010
#define PAD_START     0x0008
#define PAD_R3        0x0004
#define PAD_L3        0x0002
#define PAD_SELECT    0x0001
#define PAD_SQUARE    0x8000
#define PAD_CROSS     0x4000
#define PAD_CIRCLE    0x2000
#define PAD_TRIANGLE  0x1000
#define PAD_R1        0x0800
#define PAD_L1        0x0400
#define PAD_R2        0x0200
#define PAD_L2        0x0100

#define PAD_STATE_DISCONN   0x00
#define PAD_STATE_FINDPAD   0x01
#define PAD_STATE_FINDCTP1  0x02
#define PAD_STATE_EXECCMD   0x05
#define PAD_STATE_STABLE    0x06
#define PAD_STATE_ERROR     0x07

#define PAD_TYPE_NEJICON    0x2
#define PAD_TYPE_DIGITAL    0x4
#define PAD_TYPE_ANALOG     0x5
#define PAD_TYPE_DUALSHOCK  0x7

struct padButtonStatus
{
   unsigned char ok;
   unsigned char mode;
   unsigned short btns;
   unsigned char rjoy_h;
   unsigned char rjoy_v;
   unsigned char ljoy_h;
   unsigned char ljoy_v;
   unsigned char pressure[12];
   unsigned char unkn16[12];
} __attribute__((packed));

int padPortOpen(int port, int slot, void *padArea);
int padPortClose(int port, int slot);
unsigned char padRead(int port, int slot, struct padButtonStatus *data);
int padGetState(int port, int slot);

#endif
