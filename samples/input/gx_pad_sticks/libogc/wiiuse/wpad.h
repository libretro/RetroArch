/* Stand-in for libogc's wiiuse/wpad.h (and the wiiuse types it
 * brings): the constants as libogc defines them, the fields of the
 * report the gx joypad driver reads; the test defines the calls. */
#ifndef GX_JOYPAD_TEST_WPAD_H
#define GX_JOYPAD_TEST_WPAD_H

#include "../gccore.h"

enum
{
   WPAD_CHAN_ALL = -1,
   WPAD_CHAN_0,
   WPAD_CHAN_1,
   WPAD_CHAN_2,
   WPAD_CHAN_3
};

#define WPAD_BUTTON_2              0x0001
#define WPAD_BUTTON_1              0x0002
#define WPAD_BUTTON_B              0x0004
#define WPAD_BUTTON_A              0x0008
#define WPAD_BUTTON_MINUS          0x0010
#define WPAD_BUTTON_HOME           0x0080
#define WPAD_BUTTON_LEFT           0x0100
#define WPAD_BUTTON_RIGHT          0x0200
#define WPAD_BUTTON_DOWN           0x0400
#define WPAD_BUTTON_UP             0x0800
#define WPAD_BUTTON_PLUS           0x1000
#define WPAD_NUNCHUK_BUTTON_Z      (0x0001<<16)
#define WPAD_NUNCHUK_BUTTON_C      (0x0002<<16)
#define WPAD_CLASSIC_BUTTON_UP     (0x0001u<<16)
#define WPAD_CLASSIC_BUTTON_LEFT   (0x0002u<<16)
#define WPAD_CLASSIC_BUTTON_ZR     (0x0004u<<16)
#define WPAD_CLASSIC_BUTTON_X      (0x0008u<<16)
#define WPAD_CLASSIC_BUTTON_A      (0x0010u<<16)
#define WPAD_CLASSIC_BUTTON_Y      (0x0020u<<16)
#define WPAD_CLASSIC_BUTTON_B      (0x0040u<<16)
#define WPAD_CLASSIC_BUTTON_ZL     (0x0080u<<16)
#define WPAD_CLASSIC_BUTTON_FULL_R (0x0200u<<16)
#define WPAD_CLASSIC_BUTTON_PLUS   (0x0400u<<16)
#define WPAD_CLASSIC_BUTTON_HOME   (0x0800u<<16)
#define WPAD_CLASSIC_BUTTON_MINUS  (0x1000u<<16)
#define WPAD_CLASSIC_BUTTON_FULL_L (0x2000u<<16)
#define WPAD_CLASSIC_BUTTON_DOWN   (0x4000u<<16)
#define WPAD_CLASSIC_BUTTON_RIGHT  (0x8000u<<16)

enum
{
   WPAD_EXP_NONE = 0,
   WPAD_EXP_NUNCHUK,
   WPAD_EXP_CLASSIC,
   WPAD_EXP_GUITARHERO3,
   WPAD_EXP_WIIBOARD,
   WPAD_EXP_UNKNOWN = 255
};

#define WPAD_ERR_NONE         0
#define WPAD_FMT_BTNS_ACC_IR  2

typedef struct joystick_t
{
   float ang;
   float mag;
} joystick_t;

typedef struct nunchuk_t     { joystick_t js; } nunchuk_t;
typedef struct classic_ctrl_t{ joystick_t ljs; joystick_t rjs; } classic_ctrl_t;
typedef struct guitar_hero_3_t { joystick_t js; } guitar_hero_3_t;

typedef struct expansion_t
{
   int type;
   union
   {
      nunchuk_t       nunchuk;
      classic_ctrl_t  classic;
      guitar_hero_3_t gh3;
   };
} expansion_t;

typedef struct ir_t
{
   ubyte valid;
   float x;
   float y;
} ir_t;

typedef struct _wpad_data
{
   s16         err;
   u32         btns_h;
   ir_t        ir;
   expansion_t exp;
} WPADData;

typedef void (*WPADDataCallback)(s32 chan, const WPADData *data);

s32       WPAD_Init(void);
s32       WPAD_ReadPending(s32 chan, WPADDataCallback datacb);
s32       WPAD_SetDataFormat(s32 chan, s32 fmt);
s32       WPAD_SetVRes(s32 chan, u32 xres, u32 yres);
s32       WPAD_Probe(s32 chan, u32 *type);
s32       WPAD_Disconnect(s32 chan);
s32       WPAD_Flush(s32 chan);
void      WPAD_IR(int chan, ir_t *ir);
WPADData *WPAD_Data(int chan);

#endif
