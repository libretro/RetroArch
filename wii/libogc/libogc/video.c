/*-------------------------------------------------------------

video.c -- VIDEO subsystem

 Copyright (C) 2004 - 2008
   Michael Wiedenbauer (shagkur)
   Dave Murphy (WinterMute)

 Redistribution and use in source and binary forms, with or without modification,
 are permitted provided that the following conditions are met:

  1. Redistributions of source code must retain the above copyright notice,
     this list of conditions and the following disclaimer.
  2. Redistributions in binary form must reproduce the above copyright notice,
     this list of conditions and the following disclaimer in the documentation and/or
     other materials provided with the distribution.
  3. The name of the author may not be used to endorse or promote products derived
     from this software without specific prior written permission.

 THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE
 LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

-------------------------------------------------------------*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "asm.h"
#include "processor.h"
#include "ogcsys.h"
#include "irq.h"
#include "exi.h"
#include "gx.h"
#include "si.h"
#include "lwp.h"
#include "system.h"
#include "video.h"
#include "video_types.h"

#define VIDEO_MQ					1

#define _SHIFTL(v, s, w)	\
    ((u32) (((u32)(v) & ((0x01 << (w)) - 1)) << (s)))
#define _SHIFTR(v, s, w)	\
    ((u32)(((u32)(v) >> (s)) & ((0x01 << (w)) - 1)))

#define VI_REGCHANGE(_reg)	\
	((u64)0x01<<(63-_reg))

typedef struct _horVer {
	u16 dispPosX;
	u16 dispPosY;
	u16 dispSizeX;
	u16 dispSizeY;
	u16 adjustedDispPosX;
	u16 adjustedDispPosY;
	u16 adjustedDispSizeY;
	u16 adjustedPanPosY;
	u16 adjustedPanSizeY;
	u16 fbSizeX;
	u16 fbSizeY;
	u16 panPosX;
	u16 panPosY;
	u16 panSizeX;
	u16 panSizeY;
	u32 fbMode;
	u32 nonInter;
	u32 tv;
	u8 wordPerLine;
	u8 std;
	u8 wpl;
	void *bufAddr;
	u32 tfbb;
	u32 bfbb;
	u8 xof;
	s32 black;
	s32 threeD;
	void *rbufAddr;
	u32 rtfbb;
	u32 rbfbb;
	const struct _timing *timing;
} horVer;

GXRModeObj TVNtsc240Ds =
{
    VI_TVMODE_NTSC_DS,      // viDisplayMode
    640,             // fbWidth
    240,             // efbHeight
    240,             // xfbHeight
    (VI_MAX_WIDTH_NTSC - 640)/2,        // viXOrigin
    (VI_MAX_HEIGHT_NTSC - 480)/2,       // viYOrigin
    640,             // viWidth
    480,             // viHeight
    VI_XFBMODE_SF,   // xFBmode
    GX_FALSE,        // field_rendering
    GX_FALSE,        // aa

     // sample points arranged in increasing Y order
	{
		{6,6},{6,6},{6,6},  // pix 0, 3 sample points, 1/12 units, 4 bits each
		{6,6},{6,6},{6,6},  // pix 1
		{6,6},{6,6},{6,6},  // pix 2
		{6,6},{6,6},{6,6}   // pix 3
	},

     // vertical filter[7], 1/64 units, 6 bits each
	{
		  0,         // line n-1
		  0,         // line n-1
		 21,         // line n
		 22,         // line n
		 21,         // line n
		  0,         // line n+1
		  0          // line n+1
	}
};

GXRModeObj TVNtsc240DsAa =
{
    VI_TVMODE_NTSC_DS,      // viDisplayMode
    640,             // fbWidth
    240,             // efbHeight
    240,             // xfbHeight
    (VI_MAX_WIDTH_NTSC - 640)/2,        // viXOrigin
    (VI_MAX_HEIGHT_NTSC - 480)/2,       // viYOrigin
    640,             // viWidth
    480,             // viHeight
    VI_XFBMODE_SF,   // xFBmode
    GX_FALSE,        // field_rendering
    GX_TRUE,         // aa

    // sample points arranged in increasing Y order
	{
		{3,2},{9,6},{3,10},  // pix 0, 3 sample points, 1/12 units, 4 bits each
		{3,2},{9,6},{3,10},  // pix 1
		{9,2},{3,6},{9,10},  // pix 2
		{9,2},{3,6},{9,10}   // pix 3
	},

    // vertical filter[7], 1/64 units, 6 bits each
	{
		 0,         // line n-1
		 0,         // line n-1
		 21,        // line n
		 22,        // line n
		 21,        // line n
		 0,         // line n+1
		 0          // line n+1
	}
};

GXRModeObj TVNtsc240Int =
{
    VI_TVMODE_NTSC_INT,     // viDisplayMode
    640,             // fbWidth
    240,             // efbHeight
    240,             // xfbHeight
    (VI_MAX_WIDTH_NTSC - 640)/2,        // viXOrigin
    (VI_MAX_HEIGHT_NTSC - 480)/2,       // viYOrigin
    640,             // viWidth
    480,             // viHeight
    VI_XFBMODE_SF,   // xFBmode
    GX_TRUE,         // field_rendering
    GX_FALSE,        // aa

    // sample points arranged in increasing Y order
	{
		{6,6},{6,6},{6,6},  // pix 0, 3 sample points, 1/12 units, 4 bits each
		{6,6},{6,6},{6,6},  // pix 1
		{6,6},{6,6},{6,6},  // pix 2
		{6,6},{6,6},{6,6}   // pix 3
	},

    // vertical filter[7], 1/64 units, 6 bits each
	{
		 0,         // line n-1
		 0,         // line n-1
		21,         // line n
		22,         // line n
		21,         // line n
		 0,         // line n+1
		 0          // line n+1
	}
};

GXRModeObj TVNtsc240IntAa =
{
    VI_TVMODE_NTSC_INT,     // viDisplayMode
    640,             // fbWidth
    240,             // efbHeight
    240,             // xfbHeight
    (VI_MAX_WIDTH_NTSC - 640)/2,        // viXOrigin
    (VI_MAX_HEIGHT_NTSC - 480)/2,       // viYOrigin
    640,             // viWidth
    480,             // viHeight
    VI_XFBMODE_SF,   // xFBmode
    GX_TRUE,         // field_rendering
    GX_TRUE,         // aa

    // sample points arranged in increasing Y order
	{
		{3,2},{9,6},{3,10},  // pix 0, 3 sample points, 1/12 units, 4 bits each
		{3,2},{9,6},{3,10},  // pix 1
		{9,2},{3,6},{9,10},  // pix 2
		{9,2},{3,6},{9,10}   // pix 3
	},

    // vertical filter[7], 1/64 units, 6 bits each
	{
		 0,         // line n-1
		 0,         // line n-1
		 21,        // line n
		 22,        // line n
		 21,        // line n
		 0,         // line n+1
		 0          // line n+1
	}
};

GXRModeObj TVNtsc480Int =
{
    VI_TVMODE_NTSC_INT,     // viDisplayMode
    640,             // fbWidth
    480,             // efbHeight
    480,             // xfbHeight
    (VI_MAX_WIDTH_NTSC - 640)/2,        // viXOrigin
    (VI_MAX_HEIGHT_NTSC - 480)/2,       // viYOrigin
    640,             // viWidth
    480,             // viHeight
    VI_XFBMODE_DF,   // xFBmode
    GX_FALSE,        // field_rendering
    GX_FALSE,        // aa

    // sample points arranged in increasing Y order
    {
		{6,6},{6,6},{6,6},  // pix 0, 3 sample points, 1/12 units, 4 bits each
		{6,6},{6,6},{6,6},  // pix 1
		{6,6},{6,6},{6,6},  // pix 2
		{6,6},{6,6},{6,6}   // pix 3
    },

    // vertical filter[7], 1/64 units, 6 bits each
    {
          0,         // line n-1
          0,         // line n-1
         21,         // line n
         22,         // line n
         21,         // line n
          0,         // line n+1
          0          // line n+1
    }
};

GXRModeObj TVNtsc480IntDf =
{
    VI_TVMODE_NTSC_INT,     // viDisplayMode
    640,             // fbWidth
    480,             // efbHeight
    480,             // xfbHeight
    (VI_MAX_WIDTH_NTSC - 640)/2,        // viXOrigin
    (VI_MAX_HEIGHT_NTSC - 480)/2,       // viYOrigin
    640,             // viWidth
    480,             // viHeight
    VI_XFBMODE_DF,   // xFBmode
    GX_FALSE,        // field_rendering
    GX_FALSE,        // aa

    // sample points arranged in increasing Y order
	{
		{6,6},{6,6},{6,6},  // pix 0, 3 sample points, 1/12 units, 4 bits each
		{6,6},{6,6},{6,6},  // pix 1
		{6,6},{6,6},{6,6},  // pix 2
		{6,6},{6,6},{6,6}   // pix 3
	},

    // vertical filter[7], 1/64 units, 6 bits each
	{
		 8,         // line n-1
		 8,         // line n-1
		10,         // line n
		12,         // line n
		10,         // line n
		 8,         // line n+1
		 8          // line n+1
	}
};

GXRModeObj TVNtsc480IntAa =
{
    VI_TVMODE_NTSC_INT,     // viDisplayMode
    640,             // fbWidth
    242,             // efbHeight
    480,             // xfbHeight
    (VI_MAX_WIDTH_NTSC - 640)/2,        // viXOrigin
    (VI_MAX_HEIGHT_NTSC - 480)/2,       // viYOrigin
    640,             // viWidth
    480,             // viHeight
    VI_XFBMODE_DF,   // xFBmode
    GX_FALSE,        // field_rendering
    GX_TRUE,         // aa

    // sample points arranged in increasing Y order
	{
		{3,2},{9,6},{3,10},  // pix 0, 3 sample points, 1/12 units, 4 bits each
		{3,2},{9,6},{3,10},  // pix 1
		{9,2},{3,6},{9,10},  // pix 2
		{9,2},{3,6},{9,10}   // pix 3
	},

    // vertical filter[7], 1/64 units, 6 bits each
	{
		 4,         // line n-1
		 8,         // line n-1
		12,         // line n
		16,         // line n
		12,         // line n
		 8,         // line n+1
		 4          // line n+1
	}
};

GXRModeObj TVNtsc480Prog =
{
    VI_TVMODE_NTSC_PROG,     // viDisplayMode
    640,             // fbWidth
    480,             // efbHeight
    480,             // xfbHeight
    (VI_MAX_WIDTH_NTSC - 640)/2,        // viXOrigin
    (VI_MAX_HEIGHT_NTSC - 480)/2,       // viYOrigin
    640,             // viWidth
    480,             // viHeight
    VI_XFBMODE_SF,   // xFBmode
    GX_FALSE,        // field_rendering
    GX_FALSE,        // aa

    // sample points arranged in increasing Y order
    {
		{6,6},{6,6},{6,6},  // pix 0, 3 sample points, 1/12 units, 4 bits each
		{6,6},{6,6},{6,6},  // pix 1
		{6,6},{6,6},{6,6},  // pix 2
		{6,6},{6,6},{6,6}   // pix 3
    },

    // vertical filter[7], 1/64 units, 6 bits each
    {
          0,         // line n-1
          0,         // line n-1
         21,         // line n
         22,         // line n
         21,         // line n
          0,         // line n+1
          0          // line n+1
    }
};

GXRModeObj TVNtsc480ProgSoft =
{
    VI_TVMODE_NTSC_PROG,     // viDisplayMode
    640,             // fbWidth
    480,             // efbHeight
    480,             // xfbHeight
    (VI_MAX_WIDTH_NTSC - 640)/2,        // viXOrigin
    (VI_MAX_HEIGHT_NTSC - 480)/2,       // viYOrigin
    640,             // viWidth
    480,             // viHeight
    VI_XFBMODE_SF,   // xFBmode
    GX_FALSE,        // field_rendering
    GX_FALSE,        // aa

    // sample points arranged in increasing Y order
    {
		{6,6},{6,6},{6,6},  // pix 0, 3 sample points, 1/12 units, 4 bits each
		{6,6},{6,6},{6,6},  // pix 1
		{6,6},{6,6},{6,6},  // pix 2
		{6,6},{6,6},{6,6}   // pix 3
    },

    // vertical filter[7], 1/64 units, 6 bits each
    {
          8,         // line n-1
          8,         // line n-1
         10,         // line n
         12,         // line n
         10,         // line n
          8,         // line n+1
          8          // line n+1
    }
};

GXRModeObj TVNtsc480ProgAa =
{
    VI_TVMODE_NTSC_PROG,     // viDisplayMode
    640,             // fbWidth
    242,             // efbHeight
    480,             // xfbHeight
    (VI_MAX_WIDTH_NTSC - 640)/2,        // viXOrigin
    (VI_MAX_HEIGHT_NTSC - 480)/2,       // viYOrigin
    640,             // viWidth
    480,             // viHeight
    VI_XFBMODE_SF,   // xFBmode
    GX_FALSE,        // field_rendering
    GX_TRUE,        // aa

    // sample points arranged in increasing Y order
    {
		{3,2},{9,6},{3,10},  // pix 0, 3 sample points, 1/12 units, 4 bits each
		{3,2},{9,6},{3,10},  // pix 1
		{9,2},{3,6},{9,10},  // pix 2
		{9,2},{3,6},{9,10}   // pix 3
    },

    // vertical filter[7], 1/64 units, 6 bits each
    {
          4,         // line n-1
          8,         // line n-1
         12,         // line n
         16,         // line n
         12,         // line n
          8,         // line n+1
          4          // line n+1
    }
};

GXRModeObj TVMpal240Ds =
{
    VI_TVMODE_MPAL_DS,     // viDisplayMode
    640,             // fbWidth
    240,             // efbHeight
    240,             // xfbHeight
    (VI_MAX_WIDTH_MPAL - 640)/2,        // viXOrigin
    (VI_MAX_HEIGHT_MPAL - 480)/2,       // viYOrigin
    640,             // viWidth
    480,             // viHeight
    VI_XFBMODE_SF,   // xFBmode
    GX_FALSE,        // field_rendering
    GX_FALSE,        // aa

    // sample points arranged in increasing Y order
	{
		{6,6},{6,6},{6,6},  // pix 0, 3 sample points, 1/12 units, 4 bits each
		{6,6},{6,6},{6,6},  // pix 1
		{6,6},{6,6},{6,6},  // pix 2
		{6,6},{6,6},{6,6}   // pix 3
	},

    // vertical filter[7], 1/64 units, 6 bits each
	{
		 0,         // line n-1
		 0,         // line n-1
		21,         // line n
		22,         // line n
		21,         // line n
		 0,         // line n+1
		 0          // line n+1
	}
};

GXRModeObj TVMpal240DsAa =
{
    VI_TVMODE_MPAL_DS,     // viDisplayMode
    640,             // fbWidth
    240,             // efbHeight
    240,             // xfbHeight
    (VI_MAX_WIDTH_MPAL - 640)/2,        // viXOrigin
    (VI_MAX_HEIGHT_MPAL - 480)/2,       // viYOrigin
    640,             // viWidth
    480,             // viHeight
    VI_XFBMODE_SF,   // xFBmode
    GX_FALSE,        // field_rendering
    GX_TRUE,         // aa

    // sample points arranged in increasing Y order
	{
		{3,2},{9,6},{3,10},  // pix 0, 3 sample points, 1/12 units, 4 bits each
		{3,2},{9,6},{3,10},  // pix 1
		{9,2},{3,6},{9,10},  // pix 2
		{9,2},{3,6},{9,10}   // pix 3
	},

    // vertical filter[7], 1/64 units, 6 bits each
	{
		 0,         // line n-1
		 0,         // line n-1
		21,         // line n
		22,         // line n
		21,         // line n
		 0,         // line n+1
		 0          // line n+1
	}
};

GXRModeObj TVMpal480IntDf =
{
    VI_TVMODE_MPAL_INT,     // viDisplayMode
    640,             // fbWidth
    480,             // efbHeight
    480,             // xfbHeight
    (VI_MAX_WIDTH_MPAL - 640)/2,        // viXOrigin
    (VI_MAX_HEIGHT_MPAL - 480)/2,       // viYOrigin
    640,             // viWidth
    480,             // viHeight
    VI_XFBMODE_DF,   // xFBmode
    GX_FALSE,        // field_rendering
    GX_FALSE,        // aa

    // sample points arranged in increasing Y order
	{
		{6,6},{6,6},{6,6},  // pix 0, 3 sample points, 1/12 units, 4 bits each
		{6,6},{6,6},{6,6},  // pix 1
		{6,6},{6,6},{6,6},  // pix 2
		{6,6},{6,6},{6,6}   // pix 3
	},

    // vertical filter[7], 1/64 units, 6 bits each
	{
		 8,         // line n-1
		 8,         // line n-1
		10,         // line n
		12,         // line n
		10,         // line n
		 8,         // line n+1
		 8          // line n+1
	}
};

GXRModeObj TVMpal480IntAa =
{
    VI_TVMODE_MPAL_INT,     // viDisplayMode
    640,             // fbWidth
    242,             // efbHeight
    480,             // xfbHeight
    (VI_MAX_WIDTH_MPAL - 640)/2,        // viXOrigin
    (VI_MAX_HEIGHT_MPAL - 480)/2,       // viYOrigin
    640,             // viWidth
    480,             // viHeight
    VI_XFBMODE_DF,   // xFBmode
    GX_FALSE,        // field_rendering
    GX_TRUE,         // aa

    // sample points arranged in increasing Y order
	{
		{3,2},{9,6},{3,10},				// pix 0, 3 sample points, 1/12 units, 4 bits each
		{3,2},{9,6},{3,10},				// pix 1
		{9,2},{3,6},{9,10},				// pix 2
		{9,2},{3,6},{9,10}				// pix 3
	},

    // vertical filter[7], 1/64 units, 6 bits each
	{
		 4,         // line n-1
		 8,         // line n-1
		12,         // line n
		16,         // line n
		12,         // line n
		 8,         // line n+1
		 4          // line n+1
	}
};

GXRModeObj TVMpal480Prog =
{
    VI_TVMODE_MPAL_PROG,     // viDisplayMode
    640,             // fbWidth
    480,             // efbHeight
    480,             // xfbHeight
    (VI_MAX_WIDTH_MPAL - 640)/2,        // viXOrigin
    (VI_MAX_HEIGHT_MPAL - 480)/2,       // viYOrigin
    640,             // viWidth
    480,             // viHeight
    VI_XFBMODE_SF,   // xFBmode
    GX_FALSE,        // field_rendering
    GX_FALSE,        // aa

    // sample points arranged in increasing Y order
	{
		{6,6},{6,6},{6,6},  // pix 0, 3 sample points, 1/12 units, 4 bits each
		{6,6},{6,6},{6,6},  // pix 1
		{6,6},{6,6},{6,6},  // pix 2
		{6,6},{6,6},{6,6}   // pix 3
	},

    // vertical filter[7], 1/64 units, 6 bits each
	{
		 0,         // line n-1
		 0,         // line n-1
		21,         // line n
		22,         // line n
		21,         // line n
		 0,         // line n+1
		 0          // line n+1
	}
};

GXRModeObj TVPal264Ds =
{
    VI_TVMODE_PAL_DS,       // viDisplayMode
    640,             // fbWidth
    264,             // efbHeight
    264,             // xfbHeight
    (VI_MAX_WIDTH_PAL - 640)/2,         // viXOrigin
    (VI_MAX_HEIGHT_PAL - 528)/2,        // viYOrigin
    640,             // viWidth
    528,             // viHeight
    VI_XFBMODE_SF,   // xFBmode
    GX_FALSE,        // field_rendering
    GX_FALSE,        // aa

    // sample points arranged in increasing Y order
	{
		{6,6},{6,6},{6,6},  // pix 0, 3 sample points, 1/12 units, 4 bits each
		{6,6},{6,6},{6,6},  // pix 1
		{6,6},{6,6},{6,6},  // pix 2
		{6,6},{6,6},{6,6}   // pix 3
	},

    // vertical filter[7], 1/64 units, 6 bits each
	{
		 0,         // line n-1
		 0,         // line n-1
		21,         // line n
		22,         // line n
		21,         // line n
		 0,         // line n+1
		 0          // line n+1
	}
};

GXRModeObj TVPal264DsAa =
{
    VI_TVMODE_PAL_DS,       // viDisplayMode
    640,             // fbWidth
    264,             // efbHeight
    264,             // xfbHeight
    (VI_MAX_WIDTH_PAL - 640)/2,         // viXOrigin
    (VI_MAX_HEIGHT_PAL - 528)/2,        // viYOrigin
    640,             // viWidth
    528,             // viHeight
    VI_XFBMODE_SF,   // xFBmode
    GX_FALSE,        // field_rendering
    GX_TRUE,         // aa

    // sample points arranged in increasing Y order
	{
		{3,2},{9,6},{3,10},  // pix 0, 3 sample points, 1/12 units, 4 bits each
		{3,2},{9,6},{3,10},  // pix 1
		{9,2},{3,6},{9,10},  // pix 2
		{9,2},{3,6},{9,10}   // pix 3
	},

    // vertical filter[7], 1/64 units, 6 bits each
	{
		 0,         // line n-1
		 0,         // line n-1
		21,         // line n
		22,         // line n
		21,         // line n
		 0,         // line n+1
		 0          // line n+1
	}
};

GXRModeObj TVPal264Int =
{
    VI_TVMODE_PAL_INT,      // viDisplayMode
    640,             // fbWidth
    264,             // efbHeight
    264,             // xfbHeight
    (VI_MAX_WIDTH_PAL - 640)/2,         // viXOrigin
    (VI_MAX_HEIGHT_PAL - 528)/2,        // viYOrigin
    640,             // viWidth
    528,             // viHeight
    VI_XFBMODE_SF,   // xFBmode
    GX_TRUE,         // field_rendering
    GX_FALSE,        // aa

    // sample points arranged in increasing Y order
	{
		{6,6},{6,6},{6,6},  // pix 0, 3 sample points, 1/12 units, 4 bits each
		{6,6},{6,6},{6,6},  // pix 1
		{6,6},{6,6},{6,6},  // pix 2
		{6,6},{6,6},{6,6}   // pix 3
	},

    // vertical filter[7], 1/64 units, 6 bits each
	{
		 0,         // line n-1
		 0,         // line n-1
		21,         // line n
		22,         // line n
		21,         // line n
		 0,         // line n+1
		 0          // line n+1
	}
};

GXRModeObj TVPal264IntAa =
{
    VI_TVMODE_PAL_INT,      // viDisplayMode
    640,             // fbWidth
    264,             // efbHeight
    264,             // xfbHeight
    (VI_MAX_WIDTH_PAL - 640)/2,         // viXOrigin
    (VI_MAX_HEIGHT_PAL - 528)/2,        // viYOrigin
    640,             // viWidth
    528,             // viHeight
    VI_XFBMODE_SF,   // xFBmode
    GX_TRUE,         // field_rendering
    GX_TRUE,         // aa

    // sample points arranged in increasing Y order
	{
		{3,2},{9,6},{3,10},  // pix 0, 3 sample points, 1/12 units, 4 bits each
		{3,2},{9,6},{3,10},  // pix 1
		{9,2},{3,6},{9,10},  // pix 2
		{9,2},{3,6},{9,10}   // pix 3
	},

    // vertical filter[7], 1/64 units, 6 bits each
	{
		 0,         // line n-1
		 0,         // line n-1
		21,         // line n
		22,         // line n
		21,         // line n
		 0,         // line n+1
		 0          // line n+1
	}
};

GXRModeObj TVPal524IntAa =
{
	VI_TVMODE_PAL_INT,
	640,
	264,
	524,
	(VI_MAX_WIDTH_PAL-640)/2,
	(VI_MAX_HEIGHT_PAL-528)/2,
	640,
	524,
	VI_XFBMODE_DF,
	GX_FALSE,
	GX_TRUE,

    // sample points arranged in increasing Y order
	{
		{3,2},{9,6},{3,10},				// pix 0, 3 sample points, 1/12 units, 4 bits each
		{3,2},{9,6},{3,10},				// pix 1
		{9,2},{3,6},{9,10},				// pix 2
		{9,2},{3,6},{9,10}				// pix 3
	},

	 // vertical filter[7], 1/64 units, 6 bits each
	{
		4,         // line n-1
		8,         // line n-1
		12,        // line n
		16,        // line n
		12,        // line n
		8,         // line n+1
		4          // line n+1
	}
};

GXRModeObj TVPal528Int =
{
    VI_TVMODE_PAL_INT,       // viDisplayMode
    640,             // fbWidth
    528,             // efbHeight
    528,             // xfbHeight
    (VI_MAX_WIDTH_PAL - 640)/2,         // viXOrigin
    (VI_MAX_HEIGHT_PAL - 528)/2,        // viYOrigin
    640,             // viWidth
    528,             // viHeight
    VI_XFBMODE_DF,   // xFBmode
    GX_FALSE,        // field_rendering
    GX_FALSE,        // aa

    // sample points arranged in increasing Y order
	{
		{6,6},{6,6},{6,6},  // pix 0, 3 sample points, 1/12 units, 4 bits each
		{6,6},{6,6},{6,6},  // pix 1
		{6,6},{6,6},{6,6},  // pix 2
		{6,6},{6,6},{6,6}   // pix 3
	},

    // vertical filter[7], 1/64 units, 6 bits each
	{
		 0,         // line n-1
		 0,         // line n-1
		21,         // line n
		22,         // line n
		21,         // line n
		 0,         // line n+1
		 0          // line n+1
	}
};

GXRModeObj TVPal528IntDf =
{
    VI_TVMODE_PAL_INT,      // viDisplayMode
    640,             // fbWidth
    528,             // efbHeight
    528,             // xfbHeight
    (VI_MAX_WIDTH_PAL - 640)/2,         // viXOrigin
    (VI_MAX_HEIGHT_PAL - 528)/2,        // viYOrigin
    640,             // viWidth
    528,             // viHeight
    VI_XFBMODE_DF,   // xFBmode
    GX_FALSE,        // field_rendering
    GX_FALSE,        // aa

    // sample points arranged in increasing Y order
	{
		{6,6},{6,6},{6,6},  // pix 0, 3 sample points, 1/12 units, 4 bits each
		{6,6},{6,6},{6,6},  // pix 1
		{6,6},{6,6},{6,6},  // pix 2
		{6,6},{6,6},{6,6}   // pix 3
	},
    // vertical filter[7], 1/64 units, 6 bits each
	{
		 8,         // line n-1
		 8,         // line n-1
		10,         // line n
		12,         // line n
		10,         // line n
		 8,         // line n+1
		 8          // line n+1
	}
};

GXRModeObj TVPal576IntDfScale =
{
    VI_TVMODE_PAL_INT,      // viDisplayMode
    640,             // fbWidth
    480,             // efbHeight
    576,             // xfbHeight
    (VI_MAX_WIDTH_PAL - 640)/2,         // viXOrigin
    (VI_MAX_HEIGHT_PAL - 576)/2,        // viYOrigin
    640,             // viWidth
    576,             // viHeight
    VI_XFBMODE_DF,   // xFBmode
    GX_FALSE,        // field_rendering
    GX_FALSE,        // aa

    // sample points arranged in increasing Y order
	{
		{6,6},{6,6},{6,6},  // pix 0, 3 sample points, 1/12 units, 4 bits each
		{6,6},{6,6},{6,6},  // pix 1
		{6,6},{6,6},{6,6},  // pix 2
		{6,6},{6,6},{6,6}   // pix 3
	},
    // vertical filter[7], 1/64 units, 6 bits each
	{
		 8,         // line n-1
		 8,         // line n-1
		10,         // line n
		12,         // line n
		10,         // line n
		 8,         // line n+1
		 8          // line n+1
	}
};

GXRModeObj TVPal576ProgScale =
{
    VI_TVMODE_PAL_PROG,      // viDisplayMode
    640,             // fbWidth
    480,             // efbHeight
    576,             // xfbHeight
    (VI_MAX_WIDTH_PAL - 640)/2,         // viXOrigin
    (VI_MAX_HEIGHT_PAL - 576)/2,        // viYOrigin
    640,             // viWidth
    576,             // viHeight
    VI_XFBMODE_SF,   // xFBmode
    GX_FALSE,        // field_rendering
    GX_FALSE,        // aa

    // sample points arranged in increasing Y order
	{
		{6,6},{6,6},{6,6},  // pix 0, 3 sample points, 1/12 units, 4 bits each
		{6,6},{6,6},{6,6},  // pix 1
		{6,6},{6,6},{6,6},  // pix 2
		{6,6},{6,6},{6,6}   // pix 3
	},
    // vertical filter[7], 1/64 units, 6 bits each
    {
		 0,         // line n-1
		 0,         // line n-1
		21,         // line n
		22,         // line n
		21,         // line n
		 0,         // line n+1
		 0          // line n+1
    }
};

GXRModeObj TVEurgb60Hz240Ds =
{
    VI_TVMODE_EURGB60_DS,      // viDisplayMode
    640,             // fbWidth
    240,             // efbHeight
    240,             // xfbHeight
    (VI_MAX_WIDTH_EURGB60 - 640)/2,         // viXOrigin
    (VI_MAX_HEIGHT_EURGB60 - 480)/2,        // viYOrigin
    640,             // viWidth
    480,             // viHeight
    VI_XFBMODE_SF,   // xFBmode
    GX_FALSE,        // field_rendering
    GX_FALSE,        // aa

    // sample points arranged in increasing Y order
	{
		{6,6},{6,6},{6,6},  // pix 0, 3 sample points, 1/12 units, 4 bits each
		{6,6},{6,6},{6,6},  // pix 1
		{6,6},{6,6},{6,6},  // pix 2
		{6,6},{6,6},{6,6}   // pix 3
	},
    // vertical filter[7], 1/64 units, 6 bits each
	{
		 0,         // line n-1
		 0,         // line n-1
		21,         // line n
		22,         // line n
		21,         // line n
		 0,         // line n+1
		 0          // line n+1
	}
};

GXRModeObj TVEurgb60Hz240DsAa =
{
    VI_TVMODE_EURGB60_DS,      // viDisplayMode
    640,             // fbWidth
    240,             // efbHeight
    240,             // xfbHeight
    (VI_MAX_WIDTH_EURGB60 - 640)/2,         // viXOrigin
    (VI_MAX_HEIGHT_EURGB60 - 480)/2,        // viYOrigin
    640,             // viWidth
    480,             // viHeight
    VI_XFBMODE_SF,   // xFBmode
    GX_FALSE,        // field_rendering
    GX_TRUE,        // aa

    // sample points arranged in increasing Y order
	{
		{3,2},{9,6},{3,10},				// pix 0, 3 sample points, 1/12 units, 4 bits each
		{3,2},{9,6},{3,10},				// pix 1
		{9,2},{3,6},{9,10},				// pix 2
		{9,2},{3,6},{9,10}				// pix 3
	},
    // vertical filter[7], 1/64 units, 6 bits each
	{
		 0,         // line n-1
		 0,         // line n-1
		21,         // line n
		22,         // line n
		21,         // line n
		 0,         // line n+1
		 0          // line n+1
	}
};

GXRModeObj TVEurgb60Hz240Int =
{
    VI_TVMODE_EURGB60_INT,      // viDisplayMode
    640,             // fbWidth
    240,             // efbHeight
    240,             // xfbHeight
    (VI_MAX_WIDTH_EURGB60 - 640)/2,         // viXOrigin
    (VI_MAX_HEIGHT_EURGB60 - 480)/2,        // viYOrigin
    640,             // viWidth
    480,             // viHeight
    VI_XFBMODE_SF,   // xFBmode
    GX_TRUE,        // field_rendering
    GX_FALSE,        // aa

    // sample points arranged in increasing Y order
	{
		{6,6},{6,6},{6,6},  // pix 0, 3 sample points, 1/12 units, 4 bits each
		{6,6},{6,6},{6,6},  // pix 1
		{6,6},{6,6},{6,6},  // pix 2
		{6,6},{6,6},{6,6}   // pix 3
	},
    // vertical filter[7], 1/64 units, 6 bits each
	{
		 0,         // line n-1
		 0,         // line n-1
		21,         // line n
		22,         // line n
		21,         // line n
		 0,         // line n+1
		 0          // line n+1
	}
};

GXRModeObj TVEurgb60Hz240IntAa =
{
    VI_TVMODE_EURGB60_INT,      // viDisplayMode
    640,             // fbWidth
    240,             // efbHeight
    240,             // xfbHeight
    (VI_MAX_WIDTH_EURGB60 - 640)/2,         // viXOrigin
    (VI_MAX_HEIGHT_EURGB60 - 480)/2,        // viYOrigin
    640,             // viWidth
    480,             // viHeight
    VI_XFBMODE_SF,   // xFBmode
    GX_TRUE,        // field_rendering
    GX_TRUE,        // aa

    // sample points arranged in increasing Y order
	{
		{3,2},{9,6},{3,10},				// pix 0, 3 sample points, 1/12 units, 4 bits each
		{3,2},{9,6},{3,10},				// pix 1
		{9,2},{3,6},{9,10},				// pix 2
		{9,2},{3,6},{9,10}				// pix 3
	},
    // vertical filter[7], 1/64 units, 6 bits each
	{
		 0,         // line n-1
		 0,         // line n-1
		21,         // line n
		22,         // line n
		21,         // line n
		 0,         // line n+1
		 0          // line n+1
	}
};

GXRModeObj TVEurgb60Hz480Int =
{
    VI_TVMODE_EURGB60_INT,      // viDisplayMode
    640,             // fbWidth
    480,             // efbHeight
    480,             // xfbHeight
    (VI_MAX_WIDTH_EURGB60 - 640)/2,         // viXOrigin
    (VI_MAX_HEIGHT_EURGB60 - 480)/2,        // viYOrigin
    640,             // viWidth
    480,             // viHeight
    VI_XFBMODE_DF,   // xFBmode
    GX_FALSE,        // field_rendering
    GX_FALSE,        // aa

    // sample points arranged in increasing Y order
	{
		{6,6},{6,6},{6,6},  // pix 0, 3 sample points, 1/12 units, 4 bits each
		{6,6},{6,6},{6,6},  // pix 1
		{6,6},{6,6},{6,6},  // pix 2
		{6,6},{6,6},{6,6}   // pix 3
	},
    // vertical filter[7], 1/64 units, 6 bits each
	{
		 0,         // line n-1
		 0,         // line n-1
		21,         // line n
		22,         // line n
		21,         // line n
		 0,         // line n+1
		 0          // line n+1
	}
};

GXRModeObj TVEurgb60Hz480IntDf =
{
    VI_TVMODE_EURGB60_INT,      // viDisplayMode
    640,             // fbWidth
    480,             // efbHeight
    480,             // xfbHeight
    (VI_MAX_WIDTH_EURGB60 - 640)/2,         // viXOrigin
    (VI_MAX_HEIGHT_EURGB60 - 480)/2,        // viYOrigin
    640,             // viWidth
    480,             // viHeight
    VI_XFBMODE_DF,   // xFBmode
    GX_FALSE,        // field_rendering
    GX_FALSE,        // aa

    // sample points arranged in increasing Y order
	{
		{6,6},{6,6},{6,6},  // pix 0, 3 sample points, 1/12 units, 4 bits each
		{6,6},{6,6},{6,6},  // pix 1
		{6,6},{6,6},{6,6},  // pix 2
		{6,6},{6,6},{6,6}   // pix 3
	},
    // vertical filter[7], 1/64 units, 6 bits each
	{
		 8,         // line n-1
		 8,         // line n-1
		10,         // line n
		12,         // line n
		10,         // line n
		 8,         // line n+1
		 8          // line n+1
	}
};

GXRModeObj TVEurgb60Hz480IntAa =
{
    VI_TVMODE_EURGB60_INT,      // viDisplayMode
    640,             // fbWidth
    242,             // efbHeight
    480,             // xfbHeight
    (VI_MAX_WIDTH_EURGB60 - 640)/2,         // viXOrigin
    (VI_MAX_HEIGHT_EURGB60 - 480)/2,        // viYOrigin
    640,             // viWidth
    480,             // viHeight
    VI_XFBMODE_DF,   // xFBmode
    GX_FALSE,        // field_rendering
    GX_TRUE,         // aa

    // sample points arranged in increasing Y order
	{
		{3,2},{9,6},{3,10},				// pix 0, 3 sample points, 1/12 units, 4 bits each
		{3,2},{9,6},{3,10},				// pix 1
		{9,2},{3,6},{9,10},				// pix 2
		{9,2},{3,6},{9,10}				// pix 3
	},
    // vertical filter[7], 1/64 units, 6 bits each
	{
		 4,         // line n-1
		 8,         // line n-1
		12,         // line n
		16,         // line n
		12,         // line n
		 8,         // line n+1
		 4          // line n+1
	}
};

GXRModeObj TVEurgb60Hz480Prog =
{
    VI_TVMODE_EURGB60_PROG,      // viDisplayMode
    640,             // fbWidth
    480,             // efbHeight
    480,             // xfbHeight
    (VI_MAX_WIDTH_EURGB60 - 640)/2,         // viXOrigin
    (VI_MAX_HEIGHT_EURGB60 - 480)/2,        // viYOrigin
    640,             // viWidth
    480,             // viHeight
    VI_XFBMODE_SF,   // xFBmode
    GX_FALSE,        // field_rendering
    GX_FALSE,        // aa

    // sample points arranged in increasing Y order
	{
		{6,6},{6,6},{6,6},  // pix 0, 3 sample points, 1/12 units, 4 bits each
		{6,6},{6,6},{6,6},  // pix 1
		{6,6},{6,6},{6,6},  // pix 2
		{6,6},{6,6},{6,6}   // pix 3
	},
    // vertical filter[7], 1/64 units, 6 bits each
	{
		 0,         // line n-1
		 0,         // line n-1
		21,         // line n
		22,         // line n
		21,         // line n
		 0,         // line n+1
		 0          // line n+1
	}
};

GXRModeObj TVEurgb60Hz480ProgSoft =
{
    VI_TVMODE_EURGB60_PROG,      // viDisplayMode
    640,             // fbWidth
    480,             // efbHeight
    480,             // xfbHeight
    (VI_MAX_WIDTH_EURGB60 - 640)/2,         // viXOrigin
    (VI_MAX_HEIGHT_EURGB60 - 480)/2,        // viYOrigin
    640,             // viWidth
    480,             // viHeight
    VI_XFBMODE_SF,   // xFBmode
    GX_FALSE,        // field_rendering
    GX_FALSE,        // aa

    // sample points arranged in increasing Y order
	{
		{3,2},{9,6},{3,10},				// pix 0, 3 sample points, 1/12 units, 4 bits each
		{3,2},{9,6},{3,10},				// pix 1
		{9,2},{3,6},{9,10},				// pix 2
		{9,2},{3,6},{9,10}				// pix 3
	},
    // vertical filter[7], 1/64 units, 6 bits each
	{
		 4,         // line n-1
		 8,         // line n-1
		12,         // line n
		16,         // line n
		12,         // line n
		 8,         // line n+1
		 4          // line n+1
	}
};

GXRModeObj TVEurgb60Hz480ProgAa =
{
    VI_TVMODE_EURGB60_PROG,      // viDisplayMode
    640,             // fbWidth
    242,             // efbHeight
    480,             // xfbHeight
    (VI_MAX_WIDTH_EURGB60 - 640)/2,         // viXOrigin
    (VI_MAX_HEIGHT_EURGB60 - 480)/2,        // viYOrigin
    640,             // viWidth
    480,             // viHeight
    VI_XFBMODE_SF,   // xFBmode
    GX_FALSE,        // field_rendering
    GX_TRUE,        // aa

    // sample points arranged in increasing Y order
	{
		{6,6},{6,6},{6,6},  // pix 0, 3 sample points, 1/12 units, 4 bits each
		{6,6},{6,6},{6,6},  // pix 1
		{6,6},{6,6},{6,6},  // pix 2
		{6,6},{6,6},{6,6}   // pix 3
	},
    // vertical filter[7], 1/64 units, 6 bits each
	{
		 8,         // line n-1
		 8,         // line n-1
		10,         // line n
		12,         // line n
		10,         // line n
		 8,         // line n+1
		 8          // line n+1
	}
};

static const u16 taps[26] = {
	0x01F0,0x01DC,0x01AE,0x0174,0x0129,0x00DB,
	0x008E,0x0046,0x000C,0x00E2,0x00CB,0x00C0,
	0x00C4,0x00CF,0x00DE,0x00EC,0x00FC,0x0008,
	0x000F,0x0013,0x0013,0x000F,0x000C,0x0008,
	0x0001,0x0000
};

static const struct _timing {
	u8 equ;
	u16 acv;
	u16 prbOdd,prbEven;
	u16 psbOdd,psbEven;
	u8 bs1,bs2,bs3,bs4;
	u16 be1,be2,be3,be4;
	u16 nhlines,hlw;
	u8 hsy,hcs,hce,hbe640;
	u16 hbs640;
} video_timing[] = {
	{
		0x06,0x00F0,
		0x0018,0x0019,0x0003,0x0002,
		0x0C,0x0D,0x0C,0x0D,
		0x0208,0x0207,0x0208,0x0207,
		0x020D,0x01AD,
		0x40,0x47,0x69,0xA2,
		0x0175
	},
	{
		0x06,0x00F0,
		0x0018,0x0018,0x0004,0x0004,
		0x0C,0x0C,0x0C,0x0C,
		0x0208,0x0208,0x0208,0x0208,
		0x020E,0x01AD,
		0x40,0x47,0x69,0xA2,
		0x0175
	},
	{
		0x05,0x0120,
		0x0021,0x0022,0x0001,0x0000,
		0x0A,0x0B,0x0A,0x0B,
		0x026D,0x026C,0x026D,0x026C,
		0x0271,0x01B0,
		0x40,0x4B,0x6A,0xAC,
		0x017C
	},
	{
		0x05,0x011F,
		0x0021,0x0021,0x0002,0x0002,
		0x0D,0x0B,0x0D,0x0B,
		0x026B,0x026D,0x026B,0x026D,
		0x0270,0x01B0,
		0x40,0x4B,0x6A,0xAC,
		0x017C
	},
	{
		0x06,0x00F0,
		0x0018,0x0019,0x0003,0x0002,
		0x10,0x0F,0x0E,0x0D,
		0x0206,0x0205,0x0204,0x0207,
		0x020D,0x01AD,
		0x40,0x4E,0x70,0xA2,
		0x0175
	},
	{
		0x06,0x00F0,
		0x0018,0x0018,0x0004,0x0004,
		0x10,0x0E,0x10,0x0E,
		0x0206,0x0208,0x0206,0x0208,
		0x020E,0x01AD,
		0x40,0x4E,0x70,0xA2,
		0x0175
	},
	{
		0x0C,0x01E0,
		0x0030,0x0030,0x0006,0x0006,
		0x18,0x18,0x18,0x18,
		0x040E,0x040E,0x040E,0x040E,
		0x041A,0x01AD,
		0x40,0x47,0x69,0xA2,
		0x0175
	},
	{
		0x0A,0x0240,
		0x003E,0x003E,0x0006,0x0006,
		0x14,0x14,0x14,0x14,
		0x04D8,0x04D8,0x04D8,0x04D8,
		0x04E2,0x01B0,
		0x40,0x4B,0x6A,0xAC,
		0x017C
	}
};

#if defined(HW_RVL)
static u32 vdacFlagRegion;
static u32 i2cIdentFirst = 0;
static u32 i2cIdentFlag = 1;
static u32 oldTvStatus = 0x03e7;
static u32 oldDtvStatus = 0x03e7;
static vu32* const _i2cReg = (u32*)0xCD800000;
#endif

static u16 regs[60];
static u16 shdw_regs[60];
static u32 fbSet = 0;
static s16 displayOffsetH;
static s16 displayOffsetV;
static u32 currTvMode,changeMode;
static u32 shdw_changeMode,flushFlag;
static u64 changed,shdw_changed;
static vu32 retraceCount;
static const struct _timing *currTiming;
static lwpq_t video_queue;
static horVer HorVer;
static void *currentFb = NULL;
static void *nextFb = NULL;
static VIRetraceCallback preRetraceCB = NULL;
static VIRetraceCallback postRetraceCB = NULL;
static VIPositionCallback positionCB = NULL;

static vu16* const _viReg = (u16*)0xCC002000;

extern syssram* __SYS_LockSram();
extern u32 __SYS_UnlockSram(u32 write);

extern void __VIClearFramebuffer(void*,u32,u32);

extern void udelay(int us);

static __inline__ u32 cntlzd(u64 bit)
{
	u32 hi    = (u32)(bit>>32);
	u32 lo    = (u32)(bit&-1);
	u32 value = cntlzw(hi);
	if(value>=32) value += cntlzw(lo);

	return value;
}

static const struct _timing* __gettiming(u32 vimode)
{
	switch(vimode) {
		case VI_TVMODE_NTSC_INT:
			return &video_timing[0];
			break;
		case VI_TVMODE_NTSC_DS:
			return &video_timing[1];
			break;
		case VI_TVMODE_PAL_INT:
			return &video_timing[2];
			break;
		case VI_TVMODE_PAL_DS:
			return &video_timing[3];
			break;
		case VI_TVMODE_EURGB60_INT:
			return &video_timing[0];
			break;
		case VI_TVMODE_EURGB60_DS:
			return &video_timing[1];
			break;
		case VI_TVMODE_MPAL_INT:
			return &video_timing[4];
			break;
		case VI_TVMODE_MPAL_DS:
			return &video_timing[5];
			break;
		case VI_TVMODE_NTSC_PROG:
			return &video_timing[6];
			break;
		case VI_TVMODE_PAL_PROG:
			return &video_timing[7];
			break;
		case VI_TVMODE_EURGB60_PROG:
			return &video_timing[6];
			break;
		case VI_TVMODE_MPAL_PROG:
			return &video_timing[6];
			break;
		default:
			return NULL;
	}
}

#if defined(HW_RVL)
static inline void __viOpenI2C(u32 channel)
{
	u32 val = ((_i2cReg[49]&~0x8000)|0x4000);
	val |= _SHIFTL(channel,15,1);
	_i2cReg[49] = val;
}

static inline u32 __viSetSCL(u32 channel)
{
	u32 val = (_i2cReg[48]&~0x4000);
	val |= _SHIFTL(channel,14,1);
	_i2cReg[48] = val;
	return 1;
}
static inline u32 __viSetSDA(u32 channel)
{
	u32 val = (_i2cReg[48]&~0x8000);
	val |= _SHIFTL(channel,15,1);
	_i2cReg[48] = val;
	return 1;
}

static inline u32 __viGetSDA()
{
	return _SHIFTR(_i2cReg[50],15,1);
}

static inline void __viCheckI2C()
{
	__viOpenI2C(0);
	udelay(4);

	i2cIdentFlag = 0;
	if(__viGetSDA()!=0) i2cIdentFlag = 1;
}

static u32 __sendSlaveAddress(u8 addr)
{
	u32 i;

	__viSetSDA(i2cIdentFlag^1);
	udelay(2);

	__viSetSCL(0);
	for(i=0;i<8;i++) {
		if(addr&0x80) __viSetSDA(i2cIdentFlag);
		else __viSetSDA(i2cIdentFlag^1);
		udelay(2);

		__viSetSCL(1);
		udelay(2);

		__viSetSCL(0);
		addr <<= 1;
	}

	__viOpenI2C(0);
	udelay(2);

	__viSetSCL(1);
	udelay(2);

	if(i2cIdentFlag==1 && __viGetSDA()!=0) return 0;

	__viSetSDA(i2cIdentFlag^1);
	__viOpenI2C(1);
	__viSetSCL(0);

	return 1;
}
#endif

static inline void __setInterruptRegs(const struct _timing *tm)
{
	u16 hlw;

	hlw = 0;
	if(tm->nhlines%2) hlw = tm->hlw;
	regs[24] = 0x1000|((tm->nhlines/2)+1);
	regs[25] = hlw+1;
	changed |= VI_REGCHANGE(24);
	changed |= VI_REGCHANGE(25);
}

static inline void __setPicConfig(u16 fbSizeX,u32 xfbMode,u16 panPosX,u16 panSizeX,u8 *wordPerLine,u8 *std,u8 *wpl,u8 *xof)
{
	*wordPerLine = (fbSizeX+15)/16;
	*std = *wordPerLine;
	if(xfbMode==VI_XFBMODE_DF) *std <<= 1;

	*xof = panPosX%16;
	*wpl = (*xof+(panSizeX+15))/16;
	regs[36] = (*wpl<<8)|*std;
	changed |= VI_REGCHANGE(36);
}

static inline void __setBBIntervalRegs(const struct _timing *tm)
{
	regs[10] = (tm->be3<<5)|tm->bs3;
	regs[11] = (tm->be1<<5)|tm->bs1;
	changed |= VI_REGCHANGE(10);
	changed |= VI_REGCHANGE(11);

	regs[12] = (tm->be4<<5)|tm->bs4;
	regs[13] = (tm->be2<<5)|tm->bs2;
	changed |= VI_REGCHANGE(12);
	changed |= VI_REGCHANGE(13);
}

static void __setScalingRegs(u16 panSizeX,u16 dispSizeX,s32 threeD)
{
	if(threeD) panSizeX = _SHIFTL(panSizeX,1,16);
	if(panSizeX<dispSizeX) {
		regs[37] = 0x1000|((dispSizeX+(_SHIFTL(panSizeX,8,16)-1))/dispSizeX);
		regs[56] = panSizeX;
		changed |= VI_REGCHANGE(37);
		changed |= VI_REGCHANGE(56);
	} else {
		regs[37] = 0x100;
		changed |= VI_REGCHANGE(37);
	}
}

static inline void __calcFbbs(u32 bufAddr,u16 panPosX,u16 panPosY,u8 wordperline,u32 xfbMode,u16 dispPosY,u32 *tfbb,u32 *bfbb)
{
	u32 bytesPerLine,tmp;

	panPosX &= 0xfff0;
	bytesPerLine = (wordperline<<5)&0x1fe0;
	*tfbb = bufAddr+((panPosX<<5)+(panPosY*bytesPerLine));
	*bfbb = *tfbb;
	if(xfbMode==VI_XFBMODE_DF) *bfbb = *tfbb+bytesPerLine;

	if(dispPosY%2) {
		tmp = *tfbb;
		*tfbb = *bfbb;
		*bfbb = tmp;
	}

	*tfbb = MEM_VIRTUAL_TO_PHYSICAL(*tfbb);
	*bfbb = MEM_VIRTUAL_TO_PHYSICAL(*bfbb);
}

static inline void __setFbbRegs(struct _horVer *horVer,u32 *tfbb,u32 *bfbb,u32 *rtfbb,u32 *rbfbb)
{
	u32 flag;
	__calcFbbs((u32)horVer->bufAddr,horVer->panPosX,horVer->adjustedPanPosY,horVer->wordPerLine,horVer->fbMode,horVer->adjustedDispPosY,tfbb,bfbb);
	if(horVer->threeD) __calcFbbs((u32)horVer->rbufAddr,horVer->panPosX,horVer->adjustedPanPosY,horVer->wordPerLine,horVer->fbMode,horVer->adjustedDispPosY,rtfbb,rbfbb);

	flag = 1;
	if((*tfbb)<0x01000000 && (*bfbb)<0x01000000
		&& (*rtfbb)<0x01000000 && (*rbfbb)<0x01000000) flag = 0;

	if(flag) {
		*tfbb >>= 5;
		*bfbb >>= 5;
		*rtfbb >>= 5;
		*rbfbb >>= 5;
	}

	regs[14] = _SHIFTL(flag,12,1)|_SHIFTL(horVer->xof,8,4)|_SHIFTR(*tfbb,16,8);
	regs[15] = *tfbb&0xffff;
	changed |= VI_REGCHANGE(14);
	changed |= VI_REGCHANGE(15);

	regs[18] = _SHIFTR(*bfbb,16,8);
	regs[19] = *bfbb&0xffff;
	changed |= VI_REGCHANGE(18);
	changed |= VI_REGCHANGE(19);

	if(horVer->threeD) {
		regs[16] = _SHIFTR(*rtfbb,16,8);
		regs[17] = *rtfbb&0xffff;
		changed |= VI_REGCHANGE(16);
		changed |= VI_REGCHANGE(17);

		regs[20] = _SHIFTR(*rbfbb,16,8);
		regs[21] = *rbfbb&0xffff;
		changed |= VI_REGCHANGE(20);
		changed |= VI_REGCHANGE(21);
	}
}

static inline void __setHorizontalRegs(const struct _timing *tm,u16 dispPosX,u16 dispSizeX)
{
	u32 val1,val2;

	regs[2] = (tm->hcs<<8)|tm->hce;
	regs[3] = tm->hlw;
	changed |= VI_REGCHANGE(2);
	changed |= VI_REGCHANGE(3);

	val1 = (tm->hbe640+dispPosX-40)&0x01ff;
	val2 = (tm->hbs640+dispPosX+40)-(720-dispSizeX);
	regs[4] = (val1>>9)|(val2<<1);
	regs[5] = (val1<<7)|tm->hsy;
	changed |= VI_REGCHANGE(4);
	changed |= VI_REGCHANGE(5);
}

static inline void __setVerticalRegs(u16 dispPosY,u16 dispSizeY,u8 equ,u16 acv,u16 prbOdd,u16 prbEven,u16 psbOdd,u16 psbEven,s32 black)
{
	u16 tmp;
	u32 div1,div2;
	u32 psb,prb;
	u32 psbodd,prbodd;
	u32 psbeven,prbeven;

	div1 = 2;
	div2 = 1;
	if(equ>=10) {
		div1 = 1;
		div2 = 2;
	}

	prb = div2*dispPosY;
	psb = div2*(((acv*div1)-dispSizeY)-dispPosY);
	if(dispPosY%2) {
		prbodd = prbEven+prb;
		psbodd = psbEven+psb;
		prbeven = prbOdd+prb;
		psbeven = psbOdd+psb;
	} else {
		prbodd = prbOdd+prb;
		psbodd = psbOdd+psb;
		prbeven = prbEven+prb;
		psbeven = psbEven+psb;
	}

	tmp = dispSizeY/div1;
	if(black) {
		prbodd += ((tmp<<1)-2);
		prbeven += ((tmp<<1)-2);
		psbodd += 2;
		psbeven += 2;
		tmp = 0;
	}

	regs[0] = ((tmp<<4)&~0x0f)|equ;
	changed |= VI_REGCHANGE(0);

	regs[6] = psbodd;
	regs[7] = prbodd;
	changed |= VI_REGCHANGE(6);
	changed |= VI_REGCHANGE(7);

	regs[8] = psbeven;
	regs[9] = prbeven;
	changed |= VI_REGCHANGE(8);
	changed |= VI_REGCHANGE(9);
}

static inline void __adjustPosition(u16 acv)
{
	u32 fact,field;
	s16 dispPosX,dispPosY;
	s16 dispSizeY,maxDispSizeY;

	dispPosX = (HorVer.dispPosX+displayOffsetH);
	if(dispPosX<=(720-HorVer.dispSizeX)) {
		if(dispPosX>=0) HorVer.adjustedDispPosX = dispPosX;
		else HorVer.adjustedDispPosX = 0;
	} else HorVer.adjustedDispPosX = (720-HorVer.dispSizeX);

	fact = 1;
	if(HorVer.fbMode==VI_XFBMODE_SF) fact = 2;

	field = HorVer.dispPosY&0x0001;
	dispPosY = HorVer.dispPosY+displayOffsetV;
	if(dispPosY>field) HorVer.adjustedDispPosY = dispPosY;
	else HorVer.adjustedDispPosY = field;

	dispSizeY = HorVer.dispPosY+HorVer.dispSizeY+displayOffsetV;
	maxDispSizeY = ((acv<<1)-field);
	if(dispSizeY>maxDispSizeY) dispSizeY -= (acv<<1)-field;
	else dispSizeY = 0;

	dispPosY = HorVer.dispPosY+displayOffsetV;
	if(dispPosY<field) dispPosY -= field;
	else dispPosY = 0;
	HorVer.adjustedDispSizeY = HorVer.dispSizeY+dispPosY-dispSizeY;

	dispPosY = HorVer.dispPosY+displayOffsetV;
	if(dispPosY<field) dispPosY -= field;
	else dispPosY = 0;
	HorVer.adjustedPanPosY = HorVer.panPosY-(dispPosY/fact);

	dispSizeY = HorVer.dispPosY+HorVer.dispSizeY+displayOffsetV;
	if(dispSizeY>maxDispSizeY) dispSizeY -= maxDispSizeY;
	else dispSizeY = 0;

	dispPosY = HorVer.dispPosY+displayOffsetV;
	if(dispPosY<field) dispPosY -= field;
	else dispPosY = 0;
	HorVer.adjustedPanSizeY = HorVer.panSizeY+(dispPosY/fact)-(dispSizeY/fact);
}

static inline void __importAdjustingValues()
{
#ifdef HW_DOL
	syssram *sram;

	sram = __SYS_LockSram();
	displayOffsetH = sram->display_offsetH;
	__SYS_UnlockSram(0);
#else
	s8 offset;
	if ( CONF_GetDisplayOffsetH(&offset) == 0 ) {
		displayOffsetH = offset;
	} else {
		displayOffsetH = 0;
	}
#endif
	displayOffsetV = 0;
}

static void __VIInit(u32 vimode)
{
	u32 cnt;
	u32 vi_mode,interlace,progressive;
	const struct _timing *cur_timing = NULL;

	vi_mode = ((vimode>>2)&0x07);
	interlace = (vimode&0x01);
	progressive = (vimode&0x02);

	cur_timing = __gettiming(vimode);

	//reset the interface
	cnt = 0;
	_viReg[1] = 0x02;
	while(cnt<1000) cnt++;
	_viReg[1] = 0x00;

	// now begin to setup the interface
	_viReg[2] = ((cur_timing->hcs<<8)|cur_timing->hce);		//set HCS & HCE
	_viReg[3] = cur_timing->hlw;							//set Half Line Width

	_viReg[4] = (cur_timing->hbs640<<1);					//set HBS640
	_viReg[5] = ((cur_timing->hbe640<<7)|cur_timing->hsy);	//set HBE640 & HSY

	_viReg[0] = cur_timing->equ;

	_viReg[6] = (cur_timing->psbOdd+2);							//set PSB odd field
	_viReg[7] = (cur_timing->prbOdd+((cur_timing->acv<<1)-2));	//set PRB odd field

	_viReg[8] = (cur_timing->psbEven+2);						//set PSB even field
	_viReg[9] = (cur_timing->prbEven+((cur_timing->acv<<1)-2));	//set PRB even field

	_viReg[10] = ((cur_timing->be3<<5)|cur_timing->bs3);		//set BE3 & BS3
	_viReg[11] = ((cur_timing->be1<<5)|cur_timing->bs1);		//set BE1 & BS1

	_viReg[12] = ((cur_timing->be4<<5)|cur_timing->bs4);		//set BE4 & BS4
	_viReg[13] = ((cur_timing->be2<<5)|cur_timing->bs2);		//set BE2 & BS2

	_viReg[24] = (0x1000|((cur_timing->nhlines/2)+1));
	_viReg[25] = (cur_timing->hlw+1);

	_viReg[26] = 0x1001;		//set DI1
	_viReg[27] = 0x0001;		//set DI1
	_viReg[36] = 0x2828;		//set HSR

	if(vi_mode<VI_PAL && vi_mode>=VI_DEBUG_PAL) vi_mode = VI_NTSC;
	if(progressive){
		_viReg[1] = ((vi_mode<<8)|0x0005);		//set MODE & INT & enable
		_viReg[54] = 0x0001;
	} else {
		_viReg[1] = ((vi_mode<<8)|(interlace<<2)|0x0001);
		_viReg[54] = 0x0000;
	}
}

#if defined(HW_RVL)
static u32 __VISendI2CData(u8 addr,void *val,u32 len)
{
	u8 c;
	s32 i,j;
	u32 level,ret;

	if(i2cIdentFirst==0) {
		__viCheckI2C();
		i2cIdentFirst = 1;
	}

	_CPU_ISR_Disable(level);

	__viOpenI2C(1);
	__viSetSCL(1);

	__viSetSDA(i2cIdentFlag);
	udelay(4);

	ret = __sendSlaveAddress(addr);
	if(ret==0) {
		_CPU_ISR_Restore(level);
		return 0;
	}

	__viOpenI2C(1);
	for(i=0;i<len;i++) {
		c = ((u8*)val)[i];
		for(j=0;j<8;j++) {
			if(c&0x80) __viSetSDA(i2cIdentFlag);
			else __viSetSDA(i2cIdentFlag^1);
			udelay(2);

			__viSetSCL(1);
			udelay(2);
			__viSetSCL(0);

			c <<= 1;
		}
		__viOpenI2C(0);
		udelay(2);
		__viSetSCL(1);
		udelay(2);

		if(i2cIdentFlag==1 && __viGetSDA()!=0) {
			_CPU_ISR_Restore(level);
			return 0;
		}

		__viSetSDA(i2cIdentFlag^1);
		__viOpenI2C(1);
		__viSetSCL(0);
	}

	__viOpenI2C(1);
	__viSetSDA(i2cIdentFlag^1);
	udelay(2);
	__viSetSDA(i2cIdentFlag);

	_CPU_ISR_Restore(level);
	return 1;
}

static void __VIWriteI2CRegister8(u8 reg, u8 data)
{
	u8 buf[2];
	buf[0] = reg;
	buf[1] = data;
	__VISendI2CData(0xe0,buf,2);
	udelay(2);
}

static void __VIWriteI2CRegister16(u8 reg, u16 data)
{
	u8 buf[3];
	buf[0] = reg;
	buf[1] = data >> 8;
	buf[2] = data & 0xFF;
	__VISendI2CData(0xe0,buf,3);
	udelay(2);
}

static void __VIWriteI2CRegister32(u8 reg, u32 data)
{
	u8 buf[5];
	buf[0] = reg;
	buf[1] = data >> 24;
	buf[2] = (data >> 16) & 0xFF;
	buf[3] = (data >> 8) & 0xFF;
	buf[4] = data & 0xFF;
	__VISendI2CData(0xe0,buf,5);
	udelay(2);
}

static void __VIWriteI2CRegisterBuf(u8 reg, int size, u8 *data)
{
	u8 buf[0x100];
	buf[0] = reg;
	memcpy(&buf[1], data, size);
	__VISendI2CData(0xe0,buf,size+1);
	udelay(2);
}

static void __VISetYUVSEL(u8 dtvstatus)
{
	if(currTvMode==VI_NTSC) vdacFlagRegion = 0x0000;
	else if(currTvMode==VI_PAL || currTvMode==VI_EURGB60) vdacFlagRegion = 0x0002;
	else if(currTvMode==VI_MPAL) vdacFlagRegion = 0x0001;
	else vdacFlagRegion = 0x0000;

	__VIWriteI2CRegister8(0x01, _SHIFTL(dtvstatus,5,3)|(vdacFlagRegion&0x1f));
}

static void __VISetFilterEURGB60(u8 enable)
{
	__VIWriteI2CRegister8(0x6e, enable);
}

static void __VISetupEncoder(void)
{
	u8 macrobuf[0x1a];

	u8 gamma[0x21] = {
		0x10, 0x00, 0x10, 0x00, 0x10, 0x00, 0x10, 0x00,
		0x10, 0x00, 0x10, 0x00, 0x10, 0x20, 0x40, 0x60,
		0x80, 0xa0, 0xeb, 0x10, 0x00, 0x20, 0x00, 0x40,
		0x00, 0x60, 0x00, 0x80, 0x00, 0xa0, 0x00, 0xeb,
		0x00
	};

	u8 dtv, tv;

	tv = VIDEO_GetCurrentTvMode();
	dtv = (_viReg[55]&0x01);
	oldDtvStatus = dtv;

	// SetRevolutionModeSimple

	memset(macrobuf, 0, 0x1a);

	__VIWriteI2CRegister8(0x6a, 1);
	__VIWriteI2CRegister8(0x65, 1);
	__VISetYUVSEL(dtv);
	__VIWriteI2CRegister8(0x00, 0);
	__VIWriteI2CRegister16(0x71, 0x8e8e);
	__VIWriteI2CRegister8(0x02, 7);
	__VIWriteI2CRegister16(0x05, 0x0000);
	__VIWriteI2CRegister16(0x08, 0x0000);
	__VIWriteI2CRegister32(0x7A, 0x00000000);

	// Macrovision crap
	__VIWriteI2CRegisterBuf(0x40, sizeof(macrobuf), macrobuf);

	// Sometimes 1 in RGB mode? (reg 1 == 3)
	__VIWriteI2CRegister8(0x0A, 0);

	__VIWriteI2CRegister8(0x03, 1);

	__VIWriteI2CRegisterBuf(0x10, sizeof(gamma), gamma);

	__VIWriteI2CRegister8(0x04, 1);

	if(tv==VI_EURGB60) __VISetFilterEURGB60(1);
	else __VISetFilterEURGB60(0);
	oldTvStatus = tv;

}
#endif

static inline void __getCurrentDisplayPosition(u32 *px,u32 *py)
{
	u32 hpos = 0;
	u32 vpos = 0;
	u32 vpos_old;

	vpos = (_viReg[22]&0x7ff);
	do {
		vpos_old = vpos;
		hpos = (_viReg[23]&0x7ff);
		vpos = (_viReg[22]&0x7ff);
	} while(vpos_old!=vpos);
	*px = hpos;
	*py = vpos;
}

static inline u32 __getCurrentHalfLine()
{
	u32 vpos = 0;
	u32 hpos = 0;

	__getCurrentDisplayPosition(&hpos,&vpos);

	hpos--;
	vpos--;
	vpos <<= 1;

	return vpos+(hpos/currTiming->hlw);
}

static inline u32 __getCurrentFieldEvenOdd()
{
	u32 hline;

	hline = __getCurrentHalfLine();
	if(hline<currTiming->nhlines) return 1;

	return 0;
}

static inline u32 __VISetRegs()
{
	u32 val;
	u64 mask;

	if(shdw_changeMode==1){
		if(!__getCurrentFieldEvenOdd()) return 0;
	}

	while(shdw_changed) {
		val = cntlzd(shdw_changed);
		_viReg[val] = shdw_regs[val];
		mask = VI_REGCHANGE(val);
		shdw_changed &= ~mask;
	}
	shdw_changeMode = 0;
	currTiming = HorVer.timing;
	currTvMode = HorVer.tv;

	currentFb = nextFb;

	return 1;
}

static void __VIDisplayPositionToXY(s32 xpos,s32 ypos,s32 *px,s32 *py)
{
	u32 hpos,vpos;
	u32 hline,val;

	hpos = (xpos-1);
	vpos = (ypos-1);
	hline = ((vpos<<1)+(hpos/currTiming->hlw));

	*px = (s32)hpos;
	if(HorVer.nonInter==0x0000) {
		if(hline<currTiming->nhlines) {
			val = currTiming->prbOdd+(currTiming->equ*3);
			if(hline>=val) {
				val = (currTiming->nhlines-currTiming->psbOdd);
				if(hline<val) {
					*py = (s32)(((hline-(currTiming->equ*3))-currTiming->prbOdd)&~0x01);
				} else
					*py = -1;
			} else
				*py = -1;
		} else {
			hline -= currTiming->psbOdd;
			val = (currTiming->prbEven+(currTiming->equ*3));
			if(hline>=val) {
				val = (currTiming->nhlines-currTiming->psbEven);
				if(hline<val) {
					*py = (s32)((((hline-(currTiming->equ*3))-currTiming->prbEven)&~0x01)+1);
				} else
					*py = -1;
			} else
				*py = -1;
		}
	} else if(HorVer.nonInter==0x0001) {
		if(hline>=currTiming->nhlines) hline -= currTiming->nhlines;

		val = (currTiming->prbOdd+(currTiming->equ*3));
		if(hline>=val) {
			val = (currTiming->nhlines-currTiming->psbOdd);
			if(hline<val) {
				*py = (s32)(((hline-(currTiming->equ*3))-currTiming->prbOdd)&~0x01);
			} else
				*py = -1;
		} else
			*py = -1;
	} else if(HorVer.nonInter==0x0002) {
		if(hline<currTiming->nhlines) {
			val = currTiming->prbOdd+(currTiming->equ*3);
			if(hline>=val) {
				val = (currTiming->nhlines-currTiming->psbOdd);
				if(hline<val) {
					*py = (s32)((hline-(currTiming->equ*3))-currTiming->prbOdd);
				} else
					*py = -1;
			} else
				*py = -1;
		} else {
			hline -= currTiming->psbOdd;
			val = (currTiming->prbEven+(currTiming->equ*3));
			if(hline>=val) {
				val = (currTiming->nhlines-currTiming->psbEven);
				if(hline<val) {
					*py = (s32)(((hline-(currTiming->equ*3))-currTiming->prbEven)&~0x01);
				} else
					*py = -1;
			} else
				*py = -1;
		}
	}
}

static inline void __VIGetCurrentPosition(s32 *px,s32 *py)
{
	s32 xpos,ypos;

	__getCurrentDisplayPosition((u32*)&xpos,(u32*)&ypos);
	__VIDisplayPositionToXY(xpos,ypos,px,py);
}

static void __VIRetraceHandler(u32 nIrq,void *pCtx)
{
#if defined(HW_RVL)
	u8 dtv, tv;
#endif
	u32 ret = 0;
	u32 intr;
	s32 xpos,ypos;

	intr = _viReg[24];
	if(intr&0x8000) {
		_viReg[24] = intr&~0x8000;
		ret |= 0x01;
	}

	intr = _viReg[26];
	if(intr&0x8000) {
		_viReg[26] = intr&~0x8000;
		ret |= 0x02;
	}

	intr = _viReg[28];
	if(intr&0x8000) {
		_viReg[28] = intr&~0x8000;
		ret |= 0x04;
	}

	intr = _viReg[30];
	if(intr&0x8000) {
		_viReg[30] = intr&~0x8000;
		ret |= 0x08;
	}

	intr = _viReg[30];
	if(ret&0x04 || ret&0x08) {
		if(positionCB!=NULL) {
			__VIGetCurrentPosition(&xpos,&ypos);
			positionCB(xpos,ypos);
		}
	}

	retraceCount++;
	if(preRetraceCB)
		preRetraceCB(retraceCount);

	if(flushFlag) {
		if(__VISetRegs()) {
			flushFlag = 0;
			SI_RefreshSamplingRate();
		}
	}
#if defined(HW_RVL)
	tv = VIDEO_GetCurrentTvMode();
	dtv = (_viReg[55]&0x01);
	if(dtv!=oldDtvStatus || tv!=oldTvStatus) __VISetYUVSEL(dtv);
	oldDtvStatus = dtv;

	if(tv!=oldTvStatus) {
		if(tv==VI_EURGB60) __VISetFilterEURGB60(1);
		else __VISetFilterEURGB60(0);
	}
	oldTvStatus = tv;
#endif
	if(postRetraceCB)
		postRetraceCB(retraceCount);

	LWP_ThreadBroadcast(video_queue);
}

void* VIDEO_GetNextFramebuffer()
{
	return nextFb;
}

void* VIDEO_GetCurrentFramebuffer()
{
	return currentFb;
}

void VIDEO_Init()
{
	u32 level,vimode = 0;

	_CPU_ISR_Disable(level);

	if(!(_viReg[1]&0x0001))
		__VIInit(VI_TVMODE_NTSC_INT);

	retraceCount = 0;
	changed = 0;
	shdw_changed = 0;
	shdw_changeMode = 0;
	flushFlag = 0;

	_viReg[38] = ((taps[1]>>6)|(taps[2]<<4));
	_viReg[39] = (taps[0]|_SHIFTL(taps[1],10,6));
	_viReg[40] = ((taps[4]>>6)|(taps[5]<<4));
	_viReg[41] = (taps[3]|_SHIFTL(taps[4],10,6));
	_viReg[42] = ((taps[7]>>6)|(taps[8]<<4));
	_viReg[43] = (taps[6]|_SHIFTL(taps[7],10,6));
	_viReg[44] = (taps[11]|(taps[12]<<8));
	_viReg[45] = (taps[9]|(taps[10]<<8));
	_viReg[46] = (taps[15]|(taps[16]<<8));
	_viReg[47] = (taps[13]|(taps[14]<<8));
	_viReg[48] = (taps[19]|(taps[20]<<8));
	_viReg[49] = (taps[17]|(taps[18]<<8));
	_viReg[50] = (taps[23]|(taps[24]<<8));
	_viReg[51] = (taps[21]|(taps[22]<<8));
	_viReg[56] = 640;

	__importAdjustingValues();

	HorVer.nonInter = _SHIFTR(_viReg[1],2,1);
	HorVer.tv = _SHIFTR(_viReg[1],8,2);

	vimode = HorVer.nonInter;
	if(HorVer.tv!=VI_DEBUG) vimode += (HorVer.tv<<2);
	currTiming = __gettiming(vimode);
	currTvMode = HorVer.tv;

	regs[1] = _viReg[1];
	HorVer.timing = currTiming;
	HorVer.dispSizeX = 640;
	HorVer.dispSizeY = currTiming->acv<<1;
	HorVer.dispPosX = (VI_MAX_WIDTH_NTSC-HorVer.dispSizeX)/2;
	HorVer.dispPosY = 0;

	__adjustPosition(currTiming->acv);

	HorVer.fbSizeX = 640;
	HorVer.fbSizeY = currTiming->acv<<1;
	HorVer.panPosX = 0;
	HorVer.panPosY = 0;
	HorVer.panSizeX = 640;
	HorVer.panSizeY = currTiming->acv<<1;
	HorVer.fbMode = VI_XFBMODE_SF;
	HorVer.wordPerLine = 40;
	HorVer.std = 40;
	HorVer.wpl = 40;
	HorVer.xof = 0;
	HorVer.black = 1;
	HorVer.threeD = 0;
	HorVer.bfbb = 0;
	HorVer.tfbb = 0;
	HorVer.rbfbb = 0;
	HorVer.rtfbb = 0;

	_viReg[24] &= ~0x8000;
	_viReg[26] &= ~0x8000;

	preRetraceCB = NULL;
	postRetraceCB = NULL;

	LWP_InitQueue(&video_queue);

	IRQ_Request(IRQ_PI_VI,__VIRetraceHandler,NULL);
	__UnmaskIrq(IRQMASK(IRQ_PI_VI));
#if defined(HW_RVL)
	__VISetupEncoder();
#endif
	_CPU_ISR_Restore(level);
}

void VIDEO_Configure(GXRModeObj *rmode)
{
	u16 dcr;
	u32 nonint,vimode,level;
	const struct _timing *curtiming;
	_CPU_ISR_Disable(level);
	nonint = (rmode->viTVMode&0x0003);
	if(nonint!=HorVer.nonInter) {
		changeMode = 1;
		HorVer.nonInter = nonint;
	}
	HorVer.tv = _SHIFTR(rmode->viTVMode,2,3);
	HorVer.dispPosX = rmode->viXOrigin;
	HorVer.dispPosY = rmode->viYOrigin;
	if(HorVer.nonInter==VI_NON_INTERLACE) HorVer.dispPosY = HorVer.dispPosY<<1;

	HorVer.dispSizeX = rmode->viWidth;
	HorVer.fbSizeX = rmode->fbWidth;
	HorVer.fbSizeY = rmode->xfbHeight;
	HorVer.fbMode = rmode->xfbMode;
	HorVer.panSizeX = HorVer.fbSizeX;
	HorVer.panSizeY = HorVer.fbSizeY;
	HorVer.panPosX = 0;
	HorVer.panPosY = 0;

	if(HorVer.nonInter==VI_PROGRESSIVE || HorVer.nonInter==(VI_NON_INTERLACE|VI_PROGRESSIVE)) HorVer.dispSizeY = HorVer.panSizeY;
	else if(HorVer.fbMode==VI_XFBMODE_SF) HorVer.dispSizeY = HorVer.panSizeY<<1;
	else HorVer.dispSizeY = HorVer.panSizeY;

	if(HorVer.nonInter==(VI_NON_INTERLACE|VI_PROGRESSIVE)) HorVer.threeD = 1;
	else HorVer.threeD = 0;

	vimode = VI_TVMODE(HorVer.tv,HorVer.nonInter);
	curtiming = __gettiming(vimode);
	HorVer.timing = curtiming;

	__adjustPosition(curtiming->acv);
	__setInterruptRegs(curtiming);

	dcr = regs[1]&~0x030c;
	dcr |= _SHIFTL(HorVer.threeD,3,1);
	if(HorVer.nonInter==VI_PROGRESSIVE || HorVer.nonInter==(VI_NON_INTERLACE|VI_PROGRESSIVE)) dcr |= 0x0004;
	else dcr |= _SHIFTL(HorVer.nonInter,2,1);
	if(!(HorVer.tv==VI_EURGB60)) dcr |= _SHIFTL(HorVer.tv,8,2);
	regs[1] = dcr;
	changed |= VI_REGCHANGE(1);

	regs[54] &= ~0x0001;
	if(HorVer.nonInter==VI_PROGRESSIVE || HorVer.nonInter==(VI_NON_INTERLACE|VI_PROGRESSIVE)) regs[54] |= 0x0001;
	changed |= VI_REGCHANGE(54);

	__setScalingRegs(HorVer.panSizeX,HorVer.dispSizeX,HorVer.threeD);
	__setHorizontalRegs(curtiming,HorVer.adjustedDispPosX,HorVer.dispSizeX);
	__setBBIntervalRegs(curtiming);
	__setPicConfig(HorVer.fbSizeX,HorVer.fbMode,HorVer.panPosX,HorVer.panSizeX,&HorVer.wordPerLine,&HorVer.std,&HorVer.wpl,&HorVer.xof);

	if(fbSet) __setFbbRegs(&HorVer,&HorVer.tfbb,&HorVer.bfbb,&HorVer.rtfbb,&HorVer.rbfbb);

	__setVerticalRegs(HorVer.adjustedDispPosY,HorVer.adjustedDispSizeY,curtiming->equ,curtiming->acv,curtiming->prbOdd,curtiming->prbEven,curtiming->psbOdd,curtiming->psbEven,HorVer.black);
	_CPU_ISR_Restore(level);
}

void VIDEO_WaitVSync(void)
{
	u32 level;
	u32 retcnt;

	_CPU_ISR_Disable(level);
	retcnt = retraceCount;
	do {
		LWP_ThreadSleep(video_queue);
	} while(retraceCount==retcnt);
	_CPU_ISR_Restore(level);
}

void VIDEO_SetFramebuffer(void *fb)
{
	u32 level;

	_CPU_ISR_Disable(level);
	fbSet = 1;
	HorVer.bufAddr = fb;
	__setFbbRegs(&HorVer,&HorVer.tfbb,&HorVer.bfbb,&HorVer.rtfbb,&HorVer.rbfbb);
	_viReg[14] = regs[14];
	_viReg[15] = regs[15];

	_viReg[18] = regs[18];
	_viReg[19] = regs[19];

	if(HorVer.threeD) {
		_viReg[16] = regs[16];
		_viReg[17] = regs[17];

		_viReg[20] = regs[20];
		_viReg[21] = regs[21];
	}
	_CPU_ISR_Restore(level);
}

void VIDEO_SetNextFramebuffer(void *fb)
{
	u32 level;
	_CPU_ISR_Disable(level);
	fbSet = 1;
	HorVer.bufAddr = fb;
	__setFbbRegs(&HorVer,&HorVer.tfbb,&HorVer.bfbb,&HorVer.rtfbb,&HorVer.rbfbb);
	_CPU_ISR_Restore(level);
}

void VIDEO_SetNextRightFramebuffer(void *fb)
{
	u32 level;

	_CPU_ISR_Disable(level);
	fbSet = 1;
	HorVer.rbufAddr = fb;
	__setFbbRegs(&HorVer,&HorVer.tfbb,&HorVer.bfbb,&HorVer.rtfbb,&HorVer.rbfbb);
	_CPU_ISR_Restore(level);
}

void VIDEO_Flush()
{
	u32 level;
	u32 val;
	u64 mask;

	_CPU_ISR_Disable(level);
	shdw_changeMode |= changeMode;
	changeMode = 0;

	shdw_changed |= changed;
	while(changed) {
		val = cntlzd(changed);
		shdw_regs[val] = regs[val];
		mask = VI_REGCHANGE(val);
		changed &= ~mask;
	}
	flushFlag = 1;
	nextFb = HorVer.bufAddr;
	_CPU_ISR_Restore(level);
}

void VIDEO_SetBlack(bool black)
{
	u32 level;
	const struct _timing *curtiming;

	_CPU_ISR_Disable(level);
	HorVer.black = black;
	curtiming = HorVer.timing;
	__setVerticalRegs(HorVer.adjustedDispPosY,HorVer.dispSizeY,curtiming->equ,curtiming->acv,curtiming->prbOdd,curtiming->prbEven,curtiming->psbOdd,curtiming->psbEven,HorVer.black);
	_CPU_ISR_Restore(level);
}

u32 VIDEO_GetNextField()
{
	u32 level,nextfield;

	_CPU_ISR_Disable(level);
	nextfield = __getCurrentFieldEvenOdd()^1;		//we've to swap the result because it shows us only the current field,so we've the next field either even or odd
	_CPU_ISR_Restore(level);

	return nextfield^(HorVer.adjustedDispPosY&0x0001);	//if the YOrigin is at an odd position we've to swap it again, since the Fb registers are set swapped if this rule applies
}

u32 VIDEO_GetCurrentTvMode()
{
	u32 mode;
	u32 level;
	u32 tv;

	_CPU_ISR_Disable(level);
	mode = currTvMode;

	if(mode==VI_DEBUG) tv = VI_NTSC;
	else if(mode==VI_EURGB60) tv = VI_EURGB60;
	else if(mode==VI_MPAL) tv = VI_MPAL;
	else if(mode==VI_NTSC) tv = VI_NTSC;
	else tv = VI_PAL;
	_CPU_ISR_Restore(level);

	return tv;
}

GXRModeObj * VIDEO_GetPreferredMode(GXRModeObj *mode)
{

GXRModeObj *rmode = NULL;

#if defined(HW_RVL)
	u32 tvmode = CONF_GetVideo();
	if (CONF_GetProgressiveScan() > 0 && VIDEO_HaveComponentCable()) {
		switch (tvmode) {
			case CONF_VIDEO_NTSC:
				rmode = &TVNtsc480Prog;
				break;
			case CONF_VIDEO_PAL:
				if (CONF_GetEuRGB60() > 0)
					rmode = &TVEurgb60Hz480Prog;
				else rmode = &TVPal576ProgScale;
				break;
			case CONF_VIDEO_MPAL:
				rmode = &TVMpal480Prog;
				break;
			default:
				rmode = &TVNtsc480Prog;
		}
	} else {
		switch (tvmode) {
			case CONF_VIDEO_NTSC:
				rmode = &TVNtsc480IntDf;
				break;
			case CONF_VIDEO_PAL:
				if (CONF_GetEuRGB60() > 0)
					rmode = &TVEurgb60Hz480IntDf;
				else rmode = &TVPal576IntDfScale;
				break;
			case CONF_VIDEO_MPAL:
				rmode = &TVMpal480IntDf;
				break;
			default:
				rmode = &TVNtsc480IntDf;
		}
	}
#else
	u32 tvmode = VIDEO_GetCurrentTvMode();
	if (VIDEO_HaveComponentCable()) {
		switch (tvmode) {
			case VI_NTSC:
				rmode = &TVNtsc480Prog;
				break;
			case VI_PAL:
				rmode = &TVPal576ProgScale;
				break;
			case VI_MPAL:
				rmode = &TVMpal480Prog;
				break;
			case VI_EURGB60:
				rmode = &TVEurgb60Hz480Prog;
				break;
		}
	} else {
		switch (tvmode) {
			case VI_NTSC:
				rmode = &TVNtsc480IntDf;
				break;
			case VI_PAL:
				rmode = &TVPal576IntDfScale;
				break;
			case VI_MPAL:
				rmode = &TVMpal480IntDf;
				break;
			case VI_EURGB60:
				rmode = &TVEurgb60Hz480IntDf;
				break;
		}
	}
#endif

	if ( NULL != mode ) {
		memcpy( mode, rmode, sizeof(GXRModeObj));
	} else {
		mode = rmode;
	}

	return mode;

}

u32 VIDEO_GetCurrentLine()
{
	u32 level,curr_hl = 0;

	_CPU_ISR_Disable(level);
	curr_hl = __getCurrentHalfLine();
	_CPU_ISR_Restore(level);

	if(curr_hl>=currTiming->nhlines) curr_hl -=currTiming->nhlines;
	curr_hl >>= 1;

	return curr_hl;
}

VIRetraceCallback VIDEO_SetPreRetraceCallback(VIRetraceCallback callback)
{
	u32 level = 0;
	VIRetraceCallback ret = preRetraceCB;
	_CPU_ISR_Disable(level);
	preRetraceCB = callback;
	_CPU_ISR_Restore(level);
	return ret;
}

VIRetraceCallback VIDEO_SetPostRetraceCallback(VIRetraceCallback callback)
{
	u32 level = 0;
	VIRetraceCallback ret = postRetraceCB;
	_CPU_ISR_Disable(level);
	postRetraceCB = callback;
	_CPU_ISR_Restore(level);
	return ret;
}

u32 VIDEO_GetFrameBufferSize(GXRModeObj *rmode) {
	u16 w, h;

	w = VIDEO_PadFramebufferWidth(rmode->fbWidth);
	h = rmode->xfbHeight;

	if (rmode->aa)
		h += 4;

	return w * h * VI_DISPLAY_PIX_SZ;
}

void VIDEO_ClearFrameBuffer(GXRModeObj *rmode,void *fb,u32 color)
{
	__VIClearFramebuffer(fb, VIDEO_GetFrameBufferSize(rmode), color);
}

u32 VIDEO_HaveComponentCable(void)
{
	return (_viReg[55]&0x01);
}
