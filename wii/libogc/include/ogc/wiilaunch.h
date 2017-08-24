/*-------------------------------------------------------------

wiilaunch.h -- Wii NAND title launching and argument passing

Copyright (C) 2008
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

#if defined(HW_RVL)

#ifndef __WIILAUNCH_H__
#define __WIILAUNCH_H__

#include <gctypes.h>
#include <gcutil.h>

// not initialized
#define WII_ENOTINIT	-0x9001
// internal error
#define WII_EINTERNAL	-0x9002
// checksum error
#define WII_ECHECKSUM	-0x9003
// required title not installed
#define WII_EINSTALL	-0x9004
// argument list too big
#define WII_E2BIG		-0x9005

// you probably shouldn't use anything not in this list, since those may change
// these are guaranteed to exist because Nintendo hardcodes them
// any category not on this list will cause a hang when the settings menu tries to do the animation
// however, settings items contained in one of the following categories will work
// nonexistent items will cause a 404
#define SETTINGS_CALENDAR			"Calendar/Calendar_index.html"
#define SETTINGS_DISPLAY			"Display/Display_index.html"
#define SETTINGS_SOUND				"Sound/Sound_index.html"
#define SETTINGS_PARENTAL			"Parental_Control/Parental_Control_index.html"
#define SETTINGS_INTERNET			"Internet/Internet_index.html"
#define SETTINGS_WC24				"WiiConnect24/Wiiconnect24_index.html"
#define SETTINGS_UPDATE				"Update/Update_index.html"

#ifdef __cplusplus
   extern "C" {
#endif /* __cplusplus */

s32 WII_Initialize(void);
s32 WII_ReturnToMenu(void);
s32 WII_ReturnToSettings(void);
s32 WII_ReturnToSettingsPage(const char *page);
s32 WII_LaunchTitle(u64 titleID);
s32 WII_LaunchTitleWithArgs(u64 titleID, int launchcode, ...);
s32 WII_OpenURL(const char *url);

#ifdef __cplusplus
   }
#endif /* __cplusplus */

#endif

#endif
