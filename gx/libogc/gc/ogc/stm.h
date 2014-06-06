/*-------------------------------------------------------------

stm.h - System and miscellaneous hardware control functions

Copyright (C) 2008
Michael Wiedenbauer (shagkur)
Dave Murphy (WinterMute)
Hector Martin (marcan)

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

#ifndef __STM_H__
#define __STM_H__

#if defined(HW_RVL)

#include <gctypes.h>
#include <gcutil.h>

#define STM_EVENT_RESET		0x00020000
#define STM_EVENT_POWER		0x00000800

#define STM_EINVAL			-0x2004
#define STM_ENOTINIT		-0x2100
#define STM_ENOHANDLER		-0x2101

#ifdef __cplusplus
   extern "C" {
#endif /* __cplusplus */

typedef void (*stmcallback)(u32 event);

s32 __STM_Init();
s32 __STM_Close();
s32 STM_ShutdownToStandby();
s32 STM_ShutdownToIdle();
s32 STM_SetLedMode(u32 mode);
s32 STM_RebootSystem();
stmcallback STM_RegisterEventHandler(stmcallback newhandler);

#ifdef __cplusplus
   }
#endif /* __cplusplus */

#endif /* defined(HW_RVL) */

#endif
