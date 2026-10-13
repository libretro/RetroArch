/* Compile-only stand-in for PSL1GHT's <io/pad.h>, which
 * defines/ps3_defines.h includes unconditionally. It declares what
 * ps3_joypad.c uses, each in the shape and layout PSL1GHT gives it;
 * padData carries only its button array, not the bit-field view of
 * it the real union also offers. */
#ifndef PS3STUB_IO_PAD_H
#define PS3STUB_IO_PAD_H

#include <ppu-types.h>

#define MAX_PAD_CODES    (64)
#define MAX_PORT_NUM     (7)
#define MAX_PAD_ACTUATOR (2)

typedef struct _pad_info2
{
   u32 max;
   u32 connected;
   u32 info;
   u32 port_status[MAX_PORT_NUM];
   u32 port_setting[MAX_PORT_NUM];
   u32 device_capability[MAX_PORT_NUM];
   u32 device_type[MAX_PORT_NUM];
} padInfo2;

typedef struct _pad_data
{
   s32 len;
   u16 button[MAX_PAD_CODES];
} padData;

typedef struct _pad_act_param
{
   union
   {
      u8 motor[MAX_PAD_ACTUATOR];
      struct
      {
         u8 small_motor;
         u8 large_motor;
      };
   };
   u8 reserved[6];
} padActParam;

s32 ioPadInit(u32 max);
s32 ioPadEnd(void);
s32 ioPadGetInfo2(padInfo2 *info);
s32 ioPadGetData(u32 port, padData *data);
u32 ioPadSetActDirect(u32 port, padActParam *actParam);

#endif
