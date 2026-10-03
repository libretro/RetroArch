/* PSL1GHT <sysutil/video.h>, the part of it compiled against here:
 * the video output queries gfx/display_servers/dispserv_ps3.c makes,
 * in PSL1GHT's layouts and values (stdint types for its u8/u16/s32).
 * defines/ps3_defines.h includes this unconditionally. Declare what a
 * file needs as one arrives. */
#ifndef PS3STUB_SYSUTIL_VIDEO_H
#define PS3STUB_SYSUTIL_VIDEO_H

#include <stdint.h>

#define VIDEO_PRIMARY              0
#define VIDEO_ASPECT_AUTO          0

#define VIDEO_RESOLUTION_1080      1
#define VIDEO_RESOLUTION_720       2
#define VIDEO_RESOLUTION_480       4
#define VIDEO_RESOLUTION_576       5
#define VIDEO_RESOLUTION_1600x1080 10
#define VIDEO_RESOLUTION_1440x1080 11
#define VIDEO_RESOLUTION_1280x1080 12
#define VIDEO_RESOLUTION_960x1080  13

typedef struct _videodisplaymode
{
   uint8_t  resolution;
   uint8_t  scanMode;
   uint8_t  conversion;
   uint8_t  aspect;
   uint8_t  padding[2];
   uint16_t refreshRates;
} videoDisplayMode;

typedef struct _videostate
{
   uint8_t state;
   uint8_t colorSpace;
   uint8_t padding[6];
   videoDisplayMode displayMode;
} videoState;

int32_t videoGetState(int32_t videoOut, int32_t deviceIndex,
      videoState *state);
int32_t videoGetResolutionAvailability(uint32_t videoOut,
      uint32_t resolutionId, uint32_t aspect, uint32_t option);

#endif
