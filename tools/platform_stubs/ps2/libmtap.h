/* Compile-only PS2 stub of ps2sdk's libmtap.h: the multitap calls
 * ps2_joypad.c makes, as ps2sdk declares them. */
#ifndef STUB_PS2_LIBMTAP
#define STUB_PS2_LIBMTAP

extern int mtapPortOpen(int port);
extern int mtapPortClose(int port);

#endif
