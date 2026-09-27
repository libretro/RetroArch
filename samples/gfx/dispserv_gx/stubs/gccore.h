/* Host stand-in for the libogc surface gfx/display_servers/dispserv_gx.c
 * reaches: the VI constants and the four VIDEO_* queries, in the shapes
 * libogc declares them. The test drives what they return. */
#ifndef DISPSERV_GX_TEST_GCCORE_H
#define DISPSERV_GX_TEST_GCCORE_H

#include <stdint.h>

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;

#define VI_NTSC               0
#define VI_PAL                1
#define VI_MPAL               2
#define VI_DEBUG              3
#define VI_DEBUG_PAL          4
#define VI_EURGB60            5

#define VI_MAX_WIDTH_NTSC     720
#define VI_MAX_HEIGHT_NTSC    480
#define VI_MAX_WIDTH_PAL      720
#define VI_MAX_HEIGHT_PAL     576
#define VI_MAX_WIDTH_MPAL     720
#define VI_MAX_HEIGHT_MPAL    480
#define VI_MAX_WIDTH_EURGB60  VI_MAX_WIDTH_NTSC
#define VI_MAX_HEIGHT_EURGB60 VI_MAX_HEIGHT_NTSC

typedef struct _gx_rmodeobj
{
   u32 viTVMode;
   u16 fbWidth;
   u16 efbHeight;
   u16 xfbHeight;
   u16 viXOrigin;
   u16 viYOrigin;
   u16 viWidth;
   u16 viHeight;
   u32 xfbMode;
   u8  field_rendering;
   u8  aa;
   u8  sample_pattern[12][2];
   u8  vfilter[7];
} GXRModeObj;

u32 VIDEO_HaveComponentCable(void);
u32 VIDEO_GetCurrentTvMode(void);
GXRModeObj *VIDEO_GetPreferredMode(GXRModeObj *mode);

#endif
