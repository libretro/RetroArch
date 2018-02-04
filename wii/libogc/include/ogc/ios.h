/*-------------------------------------------------------------

ios.h -- IOS control

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

#ifndef __IOS_H__
#define __IOS_H__

#if defined(HW_RVL)

#include <gctypes.h>
#include <gcutil.h>

#define IOS_EINVAL			-0x3004
#define IOS_EBADVERSION		-0x3100
#define IOS_ETOOMANYVIEWS	-0x3101
#define IOS_EMISMATCH		-0x3102

#ifdef __cplusplus
   extern "C" {
#endif /* __cplusplus */

s32 __IOS_InitializeSubsystems(void);
s32 __IOS_ShutdownSubsystems(void);
s32 __IOS_LoadStartupIOS(void);
s32 __IOS_LaunchNewIOS(int version);
s32 IOS_GetPreferredVersion(void);
s32 IOS_ReloadIOS(int version);
s32 IOS_GetVersion();
s32 IOS_GetRevision();
s32 IOS_GetRevisionMajor();
s32 IOS_GetRevisionMinor();

#ifdef __cplusplus
   }
#endif /* __cplusplus */

#endif

#endif
