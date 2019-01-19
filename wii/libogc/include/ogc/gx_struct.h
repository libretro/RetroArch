/*-------------------------------------------------------------

gx_struct.h -- support header

Copyright (C) 2004
Michael Wiedenbauer (shagkur)
Dave Murphy (WinterMute)

This software is provided 'as-is', without any express or implied
warranty.  In no event will the authors be held liable for any
damages arising from the use of this software.

Permission is granted to anyone to use this software for any
purpose, including commercial applications, and to alter it and
redistribute it freely, subject to the following restrictions:

1.	The origin of this software must not be misrepresented; you
must not claim that you wrote the original software. If you use
this software in a product, an acknowledgment in the product
documentation would be appreciated but is not required.

2.	Altered source versions must be plainly marked as such, and
must not be misrepresented as being the original software.

3.	This notice may not be removed or altered from any source
distribution.

-------------------------------------------------------------*/

#ifndef __GX_STRUCT_H__
#define __GX_STRUCT_H__

/*!
\file gx_struct.h
\brief support header
*/

#include <gctypes.h>

#ifdef __cplusplus
   extern "C" {
#endif /* __cplusplus */

/*!
\typedef struct _gx_rmodeobj GXRModeObj
\brief structure to hold the selected video and render settings
\param viTVMode mode and type of TV
\param fbWidth width of external framebuffer
\param efbHeight height of embedded framebuffer
\param xfbHeight height of external framebuffer
\param viXOrigin x starting point of first pixel to draw on VI
\param viYOrigin y starting point of first pixel to draw on VI
\param viWidth width of configured VI
\param viHeight height of configured VI
*/
typedef struct _gx_rmodeobj {
	u32 viTVMode;
	u16 fbWidth;
	u16 efbHeight;
	u16 xfbHeight;
	u16 viXOrigin;
	u16 viYOrigin;
	u16 viWidth;
	u16 viHeight;
	u32  xfbMode;
	u8  field_rendering;
	u8  aa;
	u8  sample_pattern[12][2];
	u8  vfilter[7];
} GXRModeObj;

#ifdef __cplusplus
   }
#endif /* __cplusplus */

#endif
