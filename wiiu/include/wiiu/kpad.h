#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <wiiu/types.h>
#include <wiiu/wpad.h>

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


void KPADInit (void);
s32 KPADRead(s32 chan, void * data, u32 size);

#ifdef __cplusplus
}
#endif
