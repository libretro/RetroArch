#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <wiiu/types.h>

#define WPAD_BUTTON_LEFT                    0x0001
#define WPAD_BUTTON_RIGHT                   0x0002
#define WPAD_BUTTON_DOWN                    0x0004
#define WPAD_BUTTON_UP                      0x0008
#define WPAD_BUTTON_PLUS                    0x0010
#define WPAD_BUTTON_2                       0x0100
#define WPAD_BUTTON_1                       0x0200
#define WPAD_BUTTON_B                       0x0400
#define WPAD_BUTTON_A                       0x0800
#define WPAD_BUTTON_MINUS                   0x1000
#define WPAD_BUTTON_Z                       0x2000
#define WPAD_BUTTON_C                       0x4000
#define WPAD_BUTTON_HOME                    0x8000

#define WPAD_CLASSIC_BUTTON_UP              0x0001
#define WPAD_CLASSIC_BUTTON_LEFT            0x0002
#define WPAD_CLASSIC_BUTTON_ZR              0x0004
#define WPAD_CLASSIC_BUTTON_X               0x0008
#define WPAD_CLASSIC_BUTTON_A               0x0010
#define WPAD_CLASSIC_BUTTON_Y               0x0020
#define WPAD_CLASSIC_BUTTON_B               0x0040
#define WPAD_CLASSIC_BUTTON_ZL              0x0080
#define WPAD_CLASSIC_BUTTON_R               0x0200
#define WPAD_CLASSIC_BUTTON_PLUS            0x0400
#define WPAD_CLASSIC_BUTTON_HOME            0x0800
#define WPAD_CLASSIC_BUTTON_MINUS           0x1000
#define WPAD_CLASSIC_BUTTON_L               0x2000
#define WPAD_CLASSIC_BUTTON_DOWN            0x4000
#define WPAD_CLASSIC_BUTTON_RIGHT           0x8000

#define WPAD_PRO_BUTTON_STICK_R             0x10000
#define WPAD_PRO_BUTTON_STICK_L             0x20000

typedef void (* wpad_connect_callback_t)(s32 chan, s32 status);

s32 WPADProbe (s32 chan, u32 * pad_type);
s32 WPADSetDataFormat(s32 chan, s32 format);
void WPADEnableURCC(s32 enable);
void WPADEnableWiiRemote(s32 enable);
void WPADRead(s32 chan, void * data);

#ifdef __cplusplus
}
#endif
