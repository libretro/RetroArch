/* Stand-in for the PS2 SDK's libmtap.h; the test defines the calls. */
#ifndef PS2_JOYPAD_TEST_LIBMTAP_H
#define PS2_JOYPAD_TEST_LIBMTAP_H
int mtapPortOpen(int port);
int mtapPortClose(int port);
#endif
