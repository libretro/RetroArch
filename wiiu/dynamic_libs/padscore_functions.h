/****************************************************************************
 * Copyright (C) 2015
 * by Dimok
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any
 * damages arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any
 * purpose, including commercial applications, and to alter it and
 * redistribute it freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you
 * must not claim that you wrote the original software. If you use
 * this software in a product, an acknowledgment in the product
 * documentation would be appreciated but is not required.
 *
 * 2. Altered source versions must be plainly marked as such, and
 * must not be misrepresented as being the original software.
 *
 * 3. This notice may not be removed or altered from any source
 * distribution.
 ***************************************************************************/
#ifndef __PAD_SCORE_FUNCTIONS_H_
#define __PAD_SCORE_FUNCTIONS_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <gctypes.h>

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

void InitPadScoreFunctionPointers(void);


typedef struct _KPADData
{
    u32 btns_h;
    u32 btns_d;
    u32 btns_r;
    u32 unused_1[5];
    f32 pos_x;
    f32 pos_y;
    u32 unused_2[3];
    f32 angle_x;
    f32 angle_y;
    u32 unused_3[8];
    u8 device_type;
    u8 wpad_error;
    u8 pos_valid;
    u8 unused_4[1];

    union
    {
        struct
        {
            f32 stick_x;
            f32 stick_y;
        } nunchuck;

        struct
        {
            u32 btns_h;
            u32 btns_d;
            u32 btns_r;
            f32 lstick_x;
            f32 lstick_y;
            f32 rstick_x;
            f32 rstick_y;
            f32 ltrigger;
            f32 rtrigger;
        } classic;

        u32 unused_6[20];
    };
    u32 unused_7[16];
} KPADData;

typedef void (* wpad_connect_callback_t)(s32 chan, s32 status);

extern void (* KPADInit)(void);
extern s32 (* WPADProbe)(s32 chan, u32 * pad_type);
extern s32 (* WPADSetDataFormat)(s32 chan, s32 format);
extern void (* WPADEnableURCC)(s32 enable);
extern void (* WPADRead)(s32 chan, void * data);
extern s32 (* KPADRead)(s32 chan, void * data, u32 size);

#ifdef __cplusplus
}
#endif

#endif // __PAD_SCORE_FUNCTIONS_H_
