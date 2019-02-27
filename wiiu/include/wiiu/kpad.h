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
    u8 format;

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

        struct
        {
            u32 hold;
            u32 trigger;
            u32 release;
            f32 lstick_x;
            f32 lstick_y;
            f32 rstick_x;
            f32 rstick_y;
            s32 charging;
            s32 wired;
        } pro;

        u32 unused_6[20];
    };
    u32 unused_7[16];
} KPADData;

void KPADInit (void);
void KPADShutdown(void);
s32 KPADRead(s32 chan, void * data, u32 size);
s32 KPADReadEx(s32 chan, KPADData * data, u32 size, s32 *error);

typedef s32 WPADChannel;
/* legal values for WPADChannel */
enum {
   WPAD_CHAN0 = 0,
   WPAD_CHAN1 = 1,
   WPAD_CHAN2 = 2,
   WPAD_CHAN3 = 3
};

typedef s8 WPADError;
/* legal values for WPADError */
enum {
    WPAD_ERROR_NONE          =  0,
    WPAD_ERROR_NO_CONTROLLER = -1,
    WPAD_ERROR_BUSY          = -2,
    WPAD_ERROR_TRANSFER      = -3,
    WPAD_ERROR_INVALID       = -4,
    WPAD_ERROR_NOPERM        = -5,
    WPAD_ERROR_BROKEN        = -6,
    WPAD_ERROR_CORRUPTED     = -7
};

typedef void (*WPADConnectCallback) (WPADChannel channel, WPADError reason);

#ifdef __cplusplus
}
#endif
